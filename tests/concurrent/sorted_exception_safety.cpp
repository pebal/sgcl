//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Exception safety of concurrent::sorted_map and concurrent::sorted_set:
// every inserting operation run with a throw at each of its points in
// turn (the element's construction, copy and move: tests/throwing.h; the
// comparison is noexcept and the erasures throw nothing), and after each
// run the container checked against what the operation's page says in
// its Exceptions: the size the elements walked, the keys in ascending
// order, none twice, none moved from, the argument as it was where the
// page says so, and the list still usable at every level (each key
// erased again).
#include "tests/throwing.h"

#include <algorithm>
#include <utility>
#include <vector>

using throwing::Val;

namespace {
    using Map = sgcl::concurrent::sorted_map<Val, Val>;
    using Set = sgcl::concurrent::sorted_set<Val>;

    // The keys before every operation: the evens 0 to 30. A new key 13
    // goes in the middle, 31 after the last, 1 after the first. A value
    // is its key times ten.
    const std::vector<int> Keys = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};

    std::vector<int> with(std::vector<int> v, std::initializer_list<int> more) {
        v.insert(v.end(), more);
        std::sort(v.begin(), v.end());
        return v;
    }

    std::vector<int> without(std::vector<int> v, int key) {
        v.erase(std::remove(v.begin(), v.end(), key), v.end());
        return v;
    }

    // The keys the container walks, checked whole: ascending (so none
    // twice), size() the count walked, nothing moved from, every value
    // its key times ten, every key found by find and lower_bound, and the
    // odd keys between them not found
    int key_of(const auto& e) {
        if constexpr (requires { e.first; }) {
            return e.first.v;
        } else {
            return e.v;
        }
    }

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
        EXPECT_EQ(c.empty(), keys.empty());
        EXPECT_TRUE(std::adjacent_find(keys.begin(), keys.end(), std::greater_equal<int>()) == keys.end())
            << "the keys walked are not strictly ascending";
        EXPECT_EQ(std::count(keys.begin(), keys.end(), Val::Moved), 0) << "an element moved from";
        for (int k : keys) {
            auto it = c.find(Val(k));
            EXPECT_TRUE(it != c.end()) << "key " << k << " walked but not found";
            auto lb = c.lower_bound(Val(k));
            EXPECT_TRUE(lb != c.end() && key_of(*lb) == k) << "lower_bound of key " << k;
        }
        for (int k = 1; k < 40; k += 2) {
            if (std::find(keys.begin(), keys.end(), k) == keys.end()) {
                EXPECT_FALSE(c.contains(Val(k))) << "key " << k << " found but not walked";
            }
        }
        return keys;
    }

    // The list usable at every level after the run: every key 0 to 40
    // inserted where absent, then each erased by its key (1 each, so that
    // a node left marked at an upper level by a failed erasure is erased
    // and unlinked whole), and the container empty
    template<class C>
    void usable(C& c) {
        for (int k = 0; k <= 40; ++k) {
            if constexpr (requires { c.try_emplace(Val(k), Val(k * 10)); }) {
                c.try_emplace(Val(k), Val(k * 10));
            } else {
                c.insert(Val(k));
            }
        }
        EXPECT_EQ(walked(c).size(), 41u);
        for (int k = 0; k <= 40; ++k) {
            EXPECT_EQ(c.erase(Val(k)), 1u) << "key " << k;
        }
        EXPECT_TRUE(c.empty());
        EXPECT_EQ(c.size(), 0u);
        EXPECT_TRUE(c.begin() == c.end());
    }

    // The map of Keys and the arguments of the operation under test, made
    // before the countdown is set: the element and a pair of the key
    // type for insert, the key for try_emplace, `at` an iterator on key 14
    // for erase(it)
    struct MapState {
        MapState(int key, int value)
        : element(Val(key), Val(value))
        , other(Val(key), Val(value))
        , key(key) {
            for (int k : Keys) {
                m.try_emplace(Val(k), Val(k * 10));
            }
            at = m.find(Val(14));
        }

        Map m;
        pair<const Val, Val> element;
        pair<Val, Val> other;
        Val key;
        Map::iterator at;
        std::vector<pair<const Val, Val>> range;
    };

    tracked_ptr<MapState> map_state(int key = 14, int value = 140) {
        return make_tracked<MapState>(key, value);
    }

    struct SetState {
        explicit SetState(int key)
        : key(key) {
            for (int k : Keys) {
                s.insert(Val(k));
            }
            at = s.find(Val(14));
        }

        Set s;
        Val key;
        Set::iterator at;
        std::vector<Val> range;
    };

    tracked_ptr<SetState> set_state(int key = 14) {
        return make_tracked<SetState>(key);
    }

    // An insertion of one new key (insert.md, emplace.md, try_emplace.md):
    // after a throw nothing is linked and the container is as it was;
    // after a run without one it holds the key too
    void check_insertion(auto& c, bool threw, int key) {
        EXPECT_EQ(walked(c), threw ? Keys : with(Keys, {key}));
        usable(c);
    }

    // What the walk of clear or of the erase(it) loop leaves: the keys
    // are erased in ascending order, so what stays is a suffix of Keys,
    // the whole of it when nothing was erased (clear.md: the elements the
    // walk had not erased yet stay)
    void check_suffix(const std::vector<int>& keys, bool threw) {
        if (!threw) {
            EXPECT_TRUE(keys.empty());
            return;
        }
        ASSERT_LE(keys.size(), Keys.size());
        EXPECT_TRUE(std::equal(keys.begin(), keys.end(), Keys.end() - ptrdiff_t(keys.size())))
            << "the keys left are not the ones the walk had not reached";
    }

    // The keys after a range insertion of `fresh` (in range order) into
    // Keys: all of them, or after a throw a prefix of them
    void check_prefix(const std::vector<int>& keys, bool threw, const std::vector<int>& fresh) {
        if (!threw) {
            auto all = Keys;
            for (int k : fresh) {
                all = with(all, {k});
            }
            EXPECT_EQ(keys, all);
            return;
        }
        bool matched = false;
        auto expected = Keys;
        for (size_t j = 0; j <= fresh.size() && !matched; ++j) {
            if (j > 0) {
                expected = with(expected, {fresh[j - 1]});
            }
            matched = keys == expected;
        }
        EXPECT_TRUE(matched) << "the keys inserted are not a prefix of the range";
    }
}

