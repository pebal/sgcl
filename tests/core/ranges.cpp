//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The ranges of core beside the containers (tests/ranges.h)
#include "tests/types.h"
#include "tests/ranges.h"

#include <vector>

SGCL_CHECK_RANGE(std::ranges::contiguous_range, string);
SGCL_CHECK_RANGE(std::ranges::contiguous_range, u16string);
SGCL_CHECK_RANGE(std::ranges::contiguous_range, slice<int>);
SGCL_CHECK_RANGE(std::ranges::contiguous_range, slice<const char>);
SGCL_CHECK_RANGE(std::ranges::forward_range, decltype(string().runes()));
SGCL_CHECK_RANGE(std::ranges::forward_range, decltype(string().split(",")));
SGCL_CHECK_RANGE(std::ranges::forward_range, decltype(string().fields()));
SGCL_CHECK_RANGE(std::ranges::random_access_range, decltype(sgcl::range(10)));
SGCL_CHECK_RANGE(std::ranges::input_range, generator<int>);

TEST(CoreRanges_Tests, ViewsOverStringsAndRanges) {
    string s = "a,bb,ccc,dddd";
    std::vector<size_t> sizes;
    for (size_t n : s.split(",") | std::views::transform([](auto piece) { return piece.size(); })
                                 | std::views::filter([](size_t n) { return n > 1; })) {
        sizes.push_back(n);
    }
    EXPECT_EQ(sizes, (std::vector<size_t>{2, 3, 4}));

    std::vector<int> back;
    for (int i : sgcl::range(5) | std::views::reverse | std::views::take(3)) {
        back.push_back(i);
    }
    EXPECT_EQ(back, (std::vector<int>{4, 3, 2}));
}
