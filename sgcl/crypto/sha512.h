//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/sha512.h"
#include "secure_zero.h"
#include "../core/array.h"
#include "../hash/mixin/hasher.h"

#include <cstddef>
#include <cstring>

// SHA-512, SHA-384 and SHA-512/256 (FIPS 180-4), Go's crypto/sha512: the
// SHA-2 digests on words of 64 bits, faster than SHA-256 per byte where the
// processor has no SHA-256 instructions and slower where it has both.
// SHA-384 (TLS_AES_256_GCM_SHA384, P-384 signatures) and SHA-512/256 are
// SHA-512 from other initial values, cut to 48 and 32 bytes; being cut,
// neither can be extended by a length-extension attack.
//
// A hasher is the eight chaining words, a block's buffer and the length,
// about 210 bytes, a plain value that a copy branches, in the shape of the
// hash module's hashers (hash::mixin::hasher). On arm64 the blocks go
// through the SHA-512 instructions of ARMv8.2 (detail/sha512.h).
namespace sgcl::crypto {
    class sha512 : public hash::mixin::hasher<sha512> {
        friend class hash::mixin::hasher<sha512>;
        friend struct detail::HashAccess;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 64;
        static constexpr size_t block_size = 128;

        sha512() noexcept {
            _state.init(detail::sha512_iv);
        }

        void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        // The digest of everything so far; the hasher goes on
        array<byte, 64> value() const noexcept {
            sha512 h = *this;
            array<byte, 64> out;
            h._finish(detail::bytes(out.data()));
            return out;
        }

        array<byte, 64> digest() const noexcept {
            return value();
        }

        void reset() noexcept {
            _state.init(detail::sha512_iv);
        }

    private:
        detail::MdStream<detail::Sha512Traits> _state;

        void _finish(unsigned char* out) noexcept {
            _state.finish(out);
        }
    };

    class sha384 : public hash::mixin::hasher<sha384> {
        friend class hash::mixin::hasher<sha384>;
        friend struct detail::HashAccess;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 48;
        static constexpr size_t block_size = 128;

        sha384() noexcept {
            _state.init(detail::sha384_iv);
        }

        void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        array<byte, 48> value() const noexcept {
            sha384 h = *this;
            array<byte, 48> out;
            h._finish(detail::bytes(out.data()));
            return out;
        }

        array<byte, 48> digest() const noexcept {
            return value();
        }

        void reset() noexcept {
            _state.init(detail::sha384_iv);
        }

    private:
        detail::MdStream<detail::Sha512Traits> _state;

        // the first six of the eight words; the other two do not stay
        void _finish(unsigned char* out) noexcept {
            unsigned char full[64];
            _state.finish(full);
            std::memcpy(out, full, 48);
            detail::secure_zero(full, sizeof full);
        }
    };

    class sha512_256 : public hash::mixin::hasher<sha512_256> {
        friend class hash::mixin::hasher<sha512_256>;
        friend struct detail::HashAccess;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 32;
        static constexpr size_t block_size = 128;

        sha512_256() noexcept {
            _state.init(detail::sha512_256_iv);
        }

        void update(const slice<const byte>& data) noexcept {
            _state.update(detail::bytes(data.data()), data.size());
        }

        array<byte, 32> value() const noexcept {
            sha512_256 h = *this;
            array<byte, 32> out;
            h._finish(detail::bytes(out.data()));
            return out;
        }

        array<byte, 32> digest() const noexcept {
            return value();
        }

        void reset() noexcept {
            _state.init(detail::sha512_256_iv);
        }

    private:
        detail::MdStream<detail::Sha512Traits> _state;

        // the first four of the eight words; the other four do not stay
        void _finish(unsigned char* out) noexcept {
            unsigned char full[64];
            _state.finish(full);
            std::memcpy(out, full, 32);
            detail::secure_zero(full, sizeof full);
        }
    };

    namespace detail {
        template<>
        inline constexpr bool crypto_digest<sha512> = true;

        template<>
        inline constexpr bool crypto_digest<sha384> = true;

        template<>
        inline constexpr bool crypto_digest<sha512_256> = true;
    }
}
