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

// sgcl::dynamic_array: a buffer whose size is a value, fixed when it is
// created, behind a handle of two words.
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

static_assert(std::contiguous_iterator<sgcl::dynamic_array<int>::iterator>);
static_assert(std::ranges::contiguous_range<sgcl::dynamic_array<int>>);

TEST(DynamicArray_Tests, ConstructionAndAccess) {
    sgcl::dynamic_array<int> empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0u);
    EXPECT_EQ(empty.begin(), empty.end());
    EXPECT_EQ(empty.data(), nullptr);
    sgcl::dynamic_array<int> zeros(5);
    EXPECT_EQ(zeros.size(), 5u);
    for (auto x : zeros) {
        EXPECT_EQ(x, 0);
    }
    sgcl::dynamic_array<int> sevens(4, 7);
    EXPECT_EQ(sevens[3], 7);
    EXPECT_EQ(sevens.front(), 7);
    EXPECT_EQ(sevens.back(), 7);
    sgcl::dynamic_array<int> list = {1, 2, 3};
    EXPECT_EQ(list.at(2), 3);
    EXPECT_THROW(list.at(3), std::out_of_range);
    std::vector<int> src = {4, 5, 6, 7};
    sgcl::dynamic_array from(src.begin(), src.end());
    static_assert(std::is_same_v<decltype(from), sgcl::dynamic_array<int>>);
    EXPECT_EQ(from.size(), 4u);
    EXPECT_EQ(std::accumulate(from.begin(), from.end(), 0), 22);
    EXPECT_EQ(*from.rbegin(), 7);
    EXPECT_EQ(std::distance(from.crbegin(), from.crend()), 4);
    std::istringstream in("8 9");
    sgcl::dynamic_array<int> streamed{std::istream_iterator<int>(in), std::istream_iterator<int>()};
    EXPECT_EQ(streamed.size(), 2u);
    EXPECT_EQ(streamed[1], 9);
}

TEST(DynamicArray_Tests, CopyMoveAssignAndCompare) {
    sgcl::dynamic_array<std::string> a = {"x", "y", "z"};
    sgcl::dynamic_array<std::string> b(a);
    EXPECT_EQ(a, b);
    EXPECT_NE(a.data(), b.data());   // a copy has a buffer of its own
    b[0] = "w";
    EXPECT_NE(a, b);
    EXPECT_TRUE(b < a);
    sgcl::dynamic_array<std::string> c;
    c = a;   // different sizes: a new buffer
    EXPECT_EQ(c, a);
    auto data = c.data();
    c = b;   // same size: assigned in place
    EXPECT_EQ(c.data(), data);
    EXPECT_EQ(c, b);
    sgcl::dynamic_array<std::string> d = std::move(a);
    EXPECT_EQ(d.size(), 3u);
    EXPECT_TRUE(a.empty());
    a = std::move(d);
    EXPECT_EQ(a[2], "z");
    a = {"p"};
    EXPECT_EQ(a.size(), 1u);
    sgcl::dynamic_array<double> e = {1.0, 2.0};
    sgcl::dynamic_array<double> f = {1.0, 3.0};
    EXPECT_TRUE(e < f);
    EXPECT_TRUE((e <=> f) == std::partial_ordering::less);
    swap(e, f);
    EXPECT_EQ(e[1], 3.0);
}

TEST(DynamicArray_Tests, FillAndAlgorithms) {
    sgcl::dynamic_array<int> a(6);
    a.fill(3);
    EXPECT_EQ(std::accumulate(a.begin(), a.end(), 0), 18);
    std::iota(a.begin(), a.end(), 0);
    std::ranges::reverse(a);
    EXPECT_EQ(a[0], 5);
    std::ranges::sort(a);
    EXPECT_EQ(a[0], 0);
    EXPECT_EQ(std::ranges::count(a, 4), 1);
}

TEST(DynamicArray_Tests, ElementsDieWithTheHandleWhateverPointsInside) {
    collector::force_collect(true);
    Payload::alive = 0;
    Payload* alias = nullptr;   // a raw word in this frame: a candidate root for the scan
    off_frame([&] {
        sgcl::dynamic_array<Payload> a(10);
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
TEST(DynamicArray_Tests, TrackedElementsAreTraced) {
    sgcl::dynamic_array<tracked_ptr<Node>> a(1000);
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
    sgcl::dynamic_array<tracked_ptr<Node>> big(200000);   // a large buffer, many pages
    big[199999] = a[0];
    collector::force_collect(true);
    EXPECT_EQ(big[199999]->v, 0);
    EXPECT_EQ(big[0], nullptr);
}

TEST(DynamicArray_Tests, ArrayInsideAManagedObject) {
    struct Holder {
        sgcl::dynamic_array<int> values;
        sgcl::dynamic_array<tracked_ptr<Node>> nodes;
    };
    tracked_ptr<Holder> h = make_tracked<Holder>();
    h->values = sgcl::dynamic_array<int>(3, 4);
    h->nodes = sgcl::dynamic_array<tracked_ptr<Node>>(2);
    h->nodes[1] = make_tracked<Node>();
    h->nodes[1]->v = 11;
    for (int i = 0; i < 3; ++i) {
        collector::force_collect(true);
    }
    EXPECT_EQ(h->values[2], 4);
    EXPECT_EQ(h->nodes[1]->v, 11);
}

TEST(DynamicArray_Tests, LengthError) {
    sgcl::dynamic_array<int> a;
    EXPECT_THROW(sgcl::dynamic_array<int> b(a.max_size() + 1), std::length_error);
}

// The review's fixes: the constructors under an exception.
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
}

TEST(DynamicArray_Tests, ConstructorThatThrowsDestroysWhatItBuilt) {
    ThrowingPayload::alive = 0;
    {
        ThrowingPayload value(5);
        ThrowingPayload::copies = 0;
        ThrowingPayload::throw_at = 2;   // the third copy
        EXPECT_THROW(sgcl::dynamic_array<ThrowingPayload> a(5, value), std::runtime_error);
        EXPECT_EQ(ThrowingPayload::alive, 1);   // the two built were destroyed
        std::vector<ThrowingPayload> src = {value, value, value, value};
        ThrowingPayload::copies = 0;
        ThrowingPayload::throw_at = 2;
        EXPECT_THROW(sgcl::dynamic_array<ThrowingPayload> a(src.begin(), src.end()), std::runtime_error);
        EXPECT_EQ(ThrowingPayload::alive, 5);
        ThrowingPayload::throw_at = -1;
    }
    EXPECT_EQ(ThrowingPayload::alive, 0);
    DefaultThrower::made = 0;
    DefaultThrower::throw_at = 2;
    EXPECT_THROW(sgcl::dynamic_array<DefaultThrower> a(4), std::runtime_error);
    EXPECT_EQ(DefaultThrower::alive, 0);
    DefaultThrower::throw_at = -1;
}
