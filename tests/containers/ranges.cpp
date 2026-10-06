//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Every container of core is a range of the category it declares, and the
// views of the standard library take it (tests/ranges.h)
#include "tests/types.h"
#include "tests/ranges.h"

#include <vector>

SGCL_CHECK_RANGE(std::ranges::contiguous_range, vector<int>);
SGCL_CHECK_RANGE(std::ranges::contiguous_range, array<int, 4>);
SGCL_CHECK_RANGE(std::ranges::contiguous_range, dynamic_array<int>);
SGCL_CHECK_RANGE(std::ranges::random_access_range, deque<int>);
SGCL_CHECK_RANGE(std::ranges::bidirectional_range, list<int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, forward_list<int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, map<int, int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, set<int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, multimap<int, int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, multiset<int>);
SGCL_CHECK_RANGE(std::ranges::bidirectional_range, ordered_map<int, int>);
SGCL_CHECK_RANGE(std::ranges::bidirectional_range, ordered_set<int>);
SGCL_CHECK_RANGE(std::ranges::bidirectional_range, sorted_map<int, int>);
SGCL_CHECK_RANGE(std::ranges::bidirectional_range, sorted_set<int>);
SGCL_CHECK_RANGE(std::ranges::bidirectional_range, sorted_multimap<int, int>);
SGCL_CHECK_RANGE(std::ranges::bidirectional_range, sorted_multiset<int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, weak_map<tracked_ptr<int>, int>);
SGCL_CHECK_RANGE(std::ranges::forward_range, weak_set<tracked_ptr<int>>);
SGCL_CHECK_RANGE(std::ranges::forward_range, decltype(std::declval<map<int, int>&>().keys()));
SGCL_CHECK_RANGE(std::ranges::forward_range, decltype(std::declval<map<int, int>&>().values()));
SGCL_CHECK_RANGE(std::ranges::bidirectional_range, const sorted_map<int, int>);
SGCL_CHECK_RANGE(std::ranges::contiguous_range, const vector<int>);

TEST(ContainerRanges_Tests, ViewsOverContainers) {
    vector<int> v = {1, 2, 3, 4, 5, 6};
    std::vector<int> got;
    for (int x : v | std::views::filter([](int x) { return x % 2 == 0; })
                   | std::views::transform([](int x) { return x * 10; })
                   | std::views::reverse
                   | std::views::take(2)) {
        got.push_back(x);
    }
    EXPECT_EQ(got, (std::vector<int>{60, 40}));

    sorted_map<int, int> m = {{3, 30}, {1, 10}, {2, 20}};
    got.clear();
    for (int x : m | std::views::reverse | std::views::transform([](auto& p) { return p.second; })) {
        got.push_back(x);
    }
    EXPECT_EQ(got, (std::vector<int>{30, 20, 10}));

    forward_list<int> f = {5, 6, 7};
    got.clear();
    for (int x : f | std::views::take(2)) {
        got.push_back(x);
    }
    EXPECT_EQ(got, (std::vector<int>{5, 6}));
}
