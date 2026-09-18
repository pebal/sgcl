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

// sgcl::array: a fixed-size managed buffer behind a one-word handle.
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

static_assert(std::contiguous_iterator<sgcl::array<int>::iterator>);
static_assert(std::ranges::contiguous_range<sgcl::array<int>>);

TEST(Array_Tests, ConstructionAndAccess) {
    sgcl::array<int> empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0u);
    EXPECT_EQ(empty.begin(), empty.end());
    EXPECT_EQ(empty.data(), nullptr);
    sgcl::array<int> zeros(5);
    EXPECT_EQ(zeros.size(), 5u);
    for (auto x : zeros) {
        EXPECT_EQ(x, 0);
    }
    sgcl::array<int> sevens(4, 7);
    EXPECT_EQ(sevens[3], 7);
    EXPECT_EQ(sevens.front(), 7);
    EXPECT_EQ(sevens.back(), 7);
    sgcl::array<int> list = {1, 2, 3};
    EXPECT_EQ(list.at(2), 3);
    EXPECT_THROW(list.at(3), std::out_of_range);
    std::vector<int> src = {4, 5, 6, 7};
    sgcl::array from(src.begin(), src.end());
    static_assert(std::is_same_v<decltype(from), sgcl::array<int>>);
    EXPECT_EQ(from.size(), 4u);
    EXPECT_EQ(std::accumulate(from.begin(), from.end(), 0), 22);
    EXPECT_EQ(*from.rbegin(), 7);
    EXPECT_EQ(std::distance(from.crbegin(), from.crend()), 4);
    std::istringstream in("8 9");
    sgcl::array<int> streamed{std::istream_iterator<int>(in), std::istream_iterator<int>()};
    EXPECT_EQ(streamed.size(), 2u);
    EXPECT_EQ(streamed[1], 9);
}

TEST(Array_Tests, CopyMoveAssignAndCompare) {
    sgcl::array<std::string> a = {"x", "y", "z"};
    sgcl::array<std::string> b(a);
    EXPECT_EQ(a, b);
    EXPECT_NE(a.data(), b.data());   // a copy has a buffer of its own
    b[0] = "w";
    EXPECT_NE(a, b);
    EXPECT_TRUE(b < a);
    sgcl::array<std::string> c;
    c = a;   // different sizes: a new buffer
    EXPECT_EQ(c, a);
    auto data = c.data();
    c = b;   // same size: assigned in place
    EXPECT_EQ(c.data(), data);
    EXPECT_EQ(c, b);
    sgcl::array<std::string> d = std::move(a);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_TRUE(a.empty());
    a = std::move(d);
    EXPECT_EQ(a[2], "z");
    a = {"p"};
    EXPECT_EQ(a.size(), 1u);
    sgcl::array<double> e = {1.0, 2.0};
    sgcl::array<double> f = {1.0, 3.0};
    EXPECT_TRUE(e < f);
    EXPECT_TRUE((e <=> f) == std::partial_ordering::less);
    swap(e, f);
    EXPECT_EQ(e[1], 3.0);
}

TEST(Array_Tests, FillAndAlgorithms) {
    sgcl::array<int> a(6);
    a.fill(3);
    EXPECT_EQ(std::accumulate(a.begin(), a.end(), 0), 18);
    std::iota(a.begin(), a.end(), 0);
    std::ranges::reverse(a);
    EXPECT_EQ(a[0], 5);
    std::ranges::sort(a);
    EXPECT_EQ(a[0], 0);
    EXPECT_EQ(std::ranges::count(a, 4), 1);
}

TEST(Array_Tests, ElementsDieWithTheHandleWhateverPointsInside) {
    collector::force_collect(true);
    Payload::alive = 0;
    Payload* alias = nullptr;   // a raw word in this frame: a candidate root for the scan
    off_frame([&] {
        sgcl::array<Payload> a(10);
        for (int i = 0; i < 10; ++i) {
            a[i].v = i;
        }
        alias = &a[7];
        EXPECT_EQ(Payload::alive, 10);
    });
    // the handle is gone and destroyed its elements although a word still
    // points into the buffer: the buffer itself waits for the collector
    // (rooted only through its first element), empty
    EXPECT_EQ(Payload::alive, 0);
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(Payload::alive, 0);
    alias = nullptr;
}
TEST(Array_Tests, TrackedElementsAreTraced) {
    sgcl::array<tracked_ptr<Node>> a(1000);
    for (int i = 0; i < 1000; ++i) {
        a[i] = make_tracked<Node>();
        a[i]->v = i;
        if (i) {
            a[i]->next = a[i - 1];
        }
    }
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(a[i]->v, i);
    }
    EXPECT_EQ(a[999]->next->v, 998);
    sgcl::array<tracked_ptr<Node>> big(200000);   // a large buffer, many pages
    big[199999] = a[0];
    collector::force_collect(true);
    EXPECT_EQ(big[199999]->v, 0);
    EXPECT_EQ(big[0], nullptr);
}

