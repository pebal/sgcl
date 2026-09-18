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
#include <string_view>
#include <thread>
#include <vector>

namespace {
    template<class M>
    std::vector<int> keys_of(const M& m) {
        std::vector<int> keys;
        for (auto& [k, v] : m) {
            keys.push_back(k);
        }
        return keys;
    }
}

TEST(ConcurrentMap_Test, RangeConstructorBuiltAtOnce) {
    // The build from a range sorts the elements and links every node
    // behind the last at each level: duplicates (the first stays, as
    // the inserts would keep it), the order of the walk, the same map
    // as the inserts build, and the map alive after: inserts, erasures,
    // lookups across the levels the build raised
    std::vector<std::pair<int, int>> in;
    std::mt19937 rng(9);
    for (int i = 0; i < 5000; ++i) {
        in.emplace_back(int(rng() % 3000), i);
    }
    std::map<int, int> o;
    for (auto& [k, v] : in) {
        o.emplace(k, v);
    }
    sgcl::concurrent_map<int, int> m(in.begin(), in.end());
    sgcl::concurrent_map<int, int> by_inserts;
    by_inserts.insert(in.begin(), in.end());
    for (auto* c : {&m, &by_inserts}) {
        EXPECT_EQ(c->size(), o.size());
        int last = -1;
        for (auto& [k, v] : *c) {
            EXPECT_LT(last, k);   // sorted, no key twice
            EXPECT_EQ(o.at(k), v);
            last = k;
        }
        EXPECT_EQ(c->find(3000), c->end());
        EXPECT_EQ(c->lower_bound(-5)->first, o.begin()->first);
    }
    for (int k = 3000; k < 6000; ++k) {
        EXPECT_TRUE(m.try_emplace(k, -k).second);
    }
    for (int k = 0; k < 6000; k += 3) {
        m.erase(k);
    }
    for (int k = 0; k < 6000; ++k) {
        EXPECT_EQ(m.contains(k), k % 3 != 0 && (k >= 3000 || o.count(k)));
    }
    sgcl::concurrent_set<int> s = {5, 3, 5, 1, 3};
    EXPECT_EQ(s.size(), 3u);
    EXPECT_EQ(*s.begin(), 1);
    std::vector<std::pair<int, int>> none;
    sgcl::concurrent_map<int, int> e(none.begin(), none.end());
    EXPECT_TRUE(e.empty());
    EXPECT_TRUE(e.try_emplace(1, 1).second);
}

TEST(ConcurrentMap_Test, InsertFindErase) {
    sgcl::concurrent_map<int, std::string> m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.find(1), m.end());
    EXPECT_FALSE(m.contains(1));

    auto [it, inserted] = m.insert({2, "two"});
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->first, 2);
    EXPECT_EQ(it->second, "two");
    auto [it2, again] = m.insert({2, "deux"});
    EXPECT_FALSE(again);
    EXPECT_EQ(it2, it);
    EXPECT_EQ(it2->second, "two");

    EXPECT_TRUE(m.emplace(1, "one").second);
    EXPECT_TRUE(m.try_emplace(3, 3, 'c').second);
    EXPECT_FALSE(m.try_emplace(3, "no").second);
    std::pair<const int, std::string> p(4, "four");
    EXPECT_TRUE(m.insert(p).second);
    EXPECT_TRUE(m.insert(std::pair{5, "five"}).second);

    EXPECT_EQ(m.size(), 5u);
    EXPECT_FALSE(m.empty());
    EXPECT_TRUE(m.contains(3));
    EXPECT_EQ(m.count(3), 1u);
    EXPECT_EQ(m.count(9), 0u);
    EXPECT_EQ(m.find(3)->second, "ccc");
    EXPECT_EQ(m.find(9), m.end());
    EXPECT_EQ(keys_of(m), (std::vector<int>{1, 2, 3, 4, 5}));

    EXPECT_EQ(m.erase(3), 1u);
    EXPECT_EQ(m.erase(3), 0u);
    EXPECT_EQ(m.erase(9), 0u);
    EXPECT_FALSE(m.contains(3));
    EXPECT_EQ(keys_of(m), (std::vector<int>{1, 2, 4, 5}));
    EXPECT_EQ(m.size(), 4u);

    m.find(4)->second = "vier";   // the mapped value through the iterator
    EXPECT_EQ(m.find(4)->second, "vier");

    m.clear();
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.begin(), m.end());
    EXPECT_TRUE(m.insert({7, "seven"}).second);   // usable after clear
    EXPECT_EQ(keys_of(m), (std::vector<int>{7}));
}

