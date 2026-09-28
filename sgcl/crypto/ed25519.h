//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/der25519.h"
#include "detail/edwards25519.h"
#include "detail/keys.h"
#include "constant_time.h"
#include "error.h"
#include "random.h"
#include "secret.h"
#include "secure_zero.h"
#include "sha512.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// Ed25519 (RFC 8032 §5.1), Go's crypto/ed25519: signatures of 64 bytes
// under keys of 32, deterministic (the same key and message give the same
// signature, no random number at signing), the signature of SSH keys, of
// TLS 1.3 certificates, of package managers and of JWTs (EdDSA). Pure
// Ed25519 only: the message is signed as it is, not a digest of it
// (Ed25519ph and Ed25519ctx are not here, as they are not in Go's default).
//
// A private key is a seed of 32 random bytes. SHA-512 of the seed gives
// the secret scalar s (the first half, "clamped") and the prefix (the
// second half) the nonces are derived from; the public key is the point
// A = s·B, 32 bytes.
//
// Verification is Go's and RFC 8032's: S must be below L, the public key
// A must be a canonical encoding of a point on the curve (a y of p or more
// is refused, and so is x = 0 with the sign bit set: the strict decoding
// of RFC 8032 §5.1.3, where Go takes such an A reduced), R is compared as
// bytes with the encoding of S·B - k·A, so a non-canonical R never
// matches, and the equation is the one without the cofactor, [S]B = R +
// [k]A. Points of small order are not refused, as neither the RFC nor Go
// refuses them.
//
// What is secret: the seed, the scalar s, the prefix, the nonce r of each
// signature and every value computed from them until S = r + k·s mod L;
// the multiplication r·B and the scalar arithmetic are constant time
// (detail/edwards25519.h). The public key, the message, the signature and
// everything the verification computes are public, and the verification
// takes branches on them.
//
// The private key is a secret held in the object's own memory: move-only,
// clone() for a second one, zeroed on destruction and in an object moved
// from. seed() and bytes() are secret<32> and secret<64>, which zero
// themselves in turn.
namespace sgcl::crypto::ed25519 {
    inline constexpr size_t public_key_size = 32;
    inline constexpr size_t private_key_size = 64;   // the seed and the public key, as Go's PrivateKey
    inline constexpr size_t seed_size = 32;
    inline constexpr size_t signature_size = 64;

    namespace detail {
        using namespace sgcl::crypto::detail;
    }

    class private_key;

    // A public key: a point of the curve, kept both as its 32 bytes and
    // decompressed (negated, as the verification uses it). A value, copied
    // freely
    class public_key {
    public:
        // The key of 32 bytes: a canonical encoding of a point on the curve
        // (RFC 8032 §5.1.3), anything else invalid_key
        static expected<public_key, error> from_bytes(const slice<const byte>& bytes) {
            if (bytes.size() != public_key_size) {
                return unexpected<error>(error(errc::invalid_key, string("an Ed25519 public key is 32 bytes")));
            }
            public_key k;
            std::memcpy(k._bytes.data(), bytes.data(), public_key_size);
            detail::GeP3 a;
            if (!detail::ge_decode(a, detail::bytes(k._bytes.data()))) {
                return unexpected<error>(error(errc::invalid_key, string("not the encoding of a point of Ed25519")));
            }
            k._minus_a = detail::ge_neg(a);
            return k;
        }

        // The key from a SubjectPublicKeyInfo (RFC 8410, OID 1.3.101.112)
        static expected<public_key, error> from_pkix_der(const slice<const byte>& der) {
            unsigned char key[32];
            auto r = detail::der_read_pkix(der, detail::oid_ed25519, key);
            if (!r) {
                return unexpected<error>(r.error());
            }
            return from_bytes(slice<const byte>(reinterpret_cast<const byte*>(key), 32));
        }

        const array<byte, 32>& bytes() const noexcept {
            return _bytes;
        }

        // The SubjectPublicKeyInfo, 44 bytes
        vector<byte> to_pkix_der() const {
            return detail::der_pkix(detail::oid_ed25519, detail::bytes(_bytes.data()));
        }

        // Whether signature is this key's signature of message (bytes or
        // text). A signature of another length is false. [[nodiscard]]: a
        // check whose result is dropped was never made
        [[nodiscard]] bool verify(const slice<const byte>& message, const slice<const byte>& signature) const noexcept {
            return _verify(message, signature);
        }

