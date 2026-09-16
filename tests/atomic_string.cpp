//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
    SGCL_ALWAYS_INLINE size_t live_without_cells() {
        sgcl::detail::cell_allocator.release();
        collector::clear_stack(SIZE_MAX);
        collector::force_collect(true);
        return collector::get_live_object_count();
    }
}

TEST(AtomicString_Test, LoadStore) {
    sgcl::atomic<sgcl::string> a;
    EXPECT_TRUE(a.load().empty());          // the empty string: a null word
    a.store("x");
    sgcl::string s = a.load();
    EXPECT_EQ(s, "x");
    a = "yy";
    EXPECT_EQ(a.load(), "yy");
    EXPECT_EQ(s, "x");                      // what was loaded stays what it was
    sgcl::string conv = a;                  // the conversion
    EXPECT_EQ(conv, "yy");
    sgcl::string made("zzz");
    a.store(made);
    EXPECT_EQ(a.load().object(), made.object());   // the same object, nothing copied
    a.store(sgcl::string());
    EXPECT_TRUE(a.load().empty());
    sgcl::atomic<sgcl::string> b("init");
    EXPECT_EQ(b.load(), "init");
    EXPECT_EQ(sizeof(a), sizeof(void*));
}

TEST(AtomicString_Test, ExchangeAndCompareExchangeByIdentity) {
    sgcl::atomic<sgcl::string> a("a");
    sgcl::string old = a.exchange("b");
    EXPECT_EQ(old, "a");
    EXPECT_EQ(a.load(), "b");

    sgcl::string expected = a.load();
    EXPECT_TRUE(a.compare_exchange_strong(expected, "c"));
    EXPECT_EQ(a.load(), "c");
    EXPECT_EQ(expected, "b");               // untouched on success

    sgcl::string same_text("c");            // equal characters, another object
    EXPECT_FALSE(a.compare_exchange_strong(same_text, "d"));
    EXPECT_EQ(same_text, "c");
    EXPECT_EQ(same_text.object(), a.load().object());   // on failure: the current one, the object itself
    EXPECT_TRUE(a.compare_exchange_strong(same_text, "d"));
    EXPECT_EQ(a.load(), "d");

    sgcl::string e = a.load();
    while (!a.compare_exchange_weak(e, "e")) {
    }
    EXPECT_EQ(a.load(), "e");

    sgcl::string empty;
    sgcl::atomic<sgcl::string> n;
    EXPECT_TRUE(n.compare_exchange_strong(empty, "first"));   // from the empty string
    EXPECT_EQ(n.load(), "first");
    EXPECT_FALSE(n.compare_exchange_strong(empty, "second"));
    EXPECT_EQ(empty, "first");
}

TEST(AtomicString_Test, WaitNotify) {
    sgcl::atomic<sgcl::string> a("start");
    sgcl::string start = a.load();
    gc::atomic<bool> woke = {false};
    std::thread t([&] {
        a.wait(start);
        woke = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(woke.load());
    a.store("go");
    a.notify_one();
    t.join();
    EXPECT_TRUE(woke.load());
}

TEST(AtomicString_Test, OldStringsReclaimed) {
    const size_t before = collector::get_live_object_count();
    sgcl::atomic<sgcl::string> a;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            a.store(sgcl::string(std::to_string(i)));
        }
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);   // the last one
    sgcl::string kept;
    off_frame([&] {
        kept = a.load();
        a.store("new");
    });
    EXPECT_EQ(collector::get_live_object_count(), before + 2u);   // the one kept, the new one
    kept = sgcl::string();
    EXPECT_EQ(collector::get_live_object_count(), before + 1u);
}

TEST(AtomicString_Test, GcLivesAnywhere) {
    const size_t before = collector::get_live_object_count();
    auto a = std::make_unique<gc::atomic<gc::string>>();   // in unmanaged memory
    std::vector<gc::string> seen;                            // gc strings in a std container
    off_frame([&] {
        for (int i = 0; i < 5; ++i) {
            a->store(gc::string(std::to_string(i)));
            seen.push_back(a->load());
        }
        for (int i = 0; i < 5; ++i) {
            EXPECT_EQ(seen[size_t(i)], std::to_string(i));
        }
        gc::string e = a->load();
        EXPECT_TRUE(a->compare_exchange_strong(e, "last"));
        EXPECT_EQ(a->load(), "last");
    });
    seen.clear();
    a.reset();
    EXPECT_EQ(live_without_cells(), before);
}

TEST(AtomicString_Test, InsideManagedObjectAndOtherCharacters) {
    struct Holder {
        sgcl::atomic<sgcl::string> name;
        sgcl::atomic<sgcl::u16string> wide;
    };
    tracked_ptr h = make_tracked<Holder>();
    h->name = "n";
    h->wide = u"wide";
    EXPECT_EQ(h->name.load(), "n");
    EXPECT_EQ(h->wide.load(), u"wide");
    sgcl::atomic<sgcl::wstring> w(L"w");
    EXPECT_EQ(w.load(), L"w");
}

// Writers store strings of n copies of the digit n; readers load and
// check that every string they see is whole (its length is its digit,
// every character the same) while the collector runs
TEST(AtomicString_Test, ReadersSeeWholeStrings) {
    sgcl::atomic<sgcl::string> a("1");
    gc::atomic<bool> torn = {false};
    gc::atomic<bool> stop = {false};
    off_frame([&] {
        std::vector<std::thread> ws;
        for (int t = 0; t < 6; ++t) {
            ws.emplace_back([&] {
                while (!stop.load(std::memory_order_relaxed)) {
                    sgcl::string s = a.load();
                    if (s.empty() || s.size() != size_t(s[0] - '0') || s.view() != std::string(s.size(), s[0])) {
                        torn = true;
                    }
                }
            });
        }
        for (int t = 0; t < 2; ++t) {
            ws.emplace_back([&, t] {
                for (int i = 0; i < 20000; ++i) {
                    int n = 1 + (i % 9);
                    a.store(sgcl::string(size_t(n), char('0' + n)));   // "1", "22", "333", ...
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
}
