//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The mixins and the concepts (sgcl/core/mixin/): what a container
// declares by its bases, what a concept asks, a method that exists only
// for elements that allow it, a container hiding a mixin's answer with a
// better one, a class of the user's own, the adapters for std
#include "sgcl/sgcl.h"
#include "tests/types.h"

#include <list>
#include <string>
#include <utility>
#include <vector>

namespace {
    using namespace sgcl;

    struct Plain {   // no ==, no <
        int a;
    };

    struct LessOnly {   // < alone, as a std container orders
        int a;
        bool operator<(const LessOnly& o) const noexcept { return a < o.a; }
    };

    // A class of the user's own: a range of the library by declaration
    template<class T>
    class Ring
    : public mixin::bidirectional<Ring<T>>
    , public mixin::comparable<Ring<T>>
    , public mixin::enumerable<Ring<T>>
    , public mixin::equatable<Ring<T>>
    , public mixin::ordered<Ring<T>>
    , public mixin::random_access<Ring<T>>
    , public mixin::sequence<Ring<T>> {
    public:
        using value_type = T;
        Ring(std::initializer_list<T> l) : _v(l) {}
        T* begin() noexcept { return _v.data(); }
        T* end() noexcept { return _v.data() + _v.size(); }
        const T* begin() const noexcept { return _v.data(); }
        const T* end() const noexcept { return _v.data() + _v.size(); }
    private:
        std::vector<T> _v;
    };

    // The concepts as parameters, in the abbreviated form
    size_t odd(const req::enumerable auto& r) { return r.count_of([](int x) { return x % 2 != 0; }); }
    decltype(auto) largest(const req::ordered auto& r) { return r.max(); }
    void sort_in_place(req::sequence auto& r) requires req::ordered<decltype(r)> { r.sort(); }
    bool has_key(const req::lookup auto& m, int k) { return m.contains_key(k); }

    template<class R, class... A>
    concept HasContains = requires(const R& r, A... a) { r.contains(a...); };
    template<class R>
    concept HasMin = requires(const R& r) { r.min(); };
    template<class R>
    concept HasSort = requires(R& r) { r.sort(); };
    template<class M>
    concept HasValuesOf = requires(M& m) { m.values_of(1); };
}

TEST(Mixin_Tests, WhatTheContainersDeclare) {
    static_assert(req::enumerable<vector<int>> && req::bidirectional<vector<int>> && req::random_access<vector<int>> && req::contiguous<vector<int>>);
    static_assert(req::sequence<vector<int>> && req::ordered<vector<int>> && !req::lookup<vector<int>>);
    static_assert(req::contiguous<array<int, 3>> && req::ordered<array<int, 3>> && req::sequence<array<int, 3>> && req::contiguous<dynamic_array<int>> && req::sequence<dynamic_array<int>>);
    static_assert(req::random_access<deque<int>> && !req::contiguous<deque<int>>);
    static_assert(req::bidirectional<list<int>> && !req::random_access<list<int>> && req::sequence<list<int>>);
    static_assert(req::enumerable<forward_list<int>> && !req::bidirectional<forward_list<int>>);
    static_assert(req::bidirectional<sorted_set<int>> && !req::ordered<sorted_set<int>> && !req::sequence<sorted_set<int>> && !req::lookup<sorted_set<int>>);
    static_assert(req::lookup<sorted_map<int, int>> && req::lookup<sorted_multimap<int, int>> && req::lookup<map<int, int>> && req::lookup<ordered_map<int, int>> && req::lookup<immutable::map<int, int>>);
    static_assert(req::enumerable<set<int>> && !req::bidirectional<set<int>>);
    static_assert(req::random_access<immutable::vector<int>> && req::ordered<immutable::vector<int>> && !req::sequence<immutable::vector<int>>);
    static_assert(req::enumerable<immutable::list<int>> && req::ordered<immutable::list<int>> && req::enumerable<immutable::map<int, int>> && req::enumerable<immutable::set<int>>);
    static_assert(req::immutable<immutable::vector<int>> && req::immutable<immutable::list<int>> && req::immutable<immutable::map<int, int>> && req::immutable<immutable::set<int>>);
    static_assert(!req::immutable<vector<int>> && !req::immutable<slice<const int>> && !req::immutable<sorted_set<int>>);   // not written is not immutable
    static_assert(req::contiguous<slice<int>> && req::sequence<slice<int>> && req::contiguous<slice<const int>> && !req::sequence<slice<const int>>);
    static_assert(req::contiguous<range<int*>> && req::sequence<range<int*>> && req::random_access<range<detail::counter<int>>> && !req::sequence<range<detail::counter<int>>>);
    static_assert(!req::enumerable<string>);   // mixin::text, not a range of the library: as_slice() is
    static_assert(req::enumerable<Ring<int>> && req::ordered<Ring<int>> && req::sequence<Ring<int>>);
}

