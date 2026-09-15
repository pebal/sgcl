//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// root_ptr: a root that lives anywhere, a managed holder of its own under
// a unique_ptr, the object reachable while the root_ptr exists.
#include "types.h"

#include <map>
#include <unordered_map>
#include <vector>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        tracked_ptr<Node> next;
        inline static std::atomic<int> alive = {0};
    };

    struct Derived : Node {
        explicit Derived(int v) : Node(v) {}
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    // Roots on the unmanaged heap. Not globals: a root_ptr makes its
    // holder when constructed, and one made before main would be a live
    // object for the whole run, which the suite's counts do not expect.
    std::vector<root_ptr<Node>>* registry;
}

TEST(RootPtr_Tests, ARootAnywhereKeepsItsObject) {
    settle();
    const int before = Node::alive.load();
    registry = new std::vector<root_ptr<Node>>();
    auto* held = new root_ptr<Node>();                             // a root on the heap, as a global would be
    off_frame([&] {
        registry->push_back(make_tracked<Node>(1));                // from make_tracked
        tracked_ptr t = make_tracked<Node>(2);
        registry->emplace_back(t);                                 // from a tracked_ptr
        *held = t;
        gc::tracked_ptr g = gc::make_tracked<Node>(3);
        registry->push_back(root_ptr<Node>(g));                    // from a gc::tracked_ptr
        registry->push_back(root_ptr<Node>(make_tracked<Derived>(4)));   // a derived object
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 4);
    off_frame([&] {                                                // the reads in a frame of their own: what they spill is cleared
        EXPECT_EQ((*registry)[0]->value, 1);
        EXPECT_EQ((*(*registry)[1]).value, 2);
        EXPECT_EQ((*registry)[2]->value, 3);
        EXPECT_EQ((*registry)[3]->value, 4);
        EXPECT_TRUE((*registry)[3].is<Derived>());
        EXPECT_EQ(*held, (*registry)[1]);
        EXPECT_NE(*held, (*registry)[0]);
        delete registry;                                           // the holders destroyed now, the objects unreferenced
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);                     // held still
    delete held;
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(RootPtr_Tests, CopiesHaveHoldersOfTheirOwnMovesLeaveNull) {
    settle();
    const int before = Node::alive.load();
    root_ptr<Node> a;
    EXPECT_FALSE(a);
    EXPECT_EQ(a.get(), nullptr);
    EXPECT_TRUE(a == nullptr);
    off_frame([&] {
        a = make_tracked<Node>(1);
    });
    root_ptr<Node> copy = a;
    EXPECT_EQ(copy, a);
    root_ptr<Node> moved = std::move(copy);
    EXPECT_FALSE(copy);
    EXPECT_EQ(moved, a);
    copy = std::move(moved);
    EXPECT_FALSE(moved);
    EXPECT_EQ(copy, a);
    a = nullptr;
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);                     // copy holds it
    swap(a, copy);
    EXPECT_TRUE(a && !copy);
    a.reset();
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(RootPtr_Tests, ConvertsToATrackedPtrAndBack) {
    settle();
    const int before = Node::alive.load();
    root_ptr<Node> root;
    off_frame([&] {
        root = make_tracked<Node>(1);
        tracked_ptr<Node> t = root.ptr();                          // the tracked_ptr, where one may live
        tracked_ptr<Node> implicit = root;
        EXPECT_EQ(t, root);
        EXPECT_EQ(implicit, root);
        t->next = make_tracked<Node>(2);                           // reachable through the root
        gc::tracked_ptr<Node> g = root.ptr();
        EXPECT_EQ(g, root);
        root_ptr<Node> from_gc = g;
        EXPECT_EQ(from_gc, root);
        root_ptr<const Node> to_const = root;                      // converting copy
        EXPECT_EQ(to_const->value, 1);
        root.reset(t->next);
        EXPECT_EQ(root->value, 2);
        EXPECT_EQ(root.type(), typeid(Node));
        EXPECT_EQ(root.as<Node>(), root);
        EXPECT_EQ(root.as<Derived>(), nullptr);
        root = t;
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 2);
    root = nullptr;
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(RootPtr_Tests, InStdContainersAndUnderGc) {
    settle();
    const int before = Node::alive.load();
    auto* by_key = new std::map<root_ptr<Node>, int>();
    auto* hashed = new std::unordered_map<root_ptr<Node>, int>();
    off_frame([&] {
        for (int i = 0; i < 10; ++i) {
            root_ptr<Node> r = make_tracked<Node>(i);
            (*by_key)[r] = i;
            (*hashed)[r] = i;
        }
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 10);
    EXPECT_EQ(by_key->size(), 10u);
    for (auto& [root, i] : *by_key) {
        EXPECT_EQ(root->value, i);
        EXPECT_EQ((*hashed)[root], i);
    }
    static_assert(std::is_same_v<gc::root_ptr<Node>, root_ptr<Node>>);
    delete by_key;
    delete hashed;
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}
