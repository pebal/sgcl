//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "make_tracked.h"
#include "tracked_ptr.h"
#include "mixin/mixin.h"
#include "detail/contiguous_iterator.h"
#include "slice.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace sgcl {
    // array<T, N>: the elements inline, like std::array, a value that
    // lives wherever its elements may (for tracked pointers: on a stack or
    // inside a managed object) and costs nothing beyond the elements. Not
    // an aggregate: a class with the mixins of every range of the library
    // as its bases (mixin/), which an aggregate cannot carry (a base would
    // take the first brace of {1, 2, 3}), and a constructor with the
    // braces' own syntax in place of them — N parameters of type T, one
    // per element, generated from the index sequence that is the third,
    // defaulted template parameter (array<T, N> is spelled as ever): so
    // `array<int, 3> a = {1, 2, 3}` as before, each element constructed in
    // place, a fourth an error at compile time, an element that only
    // moves moved in, `array<P, 2> p = {{1, 2}, {3, 4}}` with one level of
    // braces (a parameter of type P takes its brace). The default
    // constructor is trivial, as an aggregate's: `array<int, 3> a;` leaves
    // the elements uninitialized, `= {}` zeroes them. For a size known
    // only at run time: dynamic_array<T>.
    namespace detail {
        template<class T, size_t>
        using Same = T;
    }

    template<class T, size_t N, class Seq = std::make_index_sequence<N>>
    class array;

    template<class T, size_t N, size_t... I>
    class array<T, N, std::index_sequence<I...>>
    : public mixin::bidirectional<array<T, N>>
    , public mixin::comparable<array<T, N>>
    , public mixin::contiguous<array<T, N>>
    , public mixin::enumerable<array<T, N>>
    , public mixin::equatable<array<T, N>>
    , public mixin::ordered<array<T, N>>
    , public mixin::random_access<array<T, N>>
    , public mixin::sequence<array<T, N>> {
    public:
        using value_type = T;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using iterator = detail::ContiguousIterator<value_type>;
        using const_iterator = detail::ContiguousIterator<const value_type>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        constexpr array() = default;

        // The elements, one parameter each: {1, 2, 3}
        constexpr array(detail::Same<T, I>... init)
        : elems{std::move(init)...} {
        }

        constexpr reference at(size_type pos) {
            if (pos >= N) {
                throw out_of_range("sgcl::array::at");
            }
            return elems[pos];
        }

        constexpr const_reference at(size_type pos) const {
            if (pos >= N) {
                throw out_of_range("sgcl::array::at");
            }
            return elems[pos];
        }

        constexpr reference operator[](size_type pos) noexcept {
            return elems[pos];
        }

        constexpr const_reference operator[](size_type pos) const noexcept {
            return elems[pos];
        }

        constexpr reference front() noexcept {
            return elems[0];
        }

        constexpr const_reference front() const noexcept {
            return elems[0];
        }

        constexpr reference back() noexcept {
            return elems[N - 1];
        }

        constexpr const_reference back() const noexcept {
            return elems[N - 1];
        }

        constexpr T* data() noexcept {
            return elems;
        }

        constexpr const T* data() const noexcept {
            return elems;
        }

        // The elements as a slice without an owner (slice.h), as a C array
        // and a std::array give one: the array lives on a stack or inside
        // an object, and whoever holds that keeps the elements
        slice<T> as_slice() noexcept {
            return slice<T>(data(), data() + size());
        }

        slice<const T> as_slice() const noexcept {
            return slice<const T>(data(), data() + size());
        }

        slice<T> as_slice(size_type pos, size_type n = size_type(-1)) {
            return as_slice().subslice(pos, n);
        }

        slice<const T> as_slice(size_type pos, size_type n = size_type(-1)) const {
            return as_slice().subslice(pos, n);
        }

        operator slice<T>() noexcept {
            return as_slice();
        }

        operator slice<const T>() const noexcept {
            return as_slice();
        }

        constexpr iterator begin() noexcept {
            return iterator(elems);
        }

        constexpr const_iterator begin() const noexcept {
            return const_iterator(elems);
        }

        constexpr const_iterator cbegin() const noexcept {
            return begin();
        }

        constexpr iterator end() noexcept {
            return iterator(elems + N);
        }

        constexpr const_iterator end() const noexcept {
            return const_iterator(elems + N);
        }

        constexpr const_iterator cend() const noexcept {
            return end();
        }

        constexpr reverse_iterator rbegin() noexcept {
            return reverse_iterator(end());
        }

        constexpr const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(end());
        }

        constexpr const_reverse_iterator crbegin() const noexcept {
            return rbegin();
        }

        constexpr reverse_iterator rend() noexcept {
            return reverse_iterator(begin());
        }

        constexpr const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(begin());
        }

        constexpr const_reverse_iterator crend() const noexcept {
            return rend();
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return false;
        }

        constexpr size_type size() const noexcept {
            return N;
        }

        constexpr size_type max_size() const noexcept {
            return N;
        }

        constexpr void fill(const T& value) {
            std::fill_n(elems, N, value);
        }

        constexpr void swap(array& other) noexcept(std::is_nothrow_swappable_v<T>) {
            std::swap_ranges(elems, elems + N, other.elems);
        }

        friend constexpr void swap(array& l, array& r) noexcept(noexcept(l.swap(r))) {
            l.swap(r);
        }

    private:
        T elems[N];
    };

    // slice s(a): the elements of an array as they are, const for a const one
    template<class T, size_t N>
    slice(array<T, N>&) -> slice<T>;

    template<class T, size_t N>
    slice(const array<T, N>&) -> slice<const T>;

    // No elements: the same interface over nothing
    template<class T>
    class array<T, 0, std::index_sequence<>>
    : public mixin::bidirectional<array<T, 0>>
    , public mixin::comparable<array<T, 0>>
    , public mixin::contiguous<array<T, 0>>
    , public mixin::enumerable<array<T, 0>>
    , public mixin::equatable<array<T, 0>>
    , public mixin::ordered<array<T, 0>>
    , public mixin::random_access<array<T, 0>>
    , public mixin::sequence<array<T, 0>> {
    public:
        using value_type = T;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using iterator = detail::ContiguousIterator<value_type>;
        using const_iterator = detail::ContiguousIterator<const value_type>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        constexpr array() = default;

        constexpr reference at(size_type) {
            throw out_of_range("sgcl::array::at");
        }

        constexpr const_reference at(size_type) const {
            throw out_of_range("sgcl::array::at");
        }

        constexpr T* data() noexcept {
            return nullptr;
        }

        constexpr const T* data() const noexcept {
            return nullptr;
        }

        // The elements as a slice without an owner (slice.h), as a C array
        // and a std::array give one: the array lives on a stack or inside
        // an object, and whoever holds that keeps the elements
        slice<T> as_slice() noexcept {
            return slice<T>(data(), data() + size());
        }

        slice<const T> as_slice() const noexcept {
            return slice<const T>(data(), data() + size());
        }

        slice<T> as_slice(size_type pos, size_type n = size_type(-1)) {
            return as_slice().subslice(pos, n);
        }

        slice<const T> as_slice(size_type pos, size_type n = size_type(-1)) const {
            return as_slice().subslice(pos, n);
        }

        operator slice<T>() noexcept {
            return as_slice();
        }

        operator slice<const T>() const noexcept {
            return as_slice();
        }

        constexpr iterator begin() noexcept {
            return iterator();
        }

        constexpr const_iterator begin() const noexcept {
            return const_iterator();
        }

        constexpr const_iterator cbegin() const noexcept {
            return const_iterator();
        }

        constexpr iterator end() noexcept {
            return iterator();
        }

        constexpr const_iterator end() const noexcept {
            return const_iterator();
        }

        constexpr const_iterator cend() const noexcept {
            return const_iterator();
        }

        constexpr reverse_iterator rbegin() noexcept {
            return reverse_iterator(end());
        }

        constexpr const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(end());
        }

        constexpr reverse_iterator rend() noexcept {
            return reverse_iterator(begin());
        }

        constexpr const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(begin());
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return true;
        }

        constexpr size_type size() const noexcept {
            return 0;
        }

        constexpr size_type max_size() const noexcept {
            return 0;
        }

        constexpr void fill(const T&) noexcept {
        }

        constexpr void swap(array&) noexcept {
        }
    };

    template<class T, class... U>
    array(T, U...) -> array<std::enable_if_t<(std::is_same_v<T, U> && ...), T>, 1 + sizeof...(U)>;

    template<size_t I, class T, size_t N>
    constexpr T& get(array<T, N>& a) noexcept {
        static_assert(I < N, "index out of range");
        return a.data()[I];
    }

    template<size_t I, class T, size_t N>
    constexpr const T& get(const array<T, N>& a) noexcept {
        static_assert(I < N, "index out of range");
        return a.data()[I];
    }

    template<size_t I, class T, size_t N>
    constexpr T&& get(array<T, N>&& a) noexcept {
        static_assert(I < N, "index out of range");
        return std::move(a.data()[I]);
    }

    template<class T, size_t N>
    constexpr array<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
        return [&]<size_t... I>(std::index_sequence<I...>) {
            return array<std::remove_cv_t<T>, N>{a[I]...};
        }(std::make_index_sequence<N>());
    }

    template<class T, size_t N>
    constexpr array<std::remove_cv_t<T>, N> to_array(T (&&a)[N]) {
        return [&]<size_t... I>(std::index_sequence<I...>) {
            return array<std::remove_cv_t<T>, N>{std::move(a[I])...};
        }(std::make_index_sequence<N>());
    }
}

namespace std {
    template<class T, size_t N>
    struct tuple_size<sgcl::array<T, N>> : integral_constant<size_t, N> {
    };

    template<size_t I, class T, size_t N>
    struct tuple_element<I, sgcl::array<T, N>> {
        using type = T;
    };
}
