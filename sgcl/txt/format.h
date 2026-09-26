//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/duration.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/utf8.h"
#include "properties.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <cstring>
#include <type_traits>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

// Text built from a pattern and the values that go in it:
// txt::format("{} left", n). The pattern is the one of std::format — a
// pair of braces for a value, a colon and a specification after it for
// how to write it — and it is read where the program is compiled, not
// where it runs: a pattern that does not fit the values it is given is an
// error of the compiler and not a surprise at the customer's. The other
// road is there for a pattern the compiler cannot see — a catalogue of
// translations read from a file — and is asked for by name,
// txt::format(txt::runtime(entry), n), which answers an optional.
//
// Why here and not in core. Two reasons, and the second is the real one.
// A bare format() next to sgcl::string is not ours: basic_string names
// std::char_traits among its arguments, so std is an associated namespace
// of it and an unqualified call finds std::format, which does not
// announce a clash — it wins, and fails later on a std::string that will
// not convert. And a field of text cannot be measured without the tables
// that are here: "żółć" is eight bytes and four columns, so {:>10} pads
// it by six and not by two, and {:.3} has to stop on a code point, where
// cutting bytes leaves a lead byte with nothing behind it. The standard
// says the same — width and precision over text are an estimated field
// width, wide East Asian characters counting two — which is columns().
//
// Why not std::format itself. The values go straight to the writer as the
// types they are, where the standard's passes them through a variant and
// reaches them by an indirect call, and the output is a pointer into a
// buffer rather than an iterator that cannot be written to in one piece.
// And the pattern is not read where the program runs at all: the same
// pass of the compiler that checks it writes down its steps — the run of
// literal text, the value that follows it and the specification already
// made out — and what happens at the call is a walk over as many steps
// as the pattern has fields, not over its characters.
//
// Measured on this machine, "{} left" with a number: 23.2 ns here against
// 33.1, and 14.0 when the caller lends a buffer and nothing is allocated;
// "{:>8.3f} {:#x}" 59.5 against 104.7. Those are one value repeated,
// which flatters the standard: over a stream of different ones, where the
// length of a number cannot be predicted, format_to of one whole number
// is 15.3 ns against 19.0 and of two 31.9 against 45.0.
namespace sgcl::txt {
    // How a value is to be written: what std::format's specification
    // holds, in the order it is written there —
    // [[fill]align][sign][#][0][width][.precision][type]
    struct format_spec {
        char fill = ' ';
        char align = 0;            // '<', '>', '^', or none given
        char sign = 0;             // '+', '-', ' ', or none given
        bool alternate = false;    // '#': 0x, 0b, a point that stays
        bool zero = false;         // '0': pad a number with zeros after its sign
        unsigned width = 0;
        int precision = -1;        // none given
        char type = 0;             // 'd', 'x', 'f', 's', 'c', 'p', …
    };

    // Where a value writes itself. It holds how much room is left and how
    // much has been asked for, so that one pass can both fill a buffer
    // and say what the whole would take: the caller may size a buffer
    // from an empty one and write into it with the same code.
    class format_sink {
    public:
        constexpr format_sink(char* at, size_t room) noexcept
        : _at(at)
        , _end(at + room) {
        }

        constexpr void put(char c) noexcept {
            if (_at < _end) {
                *_at++ = c;
            }
            ++_size;
        }

        // The literal runs of a pattern arrive here whole, so they are
        // written whole: a byte at a time cost 0.7 ns a character of the
        // pattern, which was the whole of what a call took — a text of
        // forty-five characters with no value in it read 33.8 ns.
        constexpr void put(const char* text, size_t n) noexcept {
            size_t room = size_t(_end - _at);
            size_t fits = n < room ? n : room;
            if (std::is_constant_evaluated()) {
                for (size_t i = 0; i < fits; ++i) {
                    _at[i] = text[i];
                }
            } else {
                sgcl::detail::copy_bytes(_at, text, fits);
            }
            _at += fits;
            _size += n;
        }

        constexpr void put(std::string_view text) noexcept {
            put(text.data(), text.size());
        }

        constexpr void fill(char c, size_t n) noexcept {
            size_t room = size_t(_end - _at);
            size_t fits = n < room ? n : room;
            if (std::is_constant_evaluated()) {
                for (size_t i = 0; i < fits; ++i) {
                    _at[i] = c;
                }
            } else {
                sgcl::detail::fill_bytes(_at, (unsigned char)c, fits);
            }
            _at += fits;
            _size += n;
        }

        // What the whole text takes, whether or not it fitted
        constexpr size_t size() const noexcept {
            return _size;
        }

        // The same sink put over other room, with what it has counted so
        // far kept rather than started again. Nobody writing a value
        // calls this: it is for whoever owns the room and can get more of
        // it — growing_sink below, which is the whole reason it is here.
        constexpr void reseat(char* at, size_t room, size_t counted) noexcept {
            _at = at;
            _end = at + room;
            _size = counted;
        }

    private:
        char* _at;
        char* _end;
        size_t _size = 0;
    };

    // A sink whose room can be added to, for a caller that writes in
    // steps and can write one of them over again.
    //
    // The pattern of format() below does not want this and is not given
    // it. There a text that does not fit is written twice, and that is
    // measured: the second pass is to_chars and memcpy over a handful of
    // fields, which costs less than the copying a buffer that doubles
    // does. A page of a template is the other case. Its second pass
    // walks the same branches, the same loops and the same pipelines
    // again — upper(), escape_html() — which is work and not copying,
    // and a page is kilobytes where a message is a line.
    //
    // What it asks of the caller is a mark: where the thing just written
    // began. When that thing ran off the end, the room is made big
    // enough for it, what stands before the mark is carried over, the
    // sink goes back to the mark and the caller writes that one thing
    // again — one step of a page and not the page.
    //
    // And it asks to be asked cheaply, which is the whole of what the
    // two methods below are shaped around: the capacity read once into a
    // local of the caller's rather than out of this object at every
    // step, and the test marked unlikely so that the page which fits
    // falls through it. Neither is tidiness. Together they were fourteen
    // per cent of a page of ten rows that never grew at all.
    //
    // The first room is the caller's, on its stack, so a page that fits
    // in it allocates nothing at all and this class is out of the way.
    // What it takes afterwards is a plain array of characters and not a
    // managed one, for the reason format's own wider buffer is: it holds
    // no pointer the collector could have to find, it dies with the call
    // that made it, and a managed block would be swept long after the
    // page it carried was handed over. A `new char[n]` and not the
    // std::string format uses, because a string of n characters writes n
    // zeros into room that is about to be written over — though that is
    // a reason and not a measurement: the two were built and alternated,
    // and over a page of a hundred kilobytes, where the doublings zero
    // 260 KB between them, they read 236.6 us and 235.7. The zeroing is
    // invisible against the walk. What decided it is that this way asks
    // for nothing it does not use, not that the other way was slower.
    class growing_sink {
    public:
        // Room of the caller's, which more can be added to
        growing_sink(char* room, size_t n) noexcept
        : _at(room)
        , _cap(n)
        , _room(n)
        , _sink(room, n) {
        }

        // Room of the caller's which cannot be added to: what fits is
        // written and the whole size still comes back, which is the
        // contract of format_to and of render_to. A cap of size_t(-1) is
        // how that is said — nothing ever runs off the end of a room
        // that big — so that a walk has one kind of sink and not two.
        //
        // Which is why the room it really has is kept beside that, and
        // is the only thing view() below may believe. For every other
        // shape of this class the two are one number and the second is
        // read nowhere a walk can feel it.
        struct lent_t {};
        static constexpr lent_t lent {};

        growing_sink(lent_t, char* room, size_t n) noexcept
        : _at(room)
        , _cap(size_t(-1))
        , _room(n)
        , _sink(room, n) {
        }

        growing_sink(const growing_sink&) = delete;
        growing_sink& operator=(const growing_sink&) = delete;

        // Where the values go. The reference stays good over a growth:
        // the sink is this object's own and is moved over the new room
        // where it stands.
        format_sink& out() noexcept {
            return _sink;
        }

        // How much the room holds. A caller that writes in a loop reads
        // this once, keeps it in a variable of its own and compares
        // against that, rather than asking here at every step, and takes
        // the answer of take_room back into it. The reason is register
        // allocation: a field of this object has to be read back after
        // every call a step makes, because a call might have written it,
        // where a local the compiler can prove nobody else reaches stays
        // in a register across the whole walk.
        size_t capacity() const noexcept {
            return _cap;
        }

        // More room: at least `want` characters of it, with what stands
        // before `mark` carried over and the sink put back there. What
        // comes back is the new capacity, for the caller's variable.
        //
        // The caller writes its step first and asks afterwards, with
        // `want` the size the step came to and `mark` where it began,
        // and writes that one step again. It is not that a step could
        // not be measured in advance — a literal run of a page knows its
        // own length — but that asking in advance buys nothing and costs
        // the same, which was measured both ways.
        //
        // What it does cost is the branch, and only if the branch is
        // written the wrong way round. The caller marks the test
        // unlikely, so the page that fits falls through it; left to
        // itself the compiler put the growth in line and jumped over it
        // for the common case, and that one taken branch a step came to
        // fourteen per cent of a page of ten rows.
        size_t take_room(size_t want, size_t mark) {
            if (_cap == size_t(-1)) {
                return _cap;        // room that was lent; it cannot grow
            }
            size_t doubled = _cap * 2;
            _grow(want > doubled ? want : doubled, mark);
            return _cap;
        }

        // What the whole page takes, whether or not it fitted
        size_t size() const noexcept {
            return _sink.size();
        }

        // What was written, where it stands. For whoever wants to read it
        // back rather than hand it over — a body about to be padded into
        // the field it goes in, which is put_body_in_field below.
        //
        // What is there and not what was counted. size() is the whole
        // text, whether or not it fitted, which is the whole point of
        // it; the characters are only the ones the room holds. The two
        // are the same number for a sink that has been given more room
        // whenever it ran out, which is what a page of a stencil does
        // and why nothing noticed — but for a sink whose room was lent
        // they are not, and reading size() characters out of a buffer of
        // n was reading whatever stood after the caller's buffer.
        // Nobody called it; it is answered here before somebody does.
        std::string_view view() const noexcept {
            size_t n = _sink.size();
            return std::string_view(_at, n < _room ? n : _room);
        }

        // The page, once the walk is over. Of a sink whose room was lent
        // and never added to there is no whole page to hand back — what
        // did not fit was dropped — so that road asks size() instead,
        // which is what it came for.
        string text() const {
            auto whole = view();
            return string(whole.data(), whole.size());
        }

    private:
        void _grow(size_t n, size_t mark) {
            std::unique_ptr<char[]> room(new char[n]);
            sgcl::detail::copy_bytes(room.get(), _at, mark);
            _owned = std::move(room);
            _at = _owned.get();
            _cap = n;
            _room = n;
            _sink.reseat(_at + mark, n - mark, mark);
        }

        char* _at;
        size_t _cap;                    // what may be written before more is asked for
        size_t _room;                   // what is really there, which view() reads
        std::unique_ptr<char[]> _owned;
        format_sink _sink;
    };

    namespace detail {
        // A value already written into a small buffer, put in a field
        // wider than itself: the padding of the specification around it.
        // `head` is what must stay in front of the zeros when a number is
        // padded with them — its sign and its 0x. `body_width` is what
        // the body takes in the field when that is not its bytes: text is
        // measured in columns (write_text), a number's digits are one
        // each.
        //
        // Apart from put_padded below, and deliberately. With both roads
        // in one function the compiler wrote a frame of 112 bytes and
        // spilled six pairs of registers before anything happened — the
        // copying ladder appears in it four times over, which is more
        // than it will inline — and every field paid that, whether or not
        // it had a width at all.
        inline void put_in_field(format_sink& out, std::string_view body, const format_spec& spec,
                                 std::string_view head, char align, size_t body_width) noexcept {
            size_t n = head.size() + (body_width == size_t(-1) ? body.size() : body_width);
            size_t pad = spec.width > n ? spec.width - n : 0;
            char how = spec.align ? spec.align : align;
            if (spec.zero && !spec.align && (align == '>')) {
                out.put(head);
                out.fill('0', pad);
                out.put(body);
                return;
            }
            if (pad && how == '>') {
                out.fill(spec.fill, pad);
            } else if (pad && how == '^') {
                out.fill(spec.fill, pad / 2);
            }
            out.put(head);
            out.put(body);
            if (pad && how == '<') {
                out.fill(spec.fill, pad);
            } else if (pad && how == '^') {
                out.fill(spec.fill, pad - pad / 2);
            }
        }

