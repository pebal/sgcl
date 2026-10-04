//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "messages.h"
#include "../../../crypto/ed25519.h"
#include "../../../crypto/p256.h"
#include "../../../crypto/p384.h"
#include "../../../crypto/rsa.h"
#include "../../../crypto/sha256.h"
#include "../../../crypto/sha512.h"
#include "../../../crypto/x509.h"

#include <cstring>

// CertificateVerify's signature (RFC 8446 §4.4.3): what is signed, and the
// schemes of v1 over the module's keys. Signed is 64 spaces, the context
// string of the side, a zero byte and the transcript's hash; ECDSA and
// RSA-PSS sign its digest (the hash of the scheme; PSS with a salt as long
// as the digest, MGF1 over the same hash), Ed25519 the content itself.
// rsa_pkcs1_* is for a certificate's signature (x509 checks those) and is
// refused here, as §4.2.3 says.
namespace sgcl::net::tls::detail {
    // The content signed: 64 + 33 + 1 + the hash's length, at most 146 bytes
    struct VerifyContent {
        uint8_t bytes[64 + 33 + 1 + 48];
        size_t size = 0;

        SGCL_INLINE_HOT Bytes view() const noexcept {
            return bytes_of(bytes, size);
        }
    };

    inline VerifyContent certificate_verify_content(bool server, const Bytes& transcript_hash) noexcept {
        static constexpr char server_context[] = "TLS 1.3, server CertificateVerify";
        static constexpr char client_context[] = "TLS 1.3, client CertificateVerify";
        VerifyContent c;
        assert(transcript_hash.size() <= 48);
        std::memset(c.bytes, 0x20, 64);
        std::memcpy(c.bytes + 64, server ? server_context : client_context, 33);
        c.bytes[97] = 0;
        std::memcpy(c.bytes + 98, transcript_hash.data(), transcript_hash.size());
        c.size = 98 + transcript_hash.size();
        return c;
    }

    // Whether the scheme is one CertificateVerify may carry in v1
    inline constexpr bool verify_scheme(uint16_t s) noexcept {
        switch (SignatureScheme(s)) {
            case SignatureScheme::ecdsa_secp256r1_sha256:
            case SignatureScheme::ecdsa_secp384r1_sha384:
            case SignatureScheme::rsa_pss_rsae_sha256:
            case SignatureScheme::rsa_pss_rsae_sha384:
            case SignatureScheme::rsa_pss_rsae_sha512:
            case SignatureScheme::ed25519:
                return true;
            default:
                return false;
        }
    }

    namespace sig {
        inline Alert illegal(const char* what) noexcept {
            return Alert{AlertDescription::illegal_parameter, 0, what};
        }

        inline Alert bad() noexcept {
            return Alert{AlertDescription::decrypt_error, 0, "CertificateVerify's signature does not verify"};
        }
    }

    // The signature checked under a key of each kind: a scheme of another
    // kind of key is illegal_parameter, a signature that does not verify
    // decrypt_error (§4.4.3)
    SGCL_INLINE_HOT expected<void, Alert> verify(uint16_t scheme, const crypto::ed25519::public_key& key, const Bytes& content, const Bytes& signature) noexcept {
        if (SignatureScheme(scheme) != SignatureScheme::ed25519) {
            return unexpected<Alert>(sig::illegal("a signature scheme of another kind of key than Ed25519"));
        }
        if (!key.verify(content, signature)) {
            return unexpected<Alert>(sig::bad());
        }
        return {};
    }

    SGCL_INLINE_HOT expected<void, Alert> verify(uint16_t scheme, const crypto::p256::public_key& key, const Bytes& content, const Bytes& signature) noexcept {
        if (SignatureScheme(scheme) != SignatureScheme::ecdsa_secp256r1_sha256) {
            return unexpected<Alert>(sig::illegal("a signature scheme of another kind of key than P-256"));
        }
        auto d = crypto::sha256::of(content);
        if (!key.verify_digest(bytes_of(d.data(), d.size()), signature)) {
            return unexpected<Alert>(sig::bad());
        }
        return {};
    }

    SGCL_INLINE_HOT expected<void, Alert> verify(uint16_t scheme, const crypto::p384::public_key& key, const Bytes& content, const Bytes& signature) noexcept {
        if (SignatureScheme(scheme) != SignatureScheme::ecdsa_secp384r1_sha384) {
            return unexpected<Alert>(sig::illegal("a signature scheme of another kind of key than P-384"));
        }
        auto d = crypto::sha384::of(content);
        if (!key.verify_digest(bytes_of(d.data(), d.size()), signature)) {
            return unexpected<Alert>(sig::bad());
        }
        return {};
    }

