//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/slice.h"

#include <cstddef>
#include <cstring>

namespace sgcl::crypto {
    namespace detail {
        // Zeros written over n bytes at p that the compiler may not remove.
        // A memset of memory nobody reads afterwards is a dead store, and
        // compilers drop it (the object is about to die, the buffer to be
        // freed); here the memset is followed by an empty asm that takes
        // the pointer and clobbers memory, so the compiler must assume the
        // zeros are read, which is what BoringSSL's OPENSSL_cleanse and
        // glibc's explicit_bzero do. Elsewhere the bytes go out one by one
        // through a volatile pointer.
        inline void secure_zero(void* p, size_t n) noexcept {
            if (n == 0) {
                return;
            }
#if defined(__GNUC__) || defined(__clang__)
            std::memset(p, 0, n);
            __asm__ __volatile__("" : : "r"(p) : "memory");
#else
            volatile unsigned char* v = static_cast<volatile unsigned char*>(p);
            for (size_t i = 0; i < n; ++i) {
                v[i] = 0;
            }
#endif
        }

        // An object's bytes zeroed, the object itself left to its
        // destructor (a trivially destructible state: lanes, a buffer)
        template<class T>
        void secure_zero_object(T& object) noexcept {
            secure_zero(static_cast<void*>(&object), sizeof(T));
        }
    }

    // Zeros over the bytes of a buffer that held a secret — a key, a
    // password, what a key derivation gave — that the compiler cannot drop
    // as a store nobody reads. The types of the module that hold a secret
    // do it themselves in their destructors; this is for the program's own
    // buffers: a stack array, a vector<byte>, a std::array.
    inline void secure_zero(const slice<byte>& bytes) noexcept {
        detail::secure_zero(bytes.data(), bytes.size());
    }
}