        // The value in its field. No width asked for is the common call
        // by far, and then there is nothing to pad: the head, when there
        // is one, and the body. Small enough to be inlined into whoever
        // wrote the value, which is the whole point of the split.
        constexpr void put_padded(format_sink& out, std::string_view body, const format_spec& spec,
                                  std::string_view head = {}, char align = '<',
                                  size_t body_width = size_t(-1)) noexcept {
            if (spec.width) {
                put_in_field(out, body, spec, head, align, body_width);
                return;
            }
            if (head.size()) {
                out.put(head);
            }
            out.put(body);
        }

        // A whole number written in decimal, which is what nearly every
        // one of them is written in. The standard's to_chars costs 1.70 ns
        // for a 32-bit value and 4.35 for a 64-bit one, over a stream of
        // values rather than one repeated — and half of that is the
        // branching on how many digits there are, which the same call on
        // one constant value does not pay (1.93 ns against 3.85 with the
        // loop's own 2.17 taken off both).
        //
        // So: no branch on the length at all. Eight digits come out at
        // once, leading zeros and all, as four dividends by a hundred and
        // then by ten in sixteen-bit lanes — the reciprocals 5243>>19 and
        // 6554>>16 are exact below 10^4 and 100 — and how many of them
        // matter is one lookup on the bit width. The digits are then slid
        // up inside the register by a table lookup, where reading past
        // the eighth gives zero, so nothing goes through memory twice.
        // 1.10 ns and 2.96. The caller must leave eight bytes of room
        // past the number, which is what the buffers here have.
        inline constexpr char DigitPairs[201] =
            "00010203040506070809" "10111213141516171819" "20212223242526272829"
            "30313233343536373839" "40414243444546474849" "50515253545556575859"
            "60616263646566676869" "70717273747576777879" "80818283848586878889"
            "90919293949596979899";

        // How many digits a value has, from its bit width: the add carries
        // into the high word exactly when the value has passed the power
        // of ten that the width allows (Willets' table)
        inline constexpr uint64_t DigitCounts[32] = {
            4294967296ull,  8589934582ull,  8589934582ull,  8589934582ull,
            12884901788ull, 12884901788ull, 12884901788ull, 17179868184ull,
            17179868184ull, 17179868184ull, 21474826480ull, 21474826480ull,
            21474826480ull, 21474826480ull, 25769703776ull, 25769703776ull,
            25769703776ull, 30063771072ull, 30063771072ull, 30063771072ull,
            34349738368ull, 34349738368ull, 34349738368ull, 34349738368ull,
            38554705664ull, 38554705664ull, 38554705664ull, 41949672960ull,
            41949672960ull, 41949672960ull, 42949672960ull, 42949672960ull };

        inline unsigned digit_count(uint32_t v) noexcept {
            return unsigned((v + DigitCounts[31 - __builtin_clz(v | 1)]) >> 32);
        }

#if defined(__ARM_NEON)
        inline uint8x8_t eight_digits(uint32_t v) noexcept {
            uint32_t hi = v / 10000;
            uint32_t lo = v % 10000;
            uint32x2_t a = {hi, lo};
            uint32x2_t q = vshr_n_u32(vmul_n_u32(a, 5243), 19);
            uint32x2_t r = vsub_u32(a, vmul_n_u32(q, 100));
            uint32x2x2_t z = vzip_u32(q, r);
            uint16x4_t f = vmovn_u32(vcombine_u32(z.val[0], z.val[1]));
            uint16x4_t q2 = vshrn_n_u32(vmull_n_u16(f, 6554), 16);
            uint16x4_t r2 = vsub_u16(f, vmul_n_u16(q2, 10));
            uint16x4x2_t z2 = vzip_u16(q2, r2);
            return vadd_u8(vmovn_u16(vcombine_u16(z2.val[0], z2.val[1])), vdup_n_u8('0'));
        }
#endif

        // Exactly eight digits, the leading ones zero
        inline void write_eight(char* to, uint32_t v) noexcept {
#if defined(__ARM_NEON)
            vst1_u8((uint8_t*)to, eight_digits(v));
#else
            uint32_t hi = v / 10000;
            uint32_t lo = v % 10000;
            std::memcpy(to, DigitPairs + (hi / 100) * 2, 2);
            std::memcpy(to + 2, DigitPairs + (hi % 100) * 2, 2);
            std::memcpy(to + 4, DigitPairs + (lo / 100) * 2, 2);
            std::memcpy(to + 6, DigitPairs + (lo % 100) * 2, 2);
#endif
        }

        // The `n` digits that matter, at `to`, with the rest of the eight
        // left as whatever follows them
        inline void write_significant(char* to, uint32_t v, unsigned n) noexcept {
#if defined(__ARM_NEON)
            uint8x8_t slide = vadd_u8(vcreate_u8(0x0706050403020100ull), vdup_n_u8(uint8_t(8 - n)));
            vst1_u8((uint8_t*)to, vtbl1_u8(eight_digits(v), slide));
#else
            // Sixteen and not eight: the copy below starts at 8 - n and
            // takes eight bytes, so for a short number it reaches past
            // the digits — into the array still, now that there is room
            char room[16];
            write_eight(room, v);
            std::memcpy(to, room + 8 - n, 8);
#endif
        }

        inline char* write_decimal(char* to, uint32_t v) noexcept {
            unsigned n = digit_count(v);
            if (v < 100000000u) {
                write_significant(to, v, n);
                return to + n;
            }
            unsigned top = n - 8;
            write_significant(to, v / 100000000u, top);
            write_eight(to + top, v % 100000000u);
            return to + n;
        }

        inline char* write_decimal(char* to, uint64_t v) noexcept {
            if (v <= 0xFFFFFFFFull) {
                return write_decimal(to, uint32_t(v));
            }
            if (v < 10000000000000000ull) {
                char* at = write_decimal(to, uint32_t(v / 100000000ull));
                write_eight(at, uint32_t(v % 100000000ull));
                return at + 8;
            }
            uint64_t rest = v % 10000000000000000ull;
            char* at = write_decimal(to, uint32_t(v / 10000000000000000ull));
            write_eight(at, uint32_t(rest / 100000000ull));
            write_eight(at + 8, uint32_t(rest % 100000000ull));
            return at + 16;
        }

        constexpr int base_of(char type) noexcept {
            return type == 'b' || type == 'B' ? 2 : type == 'o' ? 8
                 : type == 'x' || type == 'X' ? 16 : 10;
        }

        template<class T>
        void write_integer(format_sink& out, T value, const format_spec& spec) {
            char buf[80];   // the digits, and eight bytes of room past them
            using U = std::make_unsigned_t<std::conditional_t<std::is_same_v<T, bool>, unsigned char, T>>;
            bool negative = false;
            U magnitude;
            if constexpr (std::is_signed_v<T>) {
                negative = value < 0;
                magnitude = negative ? U(~U(value) + 1) : U(value);
            } else {
                magnitude = U(value);
            }
            int base = base_of(spec.type);
            std::to_chars_result r{};
            if (base == 10) {
                using W = std::conditional_t<(sizeof(U) > 4), uint64_t, uint32_t>;
                r.ptr = write_decimal(buf, W(magnitude));
            } else {
                r = std::to_chars(buf, buf + sizeof buf, magnitude, base);
            }
            if (spec.type == 'X') {
                for (char* p = buf; p < r.ptr; ++p) {
                    if (*p >= 'a' && *p <= 'f') {
                        *p = char(*p - 32);
                    }
                }
            }
            char head[4];
            size_t head_size = 0;
            if (negative) {
                head[head_size++] = '-';
            } else if (spec.sign == '+' || spec.sign == ' ') {
                head[head_size++] = spec.sign;
            }
            // '#' puts 0b, 0B, 0x or 0X in front — but in octal only a
            // single zero, and none at all in front of a zero, which
            // already is one
            if (spec.alternate && base != 10) {
                if (spec.type == 'o') {
                    if (magnitude) {
                        head[head_size++] = '0';
                    }
                } else {
                    head[head_size++] = '0';
                    head[head_size++] = spec.type;
                }
            }
            put_padded(out, {buf, size_t(r.ptr - buf)}, spec, {head, head_size}, '>');
        }

        // A number written as {} whose value is a whole one below 2^53
        // and does not end in a zero. Then the interval of values that
        // read back as it is narrower than one, so no other whole number
        // is in it and no shorter decimal either: its digits are the
        // shortest representation, and the plain form is always the
        // shorter of the two shapes, since the exponential one spends
        // five more characters on the point and the exponent. So there is
        // nothing to compare and nothing to strip — which is what keeps
        // this small enough to be worth having.
        //
        // The standard's to_chars is at its slowest on exactly these:
        // 12.5 ns for a value like 42 or 1000, where its loop takes the
        // trailing digits off one at a time. A value that does end in a
        // zero is left to it: the shape then has to be weighed (100 stays
        // 100 and 100000 becomes 1e+05) and that weighing costs more than
        // it saves.
        // Below the point where the steps between neighbours grow past
        // one: 2^53 for a double and 2^24 for a float, which is where the
        // significand runs out in each
        template<class F>
        constexpr F whole_limit() noexcept {
            return std::is_same_v<F, float> ? F(16777216) : F(9007199254740992);
        }

        template<class F>
        inline bool is_whole(F magnitude, uint64_t& whole) noexcept {
            if (!(magnitude < whole_limit<F>()) || magnitude != std::floor(magnitude)) {
                return false;
            }
            whole = uint64_t(magnitude);
            return true;
        }

        // The rest of the whole numbers: the ones that end in a zero, and
        // so have a shorter significand than their digits. Then the two
        // shapes have to be weighed — 100 stays 100, since the plain form
        // is three characters against the exponential form's five, and
        // 100000 becomes 1e+05, six against five — and the standard's
        // rule is that the shorter wins with the plain one taking a tie.
        // Out of line on purpose: it is the rarer road, and inlining it
        // grew write_float past what the compiler will keep in registers.
        inline void write_whole_with_zeros(format_sink& out, uint64_t whole,
                                           std::string_view head, const format_spec& spec) {
            char digits[24];
            char* end = write_decimal(digits, whole);
            unsigned n = unsigned(end - digits);
            unsigned p = n;
            while (p > 1 && digits[p - 1] == '0') {
                --p;
            }
            unsigned x = n - 1;
            unsigned exponential = p + (p > 1 ? 1 : 0) + 2 + (x < 100 ? 2 : 3);
            if (n <= exponential) {
                put_padded(out, {digits, n}, spec, head, '>');
                return;
            }
            char room[24];
            char* at = room;
            *at++ = digits[0];
            if (p > 1) {
                *at++ = '.';
                sgcl::detail::copy_bytes(at, digits + 1, p - 1);
                at += p - 1;
            }
            *at++ = 'e';
            *at++ = '+';
            if (x >= 100) {
                *at++ = char('0' + x / 100);
                x %= 100;
            }
            *at++ = char('0' + x / 10);
            *at++ = char('0' + x % 10);
            put_padded(out, {room, size_t(at - room)}, spec, head, '>');
        }

