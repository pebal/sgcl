//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "iterator.h"
#include "pointer.h"
#include "types.h"

#include <cstddef>
#include <stdexcept>

namespace sgcl::detail {
    template<class T, class Base>
    class ArrayPtr : public Base {
    public:
        using ElementType = typename Base::element_type;
        using ValueType = T[];

        using iterator = Iterator<T>;
        using const_iterator = Iterator<const T>;
        using reverse_iterator = typename iterator::reverse_iterator;
        using const_reverse_iterator = typename const_iterator::reverse_iterator;

        using Base::Base;
        using Base::operator=;
        T& operator*() const = delete;
        T* operator->() const = delete;

        ElementType& operator[](size_t i) const noexcept {
            assert(*this != nullptr);
            assert(i < size());
            return begin()[i];
        }

        ElementType& at(size_t i) const {
            if (!*this || i >= size()) {
                throw std::out_of_range("sgcl::ArrayPtr");
            }
            return begin()[i];
        }

        size_t size() const noexcept {
            return Pointer::size(this->get());
        }

        iterator begin() const noexcept {
            return iterator(this->get(), this->object_size());
        }

        iterator end() const noexcept {
            return begin() + size();
        }

        reverse_iterator rbegin() const noexcept {
            return end().make_reverse_iterator();
        }

        reverse_iterator rend() const noexcept {
            return begin().make_reverse_iterator();
        }

        const_iterator cbegin() const noexcept {
            return const_iterator(this->get(), this->object_size());
        }

        const_iterator cend() const noexcept {
            return cbegin() + size();
        }

        const_reverse_iterator crbegin() const noexcept {
            return cend().make_reverse_iterator();
        }

        const_reverse_iterator crend() const noexcept {
            return cbegin().make_reverse_iterator();
        }
    };
}
