//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <algorithm>
#include <iterator>
#include <list>
#include <random>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// The list holds a tracked_ptr (its sentinel), so it lives in the test's
// frame or inside a managed object. An iterator is a raw node pointer: it
// may sit in a std container or a lambda's capture like any pointer, and
// one left in a temporary of the test's frame roots its node until the
// frame dies (README, "Stack roots"). A step that takes iterators before a
// live-object count therefore runs in a frame of its own (framed below).

static_assert(std::bidirectional_iterator<sgcl::list<int>::iterator>);
static_assert(std::bidirectional_iterator<sgcl::list<int>::const_iterator>);
static_assert(std::bidirectional_iterator<sgcl::list<std::string>::iterator>);
static_assert(std::is_trivially_copyable_v<sgcl::list<int>::iterator>);
static_assert(std::is_trivially_copyable_v<sgcl::list<int>::const_iterator>);
static_assert(std::is_nothrow_default_constructible_v<sgcl::list<int>::iterator>);
static_assert(sizeof(sgcl::list<int>::iterator) == sizeof(void*));
static_assert(std::ranges::bidirectional_range<sgcl::list<int>>);
static_assert(std::ranges::bidirectional_range<const sgcl::list<int>>);
static_assert(std::ranges::sized_range<sgcl::list<int>>);
static_assert(std::convertible_to<sgcl::list<int>::iterator, sgcl::list<int>::const_iterator>);
static_assert(!std::convertible_to<sgcl::list<int>::const_iterator, sgcl::list<int>::iterator>);
static_assert(std::is_same_v<std::iter_value_t<sgcl::list<int>::const_iterator>, int>);
static_assert(std::is_same_v<std::iter_reference_t<sgcl::list<int>::iterator>, int&>);
static_assert(std::is_same_v<std::iter_reference_t<sgcl::list<int>::const_iterator>, const int&>);
static_assert(std::is_same_v<std::iterator_traits<sgcl::list<int>::iterator>::iterator_category, std::bidirectional_iterator_tag>);
static_assert(std::is_same_v<sgcl::list<int>::value_type, int>);
static_assert(std::is_same_v<sgcl::list<int>::reference, int&>);
static_assert(std::is_same_v<sgcl::list<int>::const_reference, const int&>);
static_assert(std::is_same_v<sgcl::list<int>::pointer, int*>);
static_assert(std::is_same_v<sgcl::list<int>::const_pointer, const int*>);
static_assert(std::is_same_v<sgcl::list<int>::size_type, size_t>);
static_assert(std::is_same_v<sgcl::list<int>::difference_type, ptrdiff_t>);
static_assert(std::is_same_v<sgcl::list<int>::reverse_iterator, std::reverse_iterator<sgcl::list<int>::iterator>>);
static_assert(std::is_same_v<sgcl::list<int>::const_reverse_iterator, std::reverse_iterator<sgcl::list<int>::const_iterator>>);
static_assert(std::is_nothrow_default_constructible_v<sgcl::list<int>>);
static_assert(std::is_nothrow_move_constructible_v<sgcl::list<int>>);
static_assert(std::is_nothrow_move_assignable_v<sgcl::list<int>>);
static_assert(std::is_nothrow_swappable_v<sgcl::list<int>>);
static_assert(!noexcept(std::declval<sgcl::list<int>&>().sort()));
static_assert(!noexcept(std::declval<sgcl::list<int>&>().merge(std::declval<sgcl::list<int>&>())));
static_assert(noexcept(std::declval<sgcl::list<int>&>().reverse()));
static_assert(noexcept(std::declval<sgcl::list<int>&>().clear()));
static_assert(std::three_way_comparable<sgcl::list<int>>);
static_assert(std::three_way_comparable<sgcl::list<std::string>>);

namespace {
    template<class V = int, class L>
    std::vector<V> to_vector(const L& lst) {
        return std::vector<V>(lst.begin(), lst.end());
    }

    template<class V = int, class L>
    std::vector<V> reverse_vector(const L& lst) {
        return std::vector<V>(lst.rbegin(), lst.rend());
    }

    // Runs f in a frame of its own and returns its result: the iterators f
    // takes die with that frame instead of lingering in the test's.
    template<class F>
    SGCL_NOINLINE decltype(auto) framed(F&& f) {
        return f();
    }

    // Walks the list forward and back: the links agree with each other and
    // with size().
    template<class L>
    void expect_consistent(const L& lst) {
        size_t forward = 0;
        auto it = lst.begin();
        for (; it != lst.end(); ++it) {
            ++forward;
        }
        EXPECT_EQ(forward, lst.size());
        size_t backward = 0;
        while (it != lst.begin()) {
            --it;
            ++backward;
        }
        EXPECT_EQ(backward, lst.size());
    }

    // Counts its live instances, like Int, and compares with its own kind
    // (Int's operator==(int) is ambiguous under C++20's rewriting rules).
    struct Counted {
        inline static size_t live = 0;

        int value;

        Counted(int v = 0) noexcept
        : value(v) {
            ++live;
        }

        Counted(const Counted& other) noexcept
        : value(other.value) {
            ++live;
        }

        Counted& operator=(const Counted&) = default;

        ~Counted() {
            --live;
        }

        operator int() const noexcept {
            return value;
        }

        bool operator==(const Counted& other) const noexcept {
            return value == other.value;
        }
    };

    struct MoveOnly {
        int value;

        explicit MoveOnly(int v = 0) noexcept
        : value(v) {
        }

        MoveOnly(MoveOnly&& other) noexcept
        : value(std::exchange(other.value, -1)) {
        }

        MoveOnly& operator=(MoveOnly&& other) noexcept {
            value = std::exchange(other.value, -1);
            return *this;
        }

        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
    };

    // Ordered by operator< only: synth-three-way must build the ordering.
    struct OnlyLess {
        int value;

        bool operator<(const OnlyLess& other) const noexcept {
            return value < other.value;
        }
    };

    // An element holding a tracked_ptr: the node's pointer map must trace it.
    struct Holder {
        int key;
        tracked_ptr<Baz> ptr;

        Holder(int k)
        : key(k)
        , ptr(make_tracked<Baz>(k)) {
        }
    };

    // Copies throw on request; `live` counts the instances.
    struct Throwing {
        inline static int live = 0;
        inline static int throw_at = -1;   // copies left before one throws; -1 never

        int value;

        Throwing(int v)
        : value(v) {
            ++live;
        }

        Throwing(const Throwing& other)
        : value(other.value) {
            if (throw_at == 0) {
                throw_at = -1;
                throw std::runtime_error("copy");
            }
            if (throw_at > 0) {
                --throw_at;
            }
            ++live;
        }

        ~Throwing() {
            --live;
        }
    };

    struct ThrowAfter {
        int* left;

        bool operator()(int a, int b) const {
            if (--*left < 0) {
                throw std::runtime_error("compare");
            }
            return a < b;
        }
    };
}

TEST(List_Test, DefaultConstructorEmpty) {
    sgcl::list<int> lst;
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.cbegin(), lst.cend());
    EXPECT_EQ(lst.rbegin(), lst.rend());
    EXPECT_EQ(lst.crbegin(), lst.crend());
    EXPECT_GT(lst.max_size(), 0u);
    for ([[maybe_unused]] int v : lst) {
        FAIL() << "an empty list has no elements";
    }
}

TEST(List_Test, ConstructorNDefault) {
    sgcl::list<Int> lst(3);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_FALSE(lst.empty());
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{0, 0, 0}));
    expect_consistent(lst);
}

TEST(List_Test, ConstructorNValues) {
    sgcl::list<int> lst(4, 3);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{3, 3, 3, 3}));
    expect_consistent(lst);
}

TEST(List_Test, ConstructorInitializerList) {
    sgcl::list<int> lst({4, 5, 6});
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{4, 5, 6}));
    expect_consistent(lst);
}

