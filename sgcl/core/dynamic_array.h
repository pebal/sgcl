//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "detail/bytes.h"
#include "detail/contiguous_iterator.h"
#include "make_tracked.h"
#include "mixin/mixin.h"
#include "slice.h"
#include "tracked_ptr.h"
#include "vector.h"

#include <ranges>
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
    : public mixin::bidirectional<dynamic_array<T>>
    , public mixin::comparable<dynamic_array<T>>
    , public mixin::contiguous<dynamic_array<T>>
    , public mixin::enumerable<dynamic_array<T>>
    , public mixin::equatable<dynamic_array<T>>
    , public mixin::ordered<dynamic_array<T>>
    , public mixin::random_access<dynamic_array<T>>
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
                } else if constexpr(_zero_is_value()) {
                    detail::fill_bytes(static_cast<void*>(data), 0, count * sizeof(T));
                    _size = count;
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

        // From a range of what the elements are made of (the pieces of a
        // string, a view, another container), as C++23's from_range and
        // every other sequence: a dynamic_array is copied by its own
        // constructor, not this one
        template<std::ranges::input_range R>
        requires (!std::is_same_v<std::remove_cvref_t<R>, dynamic_array>) && std::is_constructible_v<T, std::ranges::range_reference_t<R>>
        SGCL_INLINE_HOT explicit dynamic_array(R&& r)
        : dynamic_array(std::ranges::begin(r), std::ranges::end(r)) {
        }

        SGCL_INLINE_HOT dynamic_array(std::initializer_list<T> ilist)
        : dynamic_array(ilist.begin(), ilist.end()) {
        }

        SGCL_INLINE_HOT dynamic_array(const dynamic_array& other)
        : dynamic_array(other.begin(), other.end()) {
        }

        SGCL_INLINE_HOT dynamic_array(dynamic_array&& other) noexcept
        : _ptr(other._ptr)
        , _size(other._size) {
            other._ptr = nullptr;
            other._size = 0;
        }

        // The elements die with the handle, wherever it dies (vector.h:
        // ~vector); the buffer is freed by the collector, never destroyed.
        SGCL_INLINE_HOT ~dynamic_array() {
            _destroy_all();
        }

        SGCL_INLINE_HOT dynamic_array& operator=(const dynamic_array& other) {
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

        SGCL_INLINE_HOT dynamic_array& operator=(std::initializer_list<T> ilist) {
            dynamic_array fresh(ilist);
            swap(fresh);
            return *this;
        }

        SGCL_INLINE_HOT reference at(size_type pos) {
            if (pos >= size()) {
                throw out_of_range("sgcl::dynamic_array::at");
            }
            return _values()[pos];
        }

        SGCL_INLINE_HOT const_reference at(size_type pos) const {
            if (pos >= size()) {
                throw out_of_range("sgcl::dynamic_array::at");
            }
            return _values()[pos];
        }

        SGCL_INLINE_HOT reference operator[](size_type pos) noexcept {
            return _values()[pos];
        }

        SGCL_INLINE_HOT const_reference operator[](size_type pos) const noexcept {
            return _values()[pos];
        }

        SGCL_INLINE_HOT reference front() noexcept {
            return _values()[0];
        }

        SGCL_INLINE_HOT const_reference front() const noexcept {
            return _values()[0];
        }

        SGCL_INLINE_HOT reference back() noexcept {
            return _values()[size() - 1];
        }

        SGCL_INLINE_HOT const_reference back() const noexcept {
            return _values()[size() - 1];
        }

        SGCL_INLINE_HOT T* data() noexcept {
            return _values();
        }

        SGCL_INLINE_HOT const T* data() const noexcept {
            return _values();
        }

        // The elements as a slice that holds the buffer (slice.h): valid
        // for as long as the array is, and past it, the buffer never
        // moving
        SGCL_INLINE_HOT slice<T> as_slice() noexcept {
            return slice<T>(tracked_ptr<const void>(_ptr), _values(), _values() + _size);
        }

        SGCL_INLINE_HOT slice<const T> as_slice() const noexcept {
            return slice<const T>(tracked_ptr<const void>(_ptr), _values(), _values() + _size);
        }

        SGCL_INLINE_HOT slice<T> as_slice(size_type pos, size_type n = size_type(-1)) {
            if (pos > _size) {
                throw out_of_range("sgcl::dynamic_array::as_slice");
            }
            return slice<T>(tracked_ptr<const void>(_ptr), _values() + pos, _values() + pos + std::min(n, _size - pos));
        }

        SGCL_INLINE_HOT slice<const T> as_slice(size_type pos, size_type n = size_type(-1)) const {
            if (pos > _size) {
                throw out_of_range("sgcl::dynamic_array::as_slice");
            }
            return slice<const T>(tracked_ptr<const void>(_ptr), _values() + pos, _values() + pos + std::min(n, _size - pos));
        }

        SGCL_INLINE_HOT operator slice<T>() noexcept {
            return as_slice();
        }

        SGCL_INLINE_HOT operator slice<const T>() const noexcept {
            return as_slice();
        }

        SGCL_INLINE_HOT iterator begin() noexcept {
            return iterator(_values());
        }

        SGCL_INLINE_HOT const_iterator begin() const noexcept {
            return const_iterator(_values());
        }

        SGCL_INLINE_HOT const_iterator cbegin() const noexcept {
            return begin();
        }

        SGCL_INLINE_HOT iterator end() noexcept {
            return iterator(_values() + size());
        }

        SGCL_INLINE_HOT const_iterator end() const noexcept {
            return const_iterator(_values() + size());
        }

        SGCL_INLINE_HOT const_iterator cend() const noexcept {
            return end();
        }

        SGCL_INLINE_HOT reverse_iterator rbegin() noexcept {
            return reverse_iterator(end());
        }

        SGCL_INLINE_HOT const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(end());
        }

        SGCL_INLINE_HOT const_reverse_iterator crbegin() const noexcept {
            return rbegin();
        }

        SGCL_INLINE_HOT reverse_iterator rend() noexcept {
            return reverse_iterator(begin());
        }

        SGCL_INLINE_HOT const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(begin());
        }

        SGCL_INLINE_HOT const_reverse_iterator crend() const noexcept {
            return rend();
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return size() == 0;
        }

        SGCL_INLINE_HOT size_type size() const noexcept {
            return _size;
        }

        SGCL_INLINE_HOT size_type max_size() const noexcept {
            return (size_type)std::numeric_limits<difference_type>::max() / sizeof(T);
        }

        SGCL_INLINE_HOT void swap(dynamic_array& other) noexcept {
            _ptr.swap(other._ptr);
            std::swap(_size, other._size);
        }

        SGCL_INLINE_HOT friend void swap(dynamic_array& l, dynamic_array& r) noexcept {
            l.swap(r);
        }

    private:
        // Two words: the first element and the count (a size class may
        // give the buffer more room than asked; the count is the handle's).
        tracked_ptr<T> _ptr;
        size_type _size = 0;

        SGCL_INLINE_HOT T* _data() const noexcept {
            return _ptr.get_plain();   // this thread's own buffer: a plain load
        }

        SGCL_INLINE_HOT void _check_size(size_type n) const {
            if (n > max_size()) {
                throw length_error("sgcl::dynamic_array");
            }
        }

        SGCL_INLINE_HOT T* _values() const noexcept {
            return _data();
        }

        SGCL_INLINE_HOT T* _allocate(size_type n) {
            _ptr = unique_ptr<T>(detail::Maker<T[]>::make_tracked_data(n));
            return _data();
        }

        template<class... A>
        SGCL_INLINE_HOT void _construct(T* p, A&&... a) {
            _make_at(p, std::forward<A>(a)...);
            ++_size;
        }

        // An element at p from the arguments; with none, value-initialized
        // (T(): zero for a trivial type), as std's containers do. Not the
        // maker's `new T`: a buffer of a type without tracked pointers is
        // not zeroed when it is issued, and a slot of a pool or a range of
        // pages holds what its last user left there (maker.h), so a
        // default-initialized trivial element would be those bytes
        template<class... A>
        SGCL_INLINE_HOT static void _make_at(T* p, A&&... a) {
            if constexpr(sizeof...(A) == 0) {
                ::new (static_cast<void*>(p)) std::remove_cv_t<T>();
            } else {
                detail::Maker<T>::construct(p, std::forward<A>(a)...);
            }
        }

        // Elements [from, to) value-initialized, the count raised once: a
        // trivial type that holds no tracked pointer is zeroed in one pass
        // (T() is zero for it), any other type constructed one by one
        SGCL_INLINE_HOT static constexpr bool _zero_is_value() noexcept {
            return std::is_trivially_default_constructible_v<T> && !detail::TypeInfo<T>::MayContainTracked && !std::is_member_pointer_v<T>;   // a null member pointer is not zero bytes (-1 on the Itanium ABI); a class holding one is not caught here
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
