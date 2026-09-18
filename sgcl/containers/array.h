//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "detail/contiguous_iterator.h"
#include "detail/sequence.h"
#include "detail/synth_three_way.h"
#include "m_sequence.h"
#include "vector.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace sgcl {
    using std::dynamic_extent;

    // array<T, N>: the elements inline, like std::array, an aggregate that
    // lives wherever its elements may (for tracked pointers: on a stack or
    // inside a managed object) and costs nothing beyond the elements.
    // array<T> (N = dynamic_extent, below): a handle to a managed buffer
    // whose size is fixed when it is created.
    template<class T, size_t N = dynamic_extent>
    struct array {
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

        T elems[N];   // public, so that aggregate initialization applies

        constexpr reference at(size_type pos) {
            if (pos >= N) {
                throw std::out_of_range("sgcl::array::at");
            }
            return elems[pos];
        }

        constexpr const_reference at(size_type pos) const {
            if (pos >= N) {
                throw std::out_of_range("sgcl::array::at");
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

        // The algorithms of a sequence, the ones of m_sequence: an aggregate carries no base
        constexpr bool contains(const T& value) const { return detail::Sequence::contains(begin(), end(), value); }
        constexpr size_type index_of(const T& value) const { return detail::Sequence::index_of(begin(), end(), value); }
        constexpr size_type last_index_of(const T& value) const { return detail::Sequence::last_index_of(begin(), end(), value); }
        template<class Pred> constexpr size_type find_index(Pred pred) const { return detail::Sequence::find_index(begin(), end(), pred); }
        template<class Pred> constexpr T* find(Pred pred) noexcept { return detail::Sequence::find(begin(), end(), pred); }
        template<class Pred> constexpr const T* find(Pred pred) const noexcept { return detail::Sequence::find(begin(), end(), pred); }
        template<class Pred> constexpr bool exists(Pred pred) const { return detail::Sequence::exists(begin(), end(), pred); }
        template<class Pred> constexpr bool all(Pred pred) const { return detail::Sequence::all(begin(), end(), pred); }
        template<class Pred> constexpr size_type count_of(Pred pred) const { return detail::Sequence::count_of(begin(), end(), pred); }
        template<class F> constexpr void for_each(F f) { detail::Sequence::for_each(begin(), end(), f); }
        template<class F> constexpr void for_each(F f) const { detail::Sequence::for_each(begin(), end(), f); }
        constexpr const T& min() const { return detail::Sequence::min(begin(), end()); }
        template<class Compare> constexpr const T& min(Compare cmp) const { return detail::Sequence::min(begin(), end(), cmp); }
        constexpr const T& max() const { return detail::Sequence::max(begin(), end()); }
        template<class Compare> constexpr const T& max(Compare cmp) const { return detail::Sequence::max(begin(), end(), cmp); }
        constexpr void reverse() noexcept { detail::Sequence::reverse(begin(), end()); }
        constexpr void sort() { detail::Sequence::sort(begin(), end()); }
        template<class Compare> constexpr void sort(Compare cmp) { detail::Sequence::sort(begin(), end(), cmp); }
        constexpr bool is_sorted() const { return detail::Sequence::is_sorted(begin(), end()); }
        template<class Compare> constexpr bool is_sorted(Compare cmp) const { return detail::Sequence::is_sorted(begin(), end(), cmp); }
        constexpr bool binary_search(const T& value) const { return detail::Sequence::binary_search(begin(), end(), value); }
        template<class Compare> constexpr bool binary_search(const T& value, Compare cmp) const { return detail::Sequence::binary_search(begin(), end(), value, cmp); }
        constexpr size_type sorted_index_of(const T& value) const { return detail::Sequence::sorted_index_of(begin(), end(), value); }
        template<class Compare> constexpr size_type sorted_index_of(const T& value, Compare cmp) const { return detail::Sequence::sorted_index_of(begin(), end(), value, cmp); }
        constexpr iterator lower_bound(const T& value) { return detail::Sequence::lower_bound(begin(), end(), value); }
        constexpr const_iterator lower_bound(const T& value) const { return detail::Sequence::lower_bound(begin(), end(), value); }
        template<class Compare> constexpr iterator lower_bound(const T& value, Compare cmp) { return detail::Sequence::lower_bound(begin(), end(), value, cmp); }
        template<class Compare> constexpr const_iterator lower_bound(const T& value, Compare cmp) const { return detail::Sequence::lower_bound(begin(), end(), value, cmp); }
        constexpr iterator upper_bound(const T& value) { return detail::Sequence::upper_bound(begin(), end(), value); }
        constexpr const_iterator upper_bound(const T& value) const { return detail::Sequence::upper_bound(begin(), end(), value); }
        template<class Compare> constexpr iterator upper_bound(const T& value, Compare cmp) { return detail::Sequence::upper_bound(begin(), end(), value, cmp); }
        template<class Compare> constexpr const_iterator upper_bound(const T& value, Compare cmp) const { return detail::Sequence::upper_bound(begin(), end(), value, cmp); }

        friend constexpr void swap(array& l, array& r) noexcept(noexcept(l.swap(r))) {
            l.swap(r);
        }

        friend constexpr bool operator==(const array& l, const array& r) {
            return std::equal(l.elems, l.elems + N, r.elems);
        }

        friend constexpr auto operator<=>(const array& l, const array& r) {
            return std::lexicographical_compare_three_way(l.elems, l.elems + N, r.elems, r.elems + N, detail::synth_three_way);
        }
    };

    // No elements: an empty aggregate with the same interface.
    template<class T>
    struct array<T, 0> {
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

        constexpr reference at(size_type) {
            throw std::out_of_range("sgcl::array::at");
        }

        constexpr const_reference at(size_type) const {
            throw std::out_of_range("sgcl::array::at");
        }

        constexpr T* data() noexcept {
            return nullptr;
        }

        constexpr const T* data() const noexcept {
            return nullptr;
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

        // The algorithms of a sequence, the ones of m_sequence: an aggregate carries no base
        constexpr bool contains(const T& value) const { return detail::Sequence::contains(begin(), end(), value); }
        constexpr size_type index_of(const T& value) const { return detail::Sequence::index_of(begin(), end(), value); }
        constexpr size_type last_index_of(const T& value) const { return detail::Sequence::last_index_of(begin(), end(), value); }
        template<class Pred> constexpr size_type find_index(Pred pred) const { return detail::Sequence::find_index(begin(), end(), pred); }
        template<class Pred> constexpr T* find(Pred pred) noexcept { return detail::Sequence::find(begin(), end(), pred); }
        template<class Pred> constexpr const T* find(Pred pred) const noexcept { return detail::Sequence::find(begin(), end(), pred); }
        template<class Pred> constexpr bool exists(Pred pred) const { return detail::Sequence::exists(begin(), end(), pred); }
        template<class Pred> constexpr bool all(Pred pred) const { return detail::Sequence::all(begin(), end(), pred); }
        template<class Pred> constexpr size_type count_of(Pred pred) const { return detail::Sequence::count_of(begin(), end(), pred); }
        template<class F> constexpr void for_each(F f) { detail::Sequence::for_each(begin(), end(), f); }
        template<class F> constexpr void for_each(F f) const { detail::Sequence::for_each(begin(), end(), f); }
        constexpr const T& min() const { return detail::Sequence::min(begin(), end()); }
        template<class Compare> constexpr const T& min(Compare cmp) const { return detail::Sequence::min(begin(), end(), cmp); }
        constexpr const T& max() const { return detail::Sequence::max(begin(), end()); }
        template<class Compare> constexpr const T& max(Compare cmp) const { return detail::Sequence::max(begin(), end(), cmp); }
        constexpr void reverse() noexcept { detail::Sequence::reverse(begin(), end()); }
        constexpr void sort() { detail::Sequence::sort(begin(), end()); }
        template<class Compare> constexpr void sort(Compare cmp) { detail::Sequence::sort(begin(), end(), cmp); }
        constexpr bool is_sorted() const { return detail::Sequence::is_sorted(begin(), end()); }
        template<class Compare> constexpr bool is_sorted(Compare cmp) const { return detail::Sequence::is_sorted(begin(), end(), cmp); }
        constexpr bool binary_search(const T& value) const { return detail::Sequence::binary_search(begin(), end(), value); }
        template<class Compare> constexpr bool binary_search(const T& value, Compare cmp) const { return detail::Sequence::binary_search(begin(), end(), value, cmp); }
        constexpr size_type sorted_index_of(const T& value) const { return detail::Sequence::sorted_index_of(begin(), end(), value); }
        template<class Compare> constexpr size_type sorted_index_of(const T& value, Compare cmp) const { return detail::Sequence::sorted_index_of(begin(), end(), value, cmp); }
        constexpr iterator lower_bound(const T& value) { return detail::Sequence::lower_bound(begin(), end(), value); }
        constexpr const_iterator lower_bound(const T& value) const { return detail::Sequence::lower_bound(begin(), end(), value); }
        template<class Compare> constexpr iterator lower_bound(const T& value, Compare cmp) { return detail::Sequence::lower_bound(begin(), end(), value, cmp); }
        template<class Compare> constexpr const_iterator lower_bound(const T& value, Compare cmp) const { return detail::Sequence::lower_bound(begin(), end(), value, cmp); }
        constexpr iterator upper_bound(const T& value) { return detail::Sequence::upper_bound(begin(), end(), value); }
        constexpr const_iterator upper_bound(const T& value) const { return detail::Sequence::upper_bound(begin(), end(), value); }
        template<class Compare> constexpr iterator upper_bound(const T& value, Compare cmp) { return detail::Sequence::upper_bound(begin(), end(), value, cmp); }
        template<class Compare> constexpr const_iterator upper_bound(const T& value, Compare cmp) const { return detail::Sequence::upper_bound(begin(), end(), value, cmp); }

        constexpr void swap(array&) noexcept {
        }

        friend constexpr bool operator==(const array&, const array&) noexcept {
            return true;
        }

        friend constexpr std::strong_ordering operator<=>(const array&, const array&) noexcept {
            return std::strong_ordering::equal;
        }
    };

    template<class T, class... U>
    array(T, U...) -> array<std::enable_if_t<(std::is_same_v<T, U> && ...), T>, 1 + sizeof...(U)>;

    template<size_t I, class T, size_t N>
    constexpr T& get(array<T, N>& a) noexcept {
        static_assert(I < N, "index out of range");
        return a.elems[I];
    }

    template<size_t I, class T, size_t N>
    constexpr const T& get(const array<T, N>& a) noexcept {
        static_assert(I < N, "index out of range");
        return a.elems[I];
    }

    template<size_t I, class T, size_t N>
    constexpr T&& get(array<T, N>&& a) noexcept {
        static_assert(I < N, "index out of range");
        return std::move(a.elems[I]);
    }

    template<class T, size_t N>
    constexpr array<std::remove_cv_t<T>, N> to_array(T (&a)[N]) {
        return [&]<size_t... I>(std::index_sequence<I...>) {
            return array<std::remove_cv_t<T>, N>{{a[I]...}};
        }(std::make_index_sequence<N>());
    }

    template<class T, size_t N>
    constexpr array<std::remove_cv_t<T>, N> to_array(T (&&a)[N]) {
        return [&]<size_t... I>(std::index_sequence<I...>) {
            return array<std::remove_cv_t<T>, N>{{std::move(a[I])...}};
        }(std::make_index_sequence<N>());
    }

    // A managed array whose size is fixed when it is created: the handle is
    // one word (a tracked pointer to the first element) that lives on a
    // stack or inside a managed object, the elements live in a managed
    // buffer that the collector reclaims, with the elements, once nothing
    // refers to it. Copying an array copies the elements into a buffer of
    // its own; moving passes the buffer on. The buffer is referenced only
    // through the pointer to its first element: a pointer to an element
    // does not keep it, like with vector (README, "Containers").
    template<class T>
    struct array<T, dynamic_extent> : public m_sequence<array<T>> {   // the algorithms as members
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

        array() noexcept = default;

        explicit array(size_type count) {
            if (count) {
                _check_size(count);
                auto data = _allocate(count);
                if constexpr(detail::TypeInfo<T>::IsTracked) {
                    (void)data;
                    _size = count;   // zeroed: null tracked pointers already
                } else {
                    _guarded([&] {
                        for (size_type i = 0; i < count; ++i) {
                            _construct(data + i);
                        }
                    });
                }
            }
        }

        array(size_type count, const T& value) {
            if (count) {
                _check_size(count);
                auto data = _allocate(count);
                _guarded([&] {
                    for (size_type i = 0; i < count; ++i) {
                        _construct(data + i, value);
                    }
                });
            }
        }

        template<std::input_iterator InputIt>
        array(InputIt first, InputIt last) {
            if constexpr(std::forward_iterator<InputIt>) {
                auto count = (size_type)std::distance(first, last);
                if (count) {
                    _check_size(count);
                    auto data = _allocate(count);
                    _guarded([&] {
                        for (size_type i = 0; i < count; ++i, ++first) {
                            _construct(data + i, *first);
                        }
                    });
                }
            } else {
                // single pass: the count is not known in advance
                vector<T> collected(first, last);
                *this = array(std::make_move_iterator(collected.begin()), std::make_move_iterator(collected.end()));
            }
        }

        array(std::initializer_list<T> ilist)
        : array(ilist.begin(), ilist.end()) {
        }

        array(const array& other)
        : array(other.begin(), other.end()) {
        }

        array(array&& other) noexcept
        : _ptr(other._ptr)
        , _size(other._size) {
            other._ptr = nullptr;
            other._size = 0;
        }

        // The elements die with the handle, wherever it dies (vector.h:
        // ~vector); the buffer is freed by the collector, never destroyed.
        ~array() {
            _destroy_all();
        }

        array& operator=(const array& other) {
            if (this != &other) {
                if (size() == other.size()) {
                    std::copy(other.begin(), other.end(), begin());
                } else {
                    array copy(other);
                    swap(copy);
                }
            }
            return *this;
        }

        array& operator=(array&& other) noexcept {
            if (this != &other) {
                _destroy_all();
                _ptr = other._ptr;
                _size = other._size;
                other._ptr = nullptr;
                other._size = 0;
            }
            return *this;
        }

        array& operator=(std::initializer_list<T> ilist) {
            array fresh(ilist);
            swap(fresh);
            return *this;
        }

        reference at(size_type pos) {
            if (pos >= size()) {
                throw std::out_of_range("sgcl::array::at");
            }
            return _values()[pos];
        }

        const_reference at(size_type pos) const {
            if (pos >= size()) {
                throw std::out_of_range("sgcl::array::at");
            }
            return _values()[pos];
        }

        reference operator[](size_type pos) noexcept {
            return _values()[pos];
        }

        const_reference operator[](size_type pos) const noexcept {
            return _values()[pos];
        }

        reference front() noexcept {
            return _values()[0];
        }

        const_reference front() const noexcept {
            return _values()[0];
        }

        reference back() noexcept {
            return _values()[size() - 1];
        }

        const_reference back() const noexcept {
            return _values()[size() - 1];
        }

        T* data() noexcept {
            return _values();
        }

        const T* data() const noexcept {
            return _values();
        }

        iterator begin() noexcept {
            return iterator(_values());
        }

        const_iterator begin() const noexcept {
            return const_iterator(_values());
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        iterator end() noexcept {
            return iterator(_values() + size());
        }

        const_iterator end() const noexcept {
            return const_iterator(_values() + size());
        }

        const_iterator cend() const noexcept {
            return end();
        }

        reverse_iterator rbegin() noexcept {
            return reverse_iterator(end());
        }

        const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(end());
        }

        const_reverse_iterator crbegin() const noexcept {
            return rbegin();
        }

        reverse_iterator rend() noexcept {
            return reverse_iterator(begin());
        }

        const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(begin());
        }

        const_reverse_iterator crend() const noexcept {
            return rend();
        }

        bool empty() const noexcept {
            return size() == 0;
        }

        size_type size() const noexcept {
            return _size;
        }

        size_type max_size() const noexcept {
            return (size_type)std::numeric_limits<difference_type>::max() / sizeof(T);
        }

        void swap(array& other) noexcept {
            _ptr.swap(other._ptr);
            std::swap(_size, other._size);
        }

        friend void swap(array& l, array& r) noexcept {
            l.swap(r);
        }

        friend bool operator==(const array& l, const array& r) {
            return l.size() == r.size() && std::equal(l.begin(), l.end(), r.begin());
        }

        friend auto operator<=>(const array& l, const array& r) {
            return std::lexicographical_compare_three_way(l.begin(), l.end(), r.begin(), r.end(), detail::synth_three_way);
        }

    private:
        // Two words: the first element and the count (a size class may
        // give the buffer more room than asked; the count is the handle's).
        tracked_ptr<T> _ptr;
        size_type _size = 0;

        T* _data() const noexcept {
            return _ptr.get_plain();   // this thread's own buffer: a plain load
        }

        void _check_size(size_type n) const {
            if (n > max_size()) {
                throw std::length_error("sgcl::array");
            }
        }

        T* _values() const noexcept {
            return _data();
        }

        T* _allocate(size_type n) {
            _ptr = unique_ptr<T>(detail::Maker<T[]>::make_tracked_data(n));
            return _data();
        }

        template<class... A>
        void _construct(T* p, A&&... a) {
            detail::Maker<T>::construct(p, std::forward<A>(a)...);
            ++_size;
        }

        // A constructor's loop: an element that throws takes the ones
        // constructed before it with it (the destructor will not run;
        // the buffer is the collector's, as vector.h: _guarded).
        template<class F>
        void _guarded(F fill) {
            try {
                fill();
            } catch (...) {
                _destroy_all();
                throw;
            }
        }

        void _destroy_all() noexcept {   // vector.h: _destroy_range
            if constexpr(!std::is_trivially_destructible_v<T> && !detail::TypeInfo<T>::IsTracked) {
                auto data = _data();
                for (auto i = _size; i > 0; --i) {
                    detail::Maker<T>::destroy(data + i - 1);
                }
            }
            _size = 0;
        }
    };

    template<std::input_iterator InputIt>
    array(InputIt, InputIt) -> array<std::iter_value_t<InputIt>, dynamic_extent>;
}

namespace std {
    // The aggregate only: array<T> has no size to give at compile time
    template<class T, size_t N>
    requires (N != sgcl::dynamic_extent)
    struct tuple_size<sgcl::array<T, N>> : integral_constant<size_t, N> {
    };

    template<size_t I, class T, size_t N>
    requires (N != sgcl::dynamic_extent)
    struct tuple_element<I, sgcl::array<T, N>> {
        using type = T;
    };
}