TEST(List_Test, ConstructorRange) {
    std::vector<int> expected = {1, 2, 3};
    sgcl::list<int> lst(expected.begin(), expected.end());
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(to_vector(lst), expected);
    expect_consistent(lst);
}

// std::istream_iterator is single-pass: the range is read once.
TEST(List_Test, ConstructorInputIteratorRange) {
    std::istringstream in("7 8 9");
    auto first = std::istream_iterator<int>(in);
    auto last = std::istream_iterator<int>();
    sgcl::list<int> lst(first, last);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{7, 8, 9}));
    expect_consistent(lst);
}

TEST(List_Test, CopyConstructor) {
    sgcl::list<int> other({1, 2, 3});
    sgcl::list<int> lst(other);
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(to_vector(other), (std::vector<int>{1, 2, 3}));
    lst.front() = 9;
    EXPECT_EQ(other.front(), 1);
    sgcl::list<int> empty;
    sgcl::list<int> copy(empty);
    EXPECT_TRUE(copy.empty());
    EXPECT_EQ(copy.begin(), copy.end());
}

TEST(List_Test, MoveConstructor) {
    sgcl::list<int> other({4, 5, 6});
    auto it = ++other.begin();
    sgcl::list<int> lst(std::move(other));
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(other.begin(), other.end());
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{4, 5, 6}));
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(std::next(it), --lst.end());
    EXPECT_EQ(std::prev(it), lst.begin());
    other.push_back(1);
    EXPECT_EQ(to_vector(other), (std::vector<int>{1}));
}

TEST(List_Test, CopyAssignment) {
    sgcl::list<int> other({1, 2, 3});
    sgcl::list<int> lst;
    lst = other;
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(reverse_vector(lst), (std::vector<int>{3, 2, 1}));

    other = sgcl::list<int>({2, 3});
    lst = other;
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3}));
    EXPECT_EQ(reverse_vector(lst), (std::vector<int>{3, 2}));

    lst = lst;
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3}));

    other = sgcl::list<int>();
    lst = other;
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.rbegin(), lst.rend());
}

TEST(List_Test, MoveAssignment) {
    sgcl::list<int> other({4, 5, 6});
    sgcl::list<int> lst({1, 2});
    auto it = ++other.begin();
    lst = std::move(other);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{4, 5, 6}));
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(std::next(it, 2), lst.end());
}

TEST(List_Test, InitializerListAssignment) {
    sgcl::list<int> lst;
    lst = {7, 8, 9};
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{7, 8, 9}));
    EXPECT_EQ(reverse_vector(lst), (std::vector<int>{9, 8, 7}));

    lst = {8, 9};
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{8, 9}));
    EXPECT_EQ(reverse_vector(lst), (std::vector<int>{9, 8}));

    lst = std::initializer_list<int>();
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.rbegin(), lst.rend());
}

TEST(List_Test, AssignNValues) {
    sgcl::list<Int> lst({1, 2});
    lst.assign(4, 5);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{5, 5, 5, 5}));
    EXPECT_EQ(reverse_vector(lst), (std::vector<int>{5, 5, 5, 5}));

    lst.assign(2, 3);
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{3, 3}));
    EXPECT_EQ(reverse_vector(lst), (std::vector<int>{3, 3}));

    lst.assign(0, 0);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.rbegin(), lst.rend());
}

TEST(List_Test, AssignRange) {
    sgcl::list<Int> lst;
    std::vector<int> expected = {3, 4, 5};
    lst.assign(expected.begin(), expected.end());
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(reverse_vector(lst), (std::vector<int>{5, 4, 3}));

    expected = {2, 3};
    lst.assign(expected.begin(), expected.end());
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(reverse_vector(lst), (std::vector<int>{3, 2}));

    std::istringstream in("6 7 8 9");
    lst.assign(std::istream_iterator<int>(in), std::istream_iterator<int>());
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{6, 7, 8, 9}));
    expect_consistent(lst);

    lst.assign(expected.begin(), expected.begin());
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.rbegin(), lst.rend());
}

TEST(List_Test, AssignInitializerList) {
    sgcl::list<Int> lst({1, 2});
    lst.assign({3, 4, 5});
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{3, 4, 5}));
    EXPECT_EQ(reverse_vector(lst), (std::vector<int>{5, 4, 3}));

    lst.assign({2, 3});
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3}));

    lst.assign(std::initializer_list<Int>());
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.cbegin(), lst.cend());
    EXPECT_EQ(lst.crbegin(), lst.crend());
}

TEST(List_Test, FrontBack) {
    sgcl::list<int> lst({5, 6, 7});
    const auto& clst = lst;
    EXPECT_EQ(lst.front(), 5);
    EXPECT_EQ(lst.back(), 7);
    EXPECT_EQ(clst.front(), 5);
    EXPECT_EQ(clst.back(), 7);
    static_assert(std::is_same_v<decltype(clst.front()), const int&>);
    static_assert(std::is_same_v<decltype(lst.back()), int&>);
    lst.front() = 1;
    lst.back() = 3;
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 6, 3}));
}

TEST(List_Test, Iterators) {
    sgcl::list<int> lst({1, 2, 3});
    const auto& clst = lst;
    auto it = lst.begin();
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(*++it, 3);
    EXPECT_EQ(++it, lst.end());
    EXPECT_EQ(*--it, 3);
    EXPECT_EQ(*it--, 3);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(*--lst.end(), 3);
    EXPECT_EQ(*lst.rbegin(), 3);
    EXPECT_EQ(*--lst.rend(), 1);
    EXPECT_EQ(*clst.begin(), 1);
    EXPECT_EQ(*--clst.end(), 3);
    EXPECT_EQ(*clst.rbegin(), 3);
    EXPECT_EQ(*lst.crbegin(), 3);
    EXPECT_EQ(std::vector<int>(clst.rbegin(), clst.rend()), (std::vector<int>{3, 2, 1}));
    EXPECT_EQ(std::vector<int>(lst.crbegin(), lst.crend()), (std::vector<int>{3, 2, 1}));

    sgcl::list<int>::const_iterator cit = lst.begin();
    EXPECT_EQ(cit, lst.begin());
    EXPECT_EQ(lst.begin(), cit);
    EXPECT_NE(cit, lst.end());
    EXPECT_EQ(cit, lst.cbegin());
    ++cit;
    EXPECT_EQ(*cit, 2);
    EXPECT_EQ(cit.operator->(), &*cit);
    *lst.begin() = 4;
    EXPECT_EQ(lst.front(), 4);

    sgcl::list<int>::iterator none;
    sgcl::list<int>::iterator other;
    EXPECT_EQ(none, other);

    int sum = 0;
    for (int v : clst) {
        sum += v;
    }
    EXPECT_EQ(sum, 9);
    for (auto& v : lst) {
        v *= 2;
    }
    EXPECT_EQ(to_vector(lst), (std::vector<int>{8, 4, 6}));
}

TEST(List_Test, PushAndEmplaceBack) {
    sgcl::list<Int> lst({2, 3});
    lst.push_back(4);
    Int value(5);
    lst.push_back(value);
    auto& ref = lst.emplace_back(6);
    EXPECT_EQ(ref, 6);
    ref = Int(7);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3, 4, 5, 7}));
    EXPECT_EQ(&lst.back(), &ref);
    expect_consistent(lst);
}

TEST(List_Test, PushAndEmplaceFront) {
    sgcl::list<Int> lst({3, 4});
    lst.push_front(2);
    Int value(1);
    lst.push_front(value);
    auto& ref = lst.emplace_front(0);
    EXPECT_EQ(ref, 0);
    ref = Int(9);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{9, 1, 2, 3, 4}));
    EXPECT_EQ(&lst.front(), &ref);
    expect_consistent(lst);
}

