//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the exception-safety tests share: a value whose construction from
// an int, copy and move throw at the k-th call of their kind (a
// countdown of the thread, set by the test), and the loop that runs an
// operation with the throw at each point in turn, k = 1, 2, ..., until
// the operation makes fewer calls than k. Nothing else is a throw point:
// a managed allocation throws nothing (running out of managed memory
// ends the program: core/detail/maker.h), and the hash, the equality and
// the comparison of a container must be noexcept (DESIGN 356).
#pragma once

#include "tests/types.h"

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace throwing {
    // What a countdown of the value's operations throws: the kind's name
    struct Error : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    // The countdowns of the thread, one per kind of call: set to k, the
    // k-th call of the kind from then on throws Error and the countdown
    // stays at 0; 0 leaves the kind alone
    struct Countdowns {
        int construct = 0;   // Val(int): an element built from emplace's arguments
        int copy = 0;        // copy construction and copy assignment
        int move = 0;        // move construction and move assignment
    };

    inline thread_local Countdowns countdown;

    inline void tick(int& n, const char* what) {
        if (n > 0 && --n == 0) {
            throw Error(what);
        }
    }

    // Every countdown at 0 when the scope ends
    struct Disarm {
        Disarm() = default;
        Disarm(const Disarm&) = delete;

        ~Disarm() {
            countdown = {};
        }
    };

    // An int whose operations count down. A throwing copy or move throws
    // before it changes anything; a move that succeeds leaves Moved
    // behind, so that an element lost, or left twice in a container,
    // shows in what the container holds
    struct Val {
        static constexpr int Moved = -1;

        int v = 0;

        Val() noexcept = default;

        Val(int x)
        : v(x) {
            tick(countdown.construct, "construct");
        }

        Val(const Val& o)
        : v(o.v) {
            tick(countdown.copy, "copy");
        }

        Val(Val&& o) noexcept(false)
        : v(o.v) {
            tick(countdown.move, "move");
            o.v = Moved;
        }

        Val& operator=(const Val& o) {
            tick(countdown.copy, "copy");
            v = o.v;
            return *this;
        }

        Val& operator=(Val&& o) noexcept(false) {
            tick(countdown.move, "move");
            v = o.v;
            o.v = Moved;
            return *this;
        }

        friend bool operator==(const Val& a, const Val& b) noexcept {
            return a.v == b.v;
        }

        friend bool operator<(const Val& a, const Val& b) noexcept {
            return a.v < b.v;
        }
    };

    // The hash of a Val: the int, or its residue mod Mod, so that the keys
    // of one residue share a hash (a chain the equality walks)
    template<int Mod = 0>
    struct Hash {
        size_t operator()(const Val& x) const noexcept {
            return Mod ? size_t(x.v % Mod) : size_t(x.v);
        }
    };

    // The comparison as a comparator object rather than the type's (a
    // comparator with state is what priority_queue and the sorted
    // containers copy)
    struct Less {
        bool operator()(const Val& a, const Val& b) const noexcept {
            return a.v < b.v;
        }
    };

    // The ints of a range of Vals (or of the first of pairs)
    template<class R>
    std::vector<int> ints_of(const R& range) {
        std::vector<int> out;
        for (const auto& e : range) {
            if constexpr (requires { e.first; }) {
                out.push_back(e.first.v);
            } else {
                out.push_back(e.v);
            }
        }
        return out;
    }

    // The runs of one loop, printed when SGCL_ES_REPORT is set (the report
    // of the throw points reached)
    inline void report(const std::string& what, const char* kind, int fired) {
        if (std::getenv("SGCL_ES_REPORT")) {
            std::printf("[ points ] %s, %s: %d\n", what.c_str(), kind, fired);
        }
    }

    // The loop: for k = 1, 2, ... a fresh state from `setup` (a
    // tracked_ptr to a managed object, so that the containers in it are
    // where tracked pointers may live), the countdown `counter` set to k,
    // `op` run on it, then `check(state, threw, fired)` with the countdown
    // disarmed (`fired`: the countdown reached its call, the exception
    // thrown out of the operation or swallowed by it); it ends with the
    // first run in which the countdown did not fire (the operation made
    // fewer calls than k). An exception that fired is caught here: the
    // value's Error; any other goes on and fails the test.
    // Returns the number of runs in which the countdown fired.
    template<class Setup, class Op, class Check>
    int each_point(const std::string& what, const char* kind, int& counter, Setup setup, Op op, Check check, int limit = 4000) {
        Disarm disarm;
        for (int k = 1; k <= limit; ++k) {
            auto state = setup();
            bool threw = false;
            counter = k;
            try {
                op(*state);
            } catch (const Error&) {
                threw = true;
            }
            const bool fired = counter == 0;
            countdown = {};
            EXPECT_TRUE(fired || !threw) << what << ", " << kind << ": threw without the countdown at k = " << k;
            SCOPED_TRACE(what + ", " + kind + ", k = " + std::to_string(k) + (threw ? " (threw)" : fired ? " (swallowed)" : ""));
            check(*state, threw, fired);
            if (!fired) {
                report(what, kind, k - 1);
                return k - 1;
            }
        }
        ADD_FAILURE() << what << ", " << kind << ": still throwing at k = " << limit;
        return limit;
    }

    // The kinds of throw points, for an operation that must leave some out
    enum Kinds : unsigned {
        Construct = 1,
        Copy = 2,
        Move = 4,
        All = 7,
    };

    // each_point for every kind of throw point asked for (all of them by
    // default: the value's three countdowns); the points reached, all
    // kinds together
    template<class Setup, class Op, class Check>
    int each_kind(const std::string& what, Setup setup, Op op, Check check, unsigned kinds = All) {
        int points = 0;
        if (kinds & Construct) {
            points += each_point(what, "construct", countdown.construct, setup, op, check);
        }
        if (kinds & Copy) {
            points += each_point(what, "copy", countdown.copy, setup, op, check);
        }
        if (kinds & Move) {
            points += each_point(what, "move", countdown.move, setup, op, check);
        }
        return points;
    }
}
