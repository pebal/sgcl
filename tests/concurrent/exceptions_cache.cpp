//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Exception safety of concurrent::cache: put, get_or_compute and get run
// with a throw at each of their points in turn (the key's and the value's
// construction, copy and move: tests/throwing.h), and the cache checked
// after each run against the Exceptions of the operation's page: size()
// the entries found, each value the one put, the argument as it was.
#include "tests/throwing.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

using throwing::Val;

namespace {
    using Hash8 = throwing::Hash<8>;
    using Cache = sgcl::concurrent::cache<Val, Val, Hash8>;
    using namespace std::chrono_literals;

    // The keys put before every operation, as in exceptions_hash.cpp: the
    // evens, and 3, 11, 19 sharing the hash 3, in that order; a value is
    // its key times ten. 27 joins their chain, 7 opens a bucket.
    const std::vector<int> Keys = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 3, 11, 19};
    const std::vector<int> Universe = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 3, 11, 19, 27, 7};

    std::vector<int> sorted(std::vector<int> v) {
        std::sort(v.begin(), v.end());
        return v;
    }

    struct CacheState {
        CacheState(size_t capacity, Cache::duration ttl, int key, int value)
        : c(capacity, ttl)
        , key(key)
        , value(value) {
            for (int k : Keys) {
                c.put(Val(k), Val(k * 10));
            }
        }

        Cache c;
        Val key;
        Val value;
        Val result;
        bool erased = false;
    };

    // A cache of the Keys with room for more (32), or full (16), and the
    // argument of the operation
    tracked_ptr<CacheState> cache_state(size_t capacity, int key, int value, Cache::duration ttl = Cache::duration::zero()) {
        return make_tracked<CacheState>(capacity, ttl, key, value);
    }

    // The keys of Universe the cache gives a value for, each value its
    // key times ten unless `replaced` names the key and its other value;
    // size() the count of them
    std::vector<int> present(Cache& c, int replaced = -1, int replacement = 0) {
        std::vector<int> keys;
        for (int k : Universe) {
            if (auto v = c.get(Val(k))) {
                keys.push_back(k);
                if (k == replaced) {
                    EXPECT_TRUE(v->v == k * 10 || v->v == replacement) << "the value of key " << k << ": " << v->v;
                } else {
                    EXPECT_EQ(v->v, k * 10) << "the value of key " << k;
                }
            }
        }
        EXPECT_EQ(c.size(), keys.size()) << "size() against the entries found";
        return sorted(keys);
    }

    std::vector<int> with(int key) {
        auto v = Keys;
        v.push_back(key);
        return sorted(v);
    }

    std::vector<int> without(int key) {
        auto v = Keys;
        v.erase(std::remove(v.begin(), v.end(), key), v.end());
        return sorted(v);
    }
}

// put(key, const T&) and put(key, T&&) of a new key, with room: a throw
// (the copy of the key or the value) leaves the cache as it was; the
// argument stays as it was
TEST(ExceptionsCache_Tests, PutOfANewKey) {
    for (int key : {27, 7}) {
        const std::string at = " of key " + std::to_string(key);
        auto setup = [&] {
            return cache_state(32, key, key * 10);
        };
        auto check = [&](CacheState& s, bool threw, bool) {
            EXPECT_EQ(present(s.c), threw ? sorted(Keys) : with(key));
        };
        int points = 0;
        points += throwing::each_kind("cache put(key, const T&)" + at, setup, [](CacheState& s) {
            s.c.put(s.key, std::as_const(s.value));
        }, [&](CacheState& s, bool threw, bool fired) {
            check(s, threw, fired);
            EXPECT_EQ(s.value.v, key * 10);
            EXPECT_EQ(s.key.v, key);
        });
        points += throwing::each_kind("cache put(key, T&&)" + at, setup, [](CacheState& s) {
            s.c.put(s.key, std::move(s.value));
        }, [&](CacheState& s, bool threw, bool fired) {
            check(s, threw, fired);
            if (threw) {
                EXPECT_EQ(s.value.v, key * 10) << "the argument moved from by a throwing put";
            }
        });
        EXPECT_GE(points, 4);
    }
}

