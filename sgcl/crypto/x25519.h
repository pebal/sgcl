//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/der25519.h"
#include "detail/edwards25519.h"
#include "detail/field25519.h"
#include "detail/keys.h"
#include "constant_time.h"
#include "error.h"
#include "random.h"
#include "secret.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// X25519 (RFC 7748), Go's crypto/ecdh.X25519(): Diffie–Hellman on
// Curve25519. Two parties each make a private key, send the other the
// public key, and each computes the same 32-byte shared secret from its
// own private key and the other's public key — the key exchange of TLS 1.3,
// SSH, WireGuard, Signal. The shared secret is input to a key derivation
// (hkdf), not a key itself.
//
// A private key is 32 random bytes; the scalar is those bytes "clamped"
// (RFC 7748 §5) each time it is used. A public key is any 32 bytes, the u
// coordinate of a point: every string is accepted, as the RFC and Go have
// it, bit 255 ignored and a value of p or more reduced; a public key of
// small order gives the all-zero shared secret, which shared_secret
// refuses (errc::invalid_key, RFC 7748 §6.1).
//
// What is secret: the private key's bytes, the clamped scalar, every
// value of the ladder and the shared secret. The ladder takes the same
// steps whatever the bits (a conditional swap by masks, never a branch),
// the field arithmetic is constant time (detail/field25519.h), and the
// public key is computed by the constant-time fixed-base multiplication of
// Ed25519 (detail/edwards25519.h) mapped to the Montgomery curve. The peer's
// public key and the check that the result is not zero are not secret.
//
// The private key is a secret held in the object's own memory: move-only,
// clone() for a second one, zeroed on destruction and in an object moved
// from (detail::secure_zero). The shared secret and bytes() are secret<32>,
// which zero themselves in turn.
namespace sgcl::crypto::x25519 {
    inline constexpr size_t public_key_size = 32;
    inline constexpr size_t private_key_size = 32;
    inline constexpr size_t shared_secret_size = 32;

    namespace detail {
        using namespace sgcl::crypto::detail;

        inline void clamp(unsigned char* k) noexcept {
            k[0] &= 248;
            k[31] &= 127;
            k[31] |= 64;
        }

        // RFC 7748 §5: the Montgomery ladder over the 255 bits of the
        // clamped scalar k, x-only, with a conditional swap by masks each
        // step. out = the u coordinate of k·u, canonical
        inline void scalarmult(unsigned char* out, const unsigned char* scalar, const unsigned char* point) noexcept {
            unsigned char k[32];
            std::memcpy(k, scalar, 32);
            clamp(k);
            Fe x1 = fe_from_bytes(point);   // bit 255 ignored
            Fe x2 = fe_one(), z2 = fe_zero();
            Fe x3 = x1, z3 = fe_one();
            uint64_t swap = 0;
            for (int t = 254; t >= 0; --t) {
                uint64_t bit = (k[t >> 3] >> (t & 7)) & 1;
                swap ^= bit;
                uint64_t m = mask_of(swap);
                fe_cswap(x2, x3, m);
                fe_cswap(z2, z3, m);
                swap = bit;
                Fe a = fe_add(x2, z2);
                Fe aa = fe_sq(a);
                Fe b = fe_sub(x2, z2);
                Fe bb = fe_sq(b);
                Fe e = fe_sub(aa, bb);
                Fe c = fe_add(x3, z3);
                Fe d = fe_sub(x3, z3);
                Fe da = fe_mul(d, a);
                Fe cb = fe_mul(c, b);
                x3 = fe_sq(fe_add(da, cb));
                z3 = fe_mul(x1, fe_sq(fe_sub(da, cb)));
                x2 = fe_mul(aa, bb);
                z2 = fe_mul(e, fe_add(aa, fe_mul_small(e, 121665)));
            }
            uint64_t m = mask_of(swap);
            fe_cswap(x2, x3, m);
            fe_cswap(z2, z3, m);
            fe_to_bytes(out, fe_mul(x2, fe_invert(z2)));
            secure_zero(k, sizeof k);
            fe_wipe(x2);
            fe_wipe(z2);
            fe_wipe(x3);
            fe_wipe(z3);
        }

        // k·9, the public key of a private key: the fixed-base
        // multiplication of Ed25519, whose base point is the one of u = 9,
        // mapped by u = (1 + y)/(1 - y) = (Z + Y)/(Z - Y). Constant time,
        // and five times faster than the ladder
        inline void scalarmult_base(unsigned char* out, const unsigned char* scalar) noexcept {
            unsigned char k[32];
            std::memcpy(k, scalar, 32);
            clamp(k);
            GeP3 p = ge_scalarmult_base(k);
            Fe u = fe_mul(fe_add(p.Z, p.Y), fe_invert(fe_sub(p.Z, p.Y)));
            fe_to_bytes(out, u);
            secure_zero(k, sizeof k);
            secure_zero_object(p);
        }
    }

    // A peer's public key: the u coordinate of a point, 32 bytes. Any 32
    // bytes are one (RFC 7748 §5); a value, copied freely
    class public_key {
    public:
        // The key of 32 bytes; another length is invalid_key
        static expected<public_key, error> from_bytes(const slice<const byte>& bytes) {
            if (bytes.size() != public_key_size) {
                return unexpected<error>(error(errc::invalid_key, string("an X25519 public key is 32 bytes")));
            }
            public_key k;
            std::memcpy(k._bytes.data(), bytes.data(), public_key_size);
            return k;
        }

