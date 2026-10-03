//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ordered_map and ordered_set: the hash containers iterated in insertion
// order. The table under them is the one of map, tested in
// map.cpp; what is tested here is the order: kept by every
// operation, walked both ways, moved by to_back and to_front, and the
// same against a std::vector oracle under random operations.
#include "tests/types.h"

#include "sgcl/core/ordered_map.h"
#include "sgcl/core/ordered_set.h"
#include "sgcl/core/map.h"
#include "sgcl/core/multimap.h"
#include "sgcl/core/multiset.h"
#include "sgcl/core/set.h"
#include "sgcl/core/string.h"

#include <algorithm>
#include <random>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {
    template<class M>
    std::string keys_of(const M& m) {
        std::string s;
        for (auto&& [k, v] : m) {
            s += k;
        }
        return s;
    }

    template<class S>
    std::string values_of(const S& s) {
        std::string r;
        for (auto& v : s) {
            r += std::to_string(v);
        }
        return r;
    }

    struct Holder {
        sgcl::ordered_map<int, tracked_ptr<Baz>> map;
    };
}

TEST(OrderedMap_Test, IterationFollowsInsertionAndSurvivesRehash) {
    sgcl::ordered_map<std::string, int> m;
    static_assert(std::bidirectional_iterator<decltype(m)::iterator>);
    static_assert(std::bidirectional_iterator<decltype(m)::const_iterator>);
    EXPECT_TRUE(m.begin() == m.end() && m.empty());
    m["c"] = 1;
    m["a"] = 2;
    m["b"] = 3;
    EXPECT_EQ(keys_of(m), "cab");
    EXPECT_EQ(m.front().first, "c");
    EXPECT_EQ(m.back().first, "b");
    m["a"] = 9;                                    // present: stays where it was
    EXPECT_FALSE(m.insert({"c", 7}).second);
    EXPECT_FALSE(m.emplace("b", 8).second);
    EXPECT_FALSE(m.try_emplace("c", 6).second);
    EXPECT_FALSE(m.insert_or_assign("a", 5).second);
    EXPECT_EQ(keys_of(m), "cab");
    EXPECT_EQ(m.at("a"), 5);
    for (int i = 0; i < 1000; ++i) {              // grows and rehashes many times
        m.emplace("k" + std::to_string(i), i);
    }
    EXPECT_EQ(m.size(), 1003u);
    EXPECT_EQ(keys_of(m).substr(0, 3), "cab");
    EXPECT_EQ(m.back().first, "k999");
    int i = 0;
    for (auto it = std::next(m.begin(), 3); it != m.end(); ++it, ++i) {
        EXPECT_EQ(it->first, "k" + std::to_string(i));
    }
    std::string reversed;
    for (auto it = m.rbegin(); it != m.rend(); ++it) {
        reversed += it->first[0];
    }
    EXPECT_EQ(reversed.substr(1000), "bac");
    EXPECT_EQ(std::prev(m.end())->first, "k999");
    EXPECT_EQ(std::ranges::distance(m), 1003);
    EXPECT_TRUE(std::ranges::equal(std::views::reverse(m) | std::views::keys | std::views::take(3), std::vector<std::string>{"k999", "k998", "k997"}));
}

TEST(OrderedMap_Test, EraseTakesOutOfTheOrder) {
    sgcl::ordered_map<std::string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}, {"e", 5}};
    EXPECT_EQ(keys_of(m), "abcde");
    EXPECT_EQ(m.erase("c"), 1u);
    EXPECT_EQ(keys_of(m), "abde");
    auto next = m.erase(m.find("a"));              // the iterator after, in the order
    EXPECT_EQ(next->first, "b");
    EXPECT_EQ(m.erase(m.find("e")), m.end());      // the last: end
    EXPECT_EQ(keys_of(m), "bd");
    m.emplace("a", 1);                             // back in, at the end
    EXPECT_EQ(keys_of(m), "bda");
    EXPECT_EQ(m.erase(m.find("d"), m.end()), m.end());
    EXPECT_EQ(keys_of(m), "b");
    m.clear();
    EXPECT_TRUE(m.begin() == m.end() && m.empty());
    m["z"] = 100;                                  // usable after clear, the sentinel reused
    EXPECT_EQ(keys_of(m), "z");
    EXPECT_EQ(std::prev(m.end())->first, "z");
    for (int i = 0; i < 100; ++i) {
        m.emplace(std::to_string(i), i);
    }
    EXPECT_EQ(std::erase_if(m, [](auto& p) { return p.second % 2 == 1; }), 50u);
    EXPECT_EQ(m.size(), 51u);
    EXPECT_EQ(m.begin()->first, "z");
    EXPECT_EQ(std::next(m.begin())->first, "0");
    EXPECT_EQ(m.back().first, "98");
    auto nh = m.extract("z");                      // out of the order with its node
    EXPECT_EQ(m.begin()->first, "0");
    m.insert(std::move(nh));                       // back at the end
    EXPECT_EQ(m.back().first, "z");
}

