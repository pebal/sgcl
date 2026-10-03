//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The mixins' members on every sequence (mixin::enumerable, mixin::ordered, mixin::sequence); the deduction
// guides of atomic, queue, stack, copy_on_write, task
#include "sgcl/sgcl.h"
#include "tests/types.h"

using namespace sgcl::async;

#include <string>
#include <utility>

namespace {
    struct Config {
        int version;
    };

    // The mixins' members are noexcept as far as the element's operations
    // and the function given are
    inline constexpr auto odd = [](int x) noexcept { return x % 2 != 0; };
    inline constexpr auto odd_may_throw = [](int x) { return x % 2 != 0; };
    static_assert(noexcept(std::declval<const sgcl::vector<int>&>().exists(odd)));
    static_assert(!noexcept(std::declval<const sgcl::vector<int>&>().exists(odd_may_throw)));
    static_assert(noexcept(std::declval<sgcl::vector<int>&>().sort()));
    static_assert(noexcept(std::declval<const sgcl::vector<int>&>().binary_search(1)));
    static_assert(noexcept(std::declval<const sgcl::sorted_map<int, int>&>().get(1)));
    static_assert(!noexcept(std::declval<const sgcl::sorted_map<int, std::string>&>().get(1)));
}

TEST(MSequence_Tests, TheAlgorithmsAreMembersOfEverySequence) {
    sgcl::vector v = {3, 1, 2};
    sgcl::deque d = {3, 1, 2};
    sgcl::list l = {3, 1, 2};
    sgcl::forward_list f = {3, 1, 2};
    sgcl::array<int, 3> a = {3, 1, 2};
    sgcl::dynamic_array<int> da = {3, 1, 2};
    sgcl::array<int, 0> z;
    v.sort(); d.sort(); l.sort(); f.sort(); a.sort(); da.sort();
    EXPECT_TRUE(v.is_sorted() && d.is_sorted() && l.is_sorted() && f.is_sorted() && a.is_sorted() && da.is_sorted());
    v.reverse(); d.reverse(); l.reverse(); f.reverse(); a.reverse(); da.reverse();
    EXPECT_EQ(v.index_of(3), 0u);
    EXPECT_EQ(d.last_index_of(1), 2u);
    EXPECT_TRUE(l.contains(2));
    EXPECT_EQ(f.find_index([](int x) { return x == 1; }), 2u);
    EXPECT_EQ(*a.find_if([](int x) { return x < 3; }), 2);
    EXPECT_TRUE(da.exists([](int x) { return x == 3; }));
    EXPECT_TRUE(v.all([](int x) { return x > 0; }));
    EXPECT_EQ(v.count_of([](int x) { return x > 1; }), 2u);
    EXPECT_EQ(v.min(), 1);
    EXPECT_EQ(v.max(), 3);
    EXPECT_EQ(a.min(), 1);
    int sum = 0;
    l.for_each([&](int x) { sum += x; });
    EXPECT_EQ(sum, 6);
    v.fill(7);
    EXPECT_TRUE(v.all([](int x) { return x == 7; }));
    EXPECT_FALSE(z.contains(1));
    EXPECT_EQ(z.index_of(1), sgcl::npos);
    EXPECT_EQ(v.index_of(9), sgcl::npos);
    // the lookups of a sorted sequence: the flat map idiom
    sgcl::vector sorted = {1, 3, 5, 7};
    EXPECT_TRUE(sorted.binary_search(5) && !sorted.binary_search(4));
    EXPECT_EQ(sorted.sorted_index_of(7), 3u);
    EXPECT_EQ(sorted.sorted_index_of(4), sgcl::npos);
    EXPECT_EQ(*sorted.lower_bound(4), 5);
    EXPECT_EQ(*sorted.upper_bound(5), 7);
    EXPECT_EQ(sorted.upper_bound(7), sorted.end());
    EXPECT_EQ(sorted.lower_bound(0), sorted.begin());
    auto desc = std::greater<int>();
    sgcl::vector descending = {7, 5, 3, 1};
    EXPECT_TRUE(descending.binary_search(3, desc) && descending.sorted_index_of(5, desc) == 1 && *descending.lower_bound(4, desc) == 3);
    sgcl::list sorted_list = {1, 2, 3};
    EXPECT_TRUE(sorted_list.binary_search(2) && *sorted_list.lower_bound(2) == 2);
    const sgcl::deque sorted_deque = {2, 4};
    EXPECT_EQ(*sorted_deque.lower_bound(3), 4);
    EXPECT_FALSE(z.binary_search(1));
    // the mixins: no state, no size
    static_assert(std::is_empty_v<sgcl::mixin::enumerable<sgcl::vector<int>>> && std::is_empty_v<sgcl::mixin::ordered<sgcl::vector<int>>> && std::is_empty_v<sgcl::mixin::sequence<sgcl::vector<int>>>);
    static_assert(sizeof(sgcl::array<int, 3>) == 3 * sizeof(int));
    static_assert(sizeof(sgcl::vector<int>) == sizeof(sgcl::vector<int>::size_type) * 2 + sizeof(void*));
}

