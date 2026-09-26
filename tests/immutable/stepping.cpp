//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

// The copy of an immutable node against the cycle (collector::stepper, as
// tests/core/stepping.cpp): a new version made after the flip shares its
// children with the old one, the old one is dropped at once, and the
// children are reachable from the new version alone, which the cycle does
// not trace (made after the flip). What keeps them is the barrier of the
// copy: on every word copied, or once on the source node it was copied
// from (im's unshaded copy plus a shade of the source).
namespace {
    using phase = collector::stepper::phase;

    class ImStepping
    : public testing::TestWithParam<unsigned> {
    protected:
        void arm(collector::stepper& s) {
            s.helpers(GetParam());
        }
    };

    struct Counted {
        static inline sgcl::atomic<int> alive = 0;
        explicit Counted(int v) : value(v) { ++alive; }
        ~Counted() { --alive; }
        int value;
    };

    void settle(collector::stepper& s) {
        s.full(true);
        collector::clear_stack(SIZE_MAX);
        s.finish_cycle();
        collector::clear_stack(SIZE_MAX);
        s.finish_cycle();
    }

    // A vector of n objects: a trie of several leaves under the root and a tail
    sgcl::immutable::vector<tracked_ptr<Counted>> counted(int n) {
        sgcl::immutable::vector<tracked_ptr<Counted>> v;
        for (int i = 0; i < n; ++i) {
            v = v.push_back(make_tracked<Counted>(i));
        }
        return v;
    }
}

TEST_P(ImStepping, VectorSetAfterTheFlipKeepsTheSharedLeaves) {
    collector::stepper s;
    arm(s);
    settle(s);
    Counted::alive = 0;
    const int n = 4 * 32;                               // three leaves in the trie, one in the tail
    sgcl::immutable::vector<tracked_ptr<Counted>> v;
    off_frame([&] { v = counted(n); });                 // built in a frame of its own: the versions on the way leave no word here
    settle(s);                                          // everything registered, the old version's nodes marked in a past cycle only
    EXPECT_EQ(Counted::alive, n);
    EXPECT_EQ(s.advance_to(phase::flipped), phase::flipped);   // a full cycle begins: nothing traced yet
    off_frame([&] {
        auto w = v.set(0, make_tracked<Counted>(-1));   // the root branch copied: leaves 1 and 2 shared, leaf 0 replaced
        v = w;                                          // the old root dropped: its leaves are reachable through the new root alone
    });
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, n + 1) << "an element shared with the dropped version was swept";
    off_frame([&] {                                     // the walk in a frame of its own: a pointer it leaves would root a leaf
        for (int i = 1; i < n; ++i) {
            ASSERT_EQ(v[i]->value, i);
        }
        EXPECT_EQ(v[0]->value, -1);
        v = sgcl::immutable::vector<tracked_ptr<Counted>>();
    });
    settle(s);
    EXPECT_EQ(Counted::alive, 0);
}

TEST_P(ImStepping, MapInsertAfterTheFlipKeepsTheSharedEntries) {
    collector::stepper s;
    arm(s);
    settle(s);
    Counted::alive = 0;
    const int n = 40000;                                // four levels of the trie: a copied node below the root shares subtries
    sgcl::immutable::map<int, tracked_ptr<Counted>> m;
    off_frame([&] {
        for (int i = 0; i < n; ++i) {
            m = m.insert(i, make_tracked<Counted>(i));
        }
    });
    settle(s);
    EXPECT_EQ(Counted::alive, n);
    EXPECT_EQ(s.advance_to(phase::flipped), phase::flipped);
    off_frame([&] {
        auto m2 = m.insert(n, make_tracked<Counted>(n));   // the path copied, every other subtrie shared
        m = m2;
    });
    collector::clear_stack(SIZE_MAX);
    s.finish_cycle();
    EXPECT_EQ(Counted::alive, n + 1) << "an entry shared with the dropped version was swept";
    off_frame([&] {
        for (int i = 0; i <= n; ++i) {
            auto p = m.try_get(i);
            ASSERT_TRUE(p);
            ASSERT_EQ((*p)->value, i);
        }
        m = sgcl::immutable::map<int, tracked_ptr<Counted>>();
    });
    settle(s);
    EXPECT_EQ(Counted::alive, 0);
}

INSTANTIATE_TEST_SUITE_P(Helpers, ImStepping, testing::Values(0u, 2u), [](const testing::TestParamInfo<unsigned>& info) {
    return info.param ? "parallel" : "single";
});
