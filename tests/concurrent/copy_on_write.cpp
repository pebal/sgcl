//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

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
    sgcl::copy_on_write<Config> cfg(std::in_place, "a", 1);
    sgcl::copy_on_write<Config>::snapshot s1 = cfg.load();
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
    sgcl::copy_on_write<int> n;
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
    sgcl::copy_on_write<sgcl::vector<tracked_ptr<Baz>>> list;
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
    sgcl::copy_on_write<sgcl::vector<tracked_ptr<Baz>>> list;
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
        sgcl::copy_on_write<Config> cfg;
    };
    tracked_ptr h = make_tracked<Holder>();
    h->cfg.update([](Config& c) { c.host = "x"; });
    EXPECT_EQ(h->cfg.load()->host, "x");
}

// Eight writers increment a counter through update: every increment
// lands exactly once, whatever the retries
TEST(CopyOnWrite_Test, UpdatesAreLinearizable) {
    sgcl::copy_on_write<long> n(0);
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
    sgcl::copy_on_write<Pair> p;
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
