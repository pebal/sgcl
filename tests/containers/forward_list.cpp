//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "sgcl/core/forward_list.h"
#include "tests/types.h"

#include <algorithm>
#include <forward_list>
#include <iterator>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

static_assert(std::forward_iterator<sgcl::forward_list<int>::iterator>);
static_assert(std::forward_iterator<sgcl::forward_list<int>::const_iterator>);
static_assert(std::ranges::forward_range<sgcl::forward_list<int>>);
static_assert(std::ranges::forward_range<const sgcl::forward_list<int>>);
static_assert(std::is_trivially_copyable_v<sgcl::forward_list<int>::iterator>);
static_assert(std::is_trivially_copyable_v<sgcl::forward_list<int>::const_iterator>);
static_assert(std::is_nothrow_default_constructible_v<sgcl::forward_list<int>::iterator>);
static_assert(sizeof(sgcl::forward_list<int>::iterator) == sizeof(void*));

namespace {
    struct MoveOnly {
        int value;

        MoveOnly() noexcept
        : value(0) {
        }

        explicit MoveOnly(int v) noexcept
        : value(v) {
        }

        MoveOnly(MoveOnly&& other) noexcept
        : value(other.value) {
            other.value = -1;
        }

        MoveOnly& operator=(MoveOnly&& other) noexcept {
            value = other.value;
            other.value = -1;
            return *this;
        }

        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
    };

    struct Thrower {
        inline static int remaining = -1;
        inline static int live = 0;
        int value;

        explicit Thrower(int v)
        : value(v) {
            check();
            ++live;
        }

        Thrower(const Thrower& other)
        : value(other.value) {
            check();
            ++live;
        }

        Thrower& operator=(const Thrower&) = default;

        ~Thrower() {
            --live;
        }

        static void check() {
            if (remaining == 0) {
                remaining = -1;
                throw std::runtime_error("Thrower");
            }
            if (remaining > 0) {
                --remaining;
            }
        }
    };

    template<class L>
    std::vector<int> to_vector(const L& l) {
        return std::vector<int>(l.begin(), l.end());
    }

    template<class L>
    size_t length(const L& l) {
        return std::distance(l.begin(), l.end());
    }

    // Runs f in a frame of its own and returns its result. An iterator is a
    // raw node pointer: one left in a temporary of the test's frame roots
    // its node until the frame dies (README, "Stack roots"), so a step that
    // takes iterators before a live-object count runs here.
    template<class F>
    SGCL_NOINLINE decltype(auto) framed(F&& f) {
        return f();
    }

    // Elements with a key and an identity, for the stability checks.
    using Pair = std::pair<int, int>;

    bool by_key(const Pair& a, const Pair& b) {
        return a.first < b.first;
    }
}

TEST(ForwardList_Test, DefaultConstructorEmpty) {
    sgcl::forward_list<Int> lst;
    EXPECT_EQ(collector::get_live_object_count(), 1u);   // the sentinel
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(lst.begin(), lst.end());
    EXPECT_EQ(lst.cbegin(), lst.cend());
    EXPECT_EQ(std::next(lst.before_begin()), lst.end());
    EXPECT_EQ(std::next(lst.cbefore_begin()), lst.cend());
    EXPECT_GT(lst.max_size(), 0u);
}

TEST(ForwardList_Test, ConstructorNDefault) {
    sgcl::forward_list<Int> lst(3);
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_FALSE(lst.empty());
    std::vector<int> expected = {0, 0, 0};
    EXPECT_EQ(to_vector(lst), expected);
}

TEST(ForwardList_Test, ConstructorNValues) {
    sgcl::forward_list<Int> lst(4, 3);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(Int::counter, 4u);
    std::vector<int> expected = {3, 3, 3, 3};
    EXPECT_EQ(to_vector(lst), expected);
}

TEST(ForwardList_Test, InitializerList) {
    sgcl::forward_list<Int> lst({4, 5, 6});
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 3u);
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(to_vector(lst), expected);
}

TEST(ForwardList_Test, ConstructorRange) {
    std::vector<int> expected = {1, 2, 3};
    sgcl::forward_list<Int> lst(expected.begin(), expected.end());
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(to_vector(lst), expected);
    std::istringstream in("7 8 9");
    auto first = std::istream_iterator<int>(in);
    auto last = std::istream_iterator<int>();
    sgcl::forward_list<Int> other(first, last);
    expected = {7, 8, 9};
    EXPECT_EQ(to_vector(other), expected);
    EXPECT_EQ(Int::counter, 6u);
}

TEST(ForwardList_Test, CopyConstructor) {
    sgcl::forward_list<Int> other({1, 2, 3});
    sgcl::forward_list<Int> lst(other);
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(Int::counter, 6u);
    std::vector<int> expected = {1, 2, 3};
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(to_vector(other), expected);
}

TEST(ForwardList_Test, MoveConstructor) {
    sgcl::forward_list<Int> other({4, 5, 6});
    sgcl::forward_list<Int> lst(std::move(other));
    EXPECT_EQ(collector::get_live_object_count(), 5u);   // two sentinels, three nodes
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(other.begin(), other.end());
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(to_vector(lst), expected);
    other.push_front(1);   // a moved-from list is usable
    EXPECT_EQ(other.front(), 1);
}

TEST(ForwardList_Test, CopyAssignment) {
    sgcl::forward_list<Int> other({1, 2, 3});
    sgcl::forward_list<Int> lst;
    lst = other;
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(Int::counter, 6u);
    std::vector<int> expected = {1, 2, 3};
    EXPECT_EQ(to_vector(lst), expected);

    other = sgcl::forward_list<Int>({2, 3});
    lst = other;
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    EXPECT_EQ(Int::counter, 4u);
    expected = {2, 3};
    EXPECT_EQ(to_vector(lst), expected);

    other = sgcl::forward_list<Int>();
    lst = other;
    EXPECT_EQ(collector::get_live_object_count(), 2u);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(lst.empty());

    lst = {1};
    lst = lst;
    EXPECT_EQ(lst.front(), 1);
}

