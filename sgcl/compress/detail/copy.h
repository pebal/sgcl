//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/bytes.h"

#include <cstddef>
#include <cstring>

namespace sgcl::compress::detail {
    // From this many bytes on, copy_out goes to libc
    inline constexpr size_t CopyOutLibc = 4096;

    // A copy into memory the caller handed in or has just grown — a
    // reader's out, the result a whole read fills — which may be anywhere
    // but in the cache. A short run goes by copy_bytes (DESIGN 313: no libc
    // call where the run is short and its branches are the cost; reading a
    // tar of small entries is 1.14x for it). A long one, from 4 KB on, goes
    // to libc's memcpy, the module's exception to 313: into memory not in
    // the cache libc's copy was ~1.4x copy_bytes's wide loop, probably for
    // not reading the destination's lines before it writes them (64 KB
    // runs into 512 MB of memory: 47.5 GB/s against 33.0; a 7z entry of
    // Copy read whole, A/B of 14 pairs: 14.4 against 12.8). Into memory in
    // the cache the two are level from a few KB on, so libc costs nothing
    // there. Since DESIGN 394/444 copy_bytes itself hands 4 KB and more to
    // libc, so this is copy_bytes under the module's own name.
    SGCL_INLINE_HOT void copy_out(void* to, const void* from, size_t n) noexcept {
        if (n < CopyOutLibc) {
            sgcl::detail::copy_bytes(to, from, n);
        } else {
            std::memcpy(to, from, n);
        }
    }
}