TEST(Array_Tests, ArrayInsideAManagedObject) {
    struct Holder {
        sgcl::array<int> values;
        sgcl::array<tracked_ptr<Node>> nodes;
    };
    tracked_ptr<Holder> h = make_tracked<Holder>();
    h->values = sgcl::array<int>(3, 4);
    h->nodes = sgcl::array<tracked_ptr<Node>>(2);
    h->nodes[1] = make_tracked<Node>();
    h->nodes[1]->v = 11;
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(h->values[2], 4);
    EXPECT_EQ(h->nodes[1]->v, 11);
}

TEST(Array_Tests, LengthError) {
    sgcl::array<int> a;
    EXPECT_THROW(sgcl::array<int> b(a.max_size() + 1), std::length_error);
}

// array<T, N>: the elements inline, an aggregate.
static_assert(std::contiguous_iterator<sgcl::array<int, 4>::iterator>);
static_assert(std::is_aggregate_v<sgcl::array<int, 4>>);
static_assert(std::tuple_size_v<sgcl::array<int, 4>> == 4);
static_assert(std::is_same_v<std::tuple_element_t<1, sgcl::array<double, 2>>, double>);
static_assert(sizeof(sgcl::array<int, 4>) == 4 * sizeof(int));

TEST(Array_Tests, FixedSizeAggregate) {
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

// The review's fixes: the constructors of array<T> under an exception, the
// comparator overloads of the bounds, the aggregate in constant
// evaluation, the tuple interface for the aggregate only.
namespace {
    struct ThrowingPayload {
        static inline int alive = 0;
        static inline int copies = 0;
        static inline int throw_at = -1;
        int v;
        explicit ThrowingPayload(int x) : v(x) { ++alive; }
        ThrowingPayload(const ThrowingPayload& o) : v(o.v) { if (copies++ == throw_at) { throw std::runtime_error("copy"); } ++alive; }
        ~ThrowingPayload() { --alive; }
    };

    struct DefaultThrower {
        static inline int alive = 0;
        static inline int made = 0;
        static inline int throw_at = -1;
        DefaultThrower() { if (made++ == throw_at) { throw std::runtime_error("default"); } ++alive; }
        ~DefaultThrower() { --alive; }
    };

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

TEST(Array_Tests, ConstructorThatThrowsDestroysWhatItBuilt) {
    ThrowingPayload::alive = 0;
    {
        ThrowingPayload value(5);
        ThrowingPayload::copies = 0;
        ThrowingPayload::throw_at = 2;   // the third copy
        EXPECT_THROW(sgcl::array<ThrowingPayload> a(5, value), std::runtime_error);
        EXPECT_EQ(ThrowingPayload::alive, 1);   // the two built were destroyed
        std::vector<ThrowingPayload> src = {value, value, value, value};
        ThrowingPayload::copies = 0;
        ThrowingPayload::throw_at = 2;
        EXPECT_THROW(sgcl::array<ThrowingPayload> a(src.begin(), src.end()), std::runtime_error);
        EXPECT_EQ(ThrowingPayload::alive, 5);
        ThrowingPayload::throw_at = -1;
    }
    EXPECT_EQ(ThrowingPayload::alive, 0);
    DefaultThrower::made = 0;
    DefaultThrower::throw_at = 2;
    EXPECT_THROW(sgcl::array<DefaultThrower> a(4), std::runtime_error);
    EXPECT_EQ(DefaultThrower::alive, 0);
    DefaultThrower::throw_at = -1;
}

TEST(Array_Tests, BoundsWithAComparatorOnTheAggregate) {
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
    sgcl::array<int> dynamic = {7, 5, 3, 1};
    EXPECT_EQ(*dynamic.lower_bound(4, desc), 3);
}

// The aggregate in constant evaluation: the iterator is a literal type,
// the algorithms constexpr
namespace {
    constexpr sgcl::array<int, 3> constant = {1, 2, 3};
}
static_assert(*constant.begin() == 1 && constant.end() - constant.begin() == 3 && constant.begin()[2] == 3);
static_assert(constant.contains(2) && !constant.contains(4));
static_assert(*constant.find([](int x) { return x > 1; }) == 2);
static_assert(constant.find([](int x) { return x > 3; }) == nullptr);
static_assert(constant.index_of(3) == 2 && constant.last_index_of(1) == 0 && constant.find_index([](int x) { return x == 2; }) == 1);
static_assert(constant.is_sorted() && constant.binary_search(2) && constant.sorted_index_of(2) == 1);
static_assert(*constant.lower_bound(2) == 2 && constant.upper_bound(3) == constant.end());
static_assert(constant.min() == 1 && constant.max() == 3 && constant.count_of([](int x) { return x > 1; }) == 2);
static_assert(constant.exists([](int x) { return x == 3; }) && constant.all([](int x) { return x > 0; }));
static_assert(!sgcl::array<int, 0>{}.contains(1) && sgcl::array<int, 0>{}.begin() == sgcl::array<int, 0>{}.end());
static_assert(sorts_in_constant_evaluation());

// The tuple interface belongs to the aggregate: for array<T> the traits
// stay undefined, as for any non-tuple
static_assert(HasTupleSize<sgcl::array<int, 2>> && HasTupleElement<sgcl::array<int, 2>>);
static_assert(!HasTupleSize<sgcl::array<int>> && !HasTupleElement<sgcl::array<int>>);
static_assert(std::tuple_size_v<sgcl::array<int, 0>> == 0);
