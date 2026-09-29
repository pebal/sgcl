//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/sha1.h"
#include "../core/array.h"
#include "../hash/mixin/hasher.h"

#include <cstddef>

// SHA-1 (FIPS 180-4), Go's crypto/sha1: a digest of 20 bytes, broken for
// collisions since 2017 (SHAttered) and kept for what still names it — Git's
// object names, HMAC-SHA-1 in TOTP (RFC 6238) and old protocols, PBKDF2 of
// RFC 6070. A signature or a certificate takes SHA-256 or better; the
// module's x509 refuses SHA-1 in signatures.
//
// A hasher is the five chaining words, a block's buffer and the length,
// about 100 bytes, a plain value that a copy branches, in the shape of the
// hash module's hashers (hash::mixin::hasher). On arm64 the blocks go
// through the SHA-1 instructions of ARMv8 (detail/sha1.h).
namespace sgcl::crypto {
    class sha1 : public hash::mixin::hasher<sha1> {
        friend class hash::mixin::hasher<sha1>;
        friend struct detail::HashAccess;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 20;
        static constexpr size_t block_size = 64;

        sha1() noexcept {
            _state.init(detail::sha1_iv);
        }

        void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        // The digest of everything so far; the hasher goes on
        array<byte, 20> value() const noexcept {
            sha1 h = *this;
            array<byte, 20> out;
            h._finish(detail::bytes(out.data()));
            return out;
        }

        array<byte, 20> digest() const noexcept {
            return value();
        }

        void reset() noexcept {
            _state.init(detail::sha1_iv);
        }

    private:
        detail::MdStream<detail::Sha1Traits> _state;

        void _finish(unsigned char* out) noexcept {
            _state.finish(out);
        }
    };

    namespace detail {
        template<>
        inline constexpr bool crypto_digest<sha1> = true;
    }
}