TEST(MSequence_Tests, TheDeductionGuides) {
    sgcl::atomic current = sgcl::make_tracked<Config>(0);
    static_assert(std::is_same_v<decltype(current), sgcl::atomic<sgcl::tracked_ptr<Config>>>);
    sgcl::tracked_ptr p = sgcl::make_tracked<Config>(1);
    sgcl::atomic from_ptr = p;
    static_assert(std::is_same_v<decltype(from_ptr), sgcl::atomic<sgcl::tracked_ptr<Config>>>);
    sgcl::atomic host = sgcl::string("x");
    static_assert(std::is_same_v<decltype(host), sgcl::atomic<sgcl::string>>);
    sgcl::atomic n = 5;
    sgcl::atomic flag = true;
    static_assert(std::is_same_v<decltype(n), sgcl::atomic<int>> && std::is_same_v<decltype(flag), sgcl::atomic<bool>>);
    EXPECT_EQ(current.load()->version, 0);
    EXPECT_EQ(n.load(), 5);
    sgcl::vector v = {1, 2};
    sgcl::queue q(v);
    sgcl::stack s(v);
    sgcl::priority_queue pq(std::less<int>(), v);
    static_assert(std::is_same_v<decltype(q), sgcl::queue<int, sgcl::vector<int>>>);
    static_assert(std::is_same_v<decltype(s), sgcl::stack<int, sgcl::vector<int>>>);
    static_assert(std::is_same_v<decltype(pq), sgcl::priority_queue<int, sgcl::vector<int>, std::less<int>>>);
    EXPECT_EQ(q.front(), 1);
    EXPECT_EQ(s.top(), 2);
    EXPECT_EQ(pq.top(), 2);
    sgcl::concurrent::copy_on_write cow(std::string("x"));
    static_assert(std::is_same_v<decltype(cow), sgcl::concurrent::copy_on_write<std::string>>);
    EXPECT_EQ(*cow.load(), "x");
    // the task's guide checked without a task run: a spawn starts the
    // scheduler, whose objects outlive its stop and would be counted by
    // every live-object assertion of the suites after this one
    static_assert(std::is_same_v<decltype(sgcl::async::task(std::declval<sgcl::async::task<int>>())), sgcl::async::task<int>>);
}

// fill and sort_by take part only where they can work: fill with a value
// the elements are assignable from, sort_by with a key that has < (they
// were declared for anything, a wrong argument a hard error in the body,
// and a requires-expression asking for them did not compile)
namespace {
    struct Untagged {};

    struct Item {
        int rank = 0;
        Untagged tag;
    };

    template<class C, class V>
    concept Fills = requires(C& c, const V& v) { c.fill(v); };

    template<class C, class P>
    concept SortsBy = requires(C& c, P p) { c.sort_by(p); };

    // Counts the element comparisons of ==
    struct Compared {
        static inline int calls = 0;
        int v = 0;
        friend bool operator==(const Compared& a, const Compared& b) noexcept {
            ++calls;
            return a.v == b.v;
        }
    };
}

