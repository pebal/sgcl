//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {
    // The count with the thread's block of cells let go of first
    // (tests/gc_tracked_ptr.cpp): a gc::tracked_ptr in unmanaged memory
    // holds a cell of a managed block, and a block is an object of the
    // count until every cell of it is given back.
    SGCL_ALWAYS_INLINE size_t live_without_cells() {
        sgcl::detail::cell_allocator.release();
        collector::clear_stack(SIZE_MAX);
        collector::force_collect(true);
        return collector::get_live_object_count();
    }

    template<class M>
    std::vector<int> sorted_keys(const M& m) {
        std::vector<int> keys;
        for (auto& e : m) {
            if constexpr (requires { e.first; }) {
                keys.push_back(e.first);
            } else {
                keys.push_back(e);
            }
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    // every key in one bucket: the walk through the elements of one split key
    struct OneHash {
        size_t operator()(int) const noexcept {
            return 7;
        }
    };

    struct Mod10 {
        bool operator()(int a, int b) const noexcept {
            return a % 10 == b % 10;
        }
    };

    struct HashMod10 {
        size_t operator()(int k) const noexcept {
            return size_t(k % 10);
        }
    };
}

TEST(ConcurrentUnorderedMap_Test, InsertFindErase) {
    sgcl::concurrent_unordered_map<int, std::string> m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.find(1), m.end());
    EXPECT_FALSE(m.contains(1));
    EXPECT_EQ(m.bucket_count(), 16u);

    auto [it, inserted] = m.insert({2, "two"});
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->first, 2);
    EXPECT_EQ(it->second, "two");
    auto [it2, again] = m.insert({2, "deux"});
    EXPECT_FALSE(again);
    EXPECT_EQ(it2, it);
    EXPECT_TRUE(m.emplace(1, "one").second);
    EXPECT_TRUE(m.try_emplace(3, 3, 'c').second);
    EXPECT_FALSE(m.try_emplace(3, "no").second);
    std::pair<const int, std::string> p(4, "four");
    EXPECT_TRUE(m.insert(p).second);
    EXPECT_TRUE(m.insert(std::pair{5, "five"}).second);
    m.insert({{6, "six"}, {7, "seven"}});

    EXPECT_EQ(m.size(), 7u);
    EXPECT_TRUE(m.contains(3));
    EXPECT_EQ(m.count(3), 1u);
    EXPECT_EQ(m.count(9), 0u);
    EXPECT_EQ(m.find(3)->second, "ccc");
    EXPECT_EQ(sorted_keys(m), (std::vector<int>{1, 2, 3, 4, 5, 6, 7}));

    EXPECT_EQ(m.erase(3), 1u);
    EXPECT_EQ(m.erase(3), 0u);
    EXPECT_EQ(m.erase(9), 0u);
    EXPECT_FALSE(m.contains(3));
    EXPECT_EQ(m.size(), 6u);
    m.find(4)->second = "vier";
    EXPECT_EQ(m.find(4)->second, "vier");
    auto next = m.erase(m.find(4));
    EXPECT_TRUE(next == m.end() || next->first != 4);
    EXPECT_EQ(sorted_keys(m), (std::vector<int>{1, 2, 5, 6, 7}));

    m.clear();
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.begin(), m.end());
    EXPECT_TRUE(m.insert({8, "eight"}).second);
    EXPECT_EQ(sorted_keys(m), (std::vector<int>{8}));
}

TEST(ConcurrentUnorderedMap_Test, GrowthAndReserve) {
    sgcl::concurrent_unordered_map<int, int> m;
    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(m.try_emplace(i, i * 2).second);
    }
    EXPECT_EQ(m.size(), 1000u);
    EXPECT_GE(m.bucket_count(), 1024u);   // doubled as the elements outnumbered the buckets
    for (int i = 0; i < 1000; ++i) {
        auto it = m.find(i);
        ASSERT_NE(it, m.end());
        EXPECT_EQ(it->second, i * 2);
    }
    EXPECT_EQ(sorted_keys(m).size(), 1000u);
    m.reserve(1 << 14);
    EXPECT_EQ(m.bucket_count(), size_t(1 << 14));
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(m.find(i)->second, i * 2);
    }
    sgcl::concurrent_unordered_map<int, int> big(4096);
    EXPECT_EQ(big.bucket_count(), 4096u);
    sgcl::concurrent_unordered_map<int, int> odd(100);
    EXPECT_EQ(odd.bucket_count(), 128u);   // a power of two
}

