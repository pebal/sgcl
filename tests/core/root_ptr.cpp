//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// root_ptr: a root that lives anywhere, a cell of a managed block under
// it, the object reachable while the root_ptr exists.
#include "tests/types.h"

#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        tracked_ptr<Node> next;
        inline static sgcl::atomic<int> alive = {0};
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

    // Inlined into the test: clear_stack clears the stack below its caller,
    // and a frame of its own between the test and the cleared region would
    // keep the words a sanitizer build leaves in its red zones. The
    // thread's block of cells is let go of first (detail::CellAllocator),
    // so that the count holds the blocks pinned by live root_ptrs only,
    // and the next cell starts a block of its own.
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

    // Roots on the unmanaged heap. Not globals: a root_ptr takes its cell
    // when constructed, and one made before main would pin a block for
    // the whole run, which the suite's counts do not expect.
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
        root_ptr<Node> r = make_tracked<Node>(3);
        registry->push_back(r);                                    // a copy: a cell of its own
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
        delete registry;                                           // the cells given back now, the objects unreferenced
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);                     // held still
    delete held;
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(RootPtr_Tests, CopiesHaveCellsOfTheirOwnMovesLeaveNull) {
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
        tracked_ptr<Node>& word = root.ptr();                      // the cell's word itself, a tracked_ptr in a managed block
        EXPECT_EQ(&word, &root.ptr());
        EXPECT_TRUE(detail::Heap::contains(&word));
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

TEST(RootPtr_Tests, InStdContainers) {
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
    delete by_key;
    delete hashed;
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

// The cells come from blocks of a cache line (detail/cell_block.h), one
// managed object per block, handed out by the thread's allocator in
// order; a block is freed by the cycle that finds every cell of it given
// back, once the allocator has let go of it (the last cell handed out, or
// the thread gone). A cell is given back by the destructor of its
// root_ptr on whatever thread that runs.
TEST(RootPtr_Tests, BlocksOfCells) {
    constexpr auto Slots = sgcl::detail::CellBlock::Slots;
    auto live0 = live_after_collect();
    std::vector<root_ptr<Node>> v;
    v.reserve(Slots + 1);
    off_frame([&] {
        for (unsigned i = 0; i < Slots + 1; ++i) {
            v.emplace_back(make_tracked<Node>((int)i));          // in place: one cell each (a temporary would take one of its own between)
        }
    });
    EXPECT_EQ(live_after_collect(), live0 + Slots + 1 + 2);       // the nodes, a full block and the one of the last cell
    v.erase(v.begin(), v.begin() + Slots);                       // the full block's cells given back: freed, the last one's stays
    EXPECT_EQ(live_after_collect(), live0 + 1 + 1);
    v.clear();
    EXPECT_EQ(live_after_collect(), live0);
    // A block pinned by one cell: the cells taken and given back around
    // it cost nothing, and the block goes when the one cell does
    auto pin = std::make_unique<root_ptr<Node>>(make_tracked<Node>(7));
    off_frame([&] {
        for (int i = 0; i < 1000; ++i) {
            root_ptr<Node> temporary(*pin);
            EXPECT_EQ(temporary->value, 7);
        }
        std::vector<root_ptr<Node>> churn(1000, *pin);
    });
    EXPECT_EQ(live_after_collect(), live0 + 1 + 1);              // the node and the block of the pin
    pin.reset();
    EXPECT_EQ(live_after_collect(), live0);
    // Cells taken on one thread and given back on another: the thread
    // lets go of its block when it ends, and the block is freed once
    // the other thread has given the cells back
    std::vector<root_ptr<Node>> shared;
    shared.reserve(3);
    std::thread([&shared] {
        for (int i = 0; i < 3; ++i) {
            shared.emplace_back(make_tracked<Node>(i));
        }
    }).join();
    EXPECT_EQ(live_after_collect(), live0 + 3 + 1);              // the nodes and the other thread's block
    off_frame([&] {
        EXPECT_EQ(shared[2]->value, 2);
    });
    shared.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(RootPtr_Tests, ACopyIsOneCellAMoveTakesNone) {
    auto live0 = live_after_collect();
    std::vector<root_ptr<Node>> v;
    v.reserve(10);                                               // no reallocation: ten cells in a row
    off_frame([&] {
        tracked_ptr node = make_tracked<Node>(7);
        for (int i = 0; i < 10; ++i) {
            v.emplace_back(node);
        }
    });
    EXPECT_EQ(live_after_collect(), live0 + 1 + blocks(10));     // the node and the blocks of ten cells
    v.resize(5);
    EXPECT_EQ(live_after_collect(), live0 + 1 + blocks(5));      // the blocks past the fifth cell given back whole
    off_frame([&] {
        root_ptr<Node> moved = std::move(v[0]);                  // the pointer moves, the source is null and keeps its cell
        EXPECT_EQ(moved->value, 7);
        EXPECT_FALSE(v[0]);
        v[0] = std::move(moved);                                 // back: no cell changes hands
        EXPECT_EQ(v[0]->value, 7);
        EXPECT_FALSE(moved);
        moved = v[0];                                            // a moved-from root_ptr is a root_ptr still
        EXPECT_EQ(moved, v[0]);
    });
    EXPECT_EQ(live_after_collect(), live0 + 1 + blocks(5));
    v.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(RootPtr_Tests, InAGlobalAThreadLocalAndALambdaOnTheHeap) {
    // Static storage, constructed and destroyed by hand so that the cells
    // do not outlive the test.
    alignas(root_ptr<Node>) static unsigned char global_storage[sizeof(root_ptr<Node>)];
    alignas(root_ptr<Node>) static thread_local unsigned char local_storage[sizeof(root_ptr<Node>)];
    auto live0 = live_after_collect();
    auto& global = *new (global_storage) root_ptr<Node>();
    auto& local = *new (local_storage) root_ptr<Node>();
    EXPECT_EQ(live_after_collect(), live0 + 1);                  // the block of the two cells, from the constructors
    off_frame([&] {
        global = make_tracked<Node>(1);
        local = make_tracked<Node>(2);
    });
    EXPECT_EQ(live_after_collect(), live0 + 3);                  // two nodes, the block
    off_frame([&] {
        EXPECT_EQ(global->value, 1);
        EXPECT_EQ(local->value, 2);
    });
    std::thread([] {
        auto& other = *new (local_storage) root_ptr<Node>();     // another thread's thread_local
        EXPECT_EQ(other, nullptr);
        other = make_tracked<Node>(3);
        std::destroy_at(&other);                                 // released with the thread
    }).join();
    global = nullptr;
    local = nullptr;
    EXPECT_EQ(live_after_collect(), live0 + 1);                  // the cells stay, so does their block; the other thread's block went with its cell
    std::destroy_at(&global);
    std::destroy_at(&local);
    EXPECT_EQ(live_after_collect(), live0);
    std::function<int()> f;
    off_frame([&] {
        root_ptr<Node> node = make_tracked<Node>(9);
        char pad[64] = {};                                       // past the small buffer of std::function
        f = [node, pad] { return node->value + pad[0]; };        // the closure copied to the heap: a cell
    });
    EXPECT_EQ(live_after_collect(), live0 + 2);
    EXPECT_EQ(f(), 9);
    f = nullptr;
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(RootPtr_Tests, SharedBetweenThreadsAndUnderAnAtomicRef) {
    struct Hits {
        sgcl::atomic<int> hits = {0};
    };
    auto live0 = live_after_collect();
    auto shared = std::make_shared<root_ptr<Hits>>();
    off_frame([&] {
        *shared = make_tracked<Hits>();
    });
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([shared] {                          // the root_ptr read, never written, by the threads
            for (int i = 0; i < 1000; ++i) {
                ++(*shared)->hits;
                tracked_ptr<Hits> local = *shared;               // a copy onto another thread's stack
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
    threads.clear();
    // A root replaced under its readers: an atomic_ref over the cell's
    // word, the atomic of a root that lives anywhere
    struct Counter { sgcl::atomic<int> hits = {0}; };
    auto root = std::make_shared<root_ptr<Counter>>();
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([root] {
            atomic_ref a(*root);                                 // deduced: atomic_ref<tracked_ptr<Counter>>
            for (int i = 0; i < 1000; ++i) {
                tracked_ptr<Counter> c = a.load();               // held on this thread's stack
                if (!c) {
                    tracked_ptr<Counter> expected;
                    a.compare_exchange_strong(expected, make_tracked<Counter>());
                    c = a.load();
                }
                ++c->hits;
                if (i % 250 == 0) {
                    a.store(make_tracked<Counter>());            // replaced under the readers
                }
            }
        });
    }
    collector::force_collect(true);
    for (auto& t : threads) {
        t.join();
    }
    root.reset();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(RootPtr_Tests, InAManagedObjectAndAsAWeakPtrsSource) {
    auto live0 = live_after_collect();
    struct Holder {
        root_ptr<Node> root;                                     // pointless there, but allowed
    };
    off_frame([&] {
        tracked_ptr h = make_tracked<Holder>();
        h->root = make_tracked<Node>(5);
        EXPECT_EQ(h->root->value, 5);
        weak_ptr<Node> w = h->root;                              // from the root itself
        EXPECT_EQ(w.lock(), h->root);
    });
    EXPECT_EQ(live_after_collect(), live0);
}

// A weak_ptr and a tracked_ptr of a base straight from a root_ptr: the
// root's conversion to its tracked_ptr is one a template constructor
// cannot deduce through, so these have constructors of their own
TEST(RootPtr_Tests, AWeakPtrAndABasePointerFromARoot) {
    auto live0 = live_after_collect();
    off_frame([&] {
        root_ptr<Derived> root = make_tracked<Derived>(7);
        weak_ptr w = root;                                       // deduced: weak_ptr<Derived>
        static_assert(std::is_same_v<decltype(w), weak_ptr<Derived>>);
        weak_ptr<Node> base_weak = root;
        tracked_ptr<Node> base = root;
        tracked_ptr<const Derived> constant = root;
        tracked_ptr deduced = root;
        static_assert(std::is_same_v<decltype(deduced), tracked_ptr<Derived>>);
        EXPECT_EQ(w.lock(), root.ptr());
        EXPECT_EQ(base_weak.lock().get(), root.get());
        EXPECT_EQ(base.get(), root.get());
        EXPECT_EQ(constant.get(), root.get());
        base = nullptr;
        base = root;                                             // an assignment through the same constructor
        EXPECT_EQ(base->value, 7);
    });
    EXPECT_EQ(live_after_collect(), live0);
}
