//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------
#pragma once

#include <algorithm>

namespace sgcl {
    namespace Priv {
        struct Counter {
            Counter operator+(const Counter& c) const noexcept {
                return {count + c.count, size + c.size};
            }
            Counter operator-(const Counter& c) const noexcept {
                return {count - c.count, size - c.size};
            }
            void operator+=(const Counter& c) noexcept {
                count += c.count;
                size += c.size;
            }
            void operator-=(const Counter& c) noexcept {
                count += c.count;
                size += c.size;
            }
            int64_t count = {0};
            int64_t size = {0};
        };

        inline Counter max(const Counter& a, const Counter& b) {
            Counter c;
            c.count = std::max(a.count, b.count);
            c.size = std::max(a.size, b.size);
            return c;
        }

        inline Counter min(const Counter& a, const Counter& b) {
            Counter c;
            c.count = std::min(a.count, b.count);
            c.size = std::min(a.size, b.size);
            return c;
        }
    }
}
