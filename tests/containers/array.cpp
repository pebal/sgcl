//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <functional>
#include <iterator>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    struct Payload {
        int v;
        Payload(int x = 0) : v(x) { ++alive; }
        Payload(const Payload& o) : v(o.v) { ++alive; }
        Payload& operator=(const Payload&) = default;
        ~Payload() { --alive; }
        inline static sgcl::atomic<int> alive = {0};
    };

    struct Node {
        int v = 0;
        tracked_ptr<Node> next;
    };
}

// array<T, N>: the elements inline, a class with the braces of an aggregate.
static_assert(std::contiguous_iterator<sgcl::array<int, 4>::iterator>);
static_assert(!std::is_aggregate_v<sgcl::array<int, 4>> && std::is_trivially_copyable_v<sgcl::array<int, 4>> && std::is_trivially_default_constructible_v<sgcl::array<int, 4>>);
static_assert(std::tuple_size_v<sgcl::array<int, 4>> == 4);
static_assert(std::is_same_v<std::tuple_element_t<1, sgcl::array<double, 2>>, double>);
static_assert(sizeof(sgcl::array<int, 4>) == 4 * sizeof(int));

TEST(Array_Tests, FixedSizeWithTheBracesOfAnAggregate) {
    sgcl::array<int, 4> a = {1, 2, 3, 4};
    EXPECT_EQ(a.size(), 4u);
    EXPECT_FALSE(a.empty());
    EXPECT_EQ(a[2], 3);
    EXPECT_EQ(a.at(3), 4);
    EXPECT_THROW(a.at(4), std::out_of_range);
    EXPECT_EQ(a.front(), 1);
    EXPECT_EQ(a.back(), 4);
    EXPECT_EQ(std::accumulate(a.begin(), a.end(), 0), 10);
    EXPECT_EQ(*a.rbegin(), 4);
    auto [x, y, z, w] = a;   // structured bindings through the tuple interface
    EXPECT_EQ(x + w, 5);
    EXPECT_EQ(sgcl::get<1>(a), 2);
    sgcl::get<1>(a) = 20;
    EXPECT_EQ(a[1], 20);
    sgcl::array<int, 4> b = a;   // copied by value
    b.fill(7);
    EXPECT_EQ(a[0], 1);
    EXPECT_EQ(b[3], 7);
    EXPECT_NE(a, b);
    EXPECT_TRUE(a < b);
    swap(a, b);
    EXPECT_EQ(a[3], 7);
    EXPECT_EQ(b[1], 20);
    sgcl::array deduced = {1.5, 2.5};
    static_assert(std::is_same_v<decltype(deduced), sgcl::array<double, 2>>);
    auto converted = sgcl::to_array({3, 2, 1});
    static_assert(std::is_same_v<decltype(converted), sgcl::array<int, 3>>);
    std::ranges::sort(converted);
    EXPECT_EQ(converted[0], 1);
    sgcl::array<int, 0> none;
    EXPECT_TRUE(none.empty());
    EXPECT_EQ(none.begin(), none.end());
    EXPECT_EQ(none.data(), nullptr);
    constexpr sgcl::array<int, 3> c = {1, 2, 3};
    static_assert(c[1] == 2);
    static_assert(c.size() == 3);
}

TEST(Array_Tests, FixedSizeWithTrackedPointersOnTheStackAndInObjects) {
    sgcl::array<tracked_ptr<Node>, 8> roots;   // null pointers, on the stack
    for (int i = 0; i < 8; ++i) {
        roots[i] = make_tracked<Node>();
        roots[i]->v = i;
        if (i) {
            roots[i]->next = roots[i - 1];
        }
    }
    struct Holder {
        sgcl::array<tracked_ptr<Node>, 3> nodes;   // inline inside a managed object
        sgcl::array<int, 2> numbers = {5, 6};
    };
    tracked_ptr<Holder> h = make_tracked<Holder>();
    h->nodes[2] = roots[7];
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(roots[7]->next->v, 6);
    EXPECT_EQ(h->nodes[2]->v, 7);
    EXPECT_EQ(h->nodes[0], nullptr);
    EXPECT_EQ(h->numbers[1], 6);
    tracked_ptr<Node> keep = roots[3];
    roots.fill(nullptr);
    h = nullptr;
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(keep->v, 3);        // the chain behind it is intact
    EXPECT_EQ(keep->next->v, 2);
}

