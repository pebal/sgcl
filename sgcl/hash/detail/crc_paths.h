//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "crc.h"
#include "crc_arm64.h"
#include "crc_x86.h"

namespace sgcl::hash::detail {
    // The register after p[0, n) by the fastest path the processor has:
    // arm64's folding and CRC instructions behind cpu::crypto(), x86-64's
    // folding behind cpu::aes() from 128 bytes up, slicing by eight elsewhere
    template<class T, T Poly>
    inline T crc_update(T reg, const unsigned char* p, size_t n) noexcept {
#if defined(SGCL_HASH_ARM64)
        if (sgcl::detail::cpu::crypto()) {
            if constexpr (sizeof(T) == 4) {
                return crc32_update_arm64<Poly>(reg, p, n);
            } else {
                return crc64_update_arm64<Poly>(reg, p, n);
            }
        }
#elif defined(SGCL_HASH_X86)
        if (n >= 128 && sgcl::detail::cpu::aes()) {
            return crc_update_x86<T, Poly>(reg, p, n);
        }
#endif
        return crc_update_portable<T, Poly>(reg, p, n);
    }
}
