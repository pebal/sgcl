//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/managed_pages.h"
#include "tests/types.h"

#include <algorithm>
#include <map>
#include <random>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
    // A hash that sends every key below 1000 to one value, and the
    // others to a few: collisions at the last bit and at the first ones
    struct CollidingHash {
        size_t operator()(int k) const noexcept {
            return k < 1000 ? 42 : size_t(k) % 5 * 0x1000000000ull + 1;
        }
    };

    template<class M, class O>
    bool same_map(const M& m, const O& o) {
        if (m.size() != o.size()) {
            return false;
        }
        size_t n = 0;
        for (auto& [k, v] : m) {
            auto it = o.find(k);
            if (it == o.end() || it->second != v) {
                return false;
            }
            ++n;
        }
        if (n != o.size()) {
            return false;
        }
        for (auto& [k, v] : o) {
            auto p = m.try_get(k);
            if (!p || *p != v) {
                return false;
            }
        }
        return true;
    }

    template<class S, class O>
    bool same_set(const S& s, const O& o) {
        if (s.size() != o.size()) {
            return false;
        }
        size_t n = 0;
        for (auto& k : s) {
            if (!o.contains(k)) {
                return false;
            }
            ++n;
        }
        return n == o.size() && std::all_of(o.begin(), o.end(), [&](auto& k) { return s.contains(k); });
    }

    sgcl::immutable::map<int, int> squares(int n) {
        sgcl::immutable::map<int, int> m;
        for (int i = 0; i < n; ++i) {
            m = m.insert(i, i * i);
        }
        return m;
    }

    void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(ImMap_Tests, Empty) {
    const size_t before = collector::get_live_object_count();
    sgcl::immutable::map<int, int> m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);
    EXPECT_EQ(m.begin(), m.end());
    EXPECT_EQ(m.try_get(1), nullptr);
    EXPECT_FALSE(m.contains(1));
    EXPECT_EQ(m.count(1), 0u);
    EXPECT_THROW(m.at(1), std::out_of_range);
    EXPECT_EQ(m.erase(1), m);
    EXPECT_EQ(collector::get_live_object_count(), before);   // no node for an empty map
}

TEST(ImMap_Tests, InsertFindErase) {
    sgcl::immutable::map<int, std::string> m;
    auto a = m.insert(1, "one");
    auto b = a.insert(2, "two");
    auto kept = b.insert(1, "uno");  // kept: an insert keeps what it finds
    auto c = b.set(1, "uno");        // replaced
    auto d = c.erase(2);
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(a.size(), 1u);
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(c.size(), 2u);
    EXPECT_EQ(d.size(), 1u);
    EXPECT_EQ(kept, b);
    EXPECT_EQ(*kept.try_get(1), "one");
    EXPECT_EQ(*a.try_get(1), "one");
    EXPECT_EQ(*b.try_get(1), "one");
    EXPECT_EQ(*b.try_get(2), "two");
    EXPECT_EQ(*c.try_get(1), "uno");
    EXPECT_EQ(*c.try_get(2), "two");
    EXPECT_EQ(*d.try_get(1), "uno");
    EXPECT_EQ(d.try_get(2), nullptr);
    EXPECT_EQ(c.at(1), "uno");
    EXPECT_THROW(d.at(2), std::out_of_range);
    EXPECT_TRUE(c.contains(2));
    EXPECT_EQ(c.count(2), 1u);
    EXPECT_EQ(d.erase(7), d);       // absent: the same map
    auto e = d.emplace(3, 3, 'x');  // "xxx"
    EXPECT_EQ(*e.try_get(3), "xxx");
    auto f = e.insert(sgcl::pair<const int, std::string>(4, "four"));
    EXPECT_EQ(*f.try_get(4), "four");
    EXPECT_EQ(f.size(), 3u);
}

TEST(ImMap_Tests, ListAndRangeConstructors) {
    sgcl::immutable::map<std::string, int> m = {{"a", 1}, {"b", 2}, {"a", 3}};
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(*m.try_get("a"), 1);   // the first one stays, as an insert keeps what it finds (and std::unordered_map)
    std::map<int, int> o;
    for (int i = 0; i < 300; ++i) {
        o[i] = -i;
    }
    sgcl::immutable::map p(o.begin(), o.end());   // deduced
    static_assert(std::is_same_v<decltype(p), sgcl::immutable::map<int, int>>);
    EXPECT_TRUE(same_map(p, o));
    std::vector<int> keys;
    for (auto& [k, v] : p) {
        keys.push_back(k);
    }
    std::sort(keys.begin(), keys.end());
    EXPECT_EQ(keys.size(), 300u);
    EXPECT_EQ(keys[299], 299);
}

TEST(ImMap_Tests, OldVersionUnchanged) {
    auto m = squares(1000);
    auto plus = m.insert(1000, 1);
    auto minus = m.erase(500);
    auto changed = m.set(500, -1);
    EXPECT_EQ(m.size(), 1000u);
    EXPECT_EQ(plus.size(), 1001u);
    EXPECT_EQ(minus.size(), 999u);
    EXPECT_EQ(changed.size(), 1000u);
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(*m.try_get(i), i * i);
        EXPECT_EQ(*plus.try_get(i), i * i);
        EXPECT_EQ(*changed.try_get(i), i == 500 ? -1 : i * i);
        if (i != 500) {
            EXPECT_EQ(*minus.try_get(i), i * i);
        }
    }
    EXPECT_EQ(minus.try_get(500), nullptr);
    EXPECT_EQ(*plus.try_get(1000), 1);
    EXPECT_NE(m, changed);
    EXPECT_EQ(m, changed.set(500, 250000));
    EXPECT_EQ(m, minus.insert(500, 250000));
    EXPECT_EQ(m, plus.erase(1000));
}

// Two versions differing in one element share all but a path
TEST(ImMap_Tests, Sharing) {
    const size_t before = collector::get_live_object_count();
    sgcl::immutable::map<int, int> m, plus, minus;
    off_frame([&] {
        m = squares(100000);
    });
    const size_t one = collector::get_live_object_count() - before;
    EXPECT_GT(one, 3000u);         // the nodes of the trie: far fewer than the elements
    EXPECT_LT(one, 100000u);
    off_frame([&] {
        plus = m.insert(100000, 1);
        minus = m.erase(50000);
    });
    const size_t three = collector::get_live_object_count() - before;
    EXPECT_LE(three, one + 2 * 5);   // two paths of at most five nodes
    EXPECT_GT(three, one);
    m = {};
    plus = {};
    minus = {};
    EXPECT_EQ(collector::get_live_object_count(), before);
}

