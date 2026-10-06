//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The concurrent containers that are ranges (tests/ranges.h); the queues,
// stacks, cache and intern are not: they are taken from, not walked
#include "tests/types.h"
#include "tests/ranges.h"

SGCL_CHECK_RANGE(std::ranges::forward_range, concurrent::map<int, int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, concurrent::set<int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, concurrent::sorted_map<int, int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, concurrent::sorted_set<int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, concurrent::weak_map<tracked_ptr<int>, int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, concurrent::weak_set<tracked_ptr<int>>);

TEST(ConcurrentRanges_Tests, ViewsOverASortedMap) {
    concurrent::sorted_map<int, int> m;
    for (int i = 0; i < 6; ++i) {
        m.insert({i, i * 10});
    }
    int sum = 0;
    for (int x : m | std::views::filter([](auto& p) { return p.first % 2 == 1; })
                   | std::views::transform([](auto& p) { return p.second; })
                   | std::views::take(2)) {
        sum += x;
    }
    EXPECT_EQ(sum, 10 + 30);
}
