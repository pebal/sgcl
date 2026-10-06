//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/duration.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <cstdint>
#include <string>
#include <string_view>

// The values of LDAP's operations (RFC 4511)
namespace sgcl::net::ldap {
    namespace detail {
        inline bool ldap_iequal(std::string_view a, std::string_view b) noexcept {
            if (a.size() != b.size()) {
                return false;
            }
            for (size_t i = 0; i < a.size(); ++i) {
                char x = a[i], y = b[i];
                x = char(x + (unsigned(x - 'A') < 26u) * 32);
                y = char(y + (unsigned(y - 'A') < 26u) * 32);
                if (x != y) {
                    return false;
                }
            }
            return true;
        }
    }

    // How a client protects its connection: TLS from the first byte
    // (ldaps://, port 636), StartTLS before anything else (RFC 4511 §4.14),
    // or none (a test's loopback, a unix socket); automatic is TLS for
    // ldaps:// and port 636, StartTLS for the rest
    enum class security : uint8_t { automatic, tls, starttls, none };

    // How far a search looks under its base (RFC 4511 §4.5.1.2)
    enum class scope : uint8_t {
        base = 0,      // the base entry alone
        one = 1,       // its children
        subtree = 2    // the base and everything under it
    };

    // When aliases are followed (RFC 4511 §4.5.1.3)
    enum class deref : uint8_t { never = 0, searching = 1, finding = 2, always = 3 };

    // An attribute of an entry: its description ("cn", "userCertificate;binary")
    // and its values, bytes in strings (a binary value as it is)
    struct attribute {
        string name;
        vector<string> values;

        friend bool operator==(const attribute&, const attribute&) noexcept = default;
    };

    // An entry: its distinguished name and its attributes, as a search gives
    // it and add takes it
    struct entry {
        string dn;
        vector<attribute> attributes;

        // The first value of the attribute (its name compared without
        // case); "" for none
        string get(const string& name) const {
            for (auto& a : attributes) {
                if (detail::ldap_iequal(a.name.view(), name.view()) && !a.values.empty()) {
                    return a.values[0];
                }
            }
            return string();
        }

        // Every value of the attribute; none for an attribute the entry lacks
        vector<string> get_all(const string& name) const {
            for (auto& a : attributes) {
                if (detail::ldap_iequal(a.name.view(), name.view())) {
                    return a.values;
                }
            }
            return vector<string>();
        }

        friend bool operator==(const entry&, const entry&) noexcept = default;
    };

    // What a search asks (RFC 4511 §4.5.1): the base, the scope, the filter
    // as RFC 4515 writes it, the attributes, the limits, paging (RFC 2696)
    struct search_request {
        string base;
        ldap::scope scope = ldap::scope::subtree;
        string filter = string("(objectClass=*)");
        vector<string> attributes;          // empty: all user attributes ("*")
        bool types_only = false;            // the attributes' names without their values
        size_t size_limit = 0;              // entries at most; 0: the server's
        duration time_limit = {};           // whole seconds; zero: the server's
        ldap::deref deref = ldap::deref::never;
        uint32_t page_size = 0;             // pages of this size requested until the last; 0: one request
    };

    // What a search gives: the entries, the URLs of the references to
    // other servers it met (SearchResultReference, not followed), and
    // whether a limit stopped it before the end
    struct search_result {
        vector<entry> entries;
        vector<string> referrals;
        bool truncated = false;   // the size or the time limit reached (result 4 or 3): the entries found until then
    };

    // How a modification changes an attribute (RFC 4511 §4.6, RFC 4525)
    enum class modify_op : uint8_t {
        add = 0,          // the values added
        remove = 1,       // the values taken off; all of them when none are given
        replace = 2,      // the values become the attribute's; none removes it
        increment = 3     // the value added to the attribute's integer (RFC 4525)
    };

    // One change of a modify
    struct modification {
        modify_op op = modify_op::replace;
        string attribute;
        vector<string> values;

        friend bool operator==(const modification&, const modification&) noexcept = default;
    };
}