TEST(Mixin_Tests, AConceptOfAContainerIsNominal) {
    static_assert(!req::enumerable<std::vector<int>> && !req::enumerable<std::list<int>> && !req::enumerable<int[3]>);
    // the adapter is how std enters
    std::vector<int> sv = {3, 1, 2};
    range r(sv.begin(), sv.end());
    static_assert(req::contiguous<decltype(r)> && req::sequence<decltype(r)> && req::ordered<decltype(r)>);
    EXPECT_EQ(odd(r), 2u);
    EXPECT_EQ(largest(r), 3);
    sort_in_place(r);
    EXPECT_TRUE(r.is_sorted() && sv[0] == 1);
    std::list<int> sl = {1, 2};
    range lr(sl.begin(), sl.end());
    static_assert(req::bidirectional<decltype(lr)> && !req::random_access<decltype(lr)> && !HasSort<decltype(lr)>);
    EXPECT_EQ(lr.last_index_of(2), 1u);
}

TEST(Mixin_Tests, AConceptOfAValueIsStructuralToo) {
    static_assert(req::equatable<int> && req::comparable<int> && req::comparable<double>);
    static_assert(req::equatable<std::string> && req::comparable<std::string> && req::comparable<std::pair<int, int>>);
    static_assert(req::equatable<string> && req::comparable<string> && req::comparable<vector<int>> && req::equatable<std::vector<int>>);
    static_assert(!req::equatable<Plain> && !req::comparable<Plain>);
    static_assert(!req::equatable<LessOnly> && req::comparable<LessOnly>);   // as the standard containers order: < alone
    static_assert(req::comparable<Int>);   // the test type, by <=>
}

TEST(Mixin_Tests, AMethodExistsOnlyForElementsThatAllowIt) {
    static_assert(HasContains<vector<int>, int> && HasMin<vector<int>> && HasSort<vector<int>>);
    static_assert(!HasContains<vector<Plain>, Plain> && !HasMin<vector<Plain>> && !HasSort<vector<Plain>>);
    static_assert(!req::ordered<vector<Plain>> && req::enumerable<vector<Plain>> && req::sequence<vector<Plain>>);
    static_assert(HasMin<vector<LessOnly>> && HasSort<vector<LessOnly>> && !HasContains<vector<LessOnly>, LessOnly>);
    static_assert(!HasSort<immutable::vector<int>> && HasMin<immutable::vector<int>>);   // ordered, not written in place
    static_assert(!HasSort<slice<const int>> && HasSort<slice<int>>);
    vector<Plain> v = {Plain{1}, Plain{2}};
    EXPECT_TRUE(v.exists([](const Plain& p) { return p.a == 2; }));
    EXPECT_EQ(v.find_if([](const Plain& p) { return p.a == 2; })->a, 2);
    EXPECT_EQ(v.min([](const Plain& a, const Plain& b) { return a.a < b.a; }).a, 1);   // a comparator asks nothing
    v.sort([](const Plain& a, const Plain& b) { return a.a > b.a; });
    EXPECT_EQ(v[0].a, 2);
    v.sort_by(&Plain::a);
    EXPECT_EQ(v[0].a, 1);
    vector<LessOnly> lo = {{2}, {1}};
    lo.sort();
    EXPECT_EQ(lo[0].a, 1);
    vector<LessOnly> lo13 = {{1}, {3}};
    EXPECT_TRUE(lo < lo13);   // <=> from < alone, a weak ordering
    static_assert(std::is_same_v<decltype(lo <=> lo), std::weak_ordering>);
}