// insert(const value_type&), insert(value_type&&), insert(P&&) with a pair
// of the key type and with a pair of another type, emplace and
// try_emplace (both overloads) of a new key, at the front, in the middle
// and after the last: a throw links nothing and leaves the map as it was;
// the argument of (1) and (2) is left as it was (the copy or move of Val
// throws before it changes anything), and the key of
// try_emplace(const Key&) always
TEST(SortedExceptionsMap_Tests, InsertionOfANewKey) {
    for (int key : {1, 13, 31}) {
        const std::string at = " of key " + std::to_string(key);
        auto setup = [&] {
            return map_state(key, key * 10);
        };
        auto check = [&](MapState& s, bool threw, bool) {
            check_insertion(s.m, threw, key);
        };
        int points = 0;
        points += throwing::each_kind("sorted_map insert(const value_type&)" + at, setup, [](MapState& s) {
            EXPECT_TRUE(s.m.insert(std::as_const(s.element)).second);
        }, [&](MapState& s, bool threw, bool fired) {
            check(s, threw, fired);
            EXPECT_EQ(s.element.first.v, key);
            EXPECT_EQ(s.element.second.v, key * 10);
        });
        points += throwing::each_kind("sorted_map insert(value_type&&)" + at, setup, [](MapState& s) {
            EXPECT_TRUE(s.m.insert(std::move(s.element)).second);
        }, [&](MapState& s, bool threw, bool fired) {
            check(s, threw, fired);
            if (threw) {
                EXPECT_EQ(s.element.first.v, key) << "the argument changed by a throwing insertion";
                EXPECT_EQ(s.element.second.v, key * 10) << "the argument moved from by a throwing insertion";
            }
        });
        // A pair<Val, Val>&& is moved into the node member by member: a
        // second move that throws leaves the first moved from. The page
        // promises the map, not the argument; the map is checked
        points += throwing::each_kind("sorted_map insert(pair<Key, V>&&)" + at, setup, [](MapState& s) {
            EXPECT_TRUE(s.m.insert(std::move(s.other)).second);
        }, check);
        points += throwing::each_kind("sorted_map insert(pair<int, int>&&)" + at, setup, [key](MapState& s) {
            EXPECT_TRUE(s.m.insert(pair<int, int>(key, key * 10)).second);
        }, check);
        points += throwing::each_kind("sorted_map emplace(int, int)" + at, setup, [key](MapState& s) {
            EXPECT_TRUE(s.m.emplace(key, key * 10).second);
        }, check);
        points += throwing::each_kind("sorted_map try_emplace(const Key&, int)" + at, setup, [key](MapState& s) {
            EXPECT_TRUE(s.m.try_emplace(std::as_const(s.key), key * 10).second);
        }, [&](MapState& s, bool threw, bool fired) {
            check(s, threw, fired);
            EXPECT_EQ(s.key.v, key);
        });
        points += throwing::each_kind("sorted_map try_emplace(Key&&, int)" + at, setup, [key](MapState& s) {
            EXPECT_TRUE(s.m.try_emplace(std::move(s.key), key * 10).second);
        }, check);
        EXPECT_GE(points, 14);
    }
}

