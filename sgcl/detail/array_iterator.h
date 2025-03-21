//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <iterator>

namespace sgcl::detail {
    template<typename T>
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

        ArrayIterator(pointer ptr, size_t object_size) noexcept
        : _ptr(ptr)
        , _object_size(object_size) {
        }

        ArrayIterator(const ArrayIterator&) = default;
        ArrayIterator& operator=(const ArrayIterator&) = default;

        reference operator[](difference_type n) const noexcept {
            return *_at(n);
        }

        reference operator*() const noexcept {
            return *_ptr;
        }

        pointer operator->() const noexcept {
            return _ptr;
        }

        ArrayIterator& operator++() noexcept {
            _ptr = _at<1>();
            return *this;
        }

        ArrayIterator operator++(int) noexcept {
            ArrayIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        ArrayIterator& operator--() noexcept {
            _ptr = _at<-1>();
            return *this;
        }

        ArrayIterator operator--(int) noexcept {
            ArrayIterator tmp = *this;
            --(*this);
            return tmp;
        }

        ArrayIterator operator+(difference_type n) const noexcept {
            return ArrayIterator(_at(n), _object_size);
        }

        ArrayIterator operator-(difference_type n) const noexcept {
            return ArrayIterator(_at(-n), _object_size);
        }

        ArrayIterator& operator+=(difference_type n) noexcept {
            _ptr = _at(n);
            return *this;
        }

        ArrayIterator& operator-=(difference_type n) noexcept {
            _ptr = _at(-n);
            return *this;
        }

        difference_type operator-(const ArrayIterator& other) const noexcept {
            return (_ptr - other._ptr) / _object_size;
        }

        bool operator==(const ArrayIterator& i) const noexcept {
            return _ptr == i._ptr;
        }

        bool operator!=(const ArrayIterator& i) const noexcept {
            return !(*this == i);
        }

        bool operator<(const ArrayIterator& i) const noexcept {
            return _ptr < i._ptr;
        }

        bool operator>(const ArrayIterator& i) const noexcept {
            return i < *this;
        }

        bool operator<=(const ArrayIterator& i) const noexcept {
            return !(i < *this);
        }

        bool operator>=(const ArrayIterator& i) const noexcept {
            return !(*this < i);
        }

        reverse_iterator make_reverse_iterator() const noexcept {
            return reverse_iterator(*this);
        }

    private:
        pointer _at(difference_type offset) const {
            return (pointer)((char*)_ptr + _object_size * offset);
        }

        template<difference_type Offset>
        pointer _at() const {
            return (pointer)((char*)_ptr + _object_size * Offset);
        }

        pointer _ptr;
        const size_t _object_size;
    };
}
