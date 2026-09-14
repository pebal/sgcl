//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <atomic>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

// What holds an object (collector::get_referrers, get_path_to_root,
// explain): every kind of holder, and a chain that ends at a root.
namespace {
    using kind = collector::referrer::kind;

    struct Leaf { int value = 7; };
    struct Node {
        tracked_ptr<Leaf> leaf;
        tracked_ptr<Node> next;
    };

    template<class T>
    bool holds(const std::vector<collector::referrer>& rs, kind k, const T* holder) {
        for (auto& r : rs) {
            if (r.from == k && r.holder == (const void*)holder) {
                return true;
            }
        }
        return false;
    }

    size_t count(const std::vector<collector::referrer>& rs, kind k) {
        size_t n = 0;
        for (auto& r : rs) {
            n += r.from == k;
        }
        return n;
    }
}

TEST(Referrers_Tests, AnObjectHeldByAMemberAndByTheStack) {
    tracked_ptr node = make_tracked<Node>();
    node->leaf = make_tracked<Leaf>();
    tracked_ptr leaf = node->leaf;                       // on this frame too
    auto [guard, rs] = collector::get_referrers(leaf.get());
    EXPECT_TRUE(holds(rs, kind::object, node.get()));    // the member
    EXPECT_TRUE(holds(rs, kind::stack, &leaf));          // the local, this frame being above the call
    for (auto& r : rs) {
        if (r.from == kind::object) {
            EXPECT_EQ(*r.type, typeid(Node));
            EXPECT_EQ(r.offset, offsetof(Node, leaf));
        }
    }
    EXPECT_EQ(count(rs, kind::weak), 0u);
}

// The chain leaves the calling thread's stack out: a structure held by a
// local of this frame is reached from no other root, so a leaf three
// nodes down is held by its node, and the chain ends where the calling
// thread holds the head. A unique_ptr at the top makes it a root.
TEST(Referrers_Tests, APathThroughObjectsToAUniquePtr) {
    unique_ptr<Node> head = make_tracked<Node>();
    head->next = make_tracked<Node>();
    head->next->next = make_tracked<Node>();
    head->next->next->leaf = make_tracked<Leaf>();
    auto leaf = head->next->next->leaf.get();
    auto [guard, path] = collector::get_path_to_root(leaf);
    ASSERT_EQ(path.size(), 4u);
    EXPECT_EQ(path[0].from, kind::object);               // the third node, at its leaf member
    EXPECT_EQ(*path[0].type, typeid(Node));
    EXPECT_EQ(path[0].offset, offsetof(Node, leaf));
    EXPECT_EQ(path[0].holder, (const void*)head->next->next.get());
    EXPECT_EQ(path[1].from, kind::object);               // the second node, at next
    EXPECT_EQ(path[1].offset, offsetof(Node, next));
    EXPECT_EQ(path[2].from, kind::object);               // the head, at next
    EXPECT_EQ(path[2].holder, (const void*)head.get());
    EXPECT_EQ(path[3].from, kind::unique);               // which the unique_ptr owns
    EXPECT_EQ(path[3].holder, (const void*)head.get());
}

// Held by this thread only: the chain ends on this stack, searched last
TEST(Referrers_Tests, HeldByThisThreadOnly) {
    tracked_ptr node = make_tracked<Node>();
    node->leaf = make_tracked<Leaf>();
    {
        auto [guard, path] = collector::get_path_to_root(node->leaf.get());
        ASSERT_EQ(path.size(), 2u);
        EXPECT_EQ(path[0].from, kind::object);           // the node
        EXPECT_EQ(path[0].holder, (const void*)node.get());
        EXPECT_EQ(path[1].from, kind::stack);            // held by this frame
    }
    auto [guard, path] = collector::get_path_to_root(node.get());   // one guard at a time: a guard pauses the collector the next call waits for
    ASSERT_EQ(path.size(), 1u);
    EXPECT_EQ(path[0].from, kind::stack);
    EXPECT_TRUE(detail::thread_stack.holds(path[0].holder));   // a word of this stack: `node`, or a temporary of this frame that holds the same
}

TEST(Referrers_Tests, HeldByABufferAndByAUniquePtr) {
    unique_ptr<vector<tracked_ptr<Leaf>>> owned = make_tracked<vector<tracked_ptr<Leaf>>>();   // the vector: a unique_ptr's object
    owned->push_back(make_tracked<Leaf>());
    owned->push_back(make_tracked<Leaf>());
    {
        auto [guard, path] = collector::get_path_to_root(owned->back().get());
        ASSERT_EQ(path.size(), 3u);
        EXPECT_EQ(path[0].from, kind::buffer);           // the vector's buffer, at the second element
        EXPECT_EQ(path[0].offset, sizeof(detail::ArrayBase) + sizeof(tracked_ptr<Leaf>));
        EXPECT_EQ(path[1].from, kind::object);           // the vector object, its pointer to the buffer
        EXPECT_EQ(*path[1].type, typeid(vector<tracked_ptr<Leaf>>));
        EXPECT_EQ(path[2].from, kind::unique);           // which a unique_ptr owns
        EXPECT_EQ(path[2].holder, (const void*)owned.get());
    }
    {
        auto [guard, rs] = collector::get_referrers(owned.get());
        EXPECT_TRUE(holds(rs, kind::unique, owned.get()));
    }
}

