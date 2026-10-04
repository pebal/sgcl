//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "crc.h"

#include <cstdint>

// The constants of folding by carry-less multiplication, which the arm64
// (PMULL) and x86-64 (PCLMULQDQ) paths share: crc_arm64.h says the
// arithmetic. For each distance D a lane moves (512 bits in the loop, 384,
// 256 and 128 at the end), x^(D+63) and x^(D-1) mod P, a 32-bit CRC's in the
// top half of the word.
namespace sgcl::hash::detail {
    struct FoldKeys {
        uint64_t lo512, hi512;   // a lane four lanes on, in the loop
        uint64_t lo384, hi384;   // the lanes onto the last at the end
        uint64_t lo256, hi256;
        uint64_t lo128, hi128;
    };

    template<class T, T Poly>
    SGCL_INLINE_HOT constexpr uint64_t fold_key(unsigned e) noexcept {
        return uint64_t(crc_xpow<T, Poly>(e)) << (64 - RegisterBits<T>);
    }

    template<class T, T Poly>
    inline constexpr FoldKeys fold_keys = {
        fold_key<T, Poly>(512 + 63), fold_key<T, Poly>(512 - 1),
        fold_key<T, Poly>(384 + 63), fold_key<T, Poly>(384 - 1),
        fold_key<T, Poly>(256 + 63), fold_key<T, Poly>(256 - 1),
        fold_key<T, Poly>(128 + 63), fold_key<T, Poly>(128 - 1),
    };
}
