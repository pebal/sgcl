//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <array>
#include <cstddef>

namespace sgcl::detail {
    template <class T, size_t N>
    class MaxFilter {
    public:
        void add(T value) noexcept {
            values[index] = value;
            index = (index + 1) % N;
        }

        T get() const noexcept {
            return *std::max_element(values.begin(), values.end());
        }

    private:
        std::array<T, N> values {};
        unsigned index = 0;
    };
}
