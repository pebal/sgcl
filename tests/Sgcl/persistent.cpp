//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The persistent structures of the Sgcl interface: PersistentList,
// PersistentDictionary, PersistentSet over their sgcl types.
#include "tests/types.h"

#include "sgcl/Sgcl/Sgcl.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    struct Item {
        explicit Item(int v = 0) : value(v) { ++alive; }
        Item(const Item& o) : value(o.value) { ++alive; }
        ~Item() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Sgcl_Persistent_Tests, ListVersions) {
    using namespace Sgcl;
    PersistentList<int> empty;
    EXPECT_TRUE(empty.IsEmpty());
    EXPECT_EQ(empty.Count(), 0u);
    PersistentList<int> l = {1, 2, 3};
    auto added = l.Add(4);
    auto set = l.Set(0, 10);
    auto shorter = l.RemoveLast();
    EXPECT_EQ(l.Count(), 3u);
    EXPECT_EQ(added.Count(), 4u);
    EXPECT_EQ(set.Count(), 3u);
    EXPECT_EQ(shorter.Count(), 2u);
    EXPECT_EQ(l[0], 1);
    EXPECT_EQ(set[0], 10);
    EXPECT_EQ(added.Last(), 4);
    EXPECT_EQ(shorter.Last(), 2);
    EXPECT_EQ(l.First(), 1);
    EXPECT_EQ(l.At(2), 3);
    EXPECT_THROW(l.At(3), std::out_of_range);
    EXPECT_NE(l, set);
    EXPECT_EQ(l, added.RemoveLast());
    EXPECT_EQ(l, set.Set(0, 1));
    int sum = 0;
    for (int x : added) {
        sum += x;
    }
    EXPECT_EQ(sum, 10);
    static_assert(std::is_same_v<PersistentList<int>::InnerType, sgcl::persistent_vector<int>>);
    EXPECT_EQ(added.Inner().size(), 4u);
    PersistentList<int> big;
    for (int i : Range(5000)) {
        big = big.Add(i);
    }
    EXPECT_EQ(big.Count(), 5000u);
    EXPECT_EQ(big[4999], 4999);
    EXPECT_EQ(std::ranges::find(big, 1234) - begin(big), 1234);
    std::vector<int> v = {7, 8, 9};
    PersistentList from(v.begin(), v.end());
    static_assert(std::is_same_v<decltype(from), PersistentList<int>>);
    EXPECT_EQ(from.Emplace(10).Count(), 4u);
}

TEST(Sgcl_Persistent_Tests, ListElementsSharedAndCollected) {
    using namespace Sgcl;
    settle();
    const int before = Item::alive.load();
    PersistentList<Ptr<Item>> a, b;
    off_frame([&] {
        for (int i : Range(100)) {
            a = a.Add(Make<Item>(i));
        }
        b = a.Set(0, Make<Item>(-1));
    });
    settle();
    EXPECT_EQ(Item::alive.load(), before + 101);   // the 100 and the one replaced: both versions alive
    off_frame([&] {                                 // the raw pointers of the checks stay out of this frame
        EXPECT_EQ(a[0]->value, 0);
        EXPECT_EQ(b[0]->value, -1);
        EXPECT_EQ(a[1], b[1]);                      // the same object in both
    });
    a = {};
    settle();
    EXPECT_EQ(Item::alive.load(), before + 100);   // item 0 dead with the version that held it
    b = {};
    settle();
    EXPECT_EQ(Item::alive.load(), before);
}

