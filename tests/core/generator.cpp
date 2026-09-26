//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The generator, a coroutine whose frame lives on the managed heap: the
// tracked pointers in its suspended frame are roots.
#include "tests/types.h"

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