// Keys with equal hashes chain under one entry, and the chains are as
// immutable as the rest
TEST(ImMap_Tests, Collisions) {
    sgcl::immutable::map<int, int, CollidingHash> m;
    for (int i = 0; i < 1200; ++i) {
        m = m.insert(i, i * 2);
    }
    EXPECT_EQ(m.size(), 1200u);
    for (int i = 0; i < 1200; ++i) {
        ASSERT_NE(m.try_get(i), nullptr) << i;
        EXPECT_EQ(*m.try_get(i), i * 2);
    }
    EXPECT_EQ(m.try_get(1200), nullptr);
    EXPECT_EQ(m.try_get(-1), nullptr);
    auto c = m.set(500, 7).erase(3).erase(999).erase(1100).insert(0, 0);
    EXPECT_EQ(c.size(), 1197u);
    EXPECT_EQ(*c.try_get(500), 7);
    EXPECT_EQ(*m.try_get(500), 1000);
    EXPECT_EQ(*c.try_get(0), 0);
    EXPECT_EQ(*m.try_get(0), 0);
    EXPECT_FALSE(c.contains(3));
    EXPECT_FALSE(c.contains(999));
    EXPECT_FALSE(c.contains(1100));
    EXPECT_TRUE(m.contains(3));
    size_t n = 0;
    std::set<int> seen;
    for (auto& [k, v] : c) {
        ++n;
        seen.insert(k);
        EXPECT_EQ(v, k == 500 ? 7 : k * 2);
    }
    EXPECT_EQ(n, 1197u);
    EXPECT_EQ(seen.size(), 1197u);
    EXPECT_EQ(c.erase(3), c);
    // the chains erased down to nothing
    std::mt19937 rng(3);
    std::vector<int> order(1200);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);
    std::unordered_map<int, int> o;
    for (int i = 0; i < 1200; ++i) {
        o[i] = i * 2;
    }
    for (int k : order) {
        m = m.erase(k);
        o.erase(k);
        if (o.size() % 97 == 0) {
            ASSERT_TRUE(same_map(m, o)) << o.size();
        }
    }
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.begin(), m.end());
}

// Erasing everything collapses the trie: no node is left, and the map
// is what an empty one is
TEST(ImMap_Tests, RangeConstructorBuiltAtOnce) {
    // The build from a range makes the trie at once, sorted by the hash
    // in the trie's order: chains (every key below 1000 to one hash),
    // splits down to the last chunk (the others to a few), duplicate
    // keys (the first occurrence stays, as an insert keeps what it finds) and
    // the same map as one built by inserts, for the map and the set
    std::vector<std::pair<int, int>> in;
    std::mt19937 rng(7);
    for (int i = 0; i < 3000; ++i) {
        int k = int(rng() % 1500);
        in.emplace_back(k, i);
    }
    std::map<int, int> o;
    sgcl::immutable::map<int, int, CollidingHash> by_inserts;
    for (auto& [k, v] : in) {
        o.emplace(k, v);                                   // the first stays, as std::map's emplace keeps it
        by_inserts = by_inserts.insert(k, v);
    }
    sgcl::immutable::map<int, int, CollidingHash> m(in.begin(), in.end());
    EXPECT_EQ(m.size(), o.size());
    EXPECT_TRUE(same_map(m, o));
    EXPECT_EQ(m, by_inserts);
    for (auto& [k, v] : o) {
        ASSERT_NE(m.try_get(k), nullptr) << k;
        EXPECT_EQ(*m.try_get(k), v);
    }
    EXPECT_EQ(m.try_get(1500), nullptr);
    auto n = m.insert(1500, 1).erase(0).erase(999);   // a version over the built trie
    EXPECT_EQ(n.size(), o.size() - 1);
    EXPECT_EQ(*m.try_get(999), o[999]);
    std::vector<int> keys;
    for (auto& [k, v] : in) {
        keys.push_back(k);
    }
    sgcl::immutable::set<int, CollidingHash> s(keys.begin(), keys.end());
    EXPECT_TRUE(same_set(s, std::set<int>(keys.begin(), keys.end())));
    sgcl::immutable::map<int, int> e(in.end(), in.end());
    EXPECT_TRUE(e.empty());
    EXPECT_EQ(e.begin(), e.end());
}

TEST(ImMap_Tests, EraseToEmptyCollapses) {
    const size_t before = collector::get_live_object_count();
    sgcl::immutable::map<int, int> m;
    off_frame([&] {
        m = squares(5000);
        for (int i = 0; i < 5000; ++i) {
            m = m.erase(i);
        }
    });
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.begin(), m.end());
    EXPECT_EQ(collector::get_live_object_count(), before);
    EXPECT_EQ(m, (sgcl::immutable::map<int, int>()));
    // erasing down to one element leaves a trie of one node
    off_frame([&] {
        m = squares(5000);
        for (int i = 1; i < 5000; ++i) {
            m = m.erase(i);
        }
    });
    EXPECT_EQ(m.size(), 1u);
    EXPECT_EQ(*m.try_get(0), 0);
    EXPECT_EQ(collector::get_live_object_count(), before + 1);
}

TEST(ImMap_Tests, TransparentLookup) {
    sgcl::immutable::map<sgcl::string, int> ages;
    ages = ages.insert("alice", 30).insert("bob", 32);
    std::string_view view("alice");
    EXPECT_EQ(*ages.try_get(view), 30);
    EXPECT_EQ(*ages.try_get("bob"), 32);
    EXPECT_TRUE(ages.contains(view));
    EXPECT_EQ(ages.count("bob"), 1u);
    EXPECT_EQ(ages.at(view), 30);
    EXPECT_THROW(ages.at(std::string_view("carol")), std::out_of_range);
    auto without = ages.erase(std::string_view("bob"));
    EXPECT_EQ(without.size(), 1u);
    EXPECT_EQ(ages.size(), 2u);
    settle();
    auto before = collector::get_statistics().live_bytes;   // managed memory in use: a temporary string would add to it
    int sum = 0;
    for (int i = 0; i < 10000; ++i) {
        sum += *ages.try_get("alice") + ages.at(view) + int(ages.count("bob")) + int(without.contains("alice"));
    }
    EXPECT_EQ(sum, 10000 * 62);
    EXPECT_LE(collector::get_statistics().live_bytes, before);   // no string made for any lookup (a sweep meanwhile can only lower the count)
    sgcl::immutable::set<sgcl::string> names = {"alice", "bob"};
    EXPECT_TRUE(names.contains(view));
    EXPECT_TRUE(names.contains("bob"));
    EXPECT_EQ(*names.find("bob"), "bob");
    EXPECT_FALSE(names.erase(view).contains("alice"));
}

