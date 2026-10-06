//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "chacha20_poly1305.h"
#include "error.h"
#include "gcm.h"
#include "hkdf.h"
#include "hmac.h"
#include "p256.h"
#include "p384.h"
#include "p521.h"
#include "random.h"
#include "secret.h"
#include "secure_zero.h"
#include "sha256.h"
#include "sha512.h"
#include "x25519.h"
#include "../core/vector.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

// HPKE, Hybrid Public Key Encryption (RFC 9180): a message, or a stream of
// them, sealed to a recipient's public key — a KEM gives the two sides a
// shared secret, a key schedule turns it into an AEAD's key and nonces and
// a secret to export from. What TLS's Encrypted Client Hello (net::tls), MLS,
// Oblivious HTTP and DNS seal with; Go's crypto/hpke. The KEMs are
// DHKEM(X25519), DHKEM(P-256) and DHKEM(P-384) over the module's curves; the
// KDFs HKDF-SHA256/384/512; the AEADs AES-128-GCM, AES-256-GCM,
// ChaCha20-Poly1305 and export-only; the four modes, base, PSK, auth and
// auth-PSK. A context's key, nonce and exporter secret lie in the object,
// zeroed when it goes, never in managed memory. README: docs/sgcl/crypto/hpke.md
namespace sgcl::crypto::hpke {
    // The KEM of a suite (RFC 9180 §7.1), by its id
    enum class kem : uint16_t {
        dhkem_p256 = 0x0010,      // DHKEM(P-256, HKDF-SHA256)
        dhkem_p384 = 0x0011,      // DHKEM(P-384, HKDF-SHA384)
        dhkem_p521 = 0x0012,      // DHKEM(P-521, HKDF-SHA512)
        dhkem_x25519 = 0x0020     // DHKEM(X25519, HKDF-SHA256)
    };

    // The KDF of a suite (§7.2)
    enum class kdf : uint16_t {
        hkdf_sha256 = 0x0001,
        hkdf_sha384 = 0x0002,
        hkdf_sha512 = 0x0003
    };

    // The AEAD of a suite (§7.3); export_only seals nothing and exports
    enum class aead : uint16_t {
        aes128_gcm = 0x0001,
        aes256_gcm = 0x0002,
        chacha20_poly1305 = 0x0003,
        export_only = 0xffff
    };

    // The KDF and the AEAD of a ciphersuite; the KEM is the key's. The
    // default is ECH's and MLS's: HKDF-SHA256, AES-128-GCM
    struct suite {
        hpke::kdf kdf = hpke::kdf::hkdf_sha256;
        hpke::aead aead = hpke::aead::aes128_gcm;

        friend bool operator==(const suite&, const suite&) noexcept = default;
    };

    class public_key;
    class private_key;
    class sender;
    class recipient;
}

namespace sgcl::crypto::detail {
    using hpke_kem = hpke::kem;
    using hpke_kdf = hpke::kdf;
    using hpke_aead = hpke::aead;

    // The lengths of a KEM (§7.1): the shared secret, the encapsulation
    // (a public key's), the private key; and its KDF
    struct HpkeKemInfo {
        size_t secret;
        size_t enc;
        size_t sk;
        hpke_kdf kdf;
    };

    // A KEM of no value of its enumeration is a broken contract
    inline HpkeKemInfo hpke_kem_info(hpke_kem k) {
        switch (k) {
            case hpke_kem::dhkem_x25519: return {32, 32, 32, hpke_kdf::hkdf_sha256};
            case hpke_kem::dhkem_p256: return {32, 65, 32, hpke_kdf::hkdf_sha256};
            case hpke_kem::dhkem_p384: return {48, 97, 48, hpke_kdf::hkdf_sha384};
            case hpke_kem::dhkem_p521: return {64, 133, 66, hpke_kdf::hkdf_sha512};
        }
        throw invalid_argument("sgcl::crypto::hpke: a KEM of no value of its enumeration");
    }

    inline size_t hpke_nh(hpke_kdf k) {
        switch (k) {
            case hpke_kdf::hkdf_sha256: return 32;
            case hpke_kdf::hkdf_sha384: return 48;
            case hpke_kdf::hkdf_sha512: return 64;
        }
        throw invalid_argument("sgcl::crypto::hpke: a KDF of no value of its enumeration");
    }

    // Nk; 0 for export_only
    inline size_t hpke_nk(hpke_aead a) {
        switch (a) {
            case hpke_aead::aes128_gcm: return 16;
            case hpke_aead::aes256_gcm: return 32;
            case hpke_aead::chacha20_poly1305: return 32;
            case hpke_aead::export_only: return 0;
        }
        throw invalid_argument("sgcl::crypto::hpke: an AEAD of no value of its enumeration");
    }

    SGCL_INLINE_HOT slice<const byte> hpke_bytes(std::string_view s) noexcept {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
    }

