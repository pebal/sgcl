//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <atomic>
#include <map>
#include <random>
#include <set>
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
