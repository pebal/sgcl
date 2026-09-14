//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <functional>
#include <memory>
#include <thread>
#include <vector>

// A gc::tracked_ptr in three places: on the stack and inside a managed object it is a
// tracked_ptr; in unmanaged memory (new, a std container, a global, a
// thread_local, a lambda on the heap) it owns a managed cell, a root that
// counts as a live object.
namespace {
    struct Node {
        int value = 7;
        gc::tracked_ptr<Node> next;
    };

    struct Counted {
        static inline std::atomic<int> alive = 0;
        Counted() { ++alive; }
        ~Counted() { --alive; }
    };

    struct Peer;
    struct Owner {
        gc::tracked_ptr<Peer> peer;
        static inline std::atomic<int> seen_dead = 0;
        static inline std::atomic<int> seen_alive = 0;
        ~Owner() {
            if (peer.if_alive()) {
                ++seen_alive;
            } else {
                ++seen_dead;
            }
        }
    };
    struct Peer {
        gc::tracked_ptr<Owner> owner;
    };

    // Inlined into the test: clear_stack clears the stack below its caller,
    // and a frame of its own between the test and the cleared region would
    // keep the words a sanitizer build leaves in its red zones. The
    // thread's block of cells is let go of first (detail::CellAllocator),
    // so that the count holds the blocks pinned by live pointers only, and
    // the next cell starts a block of its own: a test that takes fewer
    // than eight cells pins one block.
    SGCL_ALWAYS_INLINE size_t live_after_collect() {
        sgcl::detail::cell_allocator.release();
        collector::clear_stack(SIZE_MAX);
        collector::force_collect(true);
        return collector::get_live_object_count();
    }

    // The blocks that n cells taken one after another occupy (the slots
    // of a block are handed out in order: detail::CellAllocator)
    constexpr size_t blocks(size_t cells) {
        return (cells + sgcl::detail::CellBlock::Slots - 1) / sgcl::detail::CellBlock::Slots;
    }
}

TEST(GcTrackedPtr_Tests, OneWord) {
    EXPECT_EQ(sizeof(gc::tracked_ptr<Node>), sizeof(void*));
    EXPECT_EQ(sizeof(gc::tracked_ptr<void>), sizeof(void*));
}