    // The suite ids of the labels (§4.1, §5.1): "KEM" || kem, or "HPKE" ||
    // kem || kdf || aead
    struct HpkeSuiteId {
        unsigned char b[10];
        size_t n;

        SGCL_INLINE_HOT slice<const byte> bytes() const noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(b), n);
        }

        static HpkeSuiteId of_kem(hpke_kem k) noexcept {
            HpkeSuiteId s{{'K', 'E', 'M', uint8_t(uint16_t(k) >> 8), uint8_t(k)}, 5};
            return s;
        }

        static HpkeSuiteId of(hpke_kem k, const hpke::suite& u) noexcept {
            HpkeSuiteId s{{'H', 'P', 'K', 'E', uint8_t(uint16_t(k) >> 8), uint8_t(k), uint8_t(uint16_t(u.kdf) >> 8), uint8_t(u.kdf),
                           uint8_t(uint16_t(u.aead) >> 8), uint8_t(u.aead)},
                          10};
            return s;
        }
    };

    // LabeledExtract and LabeledExpand (§4) over one KDF: the extract an
    // HMAC keyed by the salt over the labeled input, read in pieces (the
    // input keying material, a secret, is never copied); the expand HKDF's
    // over the labeled info, which is public
    template<class H>
    struct HpkeKdfOf {
        static constexpr size_t nh = H::digest_size;

        static void extract(unsigned char* prk, const slice<const byte>& salt, const slice<const byte>& suite_id, std::string_view label,
                            const slice<const byte>& ikm) noexcept {
            hmac<H> m(salt);
            m.update(hpke_bytes("HPKE-v1"));
            m.update(suite_id);
            m.update(hpke_bytes(label));
            m.update(ikm);
            auto v = m.value();
            sgcl::detail::copy_bytes(prk, v.data(), nh);
            secure_zero(v.data(), v.size());
        }

        static void expand(unsigned char* out, size_t n, const unsigned char* prk, const slice<const byte>& suite_id, std::string_view label,
                           const slice<const byte>& info) {
            std::string full;
            full.reserve(2 + 7 + suite_id.size() + label.size() + info.size());
            full += char(n >> 8);
            full += char(n);
            full += "HPKE-v1";
            full.append(reinterpret_cast<const char*>(suite_id.data()), suite_id.size());
            full += label;
            full.append(reinterpret_cast<const char*>(info.data()), info.size());
            hkdf<H>::expand_to(slice<byte>(reinterpret_cast<byte*>(out), n), slice<const byte>(reinterpret_cast<const byte*>(prk), nh), hpke_bytes(full));
        }
    };

    // The same by a KDF named at run time
    template<class F>
    decltype(auto) hpke_visit_kdf(hpke_kdf k, F&& f) {
        switch (k) {
            case hpke_kdf::hkdf_sha256: return f(HpkeKdfOf<sha256>());
            case hpke_kdf::hkdf_sha384: return f(HpkeKdfOf<sha384>());
            case hpke_kdf::hkdf_sha512: return f(HpkeKdfOf<sha512>());
        }
        throw invalid_argument("sgcl::crypto::hpke: a KDF of no value of its enumeration");
    }

    inline void hpke_extract(hpke_kdf k, unsigned char* prk, const slice<const byte>& salt, const slice<const byte>& suite_id, std::string_view label,
                             const slice<const byte>& ikm) {
        hpke_visit_kdf(k, [&](auto f) { decltype(f)::extract(prk, salt, suite_id, label, ikm); });
    }

    inline void hpke_expand(hpke_kdf k, unsigned char* out, size_t n, const unsigned char* prk, const slice<const byte>& suite_id, std::string_view label,
                            const slice<const byte>& info) {
        hpke_visit_kdf(k, [&](auto f) { decltype(f)::expand(out, n, prk, suite_id, label, info); });
    }

    // A buffer of a secret on the stack, zeroed when it goes: a PRK, a key
    template<size_t N>
    struct HpkeSecret {
        unsigned char b[N];

        HpkeSecret() noexcept = default;
        HpkeSecret(const HpkeSecret&) = delete;
        HpkeSecret& operator=(const HpkeSecret&) = delete;

        ~HpkeSecret() {
            secure_zero(b, N);
        }

        SGCL_INLINE_HOT slice<const byte> bytes(size_t n) const noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(b), n);
        }
    };

    struct HpkeAccess;
    struct HpkeDerandomized;
}

