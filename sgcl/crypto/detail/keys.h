//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/os.h"
#include "../../core/aliases.h"

#include <cstddef>
#include <stdexcept>
#include <string>

// What the types holding a key share: the words of a wrong key length, and
// the refusal to work after a move. A key object is move-only (§11 A1): the
// object moved from is zeroed and holds no key, and a call on it is a bug
// in the program, never encryption under a zero key.
namespace sgcl::crypto::detail {
    // "aes: a key of 20 bytes", the detail of a wrong key length
    inline std::string key_size_message(const char* type, size_t n) noexcept {
        std::string m = type;
        m += ": a key of ";
        m += std::to_string(n);
        m += " bytes";
        return m;
    }

    SGCL_INLINE_HOT bool is_aes_key_size(size_t n) noexcept {
        return n == 16 || n == 24 || n == 32;
    }

    [[noreturn]] inline void moved_from(const char* type) {
        throw logic_error(std::string(type) + ": used after being moved from");
    }
}
