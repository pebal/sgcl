//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ordered_map and ordered_set: the hash containers iterated in insertion
// order. The table under them is the one of map, tested in
// map.cpp; what is tested here is the order: kept by every
// operation, walked both ways, moved by to_back and to_front, and the
// same against a std::vector oracle under random operations.
#include "tests/types.h"

#include "sgcl/containers/ordered_map.h"
#include "sgcl/containers/ordered_set.h"
#include "sgcl/containers/map.h"
#include "sgcl/containers/multimap.h"
#include "sgcl/containers/set.h"
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
