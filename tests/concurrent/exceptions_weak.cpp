//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Exception safety of concurrent::weak_map and concurrent::intern: the
// insertions (the value's construction, copy and move: tests/throwing.h)
// run with a throw at each point in turn,
// and the container checked after each run against the Exceptions of the
// operation's page: the live entries walked or found, none twice, none
// lost, the argument as it was. Nothing else has a throw point: a managed
// allocation throws nothing and the hash and the equality are noexcept,
// so weak_set, the erasures and the sweeps throw nothing.
#include "tests/throwing.h"

#include <algorithm>
#include <utility>
#include <vector>

using throwing::Val;

namespace {
    struct Obj {
        explicit Obj(int i) noexcept
        : id(i) {
        }

        int id;
    };

    using WeakMap = sgcl::concurrent::weak_map<Obj, Val>;
    using Intern = sgcl::concurrent::intern<Val, throwing::Hash<8>>;

    // The objects of a test, the same in every run of its loops (the
    // entries' buckets, so the dummies an operation makes, come from the
    // objects' addresses): 0..7 live keys, 8 the new one, held by a
    // managed vector on the test's frame
    struct Objects {
        Objects() {
            for (int i = 0; i < 9; ++i) {
                all.push_back(make_tracked<Obj>(i));
            }
        }

        sgcl::vector<tracked_ptr<Obj>> all;
    };

    // Inlined into the caller's frame, as weak_map.cpp's: the dead frames
    // zeroed, then full cycles, so that the objects dropped are found
    // unreachable and their cells cleared
    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }

    std::vector<int> ids(std::initializer_list<int> extra = {}) {
        std::vector<int> v = {0, 1, 2, 3, 4, 5, 6, 7};
        v.insert(v.end(), extra);
        std::sort(v.begin(), v.end());
        return v;
    }

    // The live entries walked, checked: no object twice, every value its
    // id times ten, every object found
    template<class C>
    std::vector<int> live_of(C& c) {
        std::vector<int> out;
        for (auto r : c) {
            if constexpr (requires { r.value; }) {
                out.push_back(r.key->id);
                EXPECT_EQ(r.value.v, r.key->id * 10) << "the value of object " << r.key->id;
                EXPECT_TRUE(c.contains(r.key));
            } else {
                out.push_back(r->id);
                EXPECT_TRUE(c.contains(r));
            }
        }
        std::sort(out.begin(), out.end());
        EXPECT_EQ(std::adjacent_find(out.begin(), out.end()), out.end()) << "an object walked twice";
        return out;
    }

    // `dead` entries of objects made and dropped here, in a frame of its
    // own, so that nothing in the caller's frame holds them
    template<class C>
    SGCL_NOINLINE void add_dead(C& c, int dead) {
        for (int i = 0; i < dead; ++i) {
            tracked_ptr<Obj> o = make_tracked<Obj>(100 + i);
            if constexpr (requires { c.insert(o, Val(0)); }) {
                c.insert(o, Val(1000 + i * 10));
            } else {
                c.insert(o);
            }
        }
    }

    struct WeakMapState {
        WeakMapState(const Objects& objects, int value)
        : value(value) {
            for (int i = 0; i < 8; ++i) {
                m.try_emplace(objects.all[i], Val(i * 10));
            }
            fresh = objects.all[8];
            taken = objects.all[3];
            at = m.find(taken);
        }

        WeakMap m;
        tracked_ptr<Obj> fresh;
        tracked_ptr<Obj> taken;
        Val value;
        WeakMap::iterator at;
        size_t total = 0;   // size() after the setup, the dead entries included
        size_t returned = 0;
    };

    // The map of 0..7 (values id * 10), with `dead` entries of objects
    // gone: 8 + dead insertions since the last sweep, so the next one
    // sweeps when they make 16
    tracked_ptr<WeakMapState> weak_map_state(const Objects& objects, int dead = 0, int value = 80) {
        tracked_ptr<WeakMapState> s = make_tracked<WeakMapState>(objects, value);
        add_dead(s->m, dead);
        settle();
        s->total = s->m.size();
        return s;
    }
}

