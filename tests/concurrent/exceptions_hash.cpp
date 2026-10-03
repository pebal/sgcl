//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Exception safety of concurrent::map and concurrent::set: every inserting
// operation run with a throw at each of its points in turn (the
// element's construction, copy and move: tests/throwing.h; the erasures
// throw nothing), and after each run the container
// checked against what the operation's page says in its Exceptions: the
// size the elements walked, none twice, none lost, none moved from, the
// argument as it was.
#include "tests/throwing.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

using throwing::Val;

namespace {
    // A hash of the residue mod 8: the keys of one residue share a hash
    // and a bucket, a chain the equality walks
    using Hash8 = throwing::Hash<8>;
    using Map = sgcl::concurrent::map<Val, Val, Hash8>;
    using Set = sgcl::concurrent::set<Val, Hash8>;

    // The keys before every operation: sixteen in the map's sixteen
    // buckets, so that the next insertion looks at the growth. The evens
    // fill the buckets 0, 2, 4 and 6; 3, 11 and 19 share the hash 3, in
    // that order on the list. A new key 27 joins their chain (the
    // equality compares it with the three), a new key 7 opens bucket 7
    // (its dummy allocated). A value is its key times ten.
    const std::vector<int> Keys = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 3, 11, 19};

    std::vector<int> sorted(std::vector<int> v) {
        std::sort(v.begin(), v.end());
        return v;
    }

    std::vector<int> with(std::vector<int> v, std::initializer_list<int> more) {
        v.insert(v.end(), more);
        return sorted(v);
    }

    std::vector<int> without(std::vector<int> v, int key) {
        v.erase(std::remove(v.begin(), v.end(), key), v.end());
        return sorted(v);
    }

    // The keys the container walks, checked whole: size() the count
    // walked, no key twice, nothing moved from, every value its key times
    // ten, every key found by a lookup
    template<class C>
    std::vector<int> walked(C& c) {
        std::vector<int> keys;
        for (const auto& e : c) {
            if constexpr (requires { e.first; }) {
                keys.push_back(e.first.v);
                EXPECT_EQ(e.second.v, e.first.v * 10) << "the value of key " << e.first.v;
            } else {
                keys.push_back(e.v);
            }
        }
        EXPECT_EQ(c.size(), keys.size()) << "size() against the elements walked";
        keys = sorted(keys);
        EXPECT_EQ(std::adjacent_find(keys.begin(), keys.end()), keys.end()) << "a key walked twice";
        EXPECT_EQ(std::count(keys.begin(), keys.end(), Val::Moved), 0) << "an element moved from";
        for (int k : keys) {
            EXPECT_TRUE(c.contains(Val(k))) << "key " << k << " walked but not found";
        }
        return keys;
    }

    // The map of Keys and the arguments of the operation under test, made
    // before the countdown is set: the argument's key and value, and `at`
    // an iterator on key 19
    struct MapState {
        MapState(int key, int value)
        : element(Val(key), Val(value))
        , other(Val(key), Val(value))
        , key(key) {
            for (int k : Keys) {
                m.try_emplace(Val(k), Val(k * 10));
            }
            at = m.find(Val(19));
        }

        Map m;
        std::pair<const Val, Val> element;
        std::pair<Val, Val> other;
        Val key;
        Map::iterator at;
        std::vector<std::pair<const Val, Val>> range;
    };

    tracked_ptr<MapState> map_state(int key = 19, int value = 190) {
        return make_tracked<MapState>(key, value);
    }

    struct SetState {
        explicit SetState(int key)
        : key(key) {
            for (int k : Keys) {
                s.insert(Val(k));
            }
            at = s.find(Val(19));
        }

        Set s;
        Val key;
        Set::iterator at;
        std::vector<Val> range;
    };

    tracked_ptr<SetState> set_state(int key = 19) {
        return make_tracked<SetState>(key);
    }

    // An insertion of one new key: after a throw the container holds the
    // keys it held (nothing linked), after a run without one it holds the
    // key too
    template<class State>
    void check_insertion(State& s, auto& c, bool threw, bool fired, int key) {
        if (threw) {
            EXPECT_EQ(walked(c), sorted(Keys));
        } else {
            EXPECT_EQ(walked(c), with(Keys, {key}));
        }
        EXPECT_TRUE(c.bucket_count() == 16 || c.bucket_count() == 32);
        EXPECT_TRUE(threw || !fired) << "a countdown fired and was swallowed";
    }
}

