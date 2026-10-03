//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Exception safety of core's sequences (DESIGN 356, 408): every operation
// that makes, copies or moves an element run with a throw at each of its
// points in turn (tests/containers/exception_safety.h), on containers of
// several sizes and with and without room, and checked against what its
// page says in Exceptions: the strong guarantee where the page gives it
// for that kind of throw, else the basic one (every element alive is the
// container's or an argument's, none read after its end).
#include "tests/containers/exception_safety.h"

#include <iterator>
#include <numeric>

using es::Element;
using es::NothrowElement;

namespace {
    std::vector<int> iota(int n, int from = 0) {
        std::vector<int> v(size_t(n > 0 ? n : 0));
        std::iota(v.begin(), v.end(), from);
        return v;
    }

    std::vector<int> concat(std::vector<int> a, const std::vector<int>& b) {
        a.insert(a.end(), b.begin(), b.end());
        return a;
    }

    // The ints with `what` inserted at `pos`
    std::vector<int> inserted(std::vector<int> a, size_t pos, const std::vector<int>& what) {
        a.insert(a.begin() + ptrdiff_t(pos), what.begin(), what.end());
        return a;
    }

    std::vector<int> erased(std::vector<int> a, size_t pos, size_t n) {
        a.erase(a.begin() + ptrdiff_t(pos), a.begin() + ptrdiff_t(pos + n));
        return a;
    }

    // A sequence of n elements 0..n-1, the arguments of an operation (an
    // element -1, a source of four elements 100..103, another container
    // of seven 200..206) and what was there before it
    template<class C>
    struct SeqState {
        using T = typename C::value_type;

        C c;
        std::optional<T> arg;
        std::vector<T> src;
        std::optional<C> other;
        std::vector<int> before;
        int base = 0;
        size_t capacity = 0;

        SeqState(int n, size_t room) {
            base = T::alive;
            if constexpr (requires { c.reserve(room); }) {
                c.reserve(room);
            }
            fill(c, 0, n);
            if constexpr (requires { c.capacity(); }) {
                capacity = c.capacity();
            }
            arg.emplace(-1);
            src.reserve(4);
            for (int k = 0; k < 4; ++k) {
                src.emplace_back(100 + k);
            }
            other.emplace();
            fill(*other, 200, 7);
            before = es::ints(c);
        }

        // n elements from..from + n - 1, in order, at the back or (a
        // forward_list) at the front
        static void fill(C& to, int from, int n) {
            if constexpr (requires { to.emplace_back(0); }) {
                for (int i = 0; i < n; ++i) {
                    to.emplace_back(from + i);
                }
            } else {
                for (int i = n - 1; i >= 0; --i) {
                    to.emplace_front(from + i);
                }
            }
        }

        // Every element alive is the container's or an argument's
        int extra() const {
            return (arg ? 1 : 0) + int(src.size()) + (other ? int(std::distance(other->begin(), other->end())) : 0);
        }

        void accounted() const {
            auto n = int(std::distance(c.begin(), c.end()));
            EXPECT_EQ(int(T::alive), base + n + extra());
            if constexpr (requires { c.size(); }) {
                EXPECT_EQ(size_t(n), c.size());
            }
        }

        // Whether `count` more elements fit in the capacity (a vector)
        bool fits(size_t count) const {
            return before.size() + count <= capacity;
        }

        void reset() {
            c.clear();
            arg.reset();
            src.clear();
            other.reset();
        }
    };

    template<class C>
    using Ptr = sgcl::tracked_ptr<SeqState<C>>;

    // Runs `op` with a throw at every point: a throw leaves the elements as
    // they were where strong(state, kind) says so, everything accounted
    // for anyway; a run without one gives expected(state)
    template<class C, class Op, class Expected, class Strong>
    void check_op(const std::string& what, int n, size_t room, Op op, Expected expected, Strong strong, unsigned kinds = throwing::All) {
        es::each_throw(what + " of " + std::to_string(n) + " (room " + std::to_string(room) + ")",
            [=] { return sgcl::make_tracked<SeqState<C>>(n, room); },
            op,
            [&](SeqState<C>& s, bool threw, unsigned kind) {
                s.accounted();
                auto now = es::ints(s.c);
                if (threw) {
                    if (strong(s, kind)) {
                        EXPECT_EQ(now, s.before) << "the page promises the elements as they were";
                    }
                } else {
                    EXPECT_EQ(now, expected(s));
                }
            }, kinds);
    }

    const auto always = [](const auto&, unsigned) { return true; };
    const auto never = [](const auto&, unsigned) { return false; };
}

