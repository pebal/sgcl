//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <iterator>

namespace sgcl::detail {
    template<typename T>
    class Iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        using reverse_iterator = std::reverse_iterator<Iterator>;

        Iterator(pointer ptr, size_t object_size) noexcept
        : _ptr(ptr)
        , _object_size(object_size) {
        }

        Iterator(const Iterator&) = default;
        Iterator& operator=(const Iterator&) = default;

        reference operator[](difference_type n) const noexcept {
            return *_at(n);
        }

        reference operator*() const noexcept {
            return *_ptr;
        }

        pointer operator->() const noexcept {
            return _ptr;
        }

        Iterator& operator++() noexcept {
            _ptr = _at<1>();
            return *this;
        }

        Iterator operator++(int) noexcept {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        Iterator& operator--() noexcept {
            _ptr = _at<-1>();
            return *this;
        }

        Iterator operator--(int) noexcept {
            Iterator tmp = *this;
            --(*this);
            return tmp;
        }

        Iterator operator+(difference_type n) const noexcept {
            return Iterator(_at(n), _object_size);
        }

        Iterator operator-(difference_type n) const noexcept {
            return Iterator(_at(-n), _object_size);
        }

        Iterator& operator+=(difference_type n) noexcept {
            _ptr = _at(n);
            return *this;
        }

        Iterator& operator-=(difference_type n) noexcept {
            _ptr = _at(-n);
            return *this;
        }

        difference_type operator-(const Iterator& other) const noexcept {
            return (_ptr - other._ptr) / _object_size;
        }

        bool operator==(const Iterator& i) const noexcept {
            return _ptr == i._ptr;
        }

        bool operator!=(const Iterator& i) const noexcept {
            return !(*this == i);
        }

        bool operator<(const Iterator& i) const noexcept {
            return _ptr < i._ptr;
        }

        bool operator>(const Iterator& i) const noexcept {
            return i < *this;
        }

        bool operator<=(const Iterator& i) const noexcept {
            return !(i < *this);
        }

        bool operator>=(const Iterator& i) const noexcept {
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
