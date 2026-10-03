//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/throwing.h"
#include "tests/concurrent/together.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
    struct Config {
        std::string host;
        int port = 0;
    };

    struct Pair {
        long a = 0;
        long b = 0;
    };
}

TEST(CopyOnWrite_Test, LoadStoreUpdate) {
    sgcl::concurrent::copy_on_write<Config> cfg(std::in_place, "a", 1);
    sgcl::concurrent::copy_on_write<Config>::snapshot s1 = cfg.load();
    EXPECT_EQ(s1->host, "a");
    EXPECT_EQ(s1->port, 1);
    static_assert(std::is_same_v<decltype(s1), tracked_ptr<const Config>>);

    auto s2 = cfg.update([](Config& c) { c.port = 2; });
    EXPECT_EQ(s2->port, 2);
    EXPECT_EQ(s1->port, 1);              // the snapshot is immutable: the update copied
    EXPECT_NE(s1.get(), s2.get());
    EXPECT_EQ(cfg.load().get(), s2.get());

    cfg.store(Config{"b", 3});
    EXPECT_EQ(cfg.load()->host, "b");
    cfg = Config{"c", 4};
    EXPECT_EQ(cfg.load()->port, 4);
    Config d{"d", 5};
    cfg = d;
    EXPECT_EQ(cfg.load()->host, "d");
    tracked_ptr<const Config> conv = cfg;   // the conversion to a snapshot
    EXPECT_EQ(conv->port, 5);
    EXPECT_EQ(s1->host, "a");            // still the first value
    EXPECT_EQ(s2->port, 2);
}

TEST(CopyOnWrite_Test, DefaultAndCompareExchange) {
    sgcl::concurrent::copy_on_write<int> n;
    EXPECT_EQ(*n.load(), 0);
    auto s = n.load();
    EXPECT_TRUE(n.compare_exchange(s, 5));
    EXPECT_EQ(*n.load(), 5);
    EXPECT_EQ(*s, 0);                    // the expected snapshot, untouched on success
    EXPECT_FALSE(n.compare_exchange(s, 6));
    EXPECT_EQ(*s, 5);                    // on failure: the current one
    EXPECT_TRUE(n.compare_exchange(s, 6));
    EXPECT_EQ(*n.load(), 6);
}

TEST(CopyOnWrite_Test, ContainerValue) {
    sgcl::concurrent::copy_on_write<sgcl::vector<tracked_ptr<Baz>>> list;
    EXPECT_TRUE(list.load()->empty());
    list.update([](auto& v) { v.push_back(make_tracked<Baz>(1)); v.push_back(make_tracked<Baz>(2)); });
    auto two = list.load();
    list.update([](auto& v) { v.push_back(make_tracked<Baz>(3)); });
    EXPECT_EQ(two->size(), 2u);
    EXPECT_EQ(list.load()->size(), 3u);
    EXPECT_EQ((*two)[1]->value, 2);
    EXPECT_EQ((*list.load())[2]->value, 3);
    EXPECT_EQ((*two)[0].get(), (*list.load())[0].get());   // the copy is of the pointers: the same Baz
    int sum = 0;
    for (auto& p : *list.load()) {
        sum += p->value;
    }
    EXPECT_EQ(sum, 6);
}

TEST(CopyOnWrite_Test, OldValuesReclaimed) {
    const size_t before = collector::get_live_object_count();
    sgcl::concurrent::copy_on_write<sgcl::vector<tracked_ptr<Baz>>> list;
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);   // the empty vector's object, no buffer
    off_frame([&] {
        for (int i = 0; i < 10; ++i) {
            list.update([&](auto& v) { v.push_back(make_tracked<Baz>(i)); });
        }
    });
    // the current vector, its buffer, ten Baz; the nine replaced vectors and their buffers are garbage
    EXPECT_EQ(collector::get_live_object_count(), before + 12u);
    tracked_ptr<const sgcl::vector<tracked_ptr<Baz>>> kept;
    off_frame([&] {
        kept = list.load();
        list.store(sgcl::vector<tracked_ptr<Baz>>());
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 13u);   // the old one held by kept, the new empty one
    kept = nullptr;
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);
}