// vector, an element whose move may throw (a reallocation copies) and one
// whose move cannot (a reallocation moves): the insertions at the front,
// the middle and the end, within the capacity and past it
template<class T>
static void vector_operations() {
    using V = sgcl::vector<T>;
    for (int n : {0, 1, 5}) {
        for (size_t room : {size_t(0), size_t(16)}) {
            const size_t mid = size_t(n / 2);
            // the appends: strong whatever throws
            check_op<V>("push_back(const T&)", n, room, [](auto& s) { s.c.push_back(std::as_const(*s.arg)); },
                [](auto& s) { return concat(s.before, {-1}); }, always);
            check_op<V>("push_back(T&&)", n, room, [](auto& s) { s.c.push_back(std::move(*s.arg)); },
                [](auto& s) { return concat(s.before, {-1}); }, always);
            check_op<V>("emplace_back", n, room, [](auto& s) { s.c.emplace_back(-1); },
                [](auto& s) { return concat(s.before, {-1}); }, always);
            // the insertions: strong when reallocating, and within the
            // capacity when the new element (or its temporary) is what
            // throws; a move or an assignment shifting the tail leaves
            // the basic guarantee
            for (size_t pos : {size_t(0), mid, size_t(n)}) {
                const std::string at = " at " + std::to_string(pos);
                auto one_strong = [](const auto& s, unsigned kind) { return !s.fits(1) || kind != throwing::Move; };
                check_op<V>("insert(pos, const T&)" + at, n, room, [pos](auto& s) { s.c.insert(s.c.begin() + ptrdiff_t(pos), std::as_const(*s.arg)); },
                    [pos](auto& s) { return inserted(s.before, pos, {-1}); }, one_strong);
                check_op<V>("insert(pos, T&&)" + at, n, room, [pos](auto& s) { s.c.insert(s.c.begin() + ptrdiff_t(pos), std::move(*s.arg)); },
                    [pos](auto& s) { return inserted(s.before, pos, {-1}); }, one_strong);
                check_op<V>("emplace(pos)" + at, n, room, [pos](auto& s) { s.c.emplace(s.c.begin() + ptrdiff_t(pos), -1); },
                    [pos](auto& s) { return inserted(s.before, pos, {-1}); }, one_strong);
                auto many_strong = [](const auto& s, unsigned kind) { return !s.fits(3) || kind == throwing::Construct; };
                check_op<V>("insert(pos, 3, const T&)" + at, n, room, [pos](auto& s) { s.c.insert(s.c.begin() + ptrdiff_t(pos), 3, std::as_const(*s.arg)); },
                    [pos](auto& s) { return inserted(s.before, pos, {-1, -1, -1}); }, many_strong);
                check_op<V>("insert(pos, first, last)" + at, n, room, [pos](auto& s) { s.c.insert(s.c.begin() + ptrdiff_t(pos), s.src.begin(), s.src.end()); },
                    [pos](auto& s) { return inserted(s.before, pos, {100, 101, 102, 103}); }, many_strong);
                check_op<V>("insert(pos, single-pass)" + at, n, room, [pos](auto& s) {
                        auto [first, last] = es::input(s.src);
                        s.c.insert(s.c.begin() + ptrdiff_t(pos), first, last);
                    }, [pos](auto& s) { return inserted(s.before, pos, {100, 101, 102, 103}); }, never);
            }
            if (n) {
                check_op<V>("erase(mid)", n, room, [mid](auto& s) { s.c.erase(s.c.begin() + ptrdiff_t(mid)); },
                    [mid](auto& s) { return erased(s.before, mid, 1); }, never);
                check_op<V>("erase(first, last)", n, room, [](auto& s) { s.c.erase(s.c.begin(), s.c.begin() + 1); },
                    [](auto& s) { return erased(s.before, 0, 1); }, never);
                check_op<V>("erase(v, v[mid])", n, room, [mid](auto& s) { sgcl::erase(s.c, s.c[mid]); },
                    [mid](auto& s) { return erased(s.before, mid, 1); }, never);
            }
            check_op<V>("erase_if", n, room, [](auto& s) { sgcl::erase_if(s.c, [](const T& e) { return e.v % 2 == 0; }); },
                [](auto& s) { std::vector<int> out; for (int x : s.before) { if (x % 2) { out.push_back(x); } } return out; }, never);
            // the sizes: resize holds the elements it held, reserve and
            // shrink_to_fit are as they were
            check_op<V>("resize(n + 3)", n, room, [](auto& s) { s.c.resize(s.before.size() + 3); },
                [](auto& s) { return concat(s.before, {0, 0, 0}); }, always);
            check_op<V>("resize(n + 3, value)", n, room, [](auto& s) { s.c.resize(s.before.size() + 3, *s.arg); },
                [](auto& s) { return concat(s.before, {-1, -1, -1}); }, always);
            check_op<V>("resize(n + 40)", n, room, [](auto& s) { s.c.resize(s.before.size() + 40, *s.arg); },
                [](auto& s) { return concat(s.before, std::vector<int>(40, -1)); }, always);
            check_op<V>("reserve(64)", n, room, [](auto& s) { s.c.reserve(64); },
                [](auto& s) { return s.before; }, always);
            check_op<V>("shrink_to_fit", n, room, [](auto& s) { s.c.shrink_to_fit(); },
                [](auto& s) { return s.before; }, always);
            // the replacements: strong with a fresh buffer, else basic
            auto replace_strong = [](size_t count) {
                return [count](const auto& s, unsigned) { return count > s.capacity; };
            };
            check_op<V>("assign(3, value)", n, room, [](auto& s) { s.c.assign(3, *s.arg); },
                [](auto&) { return std::vector<int>{-1, -1, -1}; }, replace_strong(3));
            check_op<V>("assign(first, last)", n, room, [](auto& s) { s.c.assign(s.src.begin(), s.src.end()); },
                [](auto&) { return iota(4, 100); }, replace_strong(4));
            check_op<V>("assign(single-pass)", n, room, [](auto& s) { auto [first, last] = es::input(s.src); s.c.assign(first, last); },
                [](auto&) { return iota(4, 100); }, never);
            check_op<V>("operator=(const vector&)", n, room, [](auto& s) { s.c = *s.other; },
                [](auto&) { return iota(7, 200); }, replace_strong(7));
        }
    }
    // the constructors leave nothing alive when they throw
    for (int n : {0, 5}) {
        check_op<V>("vector(count)", n, 0, [](auto& s) { s.c = V(3); }, [](auto&) { return std::vector<int>{0, 0, 0}; }, never);
        check_op<V>("vector(count, value)", n, 0, [](auto& s) { s.c = V(3, *s.arg); }, [](auto&) { return std::vector<int>{-1, -1, -1}; }, never);
        check_op<V>("vector(first, last)", n, 0, [](auto& s) { s.c = V(s.src.begin(), s.src.end()); }, [](auto&) { return iota(4, 100); }, never);
        check_op<V>("vector(single-pass)", n, 0, [](auto& s) { auto [first, last] = es::input(s.src); s.c = V(first, last); }, [](auto&) { return iota(4, 100); }, never);
        check_op<V>("vector(const vector&)", n, 0, [](auto& s) { V copy(*s.other); s.c.swap(copy); }, [](auto&) { return iota(7, 200); }, never);
    }
}

