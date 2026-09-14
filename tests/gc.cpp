//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "types.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// The gc namespace: the containers, observers and coroutines whose root
// word is a gc::tracked_ptr, so that they live in unmanaged memory (a std
// container, a global, a lambda on the heap) where the sgcl ones may not.
// Each test puts one into a std::vector or a unique_ptr, uses it from
// there, and checks that the objects it holds survive a full cycle and go
// when it goes.
namespace {
    struct Item {
        int value = 7;
        gc::tracked_ptr<Item> next;
    };

    SGCL_ALWAYS_INLINE size_t live_after_collect() {
        collector::clear_stack(SIZE_MAX);
        collector::force_collect(true);
        return collector::get_live_object_count();
    }
}

TEST(Gc_Tests, TheSameTypesWithAnotherWord) {
    static_assert(std::is_same_v<gc::vector<int>, sgcl::vector<int, gc::tracked_ptr>>);
    static_assert(std::is_same_v<gc::map<int, int>, sgcl::map<int, int, std::less<int>, gc::tracked_ptr>>);
    static_assert(std::is_same_v<gc::weak_ptr<Item>, sgcl::weak_ptr<Item, gc::tracked_ptr>>);
    static_assert(std::is_same_v<gc::task<int>, sgcl::task<int, gc::tracked_ptr>>);
    static_assert(std::is_same_v<sgcl::vector<int>, sgcl::vector<int, tracked_ptr>>);
    EXPECT_EQ(sizeof(gc::vector<int>), sizeof(sgcl::vector<int>));
    EXPECT_EQ(sizeof(gc::map<int, int>), sizeof(sgcl::map<int, int>));
}

TEST(Gc_Tests, MakeIsMakeTracked) {
    off_frame([&] {
        gc::tracked_ptr item = gc::make_tracked<Item>();
        static_assert(std::is_same_v<decltype(item), gc::tracked_ptr<Item>>);
        EXPECT_EQ(item->value, 7);
        gc::unique_ptr<Item> owned = gc::make_tracked<Item>();
        EXPECT_EQ(owned->value, 7);
    });
}

