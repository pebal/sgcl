//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// concurrent_weak_map, concurrent_weak_set: the weak containers shared by
// any number of threads; an entry whose object is gone is dead, the
// inserting threads sweep the dead ones out.
#include "tests/types.h"

#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        tracked_ptr<Node> next;
        inline static sgcl::atomic<int> alive = {0};
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

TEST(ConcurrentWeakMap_Tests, ValuesAreFoundByTheObject) {
    concurrent_weak_map<Node, std::string> names;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    EXPECT_TRUE(names.empty());
    auto [it, inserted] = names.insert(a, "a");
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->key, a);
    EXPECT_EQ(it->value, "a");
    EXPECT_FALSE(names.insert(a, "again").second);
    EXPECT_EQ(names.find(a)->value, "a");
    EXPECT_TRUE(names.try_emplace(b, 3, 'b').second);
    EXPECT_FALSE(names.emplace(b, "no").second);
    EXPECT_EQ(names.size(), 2u);
    EXPECT_EQ(names.find(b)->value, "bbb");
    EXPECT_TRUE(names.contains(a));
    EXPECT_EQ(names.count(b), 1u);
    EXPECT_EQ(names.erase(a), 1u);
    EXPECT_EQ(names.erase(a), 0u);
    EXPECT_FALSE(names.contains(a));
    EXPECT_TRUE(names.find(a) == names.end());
    EXPECT_EQ(names.size(), 1u);
    std::set<std::string> seen;
    for (auto [key, value] : names) {
        seen.insert(value);
    }
    EXPECT_EQ(seen, (std::set<std::string>{"bbb"}));
    names.clear();
    EXPECT_TRUE(names.empty());
    EXPECT_EQ(names.size(), 0u);
}

TEST(ConcurrentWeakMap_Tests, ANullPointerHasNoEntry) {
    concurrent_weak_map<Node, int> counts;
    tracked_ptr<Node> null;
    EXPECT_FALSE(counts.contains(null));
    EXPECT_EQ(counts.count(null), 0u);
    EXPECT_EQ(counts.erase(null), 0u);
    EXPECT_TRUE(counts.find(null) == counts.end());
    concurrent_weak_set<Node> objects;
    EXPECT_FALSE(objects.contains(null));
    EXPECT_TRUE(objects.find(null) == objects.end());
}

TEST(ConcurrentWeakMap_Tests, TheMapDoesNotKeepItsObjectsAlive) {
    settle();
    const int before = Node::alive.load();
    concurrent_weak_map<Node, std::string> names;
    tracked_ptr<Node> kept;
    off_frame([&] {                        // the allocations in frames of their own: the test frame keeps exact words only
        kept = make_tracked<Node>(1);
        names.insert(kept, "kept");
        tracked_ptr dropped = make_tracked<Node>(2);
        names.insert(dropped, "dropped");
    });
    EXPECT_EQ(Node::alive.load(), before + 2);
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    EXPECT_EQ(names.size(), 2u);           // the dead entry is still counted
    int seen = 0;
    for (auto entry : names) {             // and not visited
        ++seen;
        EXPECT_EQ(entry.key, kept);
        EXPECT_EQ(entry.value, "kept");
    }
    EXPECT_EQ(seen, 1);
    EXPECT_EQ(names.sweep(), 1u);
    EXPECT_EQ(names.size(), 1u);
    EXPECT_EQ(names.sweep(), 0u);
    EXPECT_EQ(names.find(kept)->value, "kept");
}

TEST(ConcurrentWeakMap_Tests, ADeadEntryDoesNotBlockTheSlotsNextObject) {
    // The slots of dead objects are handed out again; the entries left for
    // the old objects must not be found for the new ones. Enough objects
    // to fill pages, so that the freed slots are taken before the sweeps
    // run by themselves would have dropped the entries.
    settle();
    constexpr int Count = 16384;
    concurrent_weak_map<Node, int> ids;
    std::set<uintptr_t> old_addresses;
    vector<tracked_ptr<Node>> kept;        // one in a while: the pages are not emptied entirely
    off_frame([&] {
        for (int i = 0; i < Count; ++i) {
            tracked_ptr dropped = make_tracked<Node>(i);
            ids.try_emplace(dropped, -1);
            old_addresses.insert(hide(dropped.get()));
            if (i % 256 == 0) {
                kept.push_back(dropped);
            }
        }
    });
    settle();
    const auto dead = ids.size();          // the sweeps have not dropped all of them
    EXPECT_GT(dead, kept.size());
    int reused = 0;
    vector<tracked_ptr<Node>> fresh;
    for (int i = 0; i < Count; ++i) {
        tracked_ptr node = make_tracked<Node>(Count + i);
        reused += (int)old_addresses.count(hide(node.get()));
        EXPECT_FALSE(ids.contains(node));
        EXPECT_TRUE(ids.try_emplace(node, i).second);
        EXPECT_EQ(ids.find(node)->value, i);
        EXPECT_EQ(ids.count(node), 1u);
        fresh.push_back(node);
    }
    EXPECT_GT(reused, 0);                  // the case under test occurred
    ids.sweep();
    EXPECT_EQ(ids.size(), (size_t)Count + kept.size());
    for (int i = 0; i < Count; ++i) {
        EXPECT_EQ(ids.find(fresh[i])->value, i);
    }
}

