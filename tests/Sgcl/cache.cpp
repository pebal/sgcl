//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ConcurrentCache over concurrent_cache: the methods forward, the values
// come out as copies, the eviction and the time to live are the inner
// cache's.
#include "tests/types.h"

#include "sgcl/Sgcl/Sgcl.h"

#include <chrono>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    struct Session {
        explicit Session(int v = 0) : value(v) {}
        int value;
    };
}

TEST(Sgcl_Tests, ConcurrentCacheSetTryGetRemove) {
    using namespace Sgcl;
    using Cache = ConcurrentCache<int, Ptr<Session>>;
    Cache cache(4);
    EXPECT_TRUE(cache.IsEmpty());
    EXPECT_EQ(cache.Capacity(), 4u);
    EXPECT_EQ(cache.TimeToLive(), Cache::Duration::zero());
    EXPECT_EQ(cache.SampleSize(), Cache::DefaultSample);
    EXPECT_FALSE(cache.TryGet(1));
    cache.Set(1, Make<Session>(1));
    cache.Set(2, Make<Session>(2));
    Ptr<Session> s = Make<Session>(3);
    cache.Set(3, s);
    EXPECT_EQ(cache.Count(), 3u);
    ASSERT_TRUE(cache.TryGet(1));
    EXPECT_EQ((*cache.TryGet(1))->value, 1);
    EXPECT_EQ((*cache.TryGet(3)).Get(), s.Get());   // the same object, the word copied out
    cache.Set(1, Make<Session>(11));                // replaced
    EXPECT_EQ((*cache.TryGet(1))->value, 11);
    EXPECT_EQ(cache.Count(), 3u);
    EXPECT_TRUE(cache.Remove(2));
    EXPECT_FALSE(cache.Remove(2));
    EXPECT_FALSE(cache.TryGet(2));
    for (int i = 10; i < 20; ++i) {
        cache.Set(i, Make<Session>(i));
        EXPECT_LE(cache.Count(), 4u);               // evicted down to the capacity by every Set
    }
    EXPECT_EQ(cache.Hits(), 4u);
    EXPECT_EQ(cache.Misses(), 2u);
    cache.Clear();
    EXPECT_TRUE(cache.IsEmpty());
    EXPECT_EQ(cache.Inner().capacity(), 4u);
}

TEST(Sgcl_Tests, ConcurrentCacheGetOrAddAndTimeToLive) {
    using namespace Sgcl;
    ConcurrentCache<String, int> cache(10, 30ms);
    int made = 0;
    auto factory = [&] { ++made; return 7; };
    EXPECT_EQ(cache.GetOrAdd("seven", factory), 7);
    EXPECT_EQ(cache.GetOrAdd("seven", factory), 7);   // present: the factory not called
    EXPECT_EQ(made, 1);
    std::string_view view = "seven";                  // a view converts to no String: the lookup is transparent
    EXPECT_EQ(*cache.TryGet(view), 7);
    auto before = collector::get_statistics().live_bytes;
    for (int i = 0; i < 1000; ++i) {
        cache.TryGet("seven");
    }
    EXPECT_EQ(collector::get_statistics().live_bytes, before);   // no String made for any lookup
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(cache.TryGet(view));                 // stale
    EXPECT_EQ(cache.GetOrAdd("seven", factory), 7);   // made again
    EXPECT_EQ(made, 2);
    EXPECT_TRUE(cache.Remove(view));
    EXPECT_EQ(cache.Count(), 0u);
}

TEST(Sgcl_Tests, ConcurrentCacheSharedByThreads) {
    using namespace Sgcl;
    struct Shared {
        ConcurrentCache<int, Ptr<Session>> cache{200};
    };
    RootPtr<Shared> shared = Make<Shared>();
    Atomic<bool> bad = {false};
    Atomic<uint64_t> gets = {0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < 20000; ++i) {
                int k = (i * 7 + t) % 600;
                if (i % 3 == 0) {
                    shared->cache.Set(k, Make<Session>(k));
                } else {
                    ++gets;
                    if (auto v = shared->cache.GetOrAdd(k, [k] { return Make<Session>(k); }); v->value != k) {
                        bad = true;
                    }
                }
                if (shared->cache.Count() > 200u + 8u) {
                    bad = true;
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_FALSE(bad.Load());
    EXPECT_LE(shared->cache.Count(), 200u);
    EXPECT_EQ(shared->cache.Hits() + shared->cache.Misses(), gets.Load());   // every GetOrAdd a hit or a miss
}