TEST(ExceptionSafety_Test, Vector) {
    vector_operations<Element>();
    vector_operations<NothrowElement>();
}

// deque: the pushes and emplaces at either end are strong; an insertion
// strong when the construction (or copy) of an inserted element throws,
// basic when the moves putting it in place do; the erasures and the
// assignments basic; resize strong. Across a block boundary too.
template<class T>
static void deque_operations() {
    using D = sgcl::deque<T>;
    for (int n : {0, 1, 5, 1030}) {
        const size_t mid = size_t(n / 2);
        check_op<D>("push_back(const T&)", n, 0, [](auto& s) { s.c.push_back(std::as_const(*s.arg)); },
            [](auto& s) { return concat(s.before, {-1}); }, always);
        check_op<D>("push_back(T&&)", n, 0, [](auto& s) { s.c.push_back(std::move(*s.arg)); },
            [](auto& s) { return concat(s.before, {-1}); }, always);
        check_op<D>("emplace_back", n, 0, [](auto& s) { s.c.emplace_back(-1); },
            [](auto& s) { return concat(s.before, {-1}); }, always);
        check_op<D>("push_front(const T&)", n, 0, [](auto& s) { s.c.push_front(std::as_const(*s.arg)); },
            [](auto& s) { return inserted(s.before, 0, {-1}); }, always);
        check_op<D>("push_front(T&&)", n, 0, [](auto& s) { s.c.push_front(std::move(*s.arg)); },
            [](auto& s) { return inserted(s.before, 0, {-1}); }, always);
        check_op<D>("emplace_front", n, 0, [](auto& s) { s.c.emplace_front(-1); },
            [](auto& s) { return inserted(s.before, 0, {-1}); }, always);
        const auto made_strong = [](const auto&, unsigned kind) { return kind != throwing::Move; };
        for (size_t pos : {size_t(0), size_t(1), mid, size_t(n)}) {
            if (pos > size_t(n)) {
                continue;
            }
            const std::string at = " at " + std::to_string(pos);
            check_op<D>("insert(pos, const T&)" + at, n, 0, [pos](auto& s) { s.c.insert(s.c.begin() + ptrdiff_t(pos), std::as_const(*s.arg)); },
                [pos](auto& s) { return inserted(s.before, pos, {-1}); }, made_strong);
            check_op<D>("insert(pos, T&&)" + at, n, 0, [pos](auto& s) { s.c.insert(s.c.begin() + ptrdiff_t(pos), std::move(*s.arg)); },
                [pos](auto& s) { return inserted(s.before, pos, {-1}); }, made_strong);
            check_op<D>("emplace(pos)" + at, n, 0, [pos](auto& s) { s.c.emplace(s.c.begin() + ptrdiff_t(pos), -1); },
                [pos](auto& s) { return inserted(s.before, pos, {-1}); }, made_strong);
            check_op<D>("insert(pos, 3, const T&)" + at, n, 0, [pos](auto& s) { s.c.insert(s.c.begin() + ptrdiff_t(pos), 3, std::as_const(*s.arg)); },
                [pos](auto& s) { return inserted(s.before, pos, {-1, -1, -1}); }, made_strong);
            check_op<D>("insert(pos, first, last)" + at, n, 0, [pos](auto& s) { s.c.insert(s.c.begin() + ptrdiff_t(pos), s.src.begin(), s.src.end()); },
                [pos](auto& s) { return inserted(s.before, pos, {100, 101, 102, 103}); }, made_strong);
            check_op<D>("insert(pos, single-pass)" + at, n, 0, [pos](auto& s) {
                    auto [first, last] = es::input(s.src);
                    s.c.insert(s.c.begin() + ptrdiff_t(pos), first, last);
                }, [pos](auto& s) { return inserted(s.before, pos, {100, 101, 102, 103}); }, [](const auto&, unsigned kind) { return kind == throwing::Copy; });
            if (pos < size_t(n)) {
                check_op<D>("erase(pos)" + at, n, 0, [pos](auto& s) { s.c.erase(s.c.begin() + ptrdiff_t(pos)); },
                    [pos](auto& s) { return erased(s.before, pos, 1); }, never);
                check_op<D>("erase(d, d[pos])" + at, n, 0, [pos](auto& s) { sgcl::erase(s.c, s.c[pos]); },
                    [pos](auto& s) { return erased(s.before, pos, 1); }, never);
            }
        }
        if (n) {
            check_op<D>("erase(first, last)", n, 0, [mid](auto& s) { s.c.erase(s.c.begin() + ptrdiff_t(mid / 2), s.c.begin() + ptrdiff_t(mid + 1)); },
                [mid](auto& s) { return erased(s.before, mid / 2, mid + 1 - mid / 2); }, never);
        }
        check_op<D>("erase_if", n, 0, [](auto& s) { sgcl::erase_if(s.c, [](const T& e) { return e.v % 2 == 0; }); },
            [](auto& s) { std::vector<int> out; for (int x : s.before) { if (x % 2) { out.push_back(x); } } return out; }, never);
        check_op<D>("resize(n + 3)", n, 0, [](auto& s) { s.c.resize(s.before.size() + 3); },
            [](auto& s) { return concat(s.before, {0, 0, 0}); }, always);
        check_op<D>("resize(n + 1100, value)", n, 0, [](auto& s) { s.c.resize(s.before.size() + 1100, *s.arg); },
            [](auto& s) { return concat(s.before, std::vector<int>(1100, -1)); }, always);
        check_op<D>("assign(3, value)", n, 0, [](auto& s) { s.c.assign(3, *s.arg); },
            [](auto&) { return std::vector<int>{-1, -1, -1}; }, never);
        check_op<D>("assign(first, last)", n, 0, [](auto& s) { s.c.assign(s.src.begin(), s.src.end()); },
            [](auto&) { return iota(4, 100); }, never);
        check_op<D>("assign(single-pass)", n, 0, [](auto& s) { auto [first, last] = es::input(s.src); s.c.assign(first, last); },
            [](auto&) { return iota(4, 100); }, never);
        check_op<D>("operator=(const deque&)", n, 0, [](auto& s) { s.c = *s.other; },
            [](auto&) { return iota(7, 200); }, never);
    }
    for (int n : {0, 5}) {
        check_op<D>("deque(count)", n, 0, [](auto& s) { s.c = D(3); }, [](auto&) { return std::vector<int>{0, 0, 0}; }, never);
        check_op<D>("deque(count, value)", n, 0, [](auto& s) { s.c = D(3, *s.arg); }, [](auto&) { return std::vector<int>{-1, -1, -1}; }, never);
        check_op<D>("deque(first, last)", n, 0, [](auto& s) { s.c = D(s.src.begin(), s.src.end()); }, [](auto&) { return iota(4, 100); }, never);
        check_op<D>("deque(single-pass)", n, 0, [](auto& s) { auto [first, last] = es::input(s.src); s.c = D(first, last); }, [](auto&) { return iota(4, 100); }, never);
        check_op<D>("deque(const deque&)", n, 0, [](auto& s) { D copy(*s.other); s.c.swap(copy); }, [](auto&) { return iota(7, 200); }, never);
    }
}