TEST(OrderedMap_Test, ToBackAndToFrontMoveAnElement) {
    sgcl::ordered_map<std::string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
    m.to_back(m.find("a"));
    EXPECT_EQ(keys_of(m), "bca");
    m.to_front(m.find("c"));
    EXPECT_EQ(keys_of(m), "cba");
    m.to_back(m.find("a"));                        // already last: unchanged
    EXPECT_EQ(keys_of(m), "cba");
    m.to_front(m.find("c"));                       // already first: unchanged
    EXPECT_EQ(keys_of(m), "cba");
    auto it = m.find("b");
    m.to_back(it);                                 // the iterator stays valid
    EXPECT_EQ(it->first, "b");
    EXPECT_EQ(std::next(it), m.end());
    EXPECT_EQ(keys_of(m), "cab");
    sgcl::ordered_map<std::string, int> one = {{"x", 1}};
    one.to_back(one.begin());
    one.to_front(one.begin());
    EXPECT_EQ(keys_of(one), "x");
    // A cache with an eviction order: the least recently touched goes first
    sgcl::ordered_map<int, int> lru;
    auto touch = [&](int k) {
        if (auto f = lru.find(k); f != lru.end()) {
            lru.to_back(f);
            return;
        }
        if (lru.size() == 3) {
            lru.erase(lru.begin());
        }
        lru[k] = k;
    };
    for (int k : {1, 2, 3, 1, 4, 2, 5}) {
        touch(k);
    }
    EXPECT_EQ(keys_of(lru | std::views::transform([](auto& p) { return std::pair(std::to_string(p.first), p.second); })), "425");
}

TEST(OrderedMap_Test, CopyMoveSwapAndEqualityKeepTheOrder) {
    sgcl::ordered_map<std::string, int> m = {{"c", 1}, {"a", 2}, {"b", 3}};
    sgcl::ordered_map<std::string, int> copy = m;
    EXPECT_EQ(keys_of(copy), "cab");
    EXPECT_TRUE(copy == m);
    copy.to_front(copy.find("b"));
    EXPECT_EQ(keys_of(copy), "bca");
    EXPECT_TRUE(copy == m);                        // equality is by contents, not by order
    sgcl::ordered_map<std::string, int> assigned;
    assigned = copy;
    EXPECT_EQ(keys_of(assigned), "bca");
    sgcl::ordered_map<std::string, int> moved = std::move(copy);
    EXPECT_EQ(keys_of(moved), "bca");
    EXPECT_TRUE(copy.empty() && copy.begin() == copy.end());
    moved.swap(m);
    EXPECT_EQ(keys_of(m), "bca");
    EXPECT_EQ(keys_of(moved), "cab");
    sgcl::ordered_map<std::string, int> other = {{"x", 1}, {"a", 5}};
    m.merge(other);                                // the new key appended, the present one left in the source
    EXPECT_EQ(keys_of(m), "bcax");
    EXPECT_EQ(keys_of(other), "a");
    m = {{"q", 1}};
    EXPECT_EQ(keys_of(m), "q");
}