TEST(ForwardList_Test, MoveAssignment) {
    sgcl::forward_list<Int> other({4, 5, 6});
    sgcl::forward_list<Int> lst{1};
    lst = std::move(other);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_TRUE(other.empty());
    std::vector<int> expected = {4, 5, 6};
    EXPECT_EQ(to_vector(lst), expected);
}

TEST(ForwardList_Test, ListAssignment) {
    sgcl::forward_list<Int> lst;
    lst = {7, 8, 9};
    EXPECT_EQ(collector::get_live_object_count(), 4u);
    EXPECT_EQ(Int::counter, 3u);
    std::vector<int> expected = {7, 8, 9};
    EXPECT_EQ(to_vector(lst), expected);
    lst = {8, 9};
    EXPECT_EQ(Int::counter, 2u);
    expected = {8, 9};
    EXPECT_EQ(to_vector(lst), expected);
    lst = std::initializer_list<Int>();
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(lst.empty());
}

TEST(ForwardList_Test, Assign) {
    sgcl::forward_list<Int> lst({1, 2});
    lst.assign(4, 5);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    EXPECT_EQ(Int::counter, 4u);
    std::vector<int> expected = {5, 5, 5, 5};
    EXPECT_EQ(to_vector(lst), expected);
    lst.assign(2, 3);
    EXPECT_EQ(collector::get_live_object_count(), 3u);
    EXPECT_EQ(Int::counter, 2u);
    expected = {3, 3};
    EXPECT_EQ(to_vector(lst), expected);

    expected = {3, 4, 5};
    lst.assign(expected.begin(), expected.end());
    EXPECT_EQ(Int::counter, 3u);
    EXPECT_EQ(to_vector(lst), expected);
    std::istringstream in("1 2");
    lst.assign(std::istream_iterator<int>(in), std::istream_iterator<int>());
    expected = {1, 2};
    EXPECT_EQ(to_vector(lst), expected);

    lst.assign({6, 7, 8, 9});
    EXPECT_EQ(Int::counter, 4u);
    expected = {6, 7, 8, 9};
    EXPECT_EQ(to_vector(lst), expected);
    lst.assign(std::initializer_list<Int>());
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
}

TEST(ForwardList_Test, FrontAndIterators) {
    sgcl::forward_list<int> lst({5, 6, 7});
    const auto& clst = lst;
    EXPECT_EQ(lst.front(), 5);
    EXPECT_EQ(clst.front(), 5);
    lst.front() = 4;
    EXPECT_EQ(*lst.begin(), 4);
    EXPECT_EQ(*clst.begin(), 4);
    EXPECT_EQ(*clst.cbegin(), 4);
    EXPECT_EQ(*std::next(lst.before_begin()), 4);
    EXPECT_EQ(*std::next(clst.before_begin()), 4);
    EXPECT_EQ(*std::next(clst.cbefore_begin()), 4);
    EXPECT_EQ(std::distance(lst.begin(), lst.end()), 3);
    EXPECT_EQ(std::distance(clst.cbegin(), clst.cend()), 3);
    auto it = lst.begin();
    sgcl::forward_list<int>::const_iterator cit = it;   // iterator converts to const_iterator
    EXPECT_EQ(cit, it);
    EXPECT_EQ(*it++, 4);
    EXPECT_EQ(*it, 6);
    EXPECT_EQ(*++it, 7);
    EXPECT_EQ(++it, lst.end());
    EXPECT_EQ(std::ranges::distance(lst), 3);
    EXPECT_EQ(std::ranges::find(lst, 6), std::next(lst.begin()));
}

TEST(ForwardList_Test, InsertAfter) {
    sgcl::forward_list<Int> lst({2, 5});
    auto it = lst.insert_after(lst.before_begin(), 1);
    EXPECT_EQ(*it, 1);
    Int three(3);
    it = lst.insert_after(std::next(lst.begin()), three);
    EXPECT_EQ(*it, 3);
    it = lst.insert_after(it, Int(4));
    EXPECT_EQ(*it, 4);
    it = lst.insert_after(std::next(it), 6);   // after the last one
    EXPECT_EQ(*it, 6);
    EXPECT_EQ(std::next(it), lst.end());
    EXPECT_EQ(collector::get_live_object_count(), 7u);
    EXPECT_EQ(Int::counter, 7u);
    std::vector<int> expected = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(to_vector(lst), expected);

    it = lst.insert_after(lst.begin(), 0, 9);
    EXPECT_EQ(it, lst.begin());
    it = lst.insert_after(lst.begin(), 2, 9);
    EXPECT_EQ(*it, 9);
    EXPECT_EQ(*std::next(lst.begin()), 9);
    expected = {1, 9, 9, 2, 3, 4, 5, 6};
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(Int::counter, 9u);   // the elements and `three`

    std::vector<int> range = {7, 8};
    it = lst.insert_after(lst.before_begin(), range.begin(), range.begin());
    EXPECT_EQ(it, lst.before_begin());
    it = lst.insert_after(lst.before_begin(), range.begin(), range.end());
    EXPECT_EQ(*it, 8);
    expected = {7, 8, 1, 9, 9, 2, 3, 4, 5, 6};
    EXPECT_EQ(to_vector(lst), expected);
    std::istringstream in("11 12");
    it = lst.insert_after(it, std::istream_iterator<int>(in), std::istream_iterator<int>());
    EXPECT_EQ(*it, 12);
    expected = {7, 8, 11, 12, 1, 9, 9, 2, 3, 4, 5, 6};
    EXPECT_EQ(to_vector(lst), expected);

    it = lst.insert_after(it, {13, 14});
    EXPECT_EQ(*it, 14);
    it = lst.insert_after(it, std::initializer_list<Int>());
    EXPECT_EQ(*it, 14);
    expected = {7, 8, 11, 12, 13, 14, 1, 9, 9, 2, 3, 4, 5, 6};
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(Int::counter, 15u);
    EXPECT_EQ(collector::get_live_object_count(), 15u);
}

