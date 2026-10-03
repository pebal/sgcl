//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of atomic and atomic_ref (DESIGN 408): of a tracked_ptr
// and of a handle (string). A null and a default atomic at every member;
// the value it holds as the argument of its own operations (a store of
// its own load, a compare-exchange expecting and storing the same, an
// atomic_ref whose expected pointer is the referenced word itself); an
// empty unique_ptr; a wait that must not block; threads together at the
// one compare-exchange that can win.
#include "tests/types.h"

#include <atomic>
#include <thread>
#include <vector>

TEST(AtomicBoundaries_Tests, TrackedNullAtEveryMember) {
    atomic<tracked_ptr<Int>> a;
    EXPECT_FALSE(a.load());
    EXPECT_FALSE(tracked_ptr<Int>(a));
    EXPECT_TRUE(a.is_lock_free());
    EXPECT_FALSE(a.exchange(nullptr));
    EXPECT_FALSE(a.exchange(tracked_ptr<Int>()));
    tracked_ptr<Int> e;
    EXPECT_TRUE(a.compare_exchange_strong(e, nullptr));   // null expected, null there
    EXPECT_TRUE(a.compare_exchange_strong(e, tracked_ptr<Int>()));
    EXPECT_TRUE(a.compare_exchange_strong(e, nullptr, std::memory_order_acq_rel, std::memory_order_acquire));
    while (!a.compare_exchange_weak(e, nullptr)) {
        EXPECT_FALSE(e);
    }
    a.store(unique_ptr<Int>());   // an empty owner: null stored
    EXPECT_FALSE(a.load());
    a = unique_ptr<Int>();
    EXPECT_FALSE(a.load());
    a.wait(make_tracked<Int>(1));   // the word is not that pointer: returns at once
    a.notify_one();
    a.notify_all();
    atomic<tracked_ptr<Int>> n(nullptr);
    EXPECT_FALSE(n.load());
    atomic<tracked_ptr<Int>> u(unique_ptr<Int>{});
    EXPECT_FALSE(u.load());
    atomic<tracked_ptr<Int>> t(tracked_ptr<Int>{});
    EXPECT_FALSE(t.load());
    EXPECT_EQ(a = nullptr, nullptr);
}

TEST(AtomicBoundaries_Tests, TrackedItsOwnValueAsTheArgument) {
    atomic<tracked_ptr<Int>> a(make_tracked<Int>(4));
    tracked_ptr<Int> p = a.load();
    a.store(a.load());
    EXPECT_EQ(a.load(), p);
    a = a.load();
    EXPECT_EQ(a.load(), p);
    EXPECT_EQ(a.exchange(a.load()), p);
    EXPECT_EQ(a.load(), p);
    tracked_ptr<Int> e = p;
    EXPECT_TRUE(a.compare_exchange_strong(e, e));   // expected and desired the same pointer
    EXPECT_EQ(a.load(), p);
    EXPECT_EQ(e, p);
    tracked_ptr<Int> other = make_tracked<Int>(5);
    tracked_ptr<Int> stale = other;
    EXPECT_FALSE(a.compare_exchange_strong(stale, stale));   // fails: stale is set to the word
    EXPECT_EQ(stale, p);
    EXPECT_EQ(a.load(), p);
    tracked_ptr<Int> null_expected;
    EXPECT_FALSE(a.compare_exchange_strong(null_expected, nullptr));
    EXPECT_EQ(null_expected, p);
    a.wait(nullptr);   // the word is not null: returns at once
    collector::force_collect(true);
    EXPECT_EQ(*a.load(), 4);
    EXPECT_EQ(*p, 4);
}

TEST(AtomicBoundaries_Tests, TrackedSwappedForUniqueAndBack) {
    atomic<tracked_ptr<Int>> a;
    auto u = make_tracked<Int>(9);
    Int* raw = u.get();
    a.store(std::move(u));
    EXPECT_FALSE(u);
    EXPECT_EQ(a.load().get(), raw);
    a = make_tracked<Int>(10);   // the previous object is the collector's now
    EXPECT_EQ(*a.load(), 10);
    collector::force_collect(true);
    EXPECT_EQ(*a.load(), 10);
}

TEST(AtomicBoundaries_Tests, TrackedThreadsAtTheOneExchangeThatWins) {
    constexpr int threads = 8;
    atomic<tracked_ptr<int>> a;   // int, not Int: Int's counter is a plain static
    std::atomic<int> winners{0};
    std::atomic<int> ready{0};
    std::vector<std::thread> ts;
    for (int i = 0; i < threads; ++i) {
        ts.emplace_back([&, i] {
            ++ready;
            while (ready.load() < threads) {}
            tracked_ptr<int> e;
            if (a.compare_exchange_strong(e, make_tracked<int>(i))) {
                ++winners;
            } else {
                EXPECT_TRUE(e);   // the loser holds the winner's pointer
            }
        });
    }
    for (auto& t : ts) {
        t.join();
    }
    EXPECT_EQ(winners.load(), 1);
    EXPECT_TRUE(a.load());
}

TEST(AtomicBoundaries_Tests, TrackedWaitWokenByAStore) {
    atomic<tracked_ptr<Int>> a;
    std::thread waiter([&] {
        a.wait(nullptr);
        EXPECT_TRUE(a.load());
    });
    a.store(make_tracked<Int>(1));
    a.notify_all();
    waiter.join();
}