TEST(OrderedMap_Test, InsideAManagedObjectAndCollected) {
    tracked_ptr<Holder> holder = make_tracked<Holder>();
    off_frame([&] {
        for (int i = 0; i < 20; ++i) {
            holder->map[i] = make_tracked<Baz>(i);
        }
    });
    collector::force_collect(true);
    off_frame([&] {
        int expected = 0;
        for (auto& [k, v] : holder->map) {
            EXPECT_EQ(k, expected);
            EXPECT_EQ(v->value, expected);
            ++expected;
        }
        EXPECT_EQ(expected, 20);
    });
    EXPECT_EQ(collector::get_live_object_count(), 43u);   // the holder, 20 nodes, 20 Baz, the buckets, the sentinel
    off_frame([&] {
        holder->map.erase(7);
    });
    EXPECT_EQ(collector::get_live_object_count(), 41u);
    holder = nullptr;
    EXPECT_EQ(collector::get_live_object_count(), 0u);
}

TEST(OrderedMap_Test, TransparentLookupWithStringKeys) {
    sgcl::ordered_map<sgcl::string, int> m = {{"b", 2}, {"a", 1}};
    EXPECT_EQ(m.find(std::string_view("a"))->second, 1);
    EXPECT_EQ(m.at(std::string_view("b")), 2);
    m.to_back(m.find(std::string_view("b")));
    EXPECT_EQ(m.back().first, "b");
    EXPECT_EQ(m.erase(std::string_view("a")), 1u);
    EXPECT_EQ(m.size(), 1u);
}

TEST(OrderedMap_Test, StressAgainstAnOrderedOracle) {
    sgcl::ordered_map<int, int> map;
    std::vector<std::pair<int, int>> oracle;      // in insertion order
    auto oracle_find = [&](int k) { return std::find_if(oracle.begin(), oracle.end(), [k](auto& p) { return p.first == k; }); };
    std::mt19937 rng(777);
    std::uniform_int_distribution<int> key(0, 500);
    std::uniform_int_distribution<int> op(0, 99);
    for (int i = 0; i < 100000; ++i) {
        auto k = key(rng);
        auto o = op(rng);
        if (o < 40) {
            auto inserted = map.insert({k, i}).second;
            auto oit = oracle_find(k);
            ASSERT_EQ(inserted, oit == oracle.end());
            if (inserted) {
                oracle.emplace_back(k, i);
            }
        } else if (o < 55) {
            map[k] = i;
            if (auto oit = oracle_find(k); oit != oracle.end()) {
                oit->second = i;
            } else {
                oracle.emplace_back(k, i);
            }
        } else if (o < 75) {
            auto erased = map.erase(k);
            auto oit = oracle_find(k);
            ASSERT_EQ(erased, oit != oracle.end() ? 1u : 0u);
            if (oit != oracle.end()) {
                oracle.erase(oit);
            }
        } else if (o < 85) {
            if (auto it = map.find(k); it != map.end()) {
                map.to_back(it);
                auto oit = oracle_find(k);
                std::rotate(oit, oit + 1, oracle.end());
            }
        } else if (o < 90) {
            if (auto it = map.find(k); it != map.end()) {
                map.to_front(it);
                auto oit = oracle_find(k);
                std::rotate(oracle.begin(), oit, oit + 1);
            }
        } else if (o < 93 && !map.empty()) {
            map.erase(map.begin());
            oracle.erase(oracle.begin());
        } else {
            auto it = map.find(k);
            auto oit = oracle_find(k);
            ASSERT_EQ(it != map.end(), oit != oracle.end());
            if (it != map.end()) {
                ASSERT_EQ(it->second, oit->second);
            }
        }
        ASSERT_EQ(map.size(), oracle.size());
        if (i % 5000 == 4999) {
            collector::force_collect();
            size_t n = 0;
            for (auto& [mk, mv] : map) {
                ASSERT_LT(n, oracle.size());
                ASSERT_EQ(mk, oracle[n].first);
                ASSERT_EQ(mv, oracle[n].second);
                ++n;
            }
            ASSERT_EQ(n, oracle.size());
            n = oracle.size();
            for (auto it = map.rbegin(); it != map.rend(); ++it) {
                --n;
                ASSERT_EQ(it->first, oracle[n].first);
            }
            ASSERT_EQ(n, 0u);
        }
    }
    collector::force_collect(true);
    EXPECT_EQ(map.size(), oracle.size());
}

