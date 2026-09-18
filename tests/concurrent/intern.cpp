//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// intern: a pool where equal values share one managed object, held weakly;
// intern_string: the same for strings, the string being its own object.
#include "tests/types.h"

#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    struct Point {
        int x, y;
        bool operator==(const Point&) const = default;
    };

    struct PointHash {
        size_t operator()(const Point& p) const noexcept {
            return size_t(p.x) * 1000003u + size_t(p.y);
        }
    };

    // A value that counts its objects: what the pool makes, what dies
    struct Counted {
        explicit Counted(int v) : value(v) { ++alive; }
        Counted(const Counted& o) : value(o.value) { ++alive; }
        ~Counted() { value = -1; --alive; }
        bool operator==(const Counted& o) const noexcept { return value == o.value; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    struct CountedHash {
        size_t operator()(const Counted& c) const noexcept {
            return std::hash<int>()(c.value);
        }
    };

    SGCL_ALWAYS_INLINE void settle() {
        collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
    }
}

TEST(Intern_Tests, EqualValuesShareTheObjectDifferentOnesDoNot) {
    intern<Point, PointHash> pool;
    EXPECT_TRUE(pool.empty());
    tracked_ptr<const Point> a = pool.get({1, 2});
    tracked_ptr<const Point> b = pool.get({1, 2});
    tracked_ptr<const Point> c = pool.get({2, 1});
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_EQ(a->x, 1);
    EXPECT_EQ(c->y, 1);
    EXPECT_EQ(pool.size(), 2u);
    EXPECT_EQ(pool.find(Point{1, 2}), a);
    EXPECT_EQ(pool.find(Point{3, 3}), nullptr);
    pool.clear();
    EXPECT_TRUE(pool.empty());
    EXPECT_EQ(pool.find(Point{1, 2}), nullptr);
    EXPECT_NE(pool.get(Point{1, 2}), a);        // forgotten by the pool, alive where held: made again
    EXPECT_EQ(a->y, 2);
}

TEST(Intern_Tests, TheDefaultPoolIsOneForTheProgram) {
    using Points = intern<Point, PointHash>;
    tracked_ptr<const Point> a = Points::make({7, 7});
    tracked_ptr<const Point> b = Points::pool().get({7, 7});
    EXPECT_EQ(a, b);
    EXPECT_EQ(Points::pool().find(Point{7, 7}), a);
    std::thread other([&] {                // the same pool from another thread
        EXPECT_EQ(Points::make(Point{7, 7}), a);
    });
    other.join();
}

TEST(Intern_Tests, ADeadObjectIsReplacedByANewOne) {
    settle();
    const int before = Counted::alive.load();
    intern<Counted, CountedHash> pool;
    tracked_ptr<const Counted> kept;
    uintptr_t dropped_address = 0;
    off_frame([&] {
        kept = pool.get(Counted(1));
        tracked_ptr<const Counted> dropped = pool.get(Counted(2));
        dropped_address = hide(dropped.get());
        EXPECT_EQ(pool.get(Counted(2)), dropped);
    });
    EXPECT_EQ(Counted::alive.load(), before + 2);
    EXPECT_EQ(pool.size(), 2u);
    settle();
    EXPECT_EQ(Counted::alive.load(), before + 1);   // the pool did not keep it
    EXPECT_EQ(pool.size(), 2u);            // the dead entry not yet swept
    EXPECT_EQ(pool.find(Counted(2)), nullptr);      // and never found
    EXPECT_EQ(pool.find(Counted(1)), kept);
    tracked_ptr<const Counted> again = pool.get(Counted(2));   // a new object, entered beside the dead entry
    EXPECT_EQ(again->value, 2);
    EXPECT_EQ(Counted::alive.load(), before + 2);
    EXPECT_EQ(pool.get(Counted(2)), again);
    EXPECT_EQ(pool.size(), 3u);
    EXPECT_EQ(pool.sweep(), 1u);
    EXPECT_EQ(pool.size(), 2u);
    EXPECT_EQ(pool.sweep(), 0u);
    (void)dropped_address;
}

TEST(Intern_Tests, TheInsertionsSweepTheDeadEntriesByThemselves) {
    settle();
    intern<Counted, CountedHash> pool;
    off_frame([&] {
        for (int i = 0; i < 300; ++i) {
            pool.get(Counted(i));          // made and dropped at once
        }
    });
    settle();
    const auto before = pool.size();
    EXPECT_GT(before, 0u);
    off_frame([&] {
        for (int i = 1000; i < 1600; ++i) {   // more insertions than entries: a sweep ran
            pool.get(Counted(i));
        }
    });
    EXPECT_LT(pool.size(), before + 600);
    settle();
    pool.sweep();
    EXPECT_TRUE(pool.empty());
}

TEST(Intern_Tests, StringsAreTheirOwnObjects) {
    string a = intern_string("alpha");
    string b = intern_string("alpha");
    string c = intern<string>::make(std::string("alpha"));
    string d = intern_string("beta");
    EXPECT_EQ(a.object(), b.object());     // one object for the value
    EXPECT_EQ(a.object(), c.object());
    EXPECT_NE(a.object(), d.object());
    EXPECT_EQ(a, "alpha");
    EXPECT_EQ(d, "beta");
    string own("gamma");                   // a string passed in enters as it is, Java's String.intern
    string got = intern<string>::make(own);
    EXPECT_EQ(got.object(), own.object());
    EXPECT_EQ(intern<string>::make(std::string_view("gamma")).object(), own.object());
    string empty = intern_string("");      // the empty string is null: one value, no object, never entered
    EXPECT_EQ(empty.object(), nullptr);
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(intern<string>::pool().find("").object(), nullptr);
    EXPECT_EQ(intern<string>::pool().find("alpha").object(), a.object());
    EXPECT_EQ(intern<string>::pool().find("delta").object(), nullptr);
    std::string longer(3000, 'x');         // a string of a large class, an object still
    string l1 = intern_string(longer);
    string l2 = intern_string(longer);
    EXPECT_EQ(l1.object(), l2.object());
    EXPECT_EQ(l1.size(), 3000u);
}

TEST(Intern_Tests, AStringPresentIsInternedFromAViewWithoutATemporary) {
    string a = intern_string("present");
    collector::force_collect(true);
    const auto before = collector::get_statistics().live_bytes;
    for (int i = 0; i < 10000; ++i) {
        string s = intern_string("present");
        if (s.object() != a.object()) {
            ADD_FAILURE() << "another object";
            break;
        }
        std::string_view view = "present";
        if (intern<string>::make(view).object() != a.object()) {
            ADD_FAILURE() << "another object from a view";
            break;
        }
    }
    EXPECT_LE(collector::get_statistics().live_bytes, before);   // no string made for the search (a sweep meanwhile can only lower the count)
}

TEST(Intern_Tests, ADeadStringIsMadeAgain) {
    settle();
    intern<string> pool;
    uintptr_t old_object = 0;
    off_frame([&] {
        string s = pool.get("transient");
        old_object = hide(s.object());
        EXPECT_EQ(pool.get(std::string_view("transient")).object(), s.object());
    });
    settle();
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_EQ(pool.find("transient").object(), nullptr);   // dead: not found
    string again = pool.get("transient");  // a new string
    EXPECT_EQ(again, "transient");
    EXPECT_EQ(pool.get("transient").object(), again.object());
    EXPECT_EQ(pool.size(), 2u);
    EXPECT_EQ(pool.sweep(), 1u);
    EXPECT_EQ(pool.size(), 1u);
    (void)old_object;
}

TEST(Intern_Tests, APoolInsideAManagedObject) {
    struct Holder {
        intern<Point, PointHash> points;
        intern<string> names;
    };
    tracked_ptr h = make_tracked<Holder>();
    tracked_ptr<const Point> p = h->points.get({4, 5});
    EXPECT_EQ(h->points.get(Point{4, 5}), p);
    EXPECT_EQ(h->names.get("x").object(), h->names.get("x").object());
    EXPECT_EQ(h->points.size(), 1u);
    EXPECT_EQ(h->names.size(), 1u);
}

// Many threads intern the same few values at once, on a pool of their own
// and on the default one, while the collector runs: every result of a
// value is the one object, whichever thread made it
TEST(Intern_Tests, ManyThreadsInternTheSameFewValues) {
    const int threads = 8;
    const int values = 10;
    const int rounds = 2000;
    off_frame([&] {
        intern<Point, PointHash> pool;
        vector<vector<tracked_ptr<const Point>>> got(threads);   // one per value per thread, held: the objects stay alive
        vector<vector<string>> names(threads);
        for (int t = 0; t < threads; ++t) {   // sized before the threads start: no buffer grows under a collection
            got[t].resize(values);
            names[t].resize(values);
        }
        std::vector<std::thread> ws;
        sgcl::atomic<bool> bad = {false};
        for (int t = 0; t < threads; ++t) {
            ws.emplace_back([&, t] {
                for (int r = 0; r < rounds; ++r) {
                    for (int v = 0; v < values; ++v) {
                        tracked_ptr<const Point> p = pool.get({v, v * 2});
                        if (p->x != v || p->y != v * 2) {
                            bad = true;
                        }
                        if (!got[t][v]) {
                            got[t][v] = p;
                        } else if (got[t][v] != p) {
                            bad = true;    // the object changed while held: two objects for one value
                        }
                        string s = intern_string("value-" + std::to_string(v));
                        if (names[t][v].empty()) {
                            names[t][v] = s;
                        } else if (names[t][v].object() != s.object()) {
                            bad = true;
                        }
                    }
                    if (r % 500 == 499 && t == 0) {
                        collector::force_collect();
                    }
                }
            });
        }
        for (auto& w : ws) {
            w.join();
        }
        EXPECT_FALSE(bad.load());
        for (int v = 0; v < values; ++v) {
            for (int t = 1; t < threads; ++t) {
                EXPECT_EQ(got[t][v], got[0][v]);
                EXPECT_EQ(names[t][v].object(), names[0][v].object());
            }
            EXPECT_EQ(pool.find(Point{v, v * 2}), got[0][v]);
        }
        EXPECT_EQ(pool.size(), size_t(values));
    });
}

// Threads intern values they drop at once, a few kept, while the
// collector runs, in two rounds with a collection between: the entries of
// the first round are dead when the second interns as many again, so a
// sweep runs under the second round and drops them; settled, the pool
// holds one live object per kept value
TEST(Intern_Tests, ManyThreadsInternValuesTheyDrop) {
    settle();
    const int before = Counted::alive.load();
    const int threads = 8;
    const int per_thread = 3000;
    intern<Counted, CountedHash> pool;
    vector<tracked_ptr<const Counted>> kept;
    off_frame([&] {
        concurrent_queue<tracked_ptr<const Counted>> kept_by_threads;   // what the threads keep, handed over
        auto round = [&](int base) {
            std::vector<std::thread> ws;
            for (int t = 0; t < threads; ++t) {
                ws.emplace_back([&, t] {
                    for (int i = 0; i < per_thread; ++i) {
                        int v = base + i * threads + t;   // a value of its own per thread: as many insertions as values
                        tracked_ptr<const Counted> p = pool.get(Counted(v));
                        if (p->value != v) {
                            ADD_FAILURE() << "wrong object";
                        }
                        if (i % 300 == 0) {
                            kept_by_threads.push(p);
                        }
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
        round(0);
        collector::force_collect(true);    // the dropped objects of the first round are dead
        collector::force_collect(true);
        const size_t first = pool.size();
        round(threads * per_thread);
        EXPECT_LT(pool.size(), first + size_t(threads * per_thread));   // a sweep ran under the second round
        while (auto p = kept_by_threads.try_pop()) {
            kept.push_back(*p);
        }
    });
    settle();
    pool.sweep();
    std::set<int> values;
    for (auto& p : kept) {
        values.insert(p->value);
        EXPECT_EQ(pool.find(*p), p);       // the kept ones are the pool's
    }
    EXPECT_EQ(pool.size(), values.size());
    EXPECT_EQ(Counted::alive.load(), before + (int)values.size());   // one object per value
    std::set<uintptr_t> objects;
    for (auto& p : kept) {
        objects.insert(hide(p.get()));
    }
    EXPECT_EQ(objects.size(), values.size());
}