TEST(GcTrackedPtr_Tests, NullInEveryPlace) {
    auto live0 = live_after_collect();
    off_frame([&] { // stack
        gc::tracked_ptr<Node> p;
        gc::tracked_ptr<Node> n(nullptr);
        EXPECT_EQ(p, nullptr);
        EXPECT_EQ(n, nullptr);
        EXPECT_FALSE(p);
    });
    off_frame([&] { // managed object
        auto p = make_tracked<gc::tracked_ptr<Node>>();
        EXPECT_EQ(*p, nullptr);
    });
    std::vector<gc::tracked_ptr<Node>> v;
    off_frame([&] { // unmanaged memory: a cell each, a null included
        auto p = std::make_unique<gc::tracked_ptr<Node>>();
        EXPECT_EQ(*p, nullptr);
        v.resize(100);
        EXPECT_EQ(v[99], nullptr);
    });
    EXPECT_EQ(live_after_collect(), live0 + blocks(101));        // the blocks of 101 cells: the one given back shares the first with v[0]
    v.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, OnTheStackItIsATrackedPtr) {
    auto live0 = live_after_collect();
    off_frame([&] {
        gc::tracked_ptr node = make_tracked<Node>();
        EXPECT_EQ(node->value, 7);
        gc::tracked_ptr<Node> copy = node;
        gc::tracked_ptr<Node> moved = std::move(node);
        EXPECT_EQ(copy, moved);
        EXPECT_EQ(node, moved);                                  // a move is a copy, as for sgcl::tracked_ptr
        EXPECT_EQ(collector::get_live_object_count(), live0 + 1);   // no cell
        tracked_ptr<Node> t = copy;
        EXPECT_EQ(t, copy);
        EXPECT_EQ(copy, t);
        gc::tracked_ptr<Node> back = t;
        EXPECT_EQ(back, t);
    });
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, InAManagedObjectItIsATrackedPtr) {
    auto live0 = live_after_collect();
    off_frame([&] {
        gc::tracked_ptr root = make_tracked<Node>();
        root->next = make_tracked<Node>();
        root->next->next = root;                                 // a cycle
        root->next->value = 8;
        EXPECT_EQ(root->next->next->next->value, 8);
        EXPECT_EQ(collector::get_live_object_count(), live0 + 2);   // no cell
    });
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, InAStdVectorItHoldsTheObject) {
    auto live0 = live_after_collect();
    std::vector<gc::tracked_ptr<Node>> unmanaged;
    off_frame([&] {
        gc::tracked_ptr node = make_tracked<Node>();
        node->next = make_tracked<Node>();
        unmanaged.push_back(node);                               // a cell
    });
    EXPECT_EQ(live_after_collect(), live0 + 3);                  // the node, its next, the cell
    off_frame([&] {
        EXPECT_EQ(unmanaged[0]->value, 7);
        ASSERT_NE(unmanaged[0]->next, nullptr);
        EXPECT_EQ(unmanaged[0]->next->value, 7);
    });
    unmanaged.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, AMoveIsACopyAndNoCellChangesHands) {
    auto live0 = live_after_collect();
    std::vector<gc::tracked_ptr<Node>> v;
    off_frame([&] {
        v.reserve(1);
        v.push_back(make_tracked<Node>());                           // the object and one cell
    });
    EXPECT_EQ(live_after_collect(), live0 + 2);
    off_frame([&] {
        for (int i = 0; i < 1000; ++i) {
            v.push_back(gc::tracked_ptr<Node>());                    // a cell each; the reallocations make and release cells
        }
        EXPECT_EQ(v[0]->value, 7);
    });
    auto live1 = live_after_collect();                           // the object; the cells of the final buffer, taken in a row by its reallocation, from wherever its block stood
    EXPECT_GE(live1, live0 + 1 + blocks(1001));
    EXPECT_LE(live1, live0 + 1 + blocks(1001) + 1);
    off_frame([&] {
        gc::tracked_ptr<Node> moved = std::move(v[0]);               // onto the stack: a copy, the source keeps its value
        EXPECT_EQ(moved->value, 7);
        EXPECT_EQ(v[0], moved);
        auto* p = new gc::tracked_ptr<Node>(std::move(v[0]));        // into unmanaged memory: a cell of its own, the source untouched
        EXPECT_EQ(v[0], moved);
        EXPECT_EQ((*p)->value, 7);
        v[1] = std::move(*p);                                        // a move assignment: a copy too, the cells stay where they are
        EXPECT_EQ(v[1]->value, 7);
        EXPECT_EQ(*p, v[1]);
        delete p;
    });
    EXPECT_EQ(live_after_collect(), live1);                      // the cells of the stack copy and of p are given back
    v.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, ACopyIntoUnmanagedMemoryIsOneCell) {
    auto live0 = live_after_collect();
    std::vector<gc::tracked_ptr<Node>> v;
    v.reserve(10);                                               // no reallocation: ten cells in a row
    off_frame([&] {
        gc::tracked_ptr node = make_tracked<Node>();
        for (int i = 0; i < 10; ++i) {
            v.push_back(node);
        }
    });
    EXPECT_EQ(live_after_collect(), live0 + 1 + blocks(10));     // the node and the blocks of ten cells
    v.resize(5);
    EXPECT_EQ(live_after_collect(), live0 + 1 + blocks(5));      // the blocks past the fifth cell given back whole
    v.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, ResetKeepsTheCellForTheNextValue) {
    auto live0 = live_after_collect();
    auto p = std::make_unique<gc::tracked_ptr<Node>>();
    EXPECT_EQ(live_after_collect(), live0 + 1);                  // the cell, from the constructor
    off_frame([&] {
        *p = make_tracked<Node>();
    });
    EXPECT_EQ(live_after_collect(), live0 + 2);
    p->reset();
    EXPECT_EQ(*p, nullptr);
    EXPECT_EQ(live_after_collect(), live0 + 1);                  // the object collected, the cell kept
    off_frame([&] {
        *p = make_tracked<Node>();
        *p = nullptr;
        EXPECT_EQ(*p, nullptr);
    });
    EXPECT_EQ(live_after_collect(), live0 + 1);
    p.reset();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, FromAUniquePtrInEveryPlace) {
    auto live0 = live_after_collect();
    std::vector<gc::tracked_ptr<Bar>> unmanaged;
    off_frame([&] {
        gc::tracked_ptr<Bar> stack = make_tracked<Baz>(3);
        EXPECT_EQ(stack->get_value(), 3);
        auto managed = make_tracked<gc::tracked_ptr<Bar>>(make_tracked<Baz>(4));
        EXPECT_EQ((*managed)->get_value(), 4);
        unmanaged.emplace_back(make_tracked<Baz>(5));
        EXPECT_EQ(unmanaged[0]->get_value(), 5);
        unmanaged[0] = make_tracked<Baz>(6);                     // assigned: the cell reused
        EXPECT_EQ(unmanaged[0]->get_value(), 6);
    });
    EXPECT_EQ(live_after_collect(), live0 + 2);                  // the Baz(6) and its cell
    unmanaged.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, ABaseAtAnOffsetInACell) {
    struct A { int a = 1; };
    struct B { int b = 2; };
    struct C : A, B { int c = 3; };
    auto live0 = live_after_collect();
    std::vector<gc::tracked_ptr<B>> v;
    off_frame([&] {
        gc::tracked_ptr c = make_tracked<C>();
        v.push_back(c);                                          // the B subobject, at an offset
        v.push_back(gc::tracked_ptr<B>(std::move(c)));
        gc::tracked_ptr<A> a = v[0].as<C>();
        EXPECT_EQ(a->a, 1);
    });
    EXPECT_EQ(live_after_collect(), live0 + 2);                  // the C and the block of the two cells
    off_frame([&] {
        EXPECT_EQ(v[0]->b, 2);
        EXPECT_EQ(v[1]->b, 2);
        EXPECT_TRUE(v[0].is<C>());
        EXPECT_EQ(v[0].as<C>()->c, 3);
        EXPECT_EQ(v[0], v[1]);
    });
    v.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, InAGlobalAndAThreadLocal) {
    // Static storage, constructed and destroyed by hand so that the cells
    // do not outlive the test.
    alignas(gc::tracked_ptr<Node>) static unsigned char global_storage[sizeof(gc::tracked_ptr<Node>)];
    alignas(gc::tracked_ptr<Node>) static thread_local unsigned char local_storage[sizeof(gc::tracked_ptr<Node>)];
    auto live0 = live_after_collect();
    auto& global = *new (global_storage) gc::tracked_ptr<Node>();
    auto& local = *new (local_storage) gc::tracked_ptr<Node>();
    EXPECT_EQ(live_after_collect(), live0 + 1);                  // the block of the two cells, from the constructors
    off_frame([&] {
        global = make_tracked<Node>();
        global->value = 1;
        local = make_tracked<Node>();
        local->value = 2;
    });
    EXPECT_EQ(live_after_collect(), live0 + 3);                  // two nodes, the block
    off_frame([&] {
        EXPECT_EQ(global->value, 1);
        EXPECT_EQ(local->value, 2);
    });
    std::thread([] {
        auto& other = *new (local_storage) gc::tracked_ptr<Node>();       // another thread's thread_local
        EXPECT_EQ(other, nullptr);
        other = make_tracked<Node>();
        std::destroy_at(&other);                                         // released with the thread
    }).join();
    global = nullptr;
    local = nullptr;
    EXPECT_EQ(live_after_collect(), live0 + 1);                  // the cells stay, so does their block; the other thread's block went with its cell
    std::destroy_at(&global);
    std::destroy_at(&local);
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, InALambdaOnTheHeap) {
    auto live0 = live_after_collect();
    std::function<int()> f;
    off_frame([&] {
        gc::tracked_ptr node = make_tracked<Node>();
        node->value = 9;
        char pad[64] = {};                                       // past the small buffer of std::function
        f = [node, pad] { return node->value + pad[0]; };        // the closure copied to the heap: a cell
    });
    EXPECT_EQ(live_after_collect(), live0 + 2);
    EXPECT_EQ(f(), 9);
    f = nullptr;
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, DestructorsRunOnceWhateverThePlace) {
    auto live0 = live_after_collect();
    Counted::alive = 0;
    std::vector<gc::tracked_ptr<Counted>> v;
    off_frame([&] {
        gc::tracked_ptr c = make_tracked<Counted>();
        v.push_back(c);
        v.push_back(std::move(c));
        auto m = make_tracked<gc::tracked_ptr<Counted>>(v[0]);
        EXPECT_EQ(Counted::alive, 1);
    });
    EXPECT_EQ(live_after_collect(), live0 + 2);                  // the Counted and the block of the two cells
    EXPECT_EQ(Counted::alive, 1);
    v.clear();
    EXPECT_EQ(live_after_collect(), live0);
    EXPECT_EQ(Counted::alive, 0);
}