// A value holding a tracked_ptr keeps its object while any version
// reaches it
TEST(ImMap_Tests, TrackedValues) {
    const size_t before = collector::get_live_object_count();
    sgcl::immutable::map<int, tracked_ptr<Baz>> m, changed;
    off_frame([&] {
        for (int i = 0; i < 100; ++i) {
            m = m.insert(i, make_tracked<Baz>(i));
        }
    });
    const size_t one = collector::get_live_object_count() - before;
    EXPECT_GT(one, 100u);
    off_frame([&] {
        changed = m.set(5, make_tracked<Baz>(-5));
    });
    EXPECT_GE(collector::get_live_object_count() - before, one + 2);   // the new Baz, the copied path
    off_frame([&] {                        // the raw pointers of the checks stay out of this frame
        EXPECT_EQ((*m.try_get(5))->value, 5);
        EXPECT_EQ((*changed.try_get(5))->value, -5);
        EXPECT_EQ(m.try_get(6)->get(), changed.try_get(6)->get());   // the same object in both
    });
    m = {};
    collector::force_collect(true);
    off_frame([&] {
        EXPECT_EQ((*changed.try_get(5))->value, -5);
        EXPECT_EQ((*changed.try_get(6))->value, 6);
    });
    changed = {};
    EXPECT_EQ(collector::get_live_object_count(), before);
}

// Keys and values with destructors die with their nodes
TEST(ImMap_Tests, ElementsDestroyed) {
    settle();
    EXPECT_EQ(Int::counter, 0u);
    sgcl::immutable::map<int, Int> m;
    off_frame([&] {
        for (int i = 0; i < 1000; ++i) {
            m = m.insert(i, Int(i));
        }
    });
    settle();
    EXPECT_EQ(Int::counter, 1000u);
    off_frame([&] {
        m = m.erase(1).erase(2).insert(3, Int(-3));
    });
    settle();
    EXPECT_EQ(Int::counter, 998u);
    m = {};
    settle();
    EXPECT_EQ(Int::counter, 0u);
}

TEST(ImMap_Tests, InsideManagedObject) {
    struct Holder {
        sgcl::immutable::map<std::string, tracked_ptr<Baz>> by_name;
        sgcl::immutable::set<int> ids;
    };
    const size_t before = collector::get_live_object_count();
    tracked_ptr h = make_tracked<Holder>();
    off_frame([&] {
        for (int i = 0; i < 50; ++i) {
            h->by_name = h->by_name.insert(std::to_string(i), make_tracked<Baz>(i));
            h->ids = h->ids.insert(i);
        }
    });
    collector::force_collect(true);
    off_frame([&] {
        EXPECT_EQ(h->by_name.size(), 50u);
        EXPECT_EQ((*h->by_name.try_get("49"))->value, 49);
        EXPECT_TRUE(h->ids.contains(49));
    });
    h = nullptr;
    EXPECT_EQ(collector::get_live_object_count(), before);
}

TEST(ImMap_Tests, Iteration) {
    auto m = squares(3000);
    static_assert(std::forward_iterator<sgcl::immutable::map<int, int>::const_iterator>);
    static_assert(std::ranges::forward_range<sgcl::immutable::map<int, int>>);
    std::set<int> keys;
    long sum = 0;
    for (auto it = m.begin(); it != m.end(); ++it) {
        keys.insert(it->first);
        sum += it->second;
    }
    EXPECT_EQ(keys.size(), 3000u);
    EXPECT_EQ(*keys.rbegin(), 2999);
    EXPECT_EQ(sum, 2999L * 3000 * 5999 / 6);
    EXPECT_EQ(std::ranges::distance(m), 3000);
    auto it = std::ranges::find_if(m, [](auto& kv) { return kv.first == 1234; });
    EXPECT_NE(it, m.end());
    EXPECT_EQ(it->second, 1234 * 1234);
    auto copy = it;
    EXPECT_EQ(copy++, it);
    EXPECT_NE(copy, it);
    EXPECT_EQ(std::ranges::distance(m.begin(), copy), std::ranges::distance(m.begin(), it) + 1);
}

// Random operations against std::unordered_map, the collector running
// meanwhile; a version kept aside stays what it was
TEST(ImMap_Tests, StressAgainstOracle) {
    std::mt19937 rng(11);
    sgcl::immutable::map<int, int> m, kept;
    std::unordered_map<int, int> o, kept_o;
    for (int step = 0; step < 100000; ++step) {
        int k = int(rng() % 20000);
        switch (rng() % 5) {
            case 0: case 1: case 2: {
                m = m.set(k, step);
                o[k] = step;
                break;
            }
            case 3:
                m = m.erase(k);
                o.erase(k);
                break;
            default:
                if (step % 10000 == 0) {
                    kept = m;
                    kept_o = o;
                    collector::force_collect();
                }
                break;
        }
        if (step % 1999 == 0) {
            ASSERT_TRUE(same_map(m, o)) << step;
            ASSERT_TRUE(same_map(kept, kept_o)) << step;
        }
    }
    EXPECT_TRUE(same_map(m, o));
    EXPECT_TRUE(same_map(kept, kept_o));
}

TEST(ImSet_Tests, Basics) {
    const size_t before = collector::get_live_object_count();
    off_frame([&] {   // the raw pointers of the checks stay below this frame
        sgcl::immutable::set<int> s;
        EXPECT_TRUE(s.empty());
        EXPECT_EQ(s.begin(), s.end());
        auto a = s.insert(1);
        auto b = a.insert(2).insert(1);
        auto c = b.erase(1);
        EXPECT_EQ(s.size(), 0u);
        EXPECT_EQ(a.size(), 1u);
        EXPECT_EQ(b.size(), 2u);
        EXPECT_EQ(c.size(), 1u);
        EXPECT_TRUE(b.contains(1));
        EXPECT_FALSE(c.contains(1));
        EXPECT_TRUE(c.contains(2));
        EXPECT_EQ(*b.find(2), 2);
        EXPECT_EQ(c.find(1), c.end());
        EXPECT_EQ(c.count(2), 1u);
        EXPECT_EQ(c.erase(5), c);
        sgcl::immutable::set<int> l = {3, 1, 2, 3};
        EXPECT_EQ(l.size(), 3u);
        EXPECT_EQ(l, sgcl::immutable::set<int>({1, 2, 3}));
        EXPECT_NE(l, b);
        std::vector<int> o = {4, 5, 6};
        sgcl::immutable::set d(o.begin(), o.end());
        static_assert(std::is_same_v<decltype(d), sgcl::immutable::set<int>>);
        EXPECT_TRUE(same_set(d, std::set<int>(o.begin(), o.end())));
    });
    EXPECT_EQ(collector::get_live_object_count(), before);   // every version dead with its frame
}