TEST(List_Test, PopBack) {
    sgcl::list<Int> lst({2, 3, 4});
    lst.pop_back();
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3}));
    lst.pop_back();
    lst.pop_back();
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(lst.begin(), lst.end());
    lst.push_back(1);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1}));
}

TEST(List_Test, PopFront) {
    sgcl::list<Int> lst({2, 3, 4});
    lst.pop_front();
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{3, 4}));
    lst.pop_front();
    lst.pop_front();
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(lst.begin(), lst.end());
}

TEST(List_Test, Emplace) {
    sgcl::list<int> lst({2, 3, 5});
    auto it = lst.emplace(lst.begin(), 1);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(it, lst.begin());
    it = lst.emplace(std::next(lst.begin(), 3), 4);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(*it, 4);
    it = lst.emplace(lst.end(), 6);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(*it, 6);
    EXPECT_EQ(it, --lst.end());
    EXPECT_EQ(lst.size(), 6u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 4, 5, 6}));
    expect_consistent(lst);

    sgcl::list<std::pair<int, std::string>> pairs;
    pairs.emplace(pairs.end(), 1, "one");
    pairs.emplace_back(2, "two");
    pairs.emplace_front(0, "zero");
    EXPECT_EQ(pairs.front().second, "zero");
    EXPECT_EQ(pairs.back().second, "two");
    EXPECT_EQ(std::next(pairs.begin())->second, "one");
}

TEST(List_Test, InsertValue) {
    sgcl::list<int> lst({2, 3, 5});
    auto it = lst.insert(lst.begin(), 1);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(*it, 1);
    int four = 4;
    it = lst.insert(std::next(lst.begin(), 3), four);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(*it, 4);
    it = lst.insert(lst.end(), 6);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(lst.size(), 6u);
    EXPECT_EQ(*it, 6);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 4, 5, 6}));
    expect_consistent(lst);
}

TEST(List_Test, InsertNValues) {
    sgcl::list<int> lst({2, 3, 5});
    auto it = lst.insert(lst.begin(), 0, 1);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(it, lst.begin());
    it = lst.insert(lst.begin(), 2, 1);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(it, lst.begin());
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(*++it, 1);
    it = lst.insert(std::next(lst.begin(), 4), 2, 4);
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(lst.size(), 7u);
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(*++it, 4);
    it = lst.insert(lst.end(), 2, 6);
    EXPECT_EQ(collector::get_live_object_count(), 10u);
    EXPECT_EQ(lst.size(), 9u);
    EXPECT_EQ(*it, 6);
    EXPECT_EQ(*++it, 6);
    EXPECT_EQ(++it, lst.end());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 1, 2, 3, 4, 4, 5, 6, 6}));
    expect_consistent(lst);
}

TEST(List_Test, InsertRange) {
    sgcl::list<int> lst({2, 3, 5});
    std::vector<int> vec = {4, 5};
    auto it = lst.insert(lst.end(), vec.begin(), vec.begin());
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(it, lst.end());
    it = lst.insert(lst.begin(), vec.begin(), vec.end());
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(it, lst.begin());
    EXPECT_EQ(*it, 4);
    EXPECT_EQ(*++it, 5);
    vec = std::vector<int>{7, 8};
    it = lst.insert(std::next(lst.begin(), 4), vec.begin(), vec.end());
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(lst.size(), 7u);
    EXPECT_EQ(*it, 7);
    EXPECT_EQ(*++it, 8);
    std::istringstream in("2 3");
    it = lst.insert(lst.end(), std::istream_iterator<int>(in), std::istream_iterator<int>());
    EXPECT_EQ(collector::get_live_object_count(), 10u);
    EXPECT_EQ(lst.size(), 9u);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(*++it, 3);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{4, 5, 2, 3, 7, 8, 5, 2, 3}));
    expect_consistent(lst);
}

TEST(List_Test, InsertInitializerList) {
    sgcl::list<int> lst({2, 3, 5});
    auto it = lst.insert(lst.end(), std::initializer_list<int>());
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(it, lst.end());
    it = lst.insert(lst.begin(), {5, 4});
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(*++it, 4);
    it = lst.insert(std::next(lst.begin(), 4), {8, 7});
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(lst.size(), 7u);
    EXPECT_EQ(*it, 8);
    EXPECT_EQ(*++it, 7);
    it = lst.insert(lst.end(), {3, 2});
    EXPECT_EQ(collector::get_live_object_count(), 10u);
    EXPECT_EQ(lst.size(), 9u);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(*++it, 2);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{5, 4, 2, 3, 8, 7, 5, 3, 2}));
    expect_consistent(lst);
}

// An end() taken from a list that has not allocated its sentinel yet still
// names the end after the list grows.
TEST(List_Test, InsertAtEndTakenWhileEmpty) {
    sgcl::list<int> lst;
    auto end = lst.end();
    lst.push_back(1);
    auto it = lst.insert(end, 2);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2}));
    sgcl::list<int> other;
    other.splice(other.end(), lst);
    EXPECT_EQ(to_vector(other), (std::vector<int>{1, 2}));
    EXPECT_TRUE(lst.empty());
    sgcl::list<int> empty;
    EXPECT_EQ(empty.erase(empty.begin(), empty.end()), empty.end());
    EXPECT_EQ(empty.insert(empty.end(), 0, 1), empty.end());
    EXPECT_EQ(empty.remove(1), 0u);
    EXPECT_EQ(empty.unique(), 0u);
    empty.reverse();
    empty.sort();
    empty.clear();
    EXPECT_EQ(collector::get_live_object_count(), 4u);
}

// The end() of a list that never held an element is a null iterator, and
// the first insertion, which makes the sentinel, invalidates it: take a
// fresh end() after it. Used all the same, it still means the end as a
// position and as the end of a range, and a range that starts at it is
// empty; it no longer compares equal to end().
TEST(List_Test, EndTakenWhileEmptyIsInvalidatedByTheFirstInsertion) {
    sgcl::list<Int> lst;
    auto stale = lst.end();
    lst.push_back(1);
    lst.push_back(2);
    EXPECT_NE(stale, lst.end());                                    // invalidated: the sentinel is end() now
    auto it = framed([&] { return lst.erase(stale, lst.end()); });  // a range from the stale end: empty, nothing erased
    EXPECT_EQ(it, lst.end());
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(lst.size(), 2u);
    it = framed([&] { return lst.erase(stale); });                  // one element at the stale end: none
    EXPECT_EQ(it, stale);
    EXPECT_EQ(lst.size(), 2u);
    sgcl::list<Int> other;
    other.splice(other.end(), lst, stale, lst.end());               // an empty range: nothing moves
    other.splice(other.end(), lst, stale);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(lst.size(), 2u);
    other.splice(other.end(), lst, lst.begin(), stale);             // a range to the stale end: to the end of lst
    EXPECT_EQ(to_vector(other), (std::vector<int>{1, 2}));
    EXPECT_TRUE(lst.empty());
    it = framed([&] { return other.erase(other.begin(), stale); }); // likewise
    EXPECT_EQ(it, other.end());
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(other.empty());
    it = other.insert(stale, 3);                                    // a position: the end
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(to_vector(other), (std::vector<int>{3}));
    expect_consistent(other);
    expect_consistent(lst);
}

TEST(List_Test, Erase) {
    sgcl::list<Int> lst({1, 2, 3, 4, 5});
    auto it = framed([&] { return lst.erase(lst.begin()); });
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(it, lst.begin());
    it = framed([&] { return lst.erase(++lst.begin()); });
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(*it, 4);
    it = framed([&] { return lst.erase(--lst.end()); });
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(it, lst.end());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 4}));
    expect_consistent(lst);
}

