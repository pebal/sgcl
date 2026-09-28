//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/sha256.h"
#include "secure_zero.h"
#include "../core/array.h"
#include "../hash/mixin/hasher.h"

#include <cstddef>
#include <cstring>

// SHA-256 and SHA-224 (FIPS 180-4), Go's crypto/sha256: the digest of TLS,
// of certificates, of Bitcoin and of most of what signs anything today.
// SHA-224 is SHA-256 from other initial values, cut to 28 bytes.
//
// A hasher is the eight chaining words, a block's buffer and the length,
// about 110 bytes, a plain value that a copy branches, in the shape of the
// hash module's hashers (hash::mixin::hasher). On arm64 the blocks go
// through the SHA-256 instructions of ARMv8 (detail/sha256.h).
namespace sgcl::crypto {
    class sha256 : public hash::mixin::hasher<sha256> {
        friend class hash::mixin::hasher<sha256>;
        friend struct detail::HashAccess;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 32;
        static constexpr size_t block_size = 64;

        sha256() noexcept {
            _state.init(detail::sha256_iv);
        }

        void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        // The digest of everything so far; the hasher goes on
        array<byte, 32> value() const noexcept {
            sha256 h = *this;
            array<byte, 32> out;
            h._finish(detail::bytes(out.data()));
            return out;
        }

        array<byte, 32> digest() const noexcept {
            return value();
        }

        void reset() noexcept {
            _state.init(detail::sha256_iv);
        }

    private:
        detail::MdStream<detail::Sha256Traits> _state;

        void _finish(unsigned char* out) noexcept {
            _state.finish(out);
        }
    };

    class sha224 : public hash::mixin::hasher<sha224> {
        friend class hash::mixin::hasher<sha224>;
        friend struct detail::HashAccess;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 28;
        static constexpr size_t block_size = 64;

        sha224() noexcept {
            _state.init(detail::sha224_iv);
        }

        void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        array<byte, 28> value() const noexcept {
            sha224 h = *this;
            array<byte, 28> out;
            h._finish(detail::bytes(out.data()));
            return out;
        }

        array<byte, 28> digest() const noexcept {
            return value();
        }

        void reset() noexcept {
            _state.init(detail::sha224_iv);
        }

    private:
        detail::MdStream<detail::Sha256Traits> _state;

        // the eight words, of which the first seven are the digest; the
        // eighth is state an HMAC's key went into, so it does not stay
        void _finish(unsigned char* out) noexcept {
            unsigned char full[32];
            _state.finish(full);
            std::memcpy(out, full, 28);
            detail::secure_zero(full, sizeof full);
        }
    };

    namespace detail {
        template<>
        inline constexpr bool crypto_digest<sha256> = true;

        template<>
        inline constexpr bool crypto_digest<sha224> = true;
    }
}