        // A float is written as a float and not as the double it widens
        // to: the shortest text that reads back as 1.1f is "1.1", where
        // the shortest that reads back as double(1.1f) is
        // "1.100000023841858" — the same number, a different question.
        template<class F>
        inline void write_float(format_sink& out, F value, const format_spec& spec) {
            char buf[64];
            std::to_chars_result r{};
            auto form = spec.type == 'e' || spec.type == 'E' ? std::chars_format::scientific
                      : spec.type == 'f' || spec.type == 'F' ? std::chars_format::fixed
                      : spec.type == 'a' || spec.type == 'A' ? std::chars_format::hex
                      : std::chars_format::general;
            bool negative = value < 0 || (value == 0 && std::signbit(value));
            F magnitude = negative ? -value : value;
            char head[2];
            size_t head_size = 0;
            if (negative) {
                head[head_size++] = '-';
            } else if (spec.sign == '+' || spec.sign == ' ') {
                head[head_size++] = spec.sign;
            }
            // The whole numbers, which {} meets more often than anything
            // else and the standard writes slowest. Before the conversion
            // below and not after it, or both are paid.
            // Not when '#' was asked for: that road writes the digits
            // and nothing else, where the alternate form wants a point
            uint64_t whole;
            if (!spec.type && spec.precision < 0 && !spec.alternate
                && is_whole(magnitude, whole)) {
                if (whole % 10) {
                    // Nothing to strip, and then the plain form always
                    // wins: the exponential one spends five more
                    // characters on the point and the exponent
                    char plain[24];
                    char* end = write_decimal(plain, whole);
                    put_padded(out, {plain, size_t(end - plain)}, spec, {head, head_size}, '>');
                } else {
                    write_whole_with_zeros(out, whole, {head, head_size}, spec);
                }
                return;
            }
            // With no precision given, a named form still has one: six,
            // as printf has always had it and as the standard keeps it —
            // {:f} of 0.5 is 0.500000 and not 0.5. Only the hexadecimal
            // form and no form at all write the shortest text that reads
            // back as the same number.
            int precision = spec.precision;
            if (precision < 0 && spec.type && form != std::chars_format::hex) {
                precision = 6;
            }

            // '#' over the general form keeps the trailing zeros, which
            // that form exists to remove: {:#g} of 1 is 1.00000 and not
            // 1. The shape is then chosen by the rule the general form
            // uses — scientific when the exponent falls outside [-4, P),
            // fixed otherwise — and the digits are asked for in full.
            //
            // The exponent that decides it is the one the value has
            // after it has been rounded to P digits, not before: 991400
            // to six digits is 991400 with an exponent of five and goes
            // out plainly, but rounded to one digit it is 1e+06 and
            // would have gone out exponential. So the scientific
            // conversion below is the real one, done to P - 1 places,
            // and the fixed form is a second pass when the exponent
            // turns out to call for it.
            bool general = !spec.type || spec.type == 'g' || spec.type == 'G';
            bool weigh = spec.alternate && general && std::isfinite(magnitude)
                      && (spec.precision >= 0 || spec.type);
            int digits = 0;
            if (weigh) {
                digits = precision < 0 ? 6 : precision;
                if (digits == 0) {
                    digits = 1;
                }
                form = std::chars_format::scientific;
                precision = digits - 1;
            }

            // The conversion, into the room on the stack when it fits and
            // into a buffer sized for it when it does not. to_chars
            // reports value_too_large by setting ptr to the end of the
            // room it was given, so writing what it points at without
            // asking is writing whatever the stack held: {:.100f} of 1
            // came out as sixty-four characters of a truncated number.
            char* at = buf;
            size_t room = sizeof buf;
            std::string wider;
            for (;;) {
                // One byte short of the room on purpose: '#' may have a
                // point to put in afterwards, and it must have somewhere
                char* last = at + room - 1;
                if (precision >= 0) {
                    r = std::to_chars(at, last, magnitude, form, precision);
                } else if (spec.type) {
                    r = std::to_chars(at, last, magnitude, form);
                } else {
                    r = std::to_chars(at, last, magnitude);
                }
                if (r.ec == std::errc{}) {
                    if (weigh) {
                        // The exponent the scientific form just wrote,
                        // which is the one the rule asks about
                        int x = 0;
                        for (char* c = at; c < r.ptr; ++c) {
                            if (*c == 'e') {
                                // from_chars refuses the leading '+' that
                                // to_chars writes, so the sign comes off
                                // by hand
                                bool minus = c + 1 < r.ptr && c[1] == '-';
                                const char* from = c + 1
                                                 + (minus || (c + 1 < r.ptr && c[1] == '+') ? 1 : 0);
                                std::from_chars(from, r.ptr, x);
                                if (minus) {
                                    x = -x;
                                }
                                break;
                            }
                        }
                        weigh = false;
                        if (x >= -4 && x < digits) {
                            form = std::chars_format::fixed;
                            precision = digits - 1 - x;
                            continue;             // the second pass, plainly
                        }
                    }
                    break;
                }
                if (at != buf) {
                    r.ptr = at;                  // cannot happen; write nothing rather than guess
                    break;
                }
                wider.resize(size_t(precision > 0 ? precision : 0) + 400);
                at = wider.data();
                room = wider.size();
            }
            // 'F' among them for infinity and NaN alone: a finite value
            // written fixed has no letter in it, but inf and nan do, and
            // the standard writes them INF and NAN
            if (spec.type == 'E' || spec.type == 'F' || spec.type == 'A' || spec.type == 'G') {
                for (char* p = at; p < r.ptr; ++p) {
                    if (*p >= 'a' && *p <= 'z') {
                        *p = char(*p - 32);
                    }
                }
            }
            // And '#' keeps the point itself, wherever the conversion
            // left none: {:#} of 1 is 1., {:#.0e} of 1 is 1.e+00
            if (spec.alternate && std::isfinite(magnitude)) {
                char* mark = r.ptr;
                bool point = false;
                for (char* p = at; p < r.ptr; ++p) {
                    if (*p == '.') {
                        point = true;
                        break;
                    }
                    if (*p == 'e' || *p == 'E' || *p == 'p' || *p == 'P') {
                        mark = p;
                        break;
                    }
                }
                if (!point) {
                    sgcl::detail::move_bytes(mark + 1, mark, size_t(r.ptr - mark));
                    *mark = '.';
                    ++r.ptr;
                }
            }
            // Zeros do not pad what has no digits: {:010} of infinity is
            // seven spaces and inf, not seven zeros and inf, which is
            // what printf has always done and what the standard keeps
            if (spec.zero && !std::isfinite(magnitude)) {
                format_spec spaced = spec;
                spaced.zero = false;
                put_padded(out, {at, size_t(r.ptr - at)}, spaced, {head, head_size}, '>');
                return;
            }
            put_padded(out, {at, size_t(r.ptr - at)}, spec, {head, head_size}, '>');
        }

        // Text in its field, measured in the columns it takes on a
        // terminal and not in its bytes. A precision cuts it to that many
        // columns and stops on a code point; a width pads it to that many.
        // The measuring is skipped when neither was asked for.
        constexpr void write_text(format_sink& out, std::string_view text, const format_spec& spec) noexcept {
            if (spec.precision >= 0) {
                text = text.substr(0, columns_prefix(text, size_t(spec.precision)));
            }
            put_padded(out, text, spec, {}, '<', spec.width ? columns_of_text(text) : size_t(-1));
        }

        //----------------------------------------------------------------
        // {:?}: text as it would be written in a program rather than as
        // it would be read — in quotes, with what a terminal cannot show
        // written as an escape. It is what tells a list of two words from
        // one word with a comma in it, and it is why the elements of a
        // range are written this way by default, as they are in C++23.
        //----------------------------------------------------------------
        // Which code points are escaped is the standard's list, and the
        // categories are the ones properties.h already answers, so
        // nothing is written twice: the controls (Cc), the format
        // characters (Cf), the surrogates (Cs), the private use area
        // (Co), what Unicode has not assigned (Cn), the line and
        // paragraph separators (Zl, Zp), and every space separator (Zs)
        // but the space itself — which is a character a reader expects to
        // see, and no other one is.
        constexpr bool needs_escape(char32_t c) noexcept {
            if (c < 0x80) {
                // Cc over ASCII is exactly these, and asking the table
                // about a letter is a lookup for an answer already known
                return c < 0x20 || c == 0x7F;
            }
            switch (category_of_fn(c)) {
                case category::unassigned:
                case category::space_separator:
                case category::line_separator:
                case category::paragraph_separator:
                case category::control:
                case category::format:
                case category::surrogate:
                case category::private_use:
                    return true;
                default:
                    return false;
            }
        }

        // The digits of a number in hexadecimal, lower case and with no
        // leading zeros, which is the shape \u{...} is written in
        constexpr void put_hex(format_sink& out, uint32_t v) noexcept {
            char room[8];
            size_t n = 0;
            do {
                room[n++] = "0123456789abcdef"[v & 0xF];
                v >>= 4;
            } while (v);
            while (n) {
                out.put(room[--n]);
            }
        }

        // The quote is the one that has to be escaped inside: a double
        // one around text and a single one around a character, as the
        // standard has it and as the source of a program would.
        constexpr void escape_text(format_sink& out, std::string_view text, char quote) noexcept {
            out.put(quote);
            size_t i = 0;
            while (i < text.size()) {
                // What needs nothing done to it goes out whole rather
                // than a byte at a time, which is the lesson the literal
                // runs of a pattern taught a few notes ago
                size_t run = i;
                while (run < text.size()) {
                    unsigned char at = (unsigned char)text[run];
                    if (at < 0x20 || at >= 0x7F || at == '\\' || char(at) == quote) {
                        break;
                    }
                    ++run;
                }
                if (run > i) {
                    out.put(text.data() + i, run - i);
                    i = run;
                    continue;
                }
                unsigned char b = (unsigned char)text[i];
                if (b < 0x80) {
                    ++i;
                    char c = char(b);
                    if (c == '\t') {
                        out.put("\\t", 2);
                    } else if (c == '\n') {
                        out.put("\\n", 2);
                    } else if (c == '\r') {
                        out.put("\\r", 2);
                    } else if (c == '\\' || c == quote) {
                        out.put('\\');
                        out.put(c);
                    } else {
                        out.put("\\u{", 3);
                        put_hex(out, uint32_t(b));
                        out.put('}');
                    }
                    continue;
                }
                auto [point, width] = utf8::decode(text, i);
                if (width == 1) {
                    // A byte that begins no sequence and ends none is not
                    // a character at all, so it goes out as the byte it
                    // is and not as the replacement it would decode to —
                    // \x{..} and not \u{fffd}, which would lose it
                    out.put("\\x{", 3);
                    put_hex(out, uint32_t(b));
                    out.put('}');
                    ++i;
                    continue;
                }
                if (needs_escape(point)) {
                    out.put("\\u{", 3);
                    put_hex(out, uint32_t(point));
                    out.put('}');
                } else {
                    out.put(text.data() + i, width);
                }
                i += width;
            }
            out.put(quote);
        }

        // With neither a width nor a precision — the common call — the
        // escaping writes itself straight into the sink and nothing is
        // held anywhere. Both of those are over what comes out and not
        // over what went in, so either one wants the whole of it first:
        // a precision cuts the escapes along with the text, as the
        // standard says, and a field is measured in the columns the
        // escapes take.
        inline void write_debug_text(format_sink& out, std::string_view text,
                                     const format_spec& spec, char quote) {
            if (!spec.width && spec.precision < 0) {
                escape_text(out, text, quote);
                return;
            }
            char none;
            format_sink counter(&none, 0);
            escape_text(counter, text, quote);
            size_t n = counter.size();
            char room[256];
            if (n <= sizeof room) {
                format_sink again(room, sizeof room);
                escape_text(again, text, quote);
                write_text(out, std::string_view(room, n), spec);
                return;
            }
            std::string wider(n, '\0');
            format_sink again(wider.data(), wider.size());
            escape_text(again, text, quote);
            write_text(out, std::string_view(wider.data(), n), spec);
        }

        // Text written either way, which is the whole of what the text
        // formatters below have to decide
        inline void write_text_or_debug(format_sink& out, std::string_view text,
                                        const format_spec& spec) {
            if (spec.type == '?') {
                write_debug_text(out, text, spec, '"');
                return;
            }
            write_text(out, text, spec);
        }