        // The key from a SubjectPublicKeyInfo (RFC 8410, OID 1.3.101.110)
        static expected<public_key, error> from_pkix_der(const slice<const byte>& der) {
            public_key k;
            auto r = detail::der_read_pkix(der, detail::oid_x25519, detail::bytes(k._bytes.data()));
            if (!r) {
                return unexpected<error>(r.error());
            }
            return k;
        }

        const array<byte, 32>& bytes() const noexcept {
            return _bytes;
        }

        // The SubjectPublicKeyInfo, 44 bytes
        vector<byte> to_pkix_der() const {
            return detail::der_pkix(detail::oid_x25519, detail::bytes(_bytes.data()));
        }

        friend bool operator==(const public_key& a, const public_key& b) noexcept {
            return std::memcmp(a._bytes.data(), b._bytes.data(), public_key_size) == 0;
        }

    private:
        friend class private_key;

        array<byte, 32> _bytes{};

        public_key() noexcept = default;
    };

    // A private key: 32 secret bytes and the public key they give, which is
    // computed once when the key is made
    class private_key {
    public:
        // 32 bytes from crypto::random
        static private_key generate() {
            private_key k;
            random::fill(k._secret.as_slice());
            k._derive();
            return k;
        }

        // The key of 32 bytes (any 32 bytes are one; they are clamped when
        // used); another length is invalid_key
        static expected<private_key, error> from_bytes(const slice<const byte>& bytes) {
            if (bytes.size() != private_key_size) {
                return unexpected<error>(error(errc::invalid_key, string("an X25519 private key is 32 bytes")));
            }
            private_key k;
            std::memcpy(k._secret.data(), bytes.data(), private_key_size);
            k._derive();
            return k;
        }

        // The key from a PKCS #8 PrivateKeyInfo (RFC 8410, OID 1.3.101.110);
        // a version 1 key whose public key is not the one its private key
        // gives is invalid_key
        static expected<private_key, error> from_pkcs8_der(const slice<const byte>& der) {
            private_key k;
            bool has_public = false;
            unsigned char given[32];
            auto r = detail::der_read_pkcs8(der, detail::oid_x25519, detail::bytes(k._secret.data()), has_public, given);
            if (!r) {
                return unexpected<error>(r.error());
            }
            k._derive();
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
        : _secret(other._secret), _public(other._public) {
            other._wipe();
        }

        private_key& operator=(private_key&& other) noexcept {
            if (this != &other) {
                _secret = other._secret;
                _public = other._public;
                other._wipe();
            }
            return *this;
        }

        ~private_key() {
            _wipe();
        }

        // A second key of the same bytes
        private_key clone() const noexcept {
            private_key k;
            k._secret = _secret;
            k._public = _public;
            return k;
        }

        x25519::public_key public_key() const {
            _check();
            return _public;
        }

        // The 32 secret bytes, as given or generated (not clamped), as a
        // secret: move-only and zeroed when it goes
        secret<32> bytes() const {
            _check();
            auto s = detail::SecretAccess::make<32>();
            std::memcpy(detail::SecretAccess::data(s), _secret.data(), 32);
            return s;
        }

        // The secret shared with the owner of peer: X25519(private, peer).
        // The all-zero result of a peer's key of small order is
        // errc::invalid_key (RFC 7748 §6.1), found with no branch on the
        // secret bytes
        expected<secret<32>, error> shared_secret(const x25519::public_key& peer) const {
            _check();
            auto out = detail::SecretAccess::make<32>();
            detail::scalarmult(detail::SecretAccess::data(out), detail::bytes(_secret.data()), detail::bytes(peer._bytes.data()));
            const array<byte, 32> zero{};
            if (constant_time::equal(out.bytes(), zero)) {
                return unexpected<error>(error(errc::invalid_key, string("the shared secret is zero: the peer's key is of small order")));
            }
            return out;
        }

        // The PKCS #8 PrivateKeyInfo, 48 bytes, version 0 as Go and OpenSSL
        // write it. It holds the secret: a managed vector, which the caller
        // should zero (secure_zero) when done with it
        vector<byte> to_pkcs8_der() const {
            _check();
            return detail::der_pkcs8(detail::oid_x25519, detail::bytes(_secret.data()));
        }

        // The same key, compared in constant time
        friend bool operator==(const private_key& a, const private_key& b) noexcept {
            return constant_time::equal(a._secret, b._secret);
        }

    private:
        array<byte, 32> _secret{};
        x25519::public_key _public;

        private_key() noexcept = default;

        void _derive() noexcept {
            detail::scalarmult_base(detail::bytes(_public._bytes.data()), detail::bytes(_secret.data()));
        }

        // A key moved from: its public key, which no private key has, is
        // all zeros (wiped); every operation on it is std::logic_error, as
        // on every key of the module
        void _check() const {
            static constexpr array<byte, 32> none{};
            if (_public._bytes == none) {
                detail::moved_from("sgcl::crypto::x25519::private_key");
            }
        }

        void _wipe() noexcept {
            detail::secure_zero_object(_secret);
            detail::secure_zero_object(_public._bytes);
        }
    };
}