TEST(List_Test, EraseRange) {
    sgcl::list<Int> lst({1, 2, 3, 4, 5, 6, 7, 8});
    auto it = framed([&] { return lst.erase(lst.begin(), std::next(lst.begin(), 2)); });
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(lst.size(), 6u);
    EXPECT_EQ(*it, 3);
    it = framed([&] { return lst.erase(++lst.begin(), std::next(lst.begin(), 3)); });
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(*it, 6);
    it = framed([&] { return lst.erase(std::prev(lst.end(), 2), lst.end()); });
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(it, lst.end());
    it = lst.erase(lst.end(), lst.end());
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(it, lst.end());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{3, 6}));
    expect_consistent(lst);
    it = lst.erase(lst.begin(), lst.end());
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(it, lst.end());
    EXPECT_EQ(lst.begin(), lst.end());
}

TEST(List_Test, Resize) {
    sgcl::list<Int> lst({1, 2, 3});
    lst.resize(5);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 0, 0}));
    lst.resize(2);
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2}));
    lst.resize(4, 8);
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 8, 8}));
    lst.resize(4, 9);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 8, 8}));
    lst.resize(3, 9);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 8}));
    expect_consistent(lst);
    lst.resize(0);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(lst.begin(), lst.end());
}

TEST(List_Test, Clear) {
    sgcl::list<Int> lst({5, 6, 7});
    auto end = lst.end();
    lst.clear();
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(lst.size(), 0u);
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.cbegin(), lst.cend());
    EXPECT_EQ(lst.rbegin(), lst.rend());
    EXPECT_EQ(lst.crbegin(), lst.crend());
    EXPECT_EQ(end, lst.end());
    lst.push_back(1);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1}));
    lst.clear();
    lst.clear();
    EXPECT_TRUE(lst.empty());
}

TEST(List_Test, DestructorDestroysElements) {
    {
        sgcl::list<Int> lst({1, 2, 3});
        EXPECT_EQ(Int::counter, 3u);
    }
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

TEST(List_Test, Swap) {
    sgcl::list<int> lst1({1, 2});
    sgcl::list<int> lst2({4, 5, 6, 7});
    auto it1 = lst1.begin();
    auto it2 = ++lst2.begin();
    lst1.swap(lst2);
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(lst1.size(), 4u);
    EXPECT_EQ(lst2.size(), 2u);
    EXPECT_EQ(to_vector(lst1), (std::vector<int>{4, 5, 6, 7}));
    EXPECT_EQ(to_vector(lst2), (std::vector<int>{1, 2}));
    EXPECT_EQ(*it1, 1);
    EXPECT_EQ(it1, lst2.begin());
    EXPECT_EQ(*it2, 5);
    EXPECT_EQ(std::prev(it2), lst1.begin());
    swap(lst1, lst2);
    EXPECT_EQ(to_vector(lst1), (std::vector<int>{1, 2}));
    EXPECT_EQ(to_vector(lst2), (std::vector<int>{4, 5, 6, 7}));
    sgcl::list<int> empty;
    lst1.swap(empty);
    EXPECT_TRUE(lst1.empty());
    EXPECT_EQ(lst1.begin(), lst1.end());
    EXPECT_EQ(to_vector(empty), (std::vector<int>{1, 2}));
}

TEST(List_Test, Merge) {
    sgcl::list<int> lst1({1, 6, 8});
    sgcl::list<int> lst2({4, 5, 6, 7});
    auto it = ++lst2.begin();
    lst1.merge(lst2);
    EXPECT_EQ(collector::get_live_object_count(), 9u);
    EXPECT_EQ(lst1.size(), 7u);
    EXPECT_EQ(lst2.size(), 0u);
    EXPECT_TRUE(lst2.empty());
    EXPECT_EQ(lst2.begin(), lst2.end());
    EXPECT_EQ(to_vector(lst1), (std::vector<int>{1, 4, 5, 6, 6, 7, 8}));
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(std::next(it, 5), lst1.end());
    expect_consistent(lst1);
    expect_consistent(lst2);

    lst1.merge(sgcl::list<int>({0, 9}));
    EXPECT_EQ(to_vector(lst1), (std::vector<int>{0, 1, 4, 5, 6, 6, 7, 8, 9}));
    lst1.merge(lst2);
    EXPECT_EQ(lst1.size(), 9u);
    lst1.merge(lst1);
    EXPECT_EQ(lst1.size(), 9u);
    lst2.merge(lst1);
    EXPECT_TRUE(lst1.empty());
    EXPECT_EQ(to_vector(lst2), (std::vector<int>{0, 1, 4, 5, 6, 6, 7, 8, 9}));
    expect_consistent(lst2);
}

TEST(List_Test, MergeCompare) {
    sgcl::list<int> lst1({8, 6, 1});
    sgcl::list<int> lst2({7, 6, 5, 4});
    lst1.merge(lst2, std::greater<>());
    EXPECT_EQ(to_vector(lst1), (std::vector<int>{8, 7, 6, 6, 5, 4, 1}));
    EXPECT_TRUE(lst2.empty());
    lst1.merge(sgcl::list<int>({9, 0}), std::greater<>());
    EXPECT_EQ(to_vector(lst1), (std::vector<int>{9, 8, 7, 6, 6, 5, 4, 1, 0}));
    expect_consistent(lst1);
}

// Of equal keys, the elements of the merged-into list come first.
TEST(List_Test, MergeStable) {
    using P = std::pair<int, int>;
    auto by_key = [](const P& a, const P& b) { return a.first < b.first; };
    sgcl::list<P> lst1({{1, 0}, {2, 1}, {2, 2}, {4, 3}});
    sgcl::list<P> lst2({{2, 10}, {3, 11}, {4, 12}});
    lst1.merge(lst2, by_key);
    EXPECT_EQ(to_vector<P>(lst1), (std::vector<P>{{1, 0}, {2, 1}, {2, 2}, {2, 10}, {3, 11}, {4, 3}, {4, 12}}));
}

TEST(List_Test, MergeThrowingCompare) {
    sgcl::list<int> lst1({1, 3, 5, 7, 9});
    sgcl::list<int> lst2({2, 4, 6, 8, 10});
    int left = 3;
    EXPECT_THROW(lst1.merge(lst2, ThrowAfter{&left}), std::runtime_error);
    EXPECT_EQ(lst1.size() + lst2.size(), 10u);
    expect_consistent(lst1);
    expect_consistent(lst2);
    auto all = to_vector(lst1);
    auto rest = to_vector(lst2);
    all.insert(all.end(), rest.begin(), rest.end());
    std::sort(all.begin(), all.end());
    EXPECT_EQ(all, (std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
    lst1.merge(lst2);
    EXPECT_EQ(to_vector(lst1), (std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
    EXPECT_TRUE(lst2.empty());
}

TEST(List_Test, SpliceWholeList) {
    sgcl::list<int> lst({1, 2, 5});
    sgcl::list<int> other({3, 4});
    auto it = other.begin();
    auto last = --lst.end();
    lst.splice(last, other);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(other.begin(), other.end());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 4, 5}));
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(std::next(it, 2), last);
    expect_consistent(lst);
    expect_consistent(other);

    lst.splice(lst.begin(), sgcl::list<int>({0}));
    EXPECT_EQ(to_vector(lst), (std::vector<int>{0, 1, 2, 3, 4, 5}));
    lst.splice(lst.end(), other);
    EXPECT_EQ(lst.size(), 6u);
    other.push_back(6);
    lst.splice(lst.end(), other);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{0, 1, 2, 3, 4, 5, 6}));
    EXPECT_TRUE(other.empty());
    other.push_back(7);
    EXPECT_EQ(to_vector(other), (std::vector<int>{7}));
}

