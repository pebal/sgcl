//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// any: std::any's interface, a tracked pointer in a word of its own, an
// object with pointers inside in a managed object of its own.
#include "tests/types.h"

#include <cstring>
#include <string>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    struct Pair {
        tracked_ptr<Node> node;
        int count;
    };

    struct Holder {
        any value;
        tracked_ptr<Holder> next;
    };

    // Inlined into the test's frame: a frame of its own would sit where
    // the dead frames were and keep their words (root_ptr.cpp: live_after_collect)
    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Any_Tests, TheInterfaceOfStdAny) {
    static_assert(sizeof(any) == sizeof(std::any));
    any a;
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(a.type() == typeid(void));
    a = 5;
    EXPECT_TRUE(a.type() == typeid(int));
    EXPECT_EQ(any_cast<int>(a), 5);
    EXPECT_EQ(*any_cast<int>(&a), 5);
    EXPECT_EQ(any_cast<double>(&a), nullptr);
    EXPECT_THROW(any_cast<double>(a), bad_any_cast);
    any_cast<int&>(a) = 6;
    EXPECT_EQ(any_cast<int>(a), 6);
    a = std::string("text");
    EXPECT_EQ(any_cast<std::string>(a), "text");
    any b(std::in_place_type<std::string>, {'a', 'b'});
    EXPECT_EQ(any_cast<const std::string&>(b), "ab");
    any c = sgcl::make_any<Pair>(make_tracked<Node>(1), 7);
    EXPECT_EQ(any_cast<Pair&>(c).count, 7);
    EXPECT_EQ(any_cast<Pair&>(c).node->value, 1);
    any d = c;                              // a copy: another managed object
    EXPECT_NE(any_cast<Pair>(&c), any_cast<Pair>(&d));
    EXPECT_EQ(any_cast<Pair>(&d)->node, any_cast<Pair>(&c)->node);
    any e = std::move(d);
    EXPECT_FALSE(d.has_value());
    EXPECT_EQ(any_cast<Pair&>(e).count, 7);
    e.swap(a);
    EXPECT_EQ(any_cast<std::string>(e), "text");
    EXPECT_EQ(any_cast<Pair&>(a).count, 7);
    EXPECT_EQ(a.emplace<int>(3), 3);
    EXPECT_EQ(any_cast<int>(a), 3);
    a.reset();
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ(any_cast<int>(&a), nullptr);
    EXPECT_EQ(any_cast<std::string>(std::move(e)), "text");
}

