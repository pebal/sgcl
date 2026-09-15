//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// weak_map, weak_multimap, weak_set: containers keyed by objects they do
// not keep alive; an entry whose object is gone is dead.
#include "types.h"

#include <set>
#include <string>
#include <vector>

namespace {
    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        tracked_ptr<Node> next;
        inline static std::atomic<int> alive = {0};
    };

    // Inlined into the test's frame: a frame of its own would sit where
    // the dead frames were and keep their words (gc.cpp: live_after_collect)
    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(WeakMap_Tests, ValuesAreFoundByTheObject) {
    weak_map<Node, std::string> names;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    EXPECT_TRUE(names.empty());
    auto [it, inserted] = names.insert(a, "a");
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->key, a);
    EXPECT_EQ(it->value, "a");
    EXPECT_FALSE(names.insert(a, "again").second);
    EXPECT_EQ(names.find(a)->value, "a");
    names[b] = "b";
    EXPECT_EQ(names.size(), 2u);
    EXPECT_EQ(names[a], "a");
    EXPECT_EQ(names[b], "b");
    EXPECT_TRUE(names.contains(a));
    EXPECT_EQ(names.count(b), 1u);
    names.insert_or_assign(a, "A");
    EXPECT_EQ(names[a], "A");
    EXPECT_EQ(names.size(), 2u);
    EXPECT_EQ(names.erase(a), 1u);
    EXPECT_EQ(names.erase(a), 0u);
    EXPECT_FALSE(names.contains(a));
    EXPECT_TRUE(names.find(a) == names.end());
    EXPECT_EQ(names.size(), 1u);
}

TEST(WeakMap_Tests, ANullPointerHasNoEntry) {
    weak_map<Node, int> counts;
    tracked_ptr<Node> null;
    EXPECT_FALSE(counts.contains(null));
    EXPECT_EQ(counts.count(null), 0u);
    EXPECT_EQ(counts.erase(null), 0u);
    EXPECT_TRUE(counts.find(null) == counts.end());
    weak_multimap<Node, int> many;
    auto [first, last] = many.equal_range(null);
    EXPECT_TRUE(first == last);
}

TEST(WeakMap_Tests, TheMapDoesNotKeepItsObjectsAlive) {
    settle();
    const int before = Node::alive.load();
    weak_map<Node, std::string> names;
    tracked_ptr<Node> kept;
    off_frame([&] {                        // the allocations in frames of their own: the test frame keeps exact words only
        kept = make_tracked<Node>(1);
        names[kept] = "kept";
        tracked_ptr dropped = make_tracked<Node>(2);
        names[dropped] = "dropped";
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
    EXPECT_EQ(names[kept], "kept");
}

TEST(WeakMap_Tests, ADeadEntryDoesNotBlockTheSlotsNextObject) {
    // The slots of dead objects are handed out again; the entries left for
    // the old objects must not be found for the new ones. Enough objects
    // to fill pages, so that the freed slots are taken before the sweeps
    // run by themselves would have dropped the entries.
    settle();
    constexpr int Count = 16384;
    weak_map<Node, int> ids;
    std::set<uintptr_t> old_addresses;
    vector<tracked_ptr<Node>> kept;        // one in a while: the pages are not emptied entirely
    off_frame([&] {
        for (int i = 0; i < Count; ++i) {
            tracked_ptr dropped = make_tracked<Node>(i);
            ids[dropped] = -1;
            old_addresses.insert(hide(dropped.get()));
            if (i % 256 == 0) {
                kept.push_back(dropped);
            }
        }
    });
    settle();
    const auto dead = ids.size();          // the sweeps have not dropped all of them
    EXPECT_GT(dead, 0u);
    int reused = 0;
    vector<tracked_ptr<Node>> fresh;
    for (int i = 0; i < Count; ++i) {
        tracked_ptr node = make_tracked<Node>(Count + i);
        reused += (int)old_addresses.count(hide(node.get()));
        EXPECT_FALSE(ids.contains(node));
        ids[node] = i;
        EXPECT_EQ(ids[node], i);
        EXPECT_EQ(ids.count(node), 1u);
        fresh.push_back(node);
    }
    EXPECT_GT(reused, 0);                  // the case under test occurred
    ids.sweep();
    EXPECT_EQ(ids.size(), (size_t)Count + kept.size());
    for (int i = 0; i < Count; ++i) {
        EXPECT_EQ(ids[fresh[i]], i);
    }
}

TEST(WeakMap_Tests, TheInsertionsSweepTheDeadEntriesByThemselves) {
    settle();
    weak_map<Node, int> counts;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            tracked_ptr dropped = make_tracked<Node>(i);
            counts[dropped] = i;
        }
    });
    settle();
    EXPECT_LE(counts.size(), 100u);
    tracked_ptr kept = make_tracked<Node>(-1);
    counts[kept] = 0;
    for (int i = 0; i < 200; ++i) {        // more insertions than entries: a sweep ran
        counts[kept] += 1;
    }
    EXPECT_EQ(counts[kept], 200);
    off_frame([&] {
        for (int i = 0; i < 200; ++i) {
            tracked_ptr dropped = make_tracked<Node>(i);
            counts[dropped] = i;
        }
    });
    settle();
    const auto before = counts.size();
    off_frame([&] {
        for (int i = 0; i < 400; ++i) {
            tracked_ptr dropped = make_tracked<Node>(i);
            counts[dropped] = i;
        }
    });
    EXPECT_LT(counts.size(), before + 400);
    EXPECT_EQ(counts[kept], 200);
}

