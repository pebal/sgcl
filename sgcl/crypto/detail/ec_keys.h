//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "der.h"
#include "key_pem.h"
#include "ec_curve.h"
#include "keys.h"
#include "../error.h"
#include "../hash_id.h"
#include "../hmac.h"
#include "../random.h"
#include "../secret.h"
#include "../secure_zero.h"
#include "../sha1.h"
#include "../sha256.h"
#include "../sha512.h"
#include "../../core/aliases.h"
#include "../../core/array.h"
#include "../../core/detail/bytes.h"
#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// The keys of the NIST curves, generic over the curve, named per curve by
// p256.h and p384.h: a public key (a point, for ECDSA verification and as
// an ECDH peer), an ECDSA private key and an ECDH private key, apart as Go
// has crypto/ecdsa and crypto/ecdh (a key is used for one purpose), the
// one turned into the other with to_ecdh().
//
// What is secret and what is not. The private scalar d, the per-signature
// scalar k and its inverse, the HMAC-DRBG state that makes k, and the
// shared secret are: they go only through the constant-time arithmetic of
// ec_field.h and ec_curve.h and are zeroed when their scope ends. A public
// key, a signature, a digest, the coordinates of k*G (r is public) and the
// parse of any encoding are not: decompressing a point, checking it is on
// the curve and verifying a signature may take time that depends on them.
// The outcome of a check on a secret (a candidate k out of range, a
// scalar of zero) is public too: it only makes the loop draw again.
namespace sgcl::crypto {
    // The tag of ECDSA's deterministic signature (RFC 6979 without the
    // random bytes): sign_digest(digest, crypto::deterministic)
    struct deterministic_t {
        explicit deterministic_t() = default;
    };
    inline constexpr deterministic_t deterministic{};
}