TEST(OrderedSet_Test, TheOrderOfInsertionKeptAndMoved) {
    sgcl::ordered_set<int> s = {3, 1, 2};
    static_assert(std::bidirectional_iterator<decltype(s)::iterator>);
    static_assert(std::is_same_v<decltype(s)::iterator, decltype(s)::const_iterator>);
    EXPECT_FALSE(s.insert(1).second);
    EXPECT_TRUE(s.insert(0).second);
    EXPECT_EQ(values_of(s), "3120");
    EXPECT_EQ(s.front(), 3);
    EXPECT_EQ(s.back(), 0);
    s.to_front(s.find(0));
    EXPECT_EQ(values_of(s), "0312");
    s.to_back(s.find(3));
    EXPECT_EQ(values_of(s), "0123");
    EXPECT_EQ(s.erase(1), 1u);
    EXPECT_EQ(*s.erase(s.find(0)), 2);
    EXPECT_EQ(values_of(s), "23");
    std::string reversed;
    for (auto it = s.rbegin(); it != s.rend(); ++it) {
        reversed += std::to_string(*it);
    }
    EXPECT_EQ(reversed, "32");
    for (int i = 100; i < 1100; ++i) {
        s.insert(i);
    }
    EXPECT_EQ(s.size(), 1002u);
    EXPECT_EQ(*std::next(s.begin(), 2), 100);
    EXPECT_EQ(s.back(), 1099);
    sgcl::ordered_set<int> copy = s;
    EXPECT_TRUE(std::ranges::equal(copy, s));
    EXPECT_TRUE(copy == s);
    EXPECT_EQ(std::erase_if(s, [](int v) { return v >= 100; }), 1000u);
    EXPECT_EQ(values_of(s), "23");
    s.clear();
    EXPECT_TRUE(s.empty() && s.begin() == s.end());
    sgcl::ordered_set<sgcl::string> names = {"bob", "alice"};
    EXPECT_TRUE(names.contains(std::string_view("alice")));
    EXPECT_EQ(names.front(), "bob");
}

namespace {
    template<class A, class B>
    concept Merges = requires(A& a, B& b) { a.merge(b); };

    template<class A, class B>
    concept MergesFromTemporary = requires(A& a, B&& b) { a.merge(std::move(b)); };
}

TEST(OrderedMap_Test, MergeTakesOnlyTablesOfItsOwnNodes) {
    // The nodes of an ordered table carry the order's two words before
    // the element: a merge across the kinds would relink nodes of the
    // wrong shape, so it does not compile; the same value type is not
    // enough
    using Unordered = sgcl::map<int, int>;
    using Ordered = sgcl::ordered_map<int, int>;
    using UnorderedSet = sgcl::set<int>;
    using OrderedSet = sgcl::ordered_set<int>;
    static_assert(Merges<Ordered, Ordered>);
    static_assert(Merges<Unordered, Unordered>);
    static_assert(Merges<Unordered, sgcl::multimap<int, int>>);
    static_assert(MergesFromTemporary<Unordered, sgcl::multimap<int, int>>);
    static_assert(!Merges<Unordered, Ordered>);
    static_assert(!Merges<Ordered, Unordered>);
    static_assert(!MergesFromTemporary<Ordered, Unordered>);
    static_assert(!Merges<UnorderedSet, OrderedSet>);
    static_assert(!Merges<OrderedSet, UnorderedSet>);
    Ordered a = {{1, 1}, {2, 2}};
    Ordered b = {{3, 3}, {2, 20}};
    a.merge(b);
    EXPECT_EQ(a.size(), 3u);
    EXPECT_EQ(a.back().first, 3);
    EXPECT_EQ(b.size(), 1u);
}