        // A value written by a format_value found beside its type:
        //     void format_value(format_sink&, const T&, const format_spec&);
        // declared in the namespace of T and found there, so that a type
        // teaches this how to write it without opening anything here.
        // Asked about up here, and not only where write_one asks it,
        // because a formatter below stands aside for it: an enumeration
        // is written as its number unless its own namespace says better.
        template<class T>
        concept has_format_value = requires(format_sink& out, const T& v, const format_spec& s) {
            format_value(out, v, s);
        };

        constexpr bool is_digit(char c) noexcept {
            return c >= '0' && c <= '9';
        }

        // [[fill]align][sign][#][0][width][.precision][type] — the body of
        // a field, which is what stands between its colon and the brace
        // that ends it. Nothing of the pattern around it is read here, so
        // the same function reads the body of a field and the
        // specification a value hands on to what it holds.
        //
        // That is what `nested` is. A value that is made of other values
        // — a range, a pair — takes a second colon, and everything after
        // it belongs to the elements rather than to the whole:
        // {::>4} pads each number of a list to four. It is handed on as
        // characters and read again by whoever holds the elements, which
        // is what lets it go down as many levels as the type has.
        //
        // Whether a colon means that, or is merely a character to pad
        // with, depends on the value it was written for, and only the
        // caller knows which value that is — hence the flag. A number is
        // still padded with colons by {::>6}, exactly as it was before;
        // for a range that pattern means something else, as it does in
        // C++23, whose grammar leaves a colon out of a range's fill for
        // the same reason.
        //
        // What comes back in `nested` when no second colon was written is
        // a view over nothing at all — data() is null — which is not the
        // same as one that is there and empty, {::}. The difference is
        // what lets a type refuse a specification it was never given.
        //
        // Three names of this block are reached for from outside it, by
        // stencil.h: this one, `takes_one` and `write_one`. That is on
        // purpose and is what keeps a template's `{{ n:>8.2f }}` the same
        // field as a pattern's — one reader of a specification, one
        // writer of a value, one place where columns are counted. They
        // are in `detail` because they are not meant for a caller of the
        // library, not because they are free to move: whoever changes
        // their shape changes that header too.
        //
        // A third kind of value reads the rest of its field as a pattern
        // of its own: a time, whose field is the one std::format gives
        // the types of <chrono> — [[fill]align][width][.precision] and
        // then the pattern, from its first '%' to the brace, colons and
        // all ({:%H:%M}, {:>12%F}). That is `nests` 2 (1 is the colon
        // above, 0 neither); the pattern comes back in `nested` too, for
        // the value to check and to write, and none written is a view
        // over nothing, as above. Nothing else of the specification is
        // read for it: no sign, no '#', no '0', no type.
        constexpr bool read_layout_spec(std::string_view body, format_spec& spec,
                                        std::string_view& nested) noexcept {
            size_t at = 0;
            size_t n = body.size();
            auto aligns = [](char c) noexcept {
                return c == '<' || c == '>' || c == '^';
            };
            if (at + 1 < n && aligns(body[at + 1])) {
                spec.fill = body[at];
                spec.align = body[at + 1];
                at += 2;
            } else if (at < n && aligns(body[at])) {
                spec.align = body[at++];
            }
            while (at < n && is_digit(body[at])) {
                unsigned digit = unsigned(body[at] - '0');
                if (spec.width > (0xFFFFu - digit) / 10) {
                    return false;
                }
                spec.width = spec.width * 10 + digit;
                ++at;
            }
            if (at < n && body[at] == '.') {
                ++at;
                if (at >= n || !is_digit(body[at])) {
                    return false;
                }
                spec.precision = 0;
                while (at < n && is_digit(body[at])) {
                    spec.precision = spec.precision * 10 + (body[at] - '0');
                    if (spec.precision > 0x7FFF) {
                        return false;
                    }
                    ++at;
                }
            }
            if (at < n) {
                if (body[at] != '%') {
                    return false;                   // a pattern starts with a specifier
                }
                nested = body.substr(at);
            }
            return true;
        }

        constexpr bool read_spec(std::string_view body, format_spec& spec,
                                 std::string_view& nested, int nests) noexcept {
            if (nests == 2) {
                return read_layout_spec(body, spec, nested);
            }
            bool nested_allowed = nests == 1;
            size_t at = 0;
            size_t n = body.size();
            auto aligns = [](char c) noexcept {
                return c == '<' || c == '>' || c == '^';
            };
            bool opens = nested_allowed && n && body[0] == ':';
            if (!opens && at + 1 < n && aligns(body[at + 1])) {
                spec.fill = body[at];
                spec.align = body[at + 1];
                at += 2;
            } else if (at < n && aligns(body[at])) {
                spec.align = body[at++];
            }
            if (at < n && (body[at] == '+' || body[at] == '-' || body[at] == ' ')) {
                spec.sign = body[at++];
            }
            if (at < n && body[at] == '#') {
                spec.alternate = true;
                ++at;
            }
            if (at < n && body[at] == '0') {
                spec.zero = true;
                ++at;
            }
            while (at < n && is_digit(body[at])) {
                // A width that does not fit is a pattern to refuse, not
                // one to wrap around: {:4294967297} padded to one. What
                // it has to fit is the sixteen bits a step of a compiled
                // pattern keeps it in, and not the unsigned it is held
                // in here — which is a bound and not a limit. Before
                // this the bound was the unsigned, so {:4294967295} was
                // read and accepted, and then a caller asking for four
                // thousand million columns of padding got what it asked
                // for: a four gigabyte string from format, or a page of
                // the same from a stencil, out of sixteen characters of
                // a pattern that came from a file. Sixty-five thousand
                // columns is a field nobody has and a page anybody can
                // afford to have refused.
                unsigned digit = unsigned(body[at] - '0');
                if (spec.width > (0xFFFFu - digit) / 10) {
                    return false;
                }
                spec.width = spec.width * 10 + digit;
                ++at;
            }
            if (at < n && body[at] == '.') {
                ++at;
                if (at >= n || !is_digit(body[at])) {
                    return false;                   // a point with no number after it
                }
                spec.precision = 0;
                while (at < n && is_digit(body[at])) {
                    // Kept inside what a step of a pattern holds, and
                    // inside what the writers below can size a buffer
                    // for; past that the pattern is refused
                    spec.precision = spec.precision * 10 + (body[at] - '0');
                    if (spec.precision > 0x7FFF) {
                        return false;
                    }
                    ++at;
                }
            }
            if (at < n && !(nested_allowed && body[at] == ':')) {
                spec.type = body[at++];
            }
            if (at < n) {
                if (!nested_allowed || body[at] != ':') {
                    return false;                   // characters nobody asked for
                }
                nested = body.substr(at + 1);
                at = n;
            }
            return true;
        }

        //----------------------------------------------------------------
        // What a value is made of, asked of the type and not of a list of
        // types. Nothing is included for this: a range is a thing with a
        // begin and an end, a pair is a thing std::tuple_size counts, and
        // both questions are answerable with what <type_traits> and the
        // aliases core already brings are. So sgcl::vector, sgcl::slice,
        // the four im containers, std::vector, std::map and a range of
        // somebody's own all answer the same, and this header still
        // reaches outside itself for four files.
        //----------------------------------------------------------------
        // Text first, because text is a range of characters and must not
        // be taken for one: it has its own formatters above, and a
        // partial specialization over ranges would be neither better nor
        // worse than the one over string_view — which is an ambiguity and
        // not a choice.
        template<class T>
        inline constexpr bool is_text = std::is_convertible_v<T, std::string_view>;

        template<> inline constexpr bool is_text<string> = true;
        template<> inline constexpr bool is_text<slice<char>> = true;
        template<> inline constexpr bool is_text<slice<const char>> = true;

        // Walked with a begin and an end. Asked of a const one, since
        // that is what a formatter is handed.
        template<class T>
        concept walkable = requires(const T& r) {
            r.begin();
            r.end();
            *r.begin();
        };

        template<class R>
        using held_type = std::remove_cvref_t<decltype(*std::declval<const R&>().begin())>;

        // What structured bindings see. A range wins over this where a
        // type is both — std::array and sgcl::array are counted by
        // tuple_size and are still lists of values, and C++23 writes them
        // in brackets.
        template<class T>
        concept tuple_like = requires { typename std::tuple_size<T>::type; };

        // A container that answers by key writes itself in braces, and
        // one that holds a value beside the key writes k: v. That is the
        // standard's own rule, which asks the type and not a list of
        // types.
        template<class T>
        concept has_key_type = requires { typename T::key_type; };

        template<class T>
        concept has_mapped_type = requires { typename T::mapped_type; };

        template<class T>
        concept range_like = walkable<T> && !is_text<T> && !has_format_value<T>;

        template<class T>
        concept tuple_value = tuple_like<T> && !walkable<T> && !is_text<T> && !has_format_value<T>;

        // The two below are needed by the formatters and defined after
        // them, the formatters being what they ask about
        template<class A>
        constexpr bool takes_one(const format_spec& spec, std::string_view nested) noexcept;

        template<class A>
        constexpr bool takes_nested_one() noexcept;

        template<class A>
        constexpr bool takes_layout_one() noexcept;

        template<class A>
        constexpr int nest_mode() noexcept;

        template<class T>
        void write_one(format_sink& out, const T& value, const format_spec& spec,
                       std::string_view nested);

        // The specification one element was given, read the way that
        // element reads specifications: whether a further colon opens
        // another one depends on what the element itself holds, so the
        // same characters are read afresh for each type that meets them.
        template<class E>
        constexpr bool read_for(std::string_view nested, format_spec& spec,
                                std::string_view& inner) noexcept {
            if (!nested.data()) {
                return true;
            }
            return read_spec(nested, spec, inner, nest_mode<E>());
        }

        // A value written inside something else is written the way it
        // would be written in a program, where it has such a way: a list
        // of two words is ["a", "b"] and a list of one word with a comma
        // in it is ["a, b"], and without the quotes those are the same
        // eight characters. Only text and a character have a debug form,
        // so this is nothing at all for a list of numbers, and naming any
        // type at all in the specification — {::s} — takes it back.
        //
        // The standard does the same and for the same reason; it is what
        // set_debug_format is over there. It is the one thing here that
        // changes what an already written program prints.
        template<class E>
        constexpr void as_element(format_spec& spec) noexcept;

        template<class E>
        constexpr bool takes_for(std::string_view nested) noexcept {
            format_spec spec;
            std::string_view inner;
            if (!read_for<E>(nested, spec, inner)) {
                return false;
            }
            as_element<E>(spec);
            return takes_one<E>(spec, inner);
        }

        template<class E>
        void write_for(format_sink& out, const E& value, std::string_view nested) {
            format_spec spec;
            std::string_view inner;
            read_for<E>(nested, spec, inner);
            as_element<E>(spec);
            write_one(out, value, spec, inner);
        }

