//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <algorithm>
#include <atomic>
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
    sgcl::concurrent_sorted_set<int> s;
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
    sgcl::concurrent_sorted_set<int, std::greater<int>> g(v.begin(), v.end());
    EXPECT_EQ(keys_of(g), (std::vector<int>{5, 4, 3, 2, 1}));
    sgcl::concurrent_sorted_set<std::string> s = {"b", "a", "c"};
    std::string order;
    for (auto& k : s) {
        order += k;
    }
    EXPECT_EQ(order, "abc");
    EXPECT_EQ(*s.find("b"), "b");
}

TEST(ConcurrentSortedSet_Test, ElementsAndNodesReclaimed) {
    const size_t before = collector::get_live_object_count();
    sgcl::concurrent_sorted_set<tracked_ptr<Baz>, std::less<tracked_ptr<Baz>>> s;   // ordered by address
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
        sgcl::concurrent_sorted_set<int> s;
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
