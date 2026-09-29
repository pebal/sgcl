//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <stdexcept>

namespace sgcl::compress {
    // How hard a compressor works: 0 stores the data as it is, 1 is the
    // fastest, 9 the smallest, 6 the default of every format of the module
    // (and of zlib and Go); huffman_only codes the bytes with no search for
    // repeats (Go's HuffmanOnly). An int converts to it, so that options
    // take `{.level = 9}`; a value outside those is invalid_argument (in a
    // constant, an error at compile time).
    class level {
    public:
        static constexpr int store = 0;
        static constexpr int fastest = 1;
        static constexpr int standard = 6;
        static constexpr int smallest = 9;
        static constexpr int huffman_only = -2;

        constexpr level() noexcept
        : _value(standard) {
        }

        constexpr level(int n)
        : _value(n) {
            if (!((n >= 0 && n <= 9) || n == huffman_only)) {
                throw std::invalid_argument("compress::level: 0..9 or level::huffman_only");
            }
        }

        constexpr int value() const noexcept {
            return _value;
        }

        friend constexpr bool operator==(level a, level b) noexcept = default;

    private:
        int _value;
    };
}
