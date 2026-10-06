//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The immutable containers as ranges (tests/ranges.h)
#include "tests/types.h"
#include "tests/ranges.h"

#include <vector>

SGCL_CHECK_RANGE(std::ranges::random_access_range, immutable::vector<int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, immutable::list<int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, immutable::map<int, int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, immutable::set<int>);

TEST(ImmutableRanges_Tests, ViewsOverAVersion) {
    immutable::vector<int> v = immutable::vector<int>().push_back(1).push_back(2).push_back(3).push_back(4);
    std::vector<int> got;
    for (int x : v | std::views::reverse | std::views::filter([](int x) { return x != 3; })) {
        got.push_back(x);
    }
    EXPECT_EQ(got, (std::vector<int>{4, 2, 1}));
}
