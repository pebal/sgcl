//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <type_traits>

namespace sgcl {
    // The result of a search that finds nothing: the position that is no position
    inline constexpr size_t npos = SIZE_MAX;

    // The concepts (c_) of the library: what a parameter of a library
    // function asks of its argument, and what the methods of a mixin ask
    // of the class that carries it. A concept of a container is nominal:
    // it asks whether the type declares the mixin (derived_from), never
    // whether the type happens to have the right members, so that only
    // what said "I am enumerable" passes and the error for what did not
    // is one line at the call. A concept of a value (c_equatable,
    // c_comparable) is also structural, because a value need not be the
    // library's: an int, a std::pair, a class with <=> of its own pass on
    // what they can do. Used in the abbreviated form as a parameter:
    // `size_t count_odd(const c_enumerable auto& r)`.
    template<class Derived> class m_enumerable;
    template<class Derived> class m_bidirectional;
    template<class Derived> class m_random_access;
    template<class Derived> class m_contiguous;
    template<class Derived> class m_equatable;
    template<class Derived> class m_comparable;
    template<class Derived> class m_ordered;
    template<class Derived> class m_sequence;
    template<class Derived> class m_lookup;

    // A class that cannot carry a base (an aggregate: array<T, N>, whose
    // braces must stay the elements') declares a mixin here instead
    template<class R, template<class> class Mixin>
    inline constexpr bool declares_mixin = false;

    namespace detail {
        template<class R, template<class> class Mixin>
        concept Declares = std::derived_from<std::remove_cvref_t<R>, Mixin<std::remove_cvref_t<R>>> || declares_mixin<std::remove_cvref_t<R>, Mixin>;

        // What the standard containers ask of an element for their own
        // == and <=>: an == (not the whole of std::equality_comparable,
        // whose != a type with a converting == cannot always form), a <
        // alone when there is no <=> (synth-three-way)
        template<class T>
        concept EqualComparable = requires(const T& a, const T& b) {
            { a == b } -> std::convertible_to<bool>;
        };

        template<class T>
        concept LessOrdered = requires(const T& a, const T& b) {
            { a < b } -> std::convertible_to<bool>;
        };
    }

    // Values
    template<class T>
    concept c_equatable = detail::Declares<T, m_equatable> || detail::EqualComparable<T>;

    template<class T>
    concept c_comparable = detail::Declares<T, m_comparable> || std::three_way_comparable<T> || detail::LessOrdered<T>;

    // Containers: what iterates, and how
    template<class R>
    concept c_enumerable = detail::Declares<R, m_enumerable>;

    template<class R>
    concept c_bidirectional = c_enumerable<R> && detail::Declares<R, m_bidirectional>;

    template<class R>
    concept c_random_access = c_bidirectional<R> && detail::Declares<R, m_random_access>;

    template<class R>
    concept c_contiguous = c_random_access<R> && detail::Declares<R, m_contiguous>;

    // Containers: what can be done with the elements
    template<class R>
    concept c_sequence = c_enumerable<R> && detail::Declares<R, m_sequence>;

    template<class R>
    concept c_ordered = c_enumerable<R> && detail::Declares<R, m_ordered> && c_comparable<std::ranges::range_value_t<R>>;

    template<class R>
    concept c_lookup = c_enumerable<R> && detail::Declares<R, m_lookup>;

    namespace detail {
        // A base that is the mixin or nothing, for a type whose own shape
        // decides (slice<const T> is not written to; range<It> is what its
        // iterator is): a distinct empty type per mixin, since two bases
        // of one type are not allowed
        template<template<class> class Mixin, class R>
        struct Absent {};

        template<bool Present, template<class> class Mixin, class R>
        using MixinIf = std::conditional_t<Present, Mixin<R>, Absent<Mixin, R>>;

        // The requirements of the mixins' methods on the elements of the
        // class that carries them, named once
        template<class R>
        concept EquatableElements = c_equatable<std::ranges::range_value_t<R>>;

        template<class R>
        concept ComparableElements = c_comparable<std::ranges::range_value_t<R>>;

        // The order the mixins' forms without a comparator use: < alone,
        // as the algorithms of <algorithm> ask (std::ranges::less would
        // ask for == too, and a type ordered by < alone is c_comparable)
        struct Less {
            template<class A, class B>
            constexpr bool operator()(const A& a, const B& b) const {
                return a < b;
            }
        };

        template<class F, class R>
        concept ElementPredicate = std::predicate<F&, std::ranges::range_reference_t<const R>>;

        template<class F, class R>
        concept ElementVisitor = std::invocable<F&, std::ranges::range_reference_t<R>>;

        template<class Cmp, class R>
        concept ElementOrder = std::strict_weak_order<Cmp&, std::ranges::range_reference_t<const R>, std::ranges::range_reference_t<const R>>;
    }
}
