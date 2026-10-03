//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <functional>
#include <array>
#include <cstring>
#include <iterator>
#include <numeric>
#include <ranges>
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

namespace {
    template<class Make>
    void leave_garbage_da(size_t n, Make make) {
        for (int round = 0; round < 20; ++round) {
            auto a = make(n);
            std::memset(static_cast<void*>(a.data()), 0xAB, n * sizeof(a[0]));
        }
        collector::force_collect(true);
        collector::force_collect(true);
    }

    template<class C>
    size_t nonzero_bytes_da(const C& c) {
        auto p = reinterpret_cast<const unsigned char*>(c.data());
        size_t bad = 0;
        for (size_t i = 0; i < c.size() * sizeof(c[0]); ++i) {
            bad += p[i] != 0;
        }
        return bad;
    }
}

// dynamic_array(count) value-initializes: zeros for a trivial type, also
// where the buffer reuses the slot or the pages of an earlier one that
// held other bytes (a buffer of a type without tracked pointers is not
// zeroed when it is issued: maker.h); past a page as well
TEST(DynamicArray_Tests, CountValueInitializesAfterReuse) {
    for (size_t n : {size_t(64), size_t(1000), size_t(100000), size_t(3000000)}) {
        leave_garbage_da(n, [](size_t k) { return dynamic_array<std::byte>(k); });
        for (int round = 0; round < 5; ++round) {
            dynamic_array<std::byte> a(n);
            EXPECT_EQ(nonzero_bytes_da(a), 0u) << "dynamic_array<byte>(" << n << ")";
        }
        leave_garbage_da(n, [](size_t k) { return dynamic_array<int>(k); });
        for (int round = 0; round < 5; ++round) {
            dynamic_array<int> a(n);
            EXPECT_EQ(nonzero_bytes_da(a), 0u) << "dynamic_array<int>(" << n << ")";
        }
    }
}

// From a range of what the elements are made of, as every other sequence
// of the library (vector, deque, list, forward_list) and C++23's
// from_range: explicit, and not for a dynamic_array of the same type,
// which is the copy. The edges: an empty range, a view, a single-pass
// range, elements of another type, a construction that throws half-way.
namespace {
    struct ThrowsAtThree {
        static inline int alive = 0;
        int v;
        ThrowsAtThree(int x) : v(x) {
            if (x == 3) {
                throw std::runtime_error("three");
            }
            ++alive;
        }
        ThrowsAtThree(const ThrowsAtThree& o) noexcept : v(o.v) { ++alive; }
        ~ThrowsAtThree() { --alive; }
    };
}

TEST(DynamicArray_Test, FromARangeOfWhatTheElementsAreMadeOf) {
    static_assert(std::is_constructible_v<sgcl::dynamic_array<long>, std::vector<int>&>);
    static_assert(std::is_constructible_v<sgcl::dynamic_array<std::string>, const std::vector<const char*>&>);
    static_assert(!std::is_convertible_v<std::vector<int>&, sgcl::dynamic_array<int>>);   // explicit
    static_assert(!std::is_constructible_v<sgcl::dynamic_array<int>, std::vector<std::string>&>);

    std::vector<int> v = {1, 2, 3};
    sgcl::dynamic_array<long> longs(v);
    ASSERT_EQ(longs.size(), 3u);
    EXPECT_EQ(longs[2], 3);
    EXPECT_TRUE(sgcl::dynamic_array<int>(std::vector<int>()).empty());
    sgcl::dynamic_array<int> squares(std::views::iota(0, 5) | std::views::transform([](int x) { return x * x; }));
    EXPECT_EQ(squares.size(), 5u);
    EXPECT_EQ(squares[4], 16);
    std::istringstream in("8 9");
    auto words = std::ranges::subrange(std::istream_iterator<int>(in), std::istream_iterator<int>());   // single pass
    sgcl::dynamic_array<int> read(words);
    EXPECT_EQ(read.size(), 2u);
    EXPECT_EQ(read[1], 9);
    sgcl::dynamic_array<int> copy(squares);              // the copy constructor, not the range's
    EXPECT_EQ(copy.size(), 5u);

    EXPECT_THROW(sgcl::dynamic_array<ThrowsAtThree>(std::vector<int>{1, 2, 3, 4}), std::runtime_error);
    EXPECT_EQ(ThrowsAtThree::alive, 0);                  // the two made before the throw destroyed
}

// Boundaries (DESIGN 408)

// Counts past max_size() are length_error (one past, past SIZE_MAX /
// sizeof, SIZE_MAX); a count within it that no memory holds ends the
// program as a refused managed allocation; at and as_slice at the ends
TEST(DynamicArray_Tests, CountsAtTheLimits) {
    EXPECT_EQ(sgcl::dynamic_array<char>().max_size(), size_t(PTRDIFF_MAX));
    EXPECT_EQ(sgcl::dynamic_array<int>().max_size(), size_t(PTRDIFF_MAX) / 4);
    for (size_t count : {sgcl::dynamic_array<int>().max_size() + 1, SIZE_MAX / 4 + 1, SIZE_MAX}) {
        EXPECT_THROW((void)sgcl::dynamic_array<int>(count), std::length_error);
        EXPECT_THROW((void)sgcl::dynamic_array<int>(count, 7), std::length_error);
    }
    sgcl::dynamic_array<int> a = {1, 2, 3};
    EXPECT_THROW(a.at(3), std::out_of_range);
    EXPECT_THROW(a.at(SIZE_MAX), std::out_of_range);
    EXPECT_THROW(a.as_slice(4), std::out_of_range);
    EXPECT_EQ(a.as_slice(3).size(), 0u);
    EXPECT_EQ(a.as_slice(1, SIZE_MAX).size(), 2u);
}

