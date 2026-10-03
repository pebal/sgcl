//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the exception-safety tests of core's containers share (DESIGN 356,
// 408): an element that counts its live objects and throws at the k-th
// construction, copy or move (tests/throwing.h's countdowns), and the loop
// that runs an operation with the throw at each point of each kind in turn.
// After every run the container is walked and checked: no element read
// after its end, every object alive accounted for (the container's, the
// arguments'), and, where the operation's page promises the strong
// guarantee for that kind of throw, the elements as they were; a run
// without a throw gives the elements the page describes.
#pragma once

#include "tests/throwing.h"

#include <atomic>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace es {
    using throwing::countdown;
    using throwing::tick;

    inline constexpr int Moved = -1;
    inline constexpr int Dead = -1000000;

    // An int that throws as tests/throwing.h's Val does and counts its
    // live objects; its move may throw (Element) or not (NothrowElement),
    // which the containers' relocations tell apart
    template<bool NothrowMove>
    struct ElementOf {
        static inline std::atomic<int> alive = 0;

        int v = 0;

        ElementOf() {
            tick(countdown.construct, "construct");
            ++alive;
        }

        ElementOf(int x)
        : v(x) {
            tick(countdown.construct, "construct");
            ++alive;
        }

        ElementOf(const ElementOf& o)
        : v(o.v) {
            tick(countdown.copy, "copy");
            ++alive;
        }

        ElementOf(ElementOf&& o) noexcept(NothrowMove)
        : v(o.v) {
            if constexpr (!NothrowMove) {
                tick(countdown.move, "move");
            }
            o.v = Moved;
            ++alive;
        }

        ElementOf& operator=(const ElementOf& o) {
            tick(countdown.copy, "copy");
            v = o.v;
            return *this;
        }

        ElementOf& operator=(ElementOf&& o) noexcept(NothrowMove) {
            if constexpr (!NothrowMove) {
                tick(countdown.move, "move");
            }
            v = o.v;
            o.v = Moved;
            return *this;
        }

        ~ElementOf() {
            --alive;
            *(volatile int*)&v = Dead;
        }

        friend bool operator==(const ElementOf& a, const ElementOf& b) noexcept {
            return a.v == b.v;
        }

        friend bool operator<(const ElementOf& a, const ElementOf& b) noexcept {
            return a.v < b.v;
        }
    };

    using Element = ElementOf<false>;
    using NothrowElement = ElementOf<true>;

    // The hash of an element: its int mod 4, so that keys share chains
    struct Hash {
        template<bool B>
        size_t operator()(const ElementOf<B>& e) const noexcept {
            return size_t(e.v) % 4;
        }
    };

    inline int iv(int x) {
        return x;
    }

    template<bool B>
    int iv(const ElementOf<B>& e) {
        EXPECT_NE(e.v, Dead) << "an element read after its end";
        return e.v;
    }

    // The ints of a sequence in its order, and its count walked
    template<class C>
    std::vector<int> ints(const C& c) {
        std::vector<int> out;
        for (auto& e : c) {
            out.push_back(iv(e));
        }
        return out;
    }

    // What a map or set holds, its keys to its values (0 for a set), as a
    // sorted multimap: comparable whatever the container's order
    template<class C>
    std::multimap<int, int> content(const C& c) {
        std::multimap<int, int> out;
        for (auto& e : c) {
            if constexpr (requires { e.first; }) {
                out.emplace(iv(e.first), iv(e.second));
            } else {
                out.emplace(iv(e), 0);
            }
        }
        return out;
    }

    inline const char* kind_name(unsigned kind) {
        return kind == throwing::Construct ? "construct" : kind == throwing::Copy ? "copy" : "move";
    }

    inline int& counter_of(unsigned kind) {
        return kind == throwing::Construct ? countdown.construct : kind == throwing::Copy ? countdown.copy : countdown.move;
    }

    // The loop over the kinds of throw points: `setup` makes a fresh state
    // (a tracked_ptr to a managed object), `op` runs on it, and then
    // `check(state, threw, kind)` with the countdowns disarmed. The state's
    // reset() runs last, so that what it held dies now and not in a sweep
    // during a later run. Returns the points reached.
    template<class Setup, class Op, class Check>
    int each_throw(const std::string& what, Setup setup, Op op, Check check, unsigned kinds = throwing::All) {
        int points = 0;
        for (unsigned kind : {unsigned(throwing::Construct), unsigned(throwing::Copy), unsigned(throwing::Move)}) {
            if (kinds & kind) {
                points += throwing::each_point(what, kind_name(kind), counter_of(kind), setup, op, [&](auto& s, bool threw, bool) {
                    check(s, threw, kind);
                    s.reset();
                });
            }
        }
        return points;
    }

    // A single-pass iterator over the elements of a std::vector: an input
    // range, which the containers collect or append as it comes
    template<class T>
    struct InputOf {
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const T* p = nullptr;

        reference operator*() const {
            return *p;
        }

        InputOf& operator++() {
            ++p;
            return *this;
        }

        void operator++(int) {
            ++p;
        }

        friend bool operator==(const InputOf& a, const InputOf& b) {
            return a.p == b.p;
        }
    };

    template<class T>
    std::pair<InputOf<T>, InputOf<T>> input(const std::vector<T>& v) {
        return {InputOf<T>{v.data()}, InputOf<T>{v.data() + v.size()}};
    }
}
