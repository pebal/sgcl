//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <compare>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace sgcl::detail {
    // Iterator over the elements of a managed buffer (vector, array): a raw
    // pointer, so copying it costs nothing.
    template<class U>
    class ContiguousIterator {
    public:
        using iterator_concept = std::contiguous_iterator_tag;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = std::remove_cv_t<U>;
        using reference = U&;
        using difference_type = ptrdiff_t;
        using pointer = U*;
        using reverse_iterator = std::reverse_iterator<ContiguousIterator>;

        ContiguousIterator() noexcept
        : _ptr(nullptr) {
        }

        explicit ContiguousIterator(pointer ptr) noexcept
        : _ptr(ptr) {
        }

        ContiguousIterator(const ContiguousIterator&) = default;
        ContiguousIterator& operator=(const ContiguousIterator&) = default;

        // A dead iterator does not keep its buffer alive: the collector
        // scans the stack conservatively, and a temporary left behind in
        // a frame would count as a root until the word is overwritten.
        ~ContiguousIterator() noexcept {
            _ptr = nullptr;
        }

        reference operator*() const noexcept {
            return *_ptr;
        }

        pointer operator->() const noexcept {
            return _ptr;
        }

        reference operator[](difference_type n) const noexcept {
            return _ptr[n];
        }

        ContiguousIterator& operator++() noexcept {
            ++_ptr;
            return *this;
        }

        ContiguousIterator operator++(int) noexcept {
            ContiguousIterator tmp = *this;
            ++_ptr;
            return tmp;
        }

        ContiguousIterator& operator--() noexcept {
            --_ptr;
            return *this;
        }

        ContiguousIterator operator--(int) noexcept {
            ContiguousIterator tmp = *this;
            --_ptr;
            return tmp;
        }

        ContiguousIterator& operator+=(difference_type n) noexcept {
            _ptr += n;
            return *this;
        }

        ContiguousIterator& operator-=(difference_type n) noexcept {
            _ptr -= n;
            return *this;
        }

        friend ContiguousIterator operator+(ContiguousIterator i, difference_type n) noexcept {
            return ContiguousIterator(i._ptr + n);
        }

        friend ContiguousIterator operator+(difference_type n, ContiguousIterator i) noexcept {
            return ContiguousIterator(i._ptr + n);
        }

        friend ContiguousIterator operator-(ContiguousIterator i, difference_type n) noexcept {
            return ContiguousIterator(i._ptr - n);
        }

        friend difference_type operator-(const ContiguousIterator& l, const ContiguousIterator& r) noexcept {
            return l._ptr - r._ptr;
        }

        friend bool operator==(const ContiguousIterator& l, const ContiguousIterator& r) noexcept {
            return l._ptr == r._ptr;
        }

        friend std::strong_ordering operator<=>(const ContiguousIterator& l, const ContiguousIterator& r) noexcept {
            return std::compare_three_way{}(l._ptr, r._ptr);
        }

        operator ContiguousIterator<const U>() const noexcept requires (!std::is_const_v<U>) {
            return ContiguousIterator<const U>(_ptr);
        }

        reverse_iterator make_reverse_iterator() const noexcept {
            return reverse_iterator(*this);
        }

    private:
        pointer _ptr;

        template<class> friend class ContiguousIterator;
    };
}