TEST(ForwardList_Test, EmplaceAfter) {
    sgcl::forward_list<Int> lst({2, 4});
    auto it = lst.emplace_after(lst.before_begin(), 1);
    EXPECT_EQ(*it, 1);
    it = lst.emplace_after(std::next(it), 3);
    EXPECT_EQ(*it, 3);
    it = lst.emplace_after(std::next(it), 5);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(Int::counter, 5u);
    std::vector<int> expected = {1, 2, 3, 4, 5};
    EXPECT_EQ(to_vector(lst), expected);
}

TEST(ForwardList_Test, EraseAfter) {
    // Every check that dereferences an element leaves the element's address
    // in the frame it runs in (a spill of `*it`); the count at the end must
    // not see those, so the whole body runs in a frame of its own; the
    // list dies with that frame, so nothing is left.
    off_frame([&] {
        sgcl::forward_list<Int> lst({1, 2, 3, 4, 5});
        auto it = lst.erase_after(lst.before_begin());
        EXPECT_EQ(*it, 2);
        EXPECT_EQ(Int::counter, 4u);
        it = lst.erase_after(lst.begin());
        EXPECT_EQ(*it, 4);
        EXPECT_EQ(Int::counter, 3u);
        it = lst.erase_after(std::next(lst.begin()));   // the last one
        EXPECT_EQ(it, lst.end());
        EXPECT_EQ(Int::counter, 2u);
        it = lst.erase_after(std::next(lst.begin()));   // nothing after the last one
        EXPECT_EQ(it, lst.end());
        std::vector<int> expected = {2, 4};
        EXPECT_EQ(to_vector(lst), expected);
        EXPECT_EQ(collector::get_live_object_count(), 3u);

        lst = {1, 2, 3, 4, 5, 6, 7, 8};
        it = lst.erase_after(lst.before_begin(), std::next(lst.begin(), 2));
        EXPECT_EQ(*it, 3);
        EXPECT_EQ(Int::counter, 6u);
        it = lst.erase_after(lst.begin(), lst.begin());
        EXPECT_EQ(it, lst.begin());
        it = lst.erase_after(lst.begin(), std::next(lst.begin()));
        EXPECT_EQ(it, std::next(lst.begin()));
        EXPECT_EQ(Int::counter, 6u);
        it = lst.erase_after(lst.begin(), std::next(lst.begin(), 3));
        EXPECT_EQ(*it, 6);
        EXPECT_EQ(Int::counter, 4u);
        expected = {3, 6, 7, 8};
        EXPECT_EQ(to_vector(lst), expected);
        it = lst.erase_after(std::next(lst.begin()), lst.end());
        EXPECT_EQ(it, lst.end());
        EXPECT_EQ(Int::counter, 2u);
        expected = {3, 6};
        EXPECT_EQ(to_vector(lst), expected);
        it = lst.erase_after(lst.before_begin(), lst.end());
        EXPECT_EQ(it, lst.end());
        EXPECT_EQ(Int::counter, 0u);
    });
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

TEST(ForwardList_Test, PushEmplacePopFront) {
    sgcl::forward_list<Int> lst{3};
    // a reference to an element left in the frame would keep its node, and
    // through the node's link the rest of the list, alive after a pop
    off_frame([&] {
        lst.push_front(2);
        Int one(1);
        lst.push_front(one);
        auto& v = lst.emplace_front(0);
        EXPECT_EQ(v, 0);
        EXPECT_EQ(&v, &lst.front());
    });
    EXPECT_EQ(Int::counter, 4u);
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    std::vector<int> expected = {0, 1, 2, 3};
    EXPECT_EQ(to_vector(lst), expected);
    lst.pop_front();
    EXPECT_EQ(Int::counter, 3u);
    off_frame([&] {
        EXPECT_EQ(lst.front(), 1);
    });
    lst.pop_front();
    lst.pop_front();
    lst.pop_front();
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
}

TEST(ForwardList_Test, Resize) {
    sgcl::forward_list<Int> lst({1, 2, 3});
    lst.resize(5);
    EXPECT_EQ(Int::counter, 5u);
    std::vector<int> expected = {1, 2, 3, 0, 0};
    EXPECT_EQ(to_vector(lst), expected);
    lst.resize(2);
    EXPECT_EQ(Int::counter, 2u);
    expected = {1, 2};
    EXPECT_EQ(to_vector(lst), expected);
    lst.resize(4, 8);
    EXPECT_EQ(Int::counter, 4u);
    expected = {1, 2, 8, 8};
    EXPECT_EQ(to_vector(lst), expected);
    lst.resize(4, 9);
    EXPECT_EQ(to_vector(lst), expected);
    lst.resize(0);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    sgcl::forward_list<MoveOnly> movers;
    movers.resize(3);   // resize(n) needs no copy
    EXPECT_EQ(length(movers), 3u);
}

TEST(ForwardList_Test, Swap) {
    sgcl::forward_list<Int> lst1({1, 2});
    sgcl::forward_list<Int> lst2({4, 5, 6, 7});
    lst1.swap(lst2);
    EXPECT_EQ(collector::get_live_object_count(), 8u);
    EXPECT_EQ(Int::counter, 6u);
    std::vector<int> expected = {4, 5, 6, 7};
    EXPECT_EQ(to_vector(lst1), expected);
    expected = {1, 2};
    EXPECT_EQ(to_vector(lst2), expected);
    swap(lst1, lst2);
    EXPECT_EQ(to_vector(lst1), expected);
    sgcl::forward_list<Int> empty;
    lst1.swap(empty);
    EXPECT_TRUE(lst1.empty());
    EXPECT_EQ(to_vector(empty), expected);
}

TEST(ForwardList_Test, Clear) {
    sgcl::forward_list<Int> lst({5, 6, 7});
    lst.clear();
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(lst.begin(), lst.end());
    lst.push_front(1);
    EXPECT_EQ(lst.front(), 1);
}

TEST(ForwardList_Test, Merge) {
    sgcl::forward_list<Int> lst({1, 3, 5, 7});
    sgcl::forward_list<Int> other({2, 3, 6, 8, 9});
    lst.merge(other);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(Int::counter, 9u);
    EXPECT_EQ(collector::get_live_object_count(), 11u);
    std::vector<int> expected = {1, 2, 3, 3, 5, 6, 7, 8, 9};
    EXPECT_EQ(to_vector(lst), expected);

    lst.merge(sgcl::forward_list<Int>({0, 10}));
    expected = {0, 1, 2, 3, 3, 5, 6, 7, 8, 9, 10};
    EXPECT_EQ(to_vector(lst), expected);

    lst.merge(other);   // an empty one
    EXPECT_EQ(to_vector(lst), expected);
    lst.merge(lst);     // itself
    EXPECT_EQ(to_vector(lst), expected);
    other.merge(lst);   // into an empty one
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(to_vector(other), expected);

    sgcl::forward_list<Int> desc1({9, 5, 1});
    sgcl::forward_list<Int> desc2({8, 2});
    desc1.merge(desc2, std::greater<int>());
    EXPECT_TRUE(desc2.empty());
    expected = {9, 8, 5, 2, 1};
    EXPECT_EQ(to_vector(desc1), expected);
    desc1.merge(sgcl::forward_list<Int>({7, 0}), std::greater<int>());
    expected = {9, 8, 7, 5, 2, 1, 0};
    EXPECT_EQ(to_vector(desc1), expected);
    EXPECT_EQ(Int::counter, 18u);

    // stable: on equal keys the elements of *this come first
    sgcl::forward_list<Pair> p1({{1, 1}, {2, 1}, {3, 1}});
    sgcl::forward_list<Pair> p2({{1, 2}, {3, 2}, {3, 3}});
    p1.merge(p2, by_key);
    std::vector<Pair> pairs(p1.begin(), p1.end());
    std::vector<Pair> expected_pairs = {{1, 1}, {1, 2}, {2, 1}, {3, 1}, {3, 2}, {3, 3}};
    EXPECT_EQ(pairs, expected_pairs);
}

TEST(ForwardList_Test, SpliceAfterOtherList) {
    // counted relative to the start: a word of an earlier test's frame may
    // survive under the listener's own frame and keep a node or two around
    const auto before = collector::get_live_object_count();
    sgcl::forward_list<Int> lst({1, 2, 3});
    sgcl::forward_list<Int> other({4, 5});
    lst.splice_after(lst.begin(), other);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(collector::get_live_object_count(), before + 7u);
    std::vector<int> expected = {1, 4, 5, 2, 3};
    EXPECT_EQ(to_vector(lst), expected);
    lst.splice_after(lst.before_begin(), sgcl::forward_list<Int>{0});
    expected = {0, 1, 4, 5, 2, 3};
    EXPECT_EQ(to_vector(lst), expected);
    lst.splice_after(std::next(lst.begin(), 5), sgcl::forward_list<Int>({6, 7}));   // after the last
    expected = {0, 1, 4, 5, 2, 3, 6, 7};
    EXPECT_EQ(to_vector(lst), expected);
    lst.splice_after(lst.begin(), other);   // nothing to move
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(Int::counter, 8u);

    // one element
    other = {10, 11, 12};
    lst.splice_after(lst.begin(), other, other.begin());   // 11
    expected = {0, 11, 1, 4, 5, 2, 3, 6, 7};
    EXPECT_EQ(to_vector(lst), expected);
    std::vector<int> expected_other = {10, 12};
    EXPECT_EQ(to_vector(other), expected_other);
    lst.splice_after(lst.before_begin(), other, other.before_begin());   // 10
    expected = {10, 0, 11, 1, 4, 5, 2, 3, 6, 7};
    EXPECT_EQ(to_vector(lst), expected);
    expected_other = {12};
    EXPECT_EQ(to_vector(other), expected_other);
    lst.splice_after(lst.begin(), other, other.begin());   // nothing after 12
    EXPECT_EQ(to_vector(lst), expected);
    lst.splice_after(lst.begin(), std::move(other), other.before_begin());   // 12
    expected = {10, 12, 0, 11, 1, 4, 5, 2, 3, 6, 7};
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(Int::counter, 11u);

    // a range
    other = {20, 21, 22, 23, 24};
    lst.splice_after(lst.begin(), other, other.begin(), std::next(other.begin(), 3));   // 21, 22
    expected = {10, 21, 22, 12, 0, 11, 1, 4, 5, 2, 3, 6, 7};
    EXPECT_EQ(to_vector(lst), expected);
    expected_other = {20, 23, 24};
    EXPECT_EQ(to_vector(other), expected_other);
    lst.splice_after(lst.begin(), other, other.begin(), std::next(other.begin()));   // empty range
    EXPECT_EQ(to_vector(lst), expected);
    lst.splice_after(std::next(lst.begin(), 12), std::move(other), other.before_begin(), other.end());   // all, after the last
    expected = {10, 21, 22, 12, 0, 11, 1, 4, 5, 2, 3, 6, 7, 20, 23, 24};
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_TRUE(other.empty());
    EXPECT_EQ(Int::counter, 16u);
    EXPECT_EQ(collector::get_live_object_count(), 18u);
}

TEST(ForwardList_Test, SpliceAfterSameList) {
    sgcl::forward_list<Int> lst({1, 2, 3, 4, 5, 6});
    lst.splice_after(lst.before_begin(), lst, std::next(lst.begin(), 2));   // 4 to the front
    std::vector<int> expected = {4, 1, 2, 3, 5, 6};
    EXPECT_EQ(to_vector(lst), expected);
    lst.splice_after(std::next(lst.begin(), 5), lst, lst.before_begin());   // 4 to the back
    expected = {1, 2, 3, 5, 6, 4};
    EXPECT_EQ(to_vector(lst), expected);
    lst.splice_after(lst.begin(), lst, lst.begin());   // pos == it: nothing
    EXPECT_EQ(to_vector(lst), expected);
    lst.splice_after(std::next(lst.begin()), lst, lst.begin());   // pos == the element after it: nothing
    EXPECT_EQ(to_vector(lst), expected);
    lst.splice_after(lst.before_begin(), lst, std::next(lst.begin(), 2), lst.end());   // 5, 6, 4 to the front
    expected = {5, 6, 4, 1, 2, 3};
    EXPECT_EQ(to_vector(lst), expected);
    lst.splice_after(std::next(lst.begin(), 5), lst, lst.before_begin(), std::next(lst.begin(), 2));   // 5, 6 to the back
    expected = {4, 1, 2, 3, 5, 6};
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
}

TEST(ForwardList_Test, RemoveAndUnique) {
    sgcl::forward_list<Int> lst({1, 2, 3, 2, 4, 2, 5});
    EXPECT_EQ(lst.remove_if([](const Int& v) { return v == 2; }), 3u);
    EXPECT_EQ(Int::counter, 4u);
    std::vector<int> expected = {1, 3, 4, 5};
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(lst.remove_if([](const Int& v) { return v > 3; }), 2u);
    EXPECT_EQ(Int::counter, 2u);
    expected = {1, 3};
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(lst.unique([](const Int& a, const Int& b) { return int(b) - int(a) == 2; }), 1u);
    EXPECT_EQ(Int::counter, 1u);
    EXPECT_EQ(erase(lst, 1), 1u);
    EXPECT_TRUE(lst.empty());
    EXPECT_EQ(lst.remove_if([](const Int&) { return true; }), 0u);
    EXPECT_EQ(collector::get_live_object_count(), 1u);
    lst = {1, 2, 3};
    EXPECT_EQ(erase_if(lst, [](const Int& v) { return v != 2; }), 2u);
    EXPECT_EQ(lst.front(), 2);
    EXPECT_EQ(Int::counter, 1u);

    sgcl::forward_list<int> ints({1, 2, 3, 2, 4, 2, 5});
    EXPECT_EQ(ints.remove(2), 3u);
    expected = {1, 3, 4, 5};
    EXPECT_EQ(to_vector(ints), expected);
    EXPECT_EQ(ints.remove(9), 0u);
    EXPECT_EQ(erase(ints, 4), 1u);
    expected = {1, 3, 5};
    EXPECT_EQ(to_vector(ints), expected);
    ints = {1, 1, 2, 2, 2, 3, 1, 1};
    EXPECT_EQ(ints.unique(), 4u);
    expected = {1, 2, 3, 1};
    EXPECT_EQ(to_vector(ints), expected);
    EXPECT_EQ(ints.unique(), 0u);
    EXPECT_EQ(ints.unique([](int a, int b) { return b - a == 1; }), 1u);   // 2 goes, then 1 and 3 are not consecutive
    expected = {1, 3, 1};
    EXPECT_EQ(to_vector(ints), expected);
    sgcl::forward_list<int> empty;
    EXPECT_EQ(empty.unique(), 0u);
    EXPECT_EQ(empty.remove(1), 0u);
    EXPECT_EQ(collector::get_live_object_count(), 7u);
}

TEST(ForwardList_Test, Reverse) {
    sgcl::forward_list<Int> lst;
    lst.reverse();
    EXPECT_TRUE(lst.empty());
    lst = {1};
    lst.reverse();
    std::vector<int> expected = {1};
    EXPECT_EQ(to_vector(lst), expected);
    lst = {1, 2, 3, 4, 5};
    lst.reverse();
    expected = {5, 4, 3, 2, 1};
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(Int::counter, 5u);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
}

TEST(ForwardList_Test, Sort) {
    sgcl::forward_list<Int> lst;
    lst.sort();
    EXPECT_TRUE(lst.empty());
    lst = {1};
    lst.sort();
    EXPECT_EQ(lst.front(), 1);
    lst = {5, 1, 4, 1, 3, 2};
    lst.sort();
    std::vector<int> expected = {1, 1, 2, 3, 4, 5};
    EXPECT_EQ(to_vector(lst), expected);
    lst.sort(std::greater<int>());
    expected = {5, 4, 3, 2, 1, 1};
    EXPECT_EQ(to_vector(lst), expected);
    EXPECT_EQ(Int::counter, 6u);
    EXPECT_EQ(collector::get_live_object_count(), 7u);

    std::mt19937 rng(3);
    sgcl::forward_list<int> big;
    std::vector<int> oracle;
    for (int i = 0; i < 5000; ++i) {
        int v = int(rng() % 1000);
        big.push_front(v);
        oracle.push_back(v);
    }
    big.sort();
    std::sort(oracle.begin(), oracle.end());
    EXPECT_EQ(to_vector(big), oracle);
    EXPECT_TRUE(std::is_sorted(big.begin(), big.end()));
    big.sort([](int a, int b) { return a > b; });
    std::sort(oracle.begin(), oracle.end(), std::greater<>());
    EXPECT_EQ(to_vector(big), oracle);

    // stable: equal keys keep their order
    sgcl::forward_list<Pair> pairs;
    std::vector<Pair> pair_oracle;
    for (int i = 0; i < 1000; ++i) {
        Pair p(int(rng() % 10), i);
        pairs.push_front(p);
        pair_oracle.insert(pair_oracle.begin(), p);
    }
    pairs.sort(by_key);
    std::stable_sort(pair_oracle.begin(), pair_oracle.end(), by_key);
    EXPECT_EQ(std::vector<Pair>(pairs.begin(), pairs.end()), pair_oracle);
}

// A comparison that throws: the list keeps all its elements (order
// unspecified) and remains usable.
TEST(ForwardList_Test, ThrowingComparator) {
    std::mt19937 rng(5);
    sgcl::forward_list<Int> lst;
    std::vector<int> oracle;
    for (int i = 0; i < 200; ++i) {
        int v = int(rng() % 100);
        lst.push_front(v);
        oracle.push_back(v);
    }
    std::sort(oracle.begin(), oracle.end());
    for (int fail_at : {1, 2, 7, 100, 700}) {
        int calls = 0;
        auto comp = [&](const Int& a, const Int& b) {
            if (++calls == fail_at) {
                throw std::runtime_error("comp");
            }
            return int(a) < int(b);
        };
        EXPECT_THROW(lst.sort(comp), std::runtime_error) << fail_at;
        auto after = to_vector(lst);
        std::sort(after.begin(), after.end());
        EXPECT_EQ(after, oracle) << fail_at;
        EXPECT_EQ(Int::counter, 200u);
        EXPECT_EQ(collector::get_live_object_count(), 201u);
    }
    lst.sort();
    EXPECT_EQ(to_vector(lst), oracle);

    sgcl::forward_list<Int> other({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    int calls = 0;
    auto comp = [&](const Int& a, const Int& b) {
        if (++calls == 6) {
            throw std::runtime_error("comp");
        }
        return int(a) < int(b);
    };
    EXPECT_THROW(lst.merge(other, comp), std::runtime_error);
    auto after = to_vector(lst);
    auto rest = to_vector(other);
    EXPECT_EQ(after.size() + rest.size(), 210u);
    EXPECT_EQ(Int::counter, 210u);
    EXPECT_EQ(collector::get_live_object_count(), 212u);
    after.insert(after.end(), rest.begin(), rest.end());
    std::sort(after.begin(), after.end());
    oracle.insert(oracle.end(), {0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    std::sort(oracle.begin(), oracle.end());
    EXPECT_EQ(after, oracle);
    lst.sort();
    other.sort();
    lst.merge(other);
    EXPECT_EQ(to_vector(lst), oracle);
}

TEST(ForwardList_Test, Comparisons) {
    sgcl::forward_list<int> a({1, 2, 3});
    sgcl::forward_list<int> b({1, 2, 3});
    sgcl::forward_list<int> c({1, 2, 4});
    sgcl::forward_list<int> d({1, 2});
    sgcl::forward_list<int> e;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a != d);
    EXPECT_TRUE(d != a);
    EXPECT_TRUE(a < c);
    EXPECT_TRUE(c > a);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(d < a);
    EXPECT_TRUE(e < d);
    EXPECT_TRUE(e == sgcl::forward_list<int>());
    EXPECT_EQ(a <=> b, std::strong_ordering::equal);
    EXPECT_EQ(a <=> c, std::strong_ordering::less);
    EXPECT_EQ(a <=> d, std::strong_ordering::greater);
    sgcl::forward_list<std::string> s1({"a", "b"});
    sgcl::forward_list<std::string> s2({"a", "c"});
    EXPECT_TRUE(s1 < s2);
    sgcl::forward_list<double> f1({1.0});
    sgcl::forward_list<double> f2({1.5});
    EXPECT_EQ(f1 <=> f2, std::partial_ordering::less);
}

TEST(ForwardList_Test, MoveOnlyElements) {
    sgcl::forward_list<MoveOnly> lst;
    lst.push_front(MoveOnly(3));
    lst.emplace_front(2);
    lst.insert_after(lst.before_begin(), MoveOnly(1));
    lst.emplace_after(std::next(lst.begin(), 2), 4);
    std::vector<int> values;
    for (auto& m : lst) {
        values.push_back(m.value);
    }
    std::vector<int> expected = {1, 2, 3, 4};
    EXPECT_EQ(values, expected);
    lst.erase_after(lst.begin());
    lst.pop_front();
    lst.resize(4);
    lst.reverse();
    lst.sort([](const MoveOnly& a, const MoveOnly& b) { return a.value < b.value; });
    values.clear();
    for (auto& m : lst) {
        values.push_back(m.value);
    }
    expected = {0, 0, 3, 4};
    EXPECT_EQ(values, expected);
    sgcl::forward_list<MoveOnly> other(std::move(lst));
    EXPECT_TRUE(lst.empty());
    lst = std::move(other);
    EXPECT_EQ(length(lst), 4u);
    EXPECT_EQ(lst.remove_if([](const MoveOnly& m) { return m.value == 0; }), 2u);
    EXPECT_EQ(lst.front().value, 3);
}

TEST(ForwardList_Test, StringElements) {
    sgcl::forward_list<std::string> lst({"pear", "apple"});
    sgcl::forward_list<std::string> copy;
    off_frame([&] {
        lst.push_front("fig");
        lst.emplace_after(lst.begin(), 3, 'z');
        std::string s = "plum";
        lst.insert_after(std::next(lst.begin(), 3), s);
        std::vector<std::string> expected = {"fig", "zzz", "pear", "apple", "plum"};
        EXPECT_EQ(std::vector<std::string>(lst.begin(), lst.end()), expected);
        lst.sort();
        std::sort(expected.begin(), expected.end());
        EXPECT_EQ(std::vector<std::string>(lst.begin(), lst.end()), expected);
        EXPECT_EQ(lst.remove("zzz"), 1u);
        copy = lst;
        EXPECT_EQ(copy, lst);
        lst.clear();
    });
    EXPECT_EQ(length(copy), 4u);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
}

TEST(ForwardList_Test, TrackedElements) {
    sgcl::forward_list<Foo> lst;
    off_frame([&] {
        lst.emplace_front(2);
        lst.push_front(Foo(1));
        lst.emplace_after(lst.begin(), 7);
    });
    collector::force_collect(true);
    EXPECT_EQ(collector::get_live_object_count(), 7u);   // the sentinel, three nodes, three Baz
    std::vector<int> values;
    off_frame([&] {
        for (auto& f : lst) {
            values.push_back(f.ptr->value);
        }
    });
    std::vector<int> expected = {1, 7, 2};
    EXPECT_EQ(values, expected);
    off_frame([&] {
        lst.erase_after(lst.begin());
    });
    EXPECT_EQ(collector::get_live_object_count(), 5u);
    lst.pop_front();
    EXPECT_EQ(collector::get_live_object_count(), 3u);

    sgcl::forward_list<tracked_ptr<Baz>> ptrs;
    for (int i = 0; i < 1000; ++i) {
        ptrs.push_front(make_tracked<Baz>(i));
    }
    collector::force_collect(true);
    off_frame([&] {
        int expected_value = 999;
        for (auto& p : ptrs) {
            ASSERT_EQ(p->value, expected_value--);
        }
    });
    ptrs.sort([](const tracked_ptr<Baz>& a, const tracked_ptr<Baz>& b) { return a->value < b->value; });
    off_frame([&] {
        EXPECT_EQ(ptrs.front()->value, 0);
    });
    EXPECT_EQ(ptrs.remove_if([](const tracked_ptr<Baz>& p) { return p->value >= 500; }), 500u);
    EXPECT_EQ(collector::get_live_object_count(), 3u + 1u + 500u + 500u);
    ptrs.clear();
    lst.clear();
    EXPECT_EQ(collector::get_live_object_count(), 2u);
}

// Iterators are raw node pointers: they may sit in a std container or a
// lambda's capture, and stay valid while their element stays in the list.
TEST(ForwardList_Test, IteratorsInStdVector) {
    sgcl::forward_list<int> lst({1, 2, 3, 4, 5});
    std::vector<sgcl::forward_list<int>::iterator> its;
    its.push_back(lst.before_begin());
    for (auto it = lst.begin(); it != lst.end(); ++it) {
        its.push_back(it);
    }
    ASSERT_EQ(its.size(), 6u);
    for (size_t i = 1; i < its.size(); ++i) {
        EXPECT_EQ(*its[i], int(i));
    }
    lst.erase_after(its[3]);   // 4
    lst.erase_after(its[0]);   // 1
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3, 5}));
    EXPECT_EQ(std::next(its[3]), its[5]);
    EXPECT_EQ(std::next(its[0]), its[2]);
    auto value_of = [it = its[2]] { return *it; };   // captured by value
    EXPECT_EQ(value_of(), 2);
    std::vector<sgcl::forward_list<int>::const_iterator> cits(its.begin(), its.end());
    EXPECT_EQ(*cits[5], 5);
    lst.insert_after(cits[3], 4);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3, 4, 5}));
}