TEST(Mixin_Tests, TheContainersCompareByTheirMixins) {
    vector<int> a = {1, 2}, b = {1, 3};
    EXPECT_TRUE(a == a && a != b && a < b && (a <=> b) < 0);
    list<int> la = {1, 2}, lb = {1, 3};
    EXPECT_TRUE(la == la && la < lb);
    deque<int> da = {1, 2}, db = {1, 2}, dc = {2};
    EXPECT_TRUE(da == db && da < dc);
    forward_list<int> fa = {1, 2, 3}, fb = {1, 2};
    EXPECT_TRUE(fa != fb && fb < fa);
    sorted_set<int> sa = {1, 2}, sb = {1, 3}, sc = {2, 1};
    EXPECT_TRUE(sa == sc && sa < sb);
    sorted_map<int, int> ma = {{1, 1}}, mb = {{1, 2}}, mc = {{1, 1}};
    EXPECT_TRUE(ma < mb && ma == mc);
    slice<const int> s = a.as_slice(), t = b.as_slice();
    EXPECT_TRUE(s == a.as_slice() && s < t);
    array<int, 2> x = {1, 2}, y = {1, 3}, z = {1, 2};
    EXPECT_TRUE(x < y && x == z);
    Ring<int> r1 = {1, 2}, r2 = {1, 3};
    EXPECT_TRUE(r1 == r1 && r1 < r2);
}

TEST(Mixin_Tests, AContainerHidesTheMixinWithABetterAnswer) {
    sorted_set<int> s = {3, 1, 2};
    EXPECT_EQ(s.min(), 1);   // *begin(), not a walk
    EXPECT_EQ(s.max(), 3);
    EXPECT_TRUE(s.contains(2));   // by the key
    EXPECT_FALSE(s.contains(9));
    EXPECT_EQ(s.count_of([](int x) { return x > 1; }), 2u);   // the mixin's, no better answer
    EXPECT_EQ(s.index_of(3), 2u);   // the position in the order
    sorted_map<int, std::string> m = {{2, "b"}, {1, "a"}};
    EXPECT_EQ(m.min().first, 1);
    EXPECT_TRUE(m.contains(1) && !m.contains(3));
    EXPECT_TRUE(m.exists([](const auto& kv) { return kv.second == "b"; }));
    set<int> us = {1, 2};
    EXPECT_TRUE(us.contains(1) && us.all([](int x) { return x > 0; }));
    immutable::set<int> is = immutable::set<int>().insert(1).insert(2);
    EXPECT_TRUE(is.contains(2) && is.exists([](int x) { return x == 1; }));
}