TEST(List_Test, SpliceOne) {
    sgcl::list<int> lst({1, 3});
    sgcl::list<int> other({2, 4, 5});
    auto it = other.begin();
    lst.splice(++lst.begin(), other, it);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(to_vector(other), (std::vector<int>{4, 5}));
    EXPECT_EQ(lst.size(), 3u);
    EXPECT_EQ(other.size(), 2u);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(std::prev(it), lst.begin());
    sgcl::list<int> six({6});
    lst.splice(lst.end(), std::move(six), six.begin());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 6}));
    EXPECT_TRUE(six.empty());
    lst.splice(lst.begin(), other, other.end());
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(other.size(), 2u);
    expect_consistent(lst);
    expect_consistent(other);
}

TEST(List_Test, SpliceRange) {
    sgcl::list<int> lst({1, 5});
    sgcl::list<int> other({0, 2, 3, 4, 6});
    auto first = ++other.begin();
    auto last = std::next(first, 3);
    lst.splice(--lst.end(), other, first, last);
    EXPECT_EQ(collector::get_live_object_count(), 9u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 4, 5}));
    EXPECT_EQ(to_vector(other), (std::vector<int>{0, 6}));
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(other.size(), 2u);
    EXPECT_EQ(*first, 2);
    EXPECT_EQ(*last, 6);
    EXPECT_EQ(std::next(last), other.end());
    expect_consistent(lst);
    expect_consistent(other);
    lst.splice(lst.begin(), other, other.begin(), other.begin());
    EXPECT_EQ(lst.size(), 5u);
    lst.splice(lst.end(), other, other.begin(), other.end());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 4, 5, 0, 6}));
    EXPECT_TRUE(other.empty());
    sgcl::list<int> temp({7, 8, 9});
    lst.splice(lst.begin(), std::move(temp), temp.begin(), --temp.end());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{7, 8, 1, 2, 3, 4, 5, 0, 6}));
    EXPECT_EQ(to_vector(temp), (std::vector<int>{9}));
    expect_consistent(lst);
    expect_consistent(temp);
}

TEST(List_Test, SpliceWithinSameList) {
    sgcl::list<int> lst({1, 2, 3, 4, 5});
    auto it = --lst.end();
    lst.splice(lst.begin(), lst, it);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{5, 1, 2, 3, 4}));
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(it, lst.begin());
    lst.splice(lst.end(), lst, lst.begin());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 4, 5}));
    lst.splice(lst.begin(), lst, lst.begin());
    lst.splice(++lst.begin(), lst, lst.begin());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 4, 5}));
    auto first = ++lst.begin();
    auto last = std::next(first, 2);
    lst.splice(lst.end(), lst, first, last);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 4, 5, 2, 3}));
    EXPECT_EQ(*first, 2);
    EXPECT_EQ(std::next(first, 2), lst.end());
    lst.splice(lst.begin(), lst, first, lst.end());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3, 1, 4, 5}));
    lst.splice(lst.begin(), lst, lst.begin(), lst.end());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3, 1, 4, 5}));
    lst.splice(std::next(lst.begin(), 2), lst, lst.begin(), std::next(lst.begin(), 2));
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3, 1, 4, 5}));
    lst.splice(std::next(lst.begin(), 3), lst, lst.begin(), std::next(lst.begin(), 2));
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 4, 5}));
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    expect_consistent(lst);
}

TEST(List_Test, Remove) {
    sgcl::list<Counted> lst({5, 1, 5, 7, 5});
    auto count = lst.remove(5);
    EXPECT_EQ(Counted::live, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 7}));
    EXPECT_EQ(lst.remove(4), 0u);
    expect_consistent(lst);
}

// The value may be an element of the list; it is read for every comparison
// and removed last.
TEST(List_Test, RemoveOwnElement) {
    sgcl::list<std::string> lst({"aaa", "bbb", "aaa", "ccc", "aaa"});
    EXPECT_EQ(lst.remove(lst.front()), 3u);
    EXPECT_EQ(to_vector<std::string>(lst), (std::vector<std::string>{"bbb", "ccc"}));
    EXPECT_EQ(lst.remove(lst.back()), 1u);
    EXPECT_EQ(to_vector<std::string>(lst), (std::vector<std::string>{"bbb"}));
    expect_consistent(lst);
}

TEST(List_Test, RemoveIf) {
    sgcl::list<Int> lst({1, 2, 3, 4, 5});
    auto count = lst.remove_if([](int v) { return v % 2 == 1; });
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(lst.size(), 2u);
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 4}));
    EXPECT_EQ(lst.remove_if([](int) { return true; }), 2u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(Int::counter, 0u);
    expect_consistent(lst);
}

TEST(List_Test, StdErase) {
    sgcl::list<int> lst({1, 2, 3, 2, 4});
    EXPECT_EQ(std::erase(lst, 2), 2u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 3, 4}));
    EXPECT_EQ(std::erase_if(lst, [](int v) { return v > 2; }), 2u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1}));
    EXPECT_EQ(std::erase(lst, 9), 0u);
    EXPECT_EQ(erase_if(lst, [](int v) { return v == 1; }), 1u);   // unqualified: found in sgcl by argument-dependent lookup
    EXPECT_EQ(erase(lst, 1), 0u);
    expect_consistent(lst);
}

TEST(List_Test, Reverse) {
    sgcl::list<int> lst({1, 2, 3, 4, 5});
    auto it = ++lst.begin();
    lst.reverse();
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{5, 4, 3, 2, 1}));
    EXPECT_EQ(reverse_vector(lst), (std::vector<int>{1, 2, 3, 4, 5}));
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(*std::next(it), 1);
    EXPECT_EQ(std::next(it, 2), lst.end());
    expect_consistent(lst);
    lst.reverse();
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 4, 5}));
    sgcl::list<int> one({1});
    one.reverse();
    EXPECT_EQ(to_vector(one), (std::vector<int>{1}));
}

TEST(List_Test, Unique) {
    sgcl::list<Counted> lst({1, 2, 2, 3, 3, 3, 2, 1, 1, 2});
    auto count = lst.unique();
    EXPECT_EQ(Counted::live, 6u);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(lst.size(), 6u);
    EXPECT_EQ(count, 4u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 2, 1, 2}));
    EXPECT_EQ(lst.unique(), 0u);
    expect_consistent(lst);
    sgcl::list<int> one({1});
    EXPECT_EQ(one.unique(), 0u);
}

TEST(List_Test, UniquePredicate) {
    sgcl::list<int> lst({1, 2, 2, 3, 3, 3, 5, 1, 1, 2});
    auto count = lst.unique([](int a, int b) { return a % 2 == b % 2; });
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(lst.size(), 4u);
    EXPECT_EQ(count, 6u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 2}));
    expect_consistent(lst);
}

TEST(List_Test, Sort) {
    sgcl::list<int> lst({5, 1, 2, 7, 3});
    auto it = lst.begin();
    lst.sort();
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(lst.size(), 5u);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 5, 7}));
    EXPECT_EQ(reverse_vector(lst), (std::vector<int>{7, 5, 3, 2, 1}));
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(*std::next(it), 7);
    EXPECT_EQ(*std::prev(it), 3);
    expect_consistent(lst);
    lst.sort(std::greater<>());
    EXPECT_EQ(to_vector(lst), (std::vector<int>{7, 5, 3, 2, 1}));
    lst.sort();
    lst.sort();
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 5, 7}));
    sgcl::list<int> one({1});
    one.sort();
    EXPECT_EQ(to_vector(one), (std::vector<int>{1}));
    sgcl::list<int> two({2, 1});
    two.sort();
    EXPECT_EQ(to_vector(two), (std::vector<int>{1, 2}));
    expect_consistent(two);

    std::mt19937 rng(7);
    sgcl::list<int> big;
    std::vector<int> expected;
    for (int i = 0; i < 1000; ++i) {
        int v = int(rng() % 100);
        big.push_back(v);
        expected.push_back(v);
    }
    big.sort();
    std::sort(expected.begin(), expected.end());
    EXPECT_EQ(to_vector(big), expected);
    expect_consistent(big);
}