TEST(ImSet_Tests, StressAgainstOracle) {
    std::mt19937 rng(5);
    sgcl::immutable::set<int, CollidingHash> s, kept;
    std::unordered_set<int> o, kept_o;
    for (int step = 0; step < 40000; ++step) {
        int k = int(rng() % 3000);
        if (rng() % 3) {
            s = s.insert(k);
            o.insert(k);
        } else {
            s = s.erase(k);
            o.erase(k);
        }
        if (step % 5000 == 0) {
            kept = s;
            kept_o = o;
            collector::force_collect();
        }
        if (step % 1499 == 0) {
            ASSERT_TRUE(same_set(s, o)) << step;
            ASSERT_TRUE(same_set(kept, kept_o)) << step;
        }
    }
    EXPECT_TRUE(same_set(s, o));
    EXPECT_TRUE(same_set(kept, kept_o));
}

// Readers on several threads hold versions while one thread publishes new
// ones through a copy_on_write: every snapshot is whole and stays what it is
TEST(ImMap_Tests, ReadersUnderCopyOnWrite) {
    sgcl::concurrent::copy_on_write<sgcl::immutable::map<int, int>> current;
    sgcl::atomic<bool> stop = {false};
    sgcl::atomic<bool> bad = {false};
    sgcl::atomic<long> reads = {0};
    off_frame([&] {
        std::vector<std::thread> readers;
        for (int t = 0; t < 6; ++t) {
            readers.emplace_back([&] {
                size_t last = 0;
                while (!stop.load(std::memory_order_relaxed)) {
                    auto s = current.load();   // a snapshot: the map as it was
                    if (s->size() < last) {
                        bad = true;
                    }
                    last = s->size();
                    for (int k = 0; k < int(s->size()); ++k) {   // the keys 0..size-1 are all there, with their values
                        auto v = s->try_get(k);
                        if (!v || *v != k * 3) {
                            bad = true;
                        }
                    }
                    ++reads;
                }
            });
        }
        for (int i = 0; i < 10000; ++i) {
            current.update([i](auto& m) { m = m.insert(i, i * 3); });   // O(log n): a path copied, the rest shared
            if (i % 2000 == 0) {
                collector::force_collect();
            }
        }
        stop = true;
        for (auto& r : readers) {
            r.join();
        }
    });
    EXPECT_FALSE(bad.load());
    EXPECT_GT(reads.load(), 0);
    EXPECT_EQ(current.load()->size(), 10000u);
}

// find is an iterator, as every find of the library: the one begin()'s
// walk comes to, so that ++ goes on from it in that order; chains of one
// hash and subtries alike. try_get is the value's pointer.
TEST(ImMap_Tests, FindIsTheIteratorTheWalkComesTo) {
    auto check = [](const auto& m, int keys) {
        std::vector<int> walk;
        for (auto& [k, v] : m) {
            walk.push_back(k);
        }
        for (size_t i = 0; i < walk.size(); ++i) {
            auto it = m.find(walk[i]);
            ASSERT_NE(it, m.end()) << walk[i];
            EXPECT_EQ(it->first, walk[i]);
            size_t n = 0;
            for (; it != m.end(); ++it) {
                ASSERT_EQ(it->first, walk[i + n]) << walk[i] << " + " << n;   // the rest of the walk, in order
                ++n;
            }
            EXPECT_EQ(n, walk.size() - i);
        }
        EXPECT_EQ(m.find(keys + 1), m.end());
        EXPECT_EQ(m.try_get(keys + 1), nullptr);
    };
    sgcl::immutable::map<int, int> plain;
    for (int i : range(2000)) {
        plain = plain.insert(i, i);
    }
    check(plain, 2000);
    sgcl::immutable::map<int, int, CollidingHash> chains;
    for (int i : range(1500)) {
        chains = chains.insert(i, i);
    }
    check(chains, 1500);
    sgcl::immutable::set<int> s = {1, 2, 3};
    EXPECT_EQ(*s.find(2), 2);
    EXPECT_EQ(s.find(4), s.end());
}

// insert keeps what it finds, set replaces it; a range keeps the first of a key
TEST(ImMap_Tests, InsertKeepsSetReplaces) {
    sgcl::immutable::map<int, std::string> m = {{1, "a"}, {1, "b"}};
    EXPECT_EQ(*m.try_get(1), "a");
    EXPECT_EQ(m.insert(1, "c"), m);
    EXPECT_EQ(*m.set(1, "c").try_get(1), "c");
    EXPECT_EQ(m.emplace(1, 2, 'x'), m);
    sgcl::immutable::set<std::string> s = {"x"};
    EXPECT_EQ(s.insert("x"), s);
}

namespace {
    // A hash that looks at the heap once, while the map is built from a
    // range: the managed objects then alive whose type is the order of
    // the build
    struct LookingHash {
        inline static long trigger = -1;
        inline static size_t seen = 0;

        size_t operator()(long k) const noexcept {
            if (k == trigger) {
                trigger = -1;
                seen = live_objects_named("6Placed");
            }
            return std::hash<long>()(k);
        }
    };
}

// The order of a build from a range (the hashes sorted, a key's first
// occurrence kept) is scratch of the constructor: plain memory, never a
// managed buffer (240 KB of one for 10 000 elements before)
TEST(ImmutableMap_Test, TheBuildFromARangeKeepsItsOrderOffTheManagedHeap) {
    std::vector<std::pair<long, long>> items;
    for (long i = 0; i < 10000; ++i) {
        items.emplace_back(i, i);
    }
    items.emplace_back(7, -1);   // a key again: the first stays
    LookingHash::trigger = 5000;
    LookingHash::seen = 0;
    immutable::map<long, long, LookingHash> m(items.begin(), items.end());
    EXPECT_EQ(LookingHash::trigger, -1);   // the hash looked
    EXPECT_EQ(LookingHash::seen, 0u);
    EXPECT_EQ(m.size(), 10000u);
    EXPECT_EQ(*m.try_get(7), 7);
    EXPECT_EQ(*m.try_get(9999), 9999);
}