        friend bool operator==(const public_key& a, const public_key& b) noexcept {
            return std::memcmp(a._bytes.data(), b._bytes.data(), public_key_size) == 0;
        }

    private:
        friend class private_key;

        array<byte, 32> _bytes{};
        detail::GeP3 _minus_a{};   // -A

        public_key() noexcept = default;

        bool _verify(const slice<const byte>& message, const slice<const byte>& signature) const noexcept {
            if (signature.size() != signature_size) {
                return false;
            }
            const unsigned char* sig = detail::bytes(signature.data());
            if (!detail::sc_is_canonical(sig + 32)) {
                return false;   // S >= L
            }
            // k = SHA-512(R || A || M) mod L
            unsigned char h[64], k[32];
            sha512 hash;
            hash.update(slice<const byte>(signature.data(), 32));
            hash.update(_bytes);
            hash.update(message);
            detail::HashAccess::finish(hash, h);
            detail::sc_reduce(k, h);
            // R' = S·B - k·A, its encoding compared with R
            detail::GeP2 r = detail::ge_double_scalarmult_vartime(k, _minus_a, sig + 32);
            unsigned char encoded[32];
            detail::ge_encode(encoded, r);
            return std::memcmp(encoded, sig, 32) == 0;
        }
    };

    // A private key: the seed, the scalar and the prefix it gives, and the
    // public key, all made once when the key is made
    class private_key {
    public:
        // A seed of 32 bytes from crypto::random
        static private_key generate() {
            unsigned char seed[32];
            random::fill(slice<byte>(reinterpret_cast<byte*>(seed), 32));
            private_key k(seed);
            detail::secure_zero(seed, sizeof seed);
            return k;
        }

        // The key of a seed of 32 bytes (RFC 8032's private key); another
        // length is invalid_key
        static expected<private_key, error> from_seed(const slice<const byte>& seed) {
            if (seed.size() != seed_size) {
                return unexpected<error>(error(errc::invalid_key, string("an Ed25519 seed is 32 bytes")));
            }
            return private_key(detail::bytes(seed.data()));
        }

        // The key of 64 bytes, the seed and the public key, as Go's
        // PrivateKey and bytes() hold them; a public half that is not the
        // one the seed gives is invalid_key (signing under a mismatched
        // pair would give away the key)
        static expected<private_key, error> from_private_bytes(const slice<const byte>& bytes) {
            if (bytes.size() != private_key_size) {
                return unexpected<error>(error(errc::invalid_key, string("an Ed25519 private key is 64 bytes")));
            }
            private_key k(detail::bytes(bytes.data()));
            if (std::memcmp(k._public._bytes.data(), bytes.data() + 32, 32) != 0) {
                return unexpected<error>(error(errc::invalid_key, string("the public half is not the seed's")));
            }
            return k;
        }

        // The key from a PKCS #8 PrivateKeyInfo (RFC 8410, OID
        // 1.3.101.112): the seed; a version 1 key whose public key is not
        // the seed's is invalid_key
        static expected<private_key, error> from_pkcs8_der(const slice<const byte>& der) {
            unsigned char seed[32], given[32];
            bool has_public = false;
            auto r = detail::der_read_pkcs8(der, detail::oid_ed25519, seed, has_public, given);
            if (!r) {
                return unexpected<error>(r.error());
            }
            private_key k(seed);
            detail::secure_zero(seed, sizeof seed);
            if (has_public && std::memcmp(given, k._public._bytes.data(), 32) != 0) {
                return unexpected<error>(error(errc::invalid_key, string("the public key is not the private key's")));
            }
            return k;
        }

        private_key(const private_key&) = delete;
        private_key& operator=(const private_key&) = delete;

        // The object moved from is zeroed and is no key of any use until
        // assigned again
        private_key(private_key&& other) noexcept
        : _seed(other._seed), _scalar(other._scalar), _prefix(other._prefix), _public(other._public) {
            other._wipe();
        }

        private_key& operator=(private_key&& other) noexcept {
            if (this != &other) {
                _seed = other._seed;
                _scalar = other._scalar;
                _prefix = other._prefix;
                _public = other._public;
                other._wipe();
            }
            return *this;
        }

        ~private_key() {
            _wipe();
        }

        // A second key of the same seed
        private_key clone() const noexcept {
            private_key k;
            k._seed = _seed;
            k._scalar = _scalar;
            k._prefix = _prefix;
            k._public = _public;
            return k;
        }

        ed25519::public_key public_key() const {
            _check();
            return _public;
        }