TEST(OrderedMap_Test, ToBackOfTheNewestAndToFrontOfTheOldestChangeNothing) {
    sgcl::ordered_map<std::string, int> m = {{"a", 1}, {"b", 2}, {"c", 3}};
    auto last = std::prev(m.end());
    m.to_back(last);                               // already last: left alone
    EXPECT_EQ(keys_of(m), "abc");
    EXPECT_EQ(last->first, "c");                   // the iterator stays valid and in place
    EXPECT_EQ(std::next(last), m.end());
    EXPECT_EQ(std::prev(last)->first, "b");
    EXPECT_EQ(std::prev(m.end()), last);
    auto first = m.begin();
    m.to_front(first);                             // already first: left alone
    EXPECT_EQ(keys_of(m), "abc");
    EXPECT_EQ(first->first, "a");
    EXPECT_EQ(m.begin(), first);
    EXPECT_EQ(std::next(first)->first, "b");
    m.to_front(last);                              // and the moves still move
    EXPECT_EQ(keys_of(m), "cab");
    m.to_back(first);
    EXPECT_EQ(keys_of(m), "cba");
    EXPECT_EQ(m.erase(last)->first, "b");
    EXPECT_EQ(keys_of(m), "ba");
    sgcl::ordered_set<int> s = {1, 2, 3};
    auto newest = std::prev(s.end());
    s.to_back(newest);
    s.to_front(s.begin());
    EXPECT_EQ(values_of(s), "123");
    EXPECT_EQ(*newest, 3);
    EXPECT_EQ(std::next(newest), s.end());
}

// value_or(key, fallback): the value under the key, or the fallback, by value;
// a key of another type through the transparent lookup; a tracked value
// comes back as the same object
TEST(OrderedMap_Test, ValueOrWithAFallback) {
    sgcl::ordered_map<sgcl::string, int> m = {{"a", 1}, {"b", 2}};
    EXPECT_EQ(m.value_or("a", 0), 1);
    EXPECT_EQ(m.value_or("z", -1), -1);
    EXPECT_EQ(m.value_or(std::string_view("b"), 0), 2);
    EXPECT_EQ(m.value_or(std::string_view("zz"), 7), 7);
    const auto& c = m;
    EXPECT_EQ(c.value_or("b", 0), 2);
    sgcl::ordered_map<int, sgcl::tracked_ptr<int>> p;
    sgcl::tracked_ptr<int> one = sgcl::make_tracked<int>(1);
    sgcl::tracked_ptr<int> none = sgcl::make_tracked<int>(0);
    p.try_emplace(1, one);
    EXPECT_EQ(p.value_or(1, none), one);
    EXPECT_EQ(p.value_or(2, none), none);
    static_assert(std::is_same_v<decltype(m.value_or("a", 0)), int>);
}

// count() of a present key is 1 and of an absent one 0, whatever the
// insertion order does to the chains: it walked from the node to the next
// in insertion order along the bucket's chain, which need not reach it,
// and ran off the chain's end (found by tests/containers/fuzz/containers_fuzz.cpp)
TEST(OrderedMap_Test, CountFollowsNoOrderAlongTheChain) {
    sgcl::ordered_map<int, int> m;
    sgcl::ordered_set<int> s;
    for (int k = 0; k < 200; ++k) {
        const int key = (k * 37) % 211;
        m.insert({key, k});
        s.insert(key);
    }
    for (int k = 0; k < 211; k += 3) {
        m.erase(k);
        s.erase(k);
    }
    for (int k = 0; k < 250; ++k) {
        const size_t want = m.find(k) != m.end() ? 1 : 0;
        EXPECT_EQ(m.count(k), want) << k;
        EXPECT_EQ(s.count(k), want) << k;
        EXPECT_EQ(m.contains(k), want == 1) << k;
        auto [first, last] = s.equal_range(k);
        EXPECT_EQ(size_t(std::distance(first, last)), want) << k;
    }
}

// A merge into an ordered table appends the source's nodes in the source's
// order, the order of its insertions, as a walk of it gives them
TEST(OrderedSet_Test, AMergeAppendsInTheSourcesOrder) {
    sgcl::ordered_set<int> from = {5, 1, 9, 3, 7, 2, 8};
    sgcl::ordered_set<int> into = {4};
    into.merge(from);
    std::vector<int> order(into.begin(), into.end());
    EXPECT_EQ(order, (std::vector<int>{4, 5, 1, 9, 3, 7, 2, 8}));
    EXPECT_TRUE(from.empty());
    sgcl::ordered_map<int, int> m = {{3, 0}};
    sgcl::ordered_map<int, int> n = {{9, 1}, {3, 1}, {1, 1}, {7, 1}};
    m.merge(n);
    std::vector<int> keys;
    for (auto& [k, v] : m) {
        keys.push_back(k);
    }
    EXPECT_EQ(keys, (std::vector<int>{3, 9, 1, 7}));
    EXPECT_EQ(n.size(), 1u);   // the taken key stays behind
}

