//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <algorithm>

namespace sgcl::detail {
    struct Counter {
        Counter() = default;
        Counter(size_t c, size_t s)
        : count(c)
        , size(s) {
        }
        Counter operator+(const Counter& c) const noexcept {
            return {count + c.count, size + c.size};
        }
        Counter operator-(const Counter& c) const noexcept {
            return {count - c.count, size - c.size};
        }
        Counter operator*(size_t v) const noexcept {
            return {count * v, size * v};
        }
        void operator+=(const Counter& c) noexcept {
            count += c.count;
            size += c.size;
        }
        void operator-=(const Counter& c) noexcept {
            count += c.count;
            size += c.size;
        }
        void operator*=(size_t v) noexcept {
            count *= v;
            size *= v;
        }
        bool operator>(const Counter& c) noexcept {
            return count > c.count || size > c.size;
        }
        size_t count = {0};
        size_t size = {0};
    };

    inline Counter max(const Counter& a, const Counter& b) noexcept {
        Counter c;
        c.count = std::max(a.count, b.count);
        c.size = std::max(a.size, b.size);
        return c;
    }

    inline Counter min(const Counter& a, const Counter& b) noexcept {
        Counter c;
        c.count = std::min(a.count, b.count);
        c.size = std::min(a.size, b.size);
        return c;
    }
}