        //----------------------------------------------------------------
        // One run of memory that the levels of a field share.
        //
        // A body in a field cannot be padded until it is written, the
        // width of it not being known before, so it has to stand
        // somewhere first. Room of the call's own was the obvious answer
        // and was wrong for one reason: a body made of bodies outgrows it
        // at every level at once, and a level that outgrew its room was
        // written a second time — which walks the levels under it a
        // second time as well, so a nest of large levels doubled at each
        // one of them. The counting pass this replaced had the same shape
        // for the same reason and over every size rather than only the
        // large ones.
        //
        // The levels of a field nest properly: the innermost is finished
        // before the one that opened it goes on. So one buffer serves the
        // lot. Each level takes what is free above the level under it and
        // gives it back on the way out, and the memory is the deepest
        // stack of bodies rather than the sum over the calls.
        //
        // Two things follow, and they are the whole of what this class is
        // for. When a level has to grow the buffer, it moves under every
        // level still open beneath it, so they are held in a list and each
        // is put back where it stood: the level that grew it is the only
        // one written again, and the ones that opened it go on as if their
        // room had always been that big. Without that the growth would
        // still have to be paid for by starting each of them over. And the
        // room a level starts with is what the arena already holds, not
        // 256 bytes afresh, so once a thread has written a large field
        // the next one is written once at every level, the innermost too.
        //
        // Measured with the two shapes of this header alternated, best of
        // three, through a template: a nest of eight levels of three
        // hundred bytes each went from 1.72 ms to 51.4 us, of twelve from
        // 27.6 ms to 103 us, of sixteen from 443 ms to 171 us — the first
        // shape four times slower for every two levels, this one a few
        // microseconds a level. A list of 400 numbers in a field of 2000
        // went from 6702 ns to 3680 through format_to, from 13.6 us to
        // 7.6 through format, whose own second pass is where it was, and
        // from 17.8 us to 9.7 through a template: one walk of the body
        // where there were two. A field that fits its first room is
        // within three nanoseconds either way — 63.3 against 66.0 over
        // five numbers of a std::vector in {:>40}, 70.6 against 68.6
        // over the same five in the library's vector — and everything
        // with no width in it is level.
        //
        // Where whoever asked for the field is the level underneath —
        // which is every level but the outermost — the field is padded
        // where it lies, and it is already exactly where that level is
        // writing, so nothing is copied at all: the sink is moved past
        // it. The outermost is copied once, with its padding written
        // round it, into a sink that is somebody else's.
        //----------------------------------------------------------------
        inline constexpr size_t FieldArenaFirst = 256;

        // Past this the buffer is given back when the last level closes,
        // so that a thread which once wrote a page of a hundred kilobytes
        // does not hold it for the rest of its life
        inline constexpr size_t FieldArenaKeep = 64 * 1024;

        // The arena of a thread is three words and nothing that has to be
        // destroyed, and that is measured rather than tidy. A thread_local
        // with a destructor is reached on this platform through two calls,
        // one for the guard that says whether it has been built on this
        // thread and one for the object, and a field that fits its first
        // room paid both at every call: {:>40} over five numbers read 70.6
        // ns against 65.9 before the arena, with every function of both
        // binaries aligned alike so that placement could not be the
        // difference. Built at compile time and with nothing to destroy,
        // it is one call and no guard, and 68.2 against 64.5; the one call
        // left is 0.6 of that, a plain global in its place reading 67.2.
        // The buffer is still given back when the thread ends: the first
        // growth on a thread touches an object that does have a
        // destructor, which is where that guard is paid, once, on the road
        // that is about to allocate anyway.
        class field_arena {
        public:
            class level;

            static field_arena& here() noexcept;

        private:
            friend class level;
            friend struct field_arena_reaper;

            // Room for at least `want` bytes, everything still open put
            // back where it stood
            void _grow(size_t want);

            void _release() noexcept {
                delete[] _room;
                _room = nullptr;
                _cap = 0;
            }

            size_t _held(size_t cap) const noexcept;
            void _reseat(size_t was) noexcept;

            char* _room = nullptr;
            size_t _cap = 0;
            level* _open = nullptr;
        };

        inline thread_local constinit field_arena this_threads_field_arena;

        inline field_arena& field_arena::here() noexcept {
            return this_threads_field_arena;
        }

        // What gives the buffer back when the thread ends
        struct field_arena_reaper {
            ~field_arena_reaper() {
                this_threads_field_arena._release();
            }
        };

        inline void field_arena::_grow(size_t want) {
            static thread_local field_arena_reaper reaper;
            (void)reaper;
            size_t doubled = _cap * 2;
            size_t n = want > doubled ? want : doubled;
            if (n < FieldArenaFirst) {
                n = FieldArenaFirst;
            }
            char* room = new char[n];
            size_t was = _cap;
            size_t keep = _held(was);
            if (keep) {
                sgcl::detail::copy_bytes(room, _room, keep);
            }
            delete[] _room;
            _room = room;
            _cap = n;
            _reseat(was);
        }

        // One level of a field: what is free above the level under it,
        // and the sink the body writes into
        class field_arena::level {
        public:
            explicit level(format_sink& into)
            : _arena(field_arena::here())
            , _under(_arena._open)
            , _into(&into)
            , _mark(_under ? _under->_mark + _under->_written(_arena._cap) : 0)
            , _sink(_open_room(), _arena._cap - _mark) {
                _arena._open = this;
            }

            ~level() {
                if (_arena._open == this) {
                    _arena._open = _under;
                }
            }

            level(const level&) = delete;
            level& operator=(const level&) = delete;

            format_sink& out() noexcept {
                return _sink;
            }

            // Whether the body ran off the end of what was free, now or
            // at any moment before the arena last grew
            bool ran_off() const noexcept {
                return _lost || _sink.size() > _arena._cap - _mark;
            }

            // Room for the whole of it, and this one level started again.
            // The levels under it keep what they have written; only the
            // one that ran off is asked for anything twice, and only
            // until the arena is as big as the page needs.
            void take_room() {
                _arena._grow(_mark + _sink.size());
                _sink.reseat(_arena._room + _mark, _arena._cap - _mark, 0);
                _lost = false;
            }

            // The body padded where it lies, and handed to whoever asked
            void close(const format_spec& spec) {
                // What is there and not what was counted. The two are one
                // number for a body that says the same thing twice, which
                // is every body this header writes; a format_value of the
                // caller's that answers longer the second time is asked
                // for garbage and gets it, but not memory past the room.
                size_t n = _written(_arena._cap);
                char* base = _arena._room;
                size_t cols = columns_of_text(std::string_view(base + _mark, n));
                size_t pad = spec.width > cols ? spec.width - cols : 0;
                char how = spec.align ? spec.align : '<';
                size_t left = how == '>' ? pad : how == '^' ? pad / 2 : 0;
                if (!_under || _into != &_under->_sink) {
                    _arena._open = _under;
                    // Somebody else's sink: the padding goes straight into
                    // it round the body, which is one copy of the body and
                    // not a move inside the arena followed by a copy
                    _into->fill(spec.fill, left);
                    _into->put(base + _mark, n);
                    _into->fill(spec.fill, pad - left);
                    if (!_under && _arena._cap > FieldArenaKeep) {
                        _arena._release();
                    }
                    return;
                }
                if (pad) {
                    if (_arena._cap - _mark < n + pad) {
                        _arena._grow(_mark + n + pad);
                        base = _arena._room;
                    }
                    if (left) {
                        sgcl::detail::move_bytes(base + _mark + left, base + _mark, n);
                        sgcl::detail::fill_bytes(base + _mark, (unsigned char)spec.fill, left);
                    }
                    if (pad - left) {
                        sgcl::detail::fill_bytes(base + _mark + left + n,
                                                 (unsigned char)spec.fill, pad - left);
                    }
                    n += pad;
                }
                // Only now, the growth above having had to carry this
                // level's body over with the rest
                _arena._open = _under;
                // The field stands exactly where the level under it is
                // writing, so there is nothing to copy: that sink is moved
                // past what this one left there
                _under->_sink.reseat(base + _mark + n, _arena._cap - _mark - n,
                                     _under->_sink.size() + n);
            }

        private:
            friend class field_arena;

            // What this level has really put there, which is what it
            // counted unless it ran off the end
            size_t _written(size_t cap) const noexcept {
                size_t n = _sink.size();
                size_t room = cap - _mark;
                return n < room ? n : room;
            }

            // Room enough to start with, before the sink is built over it
            char* _open_room() {
                if (_arena._cap < _mark + FieldArenaFirst) {
                    _arena._grow(_mark + FieldArenaFirst);
                }
                return _arena._room + _mark;
            }

            field_arena& _arena;
            level* _under;
            format_sink* _into;
            size_t _mark;
            format_sink _sink;
            bool _lost = false;
        };

        inline size_t field_arena::_held(size_t cap) const noexcept {
            return _open ? _open->_mark + _open->_written(cap) : 0;
        }

        // Every level still open put back over the new room, each where its
        // bytes stop.
        //
        // Where its bytes stop and not where its count does, which are
        // two places for a level that had already run off the end — a
        // level whose last element filled the room exactly, say, and
        // whose ", " after it went nowhere. That level's count is right
        // and its bytes are two short, and the room now being larger,
        // the test in ran_off() would no longer see it: the level would
        // go on writing two bytes early and close over a body with the
        // separator missing and two bytes of whatever stood at the end.
        // So it remembers that it ran off, and is written again when its
        // body is done, exactly as if the room had never grown. Found by
        // writing every field of a random nest with both shapes of this
        // header and comparing the two; the old shape never lost the
        // separator because its room never moved.
        inline void field_arena::_reseat(size_t was) noexcept {
            for (level* l = _open; l; l = l->_under) {
                size_t counted = l->_sink.size();
                size_t written = l->_written(was);
                if (counted > written) {
                    l->_lost = true;
                }
                l->_sink.reseat(_room + l->_mark + written,
                                _cap - l->_mark - written, counted);
            }
        }

        // A body with a width: into the arena above, padded where it lies,
        // in columns, so that a list of Polish words sits in its field as
        // a word does.
        //
        // Out of line, and that is measured rather than tidy. Written in
        // the formatter that calls it, this road made the formatter's own
        // code bigger for every call and not only the ones with a width,
        // and the call that has none paid: a list with a specification
        // handed to its elements and none of its own, {::>5}, read 79.4 ns
        // against 74.9 before the arena. Out of line it reads 72.7
        // against 74.3, and the road that does come here pays one call
        // for a field it is about to write a whole body into.
        template<class Body>
        SGCL_NOINLINE void put_body_in_wide_field(format_sink& out, const format_spec& spec,
                                                  Body& body) {
            field_arena::level held(out);
            body(held.out());
            if (held.ran_off()) [[unlikely]] {
                // Only until the arena is as big as this page wants. The
                // levels still open beneath this one are moved with it
                // rather than written again, so a nest is walked once
                // and not twice a level.
                held.take_room();
                body(held.out());
            }
            held.close(spec);
        }

        // A thing made of other things, in its field. With no width asked
        // for — which is the common call — the elements go straight into
        // the sink and nothing is held anywhere, and the arena is out of
        // the way. A width cannot be paid that way: what to pad by is not
        // known until the whole is written, and the whole is not a string
        // anybody has.
        template<class Body>
        void put_body_in_field(format_sink& out, const format_spec& spec, Body&& body) {
            if (!spec.width) {
                body(out);
                return;
            }
            put_body_in_wide_field(out, spec, body);
        }
    }

    //--------------------------------------------------------------------
    // What a value may be told to do with itself. A type of the library
    // or of the standard is written by one of these; anything else is
    // written by a format_value found beside it (below).
    //--------------------------------------------------------------------
    // Defined and empty, not merely declared: a type with no formatter
    // of its own must answer "no member" where this is asked about it —
    // a requires-expression sees that, where an incomplete type is an
    // error before anything gets to ask
    template<class T, class = void>
    struct formatter {};

    template<class T>
    requires std::is_integral_v<T> && (!std::is_same_v<T, bool>) && (!std::is_same_v<T, char>)
    struct formatter<T> {
        // The compiler's own wide integers are turned away rather than
        // written wrongly. Clang answers true to is_integral_v for
        // __int128, so without this the type was taken and then narrowed
        // to sixty-four bits by the writing below: one followed by a
        // hundred noughts came out as 0, and a value seven above it as 7.
        // A number written as another number is the worst answer a
        // formatter can give, and the standard's own format does not
        // take these types either. Cast, or write the digits by hand.
        static_assert(sizeof(T) <= sizeof(unsigned long long),
                      "txt::format does not write integers wider than long long "
                      "(__int128 and the like): it would narrow them silently");
        static constexpr bool takes(char type) noexcept {
            return !type || type == 'd' || type == 'b' || type == 'B' || type == 'o'
                || type == 'x' || type == 'X' || type == 'c';
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static void write(format_sink& out, T value, const format_spec& spec) {
            if (spec.type == 'c') {
                // The value has to be one a character can hold. Narrowing
                // it quietly writes a different character — {:c} of 300
                // came out as a comma — and the number is not known where
                // the pattern is read, so this is the only place the
                // question can be asked.
                bool holds;
                if constexpr (std::is_signed_v<T>) {
                    holds = value >= T(std::numeric_limits<char>::min())
                         && value <= T(std::numeric_limits<char>::max());
                } else {
                    // No lower bound to ask about, and T(-128) would be a
                    // very large number to compare against
                    holds = value <= T(std::numeric_limits<char>::max());
                }
                if (!holds) {
                    throw out_of_range("sgcl::txt::format: {:c} of a value no character holds");
                }
                char c = char(value);
                detail::put_padded(out, {&c, 1}, spec);
                return;
            }
            detail::write_integer(out, value, spec);
        }
    };