TEST(AtomicBoundaries_Tests, RefOverItsOwnWordAsTheArgument) {
    tracked_ptr<Int> word = make_tracked<Int>(3);
    tracked_ptr<Int> p = word;
    atomic_ref<tracked_ptr<Int>> r(word);
    EXPECT_TRUE(r.compare_exchange_strong(word, word));   // expected is the word itself: equal
    EXPECT_EQ(word, p);
    EXPECT_TRUE(r.compare_exchange_strong(word, nullptr));   // expected is the word, replaced
    EXPECT_FALSE(word);
    tracked_ptr<Int> e = p;
    EXPECT_FALSE(r.compare_exchange_strong(e, p));
    EXPECT_FALSE(e);
    r.store(p);
    r.store(r.load());
    r = word;   // the referenced word assigned to itself
    EXPECT_EQ(word, p);
    EXPECT_EQ(r.exchange(word), p);
    EXPECT_EQ(word, p);
    atomic_ref<tracked_ptr<Int>> copy(r);
    EXPECT_EQ(&copy.ref, &word);
    EXPECT_EQ(copy.load(), p);
    tracked_ptr<Int> null_word;
    atomic_ref<tracked_ptr<Int>> over_null(null_word);
    EXPECT_FALSE(over_null.load());
    over_null.wait(p);
    over_null = unique_ptr<Int>();
    EXPECT_FALSE(null_word);
}

TEST(AtomicBoundaries_Tests, RefOverARootAndARootedMember) {
    root_ptr<Int> root = make_tracked<Int>(1);
    atomic_ref<tracked_ptr<Int>> r(root.ptr());
    r.store(make_tracked<Int>(2));
    collector::force_collect(true);
    EXPECT_EQ(*root, 2);
    r = nullptr;
    EXPECT_FALSE(root);
}

TEST(AtomicBoundaries_Tests, HandleDefaultAndEmpty) {
    atomic<string> a;
    EXPECT_TRUE(a.load().empty());
    EXPECT_TRUE(string(a).empty());
    EXPECT_TRUE(a.is_lock_free());
    string e;
    EXPECT_TRUE(a.compare_exchange_strong(e, string()));   // a default string expected, one there
    string empty_made = "";
    string expected;
    EXPECT_TRUE(a.compare_exchange_strong(expected, empty_made));
    EXPECT_EQ(a.load(), "");
    EXPECT_TRUE(a.exchange(string()).empty());
    a.wait(string("x"));   // not that object: returns at once
    a.notify_one();
    a.notify_all();
    atomic<string> from_default(string{});
    EXPECT_TRUE(from_default.load().empty());
}

TEST(AtomicBoundaries_Tests, HandleItsOwnValueAsTheArgument) {
    atomic<string> a(string("abc"));
    string s = a.load();
    a.store(a.load());
    EXPECT_EQ(a.load(), "abc");
    a = a.load();
    EXPECT_EQ(a.exchange(a.load()), "abc");
    string e = a.load();
    EXPECT_TRUE(a.compare_exchange_strong(e, e));   // the same object expected and stored
    string equal_but_other = string("ab") + "c";
    EXPECT_FALSE(a.compare_exchange_strong(equal_but_other, string("x")));   // identity, not contents
    EXPECT_EQ(equal_but_other, "abc");
    EXPECT_TRUE(a.compare_exchange_strong(equal_but_other, string("x")));    // now the object loaded
    EXPECT_EQ(a.load(), "x");
    string moved = std::move(s);   // a string moved from is another handle of the same object
    EXPECT_EQ(s, "abc");
    a.store(s);
    string s2 = s;
    EXPECT_TRUE(a.compare_exchange_strong(s2, moved));   // the same object through the moved-from one
    collector::force_collect(true);
    EXPECT_EQ(a.load(), "abc");
}

TEST(AtomicBoundaries_Tests, HandleRefOverItsOwnWord) {
    string word = "w";
    atomic_ref<string> r(word);
    EXPECT_TRUE(r.compare_exchange_strong(word, word));
    EXPECT_EQ(word, "w");
    EXPECT_TRUE(r.compare_exchange_strong(word, string("v")));
    EXPECT_EQ(word, "v");
    r = word;
    EXPECT_EQ(r.exchange(word), "v");
    atomic_ref<string> copy(r);
    EXPECT_EQ(&copy.ref, &word);
    string empty;
    atomic_ref<string> over_empty(empty);
    EXPECT_TRUE(over_empty.load().empty());
    over_empty.wait(word);
    over_empty.store(word);
    EXPECT_EQ(empty, "v");
}

TEST(AtomicBoundaries_Tests, PlainValuesAtTheirLimits) {
    atomic<int64_t> i(INT64_MAX);
    EXPECT_EQ(i.fetch_add(1), INT64_MAX);   // wraps as std::atomic does: two's complement, no UB
    EXPECT_EQ(i.load(), INT64_MIN);
    atomic<uint64_t> u(0);
    EXPECT_EQ(u.fetch_sub(1), 0u);
    EXPECT_EQ(u.load(), UINT64_MAX);
    int64_t x = INT64_MIN;
    atomic_ref<int64_t> r(x);
    EXPECT_EQ(r.fetch_sub(1), INT64_MIN);
    EXPECT_EQ(x, INT64_MAX);
    int64_t e = INT64_MAX;
    EXPECT_TRUE(r.compare_exchange_strong(e, e));
}
