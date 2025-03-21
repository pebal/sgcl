//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_iterator.h"
#include "pointer.h"
#include "types.h"

#include <cstddef>
#include <stdexcept>

namespace sgcl::detail {
    template<class T, class Base>
    class ArrayPtr : public Base {
    public:
        using element_type = typename Base::element_type;
        using value_type = T[];
        using size_type	 = size_t;
        using iterator = ArrayIterator<element_type>;
        using const_iterator = ArrayIterator<const element_type>;
        using reverse_iterator = typename iterator::reverse_iterator;
        using const_reverse_iterator = typename const_iterator::reverse_iterator;

        using Base::Base;
        using Base::operator=;
        T& operator*() const = delete;
        T* operator->() const = delete;

        element_type& operator[](size_t i) noexcept {
            assert(*this != nullptr);
            assert(i < size());
            return begin()[i];
        }

        const element_type& operator[](size_t i) const noexcept {
            assert(*this != nullptr);
            assert(i < size());
            return begin()[i];
        }

        element_type& at(size_t i) {
            if (!*this || i >= size()) {
                throw std::out_of_range("sgcl::detail::ArrayPtr");
            }
            return (*this)[i];
        }

        const element_type& at(size_t i) const {
            if (!*this || i >= size()) {
                throw std::out_of_range("sgcl::detail::ArrayPtr");
            }
            return (*this)[i];
        }

        size_t size() const noexcept {
            return Pointer::size(this->get());
        }

        iterator begin() noexcept {
            return iterator(this->get(), this->object_size());
        }

        const_iterator begin() const noexcept {
            return cbegin();
        }

        iterator end() noexcept {
            return begin() + size();
        }

        const_iterator end() const noexcept {
            return cend();
        }

        const_iterator cbegin() const noexcept {
            return const_iterator(this->get(), this->object_size());
        }

        const_iterator cend() const noexcept {
            return cbegin() + size();
        }

        reverse_iterator rbegin() noexcept {
            return end().make_reverse_iterator();
        }

        const_reverse_iterator rbegin() const noexcept {
            return crbegin();
        }

        reverse_iterator rend() noexcept {
            return begin().make_reverse_iterator();
        }

        const_reverse_iterator rend() const noexcept {
            return crend();
        }

        const_reverse_iterator crbegin() const noexcept {
            return cend().make_reverse_iterator();
        }

        const_reverse_iterator crend() const noexcept {
            return cbegin().make_reverse_iterator();
        }
    };
}
