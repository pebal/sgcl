//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The generator, a coroutine whose frame lives on the managed heap: the
// tracked pointers in its suspended frame are roots.
#include "tests/types.h"

#include <memory>
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
