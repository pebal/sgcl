//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;
}

TEST(ConcurrentCache_Test, PutGetErase) {
    using Cache = sgcl::concurrent::cache<int, std::string>;
    Cache c(10);
    EXPECT_TRUE(c.empty());
    EXPECT_EQ(c.size(), 0u);
    EXPECT_EQ(c.capacity(), 10u);
    EXPECT_EQ(c.ttl(), Cache::duration::zero());
    EXPECT_EQ(c.sample_size(), 8u);
    EXPECT_FALSE(c.get(1));

    c.put(1, "one");
    c.put(2, "two");
    EXPECT_EQ(c.size(), 2u);
    ASSERT_TRUE(c.get(1));
    EXPECT_EQ(*c.get(1), "one");
    EXPECT_EQ(*c.get(2), "two");
    c.put(1, "uno");                       // replaced, the size unchanged
    EXPECT_EQ(*c.get(1), "uno");
    EXPECT_EQ(c.size(), 2u);
    std::string s = "drei";
    c.put(3, s);
    c.put(4, std::move(s));
    EXPECT_EQ(*c.get(4), "drei");

    EXPECT_TRUE(c.erase(1));
    EXPECT_FALSE(c.erase(1));
    EXPECT_FALSE(c.get(1));
    EXPECT_EQ(c.size(), 3u);
    c.clear();
    EXPECT_TRUE(c.empty());
    EXPECT_FALSE(c.get(2));
    c.put(5, "five");
    EXPECT_EQ(*c.get(5), "five");
}

TEST(ConcurrentCache_Test, CapacityRespected) {
    sgcl::concurrent::cache<int, int> c(100);
    for (int i = 0; i < 1000; ++i) {
        c.put(i, i);
        ASSERT_LE(c.size(), 100u) << "after put " << i;
    }
    EXPECT_EQ(c.size(), 100u);
    int present = 0;
    for (int i = 0; i < 1000; ++i) {
        if (auto v = c.get(i)) {
            EXPECT_EQ(*v, i);
            ++present;
        }
    }
    EXPECT_EQ(present, 100);
    for (int i = 0; i < 100; ++i) {   // replacing keys present adds nothing
        c.put(i, -i);
        ASSERT_LE(c.size(), 100u);
    }
    sgcl::concurrent::cache<int, int> one(1);
    one.put(1, 1);
    one.put(2, 2);
    EXPECT_EQ(one.size(), 1u);
    EXPECT_TRUE(one.get(2));
    sgcl::concurrent::cache<int, int> none(0);   // keeps nothing
    none.put(1, 1);
    EXPECT_EQ(none.size(), 0u);
    EXPECT_FALSE(none.get(1));
}

// A get after a put is newer than the put, so at two entries the
// approximation is exact: the ordered_map example (a, b, a read, c) in
// this cache
TEST(ConcurrentCache_Test, TheReadOneStays) {
    sgcl::concurrent::cache<std::string, std::string> c(2);
    c.put("a", "1");
    c.put("b", "2");
    c.get("a");                            // a is newer than b now
    c.put("c", "3");                       // full: b, the oldest, goes
    EXPECT_TRUE(c.get("a") && c.get("c") && !c.get("b"));
    c.put("d", "4");                       // c was put after a was read: a goes
    EXPECT_TRUE(c.get("c") && c.get("d") && !c.get("a"));
}

// The approximation: a set of keys read over and over while twice the
// capacity of others streams through. Nearly all of the hot keys
// survive; a cache without an order would keep a tenth of them.
TEST(ConcurrentCache_Test, RecentlyUsedSurvive) {
    const int capacity = 1000;
    const int hot = 100;
    sgcl::concurrent::cache<int, int> c(capacity);
    for (int i = 0; i < capacity; ++i) {
        c.put(i, i);
    }
    for (int k = capacity; k < capacity * 3; ++k) {
        for (int j = 0; j < 10; ++j) {   // ten hot keys read per insertion: every hot key every ten insertions
            c.get((k % 10) * 10 + j);
        }
        c.put(k, k);
        ASSERT_LE(c.size(), size_t(capacity));
    }
    int survived = 0;
    for (int h = 0; h < hot; ++h) {
        survived += c.get(h) ? 1 : 0;
    }
    EXPECT_GE(survived, hot * 9 / 10) << survived << " of " << hot << " hot keys survived";
    int cold = 0;
    for (int i = hot; i < capacity; ++i) {   // the first fill, never read again: gone
        cold += c.get(i) ? 1 : 0;
    }
    EXPECT_LT(cold, (capacity - hot) / 10) << cold << " untouched keys of the first fill survived";
}