namespace {
    // A predicate with both forms: erase_if of a set must take the const one
    struct BothForms {
        bool operator()(std::string&) const noexcept { return true; }
        bool operator()(const std::string&) const noexcept { return false; }
    };
}

// erase_if of a set gives the predicate the key as const, as its
// iterators do: a key changed in place would no longer hash to its bucket
TEST(OrderedSet_Test, EraseIfGivesASetsKeyAsConst) {
    sgcl::ordered_set<std::string> ordered = {"a", "b"};
    EXPECT_EQ(erase_if(ordered, BothForms()), 0u);
    sgcl::set<std::string> plain = {"a", "b"};
    EXPECT_EQ(erase_if(plain, BothForms()), 0u);
    sgcl::multiset<std::string> multi = {"a", "a"};
    EXPECT_EQ(erase_if(multi, BothForms()), 0u);
    sgcl::ordered_map<std::string, int> map = {{"a", 1}};
    EXPECT_EQ(erase_if(map, [](std::pair<const std::string, int>& e) { return ++e.second == 2; }), 1u);   // a map's value may change
}

// The public surface of the hash containers as std's: the flag the shared
// table is built on is not a member of theirs, and insert_return_type is
// declared where a node handle's insert returns it, the containers of
// unique keys (std::unordered_multimap has none either)
namespace {
    template<class C>
    concept HasUniqueFlag = requires { C::unique; };

    template<class C>
    concept HasInsertReturnType = requires { typename C::insert_return_type; };
}

TEST(HashContainers_Test, PublicSurfaceAsStd) {
    static_assert(!HasUniqueFlag<sgcl::map<int, int>>);
    static_assert(!HasUniqueFlag<sgcl::multimap<int, int>>);
    static_assert(!HasUniqueFlag<sgcl::set<int>>);
    static_assert(!HasUniqueFlag<sgcl::multiset<int>>);
    static_assert(!HasUniqueFlag<sgcl::ordered_map<int, int>>);
    static_assert(!HasUniqueFlag<sgcl::ordered_set<int>>);
    static_assert(HasInsertReturnType<sgcl::map<int, int>>);
    static_assert(HasInsertReturnType<sgcl::set<int>>);
    static_assert(HasInsertReturnType<sgcl::ordered_map<int, int>>);
    static_assert(HasInsertReturnType<sgcl::ordered_set<int>>);
    static_assert(!HasInsertReturnType<sgcl::multimap<int, int>>);
    static_assert(!HasInsertReturnType<sgcl::multiset<int>>);

    sgcl::map<int, int> m = {{1, 10}};
    sgcl::multimap<int, int> mm = {{1, 10}};
    static_assert(std::is_same_v<decltype(m.insert(m.extract(1))), sgcl::map<int, int>::insert_return_type>);
    static_assert(std::is_same_v<decltype(mm.insert(mm.extract(1))), sgcl::multimap<int, int>::iterator>);
    auto result = m.insert(m.extract(1));
    EXPECT_TRUE(result.inserted);
    EXPECT_EQ(result.position->second, 10);
    EXPECT_TRUE(result.node.empty());
}

// A map's node handle has a key and a mapped value, a set's a value, as
// std's: no member of the other kind, no void mapped_type, and mapped()
// a plain function (one class for both showed through)
namespace {
    template<class H>
    concept MapHandleShape = requires(H& h) {
        typename H::key_type;
        typename H::mapped_type;
        &H::key;
        &H::mapped;
        h.swap(h);
    } && !requires(H& h) { typename H::value_type; } && !requires(H& h) { h.value(); };