TEST(Referrers_Tests, HeldByACellOfAGcPointerInUnmanagedMemory) {
    auto kept = std::make_unique<gc::tracked_ptr<Leaf>>(make_tracked<Leaf>());   // a cell in a block, the only holder
    auto [guard, path] = collector::get_path_to_root(kept->get());
    ASSERT_EQ(path.size(), 2u);
    EXPECT_EQ(path[0].from, kind::cell);                 // the cell
    EXPECT_LT(path[0].offset, sizeof(detail::CellBlock));
    EXPECT_EQ(path[1].from, kind::unique);               // its block, a root by state
    EXPECT_EQ(path[1].holder, path[0].holder);
    EXPECT_EQ(*path[1].type, typeid(detail::CellBlock));
    kept.reset();
    detail::cell_allocator.release();                    // the block let go of: the tests after count live objects from zero
}

TEST(Referrers_Tests, AnotherThreadsStackIsARoot) {
    std::atomic<Leaf*> shared = nullptr;
    std::atomic<bool> done = false;
    std::thread other([&] {
        tracked_ptr leaf = make_tracked<Leaf>();         // held by this thread's stack only
        shared = leaf.get();
        while (!done) {
            std::this_thread::yield();
        }
    });
    while (!shared) {
        std::this_thread::yield();
    }
    {
        auto [guard, path] = collector::get_path_to_root(shared.load());
        ASSERT_EQ(path.size(), 1u);
        EXPECT_EQ(path[0].from, kind::stack);
    }
    done = true;
    other.join();
}

TEST(Referrers_Tests, AWeakPointerIsListedAndHoldsNothing) {
    tracked_ptr leaf = make_tracked<Leaf>();
    weak_ptr w = leaf;
    {
        auto [guard, rs] = collector::get_referrers(leaf.get());
        EXPECT_EQ(count(rs, kind::weak), 1u);
    }
    auto [guard, path] = collector::get_path_to_root(leaf.get());
    ASSERT_EQ(path.size(), 1u);
    EXPECT_EQ(path[0].from, kind::stack);                // the weak cell is no root: this stack is
}

TEST(Referrers_Tests, ExplainAndTheEdges) {
    unique_ptr<Node> node = make_tracked<Node>();
    node->leaf = make_tracked<Leaf>();
    std::ostringstream out;
    collector::explain(node->leaf.get(), out);
    EXPECT_NE(out.str().find("is held by"), std::string::npos);
    EXPECT_NE(out.str().find("Node"), std::string::npos);
    EXPECT_NE(out.str().find("a unique_ptr"), std::string::npos);
    tracked_ptr local = make_tracked<Leaf>();
    std::ostringstream mine;
    collector::explain(local.get(), mine);
    EXPECT_NE(mine.str().find("this thread's, above the call"), std::string::npos);
    int plain = 0;
    std::ostringstream none;
    collector::explain(&plain, none);                    // not managed
    EXPECT_NE(none.str().find("not a live managed object"), std::string::npos);
    auto [guard, rs] = collector::get_referrers(&plain);
    EXPECT_TRUE(rs.empty());
}

// What an object retains: the objects reachable from it and from nowhere
// else, itself included; what is shared with another root stays out, a
// weak pointer holds nothing, a container's buffer counts as a slot.
TEST(Referrers_Tests, RetainedSize) {
    unique_ptr<Node> head = make_tracked<Node>();
    head->next = make_tracked<Node>();
    head->next->next = make_tracked<Node>();
    head->next->leaf = make_tracked<Leaf>();
    tracked_ptr shared = head->next->next;                       // the third node has a second root: this frame
    auto [objects, bytes] = collector::get_retained(head.get());
    EXPECT_EQ(objects, 3u);                                      // head, the second node, its leaf; not the third
    EXPECT_EQ(bytes, 2 * sizeof(Node) + sizeof(Leaf));
    auto whole = collector::get_retained(head->next->next.get());
    EXPECT_EQ(whole.objects, 1u);
    EXPECT_EQ(whole.bytes, sizeof(Node));
    shared = nullptr;
    collector::clear_stack(SIZE_MAX);
    auto all = collector::get_retained(head.get());
    EXPECT_EQ(all.objects, 4u);                                  // the third node too now
    EXPECT_EQ(all.bytes, 3 * sizeof(Node) + sizeof(Leaf));
    weak_ptr w = head->next->leaf;                               // a weak pointer: its cell dies with the leaf, its target is not held by it
    auto with_weak = collector::get_retained(head.get());
    EXPECT_EQ(with_weak.objects, 4u);                            // the cell is reached from this frame's weak_ptr, not from head
    int plain = 0;
    auto none = collector::get_retained(&plain);
    EXPECT_EQ(none.objects, 0u);
    std::ostringstream out;
    collector::explain(head->next->leaf.get(), out);
    EXPECT_NE(out.str().find("keeps alive 1 object, " + std::to_string(sizeof(Leaf)) + " bytes"), std::string::npos);
}

TEST(Referrers_Tests, RetainedSizeOfAContainer) {
    unique_ptr<vector<tracked_ptr<Leaf>>> owned = make_tracked<vector<tracked_ptr<Leaf>>>();
    for (int i = 0; i < 10; ++i) {
        owned->push_back(make_tracked<Leaf>());
    }
    auto [objects, bytes] = collector::get_retained(owned.get());
    EXPECT_EQ(objects, 12u);                                     // the vector, its buffer, ten leaves
    EXPECT_GE(bytes, sizeof(vector<tracked_ptr<Leaf>>) + 10 * sizeof(Leaf) + sizeof(detail::ArrayBase) + owned->capacity() * sizeof(tracked_ptr<Leaf>));   // the buffer at least its capacity: the slot of its size class
}