namespace sgcl::crypto::hpke {
    // A KEM public key (§7.1.1): a value, copied and compared freely; the
    // recipient's, or an authenticated sender's
    class public_key {
    public:
        // DeserializePublicKey: an X25519 key of 32 bytes, a NIST point
        // uncompressed (65, 97 or 133 bytes), checked on its curve. errc::invalid_key
        // for anything else
        static expected<public_key, error> from_bytes(hpke::kem k, const slice<const byte>& bytes) noexcept {
            auto bad = [] {
                return unexpected(error(errc::invalid_key, string("sgcl::crypto::hpke: not a public key of the KEM")));
            };
            switch (k) {
                case kem::dhkem_x25519: {
                    if (bytes.size() != 32) {
                        return bad();
                    }
                    auto p = x25519::public_key::from_bytes(bytes);
                    if (!p) {
                        return bad();
                    }
                    return public_key(k, *p);
                }
                case kem::dhkem_p256: {
                    if (bytes.size() != 65) {
                        return bad();
                    }
                    auto p = p256::public_key::from_bytes(bytes);
                    if (!p) {
                        return bad();
                    }
                    return public_key(k, *p);
                }
                case kem::dhkem_p384: {
                    if (bytes.size() != 97) {
                        return bad();
                    }
                    auto p = p384::public_key::from_bytes(bytes);
                    if (!p) {
                        return bad();
                    }
                    return public_key(k, *p);
                }
                case kem::dhkem_p521: {
                    if (bytes.size() != 133) {
                        return bad();
                    }
                    auto p = p521::public_key::from_bytes(bytes);
                    if (!p) {
                        return bad();
                    }
                    return public_key(k, *p);
                }
            }
            return bad();
        }

        hpke::kem kem() const noexcept {
            return _kem;
        }

        // SerializePublicKey: 32 bytes of X25519, a point uncompressed
        vector<byte> bytes() const {
            return std::visit([](const auto& p) {
                auto b = p.bytes();
                return vector<byte>(b.data(), b.data() + b.size());
            }, _key);
        }

        friend bool operator==(const public_key& a, const public_key& b) noexcept {
            return a._kem == b._kem && a._key == b._key;
        }

    private:
        friend class private_key;
        friend struct sgcl::crypto::detail::HpkeAccess;
        hpke::kem _kem;
        std::variant<x25519::public_key, p256::public_key, p384::public_key, p521::public_key> _key;

        template<class K>
        public_key(hpke::kem k, const K& key) noexcept
        : _kem(k), _key(key) {
        }
    };

    // A KEM private key (§7.1): the recipient's, or the sender's of the
    // auth modes. Held in the object itself, move-only, zeroed when it goes,
    // as the module's keys are
    class private_key {
    public:
        // A new key of the KEM
        static private_key generate(hpke::kem k) {
            switch (k) {
                case kem::dhkem_x25519: return private_key(k, x25519::private_key::generate());
                case kem::dhkem_p256: return private_key(k, p256::ecdh_key::generate());
                case kem::dhkem_p384: return private_key(k, p384::ecdh_key::generate());
                case kem::dhkem_p521: return private_key(k, p521::ecdh_key::generate());
            }
            throw invalid_argument("sgcl::crypto::hpke: a KEM of no value of its enumeration");
        }

        // DeriveKeyPair (§7.1.3): the key of input keying material of at
        // least Nsk bytes of entropy, the same key every time
        static private_key derive(hpke::kem k, const slice<const byte>& ikm) {
            const auto info = detail::hpke_kem_info(k);
            const auto sid = detail::HpkeSuiteId::of_kem(k);
            detail::HpkeSecret<64> prk;
            detail::hpke_extract(info.kdf, prk.b, slice<const byte>(), sid.bytes(), "dkp_prk", ikm);
            detail::HpkeSecret<66> sk;
            if (k == kem::dhkem_x25519) {
                detail::hpke_expand(info.kdf, sk.b, 32, prk.b, sid.bytes(), "sk", slice<const byte>());
                return private_key(k, *x25519::private_key::from_bytes(sk.bytes(32)));
            }
            for (unsigned counter = 0; counter < 256; ++counter) {
                const unsigned char c = static_cast<unsigned char>(counter);
                detail::hpke_expand(info.kdf, sk.b, info.sk, prk.b, sid.bytes(), "candidate", slice<const byte>(reinterpret_cast<const byte*>(&c), 1));
                // the bitmask of P-256 and P-384 is 0xff, every bit kept;
                // P-521's 0x01, the first byte's top seven bits cleared
                if (k == kem::dhkem_p256) {
                    if (auto key = p256::ecdh_key::from_bytes(sk.bytes(32))) {
                        return private_key(k, std::move(*key));
                    }
                } else if (k == kem::dhkem_p384) {
                    if (auto key = p384::ecdh_key::from_bytes(sk.bytes(48))) {
                        return private_key(k, std::move(*key));
                    }
                } else {
                    sk.b[0] &= 0x01;
                    if (auto key = p521::ecdh_key::from_bytes(sk.bytes(66))) {
                        return private_key(k, std::move(*key));
                    }
                }
            }
            throw invalid_argument("sgcl::crypto::hpke::private_key::derive: no candidate in 256 (DeriveKeyPairError)");
        }

