//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The ranges of txt (tests/ranges.h)
#include "tests/types.h"
#include "tests/ranges.h"

#include <vector>

SGCL_CHECK_RANGE(std::ranges::forward_range, txt::graphemes);
SGCL_CHECK_RANGE(std::ranges::forward_range, txt::words);
SGCL_CHECK_RANGE(std::ranges::forward_range, txt::word_breaks);
SGCL_CHECK_RANGE(std::ranges::forward_range, txt::sentences);
SGCL_CHECK_RANGE(std::ranges::forward_range, txt::line_breaks);
SGCL_CHECK_RANGE(std::ranges::forward_range, txt::regex_matches);
SGCL_CHECK_RANGE(std::ranges::forward_range, txt::fold_matches);
SGCL_CHECK_RANGE(std::ranges::forward_range, txt::normalized_matches);
SGCL_CHECK_RANGE(std::ranges::forward_range, txt::collated_matches);
SGCL_CHECK_RANGE(std::ranges::contiguous_range, txt::bidi_runs);

TEST(TxtRanges_Tests, ViewsOverGraphemes) {
    string s = "ab\u0301cd";      // b with a combining acute: one grapheme
    std::vector<size_t> sizes;
    for (size_t n : txt::graphemes(s) | std::views::transform([](auto g) { return g.size(); })
                                      | std::views::filter([](size_t n) { return n > 1; })) {
        sizes.push_back(n);
    }
    EXPECT_EQ(sizes, (std::vector<size_t>{3}));
}