        // The seed, as a secret: move-only and zeroed when it goes
        secret<32> seed() const {
            _check();
            auto s = detail::SecretAccess::make<32>();
            std::memcpy(detail::SecretAccess::data(s), _seed.data(), 32);
            return s;
        }

        // The seed and the public key, 64 bytes (Go's PrivateKey), which
        // from_private_bytes takes back, as a secret
        secret<64> bytes() const {
            _check();
            auto s = detail::SecretAccess::make<64>();
            std::memcpy(detail::SecretAccess::data(s), _seed.data(), 32);
            std::memcpy(detail::SecretAccess::data(s) + 32, _public._bytes.data(), 32);
            return s;
        }

        // The signature of message (bytes or text), RFC 8032 §5.1.6:
        // r = SHA-512(prefix || M) mod L, R = r·B, k = SHA-512(R || A || M)
        // mod L, S = r + k·s mod L; R || S
        array<byte, 64> sign(const slice<const byte>& message) const {
            _check();
            array<byte, 64> out;
            _sign(detail::bytes(out.data()), message);
            return out;
        }

        // The PKCS #8 PrivateKeyInfo, 48 bytes, version 0 as Go and OpenSSL
        // write it. It holds the seed: a managed vector, which the caller
        // should zero (secure_zero) when done with it
        vector<byte> to_pkcs8_der() const {
            _check();
            return detail::der_pkcs8(detail::oid_ed25519, detail::bytes(_seed.data()));
        }

        // The same key, compared in constant time
        friend bool operator==(const private_key& a, const private_key& b) noexcept {
            return constant_time::equal(a._seed, b._seed);
        }

    private:
        array<byte, 32> _seed{};
        array<byte, 32> _scalar{};   // s, clamped
        array<byte, 32> _prefix{};
        ed25519::public_key _public;

        private_key() noexcept = default;

        // The key of a seed of 32 bytes (RFC 8032 §5.1.5)
        explicit private_key(const unsigned char* seed) noexcept {
            std::memcpy(_seed.data(), seed, 32);
            unsigned char h[64];
            sha512 hash;
            hash.update(slice<const byte>(reinterpret_cast<const byte*>(seed), 32));
            detail::HashAccess::finish(hash, h);
            detail::secure_zero_object(hash);
            h[0] &= 248;
            h[31] &= 127;
            h[31] |= 64;
            std::memcpy(_scalar.data(), h, 32);
            std::memcpy(_prefix.data(), h + 32, 32);
            detail::secure_zero(h, sizeof h);
            detail::GeP3 a = detail::ge_scalarmult_base(detail::bytes(_scalar.data()));
            detail::ge_encode(detail::bytes(_public._bytes.data()), a);
            _public._minus_a = detail::ge_neg(a);
        }

        void _sign(unsigned char* out, const slice<const byte>& message) const noexcept {
            unsigned char h[64], r[32], k[32];
            // r = SHA-512(prefix || M) mod L: the secret nonce
            sha512 hash;
            hash.update(_prefix);
            hash.update(message);
            detail::HashAccess::finish(hash, h);
            detail::sc_reduce(r, h);
            // R = r·B, in constant time
            detail::GeP3 big_r = detail::ge_scalarmult_base(r);
            detail::ge_encode(out, big_r);
            // k = SHA-512(R || A || M) mod L: public
            hash.reset();
            hash.update(slice<const byte>(reinterpret_cast<const byte*>(out), 32));
            hash.update(_public._bytes);
            hash.update(message);
            detail::HashAccess::finish(hash, h);
            detail::sc_reduce(k, h);
            // S = r + k·s mod L, in constant time
            detail::sc_muladd(out + 32, k, detail::bytes(_scalar.data()), r);
            detail::secure_zero(h, sizeof h);
            detail::secure_zero(r, sizeof r);
            detail::secure_zero_object(big_r);
            detail::secure_zero_object(hash);
        }

        // A key moved from: its public key, which no private key has, is
        // all zeros (wiped); every operation on it is std::logic_error, as
        // on every key of the module
        void _check() const {
            static constexpr array<byte, 32> none{};
            if (_public._bytes == none) {
                detail::moved_from("sgcl::crypto::ed25519::private_key");
            }
        }

        void _wipe() noexcept {
            detail::secure_zero_object(_seed);
            detail::secure_zero_object(_scalar);
            detail::secure_zero_object(_prefix);
            detail::secure_zero_object(_public);
        }
    };
}