        // DeserializePrivateKey: 32 bytes of X25519 (clamped when used), a
        // scalar of the curve's size in [1, n - 1]; errc::invalid_key else
        static expected<private_key, error> from_bytes(hpke::kem k, const slice<const byte>& bytes) noexcept {
            auto bad = [] {
                return unexpected(error(errc::invalid_key, string("sgcl::crypto::hpke: not a private key of the KEM")));
            };
            switch (k) {
                case kem::dhkem_x25519: {
                    auto s = x25519::private_key::from_bytes(bytes);
                    if (!s) {
                        return bad();
                    }
                    return private_key(k, std::move(*s));
                }
                case kem::dhkem_p256: {
                    auto s = p256::ecdh_key::from_bytes(bytes);
                    if (!s) {
                        return bad();
                    }
                    return private_key(k, std::move(*s));
                }
                case kem::dhkem_p384: {
                    auto s = p384::ecdh_key::from_bytes(bytes);
                    if (!s) {
                        return bad();
                    }
                    return private_key(k, std::move(*s));
                }
                case kem::dhkem_p521: {
                    auto s = p521::ecdh_key::from_bytes(bytes);
                    if (!s) {
                        return bad();
                    }
                    return private_key(k, std::move(*s));
                }
            }
            return bad();
        }

        private_key(private_key&&) noexcept = default;
        private_key& operator=(private_key&&) noexcept = default;
        private_key(const private_key&) = delete;
        private_key& operator=(const private_key&) = delete;

        private_key clone() const {
            return std::visit([&](const auto& s) { return private_key(_kem, s.clone()); }, _key);
        }

        hpke::kem kem() const noexcept {
            return _kem;
        }

        // SerializePrivateKey: the scalar; X25519's clamped (§7.1.2)
        secret_bytes bytes() const {
            return std::visit([&](const auto& s) {
                auto b = s.bytes();
                secret_bytes out(b.size);
                auto* o = reinterpret_cast<unsigned char*>(out.as_slice().data());
                sgcl::detail::copy_bytes(o, b.bytes().data(), b.size);
                if (_kem == kem::dhkem_x25519) {
                    o[0] &= 248;
                    o[31] &= 127;
                    o[31] |= 64;
                }
                return out;
            }, _key);
        }

        hpke::public_key public_key() const {
            return std::visit([&](const auto& s) { return hpke::public_key(_kem, s.public_key()); }, _key);
        }

    private:
        friend struct sgcl::crypto::detail::HpkeAccess;
        hpke::kem _kem;
        std::variant<x25519::private_key, p256::ecdh_key, p384::ecdh_key, p521::ecdh_key> _key;

        template<class K>
        private_key(hpke::kem k, K&& key) noexcept
        : _kem(k), _key(std::forward<K>(key)) {
        }
    };

    // The inputs of the modes beyond base (§5.1): a pre-shared key and its
    // id (mode_psk), the sender's private key (mode_auth, the sender's side)
    // or its public key (the recipient's side); a PSK and a sender's key
    // together are mode_auth_psk. The slices are read during the call
    struct options {
        slice<const byte> psk;                         // empty: no pre-shared key
        slice<const byte> psk_id;                      // its id, given with it
    };
}

namespace sgcl::crypto::detail {
    // The keyed AEAD of a context, or none (export_only)
    using HpkeAead = std::variant<std::monostate, aes_gcm, chacha20_poly1305>;

    // What the key schedule gives a context (§5.1): the AEAD keyed, the base
    // nonce, the exporter secret, the sequence number. The bytes lie in the
    // object and are zeroed when it goes
    struct HpkeContext {
        hpke::kem kem = hpke::kem::dhkem_x25519;
        hpke::suite suite;
        HpkeAead aead;
        unsigned char base_nonce[12] = {};
        unsigned char exporter[64] = {};
        size_t nh = 0;
        uint64_t seq = 0;

        HpkeContext() noexcept = default;
        HpkeContext(const HpkeContext&) = delete;
        HpkeContext& operator=(const HpkeContext&) = delete;

        HpkeContext(HpkeContext&& o) noexcept
        : kem(o.kem), suite(o.suite), aead(std::move(o.aead)), nh(o.nh), seq(o.seq) {
            sgcl::detail::copy_bytes(base_nonce, o.base_nonce, 12);
            sgcl::detail::copy_bytes(exporter, o.exporter, 64);
            o._wipe();
        }

        HpkeContext& operator=(HpkeContext&& o) noexcept {
            if (this != &o) {
                kem = o.kem;
                suite = o.suite;
                aead = std::move(o.aead);
                nh = o.nh;
                seq = o.seq;
                sgcl::detail::copy_bytes(base_nonce, o.base_nonce, 12);
                sgcl::detail::copy_bytes(exporter, o.exporter, 64);
                o._wipe();
            }
            return *this;
        }

        ~HpkeContext() {
            _wipe();
        }