// insert(const value_type&), insert(value_type&&), insert(P&&) of a pair
// of the key type, emplace and try_emplace (both overloads) of a new key,
// one in a chain of the same hash and one in a fresh bucket: a throw
// links nothing, and the argument stays as it was (the throwing copy or
// move of Val changes nothing before it throws)
TEST(ExceptionsMap_Tests, InsertionOfANewKey) {
    for (int key : {27, 7}) {
        const std::string at = " of key " + std::to_string(key);
        auto setup = [&] {
            return map_state(key, key * 10);
        };
        auto check = [&](MapState& s, bool threw, bool fired) {
            check_insertion(s, s.m, threw, fired, key);
        };
        int points = 0;
        points += throwing::each_kind("map insert(const value_type&)" + at, setup, [](MapState& s) {
            EXPECT_TRUE(s.m.insert(std::as_const(s.element)).second);
        }, [&](MapState& s, bool threw, bool fired) {
            check(s, threw, fired);
            EXPECT_EQ(s.element.first.v, key);
            EXPECT_EQ(s.element.second.v, key * 10);
        });
        points += throwing::each_kind("map insert(value_type&&)" + at, setup, [](MapState& s) {
            EXPECT_TRUE(s.m.insert(std::move(s.element)).second);
        }, [&](MapState& s, bool threw, bool fired) {
            check(s, threw, fired);
            if (threw) {
                EXPECT_EQ(s.element.second.v, key * 10) << "the argument moved from by a throwing insertion";
            }
        });
        // The node's pair is built from the pair<Val, Val>&& member by
        // member (std::pair's converting move): a second move that throws
        // leaves the first moved from. The page promises the map, not the
        // argument; the map is checked
        points += throwing::each_kind("map insert(P&&)" + at, setup, [](MapState& s) {
            EXPECT_TRUE(s.m.insert(std::move(s.other)).second);
        }, check);
        points += throwing::each_kind("map emplace(int, int)" + at, setup, [key](MapState& s) {
            EXPECT_TRUE(s.m.emplace(key, key * 10).second);
        }, check);
        points += throwing::each_kind("map try_emplace(const Key&, int)" + at, setup, [key](MapState& s) {
            EXPECT_TRUE(s.m.try_emplace(std::as_const(s.key), key * 10).second);
        }, [&](MapState& s, bool threw, bool fired) {
            check(s, threw, fired);
            EXPECT_EQ(s.key.v, key);
        });
        points += throwing::each_kind("map try_emplace(Key&&, int)" + at, setup, [key](MapState& s) {
            EXPECT_TRUE(s.m.try_emplace(std::move(s.key), key * 10).second);
        }, check);
        EXPECT_GE(points, 12);
    }
}

// An insertion of a key the map holds: nothing changes whatever throws,
// and the argument is left as it was, thrown or not (the search comes
// first: insert.md, try_emplace.md); emplace builds its node first and
// drops it
TEST(ExceptionsMap_Tests, InsertionOfATakenKey) {
    const int key = 11;
    auto setup = [&] {
        return map_state(key, 5);
    };
    auto check = [](MapState& s, bool, bool) {
        EXPECT_EQ(walked(s.m), sorted(Keys));
        EXPECT_EQ(s.m.bucket_count(), 16u);
    };
    auto intact = [&](MapState& s, bool threw, bool fired) {
        check(s, threw, fired);
        EXPECT_EQ(s.element.first.v, key);
        EXPECT_EQ(s.element.second.v, 5);
        EXPECT_EQ(s.other.first.v, key);
        EXPECT_EQ(s.other.second.v, 5);
        EXPECT_EQ(s.key.v, key);
    };
    int points = 0;
    points += throwing::each_kind("map insert(const value_type&) taken", setup, [](MapState& s) {
        EXPECT_FALSE(s.m.insert(std::as_const(s.element)).second);
    }, intact);
    points += throwing::each_kind("map insert(value_type&&) taken", setup, [](MapState& s) {
        EXPECT_FALSE(s.m.insert(std::move(s.element)).second);
    }, intact);
    points += throwing::each_kind("map insert(P&&) taken", setup, [](MapState& s) {
        EXPECT_FALSE(s.m.insert(std::move(s.other)).second);
    }, intact);
    points += throwing::each_kind("map try_emplace(Key&&, int) taken", setup, [](MapState& s) {
        EXPECT_FALSE(s.m.try_emplace(std::move(s.key), 5).second);
    }, intact);
    points += throwing::each_kind("map emplace(int, int) taken", setup, [key](MapState& s) {
        EXPECT_FALSE(s.m.emplace(key, 5).second);
    }, check);
    EXPECT_GE(points, 2);
}

