//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The bytes a harness hands to a parser, in a buffer of exactly their size
// outside the managed heap. ASan sees a read past the end of a malloc'd
// buffer (its redzone follows it), not one past a managed string, vector or
// array: the collector's slots lie side by side on its own pages, and a
// string's terminator follows its characters, so an overread there reads
// the next object or nothing at all and shows, if ever, as a random SEGV.
// Nor one past a std::string, whose capacity and small buffer go beyond its
// size. libFuzzer's own data is a buffer of the input's size, so a piece of
// it that ends where the input ends needs no copy; a piece that ends
// earlier (a part split at a NUL, the input less a trailing byte of
// options, a prefix) and bytes the harness made (a std::string grown in
// steps, an sgcl::string) are copied into one of these first:
//
//   sgcl_fuzz::exact part(in.substr(at, nul - at));
//   auto r = parser::parse(part.bytes());       // or part.chars(), part.view()
#pragma once

#include "sgcl/core/slice.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace sgcl_fuzz {
    class exact {
    public:
        exact(const void* p, size_t n)
        : _p(static_cast<char*>(std::malloc(n))), _n(n) {
            if (n) {
                std::memcpy(_p, p, n);
            }
        }

        explicit exact(std::string_view s)
        : exact(s.data(), s.size()) {
        }

        exact(const exact&) = delete;
        exact& operator=(const exact&) = delete;

        ~exact() {
            std::free(_p);
        }

        const char* data() const noexcept {
            return _p;
        }

        const uint8_t* u8() const noexcept {
            return reinterpret_cast<const uint8_t*>(_p);
        }

        size_t size() const noexcept {
            return _n;
        }

        std::string_view view() const noexcept {
            return std::string_view(_p, _n);
        }

        sgcl::slice<const std::byte> bytes() const noexcept {
            return sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(_p), _n);
        }

        sgcl::slice<const char> chars() const noexcept {
            return sgcl::slice<const char>(_p, _n);
        }

    private:
        char* _p;
        size_t _n;
    };
}
