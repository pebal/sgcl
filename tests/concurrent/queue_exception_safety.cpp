//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Exception safety of concurrent::queue and concurrent::priority_queue:
// every pushing and popping operation run with a throw at each of its
// points in turn (the element's construction, copy and move:
// tests/throwing.h), and after each run the queue
// drained and checked against what the operation's page says in its
// Exceptions: size() the elements drained, the order kept, none twice,
// none lost beyond what the page allows. The comparator of
// priority_queue is noexcept.
#include "tests/throwing.h"

#include <algorithm>
#include <iterator>
#include <random>
#include <utility>
#include <vector>

using throwing::Val;

namespace {
    // Queue
    using Queue = sgcl::concurrent::queue<Val>;

    // The elements before every operation: 0, 1, ..., 4 in this order
    constexpr int QueueSize = 5;

    std::vector<int> iota(int from, int to) {
        std::vector<int> v;
        for (int i = from; i < to; ++i) {
            v.push_back(i);
        }
        return v;
    }

    struct QueueState {
        QueueState() {
            for (int i = 0; i < QueueSize; ++i) {
                q.push(Val(i));
            }
            range = {Val(100), Val(101), Val(102)};
        }

        Queue q;
        Val value{100};
        std::vector<Val> range;
        int got = -2;   // the value popped, read without a move of its own
    };

    tracked_ptr<QueueState> queue_state() {
        return make_tracked<QueueState>();
    }

    // The queue drained (the countdowns disarmed): size() before the
    // drain the count drained, nothing moved from, the elements in the
    // order they went in
    std::vector<int> drained(Queue& q) {
        const size_t size = q.size();
        std::vector<int> out;
        while (auto v = q.try_pop()) {
            out.push_back(v->v);
        }
        EXPECT_EQ(size, out.size()) << "size() against the elements drained";
        EXPECT_EQ(std::count(out.begin(), out.end(), Val::Moved), 0) << "an element moved from";
        EXPECT_TRUE(q.empty());
        EXPECT_EQ(q.size(), 0u);
        return out;
    }

    // A push of the elements `pushed`: after a throw the queue is as it
    // was (nothing linked), after a run without one the elements follow
    // the old ones
    void check_push(QueueState& s, bool threw, const std::vector<int>& pushed) {
        auto expected = iota(0, QueueSize);
        if (!threw) {
            expected.insert(expected.end(), pushed.begin(), pushed.end());
        }
        EXPECT_EQ(drained(s.q), expected);
    }

    // A pop of the first element: taken whether its move threw or not
    // (try_pop.md: claimed before it is moved, lost when the move throws),
    // the others intact and in order
    void check_pop(QueueState& s, bool threw) {
        EXPECT_EQ(drained(s.q), iota(1, QueueSize));
        if (!threw) {
            EXPECT_EQ(s.got, 0);
        }
    }

    // Priority queue
    using PriorityQueue = sgcl::concurrent::priority_queue<Val>;

    // The values before every operation, pushed in a scrambled order so
    // that the heap is not a sorted array; popped smallest first. A push
    // of 5 climbs to the root (moves in the sift), one of 1000 stays at
    // the bottom (one comparison, no move in the sift)
    std::vector<int> pq_values(int n) {
        std::vector<int> v;
        for (int i = 0; i < n; ++i) {
            v.push_back(10 + i * 10);
        }
        std::shuffle(v.begin(), v.end(), std::mt19937(unsigned(n)));
        return v;
    }

    struct PqState {
        explicit PqState(int n, int value)
        : value(value)
        , values(pq_values(n)) {
            for (int x : values) {
                q.push(Val(x));
            }
        }

        PriorityQueue q;
        Val value;
        std::vector<int> values;
        int got = -2;   // the value popped, read without a move of its own
    };

    std::vector<int> sorted(std::vector<int> v) {
        std::sort(v.begin(), v.end());
        return v;
    }

    // The queue drained (the countdowns disarmed), checked whole:
    // size() the count drained, nothing moved from, smallest first
    std::vector<int> drained(PriorityQueue& q) {
        const size_t size = q.size();
        std::vector<int> out;
        while (auto v = q.try_pop()) {
            out.push_back(v->v);
        }
        EXPECT_EQ(size, out.size()) << "size() against the elements drained";
        EXPECT_EQ(std::count(out.begin(), out.end(), Val::Moved), 0) << "an element moved from";
        EXPECT_TRUE(std::is_sorted(out.begin(), out.end())) << "the priority order";
        EXPECT_TRUE(q.empty());
        return out;
    }