// A raw iterator kept across a collection stays valid while its element
// stays in the list: the list roots the node. The nodes erased around it
// are collected: the vector's words are not roots.
TEST(ForwardList_Test, IteratorAcrossCollection) {
    sgcl::forward_list<Int> lst({1, 2, 3, 4});
    std::vector<sgcl::forward_list<Int>::iterator> kept;
    off_frame([&] {
        for (auto it = lst.begin(); it != lst.end(); ++it) {
            kept.push_back(it);
        }
    });
    auto it = kept[2];
    off_frame([&] {
        lst.pop_front();
        lst.erase_after(std::next(lst.begin()));   // 4
    });
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(*it, 3);
    EXPECT_EQ(*kept[1], 2);
    EXPECT_EQ(std::next(kept[1]), it);
    EXPECT_EQ(std::next(it), lst.end());
    EXPECT_EQ(Int::counter, 2u);
    EXPECT_EQ(collector::get_live_object_count(), 3u);   // the sentinel and two nodes
    lst.insert_after(it, 9);
    EXPECT_EQ(to_vector(lst), (std::vector<int>{2, 3, 9}));
    EXPECT_EQ(*std::next(it), 9);
}

TEST(ForwardList_Test, ListInsideManagedObject) {
    struct Holder {
        sgcl::forward_list<Int> values;
        sgcl::forward_list<tracked_ptr<Holder>> links;
    };
    off_frame([&] {
        tracked_ptr<Holder> a = make_tracked<Holder>();
        tracked_ptr<Holder> b = make_tracked<Holder>();
        a->values = {1, 2, 3};
        b->values.assign(500, 4);
        a->links.push_front(b);
        b->links.push_front(a);   // a cycle
        collector::force_collect(true);
        EXPECT_EQ(length(a->values), 3u);
        EXPECT_EQ(length(a->links.front()->values), 500u);
        EXPECT_EQ(Int::counter, 503u);
        // two holders, four sentinels, 3 + 1 + 500 + 1 nodes
        EXPECT_EQ(collector::get_live_object_count(), 2u + 4u + 505u);
    });
    EXPECT_EQ(collector::get_live_object_count(), 0u);
    EXPECT_EQ(Int::counter, 0u);
}