// Out of memory throws nothing (it ends the program) and the hash and
// the equality cannot throw (asserted): a change is noexcept as far as
// the elements' copies and constructions are, a lookup always
TEST(ImMap_Tests, NoexceptFollowsTheElements) {
    using M = sgcl::immutable::map<int, int>;
    using MS = sgcl::immutable::map<int, std::string>;   // a copy that can throw
    using S = sgcl::immutable::set<int>;
    using SS = sgcl::immutable::set<std::string>;
    std::vector<std::pair<int, int>> pairs;
    std::vector<int> ints;
    int k = 0;
    std::string str;
    static_assert(noexcept(M().insert(k, k)) && noexcept(M().set(k, 1)) && noexcept(M().emplace(k, 1)) && noexcept(M().erase(k)));
    static_assert(noexcept(M().find(k)) && noexcept(M().contains(k)) && noexcept(M().count(k)) && noexcept(M().try_get(k)));
    static_assert(noexcept(M(pairs.begin(), pairs.end())) && noexcept(M({{1, 2}})));
    static_assert(!noexcept(M().at(k)));   // out_of_range
    static_assert(noexcept(M().thaw()) && noexcept(M().thaw().insert(k, k)) && noexcept(M().thaw().set(k, k)) && noexcept(M().thaw().erase(k)) && noexcept(M().thaw().freeze()));
    static_assert(!noexcept(MS().insert(k, str)) && !noexcept(MS().erase(k)) && !noexcept(MS().thaw().set(k, std::string())) && !noexcept(MS().thaw().erase(k)));
    static_assert(noexcept(MS().find(k)) && noexcept(MS().thaw().try_get(k)));
    static_assert(noexcept(S().insert(k)) && noexcept(S().erase(k)) && noexcept(S(ints.begin(), ints.end())) && noexcept(S().thaw().insert(k)));
    static_assert(!noexcept(SS().insert(str)) && !noexcept(SS().erase(str)) && noexcept(SS().contains(str)));
    SUCCEED();
}

// A map or set moved from keeps its version, as the copy does (map.md (6),
// set.md (6), operator_assign.md (2)), and every member works on it; an
// assignment to itself, by a copy or a move, changes nothing
TEST(ImMap_Tests, MovedFromAndSelfAssignment) {
    for (int n : {0, 1, 33, 1000}) {
        auto m = squares(n);
        const auto copy = m;
        auto moved = std::move(m);
        EXPECT_EQ(m, copy) << n;
        EXPECT_EQ(moved, copy) << n;
        EXPECT_EQ(m.size(), size_t(n));
        EXPECT_EQ(size_t(std::distance(m.begin(), m.end())), size_t(n));
        EXPECT_EQ(m.insert(-1, 1).size(), size_t(n + 1));
        EXPECT_EQ(m.contains(0), n > 0);
        EXPECT_EQ(m.erase(0).size(), size_t(n > 0 ? n - 1 : 0));
        sgcl::immutable::map<int, int> w;
        w = std::move(m);
        EXPECT_EQ(m, copy) << n;
        EXPECT_EQ(w, copy) << n;
        auto& alias = m;   // through a reference: the compiler's warning would see `m = m`
        m = alias;
        EXPECT_EQ(m, copy) << n;
        m = std::move(alias);
        EXPECT_EQ(m, copy) << n;
        sgcl::immutable::set<int> s;
        for (int i : range(n)) {
            s = s.insert(i);
        }
        const auto scopy = s;
        auto smoved = std::move(s);
        EXPECT_EQ(s, scopy) << n;
        EXPECT_EQ(smoved, scopy) << n;
        EXPECT_EQ(s.insert(-1).size(), size_t(n + 1));
        auto& salias = s;
        s = salias;
        s = std::move(salias);
        EXPECT_EQ(s, scopy) << n;
    }
}

// The edges of an empty map and set and of ones of one element: the
// iterators meet and equal a default one (end.md), every lookup misses,
// an erase gives the same container; one element erased leaves a
// container that holds no node (empty.md)
TEST(ImMap_Tests, EmptyAndOneElement) {
    sgcl::immutable::map<int, int> e;
    EXPECT_EQ(e.cbegin(), e.cend());
    EXPECT_EQ(e.end(), (sgcl::immutable::map<int, int>::const_iterator()));
    EXPECT_EQ(e.find(0), e.end());
    EXPECT_EQ(e.count(0), 0u);
    EXPECT_EQ(e, (sgcl::immutable::map<int, int>({})));
    sgcl::immutable::set<int> es;
    EXPECT_EQ(es.find(0), es.end());
    EXPECT_EQ(es.count(0), 0u);
    EXPECT_EQ(es.erase(0), es);
    EXPECT_EQ(es, sgcl::immutable::set<int>({}));
    EXPECT_TRUE(e.thaw().freeze().empty());
    auto b = e.thaw();
    EXPECT_FALSE(b.erase(0));
    EXPECT_EQ(b.try_get(0), nullptr);
    EXPECT_FALSE(b.contains(0));
    EXPECT_TRUE(b.freeze().empty());
    const size_t before = collector::get_live_object_count();
    off_frame([&] {
        auto one = e.insert(0, 0);
        EXPECT_EQ(one.size(), 1u);
        EXPECT_EQ(one.begin()->first, 0);
        EXPECT_EQ(std::next(one.begin()), one.end());
        EXPECT_EQ(one.find(0), one.begin());
        EXPECT_EQ(one.find(1), one.end());
        auto none = one.erase(0);
        EXPECT_TRUE(none.empty());
        EXPECT_EQ(none.begin(), none.end());
        EXPECT_EQ(none, e);
        auto s = es.insert(0).erase(0);
        EXPECT_TRUE(s.empty());
        auto ob = one.thaw();
        EXPECT_TRUE(ob.erase(0));
        EXPECT_TRUE(ob.empty());
        auto frozen = ob.freeze();
        EXPECT_TRUE(frozen.empty());
        EXPECT_EQ(frozen.begin(), frozen.end());
        EXPECT_TRUE(ob.insert(1, 1));   // the builder goes on from nothing
        EXPECT_EQ(ob.freeze().size(), 1u);
    });
    settle();
    EXPECT_EQ(collector::get_live_object_count(), before);   // the nodes of the frame collected, none for an empty container
}

namespace {
    // A hash and an equality with a state of their own, which the
    // containers keep and hand out
    struct Seeded {
        size_t seed = 0;

