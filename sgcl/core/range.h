//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// range<It>: a pair of iterators as a range, half-open, for a range-for and
// std::ranges: what equal_range hands back, made iterable. range(n) and
// range(first, last) count integers instead (no step: a plain for says a
// step better, and the reverse walk is a view's job).
#pragma once

#include "mixin/mixin.h"

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

namespace sgcl {
    // The iterator of range(n) and range(first, last): an integer as a
    // random-access iterator, *i is the number, ++i the next one; a
    // difference is a subtraction, so a range of them has its size in
    // O(1). Random access to std::ranges
    // (iterator_concept); to the pre-C++20 iterator_category, which a
    // forward iterator's reference must be a true reference for, an
    // input iterator, as std::ranges::iota_view's is: *i is a value.
    template<std::integral T>
    class counting_iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using iterator_concept = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = T;

        counting_iterator() = default;

        explicit counting_iterator(T value) noexcept
        : _value(value) {
        }

        T operator*() const noexcept {
            return _value;
        }

        T operator[](difference_type n) const noexcept {
            return (T)((Wide)_value + (Wide)n);
        }

        counting_iterator& operator++() noexcept {
            ++_value;
            return *this;
        }

        counting_iterator operator++(int) noexcept {
            counting_iterator c = *this;
            ++_value;
            return c;
        }

        counting_iterator& operator--() noexcept {
            --_value;
            return *this;
        }

        counting_iterator operator--(int) noexcept {
            counting_iterator c = *this;
            --_value;
            return c;
        }

        counting_iterator& operator+=(difference_type n) noexcept {
            _value = (T)((Wide)_value + (Wide)n);
            return *this;
        }

        counting_iterator& operator-=(difference_type n) noexcept {
            _value = (T)((Wide)_value - (Wide)n);
            return *this;
        }

        friend counting_iterator operator+(counting_iterator c, difference_type n) noexcept {
            return c += n;
        }

        friend counting_iterator operator+(difference_type n, counting_iterator c) noexcept {
            return c += n;
        }

        friend counting_iterator operator-(counting_iterator c, difference_type n) noexcept {
            return c -= n;
        }

        friend difference_type operator-(counting_iterator a, counting_iterator b) noexcept {
            return (difference_type)((Wide)a._value - (Wide)b._value);
        }

        friend bool operator==(counting_iterator, counting_iterator) noexcept = default;
        friend auto operator<=>(counting_iterator, counting_iterator) noexcept = default;

    private:
        // The steps and the difference in unsigned arithmetic, modulo 2^64:
        // the ends of a 64-bit T (range(INT64_MIN, INT64_MAX)) are 2^64 - 1
        // apart, past a signed 64-bit difference, and that subtraction was
        // an overflow. Modular, the difference wraps into the ptrdiff_t,
        // size() takes it back as a size_t, and an iterator moved by it
        // lands on the other end
        using Wide = std::make_unsigned_t<std::common_type_t<T, difference_type>>;

        T _value = {};
    };

    // A range of the library over any pair of iterators (mixin/): what
    // the iterator can do, the range declares — bidirectional, random
    // access, contiguous by the iterator's concept, writable when the
    // iterator writes — so `range(v.begin(), v.end())` is how a std
    // container enters a function that asks for req::enumerable, req::ordered
    // or req::sequence.
    template<class It>
    class range
    : public detail::MixinIf<std::bidirectional_iterator<It>, mixin::bidirectional, range<It>>
    , public mixin::comparable<range<It>>
    , public detail::MixinIf<std::contiguous_iterator<It>, mixin::contiguous, range<It>>
    , public mixin::enumerable<range<It>>
    , public mixin::equatable<range<It>>
    , public mixin::ordered<range<It>>
    , public detail::MixinIf<std::random_access_iterator<It>, mixin::random_access, range<It>>
    , public detail::MixinIf<std::indirectly_writable<It, std::iter_value_t<It>>, mixin::sequence, range<It>> {
    public:
        using iterator = It;
        using value_type = typename std::iterator_traits<It>::value_type;

        range() = default;

        range(It first, It last) noexcept
        : _first(first)
        , _last(last) {
        }

        // From what equal_range hands back
        template<class Pair>
        requires requires(Pair p) { It(p.first); It(p.second); }
        range(Pair p) noexcept
        : _first(p.first)
        , _last(p.second) {
        }

        // The integers 0..last, or first..last, half-open; first > last is
        // empty (the counting forms, through the deduction guides below)
        template<std::integral T>
        requires std::same_as<It, counting_iterator<T>>
        explicit range(T last) noexcept
        : _first(T(0))
        , _last(last < T(0) ? T(0) : last) {
        }

        template<std::integral T>
        requires std::same_as<It, counting_iterator<T>>
        range(T first, T last) noexcept
        : _first(first)
        , _last(last < first ? first : last) {
        }

        bool empty() const noexcept {
            return _first == _last;
        }

        // A subtraction for a random-access iterator (by the C++20 concept:
        // the counting iterator's category says input), a walk for a forward one
        size_t size() const noexcept(_nothrow_distance()) {
            return (size_t)std::ranges::distance(_first, _last);
        }

        decltype(auto) front() const noexcept {
            return *_first;
        }

        It begin() const noexcept {
            return _first;
        }

        It end() const noexcept {
            return _last;
        }

    private:
        // Whether the distance cannot throw: the subtraction of a sized
        // iterator, or the copy, increment and comparison of the walk
        static consteval bool _nothrow_distance() noexcept {
            if constexpr(std::sized_sentinel_for<It, It>) {
                return noexcept(std::declval<const It&>() - std::declval<const It&>());
            } else {
                return std::is_nothrow_copy_constructible_v<It> && noexcept(++std::declval<It&>()) && noexcept(std::declval<const It&>() == std::declval<const It&>());
            }
        }

        It _first = {};
        It _last = {};
    };

    template<std::integral T>
    range(T last) -> range<counting_iterator<T>>;

    template<std::integral T>
    range(T first, T last) -> range<counting_iterator<T>>;

    template<class Pair>
    range(Pair p) -> range<decltype(p.first)>;
}

// A range owns nothing, so an iterator into it outlives the range object,
// as with std::ranges::subrange: the algorithms may hand one back from a
// temporary
template<class It>
inline constexpr bool std::ranges::enable_borrowed_range<sgcl::range<It>> = true;