        // base_nonce XOR the sequence number, big-endian in its last bytes
        void nonce(unsigned char* out) const noexcept {
            sgcl::detail::copy_bytes(out, base_nonce, 12);
            for (int i = 0; i < 8; ++i) {
                out[11 - i] ^= static_cast<unsigned char>(seq >> (8 * i));
            }
        }

        void _wipe() noexcept {
            secure_zero(base_nonce, 12);
            secure_zero(exporter, 64);
            nh = 0;
        }
    };

    // KeySchedule (§5.1): the mode, the KEM's shared secret, the info and
    // the PSK into a context
    inline HpkeContext hpke_schedule(hpke_kem k, const hpke::suite& s, uint8_t mode, const slice<const byte>& shared, const slice<const byte>& info,
                                     const slice<const byte>& psk, const slice<const byte>& psk_id) {
        const auto sid = HpkeSuiteId::of(k, s);
        const size_t nh = hpke_nh(s.kdf);
        const size_t nk = hpke_nk(s.aead);
        unsigned char ksc[1 + 64 + 64];
        ksc[0] = mode;
        hpke_extract(s.kdf, ksc + 1, slice<const byte>(), sid.bytes(), "psk_id_hash", psk_id);
        hpke_extract(s.kdf, ksc + 1 + nh, slice<const byte>(), sid.bytes(), "info_hash", info);
        const slice<const byte> context(reinterpret_cast<const byte*>(ksc), 1 + 2 * nh);
        HpkeSecret<64> secret;
        hpke_extract(s.kdf, secret.b, shared, sid.bytes(), "secret", psk);
        HpkeContext c;
        c.kem = k;
        c.suite = s;
        c.nh = nh;
        if (nk) {
            HpkeSecret<32> key;
            hpke_expand(s.kdf, key.b, nk, secret.b, sid.bytes(), "key", context);
            if (s.aead == hpke::aead::chacha20_poly1305) {
                c.aead.emplace<chacha20_poly1305>(key.bytes(nk));
            } else {
                c.aead.emplace<aes_gcm>(key.bytes(nk));
            }
        }
        hpke_expand(s.kdf, c.base_nonce, 12, secret.b, sid.bytes(), "base_nonce", context);
        hpke_expand(s.kdf, c.exporter, nh, secret.b, sid.bytes(), "exp", context);
        return c;
    }

    struct HpkeAccess {
        // The DH of a private key with a public key of the same KEM, into
        // out (Nsecret bytes... the curve's size); errc::invalid_key for the
        // all-zero output of X25519 (a point of small order)
        static expected<size_t, error> dh(const hpke::private_key& sk, const hpke::public_key& pk, unsigned char* out) noexcept {
            auto put = [&](auto&& r) -> expected<size_t, error> {
                if (!r) {
                    return unexpected(error(errc::invalid_key, string("sgcl::crypto::hpke: the shared secret is zero (a point of small order)")));
                }
                sgcl::detail::copy_bytes(out, r->bytes().data(), r->size);
                return size_t(r->size);
            };
            switch (sk._key.index()) {
                case 0: return put(std::get<0>(sk._key).shared_secret(std::get<0>(pk._key)));
                case 1: return put(std::get<1>(sk._key).shared_secret(std::get<1>(pk._key)));
                case 2: return put(std::get<2>(sk._key).shared_secret(std::get<2>(pk._key)));
                default: return put(std::get<3>(sk._key).shared_secret(std::get<3>(pk._key)));
            }
        }
    };

    // ExtractAndExpand of the DH KEMs (§4.1): the shared secret of the DH
    // output and the KEM context
    inline secret_bytes hpke_kem_secret(hpke_kem k, const slice<const byte>& dh, const slice<const byte>& kem_context) {
        const auto info = hpke_kem_info(k);
        const auto sid = HpkeSuiteId::of_kem(k);
        HpkeSecret<64> prk;
        hpke_extract(info.kdf, prk.b, slice<const byte>(), sid.bytes(), "eae_prk", dh);
        secret_bytes out(info.secret);
        hpke_expand(info.kdf, reinterpret_cast<unsigned char*>(out.as_slice().data()), info.secret, prk.b, sid.bytes(), "shared_secret", kem_context);
        return out;
    }

    // Encap and AuthEncap (§4.1) with an ephemeral key: the shared secret
    // and enc
    inline expected<pair<secret_bytes, vector<byte>>, error> hpke_encap(const hpke::public_key& to, const hpke::private_key& eph, const hpke::private_key* sender) {
        HpkeSecret<132> dh;   // two DH outputs of the longest curve, P-521's
        auto n = HpkeAccess::dh(eph, to, dh.b);
        if (!n) {
            return unexpected(n.error());
        }
        size_t len = *n;
        if (sender) {
            auto m = HpkeAccess::dh(*sender, to, dh.b + len);
            if (!m) {
                return unexpected(m.error());
            }
            len += *m;
        }
        vector<byte> enc = eph.public_key().bytes();
        vector<byte> ctx = enc;
        for (auto b : to.bytes()) {
            ctx.push_back(b);
        }
        if (sender) {
            for (auto b : sender->public_key().bytes()) {
                ctx.push_back(b);
            }
        }
        auto shared = hpke_kem_secret(to.kem(), dh.bytes(len), ctx.as_slice());
        return pair<secret_bytes, vector<byte>>(std::move(shared), std::move(enc));
    }