        size_t operator()(int k) const noexcept {
            return std::hash<int>()(k) * 0x9e3779b97f4a7c15ull ^ seed;
        }
    };

    struct Tagged {
        int tag = 0;

        bool operator()(int a, int b) const noexcept {
            return a == b;
        }
    };
}

// The hash and the equality given to a constructor are the ones every
// version made from the container keeps (hash_function.md, key_eq.md),
// through a change, a copy, a builder and its freeze; a builder moved
// from keeps them too (map-builder.md (3), operator_assign.md (1))
TEST(ImMap_Tests, StatefulHashAndEquality) {
    using M = sgcl::immutable::map<int, int, Seeded, Tagged>;
    using S = sgcl::immutable::set<int, Seeded, Tagged>;
    auto keeps = [](const auto& c, size_t seed, int tag) {
        EXPECT_EQ(c.hash_function().seed, seed);
        EXPECT_EQ(c.key_eq().tag, tag);
    };
    M m(Seeded{77}, Tagged{5});
    keeps(m, 77, 5);
    for (int i : range(200)) {
        m = m.insert(i, i);
    }
    keeps(m, 77, 5);
    keeps(m.set(1, 2).erase(3), 77, 5);
    keeps(M(m), 77, 5);
    keeps(m.thaw().freeze(), 77, 5);
    std::vector<std::pair<int, int>> items = {{1, 1}, {2, 2}};
    M ranged(items.begin(), items.end(), Seeded{9}, Tagged{3});
    keeps(ranged, 9, 3);
    EXPECT_EQ(*ranged.try_get(2), 2);
    M::builder b(Seeded{11}, Tagged{4});
    b.insert(1, 1);
    auto taken = std::move(b);
    keeps(b.freeze(), 11, 4);
    EXPECT_TRUE(b.insert(2, 2));
    EXPECT_EQ(*b.try_get(2), 2);
    M::builder other(Seeded{12}, Tagged{6});
    other = std::move(taken);
    keeps(other.freeze(), 11, 4);
    keeps(taken.freeze(), 11, 4);
    auto& alias = other;
    other = std::move(alias);   // to itself: nothing
    EXPECT_EQ(other.size(), 1u);
    keeps(other.freeze(), 11, 4);
    S s(Seeded{21}, Tagged{8});
    keeps(s.insert(1).insert(2).erase(1), 21, 8);
    S::builder sb(Seeded{22}, Tagged{9});
    sb.insert(1);
    auto st = std::move(sb);
    keeps(sb.freeze(), 22, 9);
    keeps(st.freeze(), 22, 9);
    EXPECT_TRUE(st.freeze().contains(1));
}

// An argument that is an element of the map itself: the version called
// on holds it while the new one is made, a builder makes what it adds
// before it touches the node the argument lies in. Strings (a key whose
// copy can throw: a builder copies the node it changes) and ints with
// strings (a pair whose move cannot: it moves the elements of its own
// node along)
TEST(ImMap_Tests, ArgumentsFromTheMapItself) {
    auto key = [](int i) {
        return std::string(20, char('k' + i % 8)) + std::to_string(i);
    };
    sgcl::immutable::map<std::string, std::string> m;
    for (int i : range(300)) {
        m = m.insert(key(i), key(i + 1));   // a value is the next key
    }
    EXPECT_EQ(m.set(key(5), m.at(key(5))), m);
    EXPECT_EQ(m.insert(key(1000), m.at(key(7))).at(key(1000)), key(8));
    EXPECT_EQ(m.set(key(7), m.at(key(9))).at(key(7)), key(10));
    EXPECT_EQ(m.emplace(key(1001), m.at(key(3))).at(key(1001)), key(4));
    auto first = m.begin()->first;
    auto erased = m.erase(m.begin()->first);
    EXPECT_FALSE(erased.contains(first));
    EXPECT_EQ(erased.size(), 299u);
    EXPECT_EQ(m.erase(m.at(key(3))).size(), 299u);   // the value names key 4
    EXPECT_EQ(m.set(m.begin()->first, m.begin()->second), m);
    EXPECT_EQ((sgcl::immutable::map<std::string, std::string>(m.begin(), m.end())), m);
    auto w = m;
    w = w.set(key(9), w.at(key(10)));   // the variable's own version read, then replaced
    EXPECT_EQ(w.at(key(9)), key(11));
    auto copies = m.thaw();
    EXPECT_FALSE(copies.set(key(5), *copies.try_get(key(6))));
    EXPECT_EQ(*copies.try_get(key(5)), key(7));
    EXPECT_TRUE(copies.insert(key(2000), *copies.try_get(key(7))));
    EXPECT_EQ(*copies.try_get(key(2000)), key(8));
    EXPECT_TRUE(copies.erase(*copies.try_get(key(3))));   // key 4, named by a value in the builder's node
    EXPECT_FALSE(copies.contains(key(4)));
    sgcl::immutable::map<std::string, std::string> self;
    for (int i : range(100)) {
        self = self.insert(key(i), key(i));   // a value is its own key
    }
    auto sb = self.thaw();
    EXPECT_TRUE(sb.erase(*sb.try_get(key(3))));   // the key read out of the element erased
    EXPECT_FALSE(sb.contains(key(3)));
    EXPECT_EQ(sb.size(), 99u);
    sgcl::immutable::map<int, std::string> moves;
    for (int i : range(300)) {
        moves = moves.insert(i, key(i));
    }
    auto mb = moves.thaw();
    for (int i : range(300)) {   // into nodes the builder owns: each add moves the elements after its slot along
        ASSERT_TRUE(mb.insert(1000 + i, *mb.try_get(i))) << i;
        ASSERT_FALSE(mb.set(i, *mb.try_get(1000 + i))) << i;
        ASSERT_FALSE(mb.set(i, *mb.try_get(i))) << i;
    }
    auto frozen = mb.freeze();
    for (int i : range(300)) {
        ASSERT_EQ(frozen.at(i), key(i));
        ASSERT_EQ(frozen.at(1000 + i), key(i));
    }
    sgcl::immutable::set<std::string> s;
    for (int i : range(100)) {
        s = s.insert(key(i));
    }
    EXPECT_EQ(s.insert(*s.begin()), s);
    EXPECT_EQ(s.erase(*s.begin()).size(), 99u);
    auto keys = s.thaw();
    auto snapshot = keys.freeze();
    EXPECT_FALSE(keys.insert(*snapshot.begin()));
    EXPECT_TRUE(keys.erase(*snapshot.begin()));
    EXPECT_EQ(keys.size(), 99u);
    EXPECT_EQ(snapshot, s);
}

