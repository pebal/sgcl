//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../crypto/error.h"
#include "../../io/error.h"
#include "detail/types.h"

#include <cstdint>
#include <string>
#include <system_error>

namespace sgcl::net::tls {
    // The alerts of TLS 1.3 (RFC 8446 §6.2), by their numbers on the wire
    enum class alert : uint8_t {
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

    // The failures of a TLS connection form the tls category of io::error:
    // an alert this side sent (the code is the alert's number: "tls: bad
    // certificate"), one the peer sent (256 + its number: "remote error:
    // tls: bad certificate", as Go words both), and a server's chain that
    // did not verify (512 + its x509::reason: "tls: certificate signed by
    // unknown authority"; the alert sent for it follows from the reason),
    // and an alert the server sent before its hello (1024 + its number:
    // "remote error: tls: no application protocol, before the server's
    // hello"; a close_notify or a protocol_version there, what a server
    // without TLS 1.3 answers a hello of 1.3 alone with, adds "(no TLS
    // 1.3?)").
    // `e.code() == tls::alert::decode_error` asks for an alert of this
    // side; alert_of, is_remote and certificate_reason take any of them
    // apart.
    namespace detail {
        inline const char* alert_text(int a) noexcept {
            switch (a) {
                case 0: return "close notify";
                case 10: return "unexpected message";
                case 20: return "bad record MAC";
                case 22: return "record overflow";
                case 40: return "handshake failure";
                case 42: return "bad certificate";
                case 43: return "unsupported certificate";
                case 44: return "revoked certificate";
                case 45: return "expired certificate";
                case 46: return "unknown certificate";
                case 47: return "illegal parameter";
                case 48: return "unknown certificate authority";
                case 49: return "access denied";
                case 50: return "error decoding message";
                case 51: return "error decrypting message";
                case 70: return "protocol version not supported";
                case 71: return "insufficient security level";
                case 80: return "internal error";
                case 86: return "inappropriate fallback";
                case 90: return "user canceled";
                case 109: return "missing extension";
                case 110: return "unsupported extension";
                case 112: return "unrecognized name";
                case 113: return "bad certificate status response";
                case 115: return "unknown PSK identity";
                case 116: return "certificate required";
                case 120: return "no application protocol";
            }
            return nullptr;
        }

        inline const char* reason_text(int r) noexcept {
            using crypto::x509::reason;
            switch (reason(r)) {
                case reason::expired:
                case reason::not_yet_valid: return "certificate has expired or is not yet valid";
                case reason::unknown_authority: return "certificate signed by unknown authority";
                case reason::hostname_mismatch: return "certificate is not valid for the server name";
                case reason::name_constraints: return "certificate is outside a name constraint of its issuer";
                case reason::unsupported_algorithm: return "certificate of an algorithm not supported";
                case reason::insecure_algorithm: return "certificate signed with an insecure algorithm";
                case reason::invalid_signature: return "certificate signature does not verify";
                case reason::too_many_intermediates: return "too many intermediate certificates";
                case reason::path_length: return "certificate chain past a path length constraint";
                case reason::not_a_ca: return "certificate issued by one that is not a CA";
                case reason::missing_cert_sign: return "certificate issued by a CA not allowed to sign certificates";
                case reason::incompatible_usage: return "certificate specifies an incompatible key usage";
                case reason::unhandled_critical_extension: return "certificate with an unhandled critical extension";
                case reason::too_many_constraints: return "certificate chain with too many name constraints";
                case reason::none: break;
            }
            return "certificate does not verify";
        }

        class TlsCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "tls";
            }

            std::string message(int c) const noexcept override {
                if (c >= 512 && c < 1024) {
                    return std::string("tls: ") + reason_text(c - 512);
                }
                const int a = c & 0xFF;
                const char* t = alert_text(a);
                std::string text = (c >= 256 ? "remote error: tls: " : "tls: ") + (t ? std::string(t) : "alert(" + std::to_string(a) + ")");
                if (c >= 1024) {
                    text += a == 0 || a == 70 ? ", before the server's hello (no TLS 1.3?)" : ", before the server's hello";
                }
                return text;
            }
        };
    }

    inline const std::error_category& category() noexcept {
        static const detail::TlsCategory instance;
        return instance;
    }

    inline error_code make_error_code(alert a) noexcept {
        return error_code(int(a), category());
    }

    // The alert of a tls error, this side's or the peer's; nullopt for
    // another error and for a chain that did not verify
    SGCL_INLINE_HOT optional<alert> alert_of(const io::error& e) noexcept {
        if (e.code().category() != category() || (e.code().value() >= 512 && e.code().value() < 1024)) {
            return nullopt;
        }
        return alert(uint8_t(e.code().value() & 0xFF));
    }

    // Whether the error is an alert the peer sent
    SGCL_INLINE_HOT bool is_remote(const io::error& e) noexcept {
        return e.code().category() == category() && e.code().value() >= 256 && (e.code().value() < 512 || e.code().value() >= 1024);
    }

    // Why the server's chain did not verify, for an error that says so
    SGCL_INLINE_HOT optional<crypto::x509::reason> certificate_reason(const io::error& e) noexcept {
        if (e.code().category() != category() || e.code().value() < 512 || e.code().value() >= 1024) {
            return nullopt;
        }
        return crypto::x509::reason(e.code().value() - 512);
    }

    namespace detail {
        inline io::error local_error(AlertDescription d, const string& op, const string& what) noexcept {
            return io::error(error_code(int(d), category()), op, what);
        }

        inline io::error remote_error(AlertDescription d, const string& op, const string& what) noexcept {
            return io::error(error_code(256 + int(d), category()), op, what);
        }

        // An alert of the server before its hello (in the clear; a server
        // without TLS 1.3 among the senders)
        inline io::error before_hello_error(AlertDescription d, const string& op, const string& what) noexcept {
            return io::error(error_code(1024 + int(d), category()), op, what);
        }

        inline io::error certificate_error(crypto::x509::reason r, const string& op, const string& what) noexcept {
            return io::error(error_code(512 + int(r), category()), op, what);
        }
    }
}

template<>
struct std::is_error_code_enum<sgcl::net::tls::alert> : std::true_type {};