TEST(CopyOnWrite_Test, InsideManagedObject) {
    struct Holder {
        sgcl::concurrent::copy_on_write<Config> cfg;
    };
    tracked_ptr h = make_tracked<Holder>();
    h->cfg.update([](Config& c) { c.host = "x"; });
    EXPECT_EQ(h->cfg.load()->host, "x");
}

// Eight writers increment a counter through update: every increment
// lands exactly once, whatever the retries
TEST(CopyOnWrite_Test, UpdatesAreLinearizable) {
    sgcl::concurrent::copy_on_write<long> n(0);
    sgcl::atomic<long> retries = {0};
    off_frame([&] {
        std::vector<std::thread> ws;
        for (int t = 0; t < 8; ++t) {
            ws.emplace_back([&] {
                for (int i = 0; i < 5000; ++i) {
                    long calls = 0;
                    n.update([&](long& v) { ++v; ++calls; });
                    retries += calls - 1;
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
    });
    EXPECT_EQ(*n.load(), 40000);
    EXPECT_GT(retries.load(), 0);   // the writers did collide
}

// Readers see whole values while writers replace them: the two fields
// of a Pair are always equal in a snapshot
TEST(CopyOnWrite_Test, SnapshotsAreWhole) {
    sgcl::concurrent::copy_on_write<Pair> p;
    sgcl::atomic<bool> torn = {false};
    sgcl::atomic<bool> stop = {false};
    off_frame([&] {
        std::vector<std::thread> ws;
        for (int t = 0; t < 6; ++t) {
            ws.emplace_back([&] {
                long last = -1;
                while (!stop.load(std::memory_order_relaxed)) {
                    auto s = p.load();
                    if (s->a != s->b || s->a < last) {
                        torn = true;
                    }
                    last = s->a;
                }
            });
        }
        for (int t = 0; t < 2; ++t) {
            ws.emplace_back([&, t] {
                for (int i = 0; i < 20000; ++i) {
                    p.update([](Pair& x) { ++x.a; ++x.b; });
                    if (t == 0 && i % 5000 == 0) {
                        collector::force_collect();
                    }
                }
            });
        }
        ws[6].join();
        ws[7].join();
        stop = true;
        for (int t = 0; t < 6; ++t) {
            ws[size_t(t)].join();
        }
    });
    EXPECT_FALSE(torn.load());
    EXPECT_EQ(p.load()->a, 40000);
}

namespace {
    // Slots of T's pool left holding `garbage`: many objects made with it,
    // every eighth kept (so that the pages stay the type's and are not
    // zeroed on their way back to the heap), the rest collected
    template<class T>
    sgcl::vector<tracked_ptr<T>> slots_left_with(const T& garbage) {
        sgcl::vector<tracked_ptr<T>> kept;
        {
            sgcl::vector<tracked_ptr<T>> all;
            for (int i = 0; i < 20000; ++i) {
                all.push_back(sgcl::make_tracked<T>(garbage));
            }
            for (size_t i = 0; i < all.size(); i += 8) {
                kept.push_back(all[i]);
            }
        }
        sgcl::collector::force_collect(true);
        sgcl::collector::force_collect(true);
        return kept;
    }

    struct Plain {
        long a;
        long b;
    };
}

// copy_on_write<T>() holds T(): zero for a number and for the members of
// a plain struct, also in a slot whose last object held other bytes
TEST(CopyOnWrite_Test, DefaultIsValueInitialized) {
    auto kept_ints = slots_left_with<long>(0x5A5A5A5A5A5A5A5Al);
    auto kept_plain = slots_left_with<Plain>(Plain{0x5A5A5A5A, 0x5A5A5A5A});
    for (int i = 0; i < 2000; ++i) {
        sgcl::concurrent::copy_on_write<long> n;
        ASSERT_EQ(*n.load(), 0) << i;
        sgcl::concurrent::copy_on_write<Plain> p;
        ASSERT_EQ(p.load()->a, 0) << i;
        ASSERT_EQ(p.load()->b, 0) << i;
    }
    EXPECT_FALSE(kept_ints.empty());
    EXPECT_FALSE(kept_plain.empty());
}

// Boundaries (DESIGN 408)

// The value itself as the argument: a store, an assignment and a
// compare_exchange of the value its own snapshot reads, each a copy of
// its own; an update that reads the old snapshot in its change
TEST(CopyOnWrite_Test, ItsOwnValueAsTheArgument) {
    sgcl::concurrent::copy_on_write<std::string> v(std::string(64, 'a'));
    auto first = v.load();
    v.store(*v.load());
    EXPECT_EQ(*v.load(), std::string(64, 'a'));
    EXPECT_NE(v.load().get(), first.get());   // a new object of the same value
    v = *v.load();
    EXPECT_EQ(*v.load(), std::string(64, 'a'));
    auto seen = v.load();
    EXPECT_TRUE(v.compare_exchange(seen, *seen));   // desired read from expected's own object
    EXPECT_EQ(*v.load(), std::string(64, 'a'));
    EXPECT_NE(v.load().get(), seen.get());
    auto before = v.load();
    v.update([&](std::string& s) { s += *before; });
    EXPECT_EQ(*v.load(), std::string(128, 'a'));
    EXPECT_EQ(*before, std::string(64, 'a'));
}

// The snapshot passed as expected at its ends: a null one, and one of an
// equal value but another object, both fail and get the current
// snapshot (the comparison is by identity); an update that changes
// nothing still installs a new object
TEST(CopyOnWrite_Test, ExpectedAtItsEnds) {
    sgcl::concurrent::copy_on_write<int> n(5);
    sgcl::concurrent::copy_on_write<int>::snapshot null;
    EXPECT_FALSE(n.compare_exchange(null, 6));
    ASSERT_TRUE(null);
    EXPECT_EQ(*null, 5);
    sgcl::concurrent::copy_on_write<int> other(5);
    auto equal = other.load();
    EXPECT_FALSE(n.compare_exchange(equal, 6));
    EXPECT_EQ(equal.get(), n.load().get());
    EXPECT_EQ(*n.load(), 5);
    auto old = n.load();
    auto same = n.update([](int&) {});
    EXPECT_EQ(*same, 5);
    EXPECT_NE(same.get(), old.get());
}

// A change that throws (the copy of the value, f, the construction of a
// stored value): the value as it was, the snapshot the readers hold the
// same object (update.md, store.md, compare_exchange.md)
TEST(CopyOnWrite_Test, AChangeThatThrows) {
    using throwing::Val;
    throwing::Disarm disarm;
    sgcl::concurrent::copy_on_write<Val> v(std::in_place, 1);
    auto held = v.load();
    EXPECT_THROW(v.update([](Val&) { throw throwing::Error("f"); }), throwing::Error);
    throwing::countdown.copy = 1;
    EXPECT_THROW(v.update([](Val& x) { x.v = 2; }), throwing::Error);
    Val two(2);
    throwing::countdown.copy = 1;
    EXPECT_THROW(v.store(two), throwing::Error);
    throwing::countdown.copy = 1;
    EXPECT_THROW(v = two, throwing::Error);
    auto expected = v.load();
    throwing::countdown.copy = 1;
    EXPECT_THROW(v.compare_exchange(expected, two), throwing::Error);
    EXPECT_EQ(expected.get(), held.get());
    throwing::countdown.move = 1;
    EXPECT_THROW(v.store(Val(3)), throwing::Error);
    throwing::countdown = {};
    EXPECT_EQ(v.load().get(), held.get());
    EXPECT_EQ(v.load()->v, 1);
    throwing::countdown.construct = 1;
    EXPECT_THROW((sgcl::concurrent::copy_on_write<Val>(std::in_place, 4)), throwing::Error);
}

// Writers at one value released together, many rounds: of two
// compare_exchange from one snapshot exactly one succeeds; two updates
// both land, one after the other
TEST(CopyOnWrite_Test, TwoWritersAtOneSnapshot) {
    sgcl::concurrent::copy_on_write<int> n;
    for (int round = 0; round < together::Rounds; ++round) {
        auto seen = n.load();
        std::atomic<int> won = {0};
        together::run(2, [&](int i) {
            auto mine = seen;
            won += n.compare_exchange(mine, *seen + 1 + i);
        });
        EXPECT_EQ(won.load(), 1);
        int now = *n.load();
        together::run(2, [&](int) {
            n.update([](int& x) { ++x; });
        });
        EXPECT_EQ(*n.load(), now + 2);
    }
}