TEST(ConcurrentUnorderedMap_Test, CollisionsAndCustomEquality) {
    sgcl::concurrent_unordered_map<int, int, OneHash> one;   // every key the same hash
    for (int i = 0; i < 200; ++i) {
        EXPECT_TRUE(one.try_emplace(i, i).second);
    }
    for (int i = 0; i < 200; ++i) {
        EXPECT_EQ(one.find(i)->second, i);
    }
    EXPECT_FALSE(one.contains(200));
    for (int i = 0; i < 200; i += 3) {
        EXPECT_EQ(one.erase(i), 1u);
    }
    EXPECT_EQ(one.size(), 133u);
    for (int i = 0; i < 200; ++i) {
        EXPECT_EQ(one.contains(i), i % 3 != 0);
    }

    sgcl::concurrent_unordered_map<int, std::string, HashMod10, Mod10> mod;   // keys equal modulo 10
    EXPECT_TRUE(mod.try_emplace(12, "twelve").second);
    EXPECT_FALSE(mod.try_emplace(22, "twenty-two").second);
    EXPECT_EQ(mod.find(32)->second, "twelve");
    EXPECT_EQ(mod.erase(2), 1u);
    EXPECT_FALSE(mod.contains(12));
    EXPECT_EQ(mod.hash_function()(15), 5u);
    EXPECT_TRUE(mod.key_eq()(1, 11));
}

TEST(ConcurrentUnorderedMap_Test, IteratorSurvivesErase) {
    sgcl::concurrent_unordered_map<int, int> m = {{1, 1}, {2, 2}, {3, 3}};
    auto it = m.find(2);
    m.erase(2);
    EXPECT_EQ(it->first, 2);   // the node lives while the iterator holds it
    EXPECT_EQ(it->second, 2);
    ++it;                      // steps to a live element, or the end
    EXPECT_TRUE(it == m.end() || it->first != 2);
    EXPECT_EQ(sorted_keys(m), (std::vector<int>{1, 3}));
}

TEST(ConcurrentUnorderedMap_Test, ElementsDieWithTheirNodes) {
    const size_t before = Int::counter;
    sgcl::concurrent_unordered_map<int, Int> m;
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

TEST(ConcurrentUnorderedMap_Test, NodesAndObjectsReclaimed) {
    const size_t before = collector::get_live_object_count();
    sgcl::concurrent_unordered_map<int, tracked_ptr<Baz>> m;
    const size_t empty = collector::get_live_object_count();   // the head, the bucket array and its buffer, the counters
    EXPECT_EQ(empty, before + 4u);
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            m.try_emplace(i, make_tracked<Baz>(i));
        }
    });
    // 100 nodes and 100 Baz, the dummies of the buckets used, the array
    // grown (the old ones garbage): between 200 and 200 + the buckets
    size_t live = collector::get_live_object_count();
    EXPECT_GE(live, empty + 200u);
    EXPECT_LE(live, empty + 200u + m.bucket_count() + 2u);
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            m.erase(i);
        }
        EXPECT_TRUE(m.empty());
    });
    live = collector::get_live_object_count();   // the dummies stay, the nodes, markers and Baz are gone
    EXPECT_GE(live, empty);
    EXPECT_LE(live, empty + m.bucket_count() + 2u);
    off_frame([&] {
        tracked_ptr<Baz> kept;
        m.try_emplace(1, make_tracked<Baz>(1));
        auto it = m.find(1);
        kept = it->second;
        m.erase(1);
        EXPECT_EQ(it->second->value, 1);   // held by the iterator
    });
}

TEST(ConcurrentUnorderedMap_Test, GcMapLivesAnywhere) {
    const size_t before = collector::get_live_object_count();
    auto m = std::make_unique<gc::concurrent_unordered_map<std::string, gc::tracked_ptr<Baz>>>();   // in unmanaged memory
    std::vector<gc::concurrent_unordered_map<std::string, gc::tracked_ptr<Baz>>::iterator> its;
    off_frame([&] {
        for (int i = 0; i < 10; ++i) {
            m->try_emplace(std::to_string(i), gc::make_tracked<Baz>(i));
        }
        for (auto it = m->begin(); it != m->end(); ++it) {
            its.push_back(it);
        }
        ASSERT_EQ(its.size(), 10u);
        EXPECT_EQ(m->find("3")->second->value, 3);
        m->clear();
        EXPECT_TRUE(m->empty());
    });
    off_frame([&] {
        its.clear();
    });
    m.reset();
    EXPECT_EQ(live_without_cells(), before);
}