TEST(ConcurrentCache_Test, TimeToLive) {
    sgcl::concurrent::cache<int, int> c(10, 30ms);
    EXPECT_EQ(c.ttl(), 30ms);
    c.put(1, 1);
    c.put(2, 2);
    EXPECT_EQ(*c.get(1), 1);
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(c.get(1));               // stale: absent, and erased by the get
    EXPECT_EQ(c.size(), 1u);
    c.put(2, 22);                         // a put renews the entry
    EXPECT_EQ(*c.get(2), 22);
    EXPECT_EQ(c.get_or_compute(3, [] { return 3; }), 3);
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(c.get_or_compute(3, [] { return 33; }), 33);   // stale: computed again
    EXPECT_EQ(*c.get(3), 33);

    sgcl::concurrent::cache<int, int> e(4, 30ms);   // the stale go first at an eviction
    for (int i = 0; i < 4; ++i) {
        e.put(i, i);
    }
    std::this_thread::sleep_for(50ms);
    e.put(4, 4);
    e.put(5, 5);
    EXPECT_LE(e.size(), 4u);
    EXPECT_TRUE(e.get(4) && e.get(5));
    for (int i = 0; i < 4; ++i) {
        EXPECT_FALSE(e.get(i));
    }
}

TEST(ConcurrentCache_Test, GetOrCompute) {
    sgcl::concurrent::cache<int, std::string> c(10);
    int computed = 0;
    auto f = [&] { ++computed; return std::string("v"); };
    EXPECT_EQ(c.get_or_compute(1, f), "v");
    EXPECT_EQ(computed, 1);
    EXPECT_EQ(c.get_or_compute(1, f), "v");   // present: not computed
    EXPECT_EQ(computed, 1);
    c.put(1, "w");
    EXPECT_EQ(c.get_or_compute(1, f), "w");
    c.erase(1);
    EXPECT_EQ(c.get_or_compute(1, f), "v");
    EXPECT_EQ(computed, 2);
    EXPECT_EQ(c.size(), 1u);
    sgcl::concurrent::cache<int, int> small(2);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(small.get_or_compute(i, [i] { return i * i; }), i * i);
        EXPECT_LE(small.size(), 2u);   // computing past the capacity evicts too
    }
}

TEST(ConcurrentCache_Test, HitsAndMisses) {
    sgcl::concurrent::cache<int, int> c(10);
    EXPECT_EQ(c.hits(), 0u);
    EXPECT_EQ(c.misses(), 0u);
    c.get(1);
    c.put(1, 1);
    c.get(1);
    c.get(1);
    c.get(2);
    EXPECT_EQ(c.hits(), 2u);
    EXPECT_EQ(c.misses(), 2u);
    c.get_or_compute(3, [] { return 3; });   // a miss, then computed
    c.get_or_compute(3, [] { return 3; });   // a hit
    EXPECT_EQ(c.hits(), 3u);
    EXPECT_EQ(c.misses(), 3u);
    c.clear();                               // the counts stay
    EXPECT_EQ(c.hits(), 3u);
    sgcl::concurrent::cache<int, int> t(10, 20ms);
    t.put(1, 1);
    std::this_thread::sleep_for(40ms);
    t.get(1);                                // stale: a miss
    EXPECT_EQ(t.misses(), 1u);
    EXPECT_EQ(t.hits(), 0u);
}

TEST(ConcurrentCache_Test, TransparentLookupWithStringKeys) {
    sgcl::concurrent::cache<sgcl::string, int> c(10);
    c.put("alice", 30);
    c.put("bob", 40);
    std::string_view view = "alice";           // converts to no string: the transparent overloads
    ASSERT_TRUE(c.get(view));
    EXPECT_EQ(*c.get(view), 30);
    EXPECT_EQ(*c.get("bob"), 40);
    EXPECT_FALSE(c.get(std::string_view("carol")));
    auto before = collector::get_statistics().live_bytes;   // managed memory in use: a temporary string would add to it
    int sum = 0;
    for (int i = 0; i < 10000; ++i) {
        sum += *c.get("alice") + *c.get(view);
    }
    EXPECT_EQ(sum, 10000 * 60);
    EXPECT_LE(collector::get_statistics().live_bytes, before);   // no string made for any lookup (a sweep meanwhile can only lower the count)
    EXPECT_TRUE(c.erase(std::string_view("bob")));
    EXPECT_FALSE(c.erase("bob"));
    EXPECT_EQ(c.size(), 1u);
}

TEST(ConcurrentCache_Test, ValuesLiveWhileCachedAndDieWhenEvicted) {
    const size_t before = Int::counter;
    sgcl::concurrent::cache<int, tracked_ptr<Int>> c(50);
    off_frame([&] {
        for (int i = 0; i < 50; ++i) {
            c.put(i, make_tracked<Int>(i));
        }
    });
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(Int::counter, before + 50);   // all cached: all alive
    off_frame([&] {
        for (int i = 50; i < 100; ++i) {     // fifty more: fifty evicted
            c.put(i, make_tracked<Int>(i));
        }
        EXPECT_EQ(c.size(), 50u);
    });
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(Int::counter, before + 50);   // the evicted died with their nodes
    off_frame([&] {
        tracked_ptr<Int> kept = *c.get(99);   // a copy out survives the eviction of its entry
        c.clear();
        EXPECT_EQ((int)*kept, 99);
        EXPECT_EQ(c.size(), 0u);
    });
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(Int::counter, before);
}