TEST(Mixin_Tests, LookupOnEveryMap) {
    sorted_map<int, std::string> m = {{1, "one"}, {2, "two"}};
    EXPECT_EQ(*m.get(1), "one");
    EXPECT_FALSE(m.get(3));
    EXPECT_EQ(*m.try_get(2), "two");
    EXPECT_EQ(m.try_get(3), nullptr);
    EXPECT_EQ(m.value_or(3, "none"), "none");
    EXPECT_EQ(m.value_or(1, "none"), "one");
    EXPECT_TRUE(m.contains_key(2) && !m.contains_key(3));
    EXPECT_TRUE(has_key(m, 1));
    int keys = 0;
    for (int k : m.keys()) {
        keys += k;
    }
    EXPECT_EQ(keys, 3);
    size_t letters = 0;
    for (const auto& v : m.values()) {
        letters += v.size();
    }
    EXPECT_EQ(letters, 6u);
    *m.try_get(1) = "uno";
    EXPECT_EQ(m.at(1), "uno");
    sorted_multimap<int, int> mm = {{1, 10}, {1, 11}, {2, 20}};
    int sum = 0;
    for (int v : mm.values_of(1)) {
        sum += v;
    }
    EXPECT_EQ(sum, 21);
    EXPECT_EQ(*mm.get(2), 20);
    map<std::string, int> um = {{"a", 1}};
    EXPECT_EQ(um.get("a"), 1);
    ordered_map<int, int> om = {{5, 50}};
    EXPECT_EQ(om.value_or(6, -1), -1);
    // the immutable map: find gives a pointer, not an iterator; the mixin tells the two apart
    immutable::map<int, std::string> imm = immutable::map<int, std::string>().insert(1, "one").insert(2, "two");
    static_assert(req::lookup<decltype(imm)> && !req::lookup<immutable::set<int>>);
    EXPECT_EQ(*imm.get(1), "one");
    EXPECT_FALSE(imm.get(3));
    EXPECT_EQ(*imm.try_get(2), "two");
    EXPECT_EQ(imm.value_or(3, "none"), "none");
    EXPECT_TRUE(imm.contains_key(2) && has_key(imm, 1));
    int ikeys = 0;
    for (int k : imm.keys()) {
        ikeys += k;
    }
    EXPECT_EQ(ikeys, 3);
    static_assert(!HasValuesOf<decltype(imm)> && HasValuesOf<sorted_multimap<int, int>>);   // no equal_range: no values_of
}

TEST(Mixin_Tests, TheOrderOfARange) {
    vector<int> v = {3, 1, 2};
    EXPECT_FALSE(v.is_sorted());
    v.sort();
    EXPECT_TRUE(v.is_sorted() && v.binary_search(2) && v.sorted_index_of(3) == 2u && !v.binary_search(9));
    EXPECT_EQ(*v.lower_bound(2), 2);
    EXPECT_EQ(*v.upper_bound(2), 3);
    v.stable_sort([](int a, int b) { return a > b; });
    EXPECT_EQ(v[0], 3);
    EXPECT_TRUE(v.is_sorted([](int a, int b) { return a > b; }));
    immutable::vector<int> iv = immutable::vector<int>().push_back(1).push_back(2);
    EXPECT_TRUE(iv.is_sorted() && iv.binary_search(2) && iv.max() == 2);
    immutable::list<int> il = {1, 2, 3};
    EXPECT_TRUE(il.is_sorted() && il.min() == 1 && il.contains(3) && il.index_of(2) == 1u);
    EXPECT_EQ(range(5).max(), 4);   // a value, the counter's
    EXPECT_TRUE(range(5).contains(3) && range(5).is_sorted() && range(5).binary_search(4));
    array<int, 3> a = {3, 1, 2};
    a.sort();
    EXPECT_TRUE(a.is_sorted() && a.binary_search(2));
    constexpr array<int, 2> ca = {1, 2};
    static_assert(ca.contains(2) && ca.min() == 1 && ca.is_sorted() && ca.find_if([](int x) { return x > 1; }) != nullptr);
    slice<int> s = v.as_slice(1);
    s.sort();
    EXPECT_TRUE(s.is_sorted() && v[1] == 1);
    slice<const char> t = string("cab").as_slice();
    EXPECT_TRUE(t.contains('a') && t.contains("ab") && !t.contains("x") && t.min() == 'a' && !t.is_sorted());
}

TEST(Mixin_Tests, TheMixinsHaveNoStateAndAreNotParameters) {
    static_assert(std::is_empty_v<mixin::enumerable<Ring<int>>> && std::is_empty_v<mixin::lookup<sorted_map<int, int>>> && std::is_empty_v<mixin::contiguous<vector<int>>> && std::is_empty_v<mixin::immutable<immutable::vector<int>>>);
    static_assert(sizeof(Ring<int>) == sizeof(std::vector<int>));
    static_assert(sizeof(slice<int>) == 3 * sizeof(void*));
    static_assert(!std::is_default_constructible_v<mixin::enumerable<Ring<int>>>);   // protected: a base only
}
