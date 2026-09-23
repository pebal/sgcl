//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/slice.h"
#include "../core/string.h"
#include "../core/utf8.h"
#include "properties.h"

#include <charconv>
#include <cmath>
#include <limits>
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
// error of the compiler and not a surprise at the customer's.
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

    private:
        char* _at;
        char* _end;
        size_t _size = 0;
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
                    throw std::out_of_range("sgcl::txt::format: {:c} of a value no character holds");
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
                || type == 'o' || type == 'x' || type == 'X';
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static void write(format_sink& out, char value, const format_spec& spec) {
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
                || type == 'b' || type == 'B' || type == 'o';
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static void write(format_sink& out, char32_t value, const format_spec& spec) {
            if (!spec.type || spec.type == 'c') {
                char buf[utf8::max_width];
                detail::put_padded(out, {buf, utf8::encode(value, buf)}, spec);
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
            return !type || type == 's';
        }

        static constexpr bool takes_precision() noexcept {
            return true;
        }

        static void write(format_sink& out, std::string_view value, const format_spec& spec) {
            detail::write_text(out, value, spec);
        }
    };

    // The library's own text, which holds what it points at
    template<>
    struct formatter<string> {
        static constexpr bool takes(char type) noexcept {
            return !type || type == 's';
        }

        static constexpr bool takes_precision() noexcept {
            return true;
        }

        static void write(format_sink& out, const string& value, const format_spec& spec) {
            detail::write_text(out, value.view(), spec);
        }
    };

    template<>
    struct formatter<slice<const char>> {
        static constexpr bool takes(char type) noexcept {
            return !type || type == 's';
        }

        static constexpr bool takes_precision() noexcept {
            return true;
        }

        static void write(format_sink& out, slice<const char> value, const format_spec& spec) {
            detail::write_text(out, value.view(), spec);
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
}

namespace sgcl::txt {
    namespace detail {
        // A value written by the formatter its type names, or — for a
        // type of the caller's — by a format_value found beside it:
        //     void format_value(format_sink&, const T&, const format_spec&);
        // declared in the namespace of T and found there, so that a type
        // teaches this how to write it without opening anything here.
        template<class T>
        concept has_format_value = requires(format_sink& out, const T& v, const format_spec& s) {
            format_value(out, v, s);
        };

        template<class T>
        void write_one(format_sink& out, const T& value, const format_spec& spec) {
            using V = std::decay_t<T>;
            if constexpr (requires { formatter<V>::write(out, value, spec); }) {
                formatter<V>::write(out, value, spec);
            } else {
                static_assert(has_format_value<V>,
                              "this type says nothing about how it is written: give it a "
                              "format_value(format_sink&, const T&, const format_spec&) beside it");
                format_value(out, value, spec);
            }
        }

        // Whether one type accepts that specification. A type of the
        // caller's, which says nothing but its format_value, accepts
        // whatever it is given: the question has to be asked with
        // if constexpr and not with a ?:, whose arms are both read
        // whichever one is taken
        template<class A>
        constexpr bool takes_one(const format_spec& spec) noexcept {
            if constexpr (requires { formatter<A>::takes(spec.type); }) {
                return formatter<A>::takes(spec.type)
                    && (spec.precision < 0 || formatter<A>::takes_precision());
            } else {
                return true;
            }
        }

        // Whether the n-th of these types accepts that specification,
        // asked where the program is built
        template<class... A>
        constexpr bool takes_spec(size_t n, const format_spec& spec) noexcept {
            bool ok = false;
            size_t i = 0;
            (void)((i++ == n ? (ok = takes_one<A>(spec), true) : false) || ...);
            return ok;
        }

        template<class... A>
        void write_nth(format_sink& out, size_t n, const format_spec& spec, const A&... args) {
            size_t i = 0;
            (void)((i++ == n ? (write_one(out, args, spec), true) : false) || ...);
        }

        constexpr bool is_digit(char c) noexcept {
            return c >= '0' && c <= '9';
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

        // [[fill]align][sign][#][0][width][.precision][type] — read from
        // `at`, which is left on the closing brace when it is well formed
        constexpr bool read_spec(std::string_view fmt, size_t& at, format_spec& spec) noexcept {
            if (at < fmt.size() && fmt[at] == ':') {
                ++at;
                if (at + 1 < fmt.size()
                    && (fmt[at + 1] == '<' || fmt[at + 1] == '>' || fmt[at + 1] == '^')) {
                    spec.fill = fmt[at];
                    spec.align = fmt[at + 1];
                    at += 2;
                } else if (at < fmt.size()
                           && (fmt[at] == '<' || fmt[at] == '>' || fmt[at] == '^')) {
                    spec.align = fmt[at++];
                }
                if (at < fmt.size() && (fmt[at] == '+' || fmt[at] == '-' || fmt[at] == ' ')) {
                    spec.sign = fmt[at++];
                }
                if (at < fmt.size() && fmt[at] == '#') {
                    spec.alternate = true;
                    ++at;
                }
                if (at < fmt.size() && fmt[at] == '0') {
                    spec.zero = true;
                    ++at;
                }
                while (at < fmt.size() && is_digit(fmt[at])) {
                    // A width that does not fit is a pattern to refuse,
                    // not one to wrap around: {:4294967297} padded to one
                    unsigned digit = unsigned(fmt[at] - '0');
                    if (spec.width > (0xFFFFFFFFu - digit) / 10) {
                        return false;
                    }
                    spec.width = spec.width * 10 + digit;
                    ++at;
                }
                if (at < fmt.size() && fmt[at] == '.') {
                    ++at;
                    if (at >= fmt.size() || !is_digit(fmt[at])) {
                        return false;               // a point with no number after it
                    }
                    spec.precision = 0;
                    while (at < fmt.size() && is_digit(fmt[at])) {
                        // Kept inside what a step of a pattern holds, and
                        // inside what the writers below can size a buffer
                        // for; past that the pattern is refused
                        spec.precision = spec.precision * 10 + (fmt[at] - '0');
                        if (spec.precision > 0x7FFF) {
                            return false;
                        }
                        ++at;
                    }
                }
                if (at < fmt.size() && fmt[at] != '}') {
                    spec.type = fmt[at++];
                }
            }
            return at < fmt.size() && fmt[at] == '}';
        }

        // The pattern walked once. What is done with each piece is the
        // caller's: writing it, or — where the compiler reads the
        // pattern — asking whether the value of that number is written
        // that way. Both walks are this one, so neither can drift.
        template<class Text, class Field>
        constexpr bool walk(std::string_view fmt, size_t count, Text&& text, Field&& field) {
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
                        which = which * 10 + size_t(fmt[i++] - '0');
                    }
                } else {
                    ++next;
                }
                format_spec spec;
                if (!read_spec(fmt, i, spec)) {
                    return false;                   // the specification does not end
                }
                ++i;                                // past the closing brace
                if (which >= count) {
                    return false;                   // no value of that number
                }
                if (!field(which, spec)) {
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
                        [](size_t which, const format_spec& spec) {
                            return takes_spec<A...>(which, spec);
                        });
        }

        template<class... A>
        void run(format_sink& out, std::string_view fmt, const A&... args) {
            walk(fmt, sizeof...(A),
                 [&out](const char* at, size_t n) { out.put(at, n); },
                 [&](size_t which, const format_spec& spec) {
                     write_nth(out, which, spec, args...);
                     return true;
                 });
        }

        // One step of a pattern already read: the run of literal text and
        // the field that follows it, with the specification already made
        // out. A pattern keeps these as an array settled where the
        // program is built, so that what happens when it runs is a walk
        // over as many steps as it has fields, and not over its
        // characters — reading `{:>8}` again on every call cost 6.7 ns of
        // the 19.8 the whole of it took.
        constexpr uint8_t NoValue = uint8_t(-1);

        // The same fields a format_spec has, in fourteen bytes rather
        // than thirty-two. It matters because the pattern is built afresh
        // at every call — it is a temporary the conversion makes at the
        // call site — and what that costs is the bytes it takes: at 280
        // of them the building cost 3.5 ns of a call, and over a literal
        // with nothing to substitute, where there was nothing else to do,
        // 8.9.
        struct format_part {
            uint16_t at = 0;              // where the literal run starts
            uint16_t size = 0;            // and how long it is
            uint16_t width = 0;
            int16_t precision = -1;
            uint8_t which = NoValue;      // the value written after it
            uint8_t flags = 0;            // 1 alternate, 2 zero
            char fill = ' ';
            char align = 0;
            char sign = 0;
            char type = 0;
        };

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
                    write_nth(out, part.which, spec_of(part), args...);
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
            auto emit = [&](size_t which, const format_spec& spec) {
                // Everything must fit the narrow fields of a part, and
                // there must be a part left to put it in
                if (_count == detail::MaxParts
                    || at > 0xFFFF || size > 0xFFFF
                    || spec.width > 0xFFFF || spec.precision > 0x7FFF
                    || (which != NoField && which >= detail::NoValue)) {
                    room = false;
                    return;
                }
                detail::format_part part;
                part.at = uint16_t(at);
                part.size = uint16_t(size);
                part.width = uint16_t(spec.width);
                part.precision = int16_t(spec.precision);
                part.which = which == NoField ? detail::NoValue : uint8_t(which);
                part.flags = uint8_t((spec.alternate ? 1 : 0) | (spec.zero ? 2 : 0));
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
                            emit(NoField, format_spec{});
                        }
                        at = starts;
                        size = n;
                    }
                },
                [&](size_t which, const format_spec& spec) {
                    if (!detail::takes_spec<std::decay_t<A>...>(which, spec)) {
                        return false;
                    }
                    emit(which, spec);
                    return true;
                });
            if (!ok) {
                // the message of the compiler points here; the pattern is
                // in the call above it
                throw "sgcl::format: the pattern does not fit the values";
            }
            if (size) {
                emit(NoField, format_spec{});
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
    size_t format_to(slice<char> buffer, const format_pattern<std::type_identity_t<A>...>& pattern,
                     const A&... args) {
        format_sink out(buffer.data(), buffer.size());
        detail::write_pattern(out, pattern, args...);
        return out.size();
    }
}
