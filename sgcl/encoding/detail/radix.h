//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "codec.h"
#include "../error.h"
#include "../../core/aliases.h"

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <string>

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;
    // The codecs of RFC 4648 are one algorithm with three widths: every
    // character carries Bits bits (6 in base64, 5 in base32, 4 in hex), a
    // group of characters carries a whole number of bytes (4 characters
    // for 3 bytes, 8 for 5, 2 for 1), and a last group that is short is
    // either padded to its full length or left short. This is that
    // algorithm once, with the alphabet as a value: the 2^Bits characters,
    // the table from a byte back to its value (Invalid for a byte outside
    // the alphabet), the padding character or none, and whether line
    // endings and bits past the data are let through (lenient).
    //
    // Nothing here holds a tracked pointer: the alphabet is plain data,
    // and a codec may be a constant, a global or a member of anything.
    template<unsigned Bits>
    class Radix {
    public:
        static constexpr size_t GroupBits = std::lcm(size_t(Bits), size_t(8));
        static constexpr size_t GroupChars = GroupBits / Bits;
        static constexpr size_t GroupBytes = GroupBits / 8;
        static constexpr size_t MaxGroupChars = GroupChars;
        static constexpr size_t MaxGroupBytes = GroupBytes;
        static constexpr size_t Symbols = size_t(1) << Bits;
        static constexpr uint8_t Invalid = 0xFF;

        static_assert(GroupBits <= 64, "a group fits a word");

        // The alphabet (exactly Symbols characters, all different, none
        // of them a line ending or the padding), the padding character or
        // -1; with both_cases a letter of either case decodes, as hex
        // wants. The length is the array's, never a search for its end: an
        // array of Symbols + 1 characters whose last is not the terminator
        // (or with one earlier) is an alphabet of the wrong length. An
        // alphabet that breaks the rules is a mistake in the program, not
        // in its input: invalid_argument, which in a constant is an
        // error at compile time.
        template<size_t N>
        constexpr Radix(const char* name, const char (&alphabet)[N], int padding, bool both_cases = false)
        : _padding(int16_t(padding)), _name(name) {
            static_assert(N == Symbols + 1, "an alphabet of 2^Bits characters and its terminator");
            for (auto& v : _values) {
                v = Invalid;
            }
            if (alphabet[Symbols] != '\0') {
                throw invalid_argument("sgcl: an alphabet of the wrong length");
            }
            for (size_t i = 0; i < Symbols; ++i) {
                if (alphabet[i] == '\0') {
                    throw invalid_argument("sgcl: an alphabet of the wrong length");
                }
            }
            if (padding == '\r' || padding == '\n' || padding > 0xFF || padding < -1) {
                throw invalid_argument("sgcl: a padding character that cannot be one");
            }
            for (size_t i = 0; i < Symbols; ++i) {
                auto c = uint8_t(alphabet[i]);
                if (c == '\r' || c == '\n' || int(c) == padding || _values[c] != Invalid) {
                    throw invalid_argument("sgcl: an alphabet with a repeated, padding or line-ending character");
                }
                _symbols[i] = char(c);
                _values[c] = uint8_t(i);
                if (both_cases) {
                    if (c >= 'a' && c <= 'z') {
                        _values[c - 32] = uint8_t(i);
                    } else if (c >= 'A' && c <= 'Z') {
                        _values[c + 32] = uint8_t(i);
                    }
                }
            }
        }

        constexpr Radix without_padding() const noexcept {
            Radix r = *this;
            r._padding = -1;
            return r;
        }

        constexpr Radix lenient() const noexcept {
            Radix r = *this;
            r._lenient = true;
            return r;
        }

        constexpr bool padded() const noexcept {
            return _padding >= 0;
        }

        constexpr bool is_lenient() const noexcept {
            return _lenient;
        }

        constexpr const char* name() const noexcept {
            return _name;
        }

        // The characters of n bytes. A size that no size_t can hold is
        // SIZE_MAX, the size no buffer has: the arithmetic never wraps to
        // a small number, which is how an encoder writes past its buffer
        constexpr size_t encoded_size(size_t n) const noexcept {
            size_t groups = n / GroupBytes;
            size_t rest = n % GroupBytes;
            size_t tail = rest == 0 ? 0 : padded() ? GroupChars : (rest * 8 + Bits - 1) / Bits;
            if (groups > (SIZE_MAX - tail) / GroupChars) {
                return SIZE_MAX;
            }
            return groups * GroupChars + tail;
        }

        // The most bytes n characters decode to. With padding, every group
        // started counts whole (a short group is an error, but the bytes
        // before the error are written), so a decoder given this much room
        // never runs out of it before it has found the error; without, the
        // bits of every character
        constexpr size_t max_decoded_size(size_t n) const noexcept {
            if (padded()) {
                return (n / GroupChars + (n % GroupChars != 0)) * GroupBytes;
            }
            return n / GroupChars * GroupBytes + n % GroupChars * Bits / 8;
        }

        size_t encode_bound(size_t n) const noexcept {
            return encoded_size(n);
        }

        size_t decode_bound(const char*, size_t n) const noexcept {
            return max_decoded_size(n);
        }

        // Whole groups of bytes, while there are any and the output has
        // room for one: one read of the group into a word and one table
        // lookup a character, no branch but the loop's
        void encode_groups(const uint8_t*& in, const uint8_t* in_end, char*& out, char* out_end) const noexcept {
            auto p = in;
            auto o = out;
            while (size_t(in_end - p) >= GroupBytes && size_t(out_end - o) >= GroupChars) {
                uint64_t v = 0;
                for (size_t i = 0; i < GroupBytes; ++i) {
                    v = v << 8 | p[i];
                }
                for (size_t i = 0; i < GroupChars; ++i) {
                    o[i] = _symbols[(v >> (Bits * (GroupChars - 1 - i))) & (Symbols - 1)];
                }
                p += GroupBytes;
                o += GroupChars;
            }
            in = p;
            out = o;
        }

        // The last n bytes (fewer than a group): the characters that carry
        // them, the bits past the data zero, and the padding
        void encode_final(const uint8_t* in, size_t n, char*& out) const noexcept {
            if (n == 0) {
                return;
            }
            uint64_t v = 0;
            for (size_t i = 0; i < GroupBytes; ++i) {
                v = v << 8 | (i < n ? in[i] : 0);
            }
            size_t chars = (n * 8 + Bits - 1) / Bits;
            for (size_t i = 0; i < chars; ++i) {
                out[i] = _symbols[(v >> (Bits * (GroupChars - 1 - i))) & (Symbols - 1)];
            }
            if (padded()) {
                for (size_t i = chars; i < GroupChars; ++i) {
                    out[i] = char(_padding);
                }
                out += GroupChars;
            } else {
                out += chars;
            }
        }

        // The state of one decoding, which may be given its input in
        // pieces: the bits of the group so far, the characters of it, the
        // padding seen, whether the padding is complete, the characters
        // taken so far (the offset of the next one) and the offset of the
        // last data character (the one a bad tail is blamed on)
        struct decoding {
            uint64_t acc = 0;
            uint64_t offset = 0;
            uint64_t last = 0;
            uint8_t count = 0;
            uint8_t pads = 0;
            bool done = false;
        };

        // Decodes what [in, in_end) holds into [out, out_end), advancing
        // both. The good path is a loop over whole groups with no branch
        // but the one that leaves it when a group holds a byte outside the
        // alphabet (a line ending, the padding, an error); the rest goes a
        // character at a time and returns to the loop at the next group.
        // The output needs room for a group's bytes before the character
        // that completes it is taken; without, the step ends with room and
        // the character is still there.
        //
        // The offset of an error is where the input stops being the start
        // of a valid encoding: the character outside the alphabet, the
        // padding where a group cannot end, the first character that is
        // not padding inside the padding, the first one after the padding,
        // and the end of the input when it ends inside a group. A strict
        // decoding refuses bits past the data in the last character
        // (RFC 4648 section 3.5), at that character.
        FeedStatus feed(decoding& d, const char*& in, const char* in_end, uint8_t*& out, uint8_t* out_end, optional<error>& e) const {
            auto p = in;
            auto o = out;
            auto status = FeedStatus::more;
            for (;;) {
                if (d.count == 0 && d.pads == 0 && !d.done) {
                    auto from = p;
                    while (size_t(in_end - p) >= GroupChars && size_t(out_end - o) >= GroupBytes) {
                        uint64_t v = 0;
                        unsigned bad = 0;
                        for (size_t i = 0; i < GroupChars; ++i) {
                            unsigned x = _values[uint8_t(p[i])];
                            bad |= x;
                            v = v << Bits | x;
                        }
                        if (bad & 0x80) {
                            break;
                        }
                        for (size_t i = 0; i < GroupBytes; ++i) {
                            o[i] = uint8_t(v >> (8 * (GroupBytes - 1 - i)));
                        }
                        p += GroupChars;
                        o += GroupBytes;
                    }
                    if (p != from) {
                        d.last = d.offset + uint64_t(p - in) - 1;
                    }
                }
                if (p == in_end) {
                    break;
                }
                auto c = uint8_t(*p);
                uint64_t at = d.offset + uint64_t(p - in);
                bool line_end = c == '\r' || c == '\n';
                if (d.done) {
                    if (_lenient && line_end) {
                        ++p;
                        continue;
                    }
                    e = error(errc::syntax, at, string("data after the padding: " + quoted_byte(c)));
                    status = FeedStatus::failed;
                    break;
                }
                if (d.pads) {
                    if (int(c) == _padding) {
                        ++p;
                        if (size_t(d.count) + ++d.pads == GroupChars) {
                            d.done = true;
                        }
                        continue;
                    }
                    if (_lenient && line_end) {
                        ++p;
                        continue;
                    }
                    e = error(errc::syntax, at, string("expected padding, found " + quoted_byte(c)));
                    status = FeedStatus::failed;
                    break;
                }
                uint8_t v = _values[c];
                if (v != Invalid) {
                    if (size_t(d.count) + 1 == GroupChars && size_t(out_end - o) < GroupBytes) {
                        status = FeedStatus::room;
                        break;
                    }
                    d.acc = d.acc << Bits | v;
                    d.last = at;
                    ++p;
                    if (++d.count == GroupChars) {
                        for (size_t i = 0; i < GroupBytes; ++i) {
                            o[i] = uint8_t(d.acc >> (8 * (GroupBytes - 1 - i)));
                        }
                        o += GroupBytes;
                        d.acc = 0;
                        d.count = 0;
                    }
                    continue;
                }
                if (padded() && int(c) == _padding) {
                    if (!_valid_tail(d.count)) {
                        e = error(errc::syntax, at, string("padding where the data cannot end"));
                        status = FeedStatus::failed;
                        break;
                    }
                    if (!_lenient && _tail_bits(d) != 0) {
                        e = error(errc::syntax, d.last, string("bits past the data in the last character"));
                        status = FeedStatus::failed;
                        break;
                    }
                    size_t k = d.count * Bits / 8;
                    if (size_t(out_end - o) < k) {
                        status = FeedStatus::room;
                        break;
                    }
                    _emit_tail(d, o);
                    ++p;
                    d.pads = 1;
                    if (size_t(d.count) + 1 == GroupChars) {
                        d.done = true;
                    }
                    continue;
                }
                if (_lenient && line_end) {
                    ++p;
                    continue;
                }
                e = error(errc::invalid_character, at, string("invalid character " + quoted_byte(c)));
                status = FeedStatus::failed;
                break;
            }
            d.offset += uint64_t(p - in);
            in = p;
            out = o;
            return status;
        }

        // The end of the input: the short group without padding written
        // out, or the error of an input that ends where it cannot
        FeedStatus finish(decoding& d, uint8_t*& out, uint8_t* out_end, optional<error>& e) const {
            if (d.done || (d.count == 0 && d.pads == 0)) {
                return FeedStatus::more;
            }
            if (d.pads) {
                e = error(errc::unexpected_end, d.offset, string("the padding is cut short"));
                return FeedStatus::failed;
            }
            if (padded()) {
                e = error(errc::unexpected_end, d.offset, string("the last group is not padded"));
                return FeedStatus::failed;
            }
            if (!_valid_tail(d.count)) {
                e = error(errc::unexpected_end, d.offset, string("the input ends inside a byte"));
                return FeedStatus::failed;
            }
            if (!_lenient && _tail_bits(d) != 0) {
                e = error(errc::syntax, d.last, string("bits past the data in the last character"));
                return FeedStatus::failed;
            }
            if (size_t(out_end - out) < d.count * Bits / 8) {
                return FeedStatus::room;
            }
            _emit_tail(d, out);
            d.done = true;
            return FeedStatus::more;
        }

    private:
        // Whether a group may end after c characters: the last of them
        // must begin a byte of its own (base64: 2 or 3; base32: 2, 4, 5
        // or 7; hex: none)
        static constexpr bool _valid_tail(size_t c) noexcept {
            return c > 0 && c < GroupChars && c * Bits / 8 > (c - 1) * Bits / 8;
        }

        static uint64_t _tail_bits(const decoding& d) noexcept {
            size_t bits = d.count * Bits;
            return d.acc & ((uint64_t(1) << (bits - bits / 8 * 8)) - 1);
        }

        static void _emit_tail(const decoding& d, uint8_t*& o) noexcept {
            size_t bits = d.count * Bits;
            size_t k = bits / 8;
            for (size_t i = 0; i < k; ++i) {
                o[i] = uint8_t(d.acc >> (bits - 8 * (i + 1)));
            }
            o += k;
        }

        char _symbols[Symbols] {};
        uint8_t _values[256] {};
        int16_t _padding = -1;
        bool _lenient = false;
        const char* _name = "";
    };
}
