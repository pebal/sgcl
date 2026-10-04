//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/slice.h"

#include <cstddef>
#include <cstring>
#include <new>

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
        SGCL_INLINE_HOT void secure_zero_object(T& object) noexcept {
            secure_zero(static_cast<void*>(&object), sizeof(T));
        }

        // The memory of a SmallVector that holds a secret
        // (core/detail/small_vector.h; secret_bytes): plain memory from
        // ::operator new, never managed; every block zeroed before it goes
        // back, the one a growth leaves too, and every byte the vector lets
        // go of without freeing (a truncation, a move's source, the inline
        // bytes a growth leaves, the destructor's) zeroed by wipe. `probe`,
        // when a test sets it, is shown every block as it goes, zeroed and
        // not yet freed.
        struct WipingPolicy {
            static inline void (*probe)(const void* block, size_t n) noexcept = nullptr;

            SGCL_INLINE_HOT static void* allocate(size_t bytes) noexcept {
                return ::operator new(bytes);
            }

            SGCL_INLINE_HOT static void deallocate(void* p, size_t bytes) noexcept {
                secure_zero(p, bytes);
                if (probe) {
                    probe(p, bytes);
                }
                ::operator delete(p, bytes);
            }

            SGCL_INLINE_HOT static void wipe(void* p, size_t bytes) noexcept {
                secure_zero(p, bytes);
            }
        };
    }

    // Zeros over the bytes of a buffer that held a secret — a key, a
    // password, what a key derivation gave — that the compiler cannot drop
    // as a store nobody reads. The types of the module that hold a secret
    // do it themselves in their destructors; this is for the program's own
    // buffers: a stack array, a vector<byte>, a std::array.
    SGCL_INLINE_HOT void secure_zero(const slice<byte>& bytes) noexcept {
        detail::secure_zero(bytes.data(), bytes.size());
    }
}