    template<>
    struct formatter<bool> {
        static constexpr bool takes(char type) noexcept {
            return !type || type == 's' || type == 'd' || type == 'b' || type == 'B'
                || type == 'o' || type == 'x' || type == 'X';
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static void write(format_sink& out, bool value, const format_spec& spec) {
            if (!spec.type || spec.type == 's') {
                detail::write_text(out, value ? "true" : "false", spec);
                return;
            }
            detail::write_integer<unsigned>(out, value ? 1u : 0u, spec);
        }
    };

    template<>
    struct formatter<char> {
        static constexpr bool takes(char type) noexcept {
            return !type || type == 'c' || type == 'd' || type == 'b' || type == 'B'
                || type == 'o' || type == 'x' || type == 'X' || type == '?';
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static void write(format_sink& out, char value, const format_spec& spec) {
            if (spec.type == '?') {
                // In single quotes, which is the quote a character has to
                // have escaped inside it
                detail::write_debug_text(out, {&value, 1}, spec, '\'');
                return;
            }
            if (!spec.type || spec.type == 'c') {
                detail::put_padded(out, {&value, 1}, spec);
                return;
            }
            detail::write_integer<unsigned char>(out, (unsigned char)value, spec);
        }
    };

    // A code point, which is a character and not a number: the library's
    // text is UTF-8, so it goes out as the bytes it is written with
    template<>
    struct formatter<char32_t> {
        static constexpr bool takes(char type) noexcept {
            return !type || type == 'c' || type == 'd' || type == 'x' || type == 'X'
                || type == 'b' || type == 'B' || type == 'o' || type == '?';
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static void write(format_sink& out, char32_t value, const format_spec& spec) {
            if (!spec.type || spec.type == 'c' || spec.type == '?') {
                char buf[utf8::max_width];
                size_t n = utf8::encode(value, buf);
                if (spec.type == '?') {
                    detail::write_debug_text(out, {buf, n}, spec, '\'');
                    return;
                }
                detail::put_padded(out, {buf, n}, spec);
                return;
            }
            detail::write_integer<uint32_t>(out, uint32_t(value), spec);
        }
    };

    template<class T>
    requires std::is_floating_point_v<T>
    struct formatter<T> {
        static constexpr bool takes(char type) noexcept {
            return !type || type == 'f' || type == 'F' || type == 'e' || type == 'E'
                || type == 'g' || type == 'G' || type == 'a' || type == 'A';
        }

        static constexpr bool takes_precision() noexcept {
            return true;
        }

        static void write(format_sink& out, T value, const format_spec& spec) {
            // long double is written through double, which loses nothing
            // where the two are the same type and is what the standard's
            // own to_chars offers where they are not
            if constexpr (std::is_same_v<T, float>) {
                detail::write_float(out, value, spec);
            } else {
                detail::write_float(out, double(value), spec);
            }
        }
    };

    template<class T>
    requires std::is_convertible_v<T, std::string_view>
    struct formatter<T> {
        static constexpr bool takes(char type) noexcept {
            return !type || type == 's' || type == '?';
        }

        static constexpr bool takes_precision() noexcept {
            return true;
        }

        static void write(format_sink& out, std::string_view value, const format_spec& spec) {
            detail::write_text_or_debug(out, value, spec);
        }
    };

    // The library's own text, which holds what it points at
    template<>
    struct formatter<string> {
        static constexpr bool takes(char type) noexcept {
            return !type || type == 's' || type == '?';
        }

        static constexpr bool takes_precision() noexcept {
            return true;
        }

        static void write(format_sink& out, const string& value, const format_spec& spec) {
            detail::write_text_or_debug(out, value.view(), spec);
        }
    };

    template<>
    struct formatter<slice<const char>> {
        static constexpr bool takes(char type) noexcept {
            return !type || type == 's' || type == '?';
        }

        static constexpr bool takes_precision() noexcept {
            return true;
        }

        static void write(format_sink& out, const slice<const char>& value, const format_spec& spec) {
            detail::write_text_or_debug(out, value.view(), spec);
        }
    };

    // The library's span of time (core/duration.h): Go's text
    // ("1h30m0.5s"), as to_string writes it, in a field. Here and not in
    // time, which it does not need: `txt::format("{}", d)` with txt alone
    template<>
    struct formatter<duration> {
        static constexpr bool takes(char type) noexcept {
            return !type || type == 's';
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static void write(format_sink& out, duration d, const format_spec& spec) {
            string text = d.to_string();
            detail::put_padded(out, std::string_view(text), spec);
        }
    };

    template<class T>
    requires std::is_pointer_v<T> && (!std::is_convertible_v<T, std::string_view>)
    struct formatter<T> {
        static constexpr bool takes(char type) noexcept {
            return !type || type == 'p';
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static void write(format_sink& out, const void* value, const format_spec& spec) {
            format_spec as_hex = spec;
            as_hex.type = 'x';
            as_hex.alternate = true;
            detail::write_integer<uintptr_t>(out, uintptr_t(value), as_hex);
        }
    };

    // An enumeration, as the number it is. Neither an `enum class` nor a
    // plain `enum` is an integral type, so before this neither had a
    // formatter at all and neither compiled — which is a poor answer for
    // a value that has a perfectly good one.
    //
    // The number and not the name, because the names are not here to be
    // had: C++20 has no reflection, and a table of them would have to be
    // written by the caller anyway. So the whole of the numeric
    // specification is offered — {:d}, {:x}, {:#x}, {:b}, {:o}, a width,
    // a sign — over the underlying type, which is what decides whether
    // there is a sign to write at all.
    //
    // Not {:c} and not {:s}. A character is not what an enumeration is,
    // and 's' is left free on purpose: it is the shape a caller reaches
    // for when they do write the names, and this must not have taken it.
    //
    // It stands aside where the type's own namespace says better. A
    // format_value beside the enumeration wins over this — that is what
    // the constraint is for — so writing the names is the same thing it
    // is for any other type of the caller's, and nothing here has to be
    // opened or specialized to do it.
    template<class T>
    requires std::is_enum_v<T> && (!detail::has_format_value<T>)
    struct formatter<T> {
        using number = std::underlying_type_t<T>;

        static constexpr bool takes(char type) noexcept {
            return !type || type == 'd' || type == 'b' || type == 'B' || type == 'o'
                || type == 'x' || type == 'X';
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static void write(format_sink& out, T value, const format_spec& spec) {
            detail::write_integer<number>(out, static_cast<number>(value), spec);
        }
    };

    //--------------------------------------------------------------------
    // Values made of other values: a list in brackets, a table in braces,
    // a pair in parentheses, and a value that may not be there at all.
    // The shapes are the ones C++23 settled on, because they are the ones
    // people already read — [1, 2, 3], {"a": 1}, (1, 2) — and because a
    // second set of them would only have to be learned.
    //
    // What follows a second colon belongs to the elements: {::>4} pads
    // every number of a list to four, {:n} writes a list without its
    // brackets, and the two compose. It goes down as many levels as the
    // type has, each level reading one colon and handing on the rest.
    //--------------------------------------------------------------------
    // Anything with a begin and an end: sgcl::vector, sgcl::slice,
    // sgcl::array, the im containers, the standard's, a range of
    // somebody's own. Text is not one of them and neither is a pair,
    // which have their own roads.
    template<class R>
    requires detail::range_like<R>
    struct formatter<R> {
        using element = detail::held_type<R>;

        // A container that answers by key is a table and writes itself in
        // braces; one that holds a value beside the key writes its
        // elements as k: v, which is what 'm' asks of a pair
        static constexpr bool keyed = detail::has_key_type<R>;
        static constexpr bool paired =
            keyed && detail::has_mapped_type<R> && detail::tuple_like<element>;

        static constexpr bool takes(char type) noexcept {
            return !type || type == 'n';
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_nested(std::string_view nested) noexcept {
            return detail::takes_for<element>(nested);
        }

        static void write(format_sink& out, const R& value, const format_spec& spec,
                          std::string_view nested) {
            // Read once for the whole and not once an element: the
            // characters are the same every time round
            format_spec inner;
            std::string_view deeper;
            detail::read_for<element>(nested, inner, deeper);
            detail::as_element<element>(inner);
            if constexpr (paired) {
                if (!inner.type) {
                    inner.type = 'm';
                }
            }
            bool bare = spec.type == 'n';
            detail::put_body_in_field(out, spec, [&](format_sink& to) {
                if (!bare) {
                    to.put(keyed ? '{' : '[');
                }
                bool first = true;
                for (const auto& e : value) {
                    if (!first) {
                        to.put(", ", 2);
                    }
                    first = false;
                    detail::write_one(to, e, inner, deeper);
                }
                if (!bare) {
                    to.put(keyed ? '}' : ']');
                }
            });
        }
    };

    // A pair or a tuple, as anything structured bindings see: (a, b).
    // 'n' drops the parentheses and 'm', over two values, writes a: b —
    // which is what one element of a table is, and how a map writes its
    // own without knowing anything about pairs.
    template<class T>
    requires detail::tuple_value<T>
    struct formatter<T> {
        static constexpr size_t count = std::tuple_size<T>::value;

        static constexpr bool takes(char type) noexcept {
            return !type || type == 'n' || (type == 'm' && count == 2);
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_nested(std::string_view nested) noexcept {
            return takes_each(nested, std::make_index_sequence<count>{});
        }

        static void write(format_sink& out, const T& value, const format_spec& spec,
                          std::string_view nested) {
            bool keyed = spec.type == 'm';
            bool bare = spec.type == 'n' || keyed;
            detail::put_body_in_field(out, spec, [&](format_sink& to) {
                if (!bare) {
                    to.put('(');
                }
                write_each(to, value, keyed, nested, std::make_index_sequence<count>{});
                if (!bare) {
                    to.put(')');
                }
            });
        }

    private:
        // Every element reads the characters for itself, because what a
        // further colon means is the element's own business and the
        // elements of a pair are not of one type
        template<size_t... I>
        static constexpr bool takes_each(std::string_view nested,
                                         std::index_sequence<I...>) noexcept {
            return (detail::takes_for<std::remove_cvref_t<std::tuple_element_t<I, T>>>(nested)
                    && ...);
        }

        template<size_t... I>
        static void write_each(format_sink& to, const T& value, bool keyed,
                               std::string_view nested, std::index_sequence<I...>) {
            bool first = true;
            auto one = [&](const auto& e) {
                if (!first) {
                    to.put(keyed ? ": " : ", ", 2);
                }
                first = false;
                detail::write_for(to, e, nested);
            };
            (one(std::get<I>(value)), ...);
        }
    };

    // A value that may not be there. What is there is written with the
    // whole of its own specification, so that {:>6} lines a column up
    // whether or not this row has a number in it; what is not there is
    // the word nullopt, in the same field.
    //
    // And what is there is written the way it would be written in a
    // program, as the element of a range is and for the same reason:
    // without that, an optional<string> holding the seven letters of
    // "nullopt" and one holding nothing are the same seven letters.
    // {:s} takes it back, and a number was never quoted to begin with.
    template<class T>
    struct formatter<optional<T>> {
        static constexpr bool takes(char type) noexcept {
            if constexpr (requires { formatter<T>::takes(type); }) {
                return formatter<T>::takes(type);
            } else {
                return true;
            }
        }

