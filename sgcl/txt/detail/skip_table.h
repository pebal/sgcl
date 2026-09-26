//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

// Boyer–Moore–Horspool over bytes, with no text type of its own: a table
// of skips built from a pattern once, and the walk that uses it.
//
// It lives here rather than inside txt::searcher because two headers want
// it. searcher wraps it in the library's types and is what a program
// calls; regex.h's machine uses it over the bytes it is already walking,
// to jump to the next place a match could possibly begin. One
// implementation, so that a fault in it is one fault — the hang of note
// 170 was in this table, and there is no second copy of it to find.
//
// The bytes are safe to search. UTF-8 synchronises itself: the first byte
// of a character cannot appear inside another one, so a match of valid
// UTF-8 inside valid UTF-8 always begins on a character, and no test for
// that is needed.
namespace sgcl::txt::detail {
    struct skip_table {
        uint16_t skip[256] = {};

        // Capped, not truncated: a pattern of exactly 65536 bytes
        // narrowed to a skip of zero and the search below never moved.
        // A skip shorter than the true one only costs steps — it can
        // never pass over a match — so the cap is safe and the table
        // stays at half a kilobyte.
        constexpr void prepare(std::string_view p) noexcept {
            uint16_t whole = uint16_t(std::min(p.size(), size_t(0xFFFF)));
            for (auto& s : skip) {
                s = whole;
            }
            for (size_t i = 0; i + 1 < p.size(); ++i) {
                size_t back = p.size() - 1 - i;
                skip[uint8_t(p[i])] = uint16_t(std::min(back, size_t(0xFFFF)));
            }
        }

        // The byte position of the first occurrence of p at or after
        // `from`, or npos. An empty pattern is found at once, as it is
        // in a std::string.
        constexpr size_t find(std::string_view text, std::string_view p, size_t from) const noexcept {
            if (p.empty()) {
                return from <= text.size() ? from : std::string_view::npos;
            }
            if (p.size() > text.size()) {
                return std::string_view::npos;
            }
            // One byte is not a job for a skip table: the loop below
            // would compare and then skip by one, where the library's
            // find is a memchr. It showed up as a measurement rather
            // than as an idea — the date pattern, whose required run is
            // a single '-', cost 0.43 ns a byte on the byte scan this
            // replaced, 0.62 when the table took the work over, which
            // is worse, and 0.039 with this line.
            if (p.size() == 1) {
                return text.find(p[0], from);
            }
            size_t at = from;
            while (at + p.size() <= text.size()) {
                size_t last = at + p.size() - 1;
                if (text[last] == p.back() && text.compare(at, p.size(), p) == 0) {
                    return at;
                }
                at += skip[uint8_t(text[last])];
            }
            return std::string_view::npos;
        }
    };
}
