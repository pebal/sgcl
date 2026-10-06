//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../io/error.h"

#include <string>
#include <string_view>
#include <system_error>

namespace sgcl::net::ldap {
    // The result codes of LDAP (RFC 4511 §4.1.9, Appendix A), the same
    // numbers, for a result that is not success; and two of the client's
    // own: a message that breaks RFC 4511, a filter's text that breaks
    // RFC 4515
    enum class errc {
        operations_error = 1,
        protocol_error = 2,
        time_limit_exceeded = 3,
        size_limit_exceeded = 4,
        auth_method_not_supported = 7,
        stronger_auth_required = 8,
        referral = 10,
        admin_limit_exceeded = 11,
        unavailable_critical_extension = 12,
        confidentiality_required = 13,
        sasl_bind_in_progress = 14,
        no_such_attribute = 16,
        undefined_attribute_type = 17,
        inappropriate_matching = 18,
        constraint_violation = 19,
        attribute_or_value_exists = 20,
        invalid_attribute_syntax = 21,
        no_such_object = 32,
        alias_problem = 33,
        invalid_dn_syntax = 34,
        alias_dereferencing_problem = 36,
        inappropriate_authentication = 48,
        invalid_credentials = 49,
        insufficient_access_rights = 50,
        busy = 51,
        unavailable = 52,
        unwilling_to_perform = 53,
        loop_detect = 54,
        naming_violation = 64,
        object_class_violation = 65,
        not_allowed_on_non_leaf = 66,
        not_allowed_on_rdn = 67,
        entry_already_exists = 68,
        object_class_mods_prohibited = 69,
        affects_multiple_dsas = 71,
        other = 80,
        malformed = 1000,      // a message that breaks RFC 4511
        invalid_filter         // a filter's text that breaks RFC 4515
    };

    namespace detail {
        inline const char* ldap_code_text(int c) noexcept {
            switch (c) {
                case 1: return "operations error";
                case 2: return "protocol error";
                case 3: return "time limit exceeded";
                case 4: return "size limit exceeded";
                case 7: return "authentication method not supported";
                case 8: return "stronger authentication required";
                case 10: return "referral";
                case 11: return "administrative limit exceeded";
                case 12: return "unavailable critical extension";
                case 13: return "confidentiality required";
                case 14: return "SASL bind in progress";
                case 16: return "no such attribute";
                case 17: return "undefined attribute type";
                case 18: return "inappropriate matching";
                case 19: return "constraint violation";
                case 20: return "attribute or value exists";
                case 21: return "invalid attribute syntax";
                case 32: return "no such object";
                case 33: return "alias problem";
                case 34: return "invalid DN syntax";
                case 36: return "alias dereferencing problem";
                case 48: return "inappropriate authentication";
                case 49: return "invalid credentials";
                case 50: return "insufficient access rights";
                case 51: return "busy";
                case 52: return "unavailable";
                case 53: return "unwilling to perform";
                case 54: return "loop detected";
                case 64: return "naming violation";
                case 65: return "object class violation";
                case 66: return "not allowed on non-leaf";
                case 67: return "not allowed on RDN";
                case 68: return "entry already exists";
                case 69: return "object class modifications prohibited";
                case 71: return "affects multiple DSAs";
                case 80: return "other";
                case 1000: return "malformed LDAP message";
                case 1001: return "invalid LDAP filter";
            }
            return "unknown LDAP result";
        }

        class LdapCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "ldap";
            }

            std::string message(int c) const noexcept override {
                return ldap_code_text(c);
            }
        };
    }

    // The category of errc, named "ldap"
    inline const std::error_category& category() noexcept {
        static const detail::LdapCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), category());
    }

    // A server's answer to an operation (RFC 4511 §4.1.9): the result code,
    // the DN matched as far as the server could, its diagnostic message, the
    // URLs of a referral
    struct result {
        int code = 0;
        string matched_dn;
        string message;
        vector<string> referrals;

        friend bool operator==(const result&, const result&) noexcept = default;
    };
}

template<>
struct std::is_error_code_enum<sgcl::net::ldap::errc> : std::true_type {};

namespace sgcl::net::ldap {
    namespace detail {
        // A result as an error: its code in the category, the message in the
        // path, then the matched DN and each referral on lines of their own
        inline io::error ldap_error(const result& r, const string& op) noexcept {
            std::string p(r.message.view());
            if (!r.matched_dn.empty()) {
                p += "\nmatched-dn: ";
                p += r.matched_dn.view();
            }
            for (auto& u : r.referrals) {
                p += "\nreferral: ";
                p += u.view();
            }
            return io::error(error_code(r.code, category()), op, string(p));
        }

        inline io::error ldap_error(errc e, const string& op, const string& what = {}) noexcept {
            return io::error(make_error_code(e), op, what);
        }
    }

    // The server's result an error carries (an error of the category
    // "ldap" from a result); nullopt for any other error
    inline optional<result> result_of(const io::error& e) noexcept {
        if (e.code().category() != category() || e.code().value() >= 1000) {
            return nullopt;
        }
        result r;
        r.code = e.code().value();
        std::string_view p = e.path().view();
        size_t nl = p.find('\n');
        r.message = string(p.substr(0, nl));
        while (nl != std::string_view::npos) {
            size_t next = p.find('\n', nl + 1);
            std::string_view line = p.substr(nl + 1, next == std::string_view::npos ? std::string_view::npos : next - nl - 1);
            if (line.substr(0, 12) == "matched-dn: ") {
                r.matched_dn = string(line.substr(12));
            } else if (line.substr(0, 10) == "referral: ") {
                r.referrals.push_back(string(line.substr(10)));
            }
            nl = next;
        }
        return r;
    }
}