TEST(ExceptionSafety_Test, Deque) {
    deque_operations<Element>();
    deque_operations<NothrowElement>();
}

// list: every insertion is strong (the new nodes are made in a chain
// first and linked at once), resize too; assign and the copy assignment
// basic (the elements assigned over keep their new values)
template<class T>
static void list_operations() {
    using L = sgcl::list<T>;
    for (int n : {0, 1, 5}) {
        const size_t mid = size_t(n / 2);
        check_op<L>("push_back(const T&)", n, 0, [](auto& s) { s.c.push_back(std::as_const(*s.arg)); },
            [](auto& s) { return concat(s.before, {-1}); }, always);
        check_op<L>("push_back(T&&)", n, 0, [](auto& s) { s.c.push_back(std::move(*s.arg)); },
            [](auto& s) { return concat(s.before, {-1}); }, always);
        check_op<L>("emplace_back", n, 0, [](auto& s) { s.c.emplace_back(-1); },
            [](auto& s) { return concat(s.before, {-1}); }, always);
        check_op<L>("push_front(const T&)", n, 0, [](auto& s) { s.c.push_front(std::as_const(*s.arg)); },
            [](auto& s) { return inserted(s.before, 0, {-1}); }, always);
        check_op<L>("push_front(T&&)", n, 0, [](auto& s) { s.c.push_front(std::move(*s.arg)); },
            [](auto& s) { return inserted(s.before, 0, {-1}); }, always);
        check_op<L>("emplace_front", n, 0, [](auto& s) { s.c.emplace_front(-1); },
            [](auto& s) { return inserted(s.before, 0, {-1}); }, always);
        for (size_t pos : {size_t(0), mid, size_t(n)}) {
            const std::string at = " at " + std::to_string(pos);
            auto where = [pos](auto& s) { return std::next(s.c.begin(), ptrdiff_t(pos)); };
            check_op<L>("insert(pos, const T&)" + at, n, 0, [where](auto& s) { s.c.insert(where(s), std::as_const(*s.arg)); },
                [pos](auto& s) { return inserted(s.before, pos, {-1}); }, always);
            check_op<L>("insert(pos, T&&)" + at, n, 0, [where](auto& s) { s.c.insert(where(s), std::move(*s.arg)); },
                [pos](auto& s) { return inserted(s.before, pos, {-1}); }, always);
            check_op<L>("emplace(pos)" + at, n, 0, [where](auto& s) { s.c.emplace(where(s), -1); },
                [pos](auto& s) { return inserted(s.before, pos, {-1}); }, always);
            check_op<L>("insert(pos, 3, const T&)" + at, n, 0, [where](auto& s) { s.c.insert(where(s), 3, std::as_const(*s.arg)); },
                [pos](auto& s) { return inserted(s.before, pos, {-1, -1, -1}); }, always);
            check_op<L>("insert(pos, first, last)" + at, n, 0, [where](auto& s) { s.c.insert(where(s), s.src.begin(), s.src.end()); },
                [pos](auto& s) { return inserted(s.before, pos, {100, 101, 102, 103}); }, always);
            check_op<L>("insert(pos, single-pass)" + at, n, 0, [where](auto& s) {
                    auto [first, last] = es::input(s.src);
                    s.c.insert(where(s), first, last);
                }, [pos](auto& s) { return inserted(s.before, pos, {100, 101, 102, 103}); }, always);
        }
        check_op<L>("resize(n + 3)", n, 0, [](auto& s) { s.c.resize(s.before.size() + 3); },
            [](auto& s) { return concat(s.before, {0, 0, 0}); }, always);
        check_op<L>("resize(n + 3, value)", n, 0, [](auto& s) { s.c.resize(s.before.size() + 3, *s.arg); },
            [](auto& s) { return concat(s.before, {-1, -1, -1}); }, always);
        check_op<L>("assign(3, value)", n, 0, [](auto& s) { s.c.assign(3, *s.arg); },
            [](auto&) { return std::vector<int>{-1, -1, -1}; }, never);
        check_op<L>("assign(first, last)", n, 0, [](auto& s) { s.c.assign(s.src.begin(), s.src.end()); },
            [](auto&) { return iota(4, 100); }, never);
        check_op<L>("assign(single-pass)", n, 0, [](auto& s) { auto [first, last] = es::input(s.src); s.c.assign(first, last); },
            [](auto&) { return iota(4, 100); }, never);
        check_op<L>("operator=(const list&)", n, 0, [](auto& s) { s.c = *s.other; },
            [](auto&) { return iota(7, 200); }, never);
    }
    for (int n : {0, 5}) {
        check_op<L>("list(count)", n, 0, [](auto& s) { s.c = L(3); }, [](auto&) { return std::vector<int>{0, 0, 0}; }, never);
        check_op<L>("list(count, value)", n, 0, [](auto& s) { s.c = L(3, *s.arg); }, [](auto&) { return std::vector<int>{-1, -1, -1}; }, never);
        check_op<L>("list(first, last)", n, 0, [](auto& s) { s.c = L(s.src.begin(), s.src.end()); }, [](auto&) { return iota(4, 100); }, never);
        check_op<L>("list(single-pass)", n, 0, [](auto& s) { auto [first, last] = es::input(s.src); s.c = L(first, last); }, [](auto&) { return iota(4, 100); }, never);
        check_op<L>("list(const list&)", n, 0, [](auto& s) { L copy(*s.other); s.c.swap(copy); }, [](auto&) { return iota(7, 200); }, never);
    }
}