TEST(List_Test, SortStable) {
    using P = std::pair<int, int>;
    sgcl::list<P> lst({{3, 0}, {1, 1}, {3, 2}, {2, 3}, {1, 4}, {3, 5}, {2, 6}, {1, 7}});
    lst.sort([](const P& a, const P& b) { return a.first < b.first; });
    EXPECT_EQ(to_vector<P>(lst), (std::vector<P>{{1, 1}, {1, 4}, {1, 7}, {2, 3}, {2, 6}, {3, 0}, {3, 2}, {3, 5}}));
    expect_consistent(lst);

    std::mt19937 rng(11);
    sgcl::list<P> big;
    std::vector<P> expected;
    for (int i = 0; i < 500; ++i) {
        P p(int(rng() % 10), i);
        big.push_back(p);
        expected.push_back(p);
    }
    big.sort([](const P& a, const P& b) { return a.first < b.first; });
    std::stable_sort(expected.begin(), expected.end(), [](const P& a, const P& b) { return a.first < b.first; });
    EXPECT_EQ(to_vector<P>(big), expected);
}

// A comparator that throws leaves a valid list of the same elements.
TEST(List_Test, SortThrowingCompare) {
    std::mt19937 rng(3);
    sgcl::list<int> lst;
    std::vector<int> expected;
    for (int i = 0; i < 200; ++i) {
        int v = int(rng() % 1000);
        lst.push_back(v);
        expected.push_back(v);
    }
    for (int budget : {0, 1, 50, 400}) {
        int left = budget;
        EXPECT_THROW(lst.sort(ThrowAfter{&left}), std::runtime_error);
        EXPECT_EQ(lst.size(), 200u);
        expect_consistent(lst);
        auto all = to_vector(lst);
        std::sort(all.begin(), all.end());
        std::sort(expected.begin(), expected.end());
        EXPECT_EQ(all, expected);
    }
    lst.sort();
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(collector::get_live_object_count(), 201u);
}

TEST(List_Test, Compare) {
    sgcl::list<int> lst1({1, 2, 3});
    sgcl::list<int> lst2({1, 2, 3});
    EXPECT_EQ(lst1, lst2);
    EXPECT_TRUE(lst1 == lst2);
    EXPECT_FALSE(lst1 != lst2);
    EXPECT_TRUE(lst1 <= lst2);
    EXPECT_TRUE(lst1 >= lst2);
    EXPECT_FALSE(lst1 < lst2);
    EXPECT_TRUE((lst1 <=> lst2) == 0);
    lst2 = {3, 2, 1};
    EXPECT_NE(lst1, lst2);
    EXPECT_TRUE(lst1 < lst2);
    EXPECT_TRUE(lst2 > lst1);

    sgcl::list<int> shorter({1, 2});
    EXPECT_NE(lst1, shorter);
    EXPECT_TRUE(shorter < lst1);
    EXPECT_TRUE(shorter <= lst1);
    EXPECT_TRUE(lst1 > shorter);
    EXPECT_FALSE(lst1 < shorter);
    sgcl::list<int> bigger_shorter({1, 3});
    EXPECT_TRUE(lst1 < bigger_shorter);
    EXPECT_TRUE(bigger_shorter > lst1);
    sgcl::list<int> empty;
    EXPECT_TRUE(empty < shorter);
    EXPECT_TRUE(empty == sgcl::list<int>());
    EXPECT_FALSE(empty < sgcl::list<int>());
    EXPECT_NE(empty, shorter);

    sgcl::list<std::string> s1({"a", "b"});
    sgcl::list<std::string> s2({"a", "c"});
    EXPECT_TRUE(s1 < s2);
    EXPECT_TRUE(s1 != s2);
    s2.pop_back();
    EXPECT_TRUE(s1 > s2);
    s2.push_back("b");
    EXPECT_TRUE(s1 == s2);
}

TEST(List_Test, CompareOnlyLess) {
    sgcl::list<OnlyLess> lst1({{1}, {2}});
    sgcl::list<OnlyLess> lst2({{1}, {3}});
    sgcl::list<OnlyLess> lst3({{1}, {2}, {0}});
    static_assert(std::is_same_v<decltype(lst1 <=> lst2), std::weak_ordering>);
    EXPECT_TRUE(lst1 < lst2);
    EXPECT_TRUE(lst2 > lst1);
    EXPECT_TRUE(lst1 < lst3);
    EXPECT_TRUE(lst3 > lst1);
    EXPECT_TRUE(lst3 < lst2);
    EXPECT_TRUE((lst1 <=> lst1) == 0);
    EXPECT_TRUE(lst1 <= lst1);
    EXPECT_FALSE(lst1 < lst1);
}

// The element checks run in frames of their own: a reference to an element
// left in the test's frame would keep that node alive across the counts.
TEST(List_Test, Strings) {
    sgcl::list<std::string> lst({"delta", "alpha", "charlie"});
    sgcl::list<std::string> other({"one", "two"});
    off_frame([&] {
        lst.push_back("echo");
        lst.emplace_back(3, 'x');
        lst.push_front(std::string("bravo"));
        lst.insert(++lst.begin(), "alpha");
    });
    EXPECT_EQ(lst.size(), 7u);
    EXPECT_EQ(collector::get_live_object_count(), 11u);
    off_frame([&] {
        lst.sort();
        EXPECT_EQ(to_vector<std::string>(lst), (std::vector<std::string>{"alpha", "alpha", "bravo", "charlie", "delta", "echo", "xxx"}));
        EXPECT_EQ(lst.unique(), 1u);
        EXPECT_EQ(lst.remove("xxx"), 1u);
        EXPECT_EQ(to_vector<std::string>(lst), (std::vector<std::string>{"alpha", "bravo", "charlie", "delta", "echo"}));
        lst.erase(++lst.begin(), --lst.end());
        EXPECT_EQ(to_vector<std::string>(lst), (std::vector<std::string>{"alpha", "echo"}));
        lst.front() += "!";
        lst.back().clear();
        EXPECT_EQ(lst.front(), "alpha!");
        EXPECT_TRUE(lst.back().empty());
    });
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    off_frame([&] {
        lst.splice(lst.begin(), other);
        lst.merge(sgcl::list<std::string>({"aaa", "zzz"}));
        EXPECT_EQ(lst.size(), 6u);
        lst.reverse();
        EXPECT_EQ(lst.front(), "zzz");
        lst.resize(2);
        EXPECT_EQ(to_vector<std::string>(lst), (std::vector<std::string>{"zzz", ""}));
        lst.resize(3, "pad");
        EXPECT_EQ(lst.back(), "pad");
        lst.assign(2, "same");
        EXPECT_EQ(to_vector<std::string>(lst), (std::vector<std::string>{"same", "same"}));
        lst = {"x"};
        EXPECT_EQ(to_vector<std::string>(lst), (std::vector<std::string>{"x"}));
        lst.clear();
    });
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    expect_consistent(lst);
}

