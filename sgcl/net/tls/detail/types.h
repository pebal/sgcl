//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "schedule.h"

#include <cstddef>
#include <cstdint>

// The code points of TLS 1.3 the layers share (RFC 8446 §4, §5, §6, and
// the registries it points to), and of the client's TLS 1.2 (RFC 5246,
// RFC 8422, RFC 5289, RFC 7905): cipher suites, groups, signature schemes,
// handshake and content types, extensions and alerts, each with its
// number on the wire. The public types of tls.h carry the same numbers.
namespace sgcl::net::tls::detail {
    inline constexpr uint16_t Tls12 = 0x0303;   // legacy_version and legacy_record_version
    inline constexpr uint16_t Tls13 = 0x0304;   // supported_versions

    // The last 8 bytes of a TLS 1.3 server's random when it negotiates TLS
    // 1.2 (RFC 8446 §4.1.3): a client that offered 1.3 and reads them was
    // downgraded by something between
    inline constexpr uint8_t Downgrade12[8] = {0x44, 0x4F, 0x57, 0x4E, 0x47, 0x52, 0x44, 0x01};

    // The cipher suites: TLS 1.3's (§B.4), and the client's TLS 1.2 ones,
    // ECDHE with an AEAD alone (RFC 5289, RFC 7905)
    enum class Cipher : uint16_t {
        aes_128_gcm_sha256 = 0x1301,
        aes_256_gcm_sha384 = 0x1302,
        chacha20_poly1305_sha256 = 0x1303,
        ecdhe_ecdsa_aes_128_gcm_sha256 = 0xC02B,
        ecdhe_ecdsa_aes_256_gcm_sha384 = 0xC02C,
        ecdhe_rsa_aes_128_gcm_sha256 = 0xC02F,
        ecdhe_rsa_aes_256_gcm_sha384 = 0xC030,
        ecdhe_rsa_chacha20_poly1305_sha256 = 0xCCA8,
        ecdhe_ecdsa_chacha20_poly1305_sha256 = 0xCCA9,
    };

    // The hash of the suite: of the key schedule (1.3), of the PRF (1.2)
    SGCL_INLINE_HOT constexpr Hash hash_of(Cipher c) noexcept {
        return c == Cipher::aes_256_gcm_sha384 || c == Cipher::ecdhe_ecdsa_aes_256_gcm_sha384 || c == Cipher::ecdhe_rsa_aes_256_gcm_sha384 ? Hash::sha384 : Hash::sha256;
    }

    // Whether the AEAD is ChaCha20-Poly1305 (else AES-GCM)
    SGCL_INLINE_HOT constexpr bool chacha(Cipher c) noexcept {
        return c == Cipher::chacha20_poly1305_sha256 || c == Cipher::ecdhe_rsa_chacha20_poly1305_sha256 || c == Cipher::ecdhe_ecdsa_chacha20_poly1305_sha256;
    }

    SGCL_INLINE_HOT constexpr size_t key_size(Cipher c) noexcept {
        return c == Cipher::aes_128_gcm_sha256 || c == Cipher::ecdhe_ecdsa_aes_128_gcm_sha256 || c == Cipher::ecdhe_rsa_aes_128_gcm_sha256 ? 16 : 32;
    }

    // A suite of TLS 1.3
    SGCL_INLINE_HOT constexpr bool known(Cipher c) noexcept {
        return c == Cipher::aes_128_gcm_sha256 || c == Cipher::aes_256_gcm_sha384 || c == Cipher::chacha20_poly1305_sha256;
    }

    // A suite of TLS 1.2 the client speaks
    SGCL_INLINE_HOT constexpr bool known12(Cipher c) noexcept {
        switch (c) {
            case Cipher::ecdhe_ecdsa_aes_128_gcm_sha256:
            case Cipher::ecdhe_ecdsa_aes_256_gcm_sha384:
            case Cipher::ecdhe_rsa_aes_128_gcm_sha256:
            case Cipher::ecdhe_rsa_aes_256_gcm_sha384:
            case Cipher::ecdhe_rsa_chacha20_poly1305_sha256:
            case Cipher::ecdhe_ecdsa_chacha20_poly1305_sha256:
                return true;
            default:
                return false;
        }
    }

    // A TLS 1.2 suite authenticated by an RSA certificate (else ECDSA or
    // Ed25519, RFC 8422)
    SGCL_INLINE_HOT constexpr bool rsa_suite(Cipher c) noexcept {
        return c == Cipher::ecdhe_rsa_aes_128_gcm_sha256 || c == Cipher::ecdhe_rsa_aes_256_gcm_sha384 || c == Cipher::ecdhe_rsa_chacha20_poly1305_sha256;
    }

    // The fixed part of a TLS 1.2 AEAD's nonce (RFC 5288 §3: the salt of
    // AES-GCM, 4 bytes; RFC 7905 §2: ChaCha20-Poly1305's IV, 12) and the
    // explicit part each record carries (AES-GCM's 8 bytes)
    SGCL_INLINE_HOT constexpr size_t iv_size12(Cipher c) noexcept {
        return chacha(c) ? 12 : 4;
    }

