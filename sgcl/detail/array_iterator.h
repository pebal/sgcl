//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <iterator>

namespace sgcl::detail {
    template<class T>
    class ArrayIterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using reverse_iterator = std::reverse_iterator<ArrayIterator>;

        ArrayIterator() noexcept
        : _object_size(0) {
        }

        ArrayIterator(pointer ptr, size_t object_size) noexcept
        : _ptr(ptr)
        , _object_size(object_size) {
        }

        reference operator[](difference_type n) const noexcept {
            return *_get(n);
        }

        reference operator*() const noexcept {
            return *_ptr;
        }

        pointer operator->() const noexcept {
            return _ptr;
        }

        ArrayIterator& operator++() noexcept {
            _ptr = _get<1>();
            return *this;
        }

        ArrayIterator operator++(int) noexcept {
            ArrayIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        ArrayIterator& operator--() noexcept {
            _ptr = _get<-1>();
            return *this;
        }

        ArrayIterator operator--(int) noexcept {
            ArrayIterator tmp = *this;
            --(*this);
            return tmp;
        }

        ArrayIterator operator+(difference_type n) const noexcept {
            return ArrayIterator(_get(n), _object_size);
        }

        ArrayIterator operator-(difference_type n) const noexcept {
            return ArrayIterator(_get(-n), _object_size);
        }

        ArrayIterator& operator+=(difference_type n) noexcept {
            _ptr = _get(n);
            return *this;
        }

        ArrayIterator& operator-=(difference_type n) noexcept {
            _ptr = _get(-n);
            return *this;
        }

        difference_type operator-(const ArrayIterator& other) const noexcept {
            return ((char*)_ptr - (char*)other._ptr) / _object_size;
        }

        bool operator==(const ArrayIterator<std::remove_cv_t<T>>& other) const noexcept {
            return (_ptr <=> other._ptr) == 0;
        }

        std::strong_ordering operator<=>(const ArrayIterator<std::remove_cv_t<T>>& other) const noexcept {
            return (_ptr <=> other._ptr);
        }

        bool operator==(const ArrayIterator<const std::remove_cv_t<T>>& other) const noexcept {
            return (_ptr <=> other._ptr) == 0;
        }

        std::strong_ordering operator<=>(const ArrayIterator<const std::remove_cv_t<T>>& other) const noexcept {
            return (_ptr <=> other._ptr);
        }

        reverse_iterator make_reverse_iterator() const noexcept {
            return reverse_iterator(*this);
        }

        operator ArrayIterator<const value_type>&() const noexcept {
            return *(ArrayIterator<const value_type>*)(this);
        }

    private:
        pointer _ptr;
        size_t _object_size;

        pointer _get(difference_type offset) const {
            return (pointer)((char*)_ptr + _object_size * offset);
        }

        template<difference_type Offset>
        pointer _get() const {
            return (pointer)((char*)_ptr + _object_size * Offset);
        }

        template<class> friend class ArrayIterator;
    };
}