TEST(GcTrackedPtr_Tests, IfAliveInADestructor) {
    Owner::seen_dead = 0;
    Owner::seen_alive = 0;
    auto live0 = live_after_collect();
    off_frame([&] {
        gc::tracked_ptr owner = make_tracked<Owner>();
        owner->peer = make_tracked<Peer>();
        owner->peer->owner = owner;                              // die together
    });
    EXPECT_EQ(live_after_collect(), live0);
    EXPECT_EQ(Owner::seen_dead + Owner::seen_alive, 1);
}

TEST(GcTrackedPtr_Tests, SharedBetweenThreadsFromUnmanagedMemory) {
    struct Hits {
        std::atomic<int> hits = {0};
    };
    auto live0 = live_after_collect();
    auto shared = std::make_shared<gc::tracked_ptr<Hits>>();
    off_frame([&] {
        *shared = make_tracked<Hits>();
    });
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([shared] {                          // the gc::tracked_ptr read, never written, by the threads
            for (int i = 0; i < 1000; ++i) {
                ++(*shared)->hits;
                gc::tracked_ptr<Hits> local = *shared;                    // a copy onto another thread's stack
                if (i % 100 == 0) {
                    std::this_thread::yield();
                }
            }
        });
    }
    collector::force_collect(true);
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ((*shared)->hits.load(), 4000);
    shared.reset();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, ComparisonsCastsAndHash) {
    off_frame([&] {
        gc::tracked_ptr<Bar> bar = make_tracked<Baz>(2);
        gc::tracked_ptr<Baz> baz = dynamic_pointer_cast<Baz>(bar);
        EXPECT_EQ(baz->value, 2);
        EXPECT_EQ(bar, baz);
        EXPECT_EQ(std::hash<gc::tracked_ptr<Bar>>{}(bar), std::hash<Bar*>{}(bar.get()));
        gc::tracked_ptr<const Bar> cbar = bar;
        EXPECT_EQ(const_pointer_cast<Bar>(cbar), bar);
        EXPECT_EQ(static_pointer_cast<Baz>(bar)->value, 2);
        gc::tracked_ptr<Bar> null;
        EXPECT_NE(bar, null);
        EXPECT_TRUE((null <=> nullptr) == 0);
        EXPECT_TRUE(bar.is<Baz>());
        EXPECT_EQ(bar.type(), typeid(Baz));
        gc::tracked_ptr<Bar> a = bar;
        gc::tracked_ptr<Bar> b;
        a.swap(b);
        EXPECT_EQ(a, nullptr);
        EXPECT_EQ(b, bar);
        gc::tracked_ptr<void> v = bar;
        EXPECT_EQ(v.get(), (void*)bar.get());
        EXPECT_TRUE(v.is<Baz>());
        EXPECT_EQ(v.as<Baz>()->value, 2);
    });
}