TEST(ConcurrentMap_Test, TransparentLookupWithStringKeys) {
    sgcl::concurrent_map<sgcl::string, int> m = {{"alice", 30}, {"bob", 40}, {"carol", 50}};
    sgcl::concurrent_set<sgcl::string> s = {"alice", "bob"};
    std::string_view view = "bob";                   // converts to no string: the transparent overloads
    EXPECT_EQ(m.find(view)->second, 40);
    EXPECT_EQ(std::as_const(m).find(view)->second, 40);
    EXPECT_TRUE(m.contains(view) && !m.contains(std::string_view("dave")));
    EXPECT_EQ(m.count(view), 1u);
    EXPECT_EQ(m.lower_bound(std::string_view("b"))->first, "bob");
    EXPECT_EQ(m.upper_bound(view)->first, "carol");
    EXPECT_EQ(m.upper_bound(std::string_view("carol")), m.end());
    EXPECT_TRUE(s.contains(view) && s.find(view) != s.end() && s.lower_bound(std::string_view("b")) != s.end());
    EXPECT_EQ(m.erase(view), 1u);
    EXPECT_EQ(m.erase(view), 0u);
    EXPECT_EQ(s.erase(view), 1u);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(s.size(), 1u);
    auto before = collector::get_statistics().live_bytes;   // managed memory in use: a temporary string would add to it
    int sum = 0;
    for (int i = 0; i < 10000; ++i) {
        sum += m.find("alice")->second + (int)s.contains("alice") + (int)(m.lower_bound("c") != m.end());
    }
    EXPECT_EQ(sum, 10000 * 32);
    EXPECT_LE(collector::get_statistics().live_bytes, before);   // no string made for any lookup (a sweep meanwhile can only lower the count)
}

TEST(ConcurrentMap_Test, Bounds) {
    sgcl::concurrent_map<int, int> m = {{10, 0}, {20, 0}, {30, 0}};
    EXPECT_EQ(m.lower_bound(5)->first, 10);
    EXPECT_EQ(m.lower_bound(10)->first, 10);
    EXPECT_EQ(m.lower_bound(11)->first, 20);
    EXPECT_EQ(m.lower_bound(30)->first, 30);
    EXPECT_EQ(m.lower_bound(31), m.end());
    EXPECT_EQ(m.upper_bound(5)->first, 10);
    EXPECT_EQ(m.upper_bound(10)->first, 20);
    EXPECT_EQ(m.upper_bound(29)->first, 30);
    EXPECT_EQ(m.upper_bound(30), m.end());
    const auto& cm = m;
    EXPECT_EQ(cm.lower_bound(20)->first, 20);
    EXPECT_EQ(cm.upper_bound(20)->first, 30);
    EXPECT_EQ(cm.find(20)->first, 20);
    EXPECT_EQ(cm.find(21), cm.end());
}

TEST(ConcurrentMap_Test, Ordering) {
    std::vector<std::pair<const int, int>> v = {{5, 50}, {1, 10}, {4, 40}, {2, 20}, {3, 30}};
    sgcl::concurrent_map<int, int> m(v.begin(), v.end());
    EXPECT_EQ(keys_of(m), (std::vector<int>{1, 2, 3, 4, 5}));
    int sum = 0;
    for (const auto& [k, val] : m) {
        sum += val;
    }
    EXPECT_EQ(sum, 150);

    sgcl::concurrent_map<int, int, std::greater<int>> g(v.begin(), v.end());
    EXPECT_EQ(keys_of(g), (std::vector<int>{5, 4, 3, 2, 1}));
    EXPECT_EQ(g.lower_bound(3)->first, 3);
    EXPECT_EQ(g.upper_bound(3)->first, 2);

    sgcl::concurrent_map<std::string, int> s;
    s.try_emplace("b", 2);
    s.try_emplace("a", 1);
    s.try_emplace("c", 3);
    std::string order;
    for (auto& [k, val] : s) {
        order += k;
    }
    EXPECT_EQ(order, "abc");
    EXPECT_EQ(s.find("b")->second, 2);
    EXPECT_EQ(s.key_comp()("a", "b"), true);
}