namespace {
    // Hashes at the ends of a size_t: keys apart in the top four bits
    // alone (twelve levels of one-slot subtries, the keys parting in the
    // thirteenth, a level of four bits), and chains of 16 over it; apart
    // in the highest bit alone; 0, the largest and their neighbours
    struct TopBits {
        size_t operator()(int k) const noexcept {
            return size_t(k % 16) << 60;
        }
    };

    struct HighestBit {
        size_t operator()(int k) const noexcept {
            return size_t(k & 1) << 63;
        }
    };

    struct Extremes {
        size_t operator()(int k) const noexcept {
            static constexpr size_t Hashes[] = {0, SIZE_MAX, SIZE_MAX - 1, SIZE_MAX >> 1, size_t(1) << 63, 1, SIZE_MAX - 31, 31};
            return Hashes[k % 8];
        }
    };

    // Every operation over keys 0..n-1 under the hash, against an oracle:
    // the inserts one version at a time, the lookups and find's walk to
    // the end, the build from a range, the erases in a shuffled order, a
    // builder's inserts and erases, a set's
    template<class H>
    void shapes_against_oracle(int n) {
        sgcl::immutable::map<int, int, H> m;
        std::unordered_map<int, int> o;
        for (int i : range(n)) {
            m = m.insert(i, -i);
            o[i] = -i;
            ASSERT_TRUE(same_map(m, o)) << i;
        }
        std::vector<int> walk;
        for (auto& [k, v] : m) {
            walk.push_back(k);
        }
        for (size_t i = 0; i < walk.size(); ++i) {
            size_t rest = 0;
            for (auto it = m.find(walk[i]); it != m.end(); ++it) {
                ASSERT_EQ(it->first, walk[i + rest]);
                ++rest;
            }
            ASSERT_EQ(rest, walk.size() - i);
        }
        EXPECT_EQ(m.find(n), m.end());
        std::vector<std::pair<int, int>> items(o.begin(), o.end());
        sgcl::immutable::map<int, int, H> built(items.begin(), items.end());
        EXPECT_EQ(built, m);
        EXPECT_TRUE(same_map(built, o));
        auto b = built.thaw();
        for (int i : range(n)) {
            EXPECT_FALSE(b.set(i, i)) << i;
            EXPECT_TRUE(b.insert(n + i, i)) << i;
        }
        std::mt19937 rng{unsigned(n)};
        std::vector<int> order(walk);
        std::shuffle(order.begin(), order.end(), rng);
        for (int k : order) {
            m = m.erase(k);
            o.erase(k);
            ASSERT_TRUE(same_map(m, o)) << k;
            EXPECT_TRUE(b.erase(k)) << k;
        }
        EXPECT_TRUE(m.empty());
        auto rest = b.freeze();
        EXPECT_EQ(rest.size(), size_t(n));
        for (int i : range(n)) {
            ASSERT_NE(rest.try_get(n + i), nullptr) << i;
            EXPECT_EQ(*rest.try_get(n + i), i);
        }
        for (int i : range(n)) {
            EXPECT_TRUE(b.erase(n + i)) << i;
        }
        EXPECT_TRUE(b.freeze().empty());
        std::vector<int> keys(walk);
        sgcl::immutable::set<int, H> s(keys.begin(), keys.end());
        sgcl::immutable::set<int, H> by_inserts;
        for (int k : keys) {
            by_inserts = by_inserts.insert(k);
        }
        EXPECT_EQ(s, by_inserts);
        EXPECT_TRUE(same_set(s, std::unordered_set<int>(keys.begin(), keys.end())));
        for (int k : order) {
            s = s.erase(k);
        }
        EXPECT_TRUE(s.empty());
    }
}

// The hashes at the limits of a size_t: the trie's last level, of four
// bits, reached and split; chains at the bottom of it; 0 and SIZE_MAX
TEST(ImMap_Tests, HashesAtTheLimits) {
    shapes_against_oracle<TopBits>(48);
    shapes_against_oracle<HighestBit>(20);
    shapes_against_oracle<Extremes>(40);
}

// The lookups and the erase of a builder by a key of another type (a
// string_view for a string key) build no key, as a map's do
TEST(ImBuilder_Tests, TransparentLookup) {
    sgcl::immutable::map<sgcl::string, int> ages;
    ages = ages.insert("alice", 30).insert("bob", 32);
    auto b = ages.thaw();
    std::string_view alice("alice");
    EXPECT_TRUE(b.contains(alice));
    EXPECT_EQ(*b.try_get(alice), 30);
    EXPECT_EQ(b.try_get(std::string_view("carol")), nullptr);
    EXPECT_TRUE(b.erase(std::string_view("bob")));
    EXPECT_FALSE(b.erase(std::string_view("bob")));
    EXPECT_EQ(b.size(), 1u);
    sgcl::immutable::set<sgcl::string> names = {"alice", "bob"};
    auto sb = names.thaw();
    EXPECT_TRUE(sb.contains(alice));
    EXPECT_TRUE(sb.erase(alice));
    EXPECT_FALSE(sb.contains("alice"));
    EXPECT_EQ(names.size(), 2u);
}

// Keys and values without a default constructor: an entry constructs its
// element from the arguments, in an insert, a builder and a build
TEST(ImMap_Tests, ElementsWithoutADefaultConstructor) {
    struct Named {
        explicit Named(int i)
        : name(std::to_string(i)) {
        }

        std::string name;
    };
    static_assert(!std::is_default_constructible_v<Named>);
    sgcl::immutable::map<int, Named> m;
    for (int i : range(100)) {
        m = m.emplace(i, i);
    }
    m = m.set(5, Named(-5)).erase(6);
    EXPECT_EQ(m.size(), 99u);
    EXPECT_EQ(m.at(5).name, "-5");
    auto b = m.thaw();
    EXPECT_TRUE(b.emplace(200, 7));
    EXPECT_EQ(b.freeze().at(200).name, "7");
    std::vector<std::pair<int, Named>> items = {{1, Named(1)}, {2, Named(2)}};
    EXPECT_EQ((sgcl::immutable::map<int, Named>(items.begin(), items.end()).at(2).name), "2");
}

