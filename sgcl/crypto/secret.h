//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "constant_time.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/detail/bytes.h"
#include "../core/detail/small_vector.h"
#include "../core/slice.h"

#include <cstddef>
#include <cstring>
#include <new>

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

    // Bytes that are a secret, of a length known when the program runs: a key
    // read from a file, what HKDF or PBKDF2 derives, a key unwrapped (open_to,
    // decrypt_oaep_to), a private key's export, a password. Never in managed
    // memory: up to 64 bytes in the object itself (keys of 32, 48 or 64 bytes,
    // derived secrets, shared secrets: no allocation at all), past that in a
    // block of plain memory zeroed before it is freed, and the old block
    // zeroed when a growth leaves it. Move-only: a copy is asked for by name
    // with clone(); a move leaves the source empty. The destructor zeroes what
    // it held. Read and written through a slice without an owner, which every
    // function of the module that takes bytes takes: as_slice(), or the
    // conversions, to slice<const byte> where bytes are read and, from a
    // secret_bytes the program may change, to slice<byte> where they are
    // written (open_to, decrypt_oaep_to, random::fill); no push_back, no
    // operator[]. Kept on the stack or in a unique_ptr its bytes are gone when
    // its scope ends; in a managed object the inline ones stay in managed
    // memory until the cycle that finds the object dead, so a secret_bytes
    // belongs on the stack or in plain memory. secret<N> is the one for a
    // length known when the program is compiled. The storage is the core's
    // SmallVector with the wiping policy (core/detail/small_vector.h,
    // secure_zero.h: WipingPolicy), which zeroes every byte it lets go of.
    class secret_bytes {
    public:
        static constexpr size_t inline_capacity = 64;

        // Empty
        secret_bytes() noexcept = default;

        // n bytes, all zero
        explicit secret_bytes(size_t n) noexcept
        : _bytes(n) {
        }

        secret_bytes(const secret_bytes&) = delete;
        secret_bytes& operator=(const secret_bytes&) = delete;

        secret_bytes(secret_bytes&&) noexcept = default;
        secret_bytes& operator=(secret_bytes&&) noexcept = default;

        secret_bytes clone() const noexcept {
            secret_bytes s(size());
            sgcl::detail::copy_bytes(s._bytes.data(), _bytes.data(), size());
            return s;
        }

        slice<const byte> as_slice() const noexcept {
            return slice<const byte>(_bytes.data(), _bytes.size());
        }

        slice<byte> as_slice() noexcept {
            return slice<byte>(_bytes.data(), _bytes.size());
        }

        // As secret<N>: where the module takes bytes, a secret_bytes is taken
        operator slice<const byte>() const noexcept {
            return as_slice();
        }

        // Where the module writes bytes (an AEAD's open_to, RSA's
        // decrypt_oaep_to, random::fill), a secret_bytes the program may
        // change is taken as the output. Only an lvalue: the bytes written
        // into one about to go would be read by no one
        operator slice<byte>() & noexcept {
            return as_slice();
        }

        size_t size() const noexcept {
            return _bytes.size();
        }

        bool empty() const noexcept {
            return _bytes.empty();
        }

        // n bytes: the first min(n, size()) kept, the rest zero. Past the
        // capacity a new block of exactly n (the old one, or the inline
        // bytes, zeroed); a shrink zeroes the bytes it drops and keeps the
        // room
        void resize(size_t n) noexcept {
            _bytes.resize(n);
        }

        // The same bytes, compared in constant time (the lengths are not
        // secret: of different lengths, unequal at once)
        friend bool operator==(const secret_bytes& a, const secret_bytes& b) noexcept {
            return a.size() == b.size() && constant_time::equal(a.as_slice(), b.as_slice());
        }

    private:
        sgcl::detail::SmallVector<byte, inline_capacity, detail::WipingPolicy> _bytes;
    };
}
