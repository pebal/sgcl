//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"
#include "tests/concurrent/together.h"

#include <algorithm>
#include <atomic>
#include <climits>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {
    template<class S>
    std::vector<int> keys_of(const S& s) {
        std::vector<int> keys;
        for (auto& k : s) {
            if constexpr (requires { k.first; }) {
                keys.push_back(k.first);
            } else {
                keys.push_back(k);
            }
        }
        return keys;
    }
}

TEST(ConcurrentSortedSet_Test, InsertFindErase) {
    sgcl::concurrent::sorted_set<int> s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_EQ(s.find(1), s.end());
    auto [it, inserted] = s.insert(2);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(*it, 2);
    EXPECT_FALSE(s.insert(2).second);
    EXPECT_TRUE(s.emplace(1).second);
    int three = 3;
    EXPECT_TRUE(s.insert(three).second);
    s.insert({5, 4, 5});
    EXPECT_EQ(s.size(), 5u);
    EXPECT_TRUE(s.contains(3));
    EXPECT_EQ(s.count(3), 1u);
    EXPECT_EQ(s.count(9), 0u);
    EXPECT_EQ(keys_of(s), (std::vector<int>{1, 2, 3, 4, 5}));
    EXPECT_EQ(s.erase(3), 1u);
    EXPECT_EQ(s.erase(3), 0u);
    EXPECT_EQ(keys_of(s), (std::vector<int>{1, 2, 4, 5}));
    EXPECT_EQ(*s.lower_bound(3), 4);
    EXPECT_EQ(*s.upper_bound(4), 5);
    EXPECT_EQ(s.upper_bound(5), s.end());
    auto next = s.erase(s.find(4));
    EXPECT_EQ(*next, 5);
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_TRUE(s.insert(7).second);
    static_assert(std::is_same_v<decltype(*s.begin()), const int&>);   // const elements, as std::set
}

TEST(ConcurrentSortedSet_Test, OrderingAndTypes) {
    std::vector<int> v = {5, 1, 4, 2, 3};
    sgcl::concurrent::sorted_set<int, std::greater<int>> g(v.begin(), v.end());
    EXPECT_EQ(keys_of(g), (std::vector<int>{5, 4, 3, 2, 1}));
    sgcl::concurrent::sorted_set<std::string> s = {"b", "a", "c"};
    std::string order;
    for (auto& k : s) {
        order += k;
    }
    EXPECT_EQ(order, "abc");
    EXPECT_EQ(*s.find("b"), "b");
}

TEST(ConcurrentSortedSet_Test, ElementsAndNodesReclaimed) {
    const size_t before = collector::get_live_object_count();
    sgcl::concurrent::sorted_set<tracked_ptr<Baz>, std::less<tracked_ptr<Baz>>> s;   // ordered by address
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);   // the head
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            s.insert(make_tracked<Baz>(i));
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 201u);   // the head, 100 nodes, 100 Baz
    EXPECT_EQ(s.size(), 100u);
    off_frame([&] {
        s.clear();
        EXPECT_TRUE(s.empty());
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);
}

TEST(ConcurrentSortedSet_Test, ChurnManyThreads) {
    const int threads = 8;
    const int range = 2000;
    const int ops = 40000;
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::concurrent::sorted_set<int> s;
        sgcl::atomic<bool> bad = {false};
        std::vector<std::thread> ws;
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                std::mt19937 rng(unsigned(t + 1));
                std::uniform_int_distribution<int> key(0, range - 1);
                std::uniform_int_distribution<int> op(0, 99);
                for (int i = 0; i < ops; ++i) {
                    int k = key(rng);
                    int a = op(rng);
                    if (a < 30) {
                        auto [it, ok] = s.insert(k);
                        if (*it != k) {
                            bad = true;
                        }
                    } else if (a < 60) {
                        s.erase(k);
                    } else if (a < 95) {
                        auto it = s.find(k);
                        if (it != s.end() && *it != k) {
                            bad = true;
                        }
                    } else if (a < 99) {
                        int last = -1;
                        for (int key_ : s) {
                            if (key_ <= last) {
                                bad = true;
                            }
                            last = key_;
                        }
                    } else if (t == 0) {
                        collector::force_collect();
                    }
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_FALSE(bad.load());
        auto keys = keys_of(s);
        EXPECT_TRUE(std::is_sorted(keys.begin(), keys.end()));
        EXPECT_EQ(std::adjacent_find(keys.begin(), keys.end()), keys.end());
        EXPECT_EQ(s.size(), keys.size());
        collector::force_collect(true);
        collector::force_collect(true);
        EXPECT_EQ(collector::get_live_object_count(), before + 1u + keys.size());   // the head and a node per key
    });
    EXPECT_EQ(collector::get_live_object_count(), before);
}

// insert of a key the set holds leaves its argument as it was, as
// std::set::insert does, by rvalue and from a moved range alike
TEST(ConcurrentSortedSet_Test, InsertOfATakenKeyLeavesTheArgument) {
    const std::string long_key(64, 'k');   // past the small-string buffer: a move empties it
    sgcl::concurrent::sorted_set<std::string> s;
    EXPECT_TRUE(s.insert(long_key).second);

    std::string key = long_key;
    EXPECT_FALSE(s.insert(std::move(key)).second);
    EXPECT_EQ(key, long_key);

    std::vector<std::string> range = {long_key, std::string(64, 'z')};
    s.insert(std::make_move_iterator(range.begin()), std::make_move_iterator(range.end()));
    EXPECT_EQ(range[0], long_key);   // taken: left as it was
    EXPECT_TRUE(s.contains(std::string(64, 'z')));
    EXPECT_EQ(s.size(), 2u);
}

