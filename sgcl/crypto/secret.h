//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "constant_time.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/slice.h"

#include <cstddef>
#include <cstring>

namespace sgcl::crypto {
    template<size_t N>
    class secret;

    namespace detail {
        // The way in for the types that make a secret: its bytes written
        // in place, never passed through a copy on the way
        struct SecretAccess {
            template<size_t N>
            static secret<N> make() noexcept {
                return secret<N>();
            }

            template<size_t N>
            static unsigned char* data(secret<N>& s) noexcept {
                return reinterpret_cast<unsigned char*>(s._bytes);
            }
        };
    }

    // N bytes that are a secret — an ECDH shared secret, a private key's
    // scalar — held in the object itself (no allocation), so that they
    // have one place and the destructor can zero it. Move-only: a copy is
    // asked for by name with clone(); a move leaves the source zeroed; the
    // destructor zeroes the bytes with stores the compiler cannot drop
    // (secure_zero). Read through bytes(), a slice without an owner, which
    // every function of the module that takes bytes takes (hkdf, hmac):
    // the secret outlives the call. Kept on the stack or in a unique_ptr it
    // is gone when its scope ends; in a managed object it stays in memory
    // until the cycle that finds that object dead.
    template<size_t N>
    class secret {
        friend struct detail::SecretAccess;

    public:
        static constexpr size_t size = N;

        secret(const secret&) = delete;
        secret& operator=(const secret&) = delete;

        secret(secret&& other) noexcept {
            std::memcpy(_bytes, other._bytes, N);
            other._wipe();
        }

        secret& operator=(secret&& other) noexcept {
            if (this != &other) {
                std::memcpy(_bytes, other._bytes, N);
                other._wipe();
            }
            return *this;
        }

        ~secret() {
            _wipe();
        }

        secret clone() const noexcept {
            secret s;
            std::memcpy(s._bytes, _bytes, N);
            return s;
        }

        slice<const byte> bytes() const noexcept {
            return slice<const byte>(_bytes, N);
        }

        operator slice<const byte>() const noexcept {
            return bytes();
        }

        // The same bytes, compared in constant time
        friend bool operator==(const secret& a, const secret& b) noexcept {
            return constant_time::equal(a.bytes(), b.bytes());
        }

    private:
        byte _bytes[N];

        secret() noexcept = default;

        void _wipe() noexcept {
            detail::secure_zero(_bytes, N);
        }
    };
}
