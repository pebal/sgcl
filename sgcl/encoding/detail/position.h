//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace sgcl::detail {}

namespace sgcl::encoding::detail {
    using namespace sgcl::detail;
    // A line and a column, both from 1
    struct TextPosition {
        uint32_t line = 1;
        uint32_t column = 1;
    };

    // The line and the column of a byte of the text, counted after the
    // fact: a parser keeps the offset, which it has anyway, and this is
    // asked only when something failed, so the good path never counts a
    // line ending. A line ends at '\n' (a '\r' before it is the last
    // character of its line); the column counts code points, the bytes
    // that do not continue a UTF-8 sequence, since a reader counts the
    // characters on the screen. An offset past the end is the end.
    inline TextPosition position_of(std::string_view text, uint64_t offset) noexcept {
        size_t end = offset < text.size() ? size_t(offset) : text.size();
        auto first = text.data();
        auto last = first + end;
        // A count of one byte, which the compiler turns into a vector
        // loop by itself
        auto lines = std::count(first, last, '\n');
        auto start = first;
        for (auto p = last; p != first; --p) {
            if (p[-1] == '\n') {
                start = p;
                break;
            }
        }
        uint32_t column = 1;
        for (auto p = start; p != last; ++p) {
            column += (uint8_t(*p) & 0xC0) != 0x80;
        }
        return TextPosition{uint32_t(lines + 1), column};
    }
}