TEST(ForwardList_Test, ExceptionSafety) {
    sgcl::forward_list<Thrower> lst;
    for (int i = 4; i >= 0; --i) {
        lst.emplace_front(i);
    }
    EXPECT_EQ(Thrower::live, 5);
    auto values = [&] {
        std::vector<int> v;
        for (auto& t : lst) {
            v.push_back(t.value);
        }
        return v;
    };
    std::vector<int> expected = {0, 1, 2, 3, 4};

    Thrower::remaining = 0;
    EXPECT_THROW(lst.emplace_front(9), std::runtime_error);
    EXPECT_EQ(values(), expected);
    EXPECT_EQ(Thrower::live, 5);
    Thrower::remaining = 0;
    EXPECT_THROW(lst.emplace_after(std::next(lst.begin()), 9), std::runtime_error);
    EXPECT_EQ(values(), expected);
    Thrower value(7);
    Thrower::remaining = 2;
    EXPECT_THROW(lst.insert_after(lst.begin(), 4, value), std::runtime_error);
    EXPECT_EQ(values(), expected);
    EXPECT_EQ(Thrower::live, 6);
    std::vector<Thrower> range = {Thrower(8), Thrower(9), Thrower(10)};
    Thrower::remaining = 1;
    EXPECT_THROW(lst.insert_after(lst.before_begin(), range.begin(), range.end()), std::runtime_error);
    EXPECT_EQ(values(), expected);
    Thrower::remaining = 1;
    EXPECT_THROW(lst.resize(8, value), std::runtime_error);
    EXPECT_EQ(values(), expected);
    Thrower::remaining = 1;
    EXPECT_THROW(lst.assign(8, value), std::runtime_error);   // the first five are assigned, the sixth throws
    EXPECT_EQ(values().size(), 5u);
    EXPECT_EQ(Thrower::live, 9);
    EXPECT_EQ(collector::get_live_object_count(), 6u);   // nodes built for the failed insertions are garbage
    Thrower::remaining = 3;
    EXPECT_THROW(sgcl::forward_list<Thrower>(5, value), std::runtime_error);
    EXPECT_EQ(Thrower::live, 9);
    Thrower::remaining = 2;
    EXPECT_THROW({ sgcl::forward_list<Thrower> copy(lst); }, std::runtime_error);
    EXPECT_EQ(Thrower::live, 9);
    EXPECT_EQ(collector::get_live_object_count(), 6u);
    Thrower::remaining = -1;
    lst.clear();
    range.clear();
    EXPECT_EQ(Thrower::live, 1);
}