        static constexpr bool takes_precision() noexcept {
            if constexpr (requires { formatter<T>::takes_precision(); }) {
                return formatter<T>::takes_precision();
            } else {
                return true;
            }
        }

        // Only where what it holds takes one, so that {::>6} over an
        // optional number still means a colon to pad with
        static constexpr bool takes_nested(std::string_view nested) noexcept
        requires (detail::takes_nested_one<T>()) {
            return formatter<T>::takes_nested(nested);
        }

        // And a pattern where what it holds reads one: {:%H:%M} of an
        // optional time
        static constexpr bool takes_layout(std::string_view layout) noexcept
        requires (detail::takes_layout_one<T>()) {
            return formatter<T>::takes_layout(layout);
        }

        static void write(format_sink& out, const optional<T>& value, const format_spec& spec,
                          std::string_view nested) {
            if (!value) {
                // The word has no type and no precision of its own: {:d}
                // of a number that is not there is still the word
                format_spec plain = spec;
                plain.type = 0;
                plain.precision = -1;
                detail::write_text(out, "nullopt", plain);
                return;
            }
            format_spec held = spec;
            detail::as_element<T>(held);
            detail::write_one(out, *value, held, nested);
        }
    };
}

namespace sgcl::txt {
    namespace detail {
        // A value written by the formatter its type names, or — for a
        // type of the caller's — by the format_value found beside it.
        // A formatter that holds other values takes the specification
        // they were given as well; one that does not never sees it, and
        // the parameter costs it nothing, being a register the call does
        // not read.
        template<class T>
        void write_one(format_sink& out, const T& value, const format_spec& spec,
                       std::string_view nested) {
            using V = std::decay_t<T>;
            if constexpr (requires { formatter<V>::write(out, value, spec, nested); }) {
                formatter<V>::write(out, value, spec, nested);
            } else if constexpr (requires { formatter<V>::write(out, value, spec); }) {
                formatter<V>::write(out, value, spec);
            } else {
                static_assert(has_format_value<V>,
                              "this type says nothing about how it is written: give it a "
                              "format_value(format_sink&, const T&, const format_spec&) beside it");
                format_value(out, value, spec);
            }
        }

        template<class E>
        constexpr void as_element(format_spec& spec) noexcept {
            if constexpr (requires { formatter<E>::takes('?'); }) {
                if (!spec.type && formatter<E>::takes('?')) {
                    spec.type = '?';
                }
            }
        }

        // Whether one type accepts that specification. A type of the
        // caller's, which says nothing but its format_value, accepts
        // whatever it is given: the question has to be asked with
        // if constexpr and not with a ?:, whose arms are both read
        // whichever one is taken
        template<class A>
        constexpr bool takes_one(const format_spec& spec, std::string_view nested) noexcept {
            if constexpr (requires { formatter<A>::takes(spec.type); }) {
                if (!formatter<A>::takes(spec.type)
                    || (spec.precision >= 0 && !formatter<A>::takes_precision())) {
                    return false;
                }
                if constexpr (requires { formatter<A>::takes_layout(nested); }) {
                    return formatter<A>::takes_layout(nested);
                } else if constexpr (requires { formatter<A>::takes_nested(nested); }) {
                    return formatter<A>::takes_nested(nested);
                } else {
                    // Nothing was handed on and nothing could have been:
                    // walk only reads a second colon as one for a type
                    // that asked to be given one
                    return nested.data() == nullptr;
                }
            } else {
                return true;
            }
        }

        // Whether the n-th of these types holds other values, and so
        // reads a second colon as theirs rather than as a character to
        // pad with. Asked before the specification is read, which is why
        // it cannot simply be part of takes_one.
        template<class A>
        constexpr bool takes_nested_one() noexcept {
            return requires { formatter<A>::takes_nested(std::string_view{}); };
        }

        // Whether the type reads the rest of its field as a pattern of
        // its own (a time: read_layout_spec above)
        template<class A>
        constexpr bool takes_layout_one() noexcept {
            return requires { formatter<A>::takes_layout(std::string_view{}); };
        }

        // What a colon or a '%' after the specification means for this
        // type, as read_spec takes it: 2 a pattern, 1 a specification for
        // what it holds, 0 neither
        template<class A>
        constexpr int nest_mode() noexcept {
            return takes_layout_one<A>() ? 2 : takes_nested_one<A>() ? 1 : 0;
        }

        template<class... A>
        constexpr int takes_nested_spec(size_t n) noexcept {
            int mode = 0;
            size_t i = 0;
            (void)((i++ == n ? (mode = nest_mode<A>(), true) : false) || ...);
            return mode;
        }

        // Whether the n-th of these types accepts that specification,
        // asked where the program is built
        template<class... A>
        constexpr bool takes_spec(size_t n, const format_spec& spec,
                                  std::string_view nested) noexcept {
            bool ok = false;
            size_t i = 0;
            (void)((i++ == n ? (ok = takes_one<A>(spec, nested), true) : false) || ...);
            return ok;
        }

        template<class... A>
        void write_nth(format_sink& out, size_t n, const format_spec& spec,
                       std::string_view nested, const A&... args) {
            size_t i = 0;
            (void)((i++ == n ? (write_one(out, args, spec, nested), true) : false) || ...);
        }

        // Where the run of literal text starting at `at` ends: the next
        // brace of either kind, or the end. The plain loop asks two
        // questions of every byte and costs half a nanosecond each, which
        // over a pattern of any length was more than everything else the
        // call did — a literal of a hundred and twenty-three characters
        // read 63 ns. Sixteen bytes at a time in NEON, then eight in a
        // word where there is no NEON or fewer than sixteen are left,
        // then one at a time: 6.8 ns for the same literal and 6.0 for one
        // of forty-five, against 9.8 and 7.1 for two calls to memchr,
        // whose fixed cost does not come back over a short run.
        //
        // Nothing is read outside the text. The usual trick for the last
        // few bytes — load the aligned block around them and mask — is
        // twice as fast again on a run of five, and it is undefined
        // behaviour that the address sanitizer rightly reports, so it is
        // not here.
        constexpr size_t next_brace(std::string_view fmt, size_t at) noexcept {
            if (!std::is_constant_evaluated()) {
                const char* p = fmt.data();
                size_t n = fmt.size();
#if defined(__ARM_NEON)
                const uint8x16_t open = vdupq_n_u8('{');
                const uint8x16_t close = vdupq_n_u8('}');
                for (; at + 16 <= n; at += 16) {
                    uint8x16_t v = vld1q_u8((const uint8_t*)p + at);
                    uint8x16_t hit = vorrq_u8(vceqq_u8(v, open), vceqq_u8(v, close));
                    // Four bits a byte, which is what a mask of bytes
                    // narrows to on this instruction set
                    uint64_t bits = vget_lane_u64(
                        vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(hit), 4)), 0);
                    if (bits) {
                        return at + (size_t(__builtin_ctzll(bits)) >> 2);
                    }
                }
#endif
                // The same question of eight bytes in a word, and the
                // answer in the high bit of each: portable, and what the
                // last bytes of a NEON pass are left to anyway
                constexpr uint64_t Ones = 0x0101010101010101ull;
                constexpr uint64_t Highs = 0x8080808080808080ull;
                for (; at + 8 <= n; at += 8) {
                    uint64_t w;
                    std::memcpy(&w, p + at, 8);
                    uint64_t a = w ^ (Ones * '{');
                    uint64_t b = w ^ (Ones * '}');
                    uint64_t hit = (((a - Ones) & ~a) | ((b - Ones) & ~b)) & Highs;
                    if (hit) {
                        return at + (size_t(__builtin_ctzll(hit)) >> 3);
                    }
                }
            }
            while (at < fmt.size() && fmt[at] != '{' && fmt[at] != '}') {
                ++at;
            }
            return at;
        }

        // The pattern walked once. What is done with each piece is the
        // caller's: writing it, or — where the compiler reads the
        // pattern — asking whether the value of that number is written
        // that way. Both walks are this one, so neither can drift.
        //
        // `nests` answers, of the value a field names, whether a second
        // colon in its specification opens one for what the value holds.
        // It has to be asked before the specification is read and only
        // the caller knows the types, so it comes in as a third piece of
        // the caller's.
        template<class Text, class Field, class Nests>
        constexpr bool walk(std::string_view fmt, size_t count, Text&& text, Field&& field,
                            Nests&& nests) {
            size_t next = 0;                        // the value a bare {} takes
            for (size_t i = 0; i < fmt.size();) {
                if (fmt[i] != '{' && fmt[i] != '}') {
                    size_t from = i;
                    i = next_brace(fmt, i);
                    text(fmt.data() + from, i - from);
                    continue;
                }
                if (i + 1 < fmt.size() && fmt[i + 1] == fmt[i]) {
                    text(fmt.data() + i, size_t(1));
                    i += 2;
                    continue;
                }
                if (fmt[i] == '}') {
                    return false;                   // a closing brace on its own
                }
                ++i;
                size_t which = next;
                if (i < fmt.size() && is_digit(fmt[i])) {
                    which = 0;
                    while (i < fmt.size() && is_digit(fmt[i])) {
                        // Refused here rather than wrapped around and
                        // taken for a smaller one. A call has a handful
                        // of values, so the test below would catch every
                        // number anybody writes by hand — but not one
                        // that has gone round: {18446744073709551617}
                        // over two values came back as the second, which
                        // is a pattern that must not be written at all
                        // answering as a pattern that says something
                        // else. Past what a count can be, and the whole
                        // of it stops.
                        if (which > (size_t(-1) - 9) / 10) {
                            return false;
                        }
                        which = which * 10 + size_t(fmt[i++] - '0');
                    }
                } else {
                    ++next;
                }
                if (which >= count) {
                    return false;                   // no value of that number
                }
                format_spec spec;
                std::string_view nested;
                if (i < fmt.size() && fmt[i] == ':') {
                    // A specification holds no brace of its own, so the
                    // field ends at the first one. That is what puts a
                    // closing brace out of reach as a character to pad
                    // with — {:}<6} used to mean one — and it is out of
                    // reach in C++23 for the same reason. An opening one
                    // still pads, since nothing looks for it.
                    size_t end = i + 1;
                    while (end < fmt.size() && fmt[end] != '}') {
                        ++end;
                    }
                    if (end == fmt.size()) {
                        return false;               // the specification does not end
                    }
                    if (!read_spec(fmt.substr(i + 1, end - i - 1), spec, nested, nests(which))) {
                        return false;
                    }
                    i = end;
                }
                if (i >= fmt.size() || fmt[i] != '}') {
                    return false;                   // the field does not end
                }
                ++i;                                // past the closing brace
                if (!field(which, spec, nested)) {
                    return false;                   // that value is not written that way
                }
            }
            return true;
        }

        // The reading the compiler does: nothing is written and every
        // field is asked whether the value it names takes it
        template<class... A>
        constexpr bool fits(std::string_view fmt) {
            return walk(fmt, sizeof...(A),
                        [](const char*, size_t) {},
                        [](size_t which, const format_spec& spec, std::string_view nested) {
                            return takes_spec<A...>(which, spec, nested);
                        },
                        [](size_t which) { return takes_nested_spec<A...>(which); });
        }

        template<class... A>
        void run(format_sink& out, std::string_view fmt, const A&... args) {
            walk(fmt, sizeof...(A),
                 [&out](const char* at, size_t n) { out.put(at, n); },
                 [&](size_t which, const format_spec& spec, std::string_view nested) {
                     write_nth(out, which, spec, nested, args...);
                     return true;
                 },
                 [](size_t which) { return takes_nested_spec<std::decay_t<A>...>(which); });
        }

