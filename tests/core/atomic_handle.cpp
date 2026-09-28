//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <thread>
#include <vector>

namespace {
    // A handle as the library's are: one tracked word to the object
    // inside, made by the library, taking part in the atomics by naming
    // its word to detail::HandleWord and taking a constructor from one
    struct CounterState {
        int value = 0;
    };

    class counter {
    public:
        counter()
        : _state(sgcl::make_tracked<CounterState>()) {
        }

        explicit counter(int v)
        : _state(sgcl::make_tracked<CounterState>(v)) {
        }

        int value() const noexcept {
            return _state->value;
        }

        void set(int v) const noexcept {
            _state->value = v;
        }

        friend bool operator==(const counter& a, const counter& b) noexcept {
            return a._state == b._state;
        }

    private:
        friend struct sgcl::detail::HandleWord;

        counter(sgcl::detail::FromWord, const sgcl::tracked_ptr<CounterState>& w) noexcept
        : _state(w) {
        }

        sgcl::tracked_ptr<CounterState>& _handle_word() noexcept {
            return _state;
        }

        const sgcl::tracked_ptr<CounterState>& _handle_word() const noexcept {
            return _state;
        }

        sgcl::tracked_ptr<CounterState> _state;
    };

    struct Holder {
        counter current;
    };
}

// The concept: a handle names its word and is made from one; a string
// is one, a tracked_ptr, a number and a two-word type are not
static_assert(sgcl::req::handle<counter>);
static_assert(sgcl::req::handle<sgcl::string>);
static_assert(!sgcl::req::handle<sgcl::tracked_ptr<int>>);
static_assert(!sgcl::req::handle<int>);
static_assert(!sgcl::req::handle<sgcl::slice<const char>>);
static_assert(std::is_same_v<sgcl::atomic<counter>::value_type, counter>);
static_assert(std::is_same_v<sgcl::atomic_ref<counter>::value_type, counter>);

TEST(AtomicHandle_Test, LoadStoreExchange) {
    sgcl::atomic<counter> a;                 // the handle's own default: an object
    EXPECT_EQ(a.load().value(), 0);
    counter one(1), two(2);
    a.store(one);
    EXPECT_EQ(a.load(), one);                // the same object, nothing copied
    counter got = a;                         // the conversion
    EXPECT_EQ(got, one);
    a = two;
    EXPECT_EQ(a.load(), two);
    EXPECT_EQ(got, one);                     // what was loaded stays what it was
    counter old = a.exchange(one);
    EXPECT_EQ(old, two);
    EXPECT_EQ(a.load(), one);
    EXPECT_EQ(sizeof(a), sizeof(void*));
    sgcl::atomic<counter> b(two);
    EXPECT_EQ(b.load(), two);
}

TEST(AtomicHandle_Test, CompareExchangeByIdentity) {
    counter one(1), two(2), other(1);       // other: the same value, another object
    sgcl::atomic<counter> a(one);
    counter expected = other;
    EXPECT_FALSE(a.compare_exchange_strong(expected, two));   // not the object held
    EXPECT_EQ(expected, one);                                  // expected: what is there
    EXPECT_TRUE(a.compare_exchange_strong(expected, two));
    EXPECT_EQ(a.load(), two);
    expected = one;
    EXPECT_FALSE(a.compare_exchange_weak(expected, one, std::memory_order_acq_rel, std::memory_order_acquire));
    EXPECT_EQ(expected, two);
    while (!a.compare_exchange_weak(expected, one)) {
    }
    EXPECT_EQ(a.load(), one);
}

TEST(AtomicHandle_Test, AtomicRefOverAMemberAndARooted) {
    sgcl::tracked_ptr holder = sgcl::make_tracked<Holder>();
    counter one(1), two(2);
    sgcl::atomic_ref view(holder->current);
    static_assert(std::is_same_v<decltype(view), sgcl::atomic_ref<counter>>);
    view.store(one);
    EXPECT_EQ(holder->current, one);
    counter expected = one;
    EXPECT_TRUE(view.compare_exchange_strong(expected, two));
    EXPECT_EQ(holder->current, two);
    EXPECT_EQ(view.load(), two);
    // a global's handle: in a rooted, the view over the handle it holds
    sgcl::rooted<counter> root(one);
    sgcl::atomic_ref r(*root);
    r.store(two);
    EXPECT_EQ(*root, two);
    EXPECT_EQ(r.exchange(one), two);
    EXPECT_EQ(root->value(), 1);
}

TEST(AtomicHandle_Test, StringsGoThroughTheSameAtomic) {
    sgcl::atomic<sgcl::string> a("a");
    sgcl::string made("b");
    a.store(made);
    EXPECT_EQ(a.load().object(), made.object());
    sgcl::string expected = a.load();
    EXPECT_TRUE(a.compare_exchange_strong(expected, sgcl::string("c")));
    EXPECT_EQ(a.load(), "c");
    sgcl::atomic_ref view(made);
    view.store(sgcl::string("d"));
    EXPECT_EQ(made, "d");
}

// Many threads exchanging handles through one atomic: every handle
// stored comes out exactly once, and every load holds an object
TEST(AtomicHandle_Test, ExchangesUnderContention) {
    constexpr int Threads = 8, Rounds = 2000;
    sgcl::atomic<counter> a(counter(-1));
    std::atomic<int> seen[Threads * Rounds + 1] = {};
    std::vector<std::thread> threads;
    for (int t = 0; t < Threads; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < Rounds; ++i) {
                counter old = a.exchange(counter(t * Rounds + i));
                ++seen[old.value() + 1];
                counter now = a.load();
                EXPECT_GE(now.value(), -1);
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    ++seen[a.load().value() + 1];
    for (int i = 0; i <= Threads * Rounds; ++i) {
        EXPECT_EQ(seen[i].load(), 1) << i;
    }
}
