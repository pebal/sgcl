//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace sgcl {
    // The result of a search that finds nothing: the position that is no position
    inline constexpr size_t npos = SIZE_MAX;

    namespace detail {
        // The algorithms of a sequence over a pair of iterators, written
        // once: m_sequence gives them to the containers that can carry a
        // base, and array<T, N>, an aggregate, forwards to them by hand.
        // Constexpr, for the aggregate: <algorithm> is, since C++20.
        struct Sequence {
            template<class It, class T>
            static constexpr bool contains(It first, It last, const T& value) {
                return std::find(first, last, value) != last;
            }

            template<class It, class T>
            static constexpr size_t index_of(It first, It last, const T& value) {
                auto it = std::find(first, last, value);
                return it == last ? npos : size_t(std::distance(first, it));
            }

            template<class It, class T>
            static constexpr size_t last_index_of(It first, It last, const T& value) {
                size_t found = npos, i = 0;
                for (auto it = first; it != last; ++it, ++i) {
                    if (*it == value) {
                        found = i;
                    }
                }
                return found;
            }

            template<class It, class Pred>
            static constexpr size_t find_index(It first, It last, Pred pred) {
                auto it = std::find_if(first, last, pred);
                return it == last ? npos : size_t(std::distance(first, it));
            }

            template<class It, class Pred>
            static constexpr auto find(It first, It last, Pred pred) noexcept -> decltype(&*first) {
                auto it = std::find_if(first, last, pred);
                return it == last ? nullptr : &*it;
            }

            template<class It, class Pred>
            static constexpr bool exists(It first, It last, Pred pred) {
                return std::find_if(first, last, pred) != last;
            }

            template<class It, class Pred>
            static constexpr bool all(It first, It last, Pred pred) {
                return std::all_of(first, last, pred);
            }

            template<class It, class Pred>
            static constexpr size_t count_of(It first, It last, Pred pred) {
                return size_t(std::count_if(first, last, pred));
            }

            template<class It, class F>
            static constexpr void for_each(It first, It last, F f) {
                std::for_each(first, last, f);
            }

            template<class It>
            static constexpr decltype(auto) min(It first, It last) {
                return *std::min_element(first, last);
            }

            template<class It, class Compare>
            static constexpr decltype(auto) min(It first, It last, Compare cmp) {
                return *std::min_element(first, last, cmp);
            }

            template<class It>
            static constexpr decltype(auto) max(It first, It last) {
                return *std::max_element(first, last);
            }

            template<class It, class Compare>
            static constexpr decltype(auto) max(It first, It last, Compare cmp) {
                return *std::max_element(first, last, cmp);
            }

            template<class It, class T>
            static constexpr void fill(It first, It last, const T& value) {
                std::fill(first, last, value);
            }

            template<class It>
            static constexpr void reverse(It first, It last) noexcept {
                std::reverse(first, last);
            }

            template<class It>
            static constexpr void sort(It first, It last) {
                std::sort(first, last);
            }

            template<class It, class Compare>
            static constexpr void sort(It first, It last, Compare cmp) {
                std::sort(first, last, cmp);
            }

            template<class It>
            static constexpr bool is_sorted(It first, It last) {
                return std::is_sorted(first, last);
            }

            template<class It, class Compare>
            static constexpr bool is_sorted(It first, It last, Compare cmp) {
                return std::is_sorted(first, last, cmp);
            }

            // On a sorted sequence: whether the value is there, the first
            // position not less than it, the first greater; the sorted
            // vector is the flat map of this library, and these are its
            // lookups (O(log n) comparisons; a linked list advances n)
            template<class It, class T>
            static constexpr bool binary_search(It first, It last, const T& value) {
                return std::binary_search(first, last, value);
            }

            template<class It, class T, class Compare>
            static constexpr bool binary_search(It first, It last, const T& value, Compare cmp) {
                return std::binary_search(first, last, value, cmp);
            }

            template<class It, class T>
            static constexpr It lower_bound(It first, It last, const T& value) {
                return std::lower_bound(first, last, value);
            }

            template<class It, class T, class Compare>
            static constexpr It lower_bound(It first, It last, const T& value, Compare cmp) {
                return std::lower_bound(first, last, value, cmp);
            }

            template<class It, class T>
            static constexpr It upper_bound(It first, It last, const T& value) {
                return std::upper_bound(first, last, value);
            }

            template<class It, class T, class Compare>
            static constexpr It upper_bound(It first, It last, const T& value, Compare cmp) {
                return std::upper_bound(first, last, value, cmp);
            }

            // The position of the value in a sorted sequence, npos when
            // it is not there: the index the binary search finds
            template<class It, class T>
            static constexpr size_t sorted_index_of(It first, It last, const T& value) {
                auto it = std::lower_bound(first, last, value);
                return it != last && !(value < *it) ? size_t(std::distance(first, it)) : npos;
            }

            template<class It, class T, class Compare>
            static constexpr size_t sorted_index_of(It first, It last, const T& value, Compare cmp) {
                auto it = std::lower_bound(first, last, value, cmp);
                return it != last && !cmp(value, *it) ? size_t(std::distance(first, it)) : npos;
            }
        };
    }
}