    // Decap and AuthDecap
    inline expected<secret_bytes, error> hpke_decap(const slice<const byte>& enc, const hpke::private_key& key, const hpke::public_key* sender) noexcept {
        auto pe = hpke::public_key::from_bytes(key.kem(), enc);
        if (!pe) {
            return unexpected(error(errc::invalid_key, string("sgcl::crypto::hpke: enc is not a public key of the KEM")));
        }
        HpkeSecret<132> dh;   // two DH outputs of the longest curve, P-521's
        auto n = HpkeAccess::dh(key, *pe, dh.b);
        if (!n) {
            return unexpected(n.error());
        }
        size_t len = *n;
        if (sender) {
            auto m = HpkeAccess::dh(key, *sender, dh.b + len);
            if (!m) {
                return unexpected(m.error());
            }
            len += *m;
        }
        vector<byte> ctx(enc.data(), enc.data() + enc.size());
        for (auto b : key.public_key().bytes()) {
            ctx.push_back(b);
        }
        if (sender) {
            for (auto b : sender->bytes()) {
                ctx.push_back(b);
            }
        }
        return hpke_kem_secret(key.kem(), dh.bytes(len), ctx.as_slice());
    }

    // The mode (§5): base 0, psk 1, auth 2, auth_psk 3
    SGCL_INLINE_HOT uint8_t hpke_mode(const hpke::options& o, bool auth) noexcept {
        return uint8_t((o.psk.size() != 0 ? 1 : 0) | (auth ? 2 : 0));
    }

    // Export (§5.3)
    inline secret_bytes hpke_export(const HpkeContext& c, const slice<const byte>& context, size_t length) {
        if (c.nh == 0) {
            detail::moved_from("sgcl::crypto::hpke context");
        }
        if (length > 255 * c.nh) {
            throw invalid_argument("sgcl::crypto::hpke: an export longer than 255 Nh bytes");
        }
        secret_bytes out(length);
        hpke_expand(c.suite.kdf, reinterpret_cast<unsigned char*>(out.as_slice().data()), length, c.exporter, HpkeSuiteId::of(c.kem, c.suite).bytes(), "sec", context);
        return out;
    }
}

namespace sgcl::crypto::hpke {
    // A sending context (§5.2): made to a recipient's public key, it gives
    // enc() for the recipient and seals messages in order, each under the
    // next nonce. Move-only; its key, base nonce and exporter secret lie in
    // the object and are zeroed when it goes
    class sender {
    public:
        // SetupBaseS, with options SetupPSKS, with the sender's key
        // SetupAuthS, with both SetupAuthPSKS. errc::invalid_key for a
        // recipient's X25519 key of small order. A sender's key or a PSK
        // that does not fit (another KEM, a psk without its id or the
        // reverse, a KDF or an AEAD of no value of its enumeration) is
        // std::invalid_argument
        static expected<sender, error> setup(const public_key& to, const suite& s) {
            return setup(to, s, slice<const byte>(), options());
        }

        static expected<sender, error> setup(const public_key& to, const suite& s, const slice<const byte>& info) {
            return setup(to, s, info, options());
        }

        static expected<sender, error> setup(const public_key& to, const suite& s, const slice<const byte>& info, const options& o) {
            return _setup(to, s, info, o, nullptr, private_key::generate(to.kem()));
        }

        static expected<sender, error> setup(const public_key& to, const suite& s, const slice<const byte>& info, const private_key& sender_key) {
            return setup(to, s, info, options(), sender_key);
        }

        static expected<sender, error> setup(const public_key& to, const suite& s, const slice<const byte>& info, const options& o,
                                             const private_key& sender_key) {
            return _setup(to, s, info, o, &sender_key, private_key::generate(to.kem()));
        }

        sender(sender&&) noexcept = default;
        sender& operator=(sender&&) noexcept = default;
        sender(const sender&) = delete;
        sender& operator=(const sender&) = delete;