// try_emplace, emplace, insert(const T&), insert(T&&) of an object
// without an entry: a throw (the value's construction, copy or move)
// links nothing and the map is as it was;
// insert(const T&) leaves its argument, and insert(T&&) too when it threw
TEST(ExceptionsWeakMap_Tests, InsertionOfANewObject) {
    Objects objects;
    auto setup = [&] {
        return weak_map_state(objects);
    };
    auto check = [](WeakMapState& s, bool threw, bool) {
        EXPECT_EQ(live_of(s.m), threw ? ids() : ids({8}));
        EXPECT_EQ(s.m.size(), threw ? 8u : 9u);
        EXPECT_EQ(s.m.contains(s.fresh), !threw);
    };
    int points = 0;
    points += throwing::each_kind("weak_map try_emplace(object, int)", setup, [](WeakMapState& s) {
        EXPECT_TRUE(s.m.try_emplace(s.fresh, 80).second);
    }, check);
    points += throwing::each_kind("weak_map emplace(object, int)", setup, [](WeakMapState& s) {
        EXPECT_TRUE(s.m.emplace(s.fresh, 80).second);
    }, check);
    points += throwing::each_kind("weak_map insert(object, const T&)", setup, [](WeakMapState& s) {
        EXPECT_TRUE(s.m.insert(s.fresh, std::as_const(s.value)).second);
    }, [&](WeakMapState& s, bool threw, bool fired) {
        check(s, threw, fired);
        EXPECT_EQ(s.value.v, 80);
    });
    points += throwing::each_kind("weak_map insert(object, T&&)", setup, [](WeakMapState& s) {
        EXPECT_TRUE(s.m.insert(s.fresh, std::move(s.value)).second);
    }, [&](WeakMapState& s, bool threw, bool fired) {
        check(s, threw, fired);
        if (threw) {
            EXPECT_EQ(s.value.v, 80) << "the argument moved from by a throwing insertion";
        }
    });
    EXPECT_GE(points, 4);
}

namespace {
    // A pool of the values Values, each held by a handle of the state;
    // `dead` values more interned and dropped
    const std::vector<int> Values = {0, 2, 4, 6, 8, 3, 11, 19};

    struct InternState {
        explicit InternState(int value)
        : arg(value) {
            for (int v : Values) {
                held.push_back(pool.of(Val(v)));
            }
        }

        Intern pool;
        sgcl::vector<tracked_ptr<const Val>> held;
        Val arg;
        tracked_ptr<const Val> result;
        size_t total = 0;
        size_t returned = 0;
    };

    SGCL_NOINLINE void add_dead_values(Intern& pool, int dead) {
        for (int i = 0; i < dead; ++i) {
            pool.of(Val(100 + i));
        }
    }

    tracked_ptr<InternState> intern_state(int value, int dead = 0) {
        tracked_ptr<InternState> s = make_tracked<InternState>(value);
        add_dead_values(s->pool, dead);
        settle();
        s->total = s->pool.size();
        return s;
    }

    // The held values each found as the object held, and how many
    size_t found_held(InternState& s) {
        size_t n = 0;
        for (size_t i = 0; i < Values.size(); ++i) {
            auto h = s.pool.find(Val(Values[i]));
            if (h) {
                EXPECT_EQ(h.get(), s.held[i].get()) << "value " << Values[i] << " found as another object";
                ++n;
            }
        }
        return n;
    }
}

// of(value) of a new value, in a chain of one hash (27) and in a fresh
// bucket (7): a throw (the copy of the value) enters nothing and
// the pool is as it was (of.md); without one the value is found as the
// object returned
TEST(ExceptionsIntern_Tests, OfANewValue) {
    for (int value : {27, 7}) {
        int points = throwing::each_kind("intern of(" + std::to_string(value) + ")", [&] {
            return intern_state(value);
        }, [](InternState& s) {
            s.result = s.pool.of(std::as_const(s.arg));
        }, [&](InternState& s, bool threw, bool) {
            EXPECT_EQ(found_held(s), Values.size());
            EXPECT_EQ(s.pool.size(), threw ? Values.size() : Values.size() + 1);
            auto h = s.pool.find(Val(value));
            if (threw) {
                EXPECT_FALSE(h);
            } else {
                ASSERT_TRUE(h);
                EXPECT_EQ(h.get(), s.result.get());
                EXPECT_EQ(h->v, value);
            }
            EXPECT_EQ(s.arg.v, value);
        });
        EXPECT_GE(points, 1);
    }
}
