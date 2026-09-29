//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/slice.h"
#include "../../hash/mixin/hasher.h"

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sgcl::crypto::detail {
    using namespace sgcl::detail;

    // Words of the digests: SHA-1 and SHA-2 are big-endian, Keccak,
    // ChaCha20 and Poly1305 little-endian. A memcpy of a known length is one load, and the swap
    // is one instruction (rev) on a little-endian machine
    inline uint32_t load_be32(const unsigned char* p) noexcept {
        return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | uint32_t(p[3]);
    }

    inline uint64_t load_be64(const unsigned char* p) noexcept {
        return uint64_t(load_be32(p)) << 32 | load_be32(p + 4);
    }

    inline void store_be32(unsigned char* p, uint32_t v) noexcept {
        p[0] = static_cast<unsigned char>(v >> 24);
        p[1] = static_cast<unsigned char>(v >> 16);
        p[2] = static_cast<unsigned char>(v >> 8);
        p[3] = static_cast<unsigned char>(v);
    }

    inline void store_be64(unsigned char* p, uint64_t v) noexcept {
        store_be32(p, uint32_t(v >> 32));
        store_be32(p + 4, uint32_t(v));
    }

    inline uint32_t load_le32(const unsigned char* p) noexcept {
        uint32_t v;
        if constexpr (std::endian::native == std::endian::little) {
            std::memcpy(&v, p, 4);
        } else {
            v = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
        }
        return v;
    }

    inline void store_le32(unsigned char* p, uint32_t v) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            std::memcpy(p, &v, 4);
        } else {
            for (int i = 0; i < 4; ++i) {
                p[i] = static_cast<unsigned char>(v >> (8 * i));
            }
        }
    }

    inline uint64_t load_le64(const unsigned char* p) noexcept {
        uint64_t v;
        if constexpr (std::endian::native == std::endian::little) {
            std::memcpy(&v, p, 8);
        } else {
            v = 0;
            for (int i = 7; i >= 0; --i) {
                v = v << 8 | p[i];
            }
        }
        return v;
    }

    inline void store_le64(unsigned char* p, uint64_t v) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            std::memcpy(p, &v, 8);
        } else {
            for (int i = 0; i < 8; ++i) {
                p[i] = static_cast<unsigned char>(v >> (8 * i));
            }
        }
    }

    inline const unsigned char* bytes(const byte* p) noexcept {
        return reinterpret_cast<const unsigned char*>(p);
    }

    inline unsigned char* bytes(byte* p) noexcept {
        return reinterpret_cast<unsigned char*>(p);
    }

    // The way into a digest's insides for hmac, hkdf and pbkdf2: the
    // result written into a buffer by padding the digest's own state in
    // place, so that the state of a copy made from a keyed one is used up
    // and zeroed by the caller rather than copied again by value()
    struct HashAccess {
        template<class H>
        static void finish(H& h, unsigned char* out) noexcept {
            h._finish(out);
        }
    };

    // A digest the module's hmac, hkdf and pbkdf2 are made over: one of
    // the module's own (it says so with crypto_digest), trivially copyable,
    // so that its state can be zeroed as bytes
    template<class H>
    inline constexpr bool crypto_digest = false;

    template<class H>
    concept digest_type = crypto_digest<H> && std::is_trivially_copyable_v<H> && std::default_initializable<H>
                       && requires { H::digest_size; H::block_size; };
}