TEST(ExceptionSafety_Test, List) {
    list_operations<Element>();
    list_operations<NothrowElement>();
}

// forward_list: as list, after a position
template<class T>
static void forward_list_operations() {
    using F = sgcl::forward_list<T>;
    for (int n : {0, 1, 5}) {
        const size_t mid = size_t(n / 2);
        check_op<F>("push_front(const T&)", n, 0, [](auto& s) { s.c.push_front(std::as_const(*s.arg)); },
            [](auto& s) { return inserted(s.before, 0, {-1}); }, always);
        check_op<F>("push_front(T&&)", n, 0, [](auto& s) { s.c.push_front(std::move(*s.arg)); },
            [](auto& s) { return inserted(s.before, 0, {-1}); }, always);
        check_op<F>("emplace_front", n, 0, [](auto& s) { s.c.emplace_front(-1); },
            [](auto& s) { return inserted(s.before, 0, {-1}); }, always);
        for (size_t pos : {size_t(0), mid, size_t(n)}) {
            const std::string at = " after " + std::to_string(pos);
            auto after = [pos](auto& s) { return std::next(s.c.before_begin(), ptrdiff_t(pos)); };
            check_op<F>("insert_after(pos, const T&)" + at, n, 0, [after](auto& s) { s.c.insert_after(after(s), std::as_const(*s.arg)); },
                [pos](auto& s) { return inserted(s.before, pos, {-1}); }, always);
            check_op<F>("insert_after(pos, T&&)" + at, n, 0, [after](auto& s) { s.c.insert_after(after(s), std::move(*s.arg)); },
                [pos](auto& s) { return inserted(s.before, pos, {-1}); }, always);
            check_op<F>("emplace_after(pos)" + at, n, 0, [after](auto& s) { s.c.emplace_after(after(s), -1); },
                [pos](auto& s) { return inserted(s.before, pos, {-1}); }, always);
            check_op<F>("insert_after(pos, 3, const T&)" + at, n, 0, [after](auto& s) { s.c.insert_after(after(s), 3, std::as_const(*s.arg)); },
                [pos](auto& s) { return inserted(s.before, pos, {-1, -1, -1}); }, always);
            check_op<F>("insert_after(pos, first, last)" + at, n, 0, [after](auto& s) { s.c.insert_after(after(s), s.src.begin(), s.src.end()); },
                [pos](auto& s) { return inserted(s.before, pos, {100, 101, 102, 103}); }, always);
            check_op<F>("insert_after(pos, single-pass)" + at, n, 0, [after](auto& s) {
                    auto [first, last] = es::input(s.src);
                    s.c.insert_after(after(s), first, last);
                }, [pos](auto& s) { return inserted(s.before, pos, {100, 101, 102, 103}); }, always);
        }
        check_op<F>("resize(n + 3)", n, 0, [](auto& s) { s.c.resize(s.before.size() + 3); },
            [](auto& s) { return concat(s.before, {0, 0, 0}); }, always);
        check_op<F>("resize(n + 3, value)", n, 0, [](auto& s) { s.c.resize(s.before.size() + 3, *s.arg); },
            [](auto& s) { return concat(s.before, {-1, -1, -1}); }, always);
        check_op<F>("assign(3, value)", n, 0, [](auto& s) { s.c.assign(3, *s.arg); },
            [](auto&) { return std::vector<int>{-1, -1, -1}; }, never);
        check_op<F>("assign(first, last)", n, 0, [](auto& s) { s.c.assign(s.src.begin(), s.src.end()); },
            [](auto&) { return iota(4, 100); }, never);
        check_op<F>("assign(single-pass)", n, 0, [](auto& s) { auto [first, last] = es::input(s.src); s.c.assign(first, last); },
            [](auto&) { return iota(4, 100); }, never);
        check_op<F>("operator=(const forward_list&)", n, 0, [](auto& s) { s.c = *s.other; },
            [](auto&) { return iota(7, 200); }, never);
    }
    for (int n : {0, 5}) {
        check_op<F>("forward_list(count)", n, 0, [](auto& s) { s.c = F(3); }, [](auto&) { return std::vector<int>{0, 0, 0}; }, never);
        check_op<F>("forward_list(count, value)", n, 0, [](auto& s) { s.c = F(3, *s.arg); }, [](auto&) { return std::vector<int>{-1, -1, -1}; }, never);
        check_op<F>("forward_list(first, last)", n, 0, [](auto& s) { s.c = F(s.src.begin(), s.src.end()); }, [](auto&) { return iota(4, 100); }, never);
        check_op<F>("forward_list(single-pass)", n, 0, [](auto& s) { auto [first, last] = es::input(s.src); s.c = F(first, last); }, [](auto&) { return iota(4, 100); }, never);
        check_op<F>("forward_list(const forward_list&)", n, 0, [](auto& s) { F copy(*s.other); s.c.swap(copy); }, [](auto&) { return iota(7, 200); }, never);
    }
}