TEST(ConcurrentMap_Test, ManyKeys) {
    sgcl::concurrent_map<int, int> m;
    std::mt19937 rng(7);
    std::vector<int> keys(5000);
    for (int i = 0; i < 5000; ++i) {
        keys[size_t(i)] = i;
    }
    std::shuffle(keys.begin(), keys.end(), rng);
    for (int k : keys) {
        EXPECT_TRUE(m.try_emplace(k, k * 2).second);
    }
    EXPECT_EQ(m.size(), 5000u);
    for (int k = 0; k < 5000; ++k) {
        auto it = m.find(k);
        ASSERT_NE(it, m.end());
        EXPECT_EQ(it->second, k * 2);
    }
    std::shuffle(keys.begin(), keys.end(), rng);
    for (int i = 0; i < 2500; ++i) {
        EXPECT_EQ(m.erase(keys[size_t(i)]), 1u);
    }
    EXPECT_EQ(m.size(), 2500u);
    auto left = keys_of(m);
    EXPECT_TRUE(std::is_sorted(left.begin(), left.end()));
    for (int i = 0; i < 2500; ++i) {
        EXPECT_FALSE(m.contains(keys[size_t(i)]));
    }
    for (int i = 2500; i < 5000; ++i) {
        EXPECT_EQ(m.find(keys[size_t(i)])->second, keys[size_t(i)] * 2);
    }
}

TEST(ConcurrentMap_Test, EraseByIterator) {
    sgcl::concurrent_map<int, int> m = {{1, 1}, {2, 2}, {3, 3}};
    auto it = m.find(2);
    auto next = m.erase(it);
    EXPECT_EQ(next->first, 3);
    EXPECT_EQ(keys_of(m), (std::vector<int>{1, 3}));
    // the iterator still addresses its element: the node lives while the
    // iterator holds it, and stepping it skips what is erased
    EXPECT_EQ(it->first, 2);
    EXPECT_EQ(it->second, 2);
    ++it;
    EXPECT_EQ(it->first, 3);
    EXPECT_EQ(m.erase(m.find(3)), m.end());
    EXPECT_EQ(keys_of(m), (std::vector<int>{1}));
}

TEST(ConcurrentMap_Test, ElementsDieWithTheirNodes) {
    const size_t before = Int::counter;
    sgcl::concurrent_map<int, Int> m;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            m.try_emplace(i, i);
        }
        EXPECT_EQ(Int::counter, before + 100);
        for (int i = 0; i < 100; i += 2) {
            m.erase(i);
        }
    });
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(Int::counter, before + 50);   // the erased half died with its nodes, at the sweep
    EXPECT_EQ(m.size(), 50u);
}

TEST(ConcurrentMap_Test, NodesAndObjectsReclaimed) {
    const size_t before = collector::get_live_object_count();
    sgcl::concurrent_map<int, tracked_ptr<Baz>> m;
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);   // the head
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            m.try_emplace(i, make_tracked<Baz>(i));
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 201u);   // the head, 100 nodes, 100 Baz
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            m.erase(i);
        }
        EXPECT_TRUE(m.empty());
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);   // the nodes and their markers unlinked
    off_frame([&] {
        tracked_ptr<Baz> kept;
        m.try_emplace(1, make_tracked<Baz>(1));
        auto it = m.find(1);
        kept = it->second;
        m.erase(1);
        EXPECT_EQ(it->second->value, 1);   // held by the iterator
        it = m.end();
        EXPECT_EQ(collector::get_live_object_count(), before + 2u);   // the head and the Baz in kept
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);
}