TEST(List_Test, MoveOnlyElements) {
    sgcl::list<MoveOnly> lst;
    sgcl::list<MoveOnly> other;
    sgcl::list<MoveOnly> counted(2);
    off_frame([&] {
        lst.push_back(MoveOnly(1));
        lst.emplace_back(2);
        MoveOnly three(3);
        lst.push_back(std::move(three));
        EXPECT_EQ(three.value, -1);
        lst.emplace_front(0);
        lst.push_front(MoveOnly(-1));
        lst.insert(lst.end(), MoveOnly(4));
        lst.emplace(lst.begin(), -2);
        lst.resize(8);
        EXPECT_EQ(lst.size(), 8u);
        EXPECT_EQ(lst.back().value, 0);
        lst.pop_back();
        lst.pop_front();
        auto values = [&] {
            std::vector<int> result;
            for (const auto& m : lst) {
                result.push_back(m.value);
            }
            return result;
        };
        EXPECT_EQ(values(), (std::vector<int>{-1, 0, 1, 2, 3, 4}));
        lst.sort([](const MoveOnly& a, const MoveOnly& b) { return a.value > b.value; });
        EXPECT_EQ(values(), (std::vector<int>{4, 3, 2, 1, 0, -1}));
        lst.reverse();
        EXPECT_EQ(lst.remove_if([](const MoveOnly& m) { return m.value < 0; }), 1u);
        EXPECT_EQ(lst.unique([](const MoveOnly&, const MoveOnly&) { return false; }), 0u);
        lst.erase(lst.begin());
        EXPECT_EQ(values(), (std::vector<int>{1, 2, 3, 4}));
        other = std::move(lst);
        EXPECT_TRUE(lst.empty());
        EXPECT_EQ(other.size(), 4u);
        lst = std::move(other);
        EXPECT_EQ(lst.size(), 4u);
        EXPECT_EQ(counted.size(), 2u);
        EXPECT_EQ(counted.front().value, 0);
        lst.splice(lst.end(), counted);
        lst.merge(sgcl::list<MoveOnly>(1), [](const MoveOnly& a, const MoveOnly& b) { return a.value < b.value; });
        EXPECT_EQ(values(), (std::vector<int>{0, 1, 2, 3, 4, 0, 0}));
        expect_consistent(lst);
        lst.swap(other);
        EXPECT_TRUE(lst.empty());
        EXPECT_EQ(other.size(), 7u);
    });
    // other holds the seven nodes and the sentinel, counted its sentinel;
    // lst gave its sentinel away in the last swap
    EXPECT_EQ(collector::get_live_object_count(), 9u);
    other.clear();
    EXPECT_EQ(collector::get_live_object_count(), 2u);
}

// The nodes are traced: what the elements point to survives collections,
// and dies with the elements.
TEST(List_Test, TrackedElements) {
    sgcl::list<tracked_ptr<Baz>> lst;
    for (int i = 0; i < 10; ++i) {
        lst.push_back(make_tracked<Baz>(i));
    }
    sgcl::list<Holder> holders;
    for (int i = 0; i < 5; ++i) {
        holders.emplace_back(i);
    }
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), 21u + 11u);
    off_frame([&] {
        int i = 0;
        for (const auto& p : lst) {
            EXPECT_EQ(p->value, i++);
        }
        i = 0;
        for (const auto& h : holders) {
            EXPECT_EQ(h.key, i);
            EXPECT_EQ(h.ptr->value, i++);
        }
        lst.sort([](const tracked_ptr<Baz>& a, const tracked_ptr<Baz>& b) { return a->value > b->value; });
        EXPECT_EQ(lst.front()->value, 9);
        lst.erase(lst.begin());
        holders.pop_front();
    });
    EXPECT_EQ(collector::get_live_object_count(), 19u + 9u);
    off_frame([&] {
        EXPECT_EQ(lst.front()->value, 8);
        EXPECT_EQ(holders.front().ptr->value, 1);
        lst.clear();
        holders.clear();
    });
    EXPECT_EQ(collector::get_live_object_count(), 2u);
}

// A list inside a managed object: the node's slot holds the sentinel
// pointer of the inner list.
TEST(List_Test, ListOfLists) {
    sgcl::list<sgcl::list<int>> outer;
    sgcl::list<sgcl::list<int>> copy;
    off_frame([&] {
        outer.push_back({1, 2});
        outer.emplace_back(3, 7);
        outer.front().push_back(3);
        EXPECT_EQ(to_vector(outer.front()), (std::vector<int>{1, 2, 3}));
        EXPECT_EQ(to_vector(outer.back()), (std::vector<int>{7, 7, 7}));
    });
    EXPECT_EQ(collector::get_live_object_count(), 11u);
    off_frame([&] {
        copy = outer;
        EXPECT_EQ(copy, outer);
        copy.front().pop_back();
        EXPECT_NE(copy, outer);
        EXPECT_EQ(outer.front().size(), 3u);
        outer.pop_front();
    });
    // copy: sentinel, 2 nodes, inner lists of 2 and 3 (10); outer: sentinel,
    // 1 node, inner list of 3 (6)
    EXPECT_EQ(collector::get_live_object_count(), 10u + 6u);
    off_frame([&] {
        outer.clear();
        copy.clear();
    });
    EXPECT_EQ(collector::get_live_object_count(), 2u);
}

TEST(List_Test, IteratorsFollowElements) {
    sgcl::list<int> lst({1, 2, 3, 4, 5, 6});
    auto two = std::next(lst.begin());
    auto four = std::next(lst.begin(), 3);
    auto six = --lst.end();
    lst.erase(lst.begin());
    lst.erase(std::next(lst.begin()));
    lst.erase(std::next(four));
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 4, 6}));
    EXPECT_EQ(*two, 2);
    EXPECT_EQ(*four, 4);
    EXPECT_EQ(*six, 6);
    EXPECT_EQ(std::next(two), four);
    EXPECT_EQ(std::next(four), six);
    EXPECT_EQ(std::next(six), lst.end());
    lst.insert(four, 3);
    lst.push_front(1);
    EXPECT_EQ(std::next(two, 2), four);
    lst.reverse();
    EXPECT_EQ(*std::next(four), 3);
    EXPECT_EQ(std::prev(four), six);
    lst.sort();
    EXPECT_EQ(to_vector(lst), (std::vector<int>{1, 2, 3, 4, 6}));
    EXPECT_EQ(*std::next(four), 6);
    EXPECT_EQ(std::next(six), lst.end());
    sgcl::list<int> other({5, 7});
    auto five = other.begin();
    lst.merge(other);
    EXPECT_EQ(std::prev(five), four);
    EXPECT_EQ(std::next(five), six);
    EXPECT_EQ(*std::next(six), 7);
    sgcl::list<int> target;
    target.splice(target.end(), lst, four, lst.end());
    EXPECT_EQ(to_vector(target), (std::vector<int>{4, 5, 6, 7}));
    EXPECT_EQ(four, target.begin());
    EXPECT_EQ(std::next(six, 2), target.end());
    EXPECT_EQ(std::next(two, 2), lst.end());
    sgcl::list<int> moved(std::move(target));
    EXPECT_EQ(four, moved.begin());
    EXPECT_EQ(std::next(five), six);
    lst.swap(moved);
    EXPECT_EQ(four, lst.begin());
    EXPECT_EQ(std::next(two, 2), moved.end());
    expect_consistent(lst);
    expect_consistent(moved);
}

// Iterators are raw node pointers: they may sit in a std container or a
// lambda's capture, and stay valid while their element stays in the list.
TEST(List_Test, IteratorsInStdVector) {
    sgcl::list<int> lst({1, 2, 3, 4, 5});
    std::vector<sgcl::list<int>::iterator> its;
    for (auto it = lst.begin(); it != lst.end(); ++it) {
        its.push_back(it);
    }
    ASSERT_EQ(its.size(), 5u);
    for (size_t i = 0; i < its.size(); ++i) {
        EXPECT_EQ(*its[i], int(i + 1));
    }
    std::sort(its.begin(), its.end(), [](auto a, auto b) { return *a > *b; });
    EXPECT_EQ(*its.front(), 5);
    EXPECT_EQ(*its.back(), 1);
    lst.erase(its[4]);   // 1
    lst.erase(its[2]);   // 3
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 4, 5}));
    EXPECT_EQ(std::next(its[3]), its[1]);
    EXPECT_EQ(std::prev(its[0]), its[1]);
    auto value_of = [it = its[3]] { return *it; };   // captured by value
    EXPECT_EQ(value_of(), 2);
    std::vector<sgcl::list<int>::const_iterator> cits(its.begin(), its.begin() + 2);
    EXPECT_EQ(*cits[0], 5);
    EXPECT_EQ(*cits[1], 4);
    lst.insert(cits[1], 3);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3, 4, 5}));
    std::vector<sgcl::list<int>::reverse_iterator> rits(1, lst.rbegin());
    EXPECT_EQ(*rits[0], 5);
    expect_consistent(lst);
}

