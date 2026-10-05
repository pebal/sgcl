//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "../../core/aliases.h"
#include "../../core/duration.h"
#include "../../core/ordered_map.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../crypto/x509.h"
#include "../../time/datetime.h"

#include <cstdint>

// The objects of RFC 8555 §7.1 as the client reads them: the directory, an
// account, an order, an authorization, a challenge, a problem document
// (RFC 7807, §6.7); a certificate's renewal information (RFC 9773); the
// chain a CA issued. Plain values: what the CA said when it was asked, never
// updated behind the program's back (the client's calls give new ones).
namespace sgcl::net::acme {
    // The status of an account, an order, an authorization or a challenge
    // (RFC 8555 §7.1.6): one enumeration for all, each object using the
    // values its state machine has
    enum class status : uint8_t {
        pending,
        ready,
        processing,
        valid,
        invalid,
        revoked,
        deactivated,
        expired,
    };

    // The reasons of a revocation (RFC 5280 §5.3.1), the codes RFC 8555
    // §7.6 sends
    enum class revocation_reason : uint8_t {
        unspecified = 0,
        key_compromise = 1,
        ca_compromise = 2,
        affiliation_changed = 3,
        superseded = 4,
        cessation_of_operation = 5,
        certificate_hold = 6,
        remove_from_crl = 8,
        privilege_withdrawn = 9,
        aa_compromise = 10,
    };

    // What a certificate is asked for: a DNS name (RFC 8555 §9.7.7), its
    // wildcard among them ("*.example.com"), or an IP address (RFC 8738)
    struct identifier {
        string type;                    // "dns" or "ip"
        string value;                   // the name, or the address as text

        SGCL_INLINE_HOT friend bool operator==(const identifier& a, const identifier& b) noexcept {
            return a.type == b.type && a.value == b.value;
        }
    };

    // One error of a compound problem (RFC 8555 §6.7.1): of one identifier
    struct subproblem {
        string type;                    // urn:ietf:params:acme:error:…
        string detail;
        acme::identifier identifier;
    };

    // A problem document (RFC 7807) as the CA sends it with an error, or
    // as an order, an authorization or a challenge holds it
    struct problem {
        string type;                    // urn:ietf:params:acme:error:… ("about:blank" when the CA gave none)
        string detail;                  // for a human
        int status = 0;                 // the HTTP status, 0 when absent
        string instance;                // a URL to visit (userActionRequired)
        optional<acme::identifier> identifier;
        vector<subproblem> subproblems;

        // The type as a code of the acme category (errc::unknown_problem
        // for a type not in RFC 8555 §6.7, RFC 9773 or the profiles')
        errc code() const noexcept;
    };

    // The resources of a CA and what it says of itself (RFC 8555 §7.1.1)
    struct directory {
        string new_nonce;
        string new_account;
        string new_order;
        string new_authz;               // empty: pre-authorization not offered
        string revoke_cert;
        string key_change;
        string renewal_info;            // RFC 9773; empty: not offered
        string terms_of_service;        // meta
        string website;
        vector<string> caa_identities;
        bool external_account_required = false;
        ordered_map<string, string> profiles;   // the profiles offered, by name, with their descriptions
    };

    // An account (RFC 8555 §7.1.2)
    struct account {
        string url;                     // its URL, the kid of every request
        acme::status status = status::valid;
        vector<string> contact;
        bool terms_agreed = false;
        string orders;                  // the URL of its list of orders; empty: none given
    };

    // A challenge of an authorization (RFC 8555 §7.1.5, §8)
    struct challenge {
        string type;                    // "http-01", "dns-01", "tls-alpn-01", or another the CA has
        string url;
        acme::status status = status::pending;
        string token;
        optional<time::datetime> validated;
        optional<problem> error;
    };

    // An authorization of an identifier (RFC 8555 §7.1.4)
    struct authorization {
        string url;
        acme::identifier identifier;    // a wildcard's without its "*." (wildcard says so)
        acme::status status = status::pending;
        optional<time::datetime> expires;
        vector<acme::challenge> challenges;
        bool wildcard = false;
    };

    // An order (RFC 8555 §7.1.3)
    struct order {
        string url;
        acme::status status = status::pending;
        optional<time::datetime> expires;
        vector<acme::identifier> identifiers;
        optional<time::datetime> not_before;
        optional<time::datetime> not_after;
        optional<problem> error;
        vector<string> authorizations;  // their URLs
        string finalize;                // the URL its CSR goes to
        string certificate;             // the URL of the certificate, once valid
        string replaces;                // RFC 9773: the certificate it replaces
        string profile;
        duration retry_after = duration::zero();   // the CA's Retry-After with it (processing), zero: none
    };

    // When the CA suggests a certificate be renewed (RFC 9773 §4.2)
    struct renewal_info {
        time::datetime start;           // the window's start
        time::datetime end;             // the window's end
        string explanation_url;         // empty: none
        duration retry_after = duration::zero();   // when to ask again, the CA's Retry-After
    };

    // A certificate the CA issued, its chain to the CA's root (the leaf
    // first), as RFC 8555 §7.4.2 downloads it
    struct certificate_chain {
        string pem;                     // the chain as the CA sent it (application/pem-certificate-chain)
        crypto::x509::chain certificates;
        vector<string> alternates;      // the URLs of other chains of the same certificate (Link rel="alternate")
    };

    // External Account Binding (RFC 8555 §7.3.4): the key id and the MAC
    // key a CA that requires it gives its customers
    struct external_account {
        string key_id;
        string hmac_key;                // base64url, as the CA gives it
    };

    // What a new account is made with (RFC 8555 §7.3)
    struct account_options {
        vector<string> contact;         // "mailto:admin@example.com"
        bool terms_agreed = false;      // the CA's terms of service accepted (directory::terms_of_service)
        optional<acme::external_account> external_account;
        bool only_return_existing = false;   // the account of the key, never a new one (accountDoesNotExist)
    };

    // What a new order asks for besides its names (RFC 8555 §7.4, RFC 9773
    // §5, the profiles extension)
    struct order_options {
        optional<time::datetime> not_before;
        optional<time::datetime> not_after;
        string replaces;                // the ARI id of a certificate this one replaces (client::renewal_id)
        string profile;                 // a profile of directory::profiles; empty: the CA's default
    };

    SGCL_INLINE_HOT const char* to_string(status s) noexcept {
        switch (s) {
            case status::pending: return "pending";
            case status::ready: return "ready";
            case status::processing: return "processing";
            case status::valid: return "valid";
            case status::invalid: return "invalid";
            case status::revoked: return "revoked";
            case status::deactivated: return "deactivated";
            case status::expired: return "expired";
        }
        return "unknown";
    }
}

namespace sgcl::net::acme::detail {
    // The code of a problem document's type
    inline errc problem_code(std::string_view type) noexcept {
        if (type.substr(0, problem_prefix.size()) == problem_prefix) {
            std::string_view name = type.substr(problem_prefix.size());
            for (auto& t : problem_types) {
                if (t.name == name) {
                    return t.code;
                }
            }
        }
        return errc::unknown_problem;
    }
}

namespace sgcl::net::acme {
    inline errc problem::code() const noexcept {
        return detail::problem_code(type.view());
    }
}