        // The same walk, with the asking the compiler does put back into
        // it: for a pattern that was never compiled there is nobody to
        // have asked. One pass and not two — every field is weighed just
        // before it is written, and walk stops at the first that does not
        // fit — because whatever was written by then is thrown away.
        template<class... A>
        bool run_checked(format_sink& out, std::string_view fmt, const A&... args) {
            return walk(fmt, sizeof...(A),
                        [&out](const char* at, size_t n) { out.put(at, n); },
                        [&](size_t which, const format_spec& spec, std::string_view nested) {
                            if (!takes_spec<std::decay_t<A>...>(which, spec, nested)) {
                                return false;
                            }
                            write_nth(out, which, spec, nested, args...);
                            return true;
                        },
                        [](size_t which) { return takes_nested_spec<std::decay_t<A>...>(which); });
        }

        // One step of a pattern already read: the run of literal text and
        // the field that follows it, with the specification already made
        // out. A pattern keeps these as an array settled where the
        // program is built, so that what happens when it runs is a walk
        // over as many steps as it has fields, and not over its
        // characters — reading `{:>8}` again on every call cost 6.7 ns of
        // the 19.8 the whole of it took.
        constexpr uint8_t NoValue = uint8_t(-1);

        // The same fields a format_spec has, in eighteen bytes rather
        // than thirty-two. It matters because the pattern is built afresh
        // at every call — it is a temporary the conversion makes at the
        // call site — and what that costs is the bytes it takes: at 280
        // of them the building cost 3.5 ns of a call, and over a literal
        // with nothing to substitute, where there was nothing else to do,
        // 8.9.
        //
        // The specification a value hands on to what it holds is not kept
        // here as characters but as where they stand in the pattern,
        // which the step already has a pointer to: three bytes rather
        // than a view, and a step that has none — every step of every
        // pattern with no range in it — spends nothing on reading them.
        struct format_part {
            uint16_t at = 0;              // where the literal run starts
            uint16_t size = 0;            // and how long it is
            uint16_t width = 0;
            int16_t precision = -1;
            uint16_t nested_at = 0;       // and where what it holds is told
            uint8_t nested_size = 0;
            uint8_t which = NoValue;      // the value written after it
            uint8_t flags = 0;            // 1 alternate, 2 zero, 4 a nested spec
            char fill = ' ';
            char align = 0;
            char sign = 0;
            char type = 0;
        };

        constexpr std::string_view nested_of(const format_part& part, const char* text) noexcept {
            if (!(part.flags & 4)) {
                return {};
            }
            return std::string_view(text + part.nested_at, part.nested_size);
        }

        constexpr format_spec spec_of(const format_part& part) noexcept {
            format_spec spec;
            spec.fill = part.fill;
            spec.align = part.align;
            spec.sign = part.sign;
            spec.alternate = (part.flags & 1) != 0;
            spec.zero = (part.flags & 2) != 0;
            spec.width = part.width;
            spec.precision = part.precision;
            spec.type = part.type;
            return spec;
        }

        // Four steps covers a message; one with more of them, or with a
        // number too large for the fields above, keeps none and is read
        // again when it runs — which is what this replaces, and correct
        constexpr size_t MaxParts = 4;

        template<class... A>
        void run_parts(format_sink& out, const char* text, const format_part* parts,
                       size_t count, const A&... args) {
            for (size_t i = 0; i < count; ++i) {
                const format_part& part = parts[i];
                if (part.size) {
                    out.put(text + part.at, part.size);
                }
                if (part.which != NoValue) {
                    write_nth(out, part.which, spec_of(part), nested_of(part, text), args...);
                }
            }
        }
    }

    // The pattern, read where the program is compiled. A literal turns
    // into one of these on its way into format, and the reading happens
    // there: a brace left open, a number with no value behind it, a
    // precision asked of a whole number — each is an error of the
    // compiler, with the pattern in hand. A pattern that is not a
    // constant does not come this way at all, which is deliberate.
    template<class... A>
    class format_pattern {
    public:
        // One pass over the pattern does both: every field is asked
        // whether the value it names takes it, and every step is written
        // down so that nothing has to be read again when the program
        // runs. A pattern longer than the room kept for the steps keeps
        // none, and is read the old way — correct either way.
        template<class S>
        requires std::is_convertible_v<const S&, std::string_view>
        consteval format_pattern(const S& text)
        : _text(text) {
            size_t at = 0;
            size_t size = 0;
            bool room = true;
            // What emit is given where a step has no field of its own.
            // Not detail::NoValue: that is a uint8_t, and a value whose
            // number happened to be 255 would be taken for it and its
            // field silently dropped.
            constexpr size_t NoField = size_t(-1);
            auto emit = [&](size_t which, const format_spec& spec, std::string_view nested) {
                size_t nested_at = nested.data() ? size_t(nested.data() - _text.data()) : 0;
                // Everything must fit the narrow fields of a part, and
                // there must be a part left to put it in
                if (_count == detail::MaxParts
                    || at > 0xFFFF || size > 0xFFFF
                    || spec.width > 0xFFFF || spec.precision > 0x7FFF
                    || nested_at > 0xFFFF || nested.size() > 0xFF
                    || (which != NoField && which >= detail::NoValue)) {
                    room = false;
                    return;
                }
                detail::format_part part;
                part.at = uint16_t(at);
                part.size = uint16_t(size);
                part.width = uint16_t(spec.width);
                part.precision = int16_t(spec.precision);
                part.nested_at = uint16_t(nested_at);
                part.nested_size = uint8_t(nested.size());
                part.which = which == NoField ? detail::NoValue : uint8_t(which);
                part.flags = uint8_t((spec.alternate ? 1 : 0) | (spec.zero ? 2 : 0)
                                   | (nested.data() ? 4 : 0));
                part.fill = spec.fill;
                part.align = spec.align;
                part.sign = spec.sign;
                part.type = spec.type;
                _parts[_count++] = part;
                size = 0;
            };
            bool ok = detail::walk(
                _text, sizeof...(A),
                [&](const char* from, size_t n) {
                    size_t starts = size_t(from - _text.data());
                    // Two runs meet where a doubled brace stands for one:
                    // they are one literal only when they touch
                    if (size && at + size == starts) {
                        size += n;
                    } else {
                        if (size) {
                            emit(NoField, format_spec{}, std::string_view{});
                        }
                        at = starts;
                        size = n;
                    }
                },
                [&](size_t which, const format_spec& spec, std::string_view nested) {
                    if (!detail::takes_spec<std::decay_t<A>...>(which, spec, nested)) {
                        return false;
                    }
                    emit(which, spec, nested);
                    return true;
                },
                [](size_t which) {
                    return detail::takes_nested_spec<std::decay_t<A>...>(which);
                });
            if (!ok) {
                // the message of the compiler points here; the pattern is
                // in the call above it
                throw "sgcl::format: the pattern does not fit the values";
            }
            if (size) {
                emit(NoField, format_spec{}, std::string_view{});
            }
            if (!room) {
                _count = 0;
            }
        }

        constexpr std::string_view view() const noexcept {
            return _text;
        }

        constexpr const detail::format_part* parts() const noexcept {
            return _parts;
        }

        constexpr size_t count() const noexcept {
            return _count;
        }

    private:
        std::string_view _text;
        detail::format_part _parts[detail::MaxParts] {};
        uint8_t _count = 0;
    };

    // The other kind of pattern: one the compiler never saw. A catalogue
    // of translations is read from a file when the program starts, and
    // the text of a message is then chosen by the language of whoever is
    // reading it — "{} left" in one file and "pozostało: {}" in another.
    // No consteval can help there, so the asking moves to where the
    // pattern arrives: txt::format(txt::runtime(entry), n).
    //
    // It keeps the string rather than pointing into it. A pattern that is
    // looked up in a table and handed straight to format is a temporary,
    // and a string of this library is shared and immutable, so keeping it
    // costs a pointer and no characters.
    class runtime_pattern {
    public:
        explicit runtime_pattern(const string& text) noexcept
        : _text(text) {
        }

        constexpr std::string_view view() const noexcept {
            return _text.view();
        }

        const string& text() const noexcept {
            return _text;
        }

    private:
        string _text;
    };

    inline runtime_pattern runtime(const string& text) noexcept {
        return runtime_pattern(text);
    }

    namespace detail {
        // The steps when the pattern had room for them, the characters
        // when it did not
        template<class P, class... A>
        void write_pattern(format_sink& out, const P& pattern, const A&... args) {
            if (pattern.count()) {
                run_parts(out, pattern.view().data(), pattern.parts(), pattern.count(), args...);
            } else {
                run(out, pattern.view(), args...);
            }
        }
    }

    // The text of a pattern and its values.
    //
    // The room for what a short text takes is on the stack, so the common
    // one allocates once — for the string it hands back — and a longer
    // one is written twice rather than grown: the first pass says how
    // much it takes, which costs less than the copying a growing buffer
    // does.
    template<class... A>
    string format(const format_pattern<std::type_identity_t<A>...>& pattern, const A&... args) {
        char room[256];
        format_sink out(room, sizeof room);
        detail::write_pattern(out, pattern, args...);
        if (out.size() <= sizeof room) {
            return string(room, out.size());
        }
        std::string wider(out.size(), '\0');
        format_sink again(wider.data(), wider.size());
        detail::write_pattern(again, pattern, args...);
        return string(wider.data(), again.size());
    }

    // The same into a buffer of the caller's, for a text that lives no
    // longer than the call that reads it: what fits is written and the
    // whole size comes back, so a caller may ask with an empty buffer
    // and then size one.
    template<class... A>
    size_t format_to(const slice<char>& buffer, const format_pattern<std::type_identity_t<A>...>& pattern,
                     const A&... args) {
        format_sink out(buffer.data(), buffer.size());
        detail::write_pattern(out, pattern, args...);
        return out.size();
    }

    //--------------------------------------------------------------------
    // The same two over a pattern read when the program runs.
    //
    // What comes back is an optional and not a string, because a pattern
    // that does not fit its values is now a thing that can happen: a
    // translator wrote {0} {1} where the program passes one value, or
    // {:d} over a name. It is not an error code and not an exception, and
    // both were weighed.
    //
    // Not an exception, because the module throws nothing and because a
    // message that cannot be built is not exceptional in a program whose
    // patterns come from strangers — it is Tuesday. Not a reason, either,
    // because every reason has the same remedy: fall back on the pattern
    // the program was written with, which is a literal and was checked by
    // the compiler. So
    //     txt::format(txt::runtime(entry), n).value_or(txt::format("{} left", n))
    // is the whole of what a caller does, and a reason would only have
    // been logged. Whoever wants to know before the message is wanted has
    // txt::fits below, which asks the same question of a catalogue as it
    // is loaded.
    //
    // One thing is still not asked here and cannot be: {:c} of a number
    // no character holds is a fault of the value and not of the pattern,
    // and it throws on this road exactly as it does on the other.
    template<class... A>
    optional<string> format(const runtime_pattern& pattern, const A&... args) {
        char room[256];
        format_sink out(room, sizeof room);
        if (!detail::run_checked(out, pattern.view(), args...)) {
            return nullopt;
        }
        if (out.size() <= sizeof room) {
            return string(room, out.size());
        }
        std::string wider(out.size(), '\0');
        format_sink again(wider.data(), wider.size());
        detail::run_checked(again, pattern.view(), args...);
        return string(wider.data(), again.size());
    }

    template<class... A>
    optional<size_t> format_to(const slice<char>& buffer, const runtime_pattern& pattern,
                               const A&... args) {
        format_sink out(buffer.data(), buffer.size());
        if (!detail::run_checked(out, pattern.view(), args...)) {
            return nullopt;
        }
        return out.size();
    }

    // Whether a pattern fits the values it will be given, with nothing
    // written: the question the compiler asks of a literal, asked of a
    // catalogue where it is loaded rather than at every message. The
    // types are named and the values are not, since there are none yet —
    //     txt::fits<int>(entry)
    // is what the program will pass when it comes to it.
    template<class... A>
    bool fits(const runtime_pattern& pattern) noexcept {
        return detail::fits<std::decay_t<A>...>(pattern.view());
    }
}
