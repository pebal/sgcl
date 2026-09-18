//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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

        constexpr ContiguousIterator() noexcept
        : _ptr(nullptr) {
        }

        constexpr explicit ContiguousIterator(pointer ptr) noexcept
        : _ptr(ptr) {
        }

        constexpr ContiguousIterator(const ContiguousIterator&) = default;
        constexpr ContiguousIterator& operator=(const ContiguousIterator&) = default;

        // A dead iterator does not keep its buffer alive: the collector
        // scans the stack conservatively, and a temporary left behind in
        // a frame would count as a root until the word is overwritten.
        // Constexpr, as everything here: the iterator is a literal type,
        // so that array<T, N>'s members run in constant evaluation.
        constexpr ~ContiguousIterator() noexcept {
            _ptr = nullptr;
        }

        constexpr reference operator*() const noexcept {
            return *_ptr;
        }

        constexpr pointer operator->() const noexcept {
            return _ptr;
        }

        constexpr reference operator[](difference_type n) const noexcept {
            return _ptr[n];
        }

        constexpr ContiguousIterator& operator++() noexcept {
            ++_ptr;
            return *this;
        }

        constexpr ContiguousIterator operator++(int) noexcept {
            ContiguousIterator tmp = *this;
            ++_ptr;
            return tmp;
        }

        constexpr ContiguousIterator& operator--() noexcept {
            --_ptr;
            return *this;
        }

        constexpr ContiguousIterator operator--(int) noexcept {
            ContiguousIterator tmp = *this;
            --_ptr;
            return tmp;
        }

        constexpr ContiguousIterator& operator+=(difference_type n) noexcept {
            _ptr += n;
            return *this;
        }

        constexpr ContiguousIterator& operator-=(difference_type n) noexcept {
            _ptr -= n;
            return *this;
        }

        constexpr friend ContiguousIterator operator+(ContiguousIterator i, difference_type n) noexcept {
            return ContiguousIterator(i._ptr + n);
        }

        constexpr friend ContiguousIterator operator+(difference_type n, ContiguousIterator i) noexcept {
            return ContiguousIterator(i._ptr + n);
        }

        constexpr friend ContiguousIterator operator-(ContiguousIterator i, difference_type n) noexcept {
            return ContiguousIterator(i._ptr - n);
        }

        constexpr friend difference_type operator-(const ContiguousIterator& l, const ContiguousIterator& r) noexcept {
            return l._ptr - r._ptr;
        }

        constexpr friend bool operator==(const ContiguousIterator& l, const ContiguousIterator& r) noexcept {
            return l._ptr == r._ptr;
        }

        constexpr friend std::strong_ordering operator<=>(const ContiguousIterator& l, const ContiguousIterator& r) noexcept {
            return std::compare_three_way{}(l._ptr, r._ptr);
        }

        constexpr operator ContiguousIterator<const U>() const noexcept requires (!std::is_const_v<U>) {
            return ContiguousIterator<const U>(_ptr);
        }

        constexpr reverse_iterator make_reverse_iterator() const noexcept {
            return reverse_iterator(*this);
        }

    private:
        pointer _ptr;

        template<class> friend class ContiguousIterator;
    };
}