// The comparison of two sets cannot throw: the hash and the equality are
// noexcept and nothing else is called (set/operator_cmp.md: None)
TEST(ImSet_Tests, ComparisonIsNoexcept) {
    sgcl::immutable::set<int> a = {1, 2}, b = {2, 1};
    static_assert(noexcept(a == b) && noexcept(a != b));
    static_assert(noexcept(sgcl::immutable::set<std::string>() == sgcl::immutable::set<std::string>()));
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

// update: f of the value under a key, set in its place, f called once; an
// absent key the same map, or f(fallback) put under it; at every size of
// the trie and through collisions; f that reads the map itself, a key that
// is the map's own; f that throws leaves the map and gives nothing; a map
// moved from; noexcept as f and the elements are
TEST(ImMap_Tests, Update) {
    for (int n : {0, 1, 33, 1000}) {
        auto m = squares(n);
        const auto copy = m;
        for (int k : {0, n / 2, n - 1}) {
            if (k < 0 || k >= n) {
                continue;
            }
            int calls = 0;
            auto u = m.update(k, [&](int v) { ++calls; return v + 1; });
            EXPECT_EQ(calls, 1);
            EXPECT_EQ(u.size(), size_t(n));
            EXPECT_EQ(u.at(k), k * k + 1);
            EXPECT_EQ(u.erase(k), m.erase(k));
            auto f = m.update(k, -5, [](int v) { return v * 2; });
            EXPECT_EQ(f.at(k), 2 * k * k);
        }
        int calls = 0;
        auto absent = m.update(-1, [&](int v) { ++calls; return v; });
        EXPECT_EQ(calls, 0);
        EXPECT_EQ(absent, m);
        auto added = m.update(-1, 10, [&](int v) { ++calls; return v + 1; });
        EXPECT_EQ(calls, 1);
        EXPECT_EQ(added.size(), size_t(n + 1));
        EXPECT_EQ(added.at(-1), 11);
        EXPECT_EQ(m, copy);
        auto moved = std::move(m);
        EXPECT_EQ(m.update(-1, 0, [](int v) { return v; }).size(), size_t(n + 1));
        if (n > 0) {
            // f reads the map it updates; the key is a reference into it
            const int& own = m.begin()->first;
            auto r = m.update(own, [&](int v) { return v + int(m.size()) + m.at(own); });
            EXPECT_EQ(r.at(own), 2 * m.at(own) + n);
            EXPECT_EQ(m, copy);
        }
    }
    sgcl::immutable::map<int, int, CollidingHash> c;
    for (int i : range(2000)) {
        c = c.insert(i, i);
    }
    for (int i : range(2000)) {
        c = c.update(i, [](int v) { return -v; });
    }
    for (int i : range(2000)) {
        EXPECT_EQ(c.at(i), -i);
    }
    auto m = squares(10);
    EXPECT_THROW((void)m.update(3, [](int) -> int { throw std::runtime_error("f"); }), std::runtime_error);
    EXPECT_THROW((void)m.update(30, 0, [](int) -> int { throw std::runtime_error("f"); }), std::runtime_error);
    EXPECT_EQ(m, squares(10));
    // a value of another type than T, converted
    sgcl::immutable::map<std::string, std::string> names{{"a", "x"}};
    auto longer = names.update("a", [](const std::string& v) { return v + "y"; });
    EXPECT_EQ(longer.at("a"), "xy");
    auto nothrow = [](int v) noexcept { return v; };
    auto may_throw = [](int v) { return v; };
    static_assert(noexcept(m.update(1, nothrow)));
    static_assert(noexcept(m.update(1, 0, nothrow)));
    static_assert(!noexcept(m.update(1, may_throw)));
    static_assert(!noexcept(names.update("a", [](const std::string& v) noexcept { return v; })));   // a string's copy may throw
}

// The slot of a node a dropped version left, taken again by a node of a new
// version, holds null words where the node keeps pointers (the links and the
// elements' tracked words): the collector may read the slot from its
// allocation on (maker.h: _init). A node's destructor destroys its built
// entries, an entry moved or erased in place by a builder is destroyed
// where it was (hamt.h: _relocate, _remove_in_place), and the entries past
// `built` were never constructed.
namespace {
    using HamtMap = sgcl::immutable::map<int, tracked_ptr<int>>;
    using HamtValue = HamtMap::value_type;

    template<unsigned N>
    slot_probe::Probed probe_hamt_nodes(size_t n, const slot_probe::PageSet& pages) {
        using Node = sgcl::immutable::detail::HamtNode<HamtValue, N>;
        slot_probe::Shape<Node> shape;
        std::vector<size_t> words;
        for (unsigned i = 0; i < N; ++i) {
            auto entry = shape.offsets({&shape->entries[i].link, &shape->entries[i].value.second});
            words.insert(words.end(), entry.begin(), entry.end());
        }
        return slot_probe::probe_slots<Node>(n, pages, words);
    }
}

TEST(ImMap_Tests, ANodeOfADroppedVersionReusedHoldsNullPointers) {
    tracked_ptr<int> target = make_tracked<int>(1);
    sgcl::vector<HamtMap> kept;
    slot_probe::PageSet pages;
    constexpr int Maps = 2000;
    constexpr int Elements = 48;
    off_frame([&] {
        for (int m = 0; m < Maps; ++m) {
            HamtMap v;
            for (int k = 0; k < Elements; ++k) {
                v = v.set(m * Elements + k, target);   // the previous version's path dies
            }
            auto b = v.thaw();
            for (int k = 0; k < Elements; k += 3) {
                b.erase(m * Elements + k);              // in place, in the builder's own nodes
            }
            b.insert(m * Elements + Elements, target);
            v = b.freeze();
            for (auto& e : v) {
                pages.add(&e);
            }
            if (m % 8 == 0) {
                kept.push_back(v);                     // a few live nodes on every page
            }
        }
    });
    heap_count::settle();
    const size_t n = 8 * config::page_size / sizeof(sgcl::immutable::detail::HamtNode<HamtValue, 1>);
    std::pair<unsigned, slot_probe::Probed> probes[] = {
        {1, probe_hamt_nodes<1>(n, pages)}, {2, probe_hamt_nodes<2>(n, pages)}, {4, probe_hamt_nodes<4>(n, pages)},
        {8, probe_hamt_nodes<8>(n, pages)}, {16, probe_hamt_nodes<16>(n, pages)}, {32, probe_hamt_nodes<32>(n, pages)}};
    size_t reused = 0;
    for (auto& [capacity, r] : probes) {
        reused += r.reused;
        EXPECT_EQ(r.nonzero, 0u) << "nodes of " << capacity << " entries: of " << r.reused << " reused slots";
    }
    EXPECT_GT(reused, 0u);
    EXPECT_EQ(kept.size(), size_t(Maps / 8));
}