TEST(Gc_Tests, VectorInAStdVector) {
    auto live0 = live_after_collect();
    std::vector<gc::vector<gc::tracked_ptr<Item>>> outer;                // a std container of gc vectors
    off_frame([&] {
        outer.emplace_back();
        for (int i = 0; i < 100; ++i) {
            outer[0].push_back(gc::make_tracked<Item>());                // the elements: gc::tracked_ptrs in a managed buffer
            outer[0].back()->value = i;
        }
        outer.push_back(outer[0]);                               // a copy: another buffer, the same items
        outer.emplace_back(std::move(outer[0]));                 // a move: the buffer taken over, the vector's cell stays
        EXPECT_EQ(outer[0].size(), 0u);
    });
    EXPECT_EQ(live_after_collect(), live0 + 100 + 2 + 3);        // the items, two buffers, the three vectors' cells (the moved-from one keeps its cell)
    off_frame([&] {
        EXPECT_EQ(outer[1].size(), 100u);
        EXPECT_EQ(outer[2][42]->value, 42);
        EXPECT_EQ(outer[1][42], outer[2][42]);
        std::sort(outer[2].begin(), outer[2].end(), [](auto& a, auto& b) { return a->value > b->value; });
        EXPECT_EQ(outer[2][0]->value, 99);
    });
    outer.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(Gc_Tests, VectorOfTrackedPtrsLivesAnywhereToo) {
    auto live0 = live_after_collect();
    auto v = std::make_unique<gc::vector<tracked_ptr<Item>>>();  // the elements are tracked_ptrs: in a managed buffer, allowed
    off_frame([&] {
        v->push_back(gc::make_tracked<Item>());
        v->push_back(make_tracked<Item>());
        gc::tracked_ptr<Item> p = (*v)[0];
        tracked_ptr<Item> t = (*v)[1];
        EXPECT_EQ(p->value + t->value, 14);
    });
    EXPECT_EQ(live_after_collect(), live0 + 4);                  // two items, the buffer, the cell
    v.reset();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(Gc_Tests, ArrayDequeListAndForwardListInUnmanagedMemory) {
    auto live0 = live_after_collect();
    struct Holder {
        gc::array<gc::tracked_ptr<Item>> array;
        gc::deque<gc::tracked_ptr<Item>> deque;
        gc::list<gc::tracked_ptr<Item>> list;
        gc::forward_list<gc::tracked_ptr<Item>> forward_list;
    };
    auto h = new Holder{gc::array<gc::tracked_ptr<Item>>(3), {}, {}, {}};
    off_frame([&] {
        for (int i = 0; i < 3; ++i) {
            h->array[i] = gc::make_tracked<Item>();
            h->deque.push_front(gc::make_tracked<Item>());
            h->list.push_back(gc::make_tracked<Item>());
            h->forward_list.push_front(gc::make_tracked<Item>());
            h->array[i]->value = h->deque.front()->value = h->list.back()->value = h->forward_list.front()->value = i;
        }
    });
    size_t live1 = live_after_collect();
    EXPECT_GE(live1, live0 + 12);                                // the items at least; plus buffers, nodes, sentinels, cells
    off_frame([&] {
        EXPECT_EQ(h->array[2]->value, 2);
        EXPECT_EQ(h->deque.front()->value, 2);
        EXPECT_EQ(h->deque.back()->value, 0);
        EXPECT_EQ(h->list.back()->value, 2);
        EXPECT_EQ(h->forward_list.front()->value, 2);
        h->list.sort([](auto& a, auto& b) { return a->value > b->value; });
        EXPECT_EQ(h->list.front()->value, 2);
        gc::list<gc::tracked_ptr<Item>> moved = std::move(h->list);      // a move between two unmanaged places
        EXPECT_EQ(moved.size(), 3u);
        EXPECT_TRUE(h->list.empty());
        h->list = std::move(moved);
    });
    delete h;
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(Gc_Tests, MapsAndSetsAsGlobals) {
    // Static storage, constructed and destroyed by hand so that nothing
    // outlives the test.
    alignas(gc::map<int, gc::tracked_ptr<Item>>) static unsigned char map_storage[sizeof(gc::map<int, gc::tracked_ptr<Item>>)];
    alignas(gc::unordered_map<std::string, gc::tracked_ptr<Item>>) static unsigned char umap_storage[sizeof(gc::unordered_map<std::string, gc::tracked_ptr<Item>>)];
    alignas(gc::set<int>) static unsigned char set_storage[sizeof(gc::set<int>)];
    auto live0 = live_after_collect();
    auto& map = *new (map_storage) gc::map<int, gc::tracked_ptr<Item>>();
    auto& umap = *new (umap_storage) gc::unordered_map<std::string, gc::tracked_ptr<Item>>();
    auto& set = *new (set_storage) gc::set<int>();
    off_frame([&] {
        for (int i = 0; i < 50; ++i) {
            map[i] = gc::make_tracked<Item>();
            map[i]->value = i;
            umap[std::to_string(i)] = map[i];
            set.insert(i);
        }
        map.erase(7);
        umap.erase("7");
    });
    size_t live1 = live_after_collect();
    EXPECT_GE(live1, live0 + 49);
    off_frame([&] {
        EXPECT_EQ(map.size(), 49u);
        EXPECT_EQ(map.at(42)->value, 42);
        EXPECT_EQ(umap.at("42"), map.at(42));
        EXPECT_EQ(map.find(7), map.end());
        EXPECT_EQ(set.count(7), 1u);
        gc::multimap<int, int> mm;                               // on the stack: a tracked_ptr root, no cell
        mm.emplace(1, 1);
        mm.emplace(1, 2);
        EXPECT_EQ(mm.count(1), 2u);
        gc::unordered_multiset<int> ms = {1, 1, 2};
        EXPECT_EQ(ms.count(1), 2u);
    });
    std::destroy_at(&set);
    std::destroy_at(&umap);
    std::destroy_at(&map);
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(Gc_Tests, AdaptersOverGcContainers) {
    auto live0 = live_after_collect();
    auto s = std::make_unique<gc::stack<gc::tracked_ptr<Item>>>();
    auto q = std::make_unique<gc::queue<gc::tracked_ptr<Item>>>();
    auto pq = std::make_unique<gc::priority_queue<int>>();
    off_frame([&] {
        s->push(gc::make_tracked<Item>());
        q->push(gc::make_tracked<Item>());
        pq->push(3);
        pq->push(9);
        EXPECT_EQ(s->top()->value, 7);
        EXPECT_EQ(q->front()->value, 7);
        EXPECT_EQ(pq->top(), 9);
    });
    EXPECT_GE(live_after_collect(), live0 + 2);
    s.reset();
    q.reset();
    pq.reset();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(Gc_Tests, ExpiryQueueInUnmanagedMemory) {
    auto live0 = live_after_collect();
    auto gone = std::make_unique<gc::expiry_queue<Item>>();
    std::vector<int> released;
    off_frame([&] {
        gc::tracked_ptr item = gc::make_tracked<Item>();
        item->value = 3;
        gc::weak_ptr<Item> w = gone->watch(item, [&](gc::tracked_ptr<Item> t) { released.push_back(t->value); });
        static_assert(std::is_same_v<decltype(w), gc::weak_ptr<Item>>);
        EXPECT_FALSE(w.expired());
    });
    collector::clear_stack(SIZE_MAX);
    collector::force_collect(true);
    EXPECT_EQ(gone->drain(), 1u);
    ASSERT_EQ(released.size(), 1u);
    EXPECT_EQ(released[0], 3);
    gone.reset();
    EXPECT_EQ(live_after_collect(), live0);
}

namespace {
    gc::task<int> hold(int v) {
        gc::tracked_ptr<Item> local = gc::make_tracked<Item>();
        local->value = v;
        co_await std::suspend_always{};
        co_return local->value;
    }

    gc::generator<gc::tracked_ptr<Item>> items(int count) {
        for (int i = 0; i < count; ++i) {
            gc::tracked_ptr<Item> n = gc::make_tracked<Item>();
            n->value = i;
            co_yield n;
        }
    }
}

TEST(Gc_Tests, TasksAndGeneratorsInAStdVector) {
    auto live0 = live_after_collect();
    std::vector<gc::task<int>> tasks;                            // a scheduler's queue: tasks in a std container
    off_frame([&] {
        for (int i = 0; i < 4; ++i) {
            tasks.push_back(hold(i));
            tasks.back().resume();                               // suspended, holding its Item
        }
    });
    EXPECT_GE(live_after_collect(), live0 + 8);                  // four frames, four items
    off_frame([&] {
        int sum = 0;
        for (auto& t : tasks) {
            t.resume();
            sum += t.result();
        }
        EXPECT_EQ(sum, 6);
        auto g = std::make_unique<gc::generator<gc::tracked_ptr<Item>>>(items(3));
        int seen = 0;
        for (auto& item : *g) {
            seen += item->value;
        }
        EXPECT_EQ(seen, 3);
    });
    tasks.clear();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(Gc_Tests, WeakPtrAndAtomicOfTheFamily) {
    auto live0 = live_after_collect();
    std::vector<gc::weak_ptr<Item>> weak;
    auto current = std::make_unique<gc::atomic<gc::tracked_ptr<Item>>>();
    off_frame([&] {
        gc::tracked_ptr item = gc::make_tracked<Item>();
        weak.push_back(item);
        current->store(item);
        EXPECT_EQ(weak[0].lock(), current->load());
    });
    EXPECT_GE(live_after_collect(), live0 + 1);
    EXPECT_FALSE(weak[0].expired());                             // held by the atomic
    *current = nullptr;
    live_after_collect();
    EXPECT_TRUE(weak[0].expired());
    weak.clear();
    current.reset();
    EXPECT_EQ(live_after_collect(), live0);
}

TEST(Gc_Tests, DeductionThroughTheAliases) {
    off_frame([&] {
        gc::vector v = {1, 2, 3};                                // from a constructor
        static_assert(std::is_same_v<decltype(v), gc::vector<int>>);
        gc::vector w(v.begin(), v.end());                         // from the iterator-pair guide, which carries Ptr
        static_assert(std::is_same_v<decltype(w), gc::vector<int>>);
        gc::array a(v.begin(), v.end());
        static_assert(std::is_same_v<decltype(a), gc::array<int>>);
        gc::map m = {std::pair{1, 2}};
        static_assert(std::is_same_v<decltype(m), gc::map<int, int>>);
        gc::set s = {1, 2};
        static_assert(std::is_same_v<decltype(s), gc::set<int>>);
        gc::list l = {1, 2};
        static_assert(std::is_same_v<decltype(l), gc::list<int>>);
        sgcl::vector sv(v.begin(), v.end());                      // the default kind, as before
        static_assert(std::is_same_v<decltype(sv), sgcl::vector<int>>);
        sgcl::map sm(m.begin(), m.end());
        static_assert(std::is_same_v<decltype(sm), sgcl::map<int, int>>);
        gc::tracked_ptr p = gc::make_tracked<Item>();
        gc::weak_ptr wp = p;
        static_assert(std::is_same_v<decltype(wp), gc::weak_ptr<Item>>);
        EXPECT_EQ(w.size() + a.size() + m.size() + s.size() + l.size() + sv.size() + sm.size(), 15u);
        EXPECT_EQ(wp.lock(), p);
    });
}

// An element that names a tracked_type is stored as that type, one word
// in the tracked mode: no cell is made for a gc::tracked_ptr kept in a
// container, whichever namespace the container is from, and the interface
// still hands it out as a gc::tracked_ptr.
TEST(Gc_Tests, PointersStoredAsTheirTrackedWord) {
    static_assert(std::is_same_v<gc::vector<gc::tracked_ptr<Item>>::value_type, gc::tracked_ptr<Item>>);
    static_assert(std::is_same_v<gc::vector<gc::tracked_ptr<Item>>::reference, gc::tracked_ptr<Item>&>);
    static_assert(std::is_same_v<gc::map<int, gc::tracked_ptr<Item>>::mapped_type, gc::tracked_ptr<Item>>);
    static_assert(std::is_same_v<gc::unordered_set<gc::tracked_ptr<Item>>::key_type, gc::tracked_ptr<Item>>);
    static_assert(std::is_same_v<sgcl::detail::managed_t<gc::tracked_ptr<Item>>, tracked_ptr<Item>>);
    static_assert(std::is_same_v<sgcl::detail::managed_t<tracked_ptr<Item>>, tracked_ptr<Item>>);
    static_assert(std::is_same_v<sgcl::detail::managed_t<Item>, Item>);
    auto live0 = live_after_collect();
    auto v = std::make_unique<gc::vector<gc::tracked_ptr<Item>>>();
    sgcl::vector<gc::tracked_ptr<Item>> sv;                      // sgcl's container, on the stack as it must be; its elements mapped the same
    auto d = std::make_unique<gc::deque<gc::tracked_ptr<Item>>>();
    auto l = std::make_unique<gc::list<gc::tracked_ptr<Item>>>();
    auto f = std::make_unique<gc::forward_list<gc::tracked_ptr<Item>>>();
    auto a = std::make_unique<gc::array<gc::tracked_ptr<Item>>>(2);
    auto m = std::make_unique<gc::map<int, gc::tracked_ptr<Item>>>();
    auto s = std::make_unique<gc::set<gc::tracked_ptr<Item>>>();
    auto um = std::make_unique<gc::unordered_map<gc::tracked_ptr<Item>, gc::tracked_ptr<Item>>>();
    auto us = std::make_unique<gc::unordered_set<gc::tracked_ptr<Item>>>();
    off_frame([&] {
        gc::tracked_ptr<Item> item = gc::make_tracked<Item>();   // one cell: on this frame, unmanaged
        v->push_back(item);
        v->emplace_back(gc::make_tracked<Item>());
        sv.push_back(item);
        d->push_back(item);
        d->push_front(item);
        l->push_back(item);
        f->push_front(item);
        (*a)[0] = item;
        (*a)[1] = gc::make_tracked<Item>(item->value + 1);
        (*m)[1] = item;
        m->emplace(2, gc::make_tracked<Item>());
        s->insert(item);
        um->emplace(item, item);
        us->insert(item);
        gc::tracked_ptr<Item> back = v->back();
        gc::tracked_ptr<Item>& front = v->front();
        auto& [key, mapped] = *m->begin();
        static_assert(std::is_same_v<decltype(back), gc::tracked_ptr<Item>>);
        static_assert(std::is_same_v<decltype(*v->begin()), gc::tracked_ptr<Item>&>);
        static_assert(std::is_same_v<decltype(*l->begin()), gc::tracked_ptr<Item>&>);
        static_assert(std::is_same_v<decltype(*d->begin()), gc::tracked_ptr<Item>&>);
        static_assert(std::is_same_v<decltype((*a)[0]), gc::tracked_ptr<Item>&>);
        static_assert(std::is_same_v<decltype(*s->begin()), const gc::tracked_ptr<Item>&>);
        static_assert(std::is_same_v<decltype(mapped), gc::tracked_ptr<Item>>);
        static_assert(std::is_same_v<decltype(us->extract(us->begin()).value()), gc::tracked_ptr<Item>&>);
        EXPECT_EQ(front, item);
        EXPECT_EQ(back->value, 7);
        EXPECT_EQ((*a)[1]->value, 8);
        EXPECT_EQ(mapped, item);
        EXPECT_EQ(s->count(item), 1u);
        EXPECT_EQ(um->at(item), item);
        int sum = 0;
        for (auto& p : *v) {
            sum += p->value;
        }
        for (auto& [k, p] : *m) {
            sum += p->value + k;
        }
        EXPECT_EQ(sum, 7 + 7 + 7 + 1 + 7 + 2);
        // A node handle holds the element as stored, and hands it back
        auto h = m->extract(2);
        EXPECT_EQ(h.mapped()->value, 7);
        static_assert(std::is_same_v<decltype(h.mapped()), gc::tracked_ptr<Item>&>);
        m->insert(std::move(h));
        auto uh = us->extract(item);
        EXPECT_EQ(uh.value(), item);
        us->insert(std::move(uh));
        EXPECT_EQ(m->size() + us->size(), 3u);
    });
    // The words are in the tracked mode: the sign bit of a stored pointer
    // is clear, a pointer on the unmanaged heap is a tagged cell
    off_frame([&] {
        auto tracked_mode = [](const gc::tracked_ptr<Item>& p) { return (intptr_t&)p >= 0; };
        EXPECT_TRUE(tracked_mode(v->front()) && tracked_mode(sv.front()) && tracked_mode(d->front()) && tracked_mode(l->front()));
        EXPECT_TRUE(tracked_mode(f->front()) && tracked_mode((*a)[1]) && tracked_mode(m->begin()->second) && tracked_mode(*s->begin()));
        EXPECT_TRUE(tracked_mode(um->begin()->first) && tracked_mode(um->begin()->second) && tracked_mode(*us->begin()));
        auto cell = std::make_unique<gc::tracked_ptr<Item>>(v->front());
        EXPECT_FALSE(tracked_mode(*cell));
    });
    v.reset(); d.reset(); l.reset(); f.reset(); a.reset(); m.reset(); s.reset(); um.reset(); us.reset();
    sv.clear();
    sv.shrink_to_fit();                                          // the buffer let go; the word of sv itself is on this frame, null now
    EXPECT_EQ(live_after_collect(), live0);
}