    SGCL_INLINE_HOT constexpr size_t explicit_nonce12(Cipher c) noexcept {
        return chacha(c) ? 0 : 8;
    }

    // The groups of v1 (§4.2.7; X25519MLKEM768: draft-ietf-tls-ecdhe-mlkem)
    enum class Group : uint16_t {
        secp256r1 = 0x0017,
        secp384r1 = 0x0018,
        secp521r1 = 0x0019,
        x25519 = 0x001D,
        x25519_mlkem768 = 0x11EC,
    };

    // The signature schemes of v1 (§4.2.3): the ECDSA, RSA-PSS and Ed25519
    // ones sign and verify CertificateVerify; the rsa_pkcs1 ones only verify
    // certificates
    enum class SignatureScheme : uint16_t {
        rsa_pkcs1_sha256 = 0x0401,
        rsa_pkcs1_sha384 = 0x0501,
        rsa_pkcs1_sha512 = 0x0601,
        ecdsa_secp256r1_sha256 = 0x0403,
        ecdsa_secp384r1_sha384 = 0x0503,
        ecdsa_secp521r1_sha512 = 0x0603,
        rsa_pss_rsae_sha256 = 0x0804,
        rsa_pss_rsae_sha384 = 0x0805,
        rsa_pss_rsae_sha512 = 0x0806,
        ed25519 = 0x0807,
    };

    // §5.1
    enum class ContentType : uint8_t {
        invalid = 0,
        change_cipher_spec = 20,
        alert = 21,
        handshake = 22,
        application_data = 23,
    };

    // §4
    enum class HandshakeType : uint8_t {
        hello_request = 0,
        client_hello = 1,
        server_hello = 2,
        new_session_ticket = 4,
        end_of_early_data = 5,
        encrypted_extensions = 8,
        certificate = 11,
        server_key_exchange = 12,       // TLS 1.2
        certificate_request = 13,
        server_hello_done = 14,         // TLS 1.2
        certificate_verify = 15,
        client_key_exchange = 16,       // TLS 1.2
        finished = 20,
        certificate_status = 22,        // TLS 1.2 (RFC 6066 §8)
        key_update = 24,
        message_hash = 254,
    };

    // §4.2, the ones v1 reads or writes
    enum class ExtensionType : uint16_t {
        server_name = 0,
        status_request = 5,
        supported_groups = 10,
        ec_point_formats = 11,          // TLS 1.2 (RFC 8422 §5.1.2)
        signature_algorithms = 13,
        application_layer_protocol_negotiation = 16,
        signed_certificate_timestamp = 18,
        extended_master_secret = 23,    // TLS 1.2 (RFC 7627)
        record_size_limit = 28,
        pre_shared_key = 41,
        early_data = 42,
        supported_versions = 43,
        cookie = 44,
        psk_key_exchange_modes = 45,
        certificate_authorities = 47,
        post_handshake_auth = 49,
        signature_algorithms_cert = 50,
        key_share = 51,
    };

    // §6
    enum class AlertDescription : uint8_t {
        close_notify = 0,
        unexpected_message = 10,
        bad_record_mac = 20,
        record_overflow = 22,
        handshake_failure = 40,
        bad_certificate = 42,
        unsupported_certificate = 43,
        certificate_revoked = 44,
        certificate_expired = 45,
        certificate_unknown = 46,
        illegal_parameter = 47,
        unknown_ca = 48,
        access_denied = 49,
        decode_error = 50,
        decrypt_error = 51,
        protocol_version = 70,
        insufficient_security = 71,
        internal_error = 80,
        inappropriate_fallback = 86,
        user_canceled = 90,
        missing_extension = 109,
        unsupported_extension = 110,
        unrecognized_name = 112,
        bad_certificate_status_response = 113,
        unknown_psk_identity = 115,
        certificate_required = 116,
        no_application_protocol = 120,
    };

    // An alert: in TLS 1.3 every one is fatal but close_notify and
    // user_canceled (§6); the level on the wire follows from the kind
    struct Alert {
        AlertDescription description = AlertDescription::internal_error;
        uint32_t offset = 0;
        const char* what = nullptr;

        bool fatal() const noexcept {
            return description != AlertDescription::close_notify && description != AlertDescription::user_canceled;
        }
    };

    // ServerHello.random of a HelloRetryRequest: SHA-256 of
    // "HelloRetryRequest" (§4.1.3)
    inline constexpr uint8_t HelloRetryRandom[32] = {
        0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11, 0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91,
        0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E, 0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C,
    };

    // The keys a record travels under: none (the first flights), the
    // handshake's, the application's (§7)
    enum class Epoch : uint8_t {
        initial,
        handshake,
        application,
    };
}