TEST(ConcurrentMap_Test, MapInsideManagedObject) {
    struct Holder {
        sgcl::concurrent_map<int, tracked_ptr<Baz>> m;
    };
    tracked_ptr h = make_tracked<Holder>();
    h->m.try_emplace(1, make_tracked<Baz>(1));
    h->m.try_emplace(2, make_tracked<Baz>(2));
    EXPECT_EQ(h->m.find(2)->second->value, 2);
    EXPECT_EQ(h->m.size(), 2u);
}

TEST(ConcurrentMap_Test, DisjointInsertsManyThreads) {
    const int threads = 8;
    const int per_thread = 5000;
    sgcl::concurrent_map<int, tracked_ptr<Baz>> m;
    off_frame([&] {
        std::vector<std::thread> ws;
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                for (int i = 0; i < per_thread; ++i) {
                    int k = i * threads + t;
                    if (!m.try_emplace(k, make_tracked<Baz>(k)).second) {
                        ADD_FAILURE() << "key " << k << " inserted twice";
                    }
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
    });
    EXPECT_EQ(m.size(), size_t(threads * per_thread));
    auto keys = keys_of(m);
    ASSERT_EQ(keys.size(), size_t(threads * per_thread));
    for (int k = 0; k < threads * per_thread; ++k) {
        ASSERT_EQ(keys[size_t(k)], k);
        ASSERT_EQ(m.find(k)->second->value, k);
    }
}

TEST(ConcurrentMap_Test, SameKeysManyThreads) {
    const int threads = 8;
    const int n = 5000;
    sgcl::concurrent_map<int, int> m;
    sgcl::atomic<int> inserted = {0};
    off_frame([&] {
        std::vector<std::thread> ws;
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                for (int i = 0; i < n; ++i) {
                    if (m.try_emplace(i, t).second) {
                        ++inserted;
                    }
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
    });
    EXPECT_EQ(inserted.load(), n);   // one winner per key
    EXPECT_EQ(m.size(), size_t(n));
    auto keys = keys_of(m);
    for (int k = 0; k < n; ++k) {
        ASSERT_EQ(keys[size_t(k)], k);
    }
}

// Threads insert, erase and look up over a shared key range while the
// collector runs: the map stays a sorted set of live nodes whose values
// match their keys (a node freed too early would not), and it holds
// exactly the keys its lookups say once the threads are done.
TEST(ConcurrentMap_Test, ChurnManyThreads) {
    const int threads = 8;
    const int range = 2000;
    const int ops = 40000;
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::concurrent_map<int, tracked_ptr<Baz>> m;
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
                        auto [it, ok] = m.try_emplace(k, make_tracked<Baz>(k));
                        if (it->first != k || it->second->value != k) {
                            bad = true;
                        }
                    } else if (a < 60) {
                        m.erase(k);
                    } else if (a < 95) {
                        auto it = m.find(k);
                        if (it != m.end() && (it->first != k || it->second->value != k)) {
                            bad = true;
                        }
                    } else if (a < 99) {
                        int last = -1;
                        for (auto& [key_, val] : m) {   // a walk while the others modify: sorted, and every value its key's
                            if (key_ <= last || val->value != key_) {
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
        auto keys = keys_of(m);
        EXPECT_TRUE(std::is_sorted(keys.begin(), keys.end()));
        EXPECT_EQ(std::adjacent_find(keys.begin(), keys.end()), keys.end());
        size_t found = 0;
        for (int k = 0; k < range; ++k) {
            if (auto it = m.find(k); it != m.end()) {
                ++found;
                EXPECT_EQ(it->second->value, k);
            }
        }
        EXPECT_EQ(found, keys.size());
        EXPECT_EQ(m.size(), keys.size());
        collector::force_collect(true);
        collector::force_collect(true);
        EXPECT_EQ(collector::get_live_object_count(), before + 1u + 2 * keys.size());   // the head, a node and a Baz per key
    });
    EXPECT_EQ(collector::get_live_object_count(), before);
}