// An insertion of a key the map holds: nothing changes whatever throws;
// the argument of insert (1–3 with a first of the key type) and of
// try_emplace is left as it was, thrown or not (the search comes first:
// insert.md, try_emplace.md); emplace and insert of a pair of another
// type build a node first and drop it
TEST(SortedExceptionsMap_Tests, InsertionOfATakenKey) {
    const int key = 14;
    auto setup = [&] {
        return map_state(key, 5);
    };
    auto check = [](MapState& s, bool, bool) {
        EXPECT_EQ(walked(s.m), Keys);
        usable(s.m);
    };
    auto intact = [&](MapState& s, bool threw, bool fired) {
        EXPECT_EQ(s.element.first.v, key);
        EXPECT_EQ(s.element.second.v, 5);
        EXPECT_EQ(s.other.first.v, key);
        EXPECT_EQ(s.other.second.v, 5);
        EXPECT_EQ(s.key.v, key);
        check(s, threw, fired);
    };
    int points = 0;
    points += throwing::each_kind("sorted_map insert(const value_type&) taken", setup, [](MapState& s) {
        EXPECT_FALSE(s.m.insert(std::as_const(s.element)).second);
    }, intact);
    points += throwing::each_kind("sorted_map insert(value_type&&) taken", setup, [](MapState& s) {
        EXPECT_FALSE(s.m.insert(std::move(s.element)).second);
    }, intact);
    points += throwing::each_kind("sorted_map insert(pair<Key, V>&&) taken", setup, [](MapState& s) {
        EXPECT_FALSE(s.m.insert(std::move(s.other)).second);
    }, intact);
    points += throwing::each_kind("sorted_map try_emplace(const Key&, int) taken", setup, [](MapState& s) {
        EXPECT_FALSE(s.m.try_emplace(std::as_const(s.key), 5).second);
    }, intact);
    points += throwing::each_kind("sorted_map try_emplace(Key&&, int) taken", setup, [](MapState& s) {
        EXPECT_FALSE(s.m.try_emplace(std::move(s.key), 5).second);
    }, intact);
    points += throwing::each_kind("sorted_map insert(pair<int, int>&&) taken", setup, [key](MapState& s) {
        EXPECT_FALSE(s.m.insert(pair<int, int>(key, 5)).second);
    }, check);
    points += throwing::each_kind("sorted_map emplace(int, int) taken", setup, [key](MapState& s) {
        EXPECT_FALSE(s.m.emplace(key, 5).second);
    }, check);
    EXPECT_GE(points, 4);
}

// insert(first, last), of a moved range, and insert(initializer_list):
// the elements inserted before the throw stay, the one that threw is not
// linked, the rest not reached (insert.md (4–5)); the taken key in the
// range is left as it was, its element not moved from
TEST(SortedExceptionsMap_Tests, InsertionOfARange) {
    const std::vector<int> fresh = {13, 1, 31};   // in the order of the range; 14 among them taken
    auto setup = [] {
        auto s = map_state();
        for (int k : {13, 14, 1, 31}) {
            s->range.emplace_back(Val(k), Val(k * 10));
        }
        return s;
    };
    auto check = [&](MapState& s, bool threw, bool) {
        check_prefix(walked(s.m), threw, fresh);
        usable(s.m);
    };
    int points = 0;
    points += throwing::each_kind("sorted_map insert(first, last)", setup, [](MapState& s) {
        s.m.insert(s.range.begin(), s.range.end());
    }, [&](MapState& s, bool threw, bool fired) {
        EXPECT_EQ(s.range[1].second.v, 140);
        check(s, threw, fired);
    });
    points += throwing::each_kind("sorted_map insert(move_iterator range)", setup, [](MapState& s) {
        s.m.insert(std::make_move_iterator(s.range.begin()), std::make_move_iterator(s.range.end()));
    }, [&](MapState& s, bool threw, bool fired) {
        EXPECT_EQ(s.range[1].second.v, 140) << "the taken key's element moved from";
        check(s, threw, fired);
    });
    points += throwing::each_kind("sorted_map insert(initializer_list)", [] { return map_state(); }, [](MapState& s) {
        s.m.insert({{Val(13), Val(130)}, {Val(14), Val(140)}, {Val(1), Val(10)}, {Val(31), Val(310)}});
    }, check);
    EXPECT_GE(points, 34);
}