TEST(ConcurrentUnorderedMap_Test, MapInsideManagedObject) {
    struct Holder {
        sgcl::concurrent_unordered_map<int, tracked_ptr<Baz>> m;
    };
    tracked_ptr h = make_tracked<Holder>();
    h->m.try_emplace(1, make_tracked<Baz>(1));
    h->m.try_emplace(2, make_tracked<Baz>(2));
    EXPECT_EQ(h->m.find(2)->second->value, 2);
    EXPECT_EQ(h->m.size(), 2u);
}

TEST(ConcurrentUnorderedMap_Test, DisjointInsertsManyThreads) {
    const int threads = 8;
    const int per_thread = 5000;
    sgcl::concurrent_unordered_map<int, tracked_ptr<Baz>> m;
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
    auto keys = sorted_keys(m);
    ASSERT_EQ(keys.size(), size_t(threads * per_thread));
    for (int k = 0; k < threads * per_thread; ++k) {
        ASSERT_EQ(keys[size_t(k)], k);
        ASSERT_EQ(m.find(k)->second->value, k);
    }
    EXPECT_GE(m.bucket_count(), size_t(threads * per_thread));
}

TEST(ConcurrentUnorderedMap_Test, SameKeysManyThreads) {
    const int threads = 8;
    const int n = 5000;
    sgcl::concurrent_unordered_map<int, int> m;
    std::atomic<int> inserted = {0};
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
    auto keys = sorted_keys(m);
    for (int k = 0; k < n; ++k) {
        ASSERT_EQ(keys[size_t(k)], k);
    }
}

// Threads insert, erase and look up over a shared key range while the
// collector runs and the array doubles under them: every value matches
// its key (a node freed too early would not), and the map holds exactly
// the keys its lookups say once the threads are done.
TEST(ConcurrentUnorderedMap_Test, ChurnManyThreads) {
    const int threads = 8;
    const int range = 2000;
    const int ops = 40000;
    off_frame([&] {
        sgcl::concurrent_unordered_map<int, tracked_ptr<Baz>> m;
        std::atomic<bool> bad = {false};
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
                        for (auto& [key_, val] : m) {   // a walk while the others modify
                            if (val->value != key_) {
                                bad = true;
                            }
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
        auto keys = sorted_keys(m);
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
        EXPECT_GE(m.bucket_count(), 1024u);
    });
}

TEST(ConcurrentUnorderedSet_Test, InsertFindErase) {
    sgcl::concurrent_unordered_set<int> s;
    EXPECT_TRUE(s.empty());
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
    EXPECT_EQ(s.count(9), 0u);
    EXPECT_EQ(sorted_keys(s), (std::vector<int>{1, 2, 3, 4, 5}));
    EXPECT_EQ(s.erase(3), 1u);
    EXPECT_EQ(s.erase(3), 0u);
    EXPECT_EQ(sorted_keys(s), (std::vector<int>{1, 2, 4, 5}));
    s.erase(s.find(4));
    EXPECT_EQ(sorted_keys(s), (std::vector<int>{1, 2, 5}));
    s.clear();
    EXPECT_TRUE(s.empty());
    static_assert(std::is_same_v<decltype(*s.begin()), const int&>);

    sgcl::concurrent_unordered_set<std::string> names = {"b", "a", "c", "a"};
    EXPECT_EQ(names.size(), 3u);
    EXPECT_TRUE(names.contains("a"));
    gc::concurrent_unordered_set<int> g;
    for (int i = 0; i < 1000; ++i) {
        g.insert(i);
    }
    EXPECT_EQ(g.size(), 1000u);
    EXPECT_GE(g.bucket_count(), 1024u);
}

TEST(ConcurrentUnorderedSet_Test, ChurnManyThreads) {
    const int threads = 8;
    const int range = 2000;
    const int ops = 40000;
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::concurrent_unordered_set<int> s;
        std::atomic<bool> bad = {false};
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
                        if (*s.insert(k).first != k) {
                            bad = true;
                        }
                    } else if (a < 60) {
                        s.erase(k);
                    } else if (a < 99) {
                        auto it = s.find(k);
                        if (it != s.end() && *it != k) {
                            bad = true;
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
        auto keys = sorted_keys(s);
        EXPECT_EQ(std::adjacent_find(keys.begin(), keys.end()), keys.end());
        EXPECT_EQ(s.size(), keys.size());
        for (int k : keys) {
            EXPECT_TRUE(s.contains(k));
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), before);
}