// The review's fixes: the comparator overloads of the bounds, the array in
// constant evaluation, the tuple interface.
namespace {
    template<class A>
    concept HasTupleSize = requires { std::tuple_size<A>::value; };

    template<class A>
    concept HasTupleElement = requires { typename std::tuple_element<0, A>::type; };

    constexpr bool sorts_in_constant_evaluation() {
        sgcl::array<int, 3> a = {3, 1, 2};
        a.sort();
        bool ascending = a[0] == 1 && a.is_sorted();
        a.reverse();
        return ascending && a[0] == 3 && a.is_sorted(std::greater<int>()) && *a.lower_bound(2, std::greater<int>()) == 2;
    }
}

TEST(Array_Tests, BoundsWithAComparator) {
    sgcl::array<int, 4> a = {7, 5, 3, 1};
    auto desc = std::greater<int>();
    EXPECT_EQ(*a.lower_bound(4, desc), 3);
    EXPECT_EQ(*a.upper_bound(5, desc), 3);
    EXPECT_EQ(a.upper_bound(1, desc), a.end());
    const auto& c = a;
    EXPECT_EQ(*c.lower_bound(5, desc), 5);
    EXPECT_EQ(c.upper_bound(0, desc), c.end());
    sgcl::array<int, 0> none;
    EXPECT_EQ(none.lower_bound(1, desc), none.end());
    EXPECT_EQ(none.upper_bound(1, desc), none.end());
    sgcl::dynamic_array<int> dynamic = {7, 5, 3, 1};
    EXPECT_EQ(*dynamic.lower_bound(4, desc), 3);
}

// The array in constant evaluation: the iterator is a literal type,
// the algorithms constexpr
namespace {
    constexpr sgcl::array<int, 3> constant = {1, 2, 3};
}
static_assert(*constant.begin() == 1 && constant.end() - constant.begin() == 3 && constant.begin()[2] == 3);
static_assert(constant.contains(2) && !constant.contains(4));
static_assert(*constant.find_if([](int x) { return x > 1; }) == 2);
static_assert(constant.find_if([](int x) { return x > 3; }) == nullptr);
static_assert(constant.index_of(3) == 2 && constant.last_index_of(1) == 0 && constant.find_index([](int x) { return x == 2; }) == 1);
static_assert(constant.is_sorted() && constant.binary_search(2) && constant.sorted_index_of(2) == 1);
static_assert(*constant.lower_bound(2) == 2 && constant.upper_bound(3) == constant.end());
static_assert(constant.min() == 1 && constant.max() == 3 && constant.count_of([](int x) { return x > 1; }) == 2);
static_assert(constant.exists([](int x) { return x == 3; }) && constant.all([](int x) { return x > 0; }));
static_assert(!sgcl::array<int, 0>{}.contains(1) && sgcl::array<int, 0>{}.begin() == sgcl::array<int, 0>{}.end());
static_assert(sorts_in_constant_evaluation());

// The tuple interface belongs to array<T, N>: for dynamic_array<T> the
// traits stay undefined, as for any non-tuple
static_assert(HasTupleSize<sgcl::array<int, 2>> && HasTupleElement<sgcl::array<int, 2>>);
static_assert(!HasTupleSize<sgcl::dynamic_array<int>> && !HasTupleElement<sgcl::dynamic_array<int>>);
static_assert(std::tuple_size_v<sgcl::array<int, 0>> == 0);