TEST(ExceptionSafety_Test, ForwardList) {
    forward_list_operations<Element>();
    forward_list_operations<NothrowElement>();
}

namespace {
    // A fixed-size container (dynamic_array, array) of five elements 0..4,
    // the same arguments as SeqState's and another container of five
    // 200..204 and one of seven
    template<class C>
    struct FixedState {
        using T = typename C::value_type;

        int base = T::alive;     // first: before an array's elements are made
        std::optional<C> held;   // the container, destroyed by reset()
        C& c = held.emplace();
        std::optional<T> arg;
        std::vector<T> src;
        std::optional<C> other;
        std::optional<sgcl::dynamic_array<T>> longer;
        std::vector<int> before;

        FixedState() {
            c = C{0, 1, 2, 3, 4};
            other.emplace(C{200, 201, 202, 203, 204});
            arg.emplace(-1);
            src.reserve(4);
            for (int k = 0; k < 4; ++k) {
                src.emplace_back(100 + k);
            }
            longer.emplace(std::initializer_list<T>{200, 201, 202, 203, 204, 205, 206});
            before = es::ints(c);
        }

        int extra() const {
            return (arg ? 1 : 0) + int(src.size()) + (other ? int(other->size()) : 0) + (longer ? int(longer->size()) : 0);
        }

