//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../containers/array.h"
#include "../../core/config.h"
#include "../../core/slice.h"
#include "../../core/string.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace sgcl::io {
    namespace detail {
        // The bytes of a string, as a slice that holds it; of a text
        // slice, the same owner; of a literal, none
        inline slice<const std::byte> bytes_of(const string& s) noexcept {
            return as_bytes(s.as_slice());
        }

        inline slice<const std::byte> bytes_of(const slice<const char>& s) noexcept {
            return as_bytes(s);
        }

        inline slice<const std::byte> bytes_of(const char* s) noexcept {
            return slice<const std::byte>(reinterpret_cast<const std::byte*>(s), std::char_traits<char>::length(s));
        }

        inline slice<const std::byte> bytes_of(std::string_view s) noexcept {
            return slice<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
        }

        // The bytes as characters: a std view (for the algorithms), a
        // slice of the same owner, a new string
        inline std::string_view chars_of(const slice<const std::byte>& b) noexcept {
            return std::string_view(reinterpret_cast<const char*>(b.data()), b.size());
        }

        inline slice<const char> text_slice_of(const slice<const std::byte>& b) noexcept {
            return slice<const char>(b.owner(), reinterpret_cast<const char*>(b.data()), b.size());
        }

        inline string text_of(const slice<const std::byte>& b) {
            return string(chars_of(b));
        }

        // The block copy() moves data through: one managed array, no header,
        // two to a page
        using CopyBlock = array<std::byte, config::IoCopyBufferSize>;
    }
}
