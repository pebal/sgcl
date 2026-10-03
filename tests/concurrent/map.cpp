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
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
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

TEST(ConcurrentMap_Test, RangeConstructorBuiltAtOnce) {
    // The build from a range links the sorted elements and the dummies
    // of the buckets in use in one pass: duplicates (the first stays, as
    // the inserts would keep it), a hash of few values (chains and
    // buckets never used), the same map as the inserts build, and the
    // map alive after: inserts into buckets never used, erasures, a
    // growth
    std::vector<std::pair<int, int>> in;
    std::mt19937 rng(5);
    for (int i = 0; i < 5000; ++i) {
        in.emplace_back(int(rng() % 3000), i);
    }
    std::map<int, int> o;
    for (auto& [k, v] : in) {
        o.emplace(k, v);   // the first stays
    }
    sgcl::concurrent::map<int, int, HashMod10> few(in.begin(), in.end());
    sgcl::concurrent::map<int, int> m(in.begin(), in.end());
    sgcl::concurrent::map<int, int> by_inserts;
    by_inserts.insert(in.begin(), in.end());
    auto same = [&](auto& c) {
        EXPECT_EQ(c.size(), o.size());
        for (auto& [k, v] : o) {
            auto it = c.find(k);
            ASSERT_NE(it, c.end()) << k;
            EXPECT_EQ(it->second, v);
        }
        EXPECT_EQ(c.find(3000), c.end());
        size_t n = 0;
        for (auto& e : c) {
            n += o.count(e.first);
        }
        EXPECT_EQ(n, o.size());
    };
    same(few);
    same(m);
    same(by_inserts);
    for (int k = 3000; k < 9000; ++k) {   // buckets the build never used, and a growth past the array
        EXPECT_TRUE(m.try_emplace(k, -k).second);
        EXPECT_FALSE(m.try_emplace(k, 0).second);
    }
    EXPECT_EQ(m.size(), o.size() + 6000);
    for (int k = 0; k < 9000; k += 3) {
        m.erase(k);
    }
    for (int k = 0; k < 9000; ++k) {
        EXPECT_EQ(m.contains(k), k % 3 != 0 && (k >= 3000 || o.count(k)));
    }
    int few_keys[] = {5, 3, 5, 1, 3};
    sgcl::concurrent::set<int> s(std::begin(few_keys), std::end(few_keys));
    EXPECT_EQ(s.size(), 3u);
    std::vector<std::pair<int, int>> none;
    sgcl::concurrent::map<int, int> e(none.begin(), none.end());
    EXPECT_TRUE(e.empty());
    EXPECT_TRUE(e.try_emplace(1, 1).second);
}