        void accounted() const {
            EXPECT_EQ(int(T::alive), base + int(std::distance(c.begin(), c.end())) + extra());
        }

        void reset() {
            held.reset();
            arg.reset();
            src.clear();
            other.reset();
            longer.reset();
        }
    };

    template<class C, class Op, class Expected, class Strong>
    void check_fixed(const std::string& what, Op op, Expected expected, Strong strong) {
        es::each_throw(what, [] { return sgcl::make_tracked<FixedState<C>>(); }, op, [&](FixedState<C>& s, bool threw, unsigned kind) {
            s.accounted();
            auto now = es::ints(s.c);
            if (threw) {
                if (strong(s, kind)) {
                    EXPECT_EQ(now, s.before) << "the page promises the elements as they were";
                }
            } else {
                EXPECT_EQ(now, expected(s));
            }
        });
    }
}

// dynamic_array: a copy of another size and a list are built in a fresh
// buffer (strong); a copy of the same size is assigned in place and fill
// too (basic); the constructors leave nothing alive
template<class T>
static void dynamic_array_operations() {
    using A = sgcl::dynamic_array<T>;
    check_fixed<A>("dynamic_array: operator=(const dynamic_array&) of the same size", [](auto& s) { s.c = *s.other; },
        [](auto&) { return iota(5, 200); }, never);
    check_fixed<A>("dynamic_array: operator=(const dynamic_array&) of another size", [](auto& s) { s.c = *s.longer; },
        [](auto&) { return iota(7, 200); }, always);
    check_fixed<A>("dynamic_array: operator=(initializer_list)", [](auto& s) { s.c = {T(7), T(8)}; },
        [](auto&) { return std::vector<int>{7, 8}; }, always);
    check_fixed<A>("dynamic_array: fill", [](auto& s) { s.c.fill(*s.arg); },
        [](auto&) { return std::vector<int>(5, -1); }, never);
    check_fixed<A>("dynamic_array: fill with its own element", [](auto& s) { s.c.fill(s.c[3]); },
        [](auto&) { return std::vector<int>(5, 3); }, never);
    check_fixed<A>("dynamic_array(count)", [](auto& s) { s.c = A(3); }, [](auto&) { return std::vector<int>{0, 0, 0}; }, always);
    check_fixed<A>("dynamic_array(count, value)", [](auto& s) { s.c = A(3, *s.arg); }, [](auto&) { return std::vector<int>{-1, -1, -1}; }, always);
    check_fixed<A>("dynamic_array(first, last)", [](auto& s) { s.c = A(s.src.begin(), s.src.end()); }, [](auto&) { return iota(4, 100); }, always);
    check_fixed<A>("dynamic_array(single-pass)", [](auto& s) { auto [first, last] = es::input(s.src); s.c = A(first, last); }, [](auto&) { return iota(4, 100); }, always);
    check_fixed<A>("dynamic_array(const dynamic_array&)", [](auto& s) { A copy(*s.longer); s.c.swap(copy); }, [](auto&) { return iota(7, 200); }, always);
}

