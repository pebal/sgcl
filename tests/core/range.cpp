//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// range: a pair of iterators as a range, and the integers of a half-open
// interval, in a range-for and under std::ranges.
#include "sgcl/core/range.h"
#include "sgcl/core/sorted_multimap.h"
#include "tests/types.h"

#include <algorithm>
#include <ranges>
#include <vector>

TEST(Range_Tests, PairOfIteratorsFromEqualRange) {
    sgcl::sorted_multimap<int, int> m = {{1, 10}, {1, 11}, {2, 20}};
    sgcl::range ones = m.equal_range(1);               // range<iterator>, deduced from the pair
    EXPECT_FALSE(ones.empty());
    EXPECT_EQ(ones.size(), 2u);
    EXPECT_EQ(ones.front().second, 10);
    int sum = 0;
    for (auto& [key, value] : ones) {
        sum += value;
    }
    EXPECT_EQ(sum, 21);
    sgcl::range none = m.equal_range(3);
    EXPECT_TRUE(none.empty());
    EXPECT_EQ(none.size(), 0u);
    sgcl::range<sgcl::sorted_multimap<int, int>::iterator> empty;   // default: empty
    EXPECT_TRUE(empty.empty());
}

TEST(Range_Tests, CountingFromZero) {
    int sum = 0;
    int count = 0;
    for (int i : sgcl::range(10)) {
        sum += i;
        ++count;
    }
    EXPECT_EQ(sum, 45);
    EXPECT_EQ(count, 10);
    EXPECT_TRUE(sgcl::range(0).empty());
    EXPECT_EQ(sgcl::range(1).size(), 1u);
    EXPECT_EQ(sgcl::range(1).front(), 0);
    EXPECT_TRUE(sgcl::range(-3).empty());               // a negative end: nothing to count
}

TEST(Range_Tests, CountingFromFirstToLast) {
    std::vector<int> seen;
    for (int i : sgcl::range(2, 10)) {
        seen.push_back(i);
    }
    EXPECT_EQ(seen, (std::vector<int>{2, 3, 4, 5, 6, 7, 8, 9}));
    EXPECT_EQ(sgcl::range(2, 10).size(), 8u);
    EXPECT_EQ(sgcl::range(2, 10).front(), 2);
    EXPECT_TRUE(sgcl::range(5, 5).empty());
    EXPECT_TRUE(sgcl::range(7, 3).empty());              // first past last: empty, not a walk down
    EXPECT_EQ(sgcl::range(7, 3).size(), 0u);
    sgcl::range<sgcl::detail::counter<int>> r(-2, 2);
    EXPECT_EQ(r.size(), 4u);
    EXPECT_EQ(r.front(), -2);
}

TEST(Range_Tests, OtherIntegerTypes) {
    unsigned total = 0;
    for (unsigned i : sgcl::range(3u)) {
        total += i;
    }
    EXPECT_EQ(total, 3u);
    size_t n = 0;
    for (auto i : sgcl::range(size_t(4), size_t(6))) {
        static_assert(std::is_same_v<decltype(i), size_t>);
        n += i;
    }
    EXPECT_EQ(n, 9u);
}

TEST(Range_Tests, TheCounterIsRandomAccessToStdRangesAndInputToTheOldCategory) {
    using C = sgcl::detail::counter<int>;
    static_assert(std::is_same_v<std::iterator_traits<C>::iterator_category, std::input_iterator_tag>);   // *i is a value, which the old forward category forbids
    static_assert(std::is_same_v<C::iterator_concept, std::random_access_iterator_tag>);
    static_assert(std::random_access_iterator<C>);
    static_assert(std::sized_sentinel_for<C, C>);
    static_assert(std::ranges::random_access_range<sgcl::range<C>>);
    static_assert(std::ranges::sized_range<sgcl::range<C>>);
    EXPECT_EQ(sgcl::range(0, 1 << 30).size(), size_t(1) << 30);          // a subtraction, not a walk
    EXPECT_EQ(std::ranges::distance(sgcl::range(5, 1 << 30)), (1 << 30) - 5);
    auto r = sgcl::range(3, 8);
    EXPECT_EQ(r.begin()[2], 5);
    EXPECT_EQ(*(r.begin() + 4), 7);
    EXPECT_EQ(r.end() - r.begin(), 5);
}

TEST(Range_Tests, UnderStdRanges) {
    static_assert(std::ranges::random_access_range<sgcl::range<sgcl::detail::counter<int>>>);
    static_assert(std::ranges::sized_range<sgcl::range<sgcl::detail::counter<int>>>);
    EXPECT_EQ(std::ranges::size(sgcl::range(10)), 10u);
    int sum = 0;
    std::ranges::for_each(sgcl::range(1, 5), [&](int i) { sum += i; });
    EXPECT_EQ(sum, 10);
    auto squares = sgcl::range(4) | std::views::transform([](int i) { return i * i; });
    std::vector<int> v(squares.begin(), squares.end());
    EXPECT_EQ(v, (std::vector<int>{0, 1, 4, 9}));
    EXPECT_EQ(*std::ranges::max_element(sgcl::range(3, 8)), 7);   // a borrowed range: the iterator outlives the temporary
    static_assert(std::ranges::borrowed_range<sgcl::range<sgcl::detail::counter<int>>>);
    static_assert(std::ranges::forward_range<sgcl::range<sgcl::sorted_multimap<int, int>::iterator>>);
}
