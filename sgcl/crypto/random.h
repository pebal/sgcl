//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/slice.h"
#include "../core/vector.h"

#include "detail/drbg.h"

#include <cstddef>

// Random bytes for keys, nonces, salts and tokens, Go's crypto/rand: a
// ChaCha20 generator in user space, one per thread, seeded from the
// operating system's (getentropy() on macOS and on Linux, which reads
// getrandom() and waits once at boot until the kernel's pool is seeded;
// BCryptGenRandom on Windows) and reseeded from it after 1 MiB or 60 s,
// seeded afresh in the child of a fork. Fast key erasure: the key that
// made a byte given out is gone before the byte leaves (detail/drbg.h).
// About 21 ns for 32 bytes on an Apple M-series core, where a system call a
// request cost 1.5 us (and OpenSSL's RAND_bytes 200 ns).
//
// There is no error to handle. A system that cannot give random bytes
// cannot make a key safely, and a program that went on with zeros or with
// a weaker source would be worse off than one that stops; so a failure
// writes a line to stderr and calls std::terminate, as Go's crypto/rand has
// panicked since 1.24.
namespace sgcl::crypto {
    namespace random {
        // out filled with random bytes
        inline void fill(const slice<byte>& out) noexcept {
            detail::drbg_fill(reinterpret_cast<unsigned char*>(out.data()), out.size());
        }

        // n random bytes
        inline vector<byte> bytes(size_t n) {
            vector<byte> out(n);
            fill(out.as_slice());
            return out;
        }
    }
}
