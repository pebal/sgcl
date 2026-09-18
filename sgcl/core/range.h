//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// range<It>: a pair of iterators as a range, half-open, for a range-for and
// std::ranges: what equal_range hands back, made iterable. range(n) and
// range(first, last) count integers instead (no step: a plain for says a
// step better, and the reverse walk is a view's job).
#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>

namespace sgcl {
    namespace detail {
        // An integer as a random-access iterator: *i is the number, ++i
        // the next one; a difference is a subtraction, so a range of
        // counters has its size in O(1). Random access to std::ranges
        // (iterator_concept); to the pre-C++20 iterator_category, which a
        // forward iterator's reference must be a true reference for, an
        // input iterator, as std::ranges::iota_view's is: *i is a value.
        template<std::integral T>
        class counter {
        public:
            using iterator_category = std::input_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = const T*;
            using reference = T;

            counter() = default;

            explicit counter(T value) noexcept
            : _value(value) {
            }

            T operator*() const noexcept {
                return _value;
            }

            T operator[](difference_type n) const noexcept {
                return (T)(_value + n);
            }

            counter& operator++() noexcept {
                ++_value;
                return *this;
            }

            counter operator++(int) noexcept {
                counter c = *this;
                ++_value;
                return c;
            }

            counter& operator--() noexcept {
                --_value;
                return *this;
            }

            counter operator--(int) noexcept {
                counter c = *this;
                --_value;
                return c;
            }

            counter& operator+=(difference_type n) noexcept {
                _value = (T)(_value + n);
                return *this;
            }

            counter& operator-=(difference_type n) noexcept {
                _value = (T)(_value - n);
                return *this;
            }

            friend counter operator+(counter c, difference_type n) noexcept {
                return c += n;
            }

            friend counter operator+(difference_type n, counter c) noexcept {
                return c += n;
            }

            friend counter operator-(counter c, difference_type n) noexcept {
                return c -= n;
            }

            friend difference_type operator-(counter a, counter b) noexcept {
                return (difference_type)a._value - (difference_type)b._value;
            }

            friend bool operator==(counter, counter) noexcept = default;
            friend auto operator<=>(counter, counter) noexcept = default;

        private:
            T _value = {};
        };
    }

    template<class It>
    class range {
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
        requires std::same_as<It, detail::counter<T>>
        explicit range(T last) noexcept
        : _first(T(0))
        , _last(last < T(0) ? T(0) : last) {
        }

        template<std::integral T>
        requires std::same_as<It, detail::counter<T>>
        range(T first, T last) noexcept
        : _first(first)
        , _last(last < first ? first : last) {
        }

        bool empty() const noexcept {
            return _first == _last;
        }

        // A subtraction for a random-access iterator (by the C++20 concept:
        // the counter's category says input), a walk for a forward one
        size_t size() const {
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
        It _first = {};
        It _last = {};
    };

    template<std::integral T>
    range(T last) -> range<detail::counter<T>>;

    template<std::integral T>
    range(T first, T last) -> range<detail::counter<T>>;

    template<class Pair>
    range(Pair p) -> range<decltype(p.first)>;
}

// A range owns nothing, so an iterator into it outlives the range object,
// as with std::ranges::subrange: the algorithms may hand one back from a
// temporary
template<class It>
inline constexpr bool std::ranges::enable_borrowed_range<sgcl::range<It>> = true;