// put of a key the cache holds: the value goes into a box; a throw (the
// search, the value's copy or move) stores no box, and the entry keeps
// its value
TEST(ExceptionsCache_Tests, PutOfATakenKey) {
    auto setup = [] {
        return cache_state(32, 11, 5);
    };
    auto check = [](CacheState& s, bool threw, bool) {
        EXPECT_EQ(present(s.c, 11, 5), sorted(Keys));
        EXPECT_EQ(s.c.get(Val(11))->v, threw ? 110 : 5);
    };
    int points = 0;
    points += throwing::each_kind("cache put(taken, const T&)", setup, [](CacheState& s) {
        s.c.put(s.key, std::as_const(s.value));
    }, check);
    points += throwing::each_kind("cache put(taken, T&&)", setup, [](CacheState& s) {
        s.c.put(s.key, std::move(s.value));
    }, [&](CacheState& s, bool threw, bool fired) {
        check(s, threw, fired);
        if (threw) {
            EXPECT_EQ(s.value.v, 5);
        }
    });
    EXPECT_GE(points, 2);
}

// put of a new key into a full cache: the insertion, then the eviction
// of one entry. A throw before the insertion leaves the cache as it was;
// a put that returns leaves it at its capacity (put.md)
TEST(ExceptionsCache_Tests, PutThatEvicts) {
    int points = throwing::each_kind("cache put evicting", [] {
        return cache_state(16, 27, 270);
    }, [](CacheState& s) {
        s.c.put(s.key, std::as_const(s.value));
    }, [&](CacheState& s, bool threw, bool fired) {
        auto keys = present(s.c);
        if (threw) {
            EXPECT_EQ(keys, sorted(Keys));
            return;
        }
        EXPECT_TRUE(std::binary_search(keys.begin(), keys.end(), 27));
        EXPECT_EQ(keys.size(), 16u);
    }, throwing::Construct | throwing::Copy | throwing::Move);
    EXPECT_GE(points, 2);
}

// What does hold for get_or_compute of an absent key: whatever throws,
// size() counts the entries found, and the key is in the cache with its
// value exactly when the call returned or threw after the insertion
TEST(ExceptionsCache_Tests, GetOrComputeOfAnAbsentKeyStaysConsistent) {
    for (int key : {27, 7}) {
        int points = throwing::each_kind("cache get_or_compute absent " + std::to_string(key), [&] {
            return cache_state(32, key, 0);
        }, [key](CacheState& s) {
            s.result.v = s.c.get_or_compute(s.key, [key] {
                return Val(key * 10);
            }).v;
        }, [&](CacheState& s, bool threw, bool) {
            auto keys = present(s.c);
            EXPECT_TRUE(keys == sorted(Keys) || keys == with(key));
            if (!threw) {
                EXPECT_EQ(keys, with(key));
                EXPECT_EQ(s.result.v, key * 10);
            }
        }, throwing::Construct | throwing::Copy | throwing::Move);
        EXPECT_GE(points, 4);
    }
}

// get_or_compute and get of a key the cache holds: the copy of the value
// out (and its move into the result) may throw; the cache is as it was
// but for the stamp of the entry and the count of the hit (get.md)
TEST(ExceptionsCache_Tests, HitOfAHeldKey) {
    auto setup = [] {
        return cache_state(32, 11, 0);
    };
    auto check = [](CacheState& s, bool, bool) {
        EXPECT_EQ(present(s.c), sorted(Keys));
    };
    int points = 0;
    points += throwing::each_kind("cache get_or_compute held", setup, [](CacheState& s) {
        s.result.v = s.c.get_or_compute(s.key, [] {
            return Val(-5);
        }).v;
        EXPECT_EQ(s.result.v, 110);
    }, check, throwing::Construct | throwing::Copy | throwing::Move);
    points += throwing::each_kind("cache get held", setup, [](CacheState& s) {
        auto v = s.c.get(s.key);
        EXPECT_EQ(v->v, 110);
    }, check, throwing::Construct | throwing::Copy | throwing::Move);
    EXPECT_GE(points, 2);
}

// get_or_compute of a stale entry: the stale entry erased, f's value
// put; a throw (f, the copies) leaves the cache with or without the
// stale entry, the size counting what is there
TEST(ExceptionsCache_Tests, GetOfAStaleEntry) {
    auto setup = [] {
        auto s = cache_state(32, 11, 0, 1ms);
        std::this_thread::sleep_for(5ms);   // every entry stale
        return s;
    };
    int points = throwing::each_kind("cache get_or_compute stale", setup, [](CacheState& s) {
        s.result.v = s.c.get_or_compute(s.key, [] {
            return Val(5);
        }).v;
    }, [](CacheState& s, bool threw, bool) {
        if (threw) {
            EXPECT_LE(s.c.size(), 16u);
            EXPECT_GE(s.c.size(), 15u);
        } else {
            EXPECT_EQ(s.result.v, 5);
            EXPECT_EQ(s.c.size(), 16u);
        }
    }, throwing::Construct | throwing::Copy | throwing::Move);
    EXPECT_GE(points, 3);
}