TEST(DynamicArray_Tests, ACountNoMemoryHoldsEnds) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");   // a forked child may not allocate managed memory (os.h)
    auto construct = [] {
        sgcl::dynamic_array<char> a(sgcl::dynamic_array<char>().max_size());
    };
    auto fill = [] {
        sgcl::dynamic_array<int> a(sgcl::dynamic_array<int>().max_size(), 1);
    };
    EXPECT_DEATH(construct(), "sgcl: out of managed memory");
    EXPECT_DEATH(fill(), "sgcl: out of managed memory");
}

namespace {
    // Every member on an array without a buffer (default, moved from, of
    // no element)
    template<class T>
    void expect_dynamic_array_works_empty(sgcl::dynamic_array<T>& a) {
        EXPECT_TRUE(a.empty());
        EXPECT_EQ(a.size(), 0u);
        EXPECT_EQ(a.data(), nullptr);
        EXPECT_EQ(a.begin(), a.end());
        EXPECT_EQ(a.rbegin(), a.rend());
        EXPECT_THROW(a.at(0), std::out_of_range);
        EXPECT_EQ(a.as_slice().size(), 0u);
        EXPECT_EQ(a.as_slice(0, SIZE_MAX).size(), 0u);
        EXPECT_THROW(a.as_slice(1), std::out_of_range);
        EXPECT_TRUE(a == sgcl::dynamic_array<T>());
        EXPECT_FALSE(a < sgcl::dynamic_array<T>());
        sgcl::dynamic_array<T> copy(a);
        EXPECT_TRUE(copy.empty());
        copy = a;
        EXPECT_TRUE(copy.empty());
        sgcl::dynamic_array<T> other(2);
        a.swap(other);
        EXPECT_EQ(a.size(), 2u);
        a.swap(other);
        EXPECT_TRUE(a.empty());
        other = a;   // a copy of another size: other empty now
        EXPECT_TRUE(other.empty());
    }
}

TEST(DynamicArray_Tests, MovedFromAndDefaultWorkAsEmpty) {
    sgcl::dynamic_array<std::string> a = {"a", "b"};
    sgcl::dynamic_array<std::string> to(std::move(a));
    EXPECT_EQ(to.size(), 2u);
    expect_dynamic_array_works_empty(a);
    sgcl::dynamic_array<std::string> assigned = {"c"};
    assigned = std::move(to);
    EXPECT_EQ(assigned.size(), 2u);
    expect_dynamic_array_works_empty(to);
    sgcl::dynamic_array<int> d;
    expect_dynamic_array_works_empty(d);
    sgcl::dynamic_array<int> none(0);
    expect_dynamic_array_works_empty(none);
    sgcl::dynamic_array<int> none_of(0, 5);
    expect_dynamic_array_works_empty(none_of);
    std::vector<int> nothing;
    sgcl::dynamic_array<int> of_nothing(nothing.begin(), nothing.end());
    expect_dynamic_array_works_empty(of_nothing);
    std::istringstream in("");
    std::istream_iterator<int> first(in);
    std::istream_iterator<int> last;
    sgcl::dynamic_array<int> of_no_input(first, last);
    expect_dynamic_array_works_empty(of_no_input);
}

// The array on both sides: a copy and a move assignment to itself and
// swap with itself keep the elements and the buffer; a list of its own
// elements assigned to it
TEST(DynamicArray_Tests, AnArrayOnBothSidesKeepsItself) {
    sgcl::dynamic_array<std::string> a = {"one long enough for the heap", "two", "three"};
    const auto before = a;
    const auto data = a.data();
    auto& self = a;
    a = self;
    a = std::move(self);
    a.swap(self);
    swap(a, self);
    EXPECT_EQ(a, before);
    EXPECT_EQ(a.data(), data);
    a = {a[2], a[0]};
    EXPECT_EQ(a, (sgcl::dynamic_array<std::string>{"three", "one long enough for the heap"}));
    sgcl::dynamic_array<std::string> one = {"only"};
    EXPECT_EQ(&one.front(), &one.back());
}

// The iterators as the page states them: a copy of the same size assigns
// in place, swap leaves an iterator on its element in the other array, a
// slice keeps the buffer past the array's end
TEST(DynamicArray_Tests, IteratorsAsThePageStatesThem) {
    sgcl::dynamic_array<int> a = {1, 2, 3};
    auto second = a.begin() + 1;
    const sgcl::dynamic_array<int> same_size = {4, 5, 6};
    a = same_size;
    EXPECT_EQ(*second, 5);
    sgcl::dynamic_array<int> b;
    a.swap(b);
    EXPECT_EQ(b.begin() + 1, second);
    auto slice = b.as_slice();
    b = sgcl::dynamic_array<int>{7};
    EXPECT_EQ(slice.size(), 3u);
    EXPECT_EQ(slice[2], 6);
}