    // RSA: RSASSA-PSS with a salt as long as the digest (rsa_pss_rsae_*);
    // rsa_pkcs1_* is a certificate's, never CertificateVerify's (§4.2.3)
    inline expected<void, Alert> verify(uint16_t scheme, const crypto::rsa::public_key& key, const Bytes& content, const Bytes& signature) noexcept {
        bool ok = false;
        switch (SignatureScheme(scheme)) {
            case SignatureScheme::rsa_pss_rsae_sha256: {
                auto d = crypto::sha256::of(content);
                ok = key.verify_digest_pss(crypto::hash_id::sha256, bytes_of(d.data(), d.size()), signature, d.size());
                break;
            }
            case SignatureScheme::rsa_pss_rsae_sha384: {
                auto d = crypto::sha384::of(content);
                ok = key.verify_digest_pss(crypto::hash_id::sha384, bytes_of(d.data(), d.size()), signature, d.size());
                break;
            }
            case SignatureScheme::rsa_pss_rsae_sha512: {
                auto d = crypto::sha512::of(content);
                ok = key.verify_digest_pss(crypto::hash_id::sha512, bytes_of(d.data(), d.size()), signature, d.size());
                break;
            }
            case SignatureScheme::rsa_pkcs1_sha256:
            case SignatureScheme::rsa_pkcs1_sha384:
            case SignatureScheme::rsa_pkcs1_sha512:
                return unexpected<Alert>(sig::illegal("a PKCS #1 v1.5 signature in CertificateVerify"));
            default:
                return unexpected<Alert>(sig::illegal("a signature scheme of another kind of key than RSA"));
        }
        if (!ok) {
            return unexpected<Alert>(sig::bad());
        }
        return {};
    }

    // Under the peer's certificate key: a scheme v1 does not take is
    // illegal_parameter whatever the key; whether the scheme was offered is
    // the machine's to check
    inline expected<void, Alert> verify(uint16_t scheme, const crypto::x509::public_key& key, const Bytes& content, const Bytes& signature) noexcept {
        using crypto::x509::key_kind;
        if (!verify_scheme(scheme)) {
            bool pkcs1 = scheme == uint16_t(SignatureScheme::rsa_pkcs1_sha256) || scheme == uint16_t(SignatureScheme::rsa_pkcs1_sha384) || scheme == uint16_t(SignatureScheme::rsa_pkcs1_sha512);
            return unexpected<Alert>(sig::illegal(pkcs1 ? "a PKCS #1 v1.5 signature in CertificateVerify" : "a signature scheme v1 does not take"));
        }
        switch (key.kind()) {
            case key_kind::ed25519: return verify(scheme, key.ed25519(), content, signature);
            case key_kind::p256: return verify(scheme, key.p256(), content, signature);
            case key_kind::p384: return verify(scheme, key.p384(), content, signature);
            case key_kind::rsa: return verify(scheme, key.rsa(), content, signature);
            case key_kind::none: break;
        }
        return unexpected<Alert>(Alert{AlertDescription::unsupported_certificate, 0, "a certificate key of a kind v1 does not verify"});
    }

    // The signature made with a key of the server's identity, written as
    // CertificateVerify's signature field wants it (DER for ECDSA). The
    // scheme must fit the key (the machine chooses it so): a mismatch is a
    // mistake of the program
    SGCL_INLINE_HOT void sign(Builder& w, uint16_t scheme, const crypto::ed25519::private_key& key, const Bytes& content) noexcept {
        assert(SignatureScheme(scheme) == SignatureScheme::ed25519);
        (void)scheme;
        auto s = key.sign(content);
        w.bytes(s.data(), s.size());
    }

    SGCL_INLINE_HOT void sign(Builder& w, uint16_t scheme, const crypto::p256::private_key& key, const Bytes& content) noexcept {
        assert(SignatureScheme(scheme) == SignatureScheme::ecdsa_secp256r1_sha256);
        (void)scheme;
        auto d = crypto::sha256::of(content);
        auto s = key.sign_digest(bytes_of(d.data(), d.size()));
        w.bytes(s.data(), s.size());
    }

    SGCL_INLINE_HOT void sign(Builder& w, uint16_t scheme, const crypto::p384::private_key& key, const Bytes& content) noexcept {
        assert(SignatureScheme(scheme) == SignatureScheme::ecdsa_secp384r1_sha384);
        (void)scheme;
        auto d = crypto::sha384::of(content);
        auto s = key.sign_digest(bytes_of(d.data(), d.size()));
        w.bytes(s.data(), s.size());
    }

    // Whether RSA-PSS with a salt as long as the digest fits a key of
    // `bits` (RFC 8017 §9.1.1: an encoding of bits - 1 bits holds the
    // digest, the salt and two bytes); a key of 1024 bits has no room for
    // SHA-512's
    SGCL_INLINE_HOT constexpr bool rsa_pss_fits(size_t bits, size_t digest_size) noexcept {
        return bits >= 2 && (bits - 1 + 7) / 8 >= 2 * digest_size + 2;
    }

    // (the scheme must fit the key, rsa_pss_fits: identity_of offers only
    // those; not noexcept: a signature that does not verify under the
    // public key, a fault in the computation, is crypto's
    // std::runtime_error)
    inline void sign(Builder& w, uint16_t scheme, const crypto::rsa::private_key& key, const Bytes& content) {
        switch (SignatureScheme(scheme)) {
            case SignatureScheme::rsa_pss_rsae_sha256: {
                auto d = crypto::sha256::of(content);
                auto s = key.sign_digest_pss(crypto::hash_id::sha256, bytes_of(d.data(), d.size()));
                w.bytes(s.data(), s.size());
                return;
            }
            case SignatureScheme::rsa_pss_rsae_sha384: {
                auto d = crypto::sha384::of(content);
                auto s = key.sign_digest_pss(crypto::hash_id::sha384, bytes_of(d.data(), d.size()));
                w.bytes(s.data(), s.size());
                return;
            }
            default: {
                assert(SignatureScheme(scheme) == SignatureScheme::rsa_pss_rsae_sha512);
                auto d = crypto::sha512::of(content);
                auto s = key.sign_digest_pss(crypto::hash_id::sha512, bytes_of(d.data(), d.size()));
                w.bytes(s.data(), s.size());
                return;
            }
        }
    }
}