// set: insert(const Key&), insert(Key&&) and emplace of a new key: a
// throw links nothing and leaves the argument as it was
TEST(SortedExceptionsSet_Tests, InsertionOfANewKey) {
    for (int key : {1, 13, 31}) {
        const std::string at = " of key " + std::to_string(key);
        auto setup = [&] {
            return set_state(key);
        };
        auto check = [&](SetState& s, bool threw, bool) {
            check_insertion(s.s, threw, key);
        };
        int points = 0;
        points += throwing::each_kind("sorted_set insert(const Key&)" + at, setup, [](SetState& s) {
            EXPECT_TRUE(s.s.insert(std::as_const(s.key)).second);
        }, [&](SetState& s, bool threw, bool fired) {
            EXPECT_EQ(s.key.v, key);
            check(s, threw, fired);
        });
        points += throwing::each_kind("sorted_set insert(Key&&)" + at, setup, [](SetState& s) {
            EXPECT_TRUE(s.s.insert(std::move(s.key)).second);
        }, [&](SetState& s, bool threw, bool fired) {
            if (threw) {
                EXPECT_EQ(s.key.v, key) << "the argument moved from by a throwing insertion";
            }
            check(s, threw, fired);
        });
        points += throwing::each_kind("sorted_set emplace(int)" + at, setup, [key](SetState& s) {
            EXPECT_TRUE(s.s.emplace(key).second);
        }, check);
        EXPECT_GE(points, 3);
    }
}

// A taken key: the set as it was, the argument of insert as it was
// (insert.md (1–2)); emplace builds a node and drops it
TEST(SortedExceptionsSet_Tests, InsertionOfATakenKey) {
    auto setup = [] {
        return set_state(14);
    };
    auto intact = [](SetState& s, bool, bool) {
        EXPECT_EQ(s.key.v, 14);
        EXPECT_EQ(walked(s.s), Keys);
        usable(s.s);
    };
    int points = 0;
    points += throwing::each_kind("sorted_set insert(const Key&) taken", setup, [](SetState& s) {
        EXPECT_FALSE(s.s.insert(std::as_const(s.key)).second);
    }, intact);
    points += throwing::each_kind("sorted_set insert(Key&&) taken", setup, [](SetState& s) {
        EXPECT_FALSE(s.s.insert(std::move(s.key)).second);
    }, intact);
    points += throwing::each_kind("sorted_set emplace(int) taken", setup, [](SetState& s) {
        EXPECT_FALSE(s.s.emplace(14).second);
    }, intact);
    EXPECT_GE(points, 1);
}

// insert(first, last) of Keys, of moved Keys, of ints (a key of another
// type: built into a node first, insert.md (3)), and of an initializer
// list: a prefix of the range's new keys stays
TEST(SortedExceptionsSet_Tests, InsertionOfARange) {
    const std::vector<int> fresh = {13, 1, 31};
    auto setup = [] {
        auto s = set_state();
        for (int k : {13, 14, 1, 31}) {
            s->range.emplace_back(k);
        }
        return s;
    };
    auto check = [&](SetState& s, bool threw, bool) {
        check_prefix(walked(s.s), threw, fresh);
        usable(s.s);
    };
    int points = 0;
    points += throwing::each_kind("sorted_set insert(first, last)", setup, [](SetState& s) {
        s.s.insert(s.range.begin(), s.range.end());
    }, check);
    points += throwing::each_kind("sorted_set insert(move_iterator range)", setup, [](SetState& s) {
        s.s.insert(std::make_move_iterator(s.range.begin()), std::make_move_iterator(s.range.end()));
    }, [&](SetState& s, bool threw, bool fired) {
        EXPECT_EQ(s.range[1].v, 14) << "the taken key moved from";
        check(s, threw, fired);
    });
    points += throwing::each_kind("sorted_set insert(int range)", [] { return set_state(); }, [](SetState& s) {
        const std::vector<int> ints = {13, 14, 1, 31};
        s.s.insert(ints.begin(), ints.end());
    }, check);
    points += throwing::each_kind("sorted_set insert(initializer_list)", [] { return set_state(); }, [](SetState& s) {
        s.s.insert({Val(13), Val(14), Val(1), Val(31)});
    }, check);
    EXPECT_GE(points, 17);
}