TEST(ForwardList_Test, StressAgainstStdForwardList) {
    std::mt19937 rng(11);
    sgcl::forward_list<Int> lst;
    std::forward_list<int> oracle;
    size_t size = 0;
    auto same = [&] {
        return length(lst) == size && Int::counter == size
            && std::equal(lst.begin(), lst.end(), oracle.begin(), oracle.end(), [](const Int& a, int b) { return a == b; });
    };
    off_frame([&] {
        for (int step = 0; step < 20000; ++step) {
            int op = rng() % 100;
            int value = int(rng() % 50);
            size_t pos = size ? rng() % size : 0;
            if (op < 25 || size == 0) {
                lst.push_front(value);
                oracle.push_front(value);
                ++size;
            } else if (op < 35) {
                lst.pop_front();
                oracle.pop_front();
                --size;
            } else if (op < 50) {
                auto it = lst.insert_after(std::next(lst.before_begin(), pos), value);
                auto oit = oracle.insert_after(std::next(oracle.before_begin(), pos), value);
                ASSERT_EQ(*it, *oit);
                ++size;
            } else if (op < 60) {
                if (pos + 1 < size) {
                    lst.erase_after(std::next(lst.begin(), pos));
                    oracle.erase_after(std::next(oracle.begin(), pos));
                    --size;
                }
            } else if (op < 65) {
                size_t count = std::min<size_t>(rng() % 4, size - pos);
                lst.erase_after(std::next(lst.before_begin(), pos), std::next(lst.before_begin(), pos + count + 1));
                oracle.erase_after(std::next(oracle.before_begin(), pos), std::next(oracle.before_begin(), pos + count + 1));
                size -= count;
            } else if (op < 70) {
                size -= lst.remove_if([value](const Int& v) { return v == value; });
                oracle.remove(value);
            } else if (op < 75) {
                lst.reverse();
                oracle.reverse();
            } else if (op < 80) {
                lst.sort();
                oracle.sort();
            } else if (op < 83) {
                size -= lst.unique([](const Int& a, const Int& b) { return int(a) == int(b); });
                oracle.unique();
            } else if (op < 90) {
                // a range spliced elsewhere within the same list
                size_t first = rng() % size;
                size_t last = first + rng() % std::min<size_t>(4, size - first);
                size_t target = rng() % size;
                if (target <= first || target > last) {
                    lst.splice_after(std::next(lst.before_begin(), target), lst, std::next(lst.before_begin(), first), std::next(lst.before_begin(), last + 1));
                    oracle.splice_after(std::next(oracle.before_begin(), target), oracle, std::next(oracle.before_begin(), first), std::next(oracle.before_begin(), last + 1));
                }
            } else if (op < 93) {
                collector::force_collect();
            } else {
                ASSERT_EQ(lst.front(), oracle.front());
                ASSERT_EQ(lst.empty(), oracle.empty());
            }
            if (step % 200 == 0) {
                ASSERT_TRUE(same()) << "step " << step << " size " << size << " length " << length(lst) << " counter " << Int::counter << " live " << collector::get_live_object_count();
            }
            if (size > 300) {   // keep the walks short
                lst.erase_after(std::next(lst.before_begin(), 100), lst.end());
                oracle.erase_after(std::next(oracle.before_begin(), 100), oracle.end());
                size = 100;
            }
        }
    });
    if (::testing::Test::HasFatalFailure()) {
        return;
    }
    ASSERT_TRUE(same());
    sgcl::forward_list<Int> copy(lst);
    ASSERT_TRUE(framed([&] { return std::equal(copy.begin(), copy.end(), lst.begin(), lst.end(), [](const Int& a, const Int& b) { return int(a) == int(b); }); }));
    lst.clear();
    EXPECT_EQ(Int::counter, size);
    copy.clear();
    EXPECT_EQ(Int::counter, 0u);
    EXPECT_EQ(collector::get_live_object_count(), 2u);
}