    // What holds whatever the contents after a throwing move (the pages:
    // "the heap's contents unspecified"): the queue drains to empty, and
    // each value drained, a moved-from one aside, is one of `allowed`
    // and comes at most once (no element doubled)
    void check_unspecified(PriorityQueue& q, const std::vector<int>& allowed) {
        std::vector<int> out;
        size_t n = 0;
        while (auto v = q.try_pop()) {
            ASSERT_LE(++n, allowed.size() + 1) << "more elements drained than there were";
            if (v->v != Val::Moved) {
                out.push_back(v->v);
            }
        }
        EXPECT_TRUE(q.empty());
        EXPECT_EQ(q.size(), 0u);
        out = sorted(out);
        EXPECT_EQ(std::adjacent_find(out.begin(), out.end()), out.end()) << "an element doubled";
        for (int x : out) {
            EXPECT_TRUE(std::binary_search(allowed.begin(), allowed.end(), x)) << x << " was never pushed";
        }
    }

    // The heap sizes the priority-queue loops run at: a push into a
    // buffer with room and one that grows it, whatever the vector's
    // growth steps (1..17)
    constexpr int MaxPqSize = 17;
}

// push(const T&), push(T&&), emplace and push_range of queue: a throw of
// the element's construction, copy or move links nothing and leaves the queue as it was (push.md, emplace.md,
// push_range.md); the argument of push(const T&) as it was in every run
// (the pages promise nothing of the argument of push(T&&))
TEST(QueueExceptionSafety_Tests, Push) {
    int points = 0;
    points += throwing::each_kind("queue push(const T&)", queue_state, [](QueueState& s) {
        s.q.push(std::as_const(s.value));
    }, [](QueueState& s, bool threw, bool) {
        check_push(s, threw, {100});
        EXPECT_EQ(s.value.v, 100);
    });
    points += throwing::each_kind("queue push(T&&)", queue_state, [](QueueState& s) {
        s.q.push(std::move(s.value));
    }, [](QueueState& s, bool threw, bool) {
        check_push(s, threw, {100});
    });
    points += throwing::each_kind("queue emplace(int)", queue_state, [](QueueState& s) {
        s.q.emplace(100);
    }, [](QueueState& s, bool threw, bool) {
        check_push(s, threw, {100});
    });
    points += throwing::each_kind("queue push_range", queue_state, [](QueueState& s) {
        s.q.push_range(s.range.begin(), s.range.end());
    }, [](QueueState& s, bool threw, bool) {
        check_push(s, threw, {100, 101, 102});
        EXPECT_EQ(throwing::ints_of(s.range), (std::vector<int>{100, 101, 102}));
    });
    // copy 1 + 3, move 1, construct 1
    EXPECT_GE(points, 6);
}

// try_pop and pop of queue: the element is claimed before it is moved, so
// a move that throws loses it (try_pop.md, pop.md) and leaves the rest
// intact and in order; pop's own move of the value out is a second such
// point. Nothing else of the two throws (no allocation)
TEST(QueueExceptionSafety_Tests, Pop) {
    int points = 0;
    points += throwing::each_kind("queue try_pop", queue_state, [](QueueState& s) {
        if (auto r = s.q.try_pop()) {
            s.got = r->v;
        }
    }, [](QueueState& s, bool threw, bool) {
        check_pop(s, threw);
    });
    points += throwing::each_kind("queue pop", queue_state, [](QueueState& s) {
        Val v = s.q.pop();
        s.got = v.v;
    }, [](QueueState& s, bool threw, bool) {
        check_pop(s, threw);
    });
    EXPECT_GE(points, 2);
}

