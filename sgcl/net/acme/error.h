//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/duration.h"
#include "../../core/string.h"
#include "../../io/error.h"

#include <string>
#include <string_view>
#include <system_error>

// The failures of ACME form the acme category of io::error: every error
// type of RFC 8555 §6.7 (and RFC 9773's alreadyReplaced, the profiles'
// invalidProfile), the code of a problem document the CA answered with,
// and the client's own: a problem of a type not in the list, a response
// that breaks the RFC, a resource the CA does not offer, an order or an
// authorization that ended invalid, no challenge of a type the client
// solves, a name the manager's host policy refuses.
namespace sgcl::net::acme {
    enum class errc {
        account_does_not_exist = 1,     // urn:ietf:params:acme:error:accountDoesNotExist
        already_revoked,                // alreadyRevoked
        bad_csr,                        // badCSR
        bad_nonce,                      // badNonce
        bad_public_key,                 // badPublicKey
        bad_revocation_reason,          // badRevocationReason
        bad_signature_algorithm,        // badSignatureAlgorithm
        caa,                            // caa
        compound,                       // compound
        connection,                     // connection
        dns,                            // dns
        external_account_required,      // externalAccountRequired
        incorrect_response,             // incorrectResponse
        invalid_contact,                // invalidContact
        malformed,                      // malformed
        order_not_ready,                // orderNotReady
        rate_limited,                   // rateLimited
        rejected_identifier,            // rejectedIdentifier
        server_internal,                // serverInternal
        tls,                            // tls
        unauthorized,                   // unauthorized
        unsupported_contact,            // unsupportedContact
        unsupported_identifier,         // unsupportedIdentifier
        user_action_required,           // userActionRequired
        already_replaced,               // alreadyReplaced (RFC 9773)
        invalid_profile,                // invalidProfile (the profiles extension)
        unknown_problem,                // a problem document of a type not in this list
        malformed_response,             // a response that breaks RFC 8555
        unsupported,                    // a resource the CA's directory does not offer (keyChange, renewalInfo)
        order_invalid,                  // an order that ended invalid
        authorization_invalid,          // an authorization that ended invalid
        no_challenge,                   // no challenge of a type the client solves
        host_not_allowed                // a name the manager's host policy refuses
    };

    namespace detail {
        struct ProblemType {
            std::string_view name;   // after urn:ietf:params:acme:error:
            errc code;
            const char* text;
        };

        inline constexpr ProblemType problem_types[] = {
            {"accountDoesNotExist", errc::account_does_not_exist, "the account does not exist"},
            {"alreadyRevoked", errc::already_revoked, "the certificate is already revoked"},
            {"badCSR", errc::bad_csr, "the CSR is unacceptable"},
            {"badNonce", errc::bad_nonce, "the nonce is unacceptable"},
            {"badPublicKey", errc::bad_public_key, "the JWS key is not supported"},
            {"badRevocationReason", errc::bad_revocation_reason, "the revocation reason is not allowed"},
            {"badSignatureAlgorithm", errc::bad_signature_algorithm, "the JWS algorithm is not supported"},
            {"caa", errc::caa, "CAA records forbid the CA from issuing"},
            {"compound", errc::compound, "several errors, in the subproblems"},
            {"connection", errc::connection, "the server could not connect to the validation target"},
            {"dns", errc::dns, "a DNS query failed during validation"},
            {"externalAccountRequired", errc::external_account_required, "the CA requires an external account binding"},
            {"incorrectResponse", errc::incorrect_response, "the response to the challenge was incorrect"},
            {"invalidContact", errc::invalid_contact, "a contact URL is invalid"},
            {"malformed", errc::malformed, "the request is malformed"},
            {"orderNotReady", errc::order_not_ready, "the order is not ready to be finalized"},
            {"rateLimited", errc::rate_limited, "rate limited"},
            {"rejectedIdentifier", errc::rejected_identifier, "the CA will not issue for the identifier"},
            {"serverInternal", errc::server_internal, "the CA had an internal error"},
            {"tls", errc::tls, "a TLS error during validation"},
            {"unauthorized", errc::unauthorized, "unauthorized"},
            {"unsupportedContact", errc::unsupported_contact, "a contact URL of an unsupported scheme"},
            {"unsupportedIdentifier", errc::unsupported_identifier, "an identifier of an unsupported type"},
            {"userActionRequired", errc::user_action_required, "visit the instance URL and take the action it says"},
            {"alreadyReplaced", errc::already_replaced, "the certificate was already replaced"},
            {"invalidProfile", errc::invalid_profile, "the profile is not one the CA offers"},
        };

        inline constexpr std::string_view problem_prefix = "urn:ietf:params:acme:error:";

        class AcmeCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "acme";
            }

            std::string message(int c) const noexcept override {
                for (auto& t : problem_types) {
                    if (int(t.code) == c) {
                        return std::string("acme: ") + t.text;
                    }
                }
                switch (errc(c)) {
                    case errc::unknown_problem: return "acme: a problem of an unknown type";
                    case errc::malformed_response: return "acme: malformed response";
                    case errc::unsupported: return "acme: not offered by the CA";
                    case errc::order_invalid: return "acme: the order is invalid";
                    case errc::authorization_invalid: return "acme: the authorization is invalid";
                    case errc::no_challenge: return "acme: no challenge of a supported type";
                    case errc::host_not_allowed: return "acme: the host is not allowed";
                    default: break;
                }
                return "acme: unknown error";
            }
        };
    }

    inline const std::error_category& category() noexcept {
        static const detail::AcmeCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::net::acme::errc> : std::true_type {};

namespace sgcl::net::acme::detail {
    // A span in whole seconds, rounded up: what Retry-After and the
    // times of certificates count in
    SGCL_INLINE_HOT constexpr int64_t whole_seconds(duration d) noexcept {
        const int64_t ns = d.nanoseconds();
        return ns <= 0 ? 0 : (ns + 999999999) / 1000000000;
    }

    inline io::error acme_error(errc e, const string& op, const string& what = {}) noexcept {
        return io::error(make_error_code(e), op, what);
    }
}
