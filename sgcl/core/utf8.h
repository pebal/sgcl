//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace sgcl {
    // utf8: the encoding of Unicode in bytes, as the functions of Go's
    // unicode/utf8: a code point decoded at a position, encoded into up to
    // four bytes, counted and validated over a string. The text of the
    // library is UTF-8 in char — a string is bytes, its size() counts
    // them — and these are the primitives its runes(), rune_count() and
    // the char32_t overloads of find and the rest stand on. Every
    // function is constexpr and takes the bytes as a std::string_view
    // (a string, a slice<const char> and a literal convert), keeping
    // nothing alive: it looks and returns.
    //
    // A byte that does not begin a valid sequence, a truncated sequence,
    // an overlong encoding, a surrogate or a value past U+10FFFF decodes
    // as one byte and the replacement character U+FFFD, as browsers and
    // Go do; nothing throws and nothing is rejected.
    struct utf8 {
        static constexpr char32_t replacement = U'�';   // what an invalid byte decodes as
        static constexpr size_t max_width = 4;               // the bytes of the longest encoding
        static constexpr char32_t max_code_point = U'\U0010FFFF';

        // Whether c is a Unicode scalar value: not a surrogate, not past the last code point
        static constexpr bool valid(char32_t c) noexcept {
            return c <= max_code_point && (c < 0xD800 || c > 0xDFFF);
        }

        // Whether every sequence of s is valid
        static constexpr bool valid(std::string_view s) noexcept {
            for (size_t i = 0; i < s.size();) {
                i += ascii_run(s, i);              // a byte under 128 is a sequence of its own
                if (i >= s.size()) {
                    break;
                }
                auto [c, n] = decode(s, i);
                if (c == replacement && n == 1 && !_is_encoded_replacement(s, i)) {
                    return false;
                }
                i += n;
            }
            return true;
        }

        // The bytes the encoding of c takes, 1 to 4; 0 when c is not a scalar value
        static constexpr size_t width(char32_t c) noexcept {
            return !valid(c) ? 0 : c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
        }

        // Whether b begins a sequence: an ASCII byte or a leading byte,
        // not a continuation byte (10xxxxxx)
        static constexpr bool starts_rune(char b) noexcept {
            return (static_cast<unsigned char>(b) & 0xC0) != 0x80;
        }

        // The code point at s[i] and the bytes it takes; {replacement, 1}
        // for a byte that is not the start of a valid sequence
        static constexpr pair<char32_t, size_t> decode(std::string_view s, size_t i = 0) noexcept {
            if (i >= s.size()) {
                return {replacement, 0};
            }
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (c < 0x80) {
                return {c, 1};
            }
            size_t n = (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
            if (n == 0 || i + n > s.size()) {
                return {replacement, 1};
            }
            char32_t cp = c & (0xFF >> (n + 1));
            for (size_t k = 1; k < n; ++k) {
                unsigned char b = static_cast<unsigned char>(s[i + k]);
                if ((b & 0xC0) != 0x80) {
                    return {replacement, 1};
                }
                cp = (cp << 6) | (b & 0x3F);
            }
            if (width(cp) != n) {   // overlong, a surrogate, or past U+10FFFF
                return {replacement, 1};
            }
            return {cp, n};
        }

        // The last code point of s[0, end) and the bytes it takes;
        // {replacement, 1} when the bytes before `end` are not a valid
        // sequence, {replacement, 0} for an empty range
        static constexpr pair<char32_t, size_t> decode_last(std::string_view s, size_t end = std::string_view::npos) noexcept {
            if (end > s.size()) {
                end = s.size();
            }
            if (end == 0) {
                return {replacement, 0};
            }
            size_t start = end - 1;
            for (size_t k = 0; k < max_width && start > 0 && !starts_rune(s[start]); ++k) {
                --start;
            }
            auto [c, n] = decode(s, start);
            if (start + n != end) {
                return {replacement, 1};
            }
            return {c, n};
        }

        // The encoding of c into out, which has room for max_width bytes;
        // the bytes written. A value that is not a scalar value is
        // encoded as the replacement character.
        static constexpr size_t encode(char32_t c, char* out) noexcept {
            if (!valid(c)) {
                c = replacement;
            }
            if (c < 0x80) {
                out[0] = char(c);
                return 1;
            }
            if (c < 0x800) {
                out[0] = char(0xC0 | (c >> 6));
                out[1] = char(0x80 | (c & 0x3F));
                return 2;
            }
            if (c < 0x10000) {
                out[0] = char(0xE0 | (c >> 12));
                out[1] = char(0x80 | ((c >> 6) & 0x3F));
                out[2] = char(0x80 | (c & 0x3F));
                return 3;
            }
            out[0] = char(0xF0 | (c >> 18));
            out[1] = char(0x80 | ((c >> 12) & 0x3F));
            out[2] = char(0x80 | ((c >> 6) & 0x3F));
            out[3] = char(0x80 | (c & 0x3F));
            return 4;
        }

        // The code points of s, each invalid byte one of them: what
        // rune_count() is on a string. A run of ASCII is a code point a
        // byte and is counted eight bytes at a time; what is left is
        // decoded one code point at a time, an invalid byte being one of
        // them, which is why the continuation bytes cannot simply be
        // counted out instead.
        static constexpr size_t count(std::string_view s) noexcept {
            size_t n = 0;
            for (size_t i = 0; i < s.size();) {
                size_t run = ascii_run(s, i);
                n += run;
                i += run;
                if (i < s.size()) {
                    i += decode(s, i).second;
                    ++n;
                }
            }
            return n;
        }

        // How many bytes from `at` are plain ASCII, read eight at a
        // time. Most text is a long run of them, and for such a run the
        // questions the library asks about a character mostly have one
        // answer — two ASCII characters are always separate graphemes, a
        // run of Latin letters is one word, the case of one is a single
        // bit — so the callers walk the run in one step instead of one
        // step a character. The test for a byte over 127 is one mask
        // over a word of eight, which is no instruction a compiler would
        // not write by itself.
        static constexpr size_t ascii_run(std::string_view s, size_t at = 0) noexcept {
            size_t i = at;
            if (!std::is_constant_evaluated()) {
                constexpr uint64_t high = 0x8080808080808080ull;
                while (i + 8 <= s.size()) {
                    uint64_t word;
                    std::memcpy(&word, s.data() + i, 8);
                    if (word & high) {
                        break;
                    }
                    i += 8;
                }
            }
            while (i < s.size() && static_cast<unsigned char>(s[i]) < 0x80) {
                ++i;
            }
            return i - at;
        }

        static constexpr bool all_ascii(std::string_view s) noexcept {
            return ascii_run(s) == s.size();
        }

        // The encoding of c as a small buffer with its length: what the
        // char32_t overloads of find and the rest search for
        struct encoded {
            char bytes[max_width] = {};
            size_t size = 0;

            constexpr encoded(char32_t c) noexcept
            : size(encode(c, bytes)) {
            }

            constexpr std::string_view view() const noexcept {
                return std::string_view(bytes, size);
            }

            constexpr operator std::string_view() const noexcept {
                return view();
            }
        };

    private:
        // Whether the bytes at i are U+FFFD itself, which decodes as the
        // replacement while being valid
        static constexpr bool _is_encoded_replacement(std::string_view s, size_t i) noexcept {
            return i + 3 <= s.size() && s[i] == char(0xEF) && s[i + 1] == char(0xBF) && s[i + 2] == char(0xBD);
        }
    };
}