// push(const T&), push(T&&) and emplace of priority_queue of an element
// that stays at the bottom of the heap (no move in the sift): a throw of
// the element's construction, of its copy or move into the buffer, or of
// a copy of the elements a growth relocates, leaves the
// queue as it was: the element not in it, the others in their order,
// size() unchanged (push.md, emplace.md)
TEST(QueueExceptionSafety_Tests, PriorityQueuePushAtTheBottom) {
    int points = 0;
    for (int n = 1; n <= MaxPqSize; ++n) {
        const std::string at = " into " + std::to_string(n);
        auto setup = [n] {
            return make_tracked<PqState>(n, 1000);
        };
        auto check = [](PqState& s, bool threw, bool) {
            auto expected = s.values;
            if (!threw) {
                expected.push_back(1000);
            }
            EXPECT_EQ(drained(s.q), sorted(expected));
        };
        auto run = [&](const std::string& what, auto op, auto check_arg) {
            auto both = [&](PqState& s, bool threw, bool fired) {
                check(s, threw, fired);
                check_arg(s, threw);
            };
            points += throwing::each_point(what + at, "construct", throwing::countdown.construct, setup, op, both);
            points += throwing::each_point(what + at, "copy", throwing::countdown.copy, setup, op, both);
            points += throwing::each_point(what + at, "move", throwing::countdown.move, setup, op, both);
        };
        run("priority_queue push(const T&)", [](PqState& s) {
            s.q.push(std::as_const(s.value));
        }, [](PqState& s, bool) {
            EXPECT_EQ(s.value.v, 1000);
        });
        run("priority_queue push(T&&)", [](PqState& s) {
            s.q.push(std::move(s.value));
        }, [](PqState&, bool) {
        });
        run("priority_queue emplace(int)", [](PqState& s) {
            s.q.emplace(1000);
        }, [](PqState&, bool) {
        });
    }
    EXPECT_GT(points, 3 * MaxPqSize);
}

// The same pushes of an element that climbs to the root. The
// construction (and copy) of the element: as it was. A move that
// throws while the element is moved into its place leaves the contents
// unspecified (push.md): then only that the queue drains, nothing doubled
// and nothing foreign in it
TEST(QueueExceptionSafety_Tests, PriorityQueuePushToTheRoot) {
    int points = 0;
    for (int n = 1; n <= MaxPqSize; ++n) {
        const std::string at = " into " + std::to_string(n);
        auto setup = [n] {
            return make_tracked<PqState>(n, 5);
        };
        auto strict = [](PqState& s, bool threw, bool) {
            auto expected = s.values;
            if (!threw) {
                expected.push_back(5);
            }
            EXPECT_EQ(drained(s.q), sorted(expected));
        };
        auto weak = [](PqState& s, bool threw, bool fired) {
            if (!threw) {
                auto expected = s.values;
                expected.push_back(5);
                EXPECT_EQ(drained(s.q), sorted(expected));
                return;
            }
            check_unspecified(s.q, sorted([&] {
                auto v = s.values;
                v.push_back(5);
                return v;
            }()));
        };
        auto run = [&](const std::string& what, auto op) {
            points += throwing::each_point(what + at, "construct", throwing::countdown.construct, setup, op, strict);
            points += throwing::each_point(what + at, "copy", throwing::countdown.copy, setup, op, strict);
            points += throwing::each_point(what + at, "move", throwing::countdown.move, setup, op, weak);
        };
        run("priority_queue push(const T&)", [](PqState& s) {
            s.q.push(std::as_const(s.value));
        });
        run("priority_queue push(T&&)", [](PqState& s) {
            s.q.push(std::move(s.value));
        });
        run("priority_queue emplace(int)", [](PqState& s) {
            s.q.emplace(5);
        });
    }
    EXPECT_GT(points, 3 * MaxPqSize);
}

// try_pop and pop of priority_queue with a move of T that throws: the
// contents are unspecified (try_pop.md, pop.md), so only that the queue
// drains, nothing doubled and nothing foreign in it; a run without a
// throw takes the least element and leaves the rest in order. No copy
// in either
TEST(QueueExceptionSafety_Tests, PriorityQueuePop) {
    int points = 0;
    for (int n = 1; n <= MaxPqSize; ++n) {
        const std::string at = " of " + std::to_string(n);
        auto setup = [n] {
            return make_tracked<PqState>(n, 0);
        };
        auto check = [](PqState& s, bool threw, bool) {
            const auto all = sorted(s.values);
            if (threw) {
                check_unspecified(s.q, all);
                return;
            }
            EXPECT_EQ(s.got, all.front());
            EXPECT_EQ(drained(s.q), std::vector<int>(all.begin() + 1, all.end()));
        };
        auto run = [&](const std::string& what, auto op) {
            points += throwing::each_point(what + at, "copy", throwing::countdown.copy, setup, op, check);
            points += throwing::each_point(what + at, "move", throwing::countdown.move, setup, op, check);
        };
        run("priority_queue try_pop", [](PqState& s) {
            if (auto r = s.q.try_pop()) {
            s.got = r->v;
        }
        });
        run("priority_queue pop", [](PqState& s) {
            Val v = s.q.pop();
        s.got = v.v;
        });
    }
    EXPECT_GT(points, 2 * MaxPqSize);
}