    template<class H>
    concept SetHandleShape = requires(H& h) {
        typename H::value_type;
        &H::value;
        h.swap(h);
    } && !requires { typename H::mapped_type; } && !requires { typename H::key_type; }
      && !requires(H& h) { h.key(); } && !requires(H& h) { h.mapped(); };
}

TEST(HashContainers_Test, NodeHandleShapeAsStd) {
    static_assert(MapHandleShape<sgcl::map<int, std::string>::node_type>);
    static_assert(MapHandleShape<sgcl::ordered_map<int, std::string>::node_type>);
    static_assert(SetHandleShape<sgcl::set<int>::node_type>);
    static_assert(SetHandleShape<sgcl::ordered_set<int>::node_type>);
    static_assert(std::is_same_v<sgcl::map<int, std::string>::node_type::mapped_type, std::string>);
    static_assert(std::is_same_v<sgcl::set<std::string>::node_type::value_type, std::string>);
    static_assert(std::is_same_v<sgcl::map<int, int>::node_type, sgcl::multimap<int, int>::node_type>);
    static_assert(std::is_same_v<sgcl::set<int>::node_type, sgcl::multiset<int>::node_type>);
    static_assert(std::is_nothrow_move_constructible_v<sgcl::set<int>::node_type>);
    static_assert(!std::is_copy_constructible_v<sgcl::map<int, int>::node_type>);
}

// The edges of a hash node handle: default-constructed and moved-from
// handles are empty, swap of two empty ones and of a handle with itself,
// a key changed out of the table hashed anew, every element destroyed once
TEST(HashContainers_Test, NodeHandleEdges) {
    using Handle = sgcl::map<int, Int>::node_type;
    Handle a;
    Handle b;
    EXPECT_TRUE(a.empty());
    EXPECT_FALSE(bool(a));
    a.swap(b);
    swap(a, b);
    EXPECT_TRUE(a.empty() && b.empty());

    auto before = Int::counter;
    {
        sgcl::map<int, Int> m = {{1, 10}, {2, 20}};
        Handle c = m.extract(1);
        Handle d = std::move(c);
        EXPECT_TRUE(c.empty());
        EXPECT_EQ((int)d.mapped(), 10);
        d.swap(d);
        swap(d, d);
        EXPECT_EQ(d.key(), 1);
        EXPECT_EQ((int)d.mapped(), 10);
        d.key() = 7;                                     // out of the table the key may change
        EXPECT_TRUE(m.insert(std::move(d)).inserted);
        EXPECT_EQ((int)m.at(7), 10);
        EXPECT_FALSE(m.contains(1));
        Handle e = m.extract(2);
        e = Handle();                                    // the element dies with the handle's assignment
        EXPECT_EQ(Int::counter, before + 1);
    }
    EXPECT_EQ(Int::counter, before);

    sgcl::ordered_set<std::string> s = {"a", "b"};
    auto f = s.extract("a");
    sgcl::ordered_set<std::string>::node_type g;
    swap(f, g);
    EXPECT_TRUE(f.empty());
    g.value() = "c";
    s.insert(std::move(g));
    EXPECT_EQ(std::vector<std::string>(s.begin(), s.end()), (std::vector<std::string>{"b", "c"}));
}

// merge takes the nodes of a table whose node handle is this one's, as
// std's: a map from a map or a multimap of the same Key and T, a set from
// a set or a multiset of the same Key, whatever the hash; not a set of
// pairs from a map, whose elements have the same type
namespace {
    struct PairHash {
        size_t operator()(const std::pair<const int, int>& p) const noexcept {
            return std::hash<int>{}(p.first) ^ std::hash<int>{}(p.second);
        }
    };

    struct ModHash {
        size_t operator()(int x) const noexcept {
            return size_t(x % 3);
        }
    };

    // A transparent lookup whose hash of a view is noexcept and whose
    // equality with one is not: bucket() calls the hash alone
    struct ViewHash {
        using is_transparent = void;
        size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
    };

    struct ViewEqual {
        using is_transparent = void;
        bool operator()(const std::string& a, const std::string& b) const noexcept { return a == b; }
        bool operator()(const std::string& a, std::string_view b) const { return a == b; }
        bool operator()(std::string_view a, const std::string& b) const { return a == b; }
    };
}