TEST(GcTrackedPtr_Tests, AtomicInEveryPlace) {
    struct Config { int version; };
    auto live0 = live_after_collect();
    auto current = std::make_unique<atomic<gc::tracked_ptr<Config>>>();    // unmanaged memory: the cell, allocated at once
    EXPECT_EQ(live_after_collect(), live0 + 1);
    off_frame([&] {
        current->store(make_tracked<Config>(1));
        gc::tracked_ptr<Config> seen = current->load();
        EXPECT_EQ(seen->version, 1);
        gc::tracked_ptr<Config> expected = seen;
        EXPECT_TRUE(current->compare_exchange_strong(expected, make_tracked<Config>(2)));
        EXPECT_EQ(current->load()->version, 2);
        EXPECT_FALSE(current->compare_exchange_strong(expected, nullptr));   // stale: expected is refreshed
        EXPECT_EQ(expected->version, 2);
    });
    EXPECT_EQ(live_after_collect(), live0 + 2);                   // the Config(2) and the cell
    off_frame([&] {
        atomic<gc::tracked_ptr<Config>> local(current->load());           // on the stack: a tracked_ptr, no cell
        EXPECT_EQ(local.load()->version, 2);
        auto managed = make_tracked<atomic<gc::tracked_ptr<Config>>>(local.load());   // in a managed object
        EXPECT_EQ(managed->load()->version, 2);
        EXPECT_EQ(collector::get_live_object_count(), live0 + 3);
    });
    *current = nullptr;
    EXPECT_EQ(live_after_collect(), live0 + 1);
    current.reset();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, AtomicSharedBetweenThreads) {
    struct Counter { std::atomic<int> hits = {0}; };
    auto live0 = live_after_collect();
    auto shared = std::make_shared<atomic<gc::tracked_ptr<Counter>>>();
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([shared] {
            for (int i = 0; i < 1000; ++i) {
                gc::tracked_ptr<Counter> c = shared->load();               // held on this thread's stack
                if (!c) {
                    gc::tracked_ptr<Counter> expected;
                    shared->compare_exchange_strong(expected, make_tracked<Counter>());
                    c = shared->load();
                }
                ++c->hits;
                if (i % 250 == 0) {
                    shared->store(make_tracked<Counter>());       // replaced under the readers
                }
            }
        });
    }
    collector::force_collect(true);
    for (auto& t : threads) {
        t.join();
    }
    shared.reset();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, AtomicRefOnAGcPtrAnywhere) {
    auto live0 = live_after_collect();
    std::vector<gc::tracked_ptr<Node>> v(2);                               // two cells
    off_frame([&] {
        atomic_ref<gc::tracked_ptr<Node>> a(v[0]);                         // a view of the cell's word
        EXPECT_EQ(a.load(), nullptr);
        a.store(make_tracked<Node>());
        EXPECT_EQ(v[0]->value, 7);
        gc::tracked_ptr<Node> expected = v[0];
        EXPECT_TRUE(a.compare_exchange_weak(expected, v[0]));
        gc::tracked_ptr<Node> local;
        atomic_ref<gc::tracked_ptr<Node>> b(local);                        // on the stack: the tracked_ptr itself
        b = v[0];
        EXPECT_EQ(local, v[0]);
        EXPECT_EQ(v[1], nullptr);
    });
    EXPECT_EQ(live_after_collect(), live0 + 2);                   // the Node and the block of the two cells
    v.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(GcTrackedPtr_Tests, WeakPtrOfBothKinds) {
    auto live0 = live_after_collect();
    std::vector<weak_ptr<Node, gc::tracked_ptr>> weak;                     // a std container: the gc kind
    off_frame([&] {
        gc::tracked_ptr node = make_tracked<Node>();
        weak_ptr w = node;                                        // deduced: weak_ptr<Node, gc::tracked_ptr>
        static_assert(std::is_same_v<decltype(w), weak_ptr<Node, gc::tracked_ptr>>);
        weak_ptr<Node> t = node;                                  // the tracked kind, from a gc::tracked_ptr
        weak_ptr<Node, gc::tracked_ptr> g = t;                             // and between the kinds
        weak_ptr<Node> back = g;
        EXPECT_EQ(t.lock(), node);
        EXPECT_EQ(g.lock(), node);
        EXPECT_EQ(back.lock(), node);
        static_assert(std::is_same_v<decltype(g.lock()), gc::tracked_ptr<Node>>);
        weak.push_back(w);
        weak.push_back(g);
        weak.push_back(node);
        EXPECT_EQ(weak[2].lock()->value, 7);
    });
    EXPECT_EQ(live_after_collect(), live0 + 4);                   // no node: three weak cells (w; t, shared by g and back; the third push) and the block of the three gc cells
    EXPECT_TRUE(weak[0].expired());
    EXPECT_EQ(weak[1].lock(), nullptr);
    weak.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

// The cells come from blocks of a cache line (detail/cell_block.h), one
// managed object per block, handed out by the thread's allocator in
// order; a block is freed by the cycle that finds every cell of it given
// back, once the allocator has let go of it (the last cell handed out, or
// the thread gone). A cell is given back by the destructor of its
// pointer on whatever thread that runs.
TEST(GcTrackedPtr_Tests, BlocksOfCells) {
    constexpr auto Slots = sgcl::detail::CellBlock::Slots;
    auto live0 = live_after_collect();
    std::vector<gc::tracked_ptr<Node>> v;
    v.reserve(Slots + 1);
    off_frame([&] {
        for (unsigned i = 0; i < Slots + 1; ++i) {
            v.push_back(make_tracked<Node>());
        }
    });
    EXPECT_EQ(live_after_collect(), live0 + Slots + 1 + 2);       // the nodes, a full block and the one of the last cell
    v.erase(v.begin(), v.begin() + Slots);                       // the full block's cells given back: freed, the last one's stays
    EXPECT_EQ(live_after_collect(), live0 + 1 + 1);
    v.clear();
    EXPECT_EQ(live_after_collect(), live0);
    // A block pinned by one cell: the cells taken and given back around
    // it cost nothing, and the block goes when the one cell does
    auto pin = std::make_unique<gc::tracked_ptr<Node>>(make_tracked<Node>());
    off_frame([&] {
        for (int i = 0; i < 1000; ++i) {
            gc::tracked_ptr<Node> temporary(*pin);
            EXPECT_EQ(temporary->value, 7);
        }
        std::vector<gc::tracked_ptr<Node>> churn(1000, *pin);
    });
    EXPECT_EQ(live_after_collect(), live0 + 1 + 1);              // the node and the block of the pin
    pin.reset();
    EXPECT_EQ(live_after_collect(), live0);
    // Cells taken on one thread and given back on another: the thread
    // lets go of its block when it ends, and the block is freed once
    // the other thread has given the cells back
    std::vector<gc::tracked_ptr<Node>> shared;
    shared.reserve(3);
    std::thread([&shared] {
        for (int i = 0; i < 3; ++i) {
            shared.push_back(make_tracked<Node>());
        }
    }).join();
    EXPECT_EQ(live_after_collect(), live0 + 3 + 1);              // the nodes and the other thread's block
    off_frame([&] {
        EXPECT_EQ(shared[2]->value, 7);
    });
    shared.clear();
    EXPECT_EQ(live_after_collect(), live0);
}