TEST(ConcurrentMap_Test, InsertFindErase) {
    sgcl::concurrent::map<int, std::string> m;
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

TEST(ConcurrentMap_Test, TransparentLookupWithStringKeys) {
    sgcl::concurrent::map<sgcl::string, int> m = {{"alice", 30}, {"bob", 40}};
    sgcl::concurrent::set<sgcl::string> s = {"alice", "bob"};
    std::string_view view = "alice";                 // converts to no string: the transparent overloads
    EXPECT_EQ(m.find(view)->second, 30);
    EXPECT_EQ(std::as_const(m).find(view)->second, 30);
    EXPECT_TRUE(m.contains(view) && !m.contains(std::string_view("carol")));
    EXPECT_EQ(m.count(view), 1u);
    EXPECT_TRUE(s.contains(view) && s.find(view) != s.end() && s.count(std::string_view("x")) == 0);
    EXPECT_EQ(m.erase(std::string_view("bob")), 1u);
    EXPECT_EQ(m.erase(std::string_view("bob")), 0u);
    EXPECT_EQ(s.erase(std::string_view("bob")), 1u);
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(s.size(), 1u);
    auto before = collector::get_statistics().live_bytes;   // managed memory in use: a temporary string would add to it
    int sum = 0;
    for (int i = 0; i < 10000; ++i) {
        sum += m.find("alice")->second + (int)s.contains("alice");
    }
    EXPECT_EQ(sum, 10000 * 31);
    EXPECT_LE(collector::get_statistics().live_bytes, before);   // no string made for any lookup (a sweep meanwhile can only lower the count)
}

TEST(ConcurrentMap_Test, GrowthAndReserve) {
    sgcl::concurrent::map<int, int> m;
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
    sgcl::concurrent::map<int, int> big(4096);
    EXPECT_EQ(big.bucket_count(), 4096u);
    sgcl::concurrent::map<int, int> odd(100);
    EXPECT_EQ(odd.bucket_count(), 128u);   // a power of two
}

TEST(ConcurrentMap_Test, CollisionsAndCustomEquality) {
    sgcl::concurrent::map<int, int, OneHash> one;   // every key the same hash
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

    sgcl::concurrent::map<int, std::string, HashMod10, Mod10> mod;   // keys equal modulo 10
    EXPECT_TRUE(mod.try_emplace(12, "twelve").second);
    EXPECT_FALSE(mod.try_emplace(22, "twenty-two").second);
    EXPECT_EQ(mod.find(32)->second, "twelve");
    EXPECT_EQ(mod.erase(2), 1u);
    EXPECT_FALSE(mod.contains(12));
    EXPECT_EQ(mod.hash_function()(15), 5u);
    EXPECT_TRUE(mod.key_eq()(1, 11));
}

TEST(ConcurrentMap_Test, IteratorSurvivesErase) {
    sgcl::concurrent::map<int, int> m = {{1, 1}, {2, 2}, {3, 3}};
    auto it = m.find(2);
    m.erase(2);
    EXPECT_EQ(it->first, 2);   // the node lives while the iterator holds it
    EXPECT_EQ(it->second, 2);
    ++it;                      // steps to a live element, or the end
    EXPECT_TRUE(it == m.end() || it->first != 2);
    EXPECT_EQ(sorted_keys(m), (std::vector<int>{1, 3}));
}

TEST(ConcurrentMap_Test, ElementsDieWithTheirNodes) {
    const size_t before = Int::counter;
    sgcl::concurrent::map<int, Int> m;
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
    sgcl::concurrent::map<int, tracked_ptr<Baz>> m;
    const size_t empty = collector::get_live_object_count();   // the head, the bucket array and its buffer (the counters are plain memory)
    EXPECT_EQ(empty, before + 3u);
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

TEST(ConcurrentMap_Test, MapInsideManagedObject) {
    struct Holder {
        sgcl::concurrent::map<int, tracked_ptr<Baz>> m;
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
    sgcl::concurrent::map<int, tracked_ptr<Baz>> m;
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

TEST(ConcurrentMap_Test, SameKeysManyThreads) {
    const int threads = 8;
    const int n = 5000;
    sgcl::concurrent::map<int, int> m;
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
    auto keys = sorted_keys(m);
    for (int k = 0; k < n; ++k) {
        ASSERT_EQ(keys[size_t(k)], k);
    }
}

// Threads insert, erase and look up over a shared key range while the
// collector runs and the array doubles under them: every value matches
// its key (a node freed too early would not), and the map holds exactly
// the keys its lookups say once the threads are done.
TEST(ConcurrentMap_Test, ChurnManyThreads) {
    const int threads = 8;
    const int range = 2000;
    const int ops = 40000;
    off_frame([&] {
        sgcl::concurrent::map<int, tracked_ptr<Baz>> m;
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

TEST(ConcurrentSet_Test, InsertFindErase) {
    sgcl::concurrent::set<int> s;
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

    sgcl::concurrent::set<std::string> names = {"b", "a", "c", "a"};
    EXPECT_EQ(names.size(), 3u);
    EXPECT_TRUE(names.contains("a"));
    sgcl::concurrent::set<int> g;
    for (int i = 0; i < 1000; ++i) {
        g.insert(i);
    }
    EXPECT_EQ(g.size(), 1000u);
    EXPECT_GE(g.bucket_count(), 1024u);
}

TEST(ConcurrentSet_Test, ChurnManyThreads) {
    const int threads = 8;
    const int range = 2000;
    const int ops = 40000;
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        sgcl::concurrent::set<int> s;
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

namespace {
    // A hash that looks at the heap once, while the map is built from a
    // range: the managed objects then alive whose type is the order of
    // the build
    struct LookingHash {
        inline static long trigger = -1;
        inline static size_t seen = 0;

        size_t operator()(long k) const noexcept {
            if (k == trigger) {
                trigger = -1;
                seen = live_objects_named("6Placed");
            }
            return std::hash<long>()(k);
        }
    };
}

// The order of a build from a range is scratch of the constructor, plain
// memory; the striped count is plain memory the map owns: neither a
// managed object (the order 240 KB for 10 000 elements, the count 2 KB
// for every map before)
TEST(ConcurrentMap_Test, TheOrderOfABuildAndTheCountAreOffTheManagedHeap) {
    std::vector<std::pair<long, long>> items;
    for (long i = 0; i < 10000; ++i) {
        items.emplace_back(i, i);
    }
    items.emplace_back(7, -1);   // a key again: the first stays
    LookingHash::trigger = 5000;
    LookingHash::seen = 0;
    sgcl::concurrent::map<long, long, LookingHash> m(items.begin(), items.end());
    EXPECT_EQ(LookingHash::trigger, -1);   // the hash looked
    EXPECT_EQ(LookingHash::seen, 0u);
    EXPECT_EQ(m.size(), 10000u);
    EXPECT_EQ(m.find(7)->second, 7);
    sgcl::concurrent::map<int, int> empty;
    EXPECT_EQ(live_objects_named("8Counters"), 0u);
    EXPECT_TRUE(empty.try_emplace(1, 1).second);
    EXPECT_EQ(empty.size(), 1u);
    EXPECT_EQ(m.erase(7), 1u);
    EXPECT_EQ(m.size(), 9999u);
}

// value_or(key, fallback): the value under the key, or the fallback, by value;
// a key of another type through the transparent lookup; a tracked value
// comes back as the same object
TEST(ConcurrentMap_Test, ValueOrWithAFallback) {
    sgcl::concurrent::map<sgcl::string, int> m = {{"a", 1}, {"b", 2}};
    EXPECT_EQ(m.value_or("a", 0), 1);
    EXPECT_EQ(m.value_or("z", -1), -1);
    EXPECT_EQ(m.value_or(std::string_view("b"), 0), 2);
    EXPECT_EQ(m.value_or(std::string_view("zz"), 7), 7);
    const auto& c = m;
    EXPECT_EQ(c.value_or("b", 0), 2);
    sgcl::concurrent::map<int, sgcl::tracked_ptr<int>> p;
    sgcl::tracked_ptr<int> one = sgcl::make_tracked<int>(1);
    sgcl::tracked_ptr<int> none = sgcl::make_tracked<int>(0);
    p.try_emplace(1, one);
    EXPECT_EQ(p.value_or(1, none), one);
    EXPECT_EQ(p.value_or(2, none), none);
    static_assert(std::is_same_v<decltype(m.value_or("a", 0)), int>);
}

// An erasure allocates nothing once its node is marked: the unlinking
// that follows starts from the dummy of the bucket or of its nearest
// ancestor, so a bucket that has no dummy in the current array (grown
// since the element went in) gets none from an erasure.
TEST(ConcurrentMap_Test, AnErasureAllocatesNothingAfterItsMark) {
    sgcl::concurrent::map<int, int> m(64);
    off_frame([&] {
        for (int k = 64; k < 80; ++k) {   // buckets 0..15 of 64, buckets 64..79 of 128
            m.try_emplace(k, k);
        }
        m.reserve(128);
    });
    ASSERT_EQ(m.bucket_count(), 128u);
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        for (auto it = m.begin(); it != m.end();) {   // a walk makes no dummy, as a find would
            it = m.erase(it);
        }
    });
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(collector::get_live_object_count(), before - 16u);   // the nodes gone, and no dummy made
    off_frame([&] {
        for (int k = 64; k < 80; ++k) {
            m.try_emplace(k, k);   // the buckets 64..79 made now, by the insertions
        }
        m.clear();
    });
    EXPECT_TRUE(m.empty());
    for (int k = 64; k < 80; ++k) {
        EXPECT_FALSE(m.contains(k));
    }
}

// insert of an element whose key is taken leaves its argument as it was,
// as std::unordered_map::insert does: the search comes first and the node
// is built only for an absent key, by rvalue (the element, a pair of the
// key and another value type, the elements of a moved range) as by copy
TEST(ConcurrentMap_Test, InsertOfATakenKeyLeavesTheArgument) {
    const std::string long_value(64, 'x');   // past the small-string buffer: a move empties it
    sgcl::concurrent::map<int, std::string> m;
    EXPECT_TRUE(m.insert({1, "one"}).second);
    EXPECT_TRUE(m.insert({2, "two"}).second);

    std::pair<const int, std::string> element(1, long_value);
    EXPECT_FALSE(m.insert(std::move(element)).second);
    EXPECT_EQ(element.second, long_value);

    std::pair<int, std::string> other(2, long_value);
    EXPECT_FALSE(m.insert(std::move(other)).second);
    EXPECT_EQ(other.second, long_value);

    std::vector<std::pair<const int, std::string>> range;
    range.emplace_back(1, long_value);
    range.emplace_back(3, long_value);
    m.insert(std::make_move_iterator(range.begin()), std::make_move_iterator(range.end()));
    EXPECT_EQ(range[0].second, long_value);   // key 1 taken: left as it was
    EXPECT_EQ(m.find(3)->second, long_value);   // key 3 absent: moved in

    std::pair<int, std::string> fresh(4, long_value);
    EXPECT_TRUE(m.insert(std::move(fresh)).second);
    EXPECT_EQ(m.find(4)->second, long_value);
    EXPECT_EQ(m.find(1)->second, "one");
    EXPECT_EQ(m.find(2)->second, "two");
    EXPECT_EQ(sorted_keys(m), (std::vector<int>{1, 2, 3, 4}));
}

// insert of a key the set holds leaves its argument as it was, as
// std::unordered_set::insert does, by rvalue and from a moved range alike
TEST(ConcurrentSet_Test, InsertOfATakenKeyLeavesTheArgument) {
    const std::string long_key(64, 'k');   // past the small-string buffer: a move empties it
    sgcl::concurrent::set<std::string> s;
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

// The count of buckets at its ends: 0 and 1 make 2, a power of two stays,
// and reserve of no more than there are changes nothing
TEST(ConcurrentMap_Test, BucketCountAtItsEnds) {
    EXPECT_EQ((sgcl::concurrent::map<int, int>(0).bucket_count()), 2u);
    EXPECT_EQ((sgcl::concurrent::map<int, int>(1).bucket_count()), 2u);
    EXPECT_EQ((sgcl::concurrent::map<int, int>(2).bucket_count()), 2u);
    EXPECT_EQ((sgcl::concurrent::map<int, int>(3).bucket_count()), 4u);
    EXPECT_EQ((sgcl::concurrent::set<int>(0).bucket_count()), 2u);
    std::vector<std::pair<int, int>> none;
    EXPECT_EQ((sgcl::concurrent::map<int, int>(none.begin(), none.end(), 0).bucket_count()), 2u);
    sgcl::concurrent::map<int, int> m(2);
    m.reserve(0);
    m.reserve(1);
    m.reserve(2);
    EXPECT_EQ(m.bucket_count(), 2u);
    m.reserve(3);
    EXPECT_EQ(m.bucket_count(), 4u);
    EXPECT_TRUE(m.try_emplace(1, 1).second);   // a map of two buckets grows by its insertions
    EXPECT_TRUE(m.try_emplace(2, 2).second);
    EXPECT_TRUE(m.try_emplace(3, 3).second);
    EXPECT_EQ(m.size(), 3u);
}

// A count of buckets past what an address space holds (the rounding up
// of a count past the largest power of two was undefined) is the count
// no memory gives: the program ends as at any refused managed allocation,
// and so does reserve, whose doublings meet the limit on the way
TEST(ConcurrentMap_Test, ABucketCountNoMemoryHoldsEnds) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");   // a forked child may not allocate managed memory (os.h)
    EXPECT_DEATH((sgcl::concurrent::map<int, int>(SIZE_MAX)), "sgcl: out of managed memory");
    EXPECT_DEATH((sgcl::concurrent::map<int, int>((size_t(1) << 63) + 1)), "sgcl: out of managed memory");
    EXPECT_DEATH((sgcl::concurrent::set<int>(SIZE_MAX)), "sgcl: out of managed memory");
    std::vector<int> one = {1};
    EXPECT_DEATH((sgcl::concurrent::set<int>(one.begin(), one.end(), SIZE_MAX)), "sgcl: out of managed memory");
    using Map = sgcl::concurrent::map<int, int>;
    EXPECT_DEATH({
        collector::set_memory_limit(collector::get_committed_memory() + (size_t(64) << 20));   // the doublings refused at 64 MB more
        Map m;
        m.reserve(SIZE_MAX);
    }, "sgcl: out of managed memory");
}

// An empty map (default, from an empty range, from an empty list) and a
// map of one element: the lookups, value_or, the erasures of nothing
TEST(ConcurrentMap_Test, EmptyAndOneElement) {
    std::vector<std::pair<int, int>> none;
    sgcl::concurrent::map<int, int> d;
    sgcl::concurrent::map<int, int> r(none.begin(), none.end());
    sgcl::concurrent::map<int, int> l = {};
    for (auto* m : {&d, &r, &l}) {
        EXPECT_TRUE(m->empty());
        EXPECT_EQ(m->size(), 0u);
        EXPECT_EQ(m->begin(), m->end());
        EXPECT_EQ(m->find(0), m->end());
        EXPECT_FALSE(m->contains(INT_MAX));
        EXPECT_EQ(m->count(INT_MIN), 0u);
        EXPECT_EQ(m->value_or(0, -1), -1);
        EXPECT_EQ(m->erase(0), 0u);
        m->clear();
        m->insert(none.begin(), none.end());
        EXPECT_TRUE(m->empty());
    }
    d.try_emplace(INT_MIN, 1);
    EXPECT_EQ(d.size(), 1u);
    EXPECT_EQ(d.begin()->first, INT_MIN);
    EXPECT_EQ(d.value_or(INT_MIN, -1), 1);
    EXPECT_EQ(d.erase(d.begin()), d.end());
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(d.size(), 0u);
}

namespace {
    // The key as its own hash: the split keys at their ends, a hash of 0
    // (the head's bucket) and of all ones (the last split key there is)
    struct Identity {
        size_t operator()(size_t k) const noexcept {
            return k;
        }
    };
}

// The hashes at the ends of their range: every key found, the walk sees
// each once, the erasures leave the others, across a growth
TEST(ConcurrentMap_Test, HashesAtTheEndsOfTheirRange) {
    const std::vector<size_t> keys = {0, 1, SIZE_MAX, SIZE_MAX - 1, size_t(1) << 63, (size_t(1) << 63) - 1, 2, 3};
    sgcl::concurrent::map<size_t, int, Identity> m(2);
    for (size_t i = 0; i < keys.size(); ++i) {
        EXPECT_TRUE(m.try_emplace(keys[i], int(i)).second);
    }
    EXPECT_GE(m.bucket_count(), keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        ASSERT_NE(m.find(keys[i]), m.end());
        EXPECT_EQ(m.find(keys[i])->second, int(i));
    }
    size_t walked = 0;
    for (auto& e : m) {
        EXPECT_NE(std::find(keys.begin(), keys.end(), e.first), keys.end());
        ++walked;
    }
    EXPECT_EQ(walked, keys.size());
    EXPECT_EQ(m.erase(SIZE_MAX), 1u);
    EXPECT_EQ(m.erase(0), 1u);
    EXPECT_FALSE(m.contains(SIZE_MAX));
    EXPECT_TRUE(m.contains(SIZE_MAX - 1));
    EXPECT_EQ(m.size(), keys.size() - 2);
    std::vector<std::pair<size_t, int>> in;
    for (size_t k : keys) {
        in.emplace_back(k, 0);
    }
    sgcl::concurrent::map<size_t, int, Identity> built(in.begin(), in.end());   // the build at once, the same keys
    EXPECT_EQ(built.size(), keys.size());
    for (size_t k : keys) {
        EXPECT_TRUE(built.contains(k));
    }
}

// An element of the map as the argument: its key to try_emplace and
// erase, the element itself to insert, its value as value_or's fallback
TEST(ConcurrentMap_Test, ItsOwnElementAsTheArgument) {
    const std::string k(64, 'k'), v(64, 'v');
    sgcl::concurrent::map<std::string, std::string> m = {{k, v}};
    auto it = m.begin();
    EXPECT_FALSE(m.try_emplace(it->first, "other").second);
    EXPECT_FALSE(m.insert(*it).second);
    EXPECT_FALSE(m.emplace(it->first, it->second).second);
    EXPECT_EQ(m.value_or(std::string(64, 'z'), it->second), v);
    EXPECT_EQ(m.erase(it->first), 1u);
    auto [again, inserted] = m.insert(*it);   // the erased element, alive in the iterator
    EXPECT_TRUE(inserted);
    EXPECT_EQ(again->first, k);
    EXPECT_EQ(again->second, v);
    EXPECT_EQ(m.erase(it), m.end());   // erased before: nothing erased, the new element under its key stays
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(m.find(k), again);

    sgcl::concurrent::set<std::string> s = {k};
    auto first = s.begin();
    EXPECT_FALSE(s.insert(*first).second);
    EXPECT_EQ(s.erase(*first), 1u);
    EXPECT_TRUE(s.insert(*first).second);
    EXPECT_EQ(s.size(), 1u);
}

// Two threads at the last element, many rounds: one erasure returns 1;
// two try_emplace of one key into the empty map insert once and both read
// the element that won; an erasure by iterator against one by key
TEST(ConcurrentMap_Test, TwoThreadsAtTheLastElement) {
    sgcl::concurrent::map<int, int> m;
    for (int round = 0; round < together::Rounds; ++round) {
        std::atomic<int> erased = {0}, inserted = {0}, sum = {0};
        m.try_emplace(round, round);
        together::run(2, [&](int) {
            erased += int(m.erase(round));
        });
        EXPECT_EQ(erased.load(), 1);
        EXPECT_TRUE(m.empty());
        EXPECT_EQ(m.size(), 0u);
        together::run(2, [&](int i) {
            auto [it, ok] = m.try_emplace(round, i + 1);
            inserted += int(ok);
            sum += it->second;
        });
        EXPECT_EQ(inserted.load(), 1);
        EXPECT_EQ(sum.load(), 2 * m.value_or(round, 0));
        together::run(2, [&](int i) {
            if (i == 0) {
                auto it = m.find(round);
                if (it != m.end()) {
                    m.erase(it);
                }
            } else {
                m.erase(round);
            }
        });
        EXPECT_TRUE(m.empty());
        EXPECT_EQ(m.size(), 0u);
    }
}

// clear against insertions of other threads: every element is either
// erased by clear or found after it, none counted twice
TEST(ConcurrentMap_Test, ClearAgainstInsertions) {
    sgcl::concurrent::set<int> s;
    for (int round = 0; round < together::Rounds / 10; ++round) {
        together::run(3, [&](int i) {
            if (i == 0) {
                s.clear();
            } else {
                for (int k = 0; k < 64; ++k) {
                    s.insert(i * 1000 + k);
                }
            }
        });
        size_t walked = 0;
        for (int k : s) {
            EXPECT_TRUE(k / 1000 == 1 || k / 1000 == 2);
            ++walked;
        }
        EXPECT_EQ(s.size(), walked);
        s.clear();
        EXPECT_TRUE(s.empty());
        EXPECT_EQ(s.size(), 0u);
    }
}