// insert(first, last) and insert(initializer_list): the elements inserted
// before the throw stay, the one that threw is not linked, the rest not
// reached (insert.md (4-5)); a taken key in the range is left as it was
TEST(ExceptionsMap_Tests, InsertionOfARange) {
    const std::vector<int> fresh = {27, 7, 35};   // in the order of the range; 11 among them taken
    auto setup = [&] {
        auto s = map_state();
        for (int k : {27, 11, 7, 35}) {
            s->range.emplace_back(Val(k), Val(k * 10));
        }
        return s;
    };
    auto prefix_check = [&](MapState& s, bool threw, bool) {
        auto keys = walked(s.m);
        if (!threw) {
            EXPECT_EQ(keys, with(Keys, {27, 7, 35}));
            return;
        }
        bool matched = false;   // the keys added: a prefix of the new keys of the range
        for (size_t j = 0; j <= fresh.size() && !matched; ++j) {
            auto expected = sorted(Keys);
            for (size_t i = 0; i < j; ++i) {
                expected.push_back(fresh[i]);
            }
            matched = keys == sorted(expected);
        }
        EXPECT_TRUE(matched) << "the keys inserted are not a prefix of the range";
    };
    int points = 0;
    points += throwing::each_kind("map insert(first, last)", setup, [](MapState& s) {
        s.m.insert(s.range.begin(), s.range.end());
    }, [&](MapState& s, bool threw, bool fired) {
        prefix_check(s, threw, fired);
        EXPECT_EQ(s.range[1].second.v, 110);   // 11: taken, read by copy
    });
    points += throwing::each_kind("map insert(move_iterator range)", setup, [](MapState& s) {
        s.m.insert(std::make_move_iterator(s.range.begin()), std::make_move_iterator(s.range.end()));
    }, [&](MapState& s, bool threw, bool fired) {
        prefix_check(s, threw, fired);
        EXPECT_EQ(s.range[1].second.v, 110) << "the taken key's element moved from";
    });
    points += throwing::each_kind("map insert(initializer_list)", [] { return map_state(); }, [](MapState& s) {
        s.m.insert({{Val(27), Val(270)}, {Val(11), Val(110)}, {Val(7), Val(70)}, {Val(35), Val(350)}});
    }, prefix_check);
    EXPECT_GT(points, 20);
}

// set: insert(const Key&), insert(Key&&) and emplace of a new key, in a
// chain and in a fresh bucket: a throw links nothing and leaves the
// argument as it was
TEST(ExceptionsSet_Tests, InsertionOfANewKey) {
    for (int key : {27, 7}) {
        const std::string at = " of key " + std::to_string(key);
        auto setup = [&] {
            return set_state(key);
        };
        auto check = [&](SetState& s, bool threw, bool fired) {
            check_insertion(s, s.s, threw, fired, key);
        };
        int points = 0;
        points += throwing::each_kind("set insert(const Key&)" + at, setup, [](SetState& s) {
            EXPECT_TRUE(s.s.insert(std::as_const(s.key)).second);
        }, [&](SetState& s, bool threw, bool fired) {
            check(s, threw, fired);
            EXPECT_EQ(s.key.v, key);
        });
        points += throwing::each_kind("set insert(Key&&)" + at, setup, [](SetState& s) {
            EXPECT_TRUE(s.s.insert(std::move(s.key)).second);
        }, [&](SetState& s, bool threw, bool fired) {
            check(s, threw, fired);
            if (threw) {
                EXPECT_EQ(s.key.v, key) << "the argument moved from by a throwing insertion";
            }
        });
        points += throwing::each_kind("set emplace(int)" + at, setup, [key](SetState& s) {
            EXPECT_TRUE(s.s.emplace(key).second);
        }, check);
        EXPECT_GE(points, 3);
    }
}

TEST(ExceptionsSet_Tests, InsertionOfATakenKey) {
    auto setup = [] {
        return set_state(11);
    };
    auto intact = [](SetState& s, bool, bool) {
        EXPECT_EQ(walked(s.s), sorted(Keys));
        EXPECT_EQ(s.key.v, 11);
    };
    int points = 0;
    points += throwing::each_kind("set insert(const Key&) taken", setup, [](SetState& s) {
        EXPECT_FALSE(s.s.insert(std::as_const(s.key)).second);
    }, intact);
    points += throwing::each_kind("set insert(Key&&) taken", setup, [](SetState& s) {
        EXPECT_FALSE(s.s.insert(std::move(s.key)).second);
    }, intact);
    points += throwing::each_kind("set emplace(int) taken", setup, [](SetState& s) {
        EXPECT_FALSE(s.s.emplace(11).second);
    }, intact);
    EXPECT_GE(points, 1);
}

TEST(ExceptionsSet_Tests, InsertionOfARange) {
    const std::vector<int> fresh = {27, 7, 35};
    auto setup = [] {
        auto s = set_state();
        for (int k : {27, 11, 7, 35}) {
            s->range.emplace_back(k);
        }
        return s;
    };
    auto prefix_check = [&](SetState& s, bool threw, bool) {
        auto keys = walked(s.s);
        if (!threw) {
            EXPECT_EQ(keys, with(Keys, {27, 7, 35}));
            return;
        }
        bool matched = false;
        for (size_t j = 0; j <= fresh.size() && !matched; ++j) {
            auto expected = sorted(Keys);
            for (size_t i = 0; i < j; ++i) {
                expected.push_back(fresh[i]);
            }
            matched = keys == sorted(expected);
        }
        EXPECT_TRUE(matched) << "the keys inserted are not a prefix of the range";
    };
    int points = 0;
    points += throwing::each_kind("set insert(first, last)", setup, [](SetState& s) {
        s.s.insert(s.range.begin(), s.range.end());
    }, prefix_check);
    points += throwing::each_kind("set insert(move_iterator range)", setup, [](SetState& s) {
        s.s.insert(std::make_move_iterator(s.range.begin()), std::make_move_iterator(s.range.end()));
    }, [&](SetState& s, bool threw, bool fired) {
        prefix_check(s, threw, fired);
        EXPECT_EQ(s.range[1].v, 11) << "the taken key moved from";
    });
    EXPECT_GE(points, 6);
}