namespace sgcl::crypto::detail {
    // id-ecPublicKey, 1.2.840.10045.2.1 (RFC 5480 §2.1.1)
    inline constexpr unsigned char oid_ec_public_key[] = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};

    template<class C>
    class EcPublicKey;

    template<class C>
    class EcdsaPrivateKey;

    template<class C>
    class EcdhKey;

    SGCL_INLINE_HOT slice<const byte> view(const unsigned char* p, size_t n) noexcept {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    // The digest the hedged signature's HMAC-DRBG runs on: of the curve's
    // strength, as FIPS 186-5 §6.4 asks of the generator of k
    template<class C>
    struct EcdsaDrbgHash;

    template<>
    struct EcdsaDrbgHash<P256> {
        using type = sha256;
    };

    template<>
    struct EcdsaDrbgHash<P384> {
        using type = sha384;
    };

    template<>
    struct EcdsaDrbgHash<P521> {
        using type = sha512;
    };

    // HMAC-DRBG as RFC 6979 §3.2 runs it (steps b to h) over the private
    // key x and the reduced digest h1, with additional data k' after them
    // (§3.6): empty k' gives RFC 6979's deterministic k, random k' the
    // hedged k Go's ecdsa makes — deterministic in the key and the digest
    // if the random bytes were bad, unpredictable if they were good.
    // Everything here is secret: K and V are zeroed when the generator goes
    template<class H>
    class HmacDrbg {
    public:
        static constexpr size_t hlen = H::digest_size;

        SGCL_INLINE_HOT HmacDrbg(const unsigned char* x, const unsigned char* h1, size_t rlen, const unsigned char* extra, size_t elen) noexcept {
            std::memset(_v, 0x01, hlen);
            std::memset(_k, 0x00, hlen);
            _seed(0x00, x, h1, rlen, extra, elen);
            _seed(0x01, x, h1, rlen, extra, elen);
        }

        HmacDrbg(const HmacDrbg&) = delete;
        HmacDrbg& operator=(const HmacDrbg&) = delete;

        SGCL_INLINE_HOT ~HmacDrbg() {
            secure_zero(_k, hlen);
            secure_zero(_v, hlen);
        }

        // The next candidate of rlen bytes (step h); after the first, K
        // and V move on first (step h.3)
        void next(unsigned char* out, size_t rlen) noexcept {
            if (!_first) {
                unsigned char zero = 0x00;
                hmac<H> m(view(_k, hlen));
                m.update(view(_v, hlen));
                m.update(view(&zero, 1));
                _take(m, _k);
                _mac_v();
            }
            _first = false;
            for (size_t t = 0; t < rlen;) {
                _mac_v();
                size_t n = rlen - t < hlen ? rlen - t : hlen;
                sgcl::detail::copy_bytes(out + t, _v, n);
                t += n;
            }
        }

    private:
        unsigned char _k[hlen];
        unsigned char _v[hlen];
        bool _first = true;

        // K = HMAC_K(V || sep || x || h1 || k'); V = HMAC_K(V)
        void _seed(unsigned char sep, const unsigned char* x, const unsigned char* h1, size_t rlen, const unsigned char* extra, size_t elen) noexcept {
            hmac<H> m(view(_k, hlen));
            m.update(view(_v, hlen));
            m.update(view(&sep, 1));
            m.update(view(x, rlen));
            m.update(view(h1, rlen));
            if (elen != 0) {
                m.update(view(extra, elen));
            }
            _take(m, _k);
            _mac_v();
        }

        SGCL_INLINE_HOT void _mac_v() noexcept {
            hmac<H> m(view(_k, hlen));
            m.update(view(_v, hlen));
            _take(m, _v);
        }

        SGCL_INLINE_HOT static void _take(const hmac<H>& m, unsigned char* out) noexcept {
            auto t = m.value();
            std::memcpy(out, t.data(), hlen);
            secure_zero(t.data(), hlen);
        }
    };

    // ECDSA over the curve (FIPS 186-5 §6.4), on scalars as words
    template<class C>
    struct Ecdsa {
        using E = Curve<C>;
        using F = typename E::F;
        using S = typename E::S;
        using sc = limbs<C::words>;
        static constexpr size_t size = C::size;

        // The leftmost bits of a digest, as many as n has (C::bits: 256,
        // 384, 521): the first size bytes of a longer digest, all of a
        // shorter one, and P-521's 66 bytes shifted down by the 7 bits past
        // its 521 (FIPS 186-5 §6.4.1 step 4, RFC 6979 §2.3.2); below
        // 2^bits, so below 2n
        SGCL_INLINE_HOT static sc bits2int(const unsigned char* digest, size_t len) noexcept {
            unsigned char b[size] = {};
            if (len >= size) {
                std::memcpy(b, digest, size);
            } else if (len != 0) {
                sgcl::detail::copy_bytes(b + size - len, digest, len);
            }
            sc r = ec_from_be<C>(b);
            if constexpr (8 * size != C::bits) {
                if (8 * len > C::bits) {
                    constexpr unsigned shift = unsigned(8 * size - C::bits);
                    for (size_t i = 0; i < C::words; ++i) {
                        r[i] = r[i] >> shift | (i + 1 < C::words ? r[i + 1] << (64 - shift) : 0);
                    }
                }
            }
            return r;
        }

        // a mod n for a below 2n, with no branch
        SGCL_INLINE_HOT static sc reduce_n(const sc& a) noexcept {
            sc r;
            S::reduce_once(r, a, 0);
            return r;
        }

        // Whether 1 <= k < n, computed in constant time; the answer is
        // public (a candidate rejected, a key refused)
        SGCL_INLINE_HOT static bool in_range(const sc& k) noexcept {
            uint64_t ok = limbs_less_mask(k, S::k.m) & ~limbs_zero_mask(k);
            return ct_barrier(ok) != 0;
        }

        // r || s of the digest under d (1 <= d < n), k from an HMAC-DRBG
        // over d, the digest and extra (RFC 6979 §3.2 and §3.6), drawn
        // again while k is out of range or r or s is zero
        template<class H>
        static void sign(unsigned char* sig, const sc& d, const unsigned char* digest, size_t dlen, const unsigned char* extra, size_t elen) noexcept {
            sc e = reduce_n(bits2int(digest, dlen));
            unsigned char x_oct[size];
            unsigned char h_oct[size];
            ec_to_be<C>(x_oct, d);
            ec_to_be<C>(h_oct, e);
            HmacDrbg<H> drbg(x_oct, h_oct, size, extra, elen);
            secure_zero(x_oct, size);
            sc dm = S::to_mont(d);
            sc em = S::to_mont(e);
            unsigned char kb[size];
            sc k, km, kinv, rm, t, sm, s, r;
            for (;;) {
                // k = bits2int(T): T's leftmost C::bits bits (P-521: the
                // 528 bits of T shifted by 7), its bytes again for base_mult
                drbg.next(kb, size);
                k = bits2int(kb, size);
                if constexpr (8 * size != C::bits) {
                    ec_to_be<C>(kb, k);
                }
                if (!in_range(k)) {
                    continue;
                }
                typename E::point p = E::base_mult(kb);
                typename E::affine a = E::to_affine(p);
                r = reduce_n(F::from_mont(a.x));
                if (limbs_zero_mask(r) != 0) {
                    continue;
                }
                // s = k^-1 (e + r d) mod n, in Montgomery form throughout
                km = S::to_mont(k);
                kinv = S::inverse(km);
                rm = S::to_mont(r);
                S::mul(t, rm, dm);
                S::add(t, t, em);
                S::mul(sm, kinv, t);
                s = S::from_mont(sm);
                if (limbs_zero_mask(s) != 0) {
                    continue;
                }
                break;
            }
            ec_to_be<C>(sig, r);
            ec_to_be<C>(sig + size, s);
            secure_zero(kb, size);
            secure_zero(&k, sizeof k);
            secure_zero(&km, sizeof km);
            secure_zero(&kinv, sizeof kinv);
            secure_zero(&dm, sizeof dm);
            secure_zero(&t, sizeof t);
            secure_zero(&sm, sizeof sm);
        }

        // The hedged signature of the public API: k' is size random bytes
        SGCL_INLINE_HOT static void sign_hedged(unsigned char* sig, const sc& d, const unsigned char* digest, size_t dlen) noexcept {
            unsigned char extra[size];
            random::fill(slice<byte>(reinterpret_cast<byte*>(extra), size));
            sign<typename EcdsaDrbgHash<C>::type>(sig, d, digest, dlen, extra, size);
            secure_zero(extra, size);
        }

        // RFC 6979's deterministic signature: its HMAC is the hash that made
        // the digest, known here only by the digest's length — SHA-1,
        // SHA-224, SHA-256, SHA-384 or SHA-512 for 20, 28, 32, 48 or 64
        // bytes (what OpenSSL and the RFC's vectors give), the curve's own
        // hash for any other length
        static void sign_deterministic(unsigned char* sig, const sc& d, const unsigned char* digest, size_t dlen) noexcept {
            switch (dlen) {
                case 20: sign<sha1>(sig, d, digest, dlen, nullptr, 0); break;
                case 28: sign<sha224>(sig, d, digest, dlen, nullptr, 0); break;
                case 32: sign<sha256>(sig, d, digest, dlen, nullptr, 0); break;
                case 48: sign<sha384>(sig, d, digest, dlen, nullptr, 0); break;
                case 64: sign<sha512>(sig, d, digest, dlen, nullptr, 0); break;
                default: sign<typename EcdsaDrbgHash<C>::type>(sig, d, digest, dlen, nullptr, 0); break;
            }
        }

        // The same with the hash named: SHA-3 and SHA-512/256 share their
        // lengths with SHA-2, and only the hash that made the digest gives
        // the k every other implementation gives
        SGCL_INLINE_HOT static void sign_deterministic(unsigned char* sig, const sc& d, const unsigned char* digest, size_t dlen, hash_id id) {
            detail::visit_hash(id, [&](auto t) {
                sign<typename decltype(t)::type>(sig, d, digest, dlen, nullptr, 0);
            });
        }

        // Whether (r, s) signs the digest under the public point q (affine,
        // Montgomery form): 1 <= r, s < n and x(u1 G + u2 Q) mod n = r.
        // Nothing here is secret
        static bool verify(const typename E::affine& q, const unsigned char* digest, size_t dlen, const unsigned char* rb, const unsigned char* sb) noexcept {
            sc r = ec_from_be<C>(rb);
            sc s = ec_from_be<C>(sb);
            if (!in_range(r) || !in_range(s)) {
                return false;
            }
            sc e = reduce_n(bits2int(digest, dlen));
            sc w = S::inverse(S::to_mont(s));   // s^-1 in Montgomery form
            // a plain number times a Montgomery one is the plain product
            sc u1, u2;
            S::mul(u1, e, w);
            S::mul(u2, r, w);
            unsigned char u1b[size];
            unsigned char u2b[size];
            ec_to_be<C>(u1b, u1);
            ec_to_be<C>(u2b, u2);
            typename E::jacobian x = E::double_mult_vartime(u1b, q, u2b);
            if (E::is_zero(x.z)) {
                return false;
            }
            typename F::element zz;
            F::sqr(zz, x.z);
            return x_matches(x.x, zz, r);
        }

        // Whether x mod n = r for x = X/D (a point not the identity: D = Z
        // projective, Z^2 Jacobian), without the inversion of D: x is below
        // p, so x mod n = r when X = r D, or when X = (r + n) D and r + n is
        // below p (a field inversion costs as much as 300 multiplications).
        // Public data
        static bool x_matches(const typename F::element& X, const typename F::element& D, const sc& r) noexcept {
            auto equal = [](const typename F::element& a, const typename F::element& b) noexcept {
                uint64_t diff = 0;
                for (size_t i = 0; i < C::words; ++i) {
                    diff |= a[i] ^ b[i];
                }
                return diff == 0;
            };
            typename F::element t;
            F::mul(t, F::to_mont(r), D);   // r < n < p: a field element as it is
            if (equal(t, X)) {
                return true;
            }
            sc rn;
            if (limbs_add(rn, r, S::k.m) != 0 || limbs_less_mask(rn, F::k.m) == 0) {
                return false;   // r + n is not below p
            }
            F::mul(t, F::to_mont(rn), D);
            return equal(t, X);
        }

        // An ECDSA-Sig-Value: SEQUENCE { INTEGER r, INTEGER s }, each in
        // its shortest form (RFC 5480 §2.2, Go's SignASN1)
        static constexpr size_t max_der_size = 2 + 2 * (2 + size + 1);

        static size_t encode_signature(unsigned char* out, const unsigned char* sig) noexcept {
            DerWriter<max_der_size> w;
            w.put_unsigned(sig + size, size);
            w.put_unsigned(sig, size);
            w.wrap(der::sequence, 0);
            sgcl::detail::copy_bytes(out, w.data(), w.size());
            return w.size();
        }

        // r and s of a DER signature, strictly: false for anything else,
        // anything after it included
        SGCL_INLINE_HOT static bool decode_signature(unsigned char* sig, const unsigned char* p, size_t n) noexcept {
            DerReader in(p, n);
            DerReader seq;
            return in.read(der::sequence, seq) && in.empty() && seq.read_unsigned(sig, size) && seq.read_unsigned(sig + size, size) && seq.empty();
        }
    };

    // What an ECDSA and an ECDH private key both are: the scalar d, secret,
    // and the public point d*G, kept so that public_key() costs nothing.
    // The parsers and writers of SEC 1 and PKCS#8 live here, for both
    template<class C>
    struct EcKeyCore {
        using E = Curve<C>;
        using F = typename E::F;
        using sc = limbs<C::words>;
        static constexpr size_t size = C::size;
        static constexpr size_t point_size = 1 + 2 * size;

        sc d{};
        std::array<unsigned char, point_size> pub{};

        EcKeyCore() noexcept = default;
        EcKeyCore(const EcKeyCore&) = delete;
        EcKeyCore& operator=(const EcKeyCore&) = delete;

        SGCL_INLINE_HOT EcKeyCore(EcKeyCore&& o) noexcept
        : d(o.d), pub(o.pub) {
            o.wipe();
        }

        SGCL_INLINE_HOT EcKeyCore& operator=(EcKeyCore&& o) noexcept {
            if (this != &o) {
                d = o.d;
                pub = o.pub;
                o.wipe();
            }
            return *this;
        }

        SGCL_INLINE_HOT ~EcKeyCore() {
            wipe();
        }

        // d and the public point zeroed: a key moved from, whose use is
        // std::logic_error (check), as every key of the module's
        SGCL_INLINE_HOT void wipe() noexcept {
            secure_zero(&d, sizeof d);
            pub.fill(0);
        }

        // A key's public point starts with 0x04; a key moved from has none.
        // kind is the type the message names, "private_key" or "ecdh_key"
        SGCL_INLINE_HOT void check(const char* kind) const {
            if (pub[0] == 0) {
                moved(kind);
            }
        }

        // Out of the line, so that check stays a load and a branch where
        // it is inlined
        [[noreturn]] static void moved(const char* kind) {
            moved_from((name() + "::" + kind).c_str());
        }

        SGCL_INLINE_HOT EcKeyCore clone() const noexcept {
            EcKeyCore c;
            c.d = d;
            c.pub = pub;
            return c;
        }

        // d from size big-endian bytes and the public point from it; false
        // (and the core zeroed) when d is 0 or not below n
        bool set(const unsigned char* bytes) noexcept {
            d = ec_from_be<C>(bytes);
            if (!Ecdsa<C>::in_range(d)) {
                wipe();
                return false;
            }
            typename E::affine a = E::to_affine(E::base_mult(bytes));
            pub[0] = 0x04;
            ec_to_be<C>(pub.data() + 1, F::from_mont(a.x));
            ec_to_be<C>(pub.data() + 1 + size, F::from_mont(a.y));
            return true;
        }

        SGCL_INLINE_HOT void scalar_bytes(unsigned char* out) const noexcept {
            ec_to_be<C>(out, d);
        }

        // "sgcl::crypto::p256", what every message of the curve starts with
        SGCL_INLINE_HOT static std::string name() noexcept {
            return std::string("sgcl::crypto::") + C::module;
        }

        SGCL_INLINE_HOT static std::string prefix() noexcept {
            return name() + ": ";
        }

        static error key_error(errc code, const std::string& what) noexcept {
            return error(code, string(prefix() + what));
        }

        static error der_error(errc code, size_t offset, const std::string& what) noexcept {
            return error(code, uint64_t(offset), string(prefix() + what));
        }

        // A random key: size random bytes (P-521's top byte cut to its one
        // bit of the 521) drawn again until they are a scalar in range (a
        // draw is refused with probability below 2^-32)
        static EcKeyCore generate() noexcept {
            EcKeyCore c;
            unsigned char b[size];
            do {
                random::fill(slice<byte>(reinterpret_cast<byte*>(b), size));
                if constexpr (8 * size != C::bits) {
                    b[0] &= static_cast<unsigned char>((1u << (C::bits - 8 * (size - 1))) - 1);
                }
            } while (!c.set(b));
            secure_zero(b, size);
            return c;
        }

        SGCL_INLINE_HOT static expected<EcKeyCore, error> from_bytes(const slice<const byte>& data) noexcept {
            if (data.size() != size) {
                return unexpected<error>(key_error(errc::invalid_key, "a private key is " + std::to_string(size) + " bytes"));
            }
            EcKeyCore c;
            if (!c.set(bytes(data.data()))) {
                return unexpected<error>(key_error(errc::invalid_key, "the private scalar is not in [1, n - 1]"));
            }
            return c;
        }

        // The ECPrivateKey of SEC 1 §C.4 at the reader, its curve given by
        // the parameters when they are there (they must be this curve's)
        static expected<EcKeyCore, error> read_ec_private_key(DerReader& in) noexcept {
            DerReader seq;
            if (!in.read(der::sequence, seq)) {
                return unexpected<error>(der_error(errc::malformed, in.offset(), "DER: an ECPrivateKey is a SEQUENCE"));
            }
            static constexpr unsigned char v1[] = {0x01};
            if (!seq.read_exact(der::integer, v1, 1)) {
                return unexpected<error>(der_error(errc::malformed, seq.offset(), "DER: an ECPrivateKey's version is 1"));
            }
            DerReader key;
            size_t key_offset = seq.offset();
            if (!seq.read(der::octet_string, key)) {
                return unexpected<error>(der_error(errc::malformed, key_offset, "DER: an ECPrivateKey's privateKey is an OCTET STRING"));
            }
            // SEC 1 has it exactly size bytes; as Go and OpenSSL, zeros of
            // padding on the left are dropped and missing ones added
            const unsigned char* kp = key.data();
            size_t kn = key.size();
            while (kn > size && *kp == 0) {
                ++kp;
                --kn;
            }
            if (seq.peek(der::context0)) {
                DerReader params;
                seq.read(der::context0, params);
                size_t at = params.offset();
                if (!params.read_exact(der::object_identifier, C::oid, sizeof C::oid) || !params.empty()) {
                    return unexpected<error>(der_error(errc::unsupported, at, "the key's parameters are not this curve's"));
                }
            }
            if (seq.peek(der::context1)) {
                DerReader pk;
                seq.read(der::context1, pk);
                DerReader bits;
                size_t at = pk.offset();
                if (!pk.read(der::bit_string, bits) || !pk.empty()) {
                    return unexpected<error>(der_error(errc::malformed, at, "DER: an ECPrivateKey's publicKey is a BIT STRING"));
                }
            }
            if (!seq.empty()) {
                return unexpected<error>(der_error(errc::malformed, seq.offset(), "DER: data after an ECPrivateKey's fields"));
            }
            // after the parameters: another curve's key is unsupported
            // before its scalar is too long for this one
            if (kn > size) {
                return unexpected<error>(der_error(errc::invalid_key, key_offset, "the private scalar is longer than the curve's order"));
            }
            unsigned char b[size] = {};
            sgcl::detail::copy_bytes(b + size - kn, kp, kn);
            EcKeyCore c;
            bool ok = c.set(b);
            secure_zero(b, size);
            if (!ok) {
                return unexpected<error>(der_error(errc::invalid_key, key_offset, "the private scalar is not in [1, n - 1]"));
            }
            return c;
        }

        SGCL_INLINE_HOT static expected<EcKeyCore, error> from_sec1_der(const slice<const byte>& data) noexcept {
            DerReader in(bytes(data.data()), data.size());
            auto c = read_ec_private_key(in);
            if (c && !in.empty()) {
                return unexpected<error>(der_error(errc::malformed, in.offset(), "DER: data after the key"));
            }
            return c;
        }

        // The AlgorithmIdentifier { id-ecPublicKey, namedCurve } of this
        // curve: another algorithm or another curve is unsupported
        static expected<void, error> read_algorithm(DerReader& in) noexcept {
            DerReader alg;
            size_t at = in.offset();
            if (!in.read(der::sequence, alg)) {
                return unexpected<error>(der_error(errc::malformed, at, "DER: an AlgorithmIdentifier is a SEQUENCE"));
            }
            if (!alg.read_exact(der::object_identifier, oid_ec_public_key, sizeof oid_ec_public_key)) {
                return unexpected<error>(der_error(errc::unsupported, alg.offset(), "not an elliptic-curve key"));
            }
            if (!alg.read_exact(der::object_identifier, C::oid, sizeof C::oid) || !alg.empty()) {
                return unexpected<error>(der_error(errc::unsupported, alg.offset(), std::string("not a ") + C::name + " key"));
            }
            return {};
        }

        template<size_t Cap>
        static void write_algorithm(DerWriter<Cap>& w) noexcept {
            size_t mark = w.size();
            w.put(C::oid, sizeof C::oid);
            w.wrap(der::object_identifier, w.size() - sizeof C::oid);
            size_t m2 = w.size();
            w.put(oid_ec_public_key, sizeof oid_ec_public_key);
            w.wrap(der::object_identifier, m2);
            w.wrap(der::sequence, mark);
        }

        // PKCS#8 PrivateKeyInfo (RFC 5208), or its version 2
        // OneAsymmetricKey (RFC 5958) with attributes and a public key after
        // the private one, which are skipped
        static expected<EcKeyCore, error> from_pkcs8_der(const slice<const byte>& data) noexcept {
            DerReader in(bytes(data.data()), data.size());
            DerReader seq;
            if (!in.read(der::sequence, seq) || !in.empty()) {
                return unexpected<error>(der_error(errc::malformed, in.offset(), "DER: a PKCS#8 key is one SEQUENCE"));
            }
            static constexpr unsigned char v0[] = {0x00};
            static constexpr unsigned char v1[] = {0x01};
            if (!seq.read_exact(der::integer, v0, 1) && !seq.read_exact(der::integer, v1, 1)) {
                return unexpected<error>(der_error(errc::malformed, seq.offset(), "DER: a PKCS#8 version is 0 or 1"));
            }
            if (auto a = read_algorithm(seq); !a) {
                return unexpected<error>(a.error());
            }
            DerReader inner;
            size_t at = seq.offset();
            if (!seq.read(der::octet_string, inner)) {
                return unexpected<error>(der_error(errc::malformed, at, "DER: a PKCS#8 privateKey is an OCTET STRING"));
            }
            auto c = read_ec_private_key(inner);
            if (!c) {
                return c;
            }
            if (!inner.empty()) {
                return unexpected<error>(der_error(errc::malformed, inner.offset(), "DER: data after the ECPrivateKey"));
            }
            DerReader skip;
            if (seq.peek(der::context0)) {
                seq.read(der::context0, skip);
            }
            if (seq.peek(0x81)) {   // [1] IMPLICIT BIT STRING, primitive
                seq.read(0x81, skip);
            }
            if (!seq.empty()) {
                return unexpected<error>(der_error(errc::malformed, seq.offset(), "DER: data after a PKCS#8 key's fields"));
            }
            return c;
        }

        // The ECPrivateKey: version 1, the scalar, the parameters when
        // asked for (SEC 1 alone has them; PKCS#8 names the curve outside,
        // as Go and OpenSSL write it), the public key
        template<size_t Cap>
        void write_ec_private_key(DerWriter<Cap>& w, bool with_parameters) const noexcept {
            size_t mark = w.size();
            size_t m = w.size();
            w.put(pub.data(), point_size);
            w.put(0);
            w.wrap(der::bit_string, m);
            w.wrap(der::context1, m);
            if (with_parameters) {
                m = w.size();
                w.put(C::oid, sizeof C::oid);
                w.wrap(der::object_identifier, m);
                w.wrap(der::context0, m);
            }
            m = w.size();
            unsigned char b[size];
            scalar_bytes(b);
            w.put(b, size);
            secure_zero(b, size);
            w.wrap(der::octet_string, m);
            static constexpr unsigned char version[] = {der::integer, 0x01, 0x01};
            w.put(version, sizeof version);
            w.wrap(der::sequence, mark);
        }

        template<size_t Cap>
        SGCL_INLINE_HOT static vector<byte> take(const DerWriter<Cap>& w) noexcept {
            const byte* p = reinterpret_cast<const byte*>(w.data());
            return vector<byte>(p, p + w.size());
        }

        // The key is checked by the caller (a key moved from is refused)
        SGCL_INLINE_HOT secret_bytes to_sec1_der() const noexcept {
            DerWriter<256> w;
            write_ec_private_key(w, true);
            return take_secret(w);
        }

        secret_bytes to_pkcs8_der() const noexcept {
            DerWriter<256> w;
            size_t mark = w.size();
            write_ec_private_key(w, false);
            w.wrap(der::octet_string, mark);
            write_algorithm(w);
            static constexpr unsigned char version[] = {der::integer, 0x01, 0x00};
            w.put(version, sizeof version);
            w.wrap(der::sequence, mark);
            return take_secret(w);
        }
    };

    // A public key of the curve: a point other than the identity, checked
    // to be on the curve when it is made from bytes. Not a secret: a plain
    // value, copied and compared freely. Used to verify ECDSA signatures
    // and as the peer of an ECDH key
    template<class C>
    class EcPublicKey {
        friend class EcdsaPrivateKey<C>;
        friend class EcdhKey<C>;
        using E = Curve<C>;
        using F = typename E::F;
        using Core = EcKeyCore<C>;

    public:
        static constexpr size_t size = 1 + 2 * C::size;          // SEC 1, uncompressed: 04 || X || Y
        static constexpr size_t compressed_size = 1 + C::size;   // 02 or 03 (the parity of Y) || X
        static constexpr size_t signature_size = 2 * C::size;    // r || s, as verify_digest_raw takes it

        // A point in SEC 1's encoding (§2.3.3), uncompressed or compressed.
        // errc::invalid_key for any other length or first byte, the point
        // at infinity (a single 00), a coordinate not below p, a point off
        // the curve, an x with no y on it
        static expected<EcPublicKey, error> from_bytes(const slice<const byte>& data) noexcept {
            const unsigned char* p = detail::bytes(data.data());
            size_t n = data.size();
            using fe = typename E::fe;
            constexpr size_t cs = C::size;
            if (n == 1 && p[0] == 0x00) {
                return unexpected<error>(Core::key_error(errc::invalid_key, "the point at infinity is not a public key"));
            }
            fe x;
            fe y;
            if (n == size && p[0] == 0x04) {
                x = ec_from_be<C>(p + 1);
                y = ec_from_be<C>(p + 1 + cs);
                if (!limbs_less_mask(x, F::k.m) || !limbs_less_mask(y, F::k.m)) {
                    return unexpected<error>(Core::key_error(errc::invalid_key, "a coordinate is not below p"));
                }
                x = F::to_mont(x);
                y = F::to_mont(y);
                if (!E::on_curve(x, y)) {
                    return unexpected<error>(Core::key_error(errc::invalid_key, "the point is not on the curve"));
                }
            } else if (n == compressed_size && (p[0] == 0x02 || p[0] == 0x03)) {
                x = ec_from_be<C>(p + 1);
                if (!limbs_less_mask(x, F::k.m)) {
                    return unexpected<error>(Core::key_error(errc::invalid_key, "a coordinate is not below p"));
                }
                x = F::to_mont(x);
                if (!E::sqrt(y, E::rhs(x))) {
                    return unexpected<error>(Core::key_error(errc::invalid_key, "no point of the curve has this x"));
                }
                fe plain = F::from_mont(y);
                if ((plain[0] & 1) != (p[0] & 1u)) {
                    F::sub(y, fe{}, y);
                }
            } else {
                return unexpected<error>(Core::key_error(errc::invalid_key, "not a SEC 1 point: 04 || X || Y or 02/03 || X"));
            }
            EcPublicKey k;
            k._point[0] = 0x04;
            ec_to_be<C>(k._point.data() + 1, F::from_mont(x));
            ec_to_be<C>(k._point.data() + 1 + cs, F::from_mont(y));
            return k;
        }

        // A SubjectPublicKeyInfo (RFC 5280 §4.1.2.7, RFC 5480) of this
        // curve: errc::malformed for DER that is not one, errc::unsupported
        // for another algorithm or curve, errc::invalid_key for a point
        // from_bytes refuses
        static expected<EcPublicKey, error> from_pkix_der(const slice<const byte>& data) noexcept {
            DerReader in(detail::bytes(data.data()), data.size());
            DerReader seq;
            if (!in.read(der::sequence, seq) || !in.empty()) {
                return unexpected<error>(Core::der_error(errc::malformed, in.offset(), "DER: a SubjectPublicKeyInfo is one SEQUENCE"));
            }
            if (auto a = Core::read_algorithm(seq); !a) {
                return unexpected<error>(a.error());
            }
            DerReader bits;
            size_t at = seq.offset();
            if (!seq.read(der::bit_string, bits) || !seq.empty() || bits.size() < 1 || bits.data()[0] != 0) {
                return unexpected<error>(Core::der_error(errc::malformed, at, "DER: the key is a BIT STRING of whole bytes"));
            }
            auto k = from_bytes(view(bits.data() + 1, bits.size() - 1));
            if (!k) {
                return unexpected<error>(error(errc::invalid_key, uint64_t(at), k.error().message()));
            }
            return k;
        }

        // 04 || X || Y
        SGCL_INLINE_HOT array<byte, size> bytes() const noexcept {
            array<byte, size> out;
            std::memcpy(out.data(), _point.data(), size);
            return out;
        }

        // 02 or 03 || X
        SGCL_INLINE_HOT array<byte, compressed_size> bytes_compressed() const noexcept {
            array<byte, compressed_size> out;
            out[0] = byte(0x02 | (_point[size - 1] & 1));
            std::memcpy(out.data() + 1, _point.data() + 1, C::size);
            return out;
        }

        // The SubjectPublicKeyInfo, uncompressed point, as Go's
        // x509.MarshalPKIXPublicKey writes it
        vector<byte> to_pkix_der() const noexcept {
            DerWriter<160> w;
            w.put(_point.data(), size);
            w.put(0);
            w.wrap(der::bit_string, 0);
            Core::write_algorithm(w);
            w.wrap(der::sequence, 0);
            return Core::take(w);
        }

        // Whether signature, an ECDSA-Sig-Value in DER (what sign_digest
        // gives, Go's VerifyASN1), signs digest under this key. The digest
        // is any length, its leftmost bits used as FIPS 186-5 has it. False
        // for a signature that is not strict DER, r or s out of [1, n - 1],
        // or one that does not verify; never an exception. [[nodiscard]]: a
        // check whose result is dropped was never made
        [[nodiscard]] SGCL_INLINE_HOT bool verify_digest(const slice<const byte>& digest, const slice<const byte>& signature) const noexcept {
            if (digest.size() == 0) {
                return false;   // as sign_digest refuses it
            }
            unsigned char sig[signature_size];
            if (!Ecdsa<C>::decode_signature(sig, detail::bytes(signature.data()), signature.size())) {
                return false;
            }
            return _verify(digest, sig);
        }

        // As verify_digest, the signature r || s of signature_size bytes
        // (the IEEE P1363 form of JWS, WebAuthn and PKCS#11); another
        // length is false
        [[nodiscard]] SGCL_INLINE_HOT bool verify_digest_raw(const slice<const byte>& digest, const slice<const byte>& signature) const noexcept {
            if (digest.size() == 0 || signature.size() != signature_size) {
                return false;
            }
            return _verify(digest, detail::bytes(signature.data()));
        }

        SGCL_INLINE_HOT friend bool operator==(const EcPublicKey& a, const EcPublicKey& b) noexcept {
            return a._point == b._point;
        }

    private:
        std::array<unsigned char, size> _point{};

        EcPublicKey() noexcept = default;

        SGCL_INLINE_HOT explicit EcPublicKey(const std::array<unsigned char, size>& point) noexcept
        : _point(point) {
        }

        SGCL_INLINE_HOT typename E::affine _affine() const noexcept {
            typename E::affine a;
            a.x = F::to_mont(ec_from_be<C>(_point.data() + 1));
            a.y = F::to_mont(ec_from_be<C>(_point.data() + 1 + C::size));
            return a;
        }

        SGCL_INLINE_HOT bool _verify(const slice<const byte>& digest, const unsigned char* sig) const noexcept {
            return Ecdsa<C>::verify(_affine(), detail::bytes(digest.data()), digest.size(), sig, sig + C::size);
        }
    };

    // An ECDSA private key of the curve (FIPS 186-5 §6): the secret scalar
    // d and the public point d*G. Move-only: clone() copies by name, a move
    // zeroes the source, the destructor zeroes d
    template<class C>
    class EcdsaPrivateKey {
        using Core = EcKeyCore<C>;

    public:
        static constexpr size_t size = C::size;                                   // the scalar's bytes
        static constexpr size_t signature_size = 2 * C::size;                     // r || s
        static constexpr size_t max_signature_size = Ecdsa<C>::max_der_size;      // the longest DER signature

        // A new key from crypto::random
        SGCL_INLINE_HOT static EcdsaPrivateKey generate() noexcept {
            return EcdsaPrivateKey(Core::generate());
        }

        // A key from its scalar: size bytes, big-endian, in [1, n - 1];
        // errc::invalid_key otherwise
        SGCL_INLINE_HOT static expected<EcdsaPrivateKey, error> from_bytes(const slice<const byte>& scalar) noexcept {
            return _wrap(Core::from_bytes(scalar));
        }

        // A PKCS#8 PrivateKeyInfo ("PRIVATE KEY" in PEM) of this curve
        SGCL_INLINE_HOT static expected<EcdsaPrivateKey, error> from_pkcs8_der(const slice<const byte>& der) noexcept {
            return _wrap(Core::from_pkcs8_der(der));
        }

        // A SEC 1 ECPrivateKey ("EC PRIVATE KEY" in PEM) of this curve
        SGCL_INLINE_HOT static expected<EcdsaPrivateKey, error> from_sec1_der(const slice<const byte>& der) noexcept {
            return _wrap(Core::from_sec1_der(der));
        }

        EcdsaPrivateKey(EcdsaPrivateKey&&) noexcept = default;
        EcdsaPrivateKey& operator=(EcdsaPrivateKey&&) noexcept = default;
        EcdsaPrivateKey(const EcdsaPrivateKey&) = delete;
        EcdsaPrivateKey& operator=(const EcdsaPrivateKey&) = delete;
        ~EcdsaPrivateKey() = default;

        SGCL_INLINE_HOT EcdsaPrivateKey clone() const {
            _check();
            return EcdsaPrivateKey(_core.clone());
        }

        // The scalar d, size bytes big-endian: a secret
        SGCL_INLINE_HOT secret<size> bytes() const {
            _check();
            secret<size> s = SecretAccess::make<size>();
            _core.scalar_bytes(SecretAccess::data(s));
            return s;
        }

        SGCL_INLINE_HOT EcPublicKey<C> public_key() const {
            _check();
            return EcPublicKey<C>(_core.pub);
        }

        // The same scalar as an ECDH key
        SGCL_INLINE_HOT EcdhKey<C> to_ecdh() const {
            _check();
            return EcdhKey<C>(_core.clone());
        }

        // The signature of a digest the program made (sha256::of(message),
        // or any other): an ECDSA-Sig-Value in DER, as Go's SignASN1 gives
        // it. The digest's leftmost bits are used, as many as the order n
        // has, and its length is not checked against any hash: a digest of
        // any size is signed as FIPS 186-5 §6.4.1 truncates it. k is
        // RFC 6979's with random bytes added (hedged): deterministic in the
        // key and the digest if the system's random bytes were bad, fresh
        // for every signature when they are good. An empty digest is
        // std::invalid_argument, as Go refuses it: the signature of nothing
        // is a program's mistake (arguments swapped, a digest never made)
        SGCL_INLINE_HOT vector<byte> sign_digest(const slice<const byte>& digest) const {
            unsigned char sig[signature_size];
            _sign(sig, digest, false);
            return _der(sig);
        }

        // RFC 6979's deterministic signature: the same key and digest give
        // the same signature, so it tells who sees two signatures whether
        // the digests were equal, and a fault of the hardware during
        // signing is not masked by fresh random bytes; for test vectors
        // and protocols that ask for it. The hedged form stays the default
        SGCL_INLINE_HOT vector<byte> sign_digest(const slice<const byte>& digest, deterministic_t) const {
            unsigned char sig[signature_size];
            _sign(sig, digest, true);
            return _der(sig);
        }

        // As sign_digest, r || s of signature_size bytes (IEEE P1363)
        SGCL_INLINE_HOT array<byte, signature_size> sign_digest_raw(const slice<const byte>& digest) const {
            array<byte, signature_size> out;
            _sign(detail::bytes(out.data()), digest, false);
            return out;
        }

        SGCL_INLINE_HOT array<byte, signature_size> sign_digest_raw(const slice<const byte>& digest, deterministic_t) const {
            array<byte, signature_size> out;
            _sign(detail::bytes(out.data()), digest, true);
            return out;
        }

        // The deterministic signature with the hash that made the digest
        // named, as RFC 6979 has it: needed for SHA-3 and SHA-512/256,
        // whose lengths the two-argument form reads as SHA-2's. A digest of
        // another length than the hash's is std::invalid_argument
        SGCL_INLINE_HOT vector<byte> sign_digest(const slice<const byte>& digest, deterministic_t, hash_id id) const {
            unsigned char sig[signature_size];
            _sign(sig, digest, id);
            return _der(sig);
        }

        SGCL_INLINE_HOT array<byte, signature_size> sign_digest_raw(const slice<const byte>& digest, deterministic_t, hash_id id) const {
            array<byte, signature_size> out;
            _sign(detail::bytes(out.data()), digest, id);
            return out;
        }

        // PKCS#8 PrivateKeyInfo, as Go's x509.MarshalPKCS8PrivateKey
        // writes it; the bytes hold the secret scalar: a secret_bytes,
        // never managed memory
        SGCL_INLINE_HOT secret_bytes to_pkcs8_der() const {
            _check();
            return _core.to_pkcs8_der();
        }

        // SEC 1 ECPrivateKey with the curve and the public key, as Go's
        // x509.MarshalECPrivateKey writes it; a secret_bytes too
        SGCL_INLINE_HOT secret_bytes to_sec1_der() const {
            _check();
            return _core.to_sec1_der();
        }

        // The key from PEM text (a file's bytes, read_secret's or the
        // program's): the first private key block, "PRIVATE KEY" or "EC PRIVATE KEY",
        // its base64 decoded straight into a secret_bytes (encoding::pem
        // would put the DER in managed memory). Text around the block is
        // passed over; an encrypted key is errc::unsupported
        SGCL_INLINE_HOT static expected<EcdsaPrivateKey, error> from_pem(const slice<const byte>& text) noexcept {
            auto p = detail::read_key_pem(text, Core::prefix());
            if (!p) {
                return unexpected<error>(p.error());
            }
            if (p->label == "PRIVATE KEY") {
                return from_pkcs8_der(p->der);
            }
            if (p->label == "EC PRIVATE KEY") {
                return from_sec1_der(p->der);
            }
            return unexpected<error>(Core::key_error(errc::malformed, "PEM: a block of another key's type"));
        }

        // The key as PEM, "PRIVATE KEY" over its PKCS #8, as Go's
        // pem.Encode of x509.MarshalPKCS8PrivateKey and OpenSSL's genpkey
        // write it: a secret_bytes, never managed memory
        SGCL_INLINE_HOT secret_bytes to_pem() const {
            return detail::write_key_pem("PRIVATE KEY", to_pkcs8_der());
        }


    private:
        friend class EcdhKey<C>;

        Core _core;

        SGCL_INLINE_HOT explicit EcdsaPrivateKey(Core&& core) noexcept
        : _core(std::move(core)) {
        }

        SGCL_INLINE_HOT void _check() const {
            _core.check("private_key");
        }

        SGCL_INLINE_HOT void _sign(unsigned char* sig, const slice<const byte>& digest, bool deterministic) const {
            _check();
            if (digest.size() == 0) {
                throw invalid_argument("sgcl::crypto::ecdsa: an empty digest");
            }
            if (deterministic) {
                Ecdsa<C>::sign_deterministic(sig, _core.d, detail::bytes(digest.data()), digest.size());
            } else {
                Ecdsa<C>::sign_hedged(sig, _core.d, detail::bytes(digest.data()), digest.size());
            }
        }

        SGCL_INLINE_HOT void _sign(unsigned char* sig, const slice<const byte>& digest, hash_id id) const {
            _check();
            if (digest.size() != digest_size(id)) {
                throw invalid_argument("sgcl::crypto::ecdsa: the digest is not of the hash's length");
            }
            Ecdsa<C>::sign_deterministic(sig, _core.d, detail::bytes(digest.data()), digest.size(), id);
        }

        SGCL_INLINE_HOT static vector<byte> _der(const unsigned char* sig) noexcept {
            unsigned char der[max_signature_size];
            size_t n = Ecdsa<C>::encode_signature(der, sig);
            const byte* p = reinterpret_cast<const byte*>(der);
            return vector<byte>(p, p + n);
        }

        SGCL_INLINE_HOT static expected<EcdsaPrivateKey, error> _wrap(expected<Core, error>&& c) noexcept {
            if (!c) {
                return unexpected<error>(c.error());
            }
            return EcdsaPrivateKey(std::move(*c));
        }
    };

    // An ECDH private key of the curve (SP 800-56A §5.7.1.2, Go's
    // crypto/ecdh): the secret scalar and the public point. Move-only, as
    // the ECDSA key; shared_secret is the x-coordinate of d*Q
    template<class C>
    class EcdhKey {
        using Core = EcKeyCore<C>;
        using E = Curve<C>;
        using F = typename E::F;

    public:
        static constexpr size_t size = C::size;

        SGCL_INLINE_HOT static EcdhKey generate() noexcept {
            return EcdhKey(Core::generate());
        }

        SGCL_INLINE_HOT static expected<EcdhKey, error> from_bytes(const slice<const byte>& scalar) noexcept {
            return _wrap(Core::from_bytes(scalar));
        }

        SGCL_INLINE_HOT static expected<EcdhKey, error> from_pkcs8_der(const slice<const byte>& der) noexcept {
            return _wrap(Core::from_pkcs8_der(der));
        }

        EcdhKey(EcdhKey&&) noexcept = default;
        EcdhKey& operator=(EcdhKey&&) noexcept = default;
        EcdhKey(const EcdhKey&) = delete;
        EcdhKey& operator=(const EcdhKey&) = delete;
        ~EcdhKey() = default;

        SGCL_INLINE_HOT EcdhKey clone() const {
            _check();
            return EcdhKey(_core.clone());
        }

        SGCL_INLINE_HOT secret<size> bytes() const {
            _check();
            secret<size> s = SecretAccess::make<size>();
            _core.scalar_bytes(SecretAccess::data(s));
            return s;
        }

        SGCL_INLINE_HOT EcPublicKey<C> public_key() const {
            _check();
            return EcPublicKey<C>(_core.pub);
        }

        // The x-coordinate of d * peer, size bytes big-endian (SEC 1
        // §3.3.1, what TLS and Go's ECDH give): a secret, to be put through
        // a key derivation (hkdf) before use. The peer is a public key, so
        // it is on the curve and not the identity; errc::invalid_key if the
        // product is the identity all the same (which a prime-order curve
        // does not allow for a scalar in range: a key moved from, zeroed)
        expected<secret<size>, error> shared_secret(const EcPublicKey<C>& peer) const {
            _check();
            unsigned char b[size];
            _core.scalar_bytes(b);
            typename E::point p = E::scalar_mult(typename E::point{F::to_mont(ec_from_be<C>(peer._point.data() + 1)), F::to_mont(ec_from_be<C>(peer._point.data() + 1 + size)), F::one()}, b);
            secure_zero(b, size);
            if (E::identity_mask(p) != 0) {
                return unexpected<error>(Core::key_error(errc::invalid_key, "the shared point is the identity"));
            }
            typename E::affine a = E::to_affine(p);
            secret<size> s = SecretAccess::make<size>();
            ec_to_be<C>(SecretAccess::data(s), F::from_mont(a.x));
            secure_zero(&p, sizeof p);
            secure_zero(&a, sizeof a);
            return s;
        }

        SGCL_INLINE_HOT secret_bytes to_pkcs8_der() const {
            _check();
            return _core.to_pkcs8_der();
        }

        // The key from PEM text (a file's bytes, read_secret's or the
        // program's): the first private key block, "PRIVATE KEY",
        // its base64 decoded straight into a secret_bytes (encoding::pem
        // would put the DER in managed memory). Text around the block is
        // passed over; an encrypted key is errc::unsupported
        SGCL_INLINE_HOT static expected<EcdhKey, error> from_pem(const slice<const byte>& text) noexcept {
            auto p = detail::read_key_pem(text, Core::prefix());
            if (!p) {
                return unexpected<error>(p.error());
            }
            if (p->label == "PRIVATE KEY") {
                return from_pkcs8_der(p->der);
            }
            return unexpected<error>(Core::key_error(errc::malformed, "PEM: a block of another key's type"));
        }

        // The key as PEM, "PRIVATE KEY" over its PKCS #8, as Go's
        // pem.Encode of x509.MarshalPKCS8PrivateKey and OpenSSL's genpkey
        // write it: a secret_bytes, never managed memory
        SGCL_INLINE_HOT secret_bytes to_pem() const {
            return detail::write_key_pem("PRIVATE KEY", to_pkcs8_der());
        }

    private:
        friend class EcdsaPrivateKey<C>;

        Core _core;

        SGCL_INLINE_HOT explicit EcdhKey(Core&& core) noexcept
        : _core(std::move(core)) {
        }

        SGCL_INLINE_HOT void _check() const {
            _core.check("ecdh_key");
        }

        SGCL_INLINE_HOT static expected<EcdhKey, error> _wrap(expected<Core, error>&& c) noexcept {
            if (!c) {
                return unexpected<error>(c.error());
            }
            return EcdhKey(std::move(*c));
        }
    };
}
