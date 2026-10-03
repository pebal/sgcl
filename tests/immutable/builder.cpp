//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
    // Hashes that shape the trie: every key its own path; a hash shared
    // by the keys of one residue, to the last bit (the chains); and one
    // equal in its first 35 bits for every key (a path of seven levels
    // of one-slot subtries before the keys part, so that a split and a
    // hoist happen deep down)
    struct Spread {
        size_t operator()(long k) const noexcept {
            return std::hash<long>()(k) * 0x9e3779b97f4a7c15ull;
        }
    };

    struct Chained {
        size_t operator()(long k) const noexcept {
            return size_t(k % 13);
        }
    };

    struct Deep {
        size_t operator()(long k) const noexcept {
            return size_t(k) << 35;
        }
    };

    template<class M, class O>
    bool same(const M& m, const O& o) {
        if (m.size() != o.size()) {
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

    // Random inserts, replacements and erases through one builder, a
    // snapshot frozen every few hundred steps and kept with a copy of the
    // oracle, a builder thawed from an older snapshot going its own way
    // now and then: at the end every snapshot still holds what it held
    template<class Hash>
    void against_oracle(unsigned seed, long keys, int steps) {
        std::mt19937 rng(seed);
        using M = sgcl::immutable::map<long, long, Hash>;
        auto b = M().thaw();
        std::unordered_map<long, long> o;
        M snapshots[12];                          // on the stack: no tracked pointer in a std container
        std::vector<std::unordered_map<long, long>> oracles;
        for (int step = 0; step < steps; ++step) {
            long k = long(rng() % keys);
            switch (rng() % 5) {
                case 0: case 1: case 2: {
                    long v = long(rng());
                    EXPECT_EQ(b.set(k, v), !o.count(k)) << step;   // a replacement: set
                    o[k] = v;
                    break;
                }
                default:
                    EXPECT_EQ(b.erase(k), o.erase(k) == 1) << step;
                    break;
            }
            if (step % (steps / 12) == steps / 12 - 1 && oracles.size() < 12) {
                snapshots[oracles.size()] = b.freeze();
                oracles.push_back(o);
                ASSERT_TRUE(same(snapshots[oracles.size() - 1], o)) << step;
                if (oracles.size() % 4 == 0) {   // a builder from an older snapshot, changed and dropped
                    auto& older = snapshots[oracles.size() / 2];
                    auto other = older.thaw();
                    for (int i = 0; i < 200; ++i) {
                        long x = long(rng() % keys);
                        if (i % 3) {
                            other.insert(x, -x);
                        } else {
                            other.erase(x);
                        }
                    }
                    other.freeze();
                }
                collector::force_collect();
            }
            if (step % 257 == 0) {
                ASSERT_EQ(b.size(), o.size()) << step;
            }
        }
        EXPECT_TRUE(same(b.freeze(), o));
        for (size_t i = 0; i < oracles.size(); ++i) {
            EXPECT_TRUE(same(snapshots[i], oracles[i])) << i;
        }
    }
}

TEST(ImBuilder_Tests, ThawAndFreeze) {
    sgcl::immutable::map<int, int> source{{1, 10}, {2, 20}};
    auto b = source.thaw();
    EXPECT_EQ(b.size(), 2u);
    EXPECT_FALSE(b.insert(1, 12));   // kept: an insert keeps what it finds
    EXPECT_EQ(*b.try_get(1), 10);
    EXPECT_FALSE(b.set(1, 11));      // replaced
    EXPECT_TRUE(b.insert(3, 30));    // added
    EXPECT_TRUE(b.erase(2));
    EXPECT_FALSE(b.erase(2));
    EXPECT_EQ(*b.try_get(1), 11);
    EXPECT_FALSE(b.contains(2));
    auto first = b.freeze();
    EXPECT_EQ(first, (sgcl::immutable::map<int, int>{{1, 11}, {3, 30}}));
    EXPECT_EQ(source, (sgcl::immutable::map<int, int>{{1, 10}, {2, 20}}));   // the source untouched
    b.insert(4, 40);                 // the builder goes on after a freeze
    b.erase(1);
    auto second = b.freeze();
    EXPECT_EQ(first, (sgcl::immutable::map<int, int>{{1, 11}, {3, 30}}));    // the snapshot untouched
    EXPECT_EQ(second, (sgcl::immutable::map<int, int>{{3, 30}, {4, 40}}));
    auto moved = std::move(b);
    EXPECT_TRUE(b.empty());          // a builder moved from holds nothing
    EXPECT_EQ(moved.size(), 2u);
    b.insert(9, 90);
    EXPECT_EQ(b.freeze(), (sgcl::immutable::map<int, int>{{9, 90}}));
    EXPECT_EQ(moved.freeze(), second);
    while (!moved.empty()) {         // erased to empty and filled again
        moved.erase(moved.try_get(3) ? 3 : 4);
    }
    EXPECT_EQ(moved.freeze(), (sgcl::immutable::map<int, int>()));
    moved.emplace(5, 50);
    EXPECT_EQ(*moved.freeze().try_get(5), 50);
}

TEST(ImBuilder_Tests, AgainstOracle) {
    against_oracle<Spread>(1, 3000, 30000);
    against_oracle<std::hash<long>>(2, 100000, 30000);
}

TEST(ImBuilder_Tests, ChainsAgainstOracle) {
    against_oracle<Chained>(3, 300, 6000);
}

TEST(ImBuilder_Tests, DeepSubtriesAgainstOracle) {
    against_oracle<Deep>(4, 200, 8000);
}

// A key whose copy can throw (a std::string, const in the pair): no move
// that cannot throw, so the builder copies the node it changes rather
// than moving its elements; the same answers
TEST(ImBuilder_Tests, StringKeys) {
    std::mt19937 rng(5);
    auto b = sgcl::immutable::map<std::string, std::string>().thaw();
    std::map<std::string, std::string> o;
    for (int step = 0; step < 8000; ++step) {
        auto k = std::to_string(rng() % 1500);
        if (rng() % 3) {
            auto v = std::string(20, char('a' + step % 26));
            b.set(k, v);
            o[k] = v;
        } else {
            b.erase(k);
            o.erase(k);
        }
    }
    auto m = b.freeze();
    ASSERT_EQ(m.size(), o.size());
    for (auto& [k, v] : o) {
        ASSERT_TRUE(m.try_get(k)) << k;
        EXPECT_EQ(*m.try_get(k), v);
    }
}

// Values holding tracked pointers, changed in place while the collector
// runs: every value the map holds stays alive and whole
TEST(ImBuilder_Tests, TrackedValuesUnderTheCollector) {
    std::atomic<bool> stop = false;
    std::thread collecting([&] {
        while (!stop) {
            collector::force_collect(true);
        }
    });
    off_frame([&] {
        auto b = sgcl::immutable::map<int, tracked_ptr<Baz>>().thaw();
        sgcl::immutable::map<int, tracked_ptr<Baz>> kept;
        for (int round = 0; round < 40; ++round) {
            for (int i = 0; i < 2000; ++i) {
                int k = (i * 7 + round) % 3000;
                if ((i + round) % 4) {
                    b.insert(k, make_tracked<Baz>(k));
                } else {
                    b.erase(k);
                }
            }
            kept = b.freeze();
            for (auto& [k, v] : kept) {
                ASSERT_TRUE(v);
                ASSERT_EQ(v->value, k);
            }
        }
    });
    stop = true;
    collecting.join();
}

TEST(ImBuilder_Tests, Set) {
    std::mt19937 rng(6);
    auto b = sgcl::immutable::set<long, Spread>().thaw();
    std::set<long> o;
    sgcl::immutable::set<long, Spread> half;
    for (int step = 0; step < 20000; ++step) {
        long k = long(rng() % 2000);
        if (rng() % 3) {
            EXPECT_EQ(b.insert(k), o.insert(k).second);
        } else {
            EXPECT_EQ(b.erase(k), o.erase(k) == 1);
        }
        if (step == 10000) {
            half = b.freeze();
        }
    }
    auto all = b.freeze();
    ASSERT_EQ(all.size(), o.size());
    for (long k : o) {
        EXPECT_TRUE(all.contains(k)) << k;
    }
    EXPECT_NE(half, all);
}

namespace {
    // The copy that brings the countdown to zero throws; 0: none
    int copies_to_throw = 0;

    // An element whose copy throws on demand; its move cannot throw when
    // NothrowMove (the builder moves the elements of its own nodes) and
    // may otherwise (the builder copies a node it changes)
    template<bool NothrowMove>
    struct Fragile {
        int v = 0;

        Fragile(int x) noexcept
        : v(x) {
        }

        Fragile(const Fragile& o)
        : v(o.v) {
            if (copies_to_throw && --copies_to_throw == 0) {
                throw std::runtime_error("copy");
            }
        }

        Fragile(Fragile&& o) noexcept(NothrowMove)
        : v(o.v) {
        }

        Fragile& operator=(const Fragile&) = default;

        friend bool operator==(const Fragile& a, const Fragile& b) noexcept {
            return a.v == b.v;
        }
    };

    // The shapes the keys' hashes give the trie: the key itself (1, 33
    // and 65 under one slot of the root, apart below it); a path of seven
    // one-slot subtries before the keys part (an erase that leaves one
    // element hands it up through every level); one hash per residue of 4
    // (the chains)
    struct Low {
        size_t operator()(int k) const noexcept {
            return size_t(k);
        }
    };

    struct DeepInt {
        size_t operator()(int k) const noexcept {
            return size_t(k) << 35;
        }
    };

    struct Mod4 {
        size_t operator()(int k) const noexcept {
            return size_t(k % 4);
        }
    };

    template<class H>
    struct FragileHash {
        template<bool N>
        size_t operator()(const Fragile<N>& f) const noexcept {
            return H()(f.v);
        }
    };

    template<class C>
    size_t walked(const C& c) {
        size_t n = 0;
        for (auto it = c.begin(); it != c.end(); ++it) {
            ++n;
        }
        return n;
    }

    // An erase through a builder of `keys` with its n-th copy throwing,
    // for n = 1, 2, ... until the erase makes no copy that throws: after a
    // throw the builder is as it was before the call (its size the
    // elements it walks, every key there), after an erase without one the
    // key is gone; a container frozen before is untouched either way.
    // `shared`: the builder's nodes shared with that container (the erase
    // copies them), else its own (changed in place)
    template<class Key, class Make>
    void throwing_erase(Make make, std::initializer_list<int> keys, int erased, bool shared) {
        bool done = false;
        for (int n = 1; n < 200 && !done; ++n) {
            auto b = make();
            for (int k : keys) {
                b.insert(Key(k));
            }
            auto before = b.freeze();
            if (!shared) {
                b = make();
                for (int k : keys) {
                    b.insert(Key(k));
                }
            }
            bool threw = false;
            copies_to_throw = n;
            try {
                EXPECT_TRUE(b.erase(Key(erased))) << n;
            } catch (const std::runtime_error&) {
                threw = true;
            }
            copies_to_throw = 0;
            done = !threw;
            auto after = b.freeze();
            ASSERT_EQ(walked(after), after.size()) << "copy " << n << (threw ? " threw" : "");
            ASSERT_EQ(b.size(), threw ? keys.size() : keys.size() - 1) << n;
            for (int k : keys) {
                EXPECT_EQ(b.contains(Key(k)), threw || k != erased) << n << " key " << k;
            }
            EXPECT_EQ(before.size(), keys.size());
            EXPECT_EQ(walked(before), keys.size());
            for (int k : keys) {
                EXPECT_TRUE(before.contains(Key(k))) << n << " key " << k;
            }
        }
        EXPECT_TRUE(done);
    }

    // A map's builder with the set builder's insert: a key alone, its
    // value made of it
    template<class M>
    struct MapByKeys {
        typename M::builder b;

        void insert(int k) {
            b.insert(k, typename M::mapped_type(k * 10));
        }

        bool erase(int k) {
            return b.erase(k);
        }

        bool contains(int k) const {
            return b.contains(k);
        }

        size_t size() const {
            return b.size();
        }

        M freeze() {
            return b.freeze();
        }
    };

    template<class H, bool NothrowMove>
    void map_throwing_erase(std::initializer_list<int> keys, int erased) {
        using M = sgcl::immutable::map<int, Fragile<NothrowMove>, H>;
        for (bool shared : {false, true}) {
            throwing_erase<int>([] { return MapByKeys<M>(); }, keys, erased, shared);
        }
    }

    template<class H, bool NothrowMove>
    void set_throwing_erase(std::initializer_list<int> keys, int erased) {
        using S = sgcl::immutable::set<Fragile<NothrowMove>, FragileHash<H>>;
        for (bool shared : {false, true}) {
            throwing_erase<Fragile<NothrowMove>>([] { return S().thaw(); }, keys, erased, shared);
        }
    }
}

// An erase whose copy of an element throws leaves the builder as it was:
// never an element taken out of a subtrie whose last one then failed to
// come up, the size still counting it
TEST(ImBuilder_Tests, MapEraseThrowingCopy) {
    map_throwing_erase<Low, true>({1, 33, 2}, 33);       // one element left below: handed up a level
    map_throwing_erase<Low, false>({1, 33, 2}, 33);
    map_throwing_erase<Low, true>({1, 33, 65, 2}, 33);   // two left below: none handed up
    map_throwing_erase<DeepInt, true>({1, 2}, 2);        // handed up through every level
    map_throwing_erase<DeepInt, false>({1, 2}, 2);
    map_throwing_erase<Mod4, true>({1, 5, 9, 2}, 1);     // the chain's next takes the entry
    map_throwing_erase<Mod4, false>({1, 5, 9, 2}, 1);
    map_throwing_erase<Mod4, true>({1, 5, 9, 2}, 5);     // one out of the chain
}

TEST(ImBuilder_Tests, SetEraseThrowingCopy) {
    set_throwing_erase<Low, true>({1, 33, 2}, 33);
    set_throwing_erase<Low, false>({1, 33, 2}, 33);
    set_throwing_erase<Low, true>({1, 33, 65, 2}, 33);
    set_throwing_erase<DeepInt, true>({1, 2}, 2);
    set_throwing_erase<DeepInt, false>({1, 2}, 2);
    set_throwing_erase<Mod4, true>({1, 5, 9, 2}, 1);
    set_throwing_erase<Mod4, false>({1, 5, 9, 2}, 1);
    set_throwing_erase<Mod4, true>({1, 5, 9, 2}, 5);
}