// A raw iterator kept across a collection stays valid while its element
// stays in the list: the list roots the node. The nodes erased around it
// are collected: the vector's words are not roots.
TEST(List_Test, IteratorAcrossCollection) {
    sgcl::list<Int> lst({1, 2, 3, 4});
    std::vector<sgcl::list<Int>::iterator> kept;
    off_frame([&] {
        for (auto it = lst.begin(); it != lst.end(); ++it) {
            kept.push_back(it);
        }
    });
    auto it = kept[2];
    off_frame([&] {
        lst.pop_front();
        lst.pop_back();
    });
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(*kept[1], 2);
    EXPECT_EQ(std::next(kept[1]), it);
    EXPECT_EQ(std::prev(it), kept[1]);
    EXPECT_EQ(std::next(it), lst.end());
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);   // the sentinel and two nodes
    lst.insert(it, 9);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 9, 3}));
    EXPECT_EQ(*std::prev(it), 9);
    expect_consistent(lst);
}

TEST(List_Test, Ranges) {
    sgcl::list<int> lst({1, 2, 3, 4, 5});
    const auto& clst = lst;
    auto it = std::ranges::find(lst, 3);
    EXPECT_NE(it, lst.end());
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(std::ranges::find(clst, 9), clst.end());
    EXPECT_EQ(std::ranges::distance(lst), 5);
    EXPECT_EQ(std::ranges::distance(lst.begin(), it), 2);
    EXPECT_EQ(std::ranges::count_if(lst, [](int v) { return v % 2 == 0; }), 2);
    EXPECT_EQ(std::ranges::size(clst), 5u);
    EXPECT_TRUE(std::ranges::equal(lst | std::views::reverse, std::vector<int>{5, 4, 3, 2, 1}));
    EXPECT_TRUE(std::ranges::is_sorted(lst));
    std::ranges::for_each(lst, [](int& v) { v *= 10; });
    EXPECT_EQ(to_vector(lst), (std::vector<int>{10, 20, 30, 40, 50}));
    auto found = std::ranges::find_if(lst, [](int v) { return v > 25; });
    lst.erase(lst.begin(), found);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{30, 40, 50}));
    std::vector<int> reversed(lst.rbegin(), lst.rend());
    EXPECT_EQ(reversed, (std::vector<int>{50, 40, 30}));
    EXPECT_EQ(std::ranges::max(lst), 50);
    EXPECT_EQ(std::distance(lst.begin(), lst.end()), 3);
    EXPECT_EQ(std::find(lst.begin(), lst.end(), 40), std::next(lst.begin()));
}

// A copy that throws leaves the list as it was and destroys the copies made.
TEST(List_Test, ThrowingCopy) {
    Throwing::throw_at = -1;
    sgcl::list<Throwing> lst({1, 2, 3});
    std::vector<Throwing> source({4, 5, 6});
    int live = Throwing::live;
    EXPECT_EQ(live, 6);
    Throwing::throw_at = 1;
    EXPECT_THROW(lst.insert(lst.begin(), source.begin(), source.end()), std::runtime_error);
    EXPECT_EQ(Throwing::live, live);
    EXPECT_EQ(lst.size(), 3u);
    expect_consistent(lst);
    Throwing::throw_at = 2;
    EXPECT_THROW(lst.insert(lst.end(), 5, source.front()), std::runtime_error);
    EXPECT_EQ(Throwing::live, live);
    EXPECT_EQ(lst.size(), 3u);
    Throwing::throw_at = 0;
    EXPECT_THROW(lst.push_back(source.front()), std::runtime_error);
    EXPECT_EQ(Throwing::live, live);
    EXPECT_EQ(lst.size(), 3u);
    Throwing::throw_at = 2;
    EXPECT_THROW(sgcl::list<Throwing> copy(lst), std::runtime_error);
    EXPECT_EQ(Throwing::live, live);
    Throwing::throw_at = 1;
    EXPECT_THROW(lst.resize(6, source.front()), std::runtime_error);
    EXPECT_EQ(Throwing::live, live);
    EXPECT_EQ(lst.size(), 3u);
    std::vector<int> values;
    for (const auto& t : lst) {
        values.push_back(t.value);
    }
    EXPECT_EQ(values, (std::vector<int>{1, 2, 3}));
    Throwing::throw_at = -1;
    lst.insert(lst.end(), source.begin(), source.end());
    EXPECT_EQ(lst.size(), 6u);
    EXPECT_EQ(Throwing::live, live + 3);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    lst.clear();
    EXPECT_EQ(Throwing::live, 3);
}

namespace {
    // 100k operations mirrored on a std::list while the collector runs.
    // Runs below the test's frame: the element references the checks take
    // must not survive into the counts.
    void stress(sgcl::list<int>& lst, std::list<int>& model) {
        std::mt19937 rng(42);
        for (int i = 0; i < 100000; ++i) {
            int action = int(rng() % 100);
            if (lst.empty() || action < 40) {
                lst.push_back(i);
                model.push_back(i);
            } else if (action < 55) {
                lst.push_front(i);
                model.push_front(i);
            } else if (action < 65) {
                size_t offset = rng() % std::min<size_t>(lst.size(), 32);
                auto it = lst.insert(std::next(lst.begin(), offset), i);
                model.insert(std::next(model.begin(), offset), i);
                EXPECT_EQ(*it, i);
            } else if (action < 80) {
                lst.pop_front();
                model.pop_front();
            } else if (action < 90) {
                lst.pop_back();
                model.pop_back();
            } else if (action < 97) {
                size_t offset = rng() % std::min<size_t>(lst.size(), 32);
                auto it = lst.erase(std::next(lst.begin(), offset));
                auto mit = model.erase(std::next(model.begin(), offset));
                EXPECT_EQ(it == lst.end(), mit == model.end());
                if (mit != model.end()) {
                    EXPECT_EQ(*it, *mit);
                }
            } else if (action < 99) {
                size_t offset = rng() % std::min<size_t>(lst.size(), 32);
                size_t count = std::min<size_t>(lst.size() - offset, 8);
                auto first = std::next(lst.begin(), offset);
                lst.erase(first, std::next(first, count));
                auto mfirst = std::next(model.begin(), offset);
                model.erase(mfirst, std::next(mfirst, count));
            } else {
                lst.reverse();
                model.reverse();
            }
            EXPECT_EQ(lst.size(), model.size());
            if (i % 500 == 0) {
                collector::force_collect();
            }
            if (i % 20000 == 0) {
                EXPECT_TRUE(std::equal(lst.begin(), lst.end(), model.begin(), model.end()));
            }
        }
        collector::force_collect(true);
        EXPECT_EQ(lst.size(), model.size());
        EXPECT_TRUE(std::equal(lst.begin(), lst.end(), model.begin(), model.end()));
        EXPECT_TRUE(std::equal(lst.rbegin(), lst.rend(), model.rbegin(), model.rend()));
        expect_consistent(lst);
        lst.sort();
        model.sort();
        EXPECT_TRUE(std::equal(lst.begin(), lst.end(), model.begin(), model.end()));
    }
}

TEST(List_Test, Stress) {
    sgcl::list<int> lst;
    std::list<int> model;
    off_frame([&] {
        stress(lst, model);
    });
    EXPECT_EQ(collector::get_live_object_count(), lst.size() + 1);
    lst.clear();
    EXPECT_EQ(collector::get_live_object_count(), 1u);
}