TEST(WeakMap_Tests, AValueHoldingItsKeyKeepsTheEntryAlive) {
    settle();
    const int before = Node::alive.load();
    weak_map<Node, tracked_ptr<Node>> self;
    off_frame([&] {
        tracked_ptr dropped = make_tracked<Node>(1);
        self[dropped] = dropped;            // the documented pitfall
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    EXPECT_EQ(self.sweep(), 0u);
    self.clear();
    settle();
    EXPECT_EQ(Node::alive.load(), before);
}

TEST(WeakMap_Tests, ErasingThroughAnIteratorReturnsTheNextLiveEntry) {
    settle();
    weak_map<Node, int> counts;
    tracked_ptr<Node> a, b;
    off_frame([&] {
        a = make_tracked<Node>(1);
        b = make_tracked<Node>(2);
        counts[a] = 1;
        counts[b] = 2;
        tracked_ptr dropped = make_tracked<Node>(3);
        counts[dropped] = 3;
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

TEST(WeakMultimap_Tests, SeveralValuesPerObjectAndTheirRange) {
    settle();
    weak_multimap<Node, std::string> tags;
    tracked_ptr a = make_tracked<Node>(1);
    tracked_ptr b = make_tracked<Node>(2);
    tags.insert(a, "x");
    tags.insert(a, "y");
    tags.emplace(b, "z");
    EXPECT_EQ(tags.count(a), 2u);
    EXPECT_EQ(tags.count(b), 1u);
    std::set<std::string> found;
    for (auto [first, last] = tags.equal_range(a); first != last; ++first) {
        EXPECT_EQ(first->key, a);
        found.insert(first->value);
    }
    EXPECT_EQ(found, (std::set<std::string>{"x", "y"}));
    EXPECT_EQ(tags.erase(a), 2u);
    EXPECT_EQ(tags.count(a), 0u);
    off_frame([&] {
        tracked_ptr dropped = make_tracked<Node>(3);
        tags.insert(dropped, "gone");
        tags.insert(dropped, "gone too");
    });
    settle();
    EXPECT_EQ(tags.size(), 3u);
    int seen = 0;
    for (auto entry : tags) {
        ++seen;
        EXPECT_EQ(entry.key, b);
    }
    EXPECT_EQ(seen, 1);
    EXPECT_EQ(tags.sweep(), 2u);
    EXPECT_EQ(tags.size(), 1u);
}

TEST(WeakSet_Tests, ObjectsRegisteredWithoutBeingKept) {
    settle();
    const int before = Node::alive.load();
    weak_set<Node> seen;
    tracked_ptr a = make_tracked<Node>(1);
    EXPECT_TRUE(seen.insert(a).second);
    EXPECT_FALSE(seen.insert(a).second);
    EXPECT_TRUE(seen.contains(a));
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

TEST(WeakSet_Tests, TheIteratorHoldsTheObjectItStandsOn) {
    settle();
    const int before = Node::alive.load();
    weak_set<Node> seen;
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

TEST(WeakMap_Tests, TheGcKindLivesAnywhere) {
    settle();
    const int before = Node::alive.load();
    auto* names = new gc::weak_map<Node, std::string>();
    auto* seen = new gc::weak_set<Node>();
    gc::tracked_ptr kept = gc::make_tracked<Node>(1);
    (*names)[kept] = "kept";
    seen->insert(kept);
    off_frame([&] {
        gc::tracked_ptr dropped = gc::make_tracked<Node>(2);
        (*names)[dropped] = "dropped";
        seen->insert(dropped);
    });
    settle();
    EXPECT_EQ(Node::alive.load(), before + 1);
    EXPECT_EQ(names->sweep(), 1u);
    EXPECT_EQ(seen->sweep(), 1u);
    EXPECT_EQ((*names)[kept], "kept");
    EXPECT_TRUE(seen->contains(kept));
    delete names;
    delete seen;
}