TEST(ConcurrentWeakMap_Tests, TheInsertionsSweepTheDeadEntriesByThemselves) {
    settle();
    concurrent_weak_map<Node, int> counts;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            tracked_ptr dropped = make_tracked<Node>(i);
            counts.try_emplace(dropped, i);
        }
    });
    settle();
    EXPECT_LE(counts.size(), 100u);
    tracked_ptr kept = make_tracked<Node>(-1);
    counts.try_emplace(kept, 0);
    off_frame([&] {
        for (int i = 0; i < 200; ++i) {
            tracked_ptr dropped = make_tracked<Node>(i);
            counts.try_emplace(dropped, i);
        }
    });
    settle();
    const auto before = counts.size();
    off_frame([&] {
        for (int i = 0; i < 400; ++i) {    // more insertions than entries: a sweep ran
            tracked_ptr dropped = make_tracked<Node>(i);
            counts.try_emplace(dropped, i);
        }
    });
    EXPECT_LT(counts.size(), before + 400);
    EXPECT_EQ(counts.find(kept)->value, 0);
    settle();
    counts.sweep();
    EXPECT_EQ(counts.size(), 1u);
}

TEST(ConcurrentWeakMap_Tests, AValueHoldingItsKeyKeepsTheEntryAlive) {
    settle();
    const int before = Node::alive.load();
    concurrent_weak_map<Node, tracked_ptr<Node>> self;
    off_frame([&] {
        tracked_ptr dropped = make_tracked<Node>(1);
        self.try_emplace(dropped, dropped);   // the documented pitfall: the value is traced, the key lives
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    EXPECT_EQ(self.sweep(), 0u);
    off_frame([&] {                        // the walk and the clear in a frame below: the node cleared out is a managed object, and a word of it left in this frame would keep it, and the key with it
        for (auto [key, value] : self) {
            EXPECT_EQ(key, value);
            EXPECT_EQ(value->value, 1);
        }
        self.clear();
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(ConcurrentWeakMap_Tests, ErasingThroughAnIteratorReturnsTheNextLiveEntry) {
    settle();
    concurrent_weak_map<Node, int> counts;
    tracked_ptr<Node> a, b;
    off_frame([&] {
        a = make_tracked<Node>(1);
        b = make_tracked<Node>(2);
        counts.try_emplace(a, 1);
        counts.try_emplace(b, 2);
        tracked_ptr dropped = make_tracked<Node>(3);
        counts.try_emplace(dropped, 3);
    });
    settle();
    int seen = 0;
    for (auto it = counts.begin(); it != counts.end();) {
        ++seen;
        it = counts.erase(it);
    }
    EXPECT_EQ(seen, 2);
    EXPECT_EQ(counts.sweep(), 1u);
    EXPECT_TRUE(counts.empty());
}

TEST(ConcurrentWeakMap_Tests, MapInsideManagedObject) {
    settle();
    const int before = Node::alive.load();
    struct Holder {
        concurrent_weak_map<Node, tracked_ptr<Baz>> m;
    };
    tracked_ptr h = make_tracked<Holder>();
    tracked_ptr<Node> kept;
    off_frame([&] {
        kept = make_tracked<Node>(1);
        h->m.try_emplace(kept, make_tracked<Baz>(1));
        tracked_ptr dropped = make_tracked<Node>(2);
        h->m.try_emplace(dropped, make_tracked<Baz>(2));
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    EXPECT_EQ(h->m.find(kept)->value->value, 1);   // the value, traced through the holder
    EXPECT_EQ(h->m.sweep(), 1u);
    EXPECT_EQ(h->m.size(), 1u);
}

// Threads insert objects they then drop, a few kept, while the collector
// runs, in two rounds with a collection between: the entries of the
// first round are dead when the second inserts as many again, so a sweep
// by one of the inserting threads runs during the second round, under
// the others, and drops them all; settled, the map holds the kept ones
TEST(ConcurrentWeakMap_Tests, ManyThreadsInsertObjectsTheyDrop) {
    settle();
    const int before = Node::alive.load();
    const int threads = 8;
    const int per_thread = 4000;
    concurrent_weak_map<Node, int> m;
    vector<tracked_ptr<Node>> kept;
    sgcl::atomic<size_t> largest = {0};
    off_frame([&] {
        concurrent_queue<tracked_ptr<Node>> kept_by_threads;   // what the threads keep, handed over
        auto round = [&](int base) {
            std::vector<std::thread> ws;
            for (int t = 0; t < threads; ++t) {
                ws.emplace_back([&, t] {
                    for (int i = 0; i < per_thread; ++i) {
                        int k = base + i * threads + t;
                        tracked_ptr node = make_tracked<Node>(k);
                        if (!m.try_emplace(node, k).second) {
                            ADD_FAILURE() << "object " << k << " inserted twice";
                        }
                        if (m.find(node)->value != k) {
                            ADD_FAILURE() << "object " << k << " not found";
                        }
                        if (i % 500 == 0) {
                            kept_by_threads.push(node);
                        }
                        if (i % 500 == 499 && t == 0) {
                            collector::force_collect();
                        }
                        size_t n = m.size();
                        size_t seen = largest.load();
                        while (n > seen && !largest.compare_exchange_weak(seen, n)) {
                        }
                    }
                });
            }
            for (auto& w : ws) {
                w.join();
            }
        };
        round(0);
        collector::force_collect(true);    // the dropped objects of the first round are dead
        collector::force_collect(true);
        const size_t first = m.size();
        round(threads * per_thread);
        EXPECT_LT(m.size(), first + size_t(threads * per_thread));   // a sweep ran under the second round
        while (auto p = kept_by_threads.try_pop()) {
            kept.push_back(*p);
        }
    });
    EXPECT_LE(largest.load(), size_t(2 * threads * per_thread));
    settle();
    m.sweep();
    EXPECT_EQ(m.size(), kept.size());
    EXPECT_EQ(Node::alive.load(), before + (int)kept.size());
    for (auto& p : kept) {
        EXPECT_EQ(m.find(p)->value, p->value);
    }
    size_t walked = 0;
    for (auto [key, value] : m) {
        ++walked;
        EXPECT_EQ(key->value, value);
    }
    EXPECT_EQ(walked, kept.size());
}

// Threads insert, erase and look up over a shared set of objects, walk the
// map, and let the collector run, while other objects die under the map:
// every value matches its key, exactly one insertion of an object wins,
// and settled, the map holds what the lookups say.
TEST(ConcurrentWeakMap_Tests, ChurnManyThreadsOverSharedObjects) {
    settle();
    const int threads = 8;
    const int range = 1000;
    const int ops = 30000;
    off_frame([&] {
        concurrent_weak_map<Node, int> m;
        vector<tracked_ptr<Node>> objects;
        for (int i = 0; i < range; ++i) {
            objects.push_back(make_tracked<Node>(i));
        }
        sgcl::atomic<bool> bad = {false};
        std::vector<std::thread> ws;
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                std::mt19937 rng(unsigned(t + 1));
                std::uniform_int_distribution<int> pick(0, range - 1);
                std::uniform_int_distribution<int> op(0, 99);
                for (int i = 0; i < ops; ++i) {
                    tracked_ptr<Node> node = objects[size_t(pick(rng))];
                    int a = op(rng);
                    if (a < 30) {
                        auto [it, ok] = m.try_emplace(node, node->value);
                        if (it->key != node || it->value != node->value) {
                            bad = true;
                        }
                    } else if (a < 50) {
                        m.erase(node);
                    } else if (a < 60) {
                        tracked_ptr dropped = make_tracked<Node>(-1);   // an object that dies under the map
                        m.try_emplace(dropped, -1);
                    } else if (a < 95) {
                        auto it = m.find(node);
                        if (it != m.end() && (it->key != node || it->value != node->value)) {
                            bad = true;
                        }
                    } else if (a < 99) {
                        for (auto [key, value] : m) {   // a walk while the others modify
                            if (key->value != value) {
                                bad = true;
                            }
                        }
                    } else if (t == 0) {
                        collector::force_collect();
                    }
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_FALSE(bad.load());
        collector::force_collect(true);    // the objects dropped by the threads are dead now
        collector::force_collect(true);
        size_t found = 0;
        for (auto& node : objects) {
            if (auto it = m.find(node); it != m.end()) {
                ++found;
                EXPECT_EQ(it->value, node->value);
            }
        }
        size_t walked = 0;
        for (auto [key, value] : m) {
            ++walked;
            EXPECT_EQ(key->value, value);
        }
        EXPECT_EQ(walked, found);
        m.sweep();
        EXPECT_EQ(m.size(), found);
    });
}

TEST(ConcurrentWeakSet_Tests, ObjectsRegisteredWithoutBeingKept) {
    settle();
    const int before = Node::alive.load();
    concurrent_weak_set<Node> seen;
    tracked_ptr a = make_tracked<Node>(1);
    EXPECT_TRUE(seen.insert(a).second);
    EXPECT_FALSE(seen.insert(a).second);
    EXPECT_TRUE(seen.contains(a));
    EXPECT_EQ(*seen.find(a), a);
    off_frame([&] {
        tracked_ptr dropped = make_tracked<Node>(2);
        EXPECT_TRUE(seen.insert(dropped).second);
        EXPECT_EQ(seen.size(), 2u);
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    int visited = 0;
    for (auto object : seen) {
        ++visited;
        EXPECT_EQ(object, a);
        EXPECT_EQ(object->value, 1);
    }
    EXPECT_EQ(visited, 1);
    EXPECT_EQ(seen.sweep(), 1u);
    EXPECT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen.erase(a), 1u);
    EXPECT_TRUE(seen.empty());
}

TEST(ConcurrentWeakSet_Tests, TheIteratorHoldsTheObjectItStandsOn) {
    settle();
    const int before = Node::alive.load();
    concurrent_weak_set<Node> seen;
    off_frame([&] {
        tracked_ptr dropped = make_tracked<Node>(2);
        seen.insert(dropped);
    });
    auto it = seen.begin();                // stands on the object: holds it
    ASSERT_TRUE(it != seen.end());
    collector::force_collect(true);
    collector::force_collect(true);
    EXPECT_EQ(Node::alive.load(), before + 1);
    off_frame([&] {                        // the copies of the pointer in a frame of their own
        EXPECT_EQ((*it)->value, 2);
        it = seen.end();
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before);
    EXPECT_EQ(seen.sweep(), 1u);
}

// Every thread registers the same objects: one insertion per object wins;
// and objects registered and dropped by the threads in a first round are
// dead by a second, whose insertions sweep them out under the threads
TEST(ConcurrentWeakSet_Tests, ManyThreadsRegisterAndDrop) {
    settle();
    const int before = Node::alive.load();
    const int threads = 8;
    const int shared = 2000;
    const int dropped_per_thread = 3000;
    concurrent_weak_set<Node> s;
    vector<tracked_ptr<Node>> objects;
    sgcl::atomic<int> inserted = {0};
    off_frame([&] {
        for (int i = 0; i < shared; ++i) {
            objects.push_back(make_tracked<Node>(i));
        }
        auto round = [&] {
            std::vector<std::thread> ws;
            for (int t = 0; t < threads; ++t) {
                ws.emplace_back([&, t] {
                    for (int i = 0; i < shared; ++i) {
                        if (s.insert(objects[size_t(i)]).second) {
                            ++inserted;
                        }
                    }
                    for (int i = 0; i < dropped_per_thread; ++i) {
                        tracked_ptr dropped = make_tracked<Node>(-1);
                        s.insert(dropped);
                        if (i % 500 == 499 && t == 0) {
                            collector::force_collect();
                        }
                    }
                });
            }
            for (auto& w : ws) {
                w.join();
            }
        };
        round();
        collector::force_collect(true);    // the dropped objects of the first round are dead
        collector::force_collect(true);
        const size_t first = s.size();
        round();
        EXPECT_LT(s.size(), first + size_t(threads * dropped_per_thread));   // a sweep ran under the second round
    });
    EXPECT_EQ(inserted.load(), shared);    // one winner per shared object, in the first round
    settle();
    s.sweep();
    EXPECT_EQ(s.size(), size_t(shared));
    EXPECT_EQ(Node::alive.load(), before + shared);
    for (auto& node : objects) {
        EXPECT_TRUE(s.contains(node));
    }
    size_t walked = 0;
    for (auto object : s) {
        ++walked;
        EXPECT_GE(object->value, 0);
    }
    EXPECT_EQ(walked, size_t(shared));
}