        // The encapsulated key: what the recipient's setup takes first. A
        // view of the bytes the context holds, valid while it lives
        slice<const byte> enc() const noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(_enc), _enc_size);
        }

        hpke::suite suite() const noexcept {
            return _c.suite;
        }

        // The next message sealed (§5.2): its ciphertext and tag.
        // std::logic_error for an export_only suite and a context moved
        // from; std::length_error past 2^64 - 1 messages
        vector<byte> seal(const slice<const byte>& plaintext) {
            return seal(plaintext, slice<const byte>());
        }

        vector<byte> seal(const slice<const byte>& plaintext, const slice<const byte>& aad) {
            if (_c.nh == 0) {
                detail::moved_from("sgcl::crypto::hpke::sender");
            }
            if (_c.aead.index() == 0) {
                throw logic_error("sgcl::crypto::hpke::sender::seal: an export_only suite seals nothing");
            }
            if (_c.seq == UINT64_MAX) {
                throw length_error("sgcl::crypto::hpke::sender::seal: the context's messages are spent (MessageLimitReachedError)");
            }
            unsigned char nonce[12];
            _c.nonce(nonce);
            const slice<const byte> n(reinterpret_cast<const byte*>(nonce), 12);
            vector<byte> out = _c.aead.index() == 1 ? std::get<1>(_c.aead).seal(n, plaintext, aad) : std::get<2>(_c.aead).seal(n, plaintext, aad);
            ++_c.seq;
            return out;
        }

        // A secret of length bytes for the context's label (§5.3), in plain
        // memory; std::invalid_argument past 255 Nh bytes
        secret_bytes export_secret(const slice<const byte>& context, size_t length) const {
            return detail::hpke_export(_c, context, length);
        }

    private:
        friend struct detail::HpkeAccess;
        friend struct detail::HpkeDerandomized;
        detail::HpkeContext _c;
        unsigned char _enc[133] = {};   // the longest enc: P-521's
        size_t _enc_size = 0;

        sender() noexcept = default;

        static expected<sender, error> _setup(const public_key& to, const hpke::suite& s, const slice<const byte>& info, const options& o,
                                              const private_key* sender_key, const private_key& eph);
    };

    // The setup with the ephemeral key given (the derandomized encapsulation
    // of RFC 9180's vectors, which give ikmE)
    inline expected<sender, error> sender::_setup(const public_key& to, const hpke::suite& s, const slice<const byte>& info, const options& o,
                                                  const private_key* sender_key, const private_key& eph) {
        if (sender_key && sender_key->kem() != to.kem()) {
            throw invalid_argument("sgcl::crypto::hpke::sender: the sender's key is of another KEM");
        }
        if ((o.psk.size() == 0) != (o.psk_id.size() == 0)) {
            throw invalid_argument("sgcl::crypto::hpke::sender: a PSK without its id, or an id without a PSK");
        }
        (void)detail::hpke_nh(s.kdf);
        (void)detail::hpke_nk(s.aead);
        auto e = detail::hpke_encap(to, eph, sender_key);
        if (!e) {
            return unexpected(e.error());
        }
        sender out;
        out._c = detail::hpke_schedule(to.kem(), s, detail::hpke_mode(o, sender_key != nullptr), e->first, info, o.psk, o.psk_id);
        out._enc_size = e->second.size();
        sgcl::detail::copy_bytes(out._enc, e->second.data(), out._enc_size);
        return out;
    }

    // A receiving context (§5.2): made of enc and the recipient's private
    // key, it opens the sender's messages in the order they were sealed.
    // Move-only, its secrets in the object, zeroed when it goes
    class recipient {
    public:
        // SetupBaseR, with options SetupPSKR, with the sender's public key
        // SetupAuthR, with both SetupAuthPSKR. errc::invalid_key for an enc
        // that is not a public key of the KEM or gives the zero secret, a
        // sender's key of another KEM; errc::malformed for a PSK without its
        // id or the reverse
        static expected<recipient, error> setup(const slice<const byte>& enc, const private_key& key, const suite& s) noexcept {
            return _setup(enc, key, s, slice<const byte>(), options(), nullptr);
        }

        static expected<recipient, error> setup(const slice<const byte>& enc, const private_key& key, const suite& s, const slice<const byte>& info) noexcept {
            return _setup(enc, key, s, info, options(), nullptr);
        }

        static expected<recipient, error> setup(const slice<const byte>& enc, const private_key& key, const suite& s, const slice<const byte>& info,
                                                const options& o) noexcept {
            return _setup(enc, key, s, info, o, nullptr);
        }

        static expected<recipient, error> setup(const slice<const byte>& enc, const private_key& key, const suite& s, const slice<const byte>& info,
                                                const public_key& sender_public) noexcept {
            return _setup(enc, key, s, info, options(), &sender_public);
        }

        static expected<recipient, error> setup(const slice<const byte>& enc, const private_key& key, const suite& s, const slice<const byte>& info,
                                                const options& o, const public_key& sender_public) noexcept {
            return _setup(enc, key, s, info, o, &sender_public);
        }

        recipient(recipient&&) noexcept = default;
        recipient& operator=(recipient&&) noexcept = default;
        recipient(const recipient&) = delete;
        recipient& operator=(const recipient&) = delete;

        hpke::suite suite() const noexcept {
            return _c.suite;
        }

        // The next message opened: errc::authentication when it does not
        // open, and the sequence stays where it was. std::logic_error for an
        // export_only suite and a context moved from
        [[nodiscard]] expected<vector<byte>, error> open(const slice<const byte>& ciphertext) {
            return open(ciphertext, slice<const byte>());
        }

        [[nodiscard]] expected<vector<byte>, error> open(const slice<const byte>& ciphertext, const slice<const byte>& aad) {
            if (_c.nh == 0) {
                detail::moved_from("sgcl::crypto::hpke::recipient");
            }
            if (_c.aead.index() == 0) {
                throw logic_error("sgcl::crypto::hpke::recipient::open: an export_only suite opens nothing");
            }
            if (_c.seq == UINT64_MAX) {
                return unexpected(error(errc::authentication, string("sgcl::crypto::hpke: the context's messages are spent")));
            }
            unsigned char nonce[12];
            _c.nonce(nonce);
            const slice<const byte> n(reinterpret_cast<const byte*>(nonce), 12);
            auto r = _c.aead.index() == 1 ? std::get<1>(_c.aead).open(n, ciphertext, aad) : std::get<2>(_c.aead).open(n, ciphertext, aad);
            if (r) {
                ++_c.seq;
            }
            return r;
        }

        secret_bytes export_secret(const slice<const byte>& context, size_t length) const {
            return detail::hpke_export(_c, context, length);
        }

    private:
        detail::HpkeContext _c;

        recipient() noexcept = default;

        static expected<recipient, error> _setup(const slice<const byte>& enc, const private_key& key, const hpke::suite& s, const slice<const byte>& info,
                                                 const options& o, const public_key* sender_public) noexcept {
            if (sender_public && sender_public->kem() != key.kem()) {
                return unexpected(error(errc::invalid_key, string("sgcl::crypto::hpke::recipient: the sender's key is of another KEM than the recipient's")));
            }
            if ((o.psk.size() == 0) != (o.psk_id.size() == 0)) {
                return unexpected(error(errc::malformed, string("sgcl::crypto::hpke::recipient: a PSK without its id, or an id without a PSK")));
            }
            if (!_known(s)) {
                return unexpected(error(errc::unsupported, string("sgcl::crypto::hpke::recipient: a KDF or an AEAD of no value of its enumeration")));
            }
            auto shared = detail::hpke_decap(enc, key, sender_public);
            if (!shared) {
                return unexpected(shared.error());
            }
            recipient out;
            out._c = detail::hpke_schedule(key.kem(), s, detail::hpke_mode(o, sender_public != nullptr), *shared, info, o.psk, o.psk_id);
            return out;
        }

        static bool _known(const hpke::suite& s) noexcept {
            const auto f = uint16_t(s.kdf);
            const auto a = uint16_t(s.aead);
            return f >= 1 && f <= 3 && ((a >= 1 && a <= 3) || a == 0xffff);
        }
    };

    // Single-shot (§6.1): one message sealed to the key, enc || ciphertext,
    // as Go's hpke.Seal
    inline expected<vector<byte>, error> seal(const public_key& to, const slice<const byte>& plaintext, const suite& s = {}, const slice<const byte>& info = {},
                                              const slice<const byte>& aad = {}) {
        auto c = sender::setup(to, s, info);
        if (!c) {
            return unexpected(c.error());
        }
        const auto enc = c->enc();
        vector<byte> out(enc.data(), enc.data() + enc.size());
        for (auto b : c->seal(plaintext, aad)) {
            out.push_back(b);
        }
        return out;
    }

    // The message of a single-shot seal
    [[nodiscard]] inline expected<vector<byte>, error> open(const private_key& key, const slice<const byte>& sealed, const suite& s = {}, const slice<const byte>& info = {},
                                                            const slice<const byte>& aad = {}) {
        const size_t n = detail::hpke_kem_info(key.kem()).enc;
        if (sealed.size() < n) {
            return unexpected(error(errc::malformed, string("sgcl::crypto::hpke::open: shorter than enc")));
        }
        auto c = recipient::setup(sealed.subslice(0, n), key, s, info);
        if (!c) {
            return unexpected(c.error());
        }
        return c->open(sealed.subslice(n, sealed.size() - n), aad);
    }
}

namespace sgcl::crypto::detail {
    // The setup with the ephemeral key given: the derandomized encapsulation
    // of RFC 9180's vectors (which give ikmE), for the tests
    struct HpkeDerandomized {
        static expected<hpke::sender, error> setup(const hpke::public_key& to, const hpke::suite& s, const slice<const byte>& info, const hpke::options& o,
                                                   const hpke::private_key* sender_key, const hpke::private_key& eph) {
            return hpke::sender::_setup(to, s, info, o, sender_key, eph);
        }
    };
}