// Boundaries (DESIGN 408)

// The build from a range of keys that move, one key more than once: each
// key once. The build moves a key into its node, so the key it compares
// the next one with is the node's, not the one moved from
TEST(ConcurrentSortedSet_Test, RangeConstructorOfMovableKeysKeepsOneOfEach) {
    const std::string a(64, 'a'), b(64, 'b');   // past the small-string buffer: a move empties it
    std::vector<std::string> in = {b, a, b, a, a};
    sgcl::concurrent::sorted_set<std::string> s(in.begin(), in.end());
    EXPECT_EQ(s.size(), 2u);
    EXPECT_EQ((std::vector<std::string>(s.begin(), s.end())), (std::vector<std::string>{a, b}));
    sgcl::concurrent::sorted_set<std::string> l = {b, b, b};
    EXPECT_EQ(l.size(), 1u);
    EXPECT_EQ(*l.begin(), b);
    EXPECT_EQ(in, (std::vector<std::string>{b, a, b, a, a}));   // the range copied, not moved from
}

// An empty set, built from an empty range, and a set of one key: every
// lookup at the ends, the erasures of nothing
TEST(ConcurrentSortedSet_Test, EmptyAndOneKey) {
    std::vector<int> none;
    sgcl::concurrent::sorted_set<int> e(none.begin(), none.end());
    EXPECT_TRUE(e.empty());
    EXPECT_EQ(e.size(), 0u);
    EXPECT_EQ(e.begin(), e.end());
    EXPECT_EQ(e.find(0), e.end());
    EXPECT_EQ(e.lower_bound(INT_MIN), e.end());
    EXPECT_EQ(e.upper_bound(INT_MIN), e.end());
    EXPECT_EQ(e.erase(0), 0u);
    e.clear();
    EXPECT_TRUE(e.empty());
    e.insert(none.begin(), none.end());
    EXPECT_TRUE(e.empty());

    sgcl::concurrent::sorted_set<int> one = {7};
    EXPECT_EQ(one.size(), 1u);
    EXPECT_EQ(*one.lower_bound(INT_MIN), 7);
    EXPECT_EQ(*one.lower_bound(7), 7);
    EXPECT_EQ(one.lower_bound(8), one.end());
    EXPECT_EQ(*one.upper_bound(6), 7);
    EXPECT_EQ(one.upper_bound(7), one.end());
    EXPECT_EQ(one.erase(one.begin()), one.end());   // the last element: nothing after it
    EXPECT_TRUE(one.empty());
    EXPECT_EQ(one.erase(7), 0u);
}

// The least and the greatest values of the key type, at the two ends
TEST(ConcurrentSortedSet_Test, ExtremeKeys) {
    sgcl::concurrent::sorted_set<int> s = {0, INT_MAX, INT_MIN};
    EXPECT_EQ((std::vector<int>(s.begin(), s.end())), (std::vector<int>{INT_MIN, 0, INT_MAX}));
    EXPECT_EQ(*s.lower_bound(INT_MIN), INT_MIN);
    EXPECT_EQ(*s.upper_bound(INT_MIN), 0);
    EXPECT_EQ(*s.lower_bound(INT_MAX), INT_MAX);
    EXPECT_EQ(s.upper_bound(INT_MAX), s.end());
    EXPECT_EQ(s.erase(INT_MIN) + s.erase(INT_MAX), 2u);
    EXPECT_EQ(*s.begin(), 0);
}

// A key of the set's own element as the argument: insert of a key held
// finds it; the key of an element erased, still held by an iterator, is
// inserted anew as a node of its own
TEST(ConcurrentSortedSet_Test, ItsOwnKeyAsTheArgument) {
    const std::string k(64, 'k');
    sgcl::concurrent::sorted_set<std::string> s = {k};
    auto it = s.begin();
    EXPECT_FALSE(s.insert(*it).second);
    EXPECT_FALSE(s.emplace(*it).second);
    EXPECT_EQ(s.erase(*it), 1u);   // the key read from the node being erased
    EXPECT_TRUE(s.empty());
    auto [again, inserted] = s.insert(*it);   // the erased node's key, alive in the iterator
    EXPECT_TRUE(inserted);
    EXPECT_NE(again, it);
    EXPECT_EQ(*again, k);
    EXPECT_EQ(*it, k);
    EXPECT_EQ(s.erase(it), s.end());   // erased before: nothing erased, the walk goes on after it
    EXPECT_EQ(s.size(), 1u);
}

// Two threads at the last element, many rounds: of two erasures one
// returns 1; of two insertions of one key into the empty set one inserts
TEST(ConcurrentSortedSet_Test, TwoThreadsAtTheLastKey) {
    sgcl::concurrent::sorted_set<int> s;
    for (int round = 0; round < together::Rounds; ++round) {
        std::atomic<int> erased = {0}, inserted = {0};
        s.insert(round);
        together::run(2, [&](int) {
            erased += int(s.erase(round));
        });
        EXPECT_EQ(erased.load(), 1);
        EXPECT_TRUE(s.empty());
        together::run(2, [&](int) {
            inserted += int(s.insert(round).second);
        });
        EXPECT_EQ(inserted.load(), 1);
        EXPECT_EQ(s.size(), 1u);
        together::run(2, [&](int i) {   // an erasure by an iterator against one by the key
            if (i == 0) {
                auto it = s.find(round);
                if (it != s.end()) {
                    s.erase(it);
                }
            } else {
                s.erase(round);
            }
        });
        EXPECT_TRUE(s.empty());
        EXPECT_EQ(s.begin(), s.end());
    }
}