TEST(Mixins_Test, FillAndSortByOnlyWhereTheyWork) {
    static_assert(Fills<sgcl::vector<int>, int>);
    static_assert(Fills<sgcl::vector<int>, double>);
    static_assert(Fills<sgcl::vector<sgcl::string>, const char*>);
    static_assert(!Fills<sgcl::vector<int>, Untagged>);
    static_assert(!Fills<sgcl::deque<int>, sgcl::string>);
    static_assert(SortsBy<sgcl::vector<Item>, int Item::*>);
    static_assert(!SortsBy<sgcl::vector<Item>, Untagged Item::*>);
    static_assert(!SortsBy<sgcl::vector<Item>, int>);

    sgcl::vector<int> empty;
    empty.fill(3);                                       // nothing to fill
    empty.sort_by([](int x) noexcept { return -x; });
    EXPECT_TRUE(empty.empty());
    sgcl::vector<Item> items = {{3, {}}, {1, {}}, {2, {}}};
    items.sort_by(&Item::rank);
    EXPECT_EQ(items[0].rank, 1);
    EXPECT_EQ(items[2].rank, 3);
    sgcl::deque<double> d(3);
    d.fill(1);
    EXPECT_EQ(d[2], 1.0);
}

// == of two containers that know their size compares the sizes first, as
// std's do: lists of different lengths compare no element
TEST(Mixins_Test, EqualityComparesSizesFirst) {
    sgcl::list<Compared> a = {{1}, {2}, {3}};
    sgcl::list<Compared> b = {{1}, {2}, {3}, {4}};
    Compared::calls = 0;
    EXPECT_FALSE(a == b);
    EXPECT_EQ(Compared::calls, 0);
    sgcl::list<Compared> c = a;
    EXPECT_TRUE(a == c);
    EXPECT_EQ(Compared::calls, 3);
    EXPECT_TRUE(a == a);                                 // itself
    EXPECT_TRUE(sgcl::list<Compared>() == sgcl::list<Compared>());
    sgcl::forward_list<Compared> f = {{1}, {2}};        // no size: walked
    sgcl::forward_list<Compared> g = {{1}};
    EXPECT_FALSE(f == g);
}

// Boundaries (DESIGN 408)

// The members of the mixins on an empty container and on one element:
// every search misses or finds the one, the sorts and fill change nothing
// of nothing; the container's own element as the value of fill, contains,
// index_of and the bounds
TEST(Mixins_Test, EmptyOneAndTheContainersOwnElement) {
    sgcl::vector<int> none;
    EXPECT_EQ(none.find_index([](int) { return true; }), sgcl::npos);
    EXPECT_EQ(none.find_if([](int) { return true; }), nullptr);
    EXPECT_FALSE(none.exists([](int) { return true; }));
    EXPECT_TRUE(none.all([](int) { return false; }));
    EXPECT_EQ(none.count_of([](int) { return true; }), 0u);
    EXPECT_FALSE(none.contains(0));
    EXPECT_EQ(none.index_of(0), sgcl::npos);
    EXPECT_EQ(none.last_index_of(0), sgcl::npos);
    EXPECT_TRUE(none.is_sorted());
    EXPECT_FALSE(none.binary_search(0));
    EXPECT_EQ(none.sorted_index_of(0), sgcl::npos);
    EXPECT_EQ(none.lower_bound(0), none.end());
    none.sort();
    none.stable_sort();
    none.sort_by([](int x) { return -x; });
    none.fill(1);
    none.reverse();
    EXPECT_TRUE(none.empty());
    sgcl::list<std::string> one = {"only"};
    EXPECT_EQ(one.index_of("only"), 0u);
    EXPECT_EQ(one.last_index_of("only"), 0u);
    EXPECT_EQ(&one.min(), &one.max());
    EXPECT_EQ(one.find_if([](const std::string&) { return true; }), &one.front());
    one.fill(one.front());
    one.reverse();
    EXPECT_EQ(one.front(), "only");
    sgcl::vector<std::string> v = {"b", "long enough to live on the heap", "a"};
    v.fill(v[1]);
    EXPECT_EQ(v, (sgcl::vector<std::string>(3, "long enough to live on the heap")));
    v = {"a", "b", "c"};
    EXPECT_TRUE(v.contains(v[2]));
    EXPECT_EQ(v.index_of(v[1]), 1u);
    EXPECT_EQ(v.lower_bound(v[2]), v.begin() + 2);
    EXPECT_EQ(v.sorted_index_of(v[0]), 0u);
    sgcl::deque<int> d = {3, 1, 2};
    d.fill(d[1]);
    EXPECT_EQ(d, (sgcl::deque<int>{1, 1, 1}));
}
