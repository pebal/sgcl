//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
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