TEST(ConcurrentCache_Test, CacheInsideManagedObject) {
    struct Holder {
        sgcl::concurrent::cache<int, tracked_ptr<Baz>> cache{4};
    };
    tracked_ptr h = make_tracked<Holder>();
    for (int i = 0; i < 10; ++i) {
        h->cache.put(i, make_tracked<Baz>(i));
    }
    EXPECT_EQ(h->cache.size(), 4u);
    EXPECT_EQ((*h->cache.get(9))->value, 9);
    EXPECT_EQ(h->cache.get_or_compute(9, [] { return make_tracked<Baz>(-1); })->value, 9);
    EXPECT_EQ(h->cache.get_or_compute(100, [] { return make_tracked<Baz>(100); })->value, 100);
    EXPECT_EQ(h->cache.size(), 4u);
}

// Threads put and get random keys over three times the capacity while
// the collector runs: the size stays bounded (the capacity plus the
// threads that have inserted and not yet evicted), every value matches
// its key, every get is a hit or a miss, and no thread crashes on a
// node another evicted under it.
TEST(ConcurrentCache_Test, StressManyThreads) {
    const int threads = 8;
    const size_t capacity = 500;
    const int range = 1500;
    const int ops = 40000;
    off_frame([&] {
        sgcl::concurrent::cache<int, tracked_ptr<Baz>> c(capacity, {}, 8);
        sgcl::atomic<bool> bad = {false};
        sgcl::atomic<size_t> largest = {0};
        sgcl::atomic<uint64_t> gets = {0};   // every get and get_or_compute is a hit or a miss
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
                        c.put(k, make_tracked<Baz>(k));
                    } else if (a < 40) {
                        auto v = c.get_or_compute(k, [k] { return make_tracked<Baz>(k); });
                        ++gets;
                        if (v->value != k) {
                            bad = true;
                        }
                    } else if (a < 45) {
                        c.erase(k);
                    } else if (a < 99) {
                        ++gets;
                        if (auto v = c.get(k); v && (*v)->value != k) {
                            bad = true;
                        }
                    } else if (t == 0) {
                        collector::force_collect();
                    }
                    size_t n = c.size();
                    size_t seen = largest.load();
                    while (n > seen && !largest.compare_exchange_weak(seen, n)) {
                    }
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_FALSE(bad.load());
        EXPECT_LE(largest.load(), capacity + 2 * threads);   // a thread past its insertion and before its eviction, and the skew of a striped count read stripe by stripe
        EXPECT_LE(c.size(), capacity);
        EXPECT_EQ(c.hits() + c.misses(), gets.load());
        EXPECT_GT(c.hits(), 0u);
    });
}

// A put is never lost under a stale get's erasure or an eviction: the
// value a thread just put is there for its own get right after (a
// capacity nothing reaches, a time to live short enough for the other
// threads' gets to find the entry stale all the time; every thread puts
// keys of its own, so that no two puts race on one key, whose later
// store may carry the earlier time and be stale by right, and gets
// everyone's)
TEST(ConcurrentCache_Test, APutIsNeverLostUnderAStaleGet) {
    const int threads = 8, keys = 16, ops = 30000;
    off_frame([&] {
        sgcl::concurrent::cache<int, int> c(100000, 300us, 8);
        sgcl::atomic<int> lost = {0};
        std::vector<std::thread> ws;
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                std::mt19937 rng(unsigned(t + 7));
                std::uniform_int_distribution<int> key(0, keys - 1), any(0, threads * keys - 1);
                for (int i = 0; i < ops; ++i) {
                    int k = t * keys + key(rng);
                    c.get(any(rng));   // another thread's key, stale as often as not: erased here
                    auto t0 = std::chrono::steady_clock::now();
                    c.put(k, i);
                    auto v = c.get(k);
                    if (!v && std::chrono::steady_clock::now() - t0 < 300us) {
                        ++lost;   // gone within the time to live: a put lost (a get that took longer, a preemption or a sanitizer between, may find it stale by right)
                    }
                    if (i % 64 == 0) {
                        std::this_thread::sleep_for(400us);   // let the entries go stale for the others' gets
                    }
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_EQ(lost.load(), 0);
        size_t n = 0;
        for (int k = 0; k < threads * keys; ++k) {
            n += c.get(k).has_value() ? 1 : 0;
        }
        EXPECT_EQ(c.size(), n);   // the count exact after the erasures by node
    });
    sgcl::collector::force_collect(true);
}
