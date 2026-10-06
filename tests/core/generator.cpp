//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The generator, a coroutine whose frame lives on the managed heap: the
// tracked pointers in its suspended frame are roots.
#include "tests/types.h"

#include <memory>
#include <ranges>
#include <stdexcept>
#include <vector>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        tracked_ptr<Node> next;
        inline static sgcl::atomic<int> alive = {0};
    };

    // Yields nodes, keeping a chain of the yielded ones in a local
    generator<tracked_ptr<Node>> nodes(int count) {
        tracked_ptr<Node> chain;
        for (int i = 0; i < count; ++i) {
            tracked_ptr<Node> n = make_tracked<Node>(i);
            n->next = chain;
            chain = n;
            co_yield n;
        }
    }

    struct Owned {
        explicit Owned(int v) : value(v) { ++alive; }
        ~Owned() { --alive; }
        int value;
        inline static int alive = 0;
    };

    // Yields values that can only be moved, not copied
    generator<std::unique_ptr<Owned>> owned(int count) {
        for (int i = 0; i < count; ++i) {
            co_yield std::make_unique<Owned>(i);
        }
    }

    generator<int> counting(int count) {
        for (int i = 0; i < count; ++i) {
            co_yield i;
        }
    }

    generator<int> failing_after(int count) {
        for (int i = 0; i < count; ++i) {
            co_yield i;
        }
        throw std::runtime_error("after");
    }

    void settle() {
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Generator_Tests, AGeneratorHoldsItsChainAcrossYields) {
    settle();                            // the garbage of the previous test, before the baseline
    const int before = Node::alive.load();
    int seen = 0;
    off_frame([&] {
        for (auto& n : nodes(5)) {
            collector::force_collect(true);
            EXPECT_EQ(n->value, seen);
            ++seen;
            // the chain of the earlier nodes hangs from the local in the frame
            int length = 0;
            for (auto p = n; p; p = p->next) {
                ++length;
            }
            EXPECT_EQ(length, seen);
        }
    });
    EXPECT_EQ(seen, 5);
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Generator_Tests, AYieldedUniquePtrIsMovedOutOfTheFrame) {
    std::vector<std::unique_ptr<Owned>> taken;
    {
        auto g = owned(4);
        ASSERT_TRUE(g.next());
        taken.push_back(std::move(g.value()));
        EXPECT_EQ(g.value(), nullptr);   // the frame keeps the moved-from value until the next co_yield
        EXPECT_EQ(taken.back()->value, 0);
        ASSERT_TRUE(g.next());
        ASSERT_NE(g.value(), nullptr);   // the next co_yield replaced it
        EXPECT_EQ(g.value()->value, 1);
    }                                    // the generator dropped with 1 in its frame and 2, 3 never yielded
    EXPECT_EQ(Owned::alive, 1);
    for (auto& p : owned(3)) {
        taken.push_back(std::move(p));
    }
    ASSERT_EQ(taken.size(), 4u);
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(taken[i + 1]->value, i);
    }
    EXPECT_EQ(Owned::alive, 4);          // the moved values outlive their generators
    taken.clear();
    EXPECT_EQ(Owned::alive, 0);
}

// The iterator is lazy (the coroutine runs at the first look at an element,
// ++ only marks it used) and the end is std::default_sentinel: an input
// range for the views, a move-only element included
static_assert(std::input_iterator<generator<int>::iterator>);
static_assert(std::ranges::input_range<generator<int>&>);
static_assert(std::ranges::input_range<generator<std::unique_ptr<Owned>>&>);
static_assert(std::same_as<std::ranges::sentinel_t<generator<int>&>, std::default_sentinel_t>);

TEST(Generator_Tests, StdViews) {
    auto g = counting(10);
    int sum = 0;
    for (int v : g | std::views::filter([](int v) { return v % 2 == 1; })
                   | std::views::transform([](int v) { return v * 10; })) {
        sum += v;
    }
    EXPECT_EQ(sum, 250);              // 10 + 30 + 50 + 70 + 90
}

// views::take(n) runs the coroutine to the n-th element and no further: the
// next range-for over the generator goes on from the element after it
TEST(Generator_Tests, StdViewsTakeLosesNothing) {
    auto g = counting(6);
    std::vector<int> first, rest;
    for (int v : g | std::views::take(2)) {
        first.push_back(v);
    }
    for (int v : g) {
        rest.push_back(v);
    }
    EXPECT_EQ(first, (std::vector<int>{0, 1}));
    EXPECT_EQ(rest, (std::vector<int>{2, 3, 4, 5}));
}

TEST(Generator_Tests, BreakLosesNothing) {
    auto g = counting(5);
    for (int v : g) {
        if (v == 1) {
            break;
        }
    }
    ASSERT_TRUE(g.next());
    EXPECT_EQ(g.value(), 2);
}

TEST(Generator_Tests, BeginRunsNothingAndTheEndIsALook) {
    auto empty = counting(0);
    EXPECT_TRUE(empty.begin() == empty.end());
    auto g = counting(3);
    auto it = g.begin();              // nothing run yet
    ASSERT_TRUE(g.next());            // so next() takes the first element
    EXPECT_EQ(g.value(), 0);
    EXPECT_FALSE(it == g.end());      // the look runs to the second
    EXPECT_EQ(*it, 1);
    EXPECT_EQ(*it, 1);                // a second look holds the same one
    ++it;
    EXPECT_EQ(*it, 2);
    ++it;
    EXPECT_TRUE(it == g.end());
}

TEST(Generator_Tests, AnExceptionComesOutOfTheLook) {
    auto g = failing_after(2);
    std::vector<int> got;
    EXPECT_THROW(
        {
            for (int v : g) {
                got.push_back(v);
            }
        },
        std::runtime_error);
    EXPECT_EQ(got, (std::vector<int>{0, 1}));
}
