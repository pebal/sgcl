//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "constant_time.h"
#include "detail/blake2.h"
#include "detail/bytes.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/slice.h"
#include "../hash/mixin/hasher.h"

#include <cstddef>

// BLAKE2b and BLAKE2s (RFC 7693), Go's golang.org/x/crypto/blake2b and
// blake2s: fast digests from the ChaCha family, BLAKE2b on 64-bit words
// (blocks of 128 bytes, digests up to 64 bytes), BLAKE2s on 32-bit words
// (64, up to 32), each its own MAC when made with a key. One type per
// digest length in use, since the length is a parameter of the hash and
// not a truncation: blake2b_512, blake2b_384, blake2b_256, blake2s_256,
// blake2s_128.
//
// A hasher in the shape of the hash module's (hash::mixin::hasher): the
// state, a block's buffer, what reset() goes back to (the words after the
// parameter block and the key), about 400 bytes for BLAKE2b. A copy
// branches, as a digest's does. A hasher made with a key holds its
// equivalent, so its destructor zeroes it; an unkeyed one is left as
// sha256 leaves its state.
namespace sgcl::crypto {
    // What a BLAKE2 hasher is made with beyond its length (RFC 7693 §2.5,
    // §2.8): a key, which makes the hash a MAC, up to 64 bytes (BLAKE2s: 32);
    // a salt and a personalization, up to 16 bytes each (BLAKE2s: 8),
    // zero-padded to their fields. All empty: the plain hash
    struct blake2_options {
        slice<const byte> key;
        slice<const byte> salt;
        slice<const byte> personalization;
    };

    namespace detail {
        template<class Derived, class Tr, size_t Size>
        class Blake2 : public hash::mixin::hasher<Derived> {
            friend struct HashAccess;
            friend class hash::mixin::hasher<Derived>;

        public:
            using hash::mixin::hasher<Derived>::update;

            static constexpr size_t digest_size = Size;
            static constexpr size_t block_size = Tr::block;
            static constexpr size_t max_key_size = Tr::max_key;

            // Unkeyed, no salt, no personalization
            SGCL_INLINE_HOT Blake2() noexcept {
                _state.init(Size, nullptr, 0, nullptr, 0, nullptr, 0);
            }

            // With a key, a salt, a personalization; any of them longer
            // than its field is std::invalid_argument
            explicit Blake2(const blake2_options& o) {
                _check(o);
                _init(o);
            }

            Blake2(const Blake2&) noexcept = default;
            Blake2& operator=(const Blake2&) noexcept = default;

            // A keyed state is the key's equivalent: zeroed. The branch is
            // on whether a key was given, which is public
            SGCL_INLINE_HOT ~Blake2() {
                if (_state.key_size != 0) {
                    secure_zero_object(_state);
                }
            }

            SGCL_INLINE_HOT void update(const slice<const byte>& data) noexcept {
                _state.update(bytes(data.data()), data.size());
            }

            // The digest of everything so far; the hasher goes on
            SGCL_INLINE_HOT array<byte, Size> value() const noexcept {
                Derived h = static_cast<const Derived&>(*this);
                array<byte, Size> out;
                h._finish(bytes(out.data()));
                return out;
            }

            SGCL_INLINE_HOT array<byte, Size> digest() const noexcept {
                return value();
            }

            // As just made: the message dropped, the key, salt and
            // personalization kept
            SGCL_INLINE_HOT void reset() noexcept {
                _state.reset();
            }

            // Whether tag is the digest (the MAC, when keyed) of the message
            // so far, compared in constant time; a tag of another length is
            // false. [[nodiscard]]: a check whose result is dropped was
            // never made
            [[nodiscard]] SGCL_INLINE_HOT bool verify(const slice<const byte>& tag) const noexcept {
                array<byte, Size> mine = value();
                bool ok = constant_time::equal(mine, tag);
                secure_zero(mine.data(), mine.size());
                return ok;
            }

            // The one-shot form with options: data first, as every keyed
            // hasher has it (hmac::of(data, key)); the state on the stack
            // zeroed before the return when keyed
            static array<byte, Size> of(const slice<const byte>& data, const blake2_options& o) {
                Derived h(o);
                h.update(data);
                array<byte, Size> out;
                h._finish(bytes(out.data()));
                return out;
            }

            using hash::mixin::hasher<Derived>::of;

        protected:
            Blake2State<Tr> _state;

        private:
            SGCL_INLINE_HOT void _finish(unsigned char* out) noexcept {
                _state.finish(out);
            }

            static void _check(const blake2_options& o) {
                if (o.key.size() > Tr::max_key) {
                    throw invalid_argument("sgcl::crypto::blake2: a key longer than the hash takes");
                }
                if (o.salt.size() > Tr::salt) {
                    throw invalid_argument("sgcl::crypto::blake2: a salt longer than its field");
                }
                if (o.personalization.size() > Tr::salt) {
                    throw invalid_argument("sgcl::crypto::blake2: a personalization longer than its field");
                }
            }

            SGCL_INLINE_HOT void _init(const blake2_options& o) noexcept {
                _state.init(Size, bytes(o.key.data()), o.key.size(), bytes(o.salt.data()), o.salt.size(),
                            bytes(o.personalization.data()), o.personalization.size());
            }
        };
    }

    // BLAKE2b with a digest of 64 bytes, Go's blake2b.New512 and Sum512
    class blake2b_512 : public detail::Blake2<blake2b_512, detail::Blake2bTraits, 64> {
    public:
        using Blake2::Blake2;
    };

    // BLAKE2b with a digest of 48 bytes, Go's blake2b.New384
    class blake2b_384 : public detail::Blake2<blake2b_384, detail::Blake2bTraits, 48> {
    public:
        using Blake2::Blake2;
    };

    // BLAKE2b with a digest of 32 bytes, Go's blake2b.New256
    class blake2b_256 : public detail::Blake2<blake2b_256, detail::Blake2bTraits, 32> {
    public:
        using Blake2::Blake2;
    };

    // BLAKE2s with a digest of 32 bytes, Go's blake2s.New256: WireGuard's
    // and Noise's hash
    class blake2s_256 : public detail::Blake2<blake2s_256, detail::Blake2sTraits, 32> {
    public:
        using Blake2::Blake2;
    };

    // BLAKE2s with a digest of 16 bytes, Go's blake2s.New128: a MAC of 16
    // bytes (WireGuard's mac1 and mac2) when keyed
    class blake2s_128 : public detail::Blake2<blake2s_128, detail::Blake2sTraits, 16> {
    public:
        using Blake2::Blake2;
    };

    namespace detail {
        template<>
        inline constexpr bool crypto_digest<blake2b_512> = true;

        template<>
        inline constexpr bool crypto_digest<blake2b_384> = true;

        template<>
        inline constexpr bool crypto_digest<blake2b_256> = true;

        template<>
        inline constexpr bool crypto_digest<blake2s_256> = true;

        template<>
        inline constexpr bool crypto_digest<blake2s_128> = true;
    }
}