TEST(ExceptionSafety_Test, DynamicArray) {
    dynamic_array_operations<Element>();
    dynamic_array_operations<NothrowElement>();
}

// array: fill and swap leave the elements before the one that threw done
// and the rest as they were (basic), a copy assignment the same; the
// construction from a list leaves nothing alive
template<class T>
static void array_operations() {
    using A = sgcl::array<T, 5>;
    check_fixed<A>("array: fill", [](auto& s) { s.c.fill(*s.arg); }, [](auto&) { return std::vector<int>(5, -1); }, never);
    check_fixed<A>("array: swap", [](auto& s) { s.c.swap(*s.other); }, [](auto&) { return iota(5, 200); }, never);
    check_fixed<A>("array: swap(a, b)", [](auto& s) { swap(s.c, *s.other); }, [](auto&) { return iota(5, 200); }, never);
    check_fixed<A>("array: operator=(const array&)", [](auto& s) { s.c = *s.other; }, [](auto&) { return iota(5, 200); }, never);
    check_fixed<A>("array: operator=(array&&)", [](auto& s) { s.c = std::move(*s.other); }, [](auto&) { return iota(5, 200); }, never);
    check_fixed<A>("array(list)", [](auto&) {
            A made = {T(7), T(8), T(9)};   // the rest value-initialized
            EXPECT_EQ(es::ints(made), (std::vector<int>{7, 8, 9, 0, 0}));
        }, [](auto& s) { return s.before; }, always);
}

TEST(ExceptionSafety_Test, Array) {
    array_operations<Element>();
    array_operations<NothrowElement>();
}

// The mixins' sorts, reverse and fill on the random-access sequences, an
// element whose move may throw: basic (std's algorithms), every element
// accounted for, none read after its end
template<class C>
static void mixin_operations(const std::string& name) {
    for (int n : {0, 1, 5, 40}) {
        auto setup_reversed = [](auto& s) { std::reverse(s.c.begin(), s.c.end()); };
        check_op<C>(name + ": sort", n, 0, [=](auto& s) { setup_reversed(s); s.c.sort(); },
            [](auto& s) { return s.before; }, never);
        check_op<C>(name + ": stable_sort", n, 0, [=](auto& s) { setup_reversed(s); s.c.stable_sort(); },
            [](auto& s) { return s.before; }, never);
        check_op<C>(name + ": sort_by", n, 0, [](auto& s) { s.c.sort_by([](const auto& e) { return -e.v; }); },
            [](auto& s) { auto r = s.before; std::reverse(r.begin(), r.end()); return r; }, never);
        check_op<C>(name + ": reverse", n, 0, [](auto& s) { s.c.reverse(); },
            [](auto& s) { auto r = s.before; std::reverse(r.begin(), r.end()); return r; }, never);
        check_op<C>(name + ": fill", n, 0, [](auto& s) { s.c.fill(*s.arg); },
            [](auto& s) { return std::vector<int>(s.before.size(), -1); }, never);
    }
}

TEST(ExceptionSafety_Test, TheMixinsOfASequence) {
    mixin_operations<sgcl::vector<Element>>("vector");
    mixin_operations<sgcl::deque<Element>>("deque");
}
