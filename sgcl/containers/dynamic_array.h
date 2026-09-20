//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/make_tracked.h"
#include "../core/mixin/mixin.h"
#include "../core/slice.h"
#include "../core/tracked_ptr.h"
#include "detail/contiguous_iterator.h"
#include "vector.h"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>

namespace sgcl {
    // dynamic_array<T>: an array whose size is a value, fixed when it is
    // created and never changed after — Java's `new T[n]`, C#'s `T[]`;
    // std::dynarray was to be this. A handle of two words (a tracked
    // pointer to the first element, the count) that lives on a stack or
    // inside a managed object; the elements live in a managed buffer that
    // the collector reclaims, with the elements, once nothing refers to
    // it. What it has that vector has not: the buffer never moves, so a
    // pointer, an iterator or a slice to an element is valid for as long
    // as the array is, and a field of this type says in its type that it
    // does not grow (the rings of channel and broadcast, the buckets of
    // split_list). Copying copies the elements into a buffer of its own;
    // moving passes the buffer on. The buffer is referenced only through
    // the pointer to its first element: a pointer to an element does not
    // keep it, like with vector (README, "Containers"). array<T, N> is
    // the inline one, N in the type.
    template<class T>
    class dynamic_array
    : public mixin::enumerable<dynamic_array<T>>
    , public mixin::contiguous<dynamic_array<T>>
    , public mixin::random_access<dynamic_array<T>>
    , public mixin::bidirectional<dynamic_array<T>>
    , public mixin::equatable<dynamic_array<T>>
    , public mixin::comparable<dynamic_array<T>>
    , public mixin::ordered<dynamic_array<T>>
    , public mixin::sequence<dynamic_array<T>> {
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

        dynamic_array() noexcept = default;

        explicit dynamic_array(size_type count) {
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

        dynamic_array(size_type count, const T& value) {
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
        dynamic_array(InputIt first, InputIt last) {
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
                *this = dynamic_array(std::make_move_iterator(collected.begin()), std::make_move_iterator(collected.end()));
            }
        }

        dynamic_array(std::initializer_list<T> ilist)
        : dynamic_array(ilist.begin(), ilist.end()) {
        }

        dynamic_array(const dynamic_array& other)
        : dynamic_array(other.begin(), other.end()) {
        }

        dynamic_array(dynamic_array&& other) noexcept
        : _ptr(other._ptr)
        , _size(other._size) {
            other._ptr = nullptr;
            other._size = 0;
        }

        // The elements die with the handle, wherever it dies (vector.h:
        // ~vector); the buffer is freed by the collector, never destroyed.
        ~dynamic_array() {
            _destroy_all();
        }

        dynamic_array& operator=(const dynamic_array& other) {
            if (this != &other) {
                if (size() == other.size()) {
                    std::copy(other.begin(), other.end(), begin());
                } else {
                    dynamic_array copy(other);
                    swap(copy);
                }
            }
            return *this;
        }

        dynamic_array& operator=(dynamic_array&& other) noexcept {
            if (this != &other) {
                _destroy_all();
                _ptr = other._ptr;
                _size = other._size;
                other._ptr = nullptr;
                other._size = 0;
            }
            return *this;
        }

        dynamic_array& operator=(std::initializer_list<T> ilist) {
            dynamic_array fresh(ilist);
            swap(fresh);
            return *this;
        }

        reference at(size_type pos) {
            if (pos >= size()) {
                throw std::out_of_range("sgcl::dynamic_array::at");
            }
            return _values()[pos];
        }

        const_reference at(size_type pos) const {
            if (pos >= size()) {
                throw std::out_of_range("sgcl::dynamic_array::at");
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

        // The elements as a slice that holds the buffer (slice.h): valid
        // for as long as the array is, and past it, the buffer never
        // moving
        slice<T> as_slice() noexcept {
            return slice<T>(tracked_ptr<const void>(_ptr), _values(), _values() + _size);
        }

        slice<const T> as_slice() const noexcept {
            return slice<const T>(tracked_ptr<const void>(_ptr), _values(), _values() + _size);
        }

        slice<T> as_slice(size_type pos, size_type n = size_type(-1)) {
            if (pos > _size) {
                throw std::out_of_range("sgcl::dynamic_array::as_slice");
            }
            return slice<T>(tracked_ptr<const void>(_ptr), _values() + pos, _values() + pos + std::min(n, _size - pos));
        }

        slice<const T> as_slice(size_type pos, size_type n = size_type(-1)) const {
            if (pos > _size) {
                throw std::out_of_range("sgcl::dynamic_array::as_slice");
            }
            return slice<const T>(tracked_ptr<const void>(_ptr), _values() + pos, _values() + pos + std::min(n, _size - pos));
        }

        operator slice<T>() noexcept {
            return as_slice();
        }

        operator slice<const T>() const noexcept {
            return as_slice();
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

        void swap(dynamic_array& other) noexcept {
            _ptr.swap(other._ptr);
            std::swap(_size, other._size);
        }

        friend void swap(dynamic_array& l, dynamic_array& r) noexcept {
            l.swap(r);
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
                throw std::length_error("sgcl::dynamic_array");
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
    dynamic_array(InputIt, InputIt) -> dynamic_array<std::iter_value_t<InputIt>>;
}