TEST(HashContainers_Test, MergeTakesTheSameKindOfNodes) {
    using PairSet = sgcl::set<std::pair<const int, int>, PairHash>;
    static_assert(!Merges<PairSet, sgcl::map<int, int>>);
    static_assert(!Merges<sgcl::map<int, int>, PairSet>);
    static_assert(!Merges<sgcl::map<int, int>, sgcl::map<int, long>>);
    static_assert(!Merges<sgcl::map<int, int>, sgcl::ordered_map<int, int>>);
    static_assert(Merges<sgcl::map<int, int>, sgcl::multimap<int, int, ModHash>>);
    static_assert(Merges<sgcl::multiset<int>, sgcl::set<int, ModHash>>);
    static_assert(Merges<sgcl::ordered_set<int>, sgcl::ordered_set<int, ModHash>>);

    sgcl::map<int, int> m = {{1, 1}};
    sgcl::multimap<int, int, ModHash> mm = {{1, 2}, {2, 2}, {2, 3}};
    m.merge(mm);                                         // one 2 comes over; 1 and the other 2 stay
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(mm.size(), 2u);
    m.merge(m);                                          // itself: nothing moves
    EXPECT_EQ(m.size(), 2u);
    sgcl::multimap<int, int, ModHash> empty;
    m.merge(empty);
    EXPECT_EQ(m.size(), 2u);
    empty.merge(m);                                      // into an empty table, which has no buckets yet
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(empty.size(), 2u);
}

TEST(HashContainers_Test, BucketOfAViewCallsTheHashAlone) {
    sgcl::map<std::string, int, ViewHash, ViewEqual> m = {{"a", 1}};
    std::string_view a = "a";
    static_assert(noexcept(m.bucket(a)));
    static_assert(!noexcept(m.find(a)));
    EXPECT_EQ(m.bucket(a), m.bucket(std::string("a")));
}

// The reads of mixin::lookup are as noexcept as the map's find with that
// key: by the key type always, by a view only when the hash and the
// equality take one without throwing (they called a throwing equality
// inside noexcept, which ended the program)
TEST(HashContainers_Test, LookupReadsAsNoexceptAsFind) {
    sgcl::map<std::string, int, ViewHash, ViewEqual> m = {{"a", 1}};
    std::string_view a = "a";
    std::string key = "a";
    static_assert(!noexcept(m.try_get(a)));
    static_assert(!noexcept(std::as_const(m).try_get(a)));
    static_assert(!noexcept(m.contains_key(a)));
    static_assert(!noexcept(m.get(a)));
    static_assert(!noexcept(m.value_or(a, 0)));
    static_assert(noexcept(m.try_get(key)));
    static_assert(noexcept(std::as_const(m).try_get(key)));
    static_assert(noexcept(m.contains_key(key)));
    static_assert(noexcept(m.get(key)));
    static_assert(noexcept(m.value_or(key, 0)));
    sgcl::multimap<std::string, int, ViewHash, ViewEqual> mm = {{"a", 1}, {"a", 2}};
    static_assert(!noexcept(mm.values_of(a)));
    static_assert(noexcept(mm.values_of(key)));

    EXPECT_EQ(*m.try_get(a), 1);
    EXPECT_EQ(m.get(a), 1);
    EXPECT_TRUE(m.contains_key(a));
    std::string_view absent = "zz";
    EXPECT_EQ(m.try_get(absent), nullptr);
    EXPECT_FALSE(m.get(absent).has_value());
    EXPECT_EQ(m.value_or(absent, 7), 7);
    EXPECT_FALSE(m.contains_key(absent));
    EXPECT_EQ(std::ranges::distance(mm.values_of(a)), 2);
    EXPECT_TRUE(std::ranges::empty(mm.values_of(absent)));
    sgcl::map<std::string, int, ViewHash, ViewEqual> empty;  // no buckets yet
    EXPECT_EQ(empty.try_get(a), nullptr);
    EXPECT_FALSE(empty.contains_key(key));
    EXPECT_EQ(empty.value_or(a, -1), -1);
}
