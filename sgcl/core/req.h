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

    // The mixins (namespace mixin, core/mixin/): the bases a class of the
    // library names itself in, which give it its methods and declare what
    // it is. Declared here for the requirements below; defined each in
    // its header.
    namespace mixin {
        template<class Derived> class enumerable;
        template<class Derived> class bidirectional;
        template<class Derived> class random_access;
        template<class Derived> class contiguous;
        template<class Derived> class equatable;
        template<class Derived> class comparable;
        template<class Derived> class ordered;
        template<class Derived> class sequence;
        template<class Derived> class lookup;
        template<class Derived> class immutable;
    }

    namespace detail {
        template<class R, template<class> class Mixin>
        concept Declares = std::derived_from<std::remove_cvref_t<R>, Mixin<std::remove_cvref_t<R>>>;

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

    // The requirements (namespace req): what a parameter of a library
    // function asks of its argument, and what the methods of a mixin ask
    // of the class that carries them. A requirement of a container is
    // nominal: it asks whether the type declares the mixin (derived_from),
    // never whether the type happens to have the right members, so that
    // only what said "I am enumerable" passes and the error for what did
    // not is one line at the call. A requirement of a value (equatable,
    // comparable) is also structural, because a value need not be the
    // library's: an int, a std::pair, a class with <=> of its own pass on
    // what they can do. Used in the abbreviated form as a parameter:
    // `size_t count_odd(const req::enumerable auto& r)`.
    namespace req {
        // Values
        template<class T>
        concept equatable = detail::Declares<T, mixin::equatable> || detail::EqualComparable<T>;

        template<class T>
        concept comparable = detail::Declares<T, mixin::comparable> || std::three_way_comparable<T> || detail::LessOrdered<T>;

        // Containers: what iterates, and how
        template<class R>
        concept enumerable = detail::Declares<R, mixin::enumerable>;

        template<class R>
        concept bidirectional = enumerable<R> && detail::Declares<R, mixin::bidirectional>;

        template<class R>
        concept random_access = bidirectional<R> && detail::Declares<R, mixin::random_access>;

        template<class R>
        concept contiguous = random_access<R> && detail::Declares<R, mixin::contiguous>;

        // Containers: what can be done with the elements
        template<class R>
        concept sequence = enumerable<R> && detail::Declares<R, mixin::sequence>;

        template<class R>
        concept ordered = enumerable<R> && detail::Declares<R, mixin::ordered> && comparable<std::ranges::range_value_t<R>>;

        template<class R>
        concept lookup = enumerable<R> && detail::Declares<R, mixin::lookup>;

        // Containers: a value that never changes; a change is a new one
        template<class R>
        concept immutable = enumerable<R> && detail::Declares<R, mixin::immutable>;
    }

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
        concept EquatableElements = req::equatable<std::ranges::range_value_t<R>>;

        template<class R>
        concept ComparableElements = req::comparable<std::ranges::range_value_t<R>>;

        // The order the mixins' forms without a comparator use: < alone,
        // as the algorithms of <algorithm> ask (std::ranges::less would
        // ask for == too, and a type ordered by < alone is req::comparable)
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