TEST(Any_Tests, APointerInTheWordAValueInAnObject) {
    settle();
    const int before = Node::alive.load();
    any pointer, weak, pair, number;
    off_frame([&] {
        pointer = tracked_ptr(make_tracked<Node>(1));
        weak = weak_ptr<Node>(any_cast<tracked_ptr<Node>>(pointer));
        pair = Pair{make_tracked<Node>(2), 0x10000};
        number = 0x20000;
    });
    // the pointer and the weak pointer sit inside the any, the pair does not
    EXPECT_GE((const char*)any_cast<tracked_ptr<Node>>(&pointer), (const char*)&pointer);
    EXPECT_LT((const char*)any_cast<tracked_ptr<Node>>(&pointer), (const char*)&pointer + sizeof(any));
    EXPECT_GE((const char*)any_cast<int>(&number), (const char*)&number);
    EXPECT_TRUE(detail::Heap::contains(any_cast<Pair>(&pair)));
    settle();
    EXPECT_EQ(Node::alive.load(), before + 2);
    off_frame([&] {                         // the reads in a frame of their own: what they spill is cleared
        EXPECT_EQ(any_cast<tracked_ptr<Node>>(pointer)->value, 1);
        EXPECT_EQ(any_cast<weak_ptr<Node>>(weak).lock()->value, 1);
        EXPECT_EQ(any_cast<Pair&>(pair).node->value, 2);
        pair.reset();                       // the object destroyed now: the Node it held goes with the next cycle
        pointer = 3;
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);
    EXPECT_TRUE(any_cast<weak_ptr<Node>>(weak).expired());
}

TEST(Any_Tests, AWholeContainerGoesIntoAManagedObject) {
    settle();
    const int before = Node::alive.load();
    any a;
    off_frame([&] {
        vector<tracked_ptr<Node>> nodes;
        for (int i = 0; i < 10; ++i) {
            nodes.push_back(make_tracked<Node>(i));
        }
        a = std::move(nodes);                // the vector lives in a managed object: where an sgcl container may
        sorted_map<int, tracked_ptr<Node>> by_key;
        by_key[1] = make_tracked<Node>(100);
        a.emplace<any>(std::move(by_key));   // an any in an any, the map inside
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    any b;
    off_frame([&] {
        auto& inner = any_cast<any&>(a);
        EXPECT_EQ((any_cast<sorted_map<int, tracked_ptr<Node>>&>(inner)[1]->value), 100);
        a = vector<tracked_ptr<Node>>{make_tracked<Node>(1), make_tracked<Node>(2)};
        b = a;                               // a copy of the vector, in an object of its own
        EXPECT_EQ(any_cast<vector<tracked_ptr<Node>>&>(b).size(), 2u);
        EXPECT_EQ(any_cast<vector<tracked_ptr<Node>>&>(b)[1], any_cast<vector<tracked_ptr<Node>>&>(a)[1]);
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 2);
    off_frame([&] {
        a.reset();
        b.reset();
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

namespace {
    struct Counted {
        Counted() { ++alive; }
        Counted(const Counted&) { ++alive; }
        ~Counted() { --alive; }
        tracked_ptr<Node> node;             // may hold a pointer: lives in a managed object
        inline static sgcl::atomic<int> alive = {0};
    };
}

TEST(Any_Tests, TheValueInAnObjectIsDestroyedAtOnce) {
    any a = Counted();
    EXPECT_EQ(Counted::alive.load(), 1);
    any b = a;
    EXPECT_EQ(Counted::alive.load(), 2);
    b.reset();
    EXPECT_EQ(Counted::alive.load(), 1);    // not waiting for a cycle
    a = 1;
    EXPECT_EQ(Counted::alive.load(), 0);
    {
        any c = Counted();
        EXPECT_EQ(Counted::alive.load(), 1);
    }
    EXPECT_EQ(Counted::alive.load(), 0);
}

TEST(Any_Tests, ThePointerIsFollowedNextToInts) {
    settle();
    const int before = Node::alive.load();
    constexpr int Count = 2000;
    tracked_ptr<Holder> list;
    off_frame([&] {
        for (int i = 0; i < Count; ++i) {
            tracked_ptr holder = make_tracked<Holder>();
            switch (i % 3) {
            case 0: holder->value = tracked_ptr(make_tracked<Node>(i)); break;
            case 1: holder->value = 0x30000 + i; break;
            default: holder->value = Pair{make_tracked<Node>(i), 0x40000 + i}; break;
            }
            holder->next = list;
            list = holder;
        }
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + Count - Count / 3 - 1);   // 2000 = 667 + 667 + 666
    off_frame([&] {                         // the walk leaves pointers into the holders in its frame
        for (auto h = list; h; h = h->next) {
            if (auto p = any_cast<tracked_ptr<Node>>(&h->value)) {
                EXPECT_EQ((*p)->value % 3, 0);
            } else if (auto pair = any_cast<Pair>(&h->value)) {
                EXPECT_EQ(pair->node->value % 3, 2);
            }
        }
    });
    list = nullptr;
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(Any_Tests, AValuePointingBackAtItsOwnerIsACycle) {
    settle();
    const int before = Node::alive.load();
    struct Owner {
        explicit Owner(int v) : node(make_tracked<Node>(v)) {}
        tracked_ptr<Node> node;
        any payload;
    };
    off_frame([&] {
        tracked_ptr owner = make_tracked<Owner>(1);
        owner->payload = owner;                              // a pointer in the word: the cycle owner -> any -> owner
        tracked_ptr other = make_tracked<Owner>(2);
        other->payload = Pair{other->node, 0x10000};         // a value in a node: the cycle other -> node -> node object
        struct Back { tracked_ptr<Owner> owner; int count; };
        other->payload = Back{other, 0x20000};
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);                   // both cycles collected
}


namespace {
    // The storage's word: the second word of the any, after the manager
    // (value_storage.h); null when nothing lies in it and no node is held
    uintptr_t word_of(const any& a) noexcept {
        uintptr_t w;
        std::memcpy(&w, (const char*)&a + sizeof(void*), sizeof(w));
        return w;
    }

    // A value whose constructor throws, going to a node (it may hold a pointer)
    struct ThrowsInside {
        ThrowsInside() { throw 1; }
        ThrowsInside(const ThrowsInside&) = default;
        tracked_ptr<Node> node;
    };
}

TEST(Any_Tests, AConstructorThatThrowsLeavesTheAnyEmptyAndTheWordNull) {
    any t;
    EXPECT_THROW(t.emplace<ThrowsInside>(), int);
    EXPECT_FALSE(t.has_value());
    EXPECT_TRUE(t.type() == typeid(void));
    EXPECT_EQ(word_of(t), 0u);                          // no node placed in the word before the value was made
    EXPECT_THROW(t.emplace<ThrowsInside>(), int);       // again: nothing left over to construct upon
    EXPECT_EQ(word_of(t), 0u);
    t = 5;
    EXPECT_EQ(any_cast<int>(t), 5);
    any u = 3;
    EXPECT_THROW(u.emplace<ThrowsInside>(), int);       // emplace resets first, as std's: empty after the throw
    EXPECT_FALSE(u.has_value());
    EXPECT_EQ(word_of(u), 0u);
    EXPECT_THROW(any(std::in_place_type<ThrowsInside>), int);
}

// An any inside a managed object that dies in a sweep, holding a value in a
// node whose elements have destructors of their own (a vector of Pairs): the
// node is garbage of the same sweep and destroys the value itself, once,
// whichever of the two the sweep reaches first (value_storage.h: NodeOps)
TEST(Any_Tests, AValueInANodeDiesOnceInASweep) {
    settle();
    const int before = Node::alive.load();
    for (int round = 0; round < 50; ++round) {
        off_frame([&] {
            tracked_ptr node = make_tracked<Node>(round);
            tracked_ptr holder = make_tracked<Holder>();
            vector<Pair> pairs;
            pairs.push_back(Pair{node, 1});
            pairs.push_back(Pair{node, 2});
            holder->value = std::move(pairs);
            holder->next = make_tracked<Holder>();
            holder->next->value = vector<Pair>{Pair{node, 3}};
            holder->next->next = holder;     // a cycle of holders: both die in one sweep
        });
    }
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}