TEST(Sgcl_Persistent_Tests, DictionaryVersions) {
    using namespace Sgcl;
    PersistentDictionary<String, int> d;
    EXPECT_TRUE(d.IsEmpty());
    auto one = d.Set("one", 1);
    auto two = one.Set("two", 2);
    auto changed = two.Set("one", 11);
    auto less = two.Remove("one");
    EXPECT_EQ(d.Count(), 0u);
    EXPECT_EQ(one.Count(), 1u);
    EXPECT_EQ(two.Count(), 2u);
    EXPECT_EQ(changed.Count(), 2u);
    EXPECT_EQ(less.Count(), 1u);
    EXPECT_EQ(*two.Find("one"), 1);
    EXPECT_EQ(*changed.Find("one"), 11);
    EXPECT_EQ(less.Find("one"), nullptr);
    EXPECT_EQ(two.TryGet("two"), Optional<int>(2));
    EXPECT_EQ(less.TryGet("one"), None);
    EXPECT_EQ(two.At("two"), 2);
    EXPECT_THROW(d.At("two"), std::out_of_range);
    EXPECT_TRUE(two.ContainsKey(std::string_view("one")));   // transparent: no String made
    EXPECT_FALSE(less.ContainsKey(std::string_view("one")));
    EXPECT_EQ(two.Remove("none"), two);
    EXPECT_NE(two, changed);
    EXPECT_EQ(two, changed.Set("one", 1));
    EXPECT_EQ(two, less.Set("one", 1));
    int sum = 0;
    for (auto& [k, v] : two) {
        sum += v;
    }
    EXPECT_EQ(sum, 3);
    static_assert(std::is_same_v<PersistentDictionary<String, int>::InnerType, sgcl::persistent_map<String, int>>);
    EXPECT_EQ(two.Inner().size(), 2u);
    PersistentDictionary<int, String> il = {{1, "a"}, {2, "b"}};
    EXPECT_EQ(il.Emplace(3, "ccc", 2).At(3), "cc");
    EXPECT_EQ(il.Count(), 2u);
    PersistentDictionary<int, int> big;
    for (int i : Range(5000)) {
        big = big.Set(i, i * i);
    }
    auto kept = big;
    for (int i : Range(2500)) {
        big = big.Remove(i);
    }
    EXPECT_EQ(big.Count(), 2500u);
    EXPECT_EQ(kept.Count(), 5000u);
    EXPECT_EQ(*kept.Find(10), 100);
    EXPECT_EQ(big.Find(10), nullptr);
    EXPECT_EQ(*big.Find(4000), 16000000);
}

TEST(Sgcl_Persistent_Tests, SetVersions) {
    using namespace Sgcl;
    PersistentSet<int> s = {1, 2, 3};
    auto more = s.Add(4);
    auto fewer = s.Remove(1);
    EXPECT_EQ(s.Count(), 3u);
    EXPECT_EQ(more.Count(), 4u);
    EXPECT_EQ(fewer.Count(), 2u);
    EXPECT_TRUE(s.Contains(1));
    EXPECT_FALSE(fewer.Contains(1));
    EXPECT_TRUE(more.Contains(4));
    EXPECT_EQ(*more.Find(4), 4);
    EXPECT_EQ(s.Find(4), nullptr);
    EXPECT_EQ(s.Add(2), s);
    EXPECT_EQ(s, more.Remove(4));
    EXPECT_NE(s, fewer);
    int sum = 0;
    for (int x : more) {
        sum += x;
    }
    EXPECT_EQ(sum, 10);
    PersistentSet<String> names = {"alice", "bob"};
    EXPECT_TRUE(names.Contains(std::string_view("bob")));   // transparent
    EXPECT_FALSE(names.Remove("bob").Contains("bob"));
    static_assert(std::is_same_v<PersistentSet<int>::InnerType, sgcl::persistent_set<int>>);
    EXPECT_EQ(more.Inner().size(), 4u);
}

// Versions published through a CopyOnWrite: readers take whole snapshots
// while a writer adds, each update a path copied, not a dictionary
TEST(Sgcl_Persistent_Tests, PublishedThroughCopyOnWrite) {
    using namespace Sgcl;
    CopyOnWrite<PersistentDictionary<int, int>> current;
    Atomic<bool> stop = {false};
    Atomic<bool> bad = {false};
    off_frame([&] {
        std::vector<std::thread> readers;
        for (int t : Range(4)) {
            (void)t;
            readers.emplace_back([&] {
                while (!stop.Load()) {
                    auto s = current.Load();
                    for (int k : Range(int(s->Count()))) {
                        auto v = s->Find(k);
                        if (!v || *v != k * 2) {
                            bad = true;
                        }
                    }
                }
            });
        }
        for (int i : Range(5000)) {
            current.Update([i](auto& d) { d = d.Set(i, i * 2); });
        }
        stop = true;
        for (auto& r : readers) {
            r.join();
        }
    });
    EXPECT_FALSE(bad.Load());
    EXPECT_EQ(current.Load()->Count(), 5000u);
}
