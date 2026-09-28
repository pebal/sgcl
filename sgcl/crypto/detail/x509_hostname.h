//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// The names of X.509 as text: a host name matched against a certificate's
// dNSNames (RFC 6125 §6.4), the check of a dNSName, a mailbox (RFC 2821
// §4.1.2, as RFC 5280 §4.2.1.6 has an rfc822Name) and the host of a URI,
// and the suffix match of name constraints (RFC 5280 §4.2.1.10, with the
// leading period of errata 5997). The rules are Go's crypto/x509, which the
// tests hold this against: ASCII case folding only, a wildcard only as the
// whole leftmost label and only in the certificate, never across labels.
// Nothing here parses an IP address: an address is compared as bytes
// (x509.h); the one question asked of a text is whether it is written as
// an IPv4 address, so that such a text is never matched as a DNS name.
namespace sgcl::crypto::detail::x509_names {
    inline char lower(char c) noexcept {
        return c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : c;
    }

    inline std::string to_lower(std::string_view s) {
        std::string r(s);
        for (auto& c : r) {
            c = lower(c);
        }
        return r;
    }

    inline bool equal_fold(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            if (lower(a[i]) != lower(b[i])) {
                return false;
            }
        }
        return true;
    }

    // Whether s is a host name RFC 6125 §2.2 lets be matched, with Go's
    // leniency: labels of letters, digits, '-' (not first) and '_', none
    // empty; a pattern may have "*" as its whole first label; an input may
    // end in one '.', which is dropped; "*" alone is neither
    inline bool valid_hostname(std::string_view s, bool pattern) noexcept {
        if (!pattern && !s.empty() && s.back() == '.') {
            s.remove_suffix(1);
        }
        if (s.empty() || s == "*") {
            return false;
        }
        size_t i = 0;
        for (size_t label = 0;; ++label) {
            size_t end = s.find('.', i);
            std::string_view part = s.substr(i, end == std::string_view::npos ? std::string_view::npos : end - i);
            if (part.empty()) {
                return false;
            }
            if (!(pattern && label == 0 && part == "*")) {
                for (size_t j = 0; j < part.size(); ++j) {
                    char c = part[j];
                    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '-' && j != 0) || c == '_';
                    if (!ok) {
                        return false;
                    }
                }
            }
            if (end == std::string_view::npos) {
                return true;
            }
            i = end + 1;
        }
    }

    // A certificate's pattern against a host, both valid: label for label,
    // folded to lower case, the pattern's first label "*" standing for any
    // one label (never for none, never for two)
    inline bool match_hostname(std::string_view pattern, std::string_view host) noexcept {
        if (!host.empty() && host.back() == '.') {
            host.remove_suffix(1);
        }
        size_t pi = 0, hi = 0;
        for (size_t label = 0;; ++label) {
            size_t pe = pattern.find('.', pi);
            size_t he = host.find('.', hi);
            std::string_view p = pattern.substr(pi, pe == std::string_view::npos ? std::string_view::npos : pe - pi);
            std::string_view h = host.substr(hi, he == std::string_view::npos ? std::string_view::npos : he - hi);
            if (!(label == 0 && p == "*") && !equal_fold(p, h)) {
                return false;
            }
            if (label == 0 && p == "*" && h.empty()) {
                return false;
            }
            if ((pe == std::string_view::npos) != (he == std::string_view::npos)) {
                return false;   // not as many labels
            }
            if (pe == std::string_view::npos) {
                return true;
            }
            pi = pe + 1;
            hi = he + 1;
        }
    }

    // The exact comparison for what is not a valid host name: folded, and
    // never of an empty name or "."
    inline bool match_exactly(std::string_view a, std::string_view b) noexcept {
        if (a.empty() || a == "." || b.empty() || b == ".") {
            return false;
        }
        return equal_fold(a, b);
    }

    // Whether a certificate's dNSName pattern matches the host asked:
    // wildcards and the trailing dot only where both are valid host names,
    // else the exact comparison (Go's VerifyHostname)
    inline bool host_matches(std::string_view pattern, std::string_view host) noexcept {
        if (valid_hostname(host, false) && valid_hostname(pattern, true)) {
            return match_hostname(pattern, host);
        }
        return match_exactly(pattern, host);
    }

    // Whether s is written as an IPv4 address: four decimal numbers of 0
    // to 255 with no leading zero, joined by '.'
    inline bool looks_like_ipv4(std::string_view s) noexcept {
        int parts = 0;
        size_t i = 0;
        while (true) {
            size_t start = i;
            unsigned v = 0;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9' && i - start < 3) {
                v = v * 10 + unsigned(s[i] - '0');
                ++i;
            }
            size_t len = i - start;
            if (len == 0 || v > 255 || (len > 1 && s[start] == '0')) {
                return false;
            }
            ++parts;
            if (i == s.size()) {
                return parts == 4;
            }
            if (s[i] != '.' || parts == 4) {
                return false;
            }
            ++i;
        }
    }

    // Whether s is written as an IPv6 address, loosely: hex digits, ':'
    // and '.' (an IPv4 tail) with at least two ':'. Only to refuse such a
    // text where a domain is wanted
    inline bool looks_like_ipv6(std::string_view s) noexcept {
        int colons = 0;
        for (char c : s) {
            if (c == ':') {
                ++colons;
            } else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '.')) {
                return false;
            }
        }
        return colons >= 2;
    }

    // A domain of a SAN or of a constraint as Go checks it: printable
    // ASCII, no label empty, no trailing dot; a constraint may start with
    // '.', and may be empty (it matches every name)
    inline bool domain_valid(std::string_view s, bool constraint) noexcept {
        if (s.empty()) {
            return true;
        }
        if (s.back() == '.') {
            return false;
        }
        if (constraint && s[0] == '.') {
            s.remove_prefix(1);
        }
        size_t label = 0;
        for (size_t i = 0; i <= s.size(); ++i) {
            if (i < s.size() && (static_cast<unsigned char>(s[i]) < 33 || static_cast<unsigned char>(s[i]) > 126)) {
                return false;
            }
            if (i == s.size() || s[i] == '.') {
                if (label == 0) {
                    return false;
                }
                label = 0;
            } else {
                ++label;
            }
        }
        return true;
    }

    // A mailbox of RFC 2821 as Go reads an rfc822Name: a dot-atom or a
    // quoted string before '@' (backslash escapes and a space in quotes
    // allowed, as the errata argue), anything valid as a domain after it
    // with no second '@'. The local part without its quoting in local, the
    // domain in domain
    inline bool parse_mailbox(std::string_view in, std::string& local, std::string& domain) {
        local.clear();
        if (in.empty()) {
            return false;
        }
        size_t i = 0;
        if (in[0] == '"') {
            ++i;
            for (;;) {
                if (i >= in.size()) {
                    return false;
                }
                unsigned char c = static_cast<unsigned char>(in[i++]);
                if (c == '"') {
                    break;
                }
                if (c == '\\') {
                    if (i >= in.size()) {
                        return false;
                    }
                    unsigned char d = static_cast<unsigned char>(in[i]);
                    if (d == 11 || d == 12 || (d >= 1 && d <= 9) || (d >= 14 && d <= 127)) {
                        local += char(d);
                        ++i;
                        continue;
                    }
                    return false;
                }
                if (c == 11 || c == 12 || c == 32 || c == 33 || c == 127 || (c >= 1 && c <= 8) || (c >= 14 && c <= 31) || (c >= 35 && c <= 91) || (c >= 93 && c <= 126)) {
                    local += char(c);
                    continue;
                }
                return false;
            }
        } else {
            while (i < in.size()) {
                char c = in[i];
                if (c == '\\') {
                    ++i;
                    if (i >= in.size()) {
                        return false;
                    }
                    c = in[i];
                    // an escaped character counts as an atext one, whatever it is
                    local += c;
                    ++i;
                    continue;
                }
                bool atext = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '!' || c == '#' || c == '$'
                          || c == '%' || c == '&' || c == '\'' || c == '*' || c == '+' || c == '-' || c == '/' || c == '=' || c == '?' || c == '^'
                          || c == '_' || c == '`' || c == '{' || c == '|' || c == '}' || c == '~' || c == '.';
                if (!atext) {
                    break;
                }
                local += c;
                ++i;
            }
            if (local.empty() || local.front() == '.' || local.back() == '.' || local.find("..") != std::string::npos) {
                return false;
            }
        }
        if (i >= in.size() || in[i] != '@') {
            return false;
        }
        std::string_view d = in.substr(i + 1);
        if (!domain_valid(d, false) || d.find('@') != std::string_view::npos) {
            return false;
        }
        domain.assign(d);
        return true;
    }

    // The host of a URI, "scheme://userinfo@host:port/path": host and port
    // as they are written (an IPv6 literal with its brackets), empty for a
    // URI with no authority. False for a control character, which Go's
    // url.Parse refuses
    inline bool uri_host(std::string_view uri, std::string_view& host) noexcept {
        host = {};
        for (char c : uri) {
            if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
                return false;
            }
        }
        size_t colon = uri.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            return true;   // a relative reference: no authority
        }
        for (size_t i = 0; i < colon; ++i) {
            char c = uri[i];
            bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (i > 0 && ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.'));
            if (!ok) {
                return true;   // not a scheme: a path with a colon
            }
        }
        std::string_view rest = uri.substr(colon + 1);
        if (rest.substr(0, 2) != "//") {
            return true;
        }
        rest.remove_prefix(2);
        size_t end = rest.find_first_of("/?#");
        std::string_view authority = rest.substr(0, end);
        size_t at = authority.rfind('@');
        if (at != std::string_view::npos) {
            authority.remove_prefix(at + 1);
        }
        host = authority;
        return true;
    }

    // The host of a URI's authority without its port, folded, for name
    // constraints; false for an empty host, an IP address (a URI holding
    // one cannot be checked against a domain) or a host that is not a
    // valid domain
    inline bool uri_constraint_host(std::string_view authority, std::string& out) {
        std::string host = to_lower(authority);
        if (host.empty()) {
            return false;
        }
        if (host.front() == '[') {
            return false;   // an IPv6 literal
        }
        size_t colon = host.find(':');
        if (colon != std::string::npos) {
            std::string_view port = std::string_view(host).substr(colon + 1);
            for (char c : port) {
                if (c < '0' || c > '9') {
                    return false;
                }
            }
            host.resize(colon);
            if (host.empty()) {
                return false;
            }
        }
        if (looks_like_ipv4(host) || !domain_valid(host, false)) {
            return false;
        }
        out = std::move(host);
        return true;
    }

    // Whether the constraint (a domain, maybe with a leading '.') covers
    // the name: a suffix of it at a label boundary, folded; ".example.com"
    // only names below example.com, "example.com" the name itself too;
    // "" every name
    inline bool dns_has_suffix(std::string_view constraint, std::string_view name) noexcept {
        if (constraint.empty()) {
            return true;
        }
        if (constraint.size() > name.size()) {
            return false;
        }
        size_t off = name.size() - constraint.size();
        if (!equal_fold(constraint, name.substr(off))) {
            return false;
        }
        if (constraint[0] != '.' && name.size() > constraint.size() && name[off - 1] != '.') {
            return false;
        }
        return true;
    }

    // Whether an excluded constraint takes a wildcard name: "*.example.com"
    // is excluded by "foo.example.com", since it would match that name
    inline bool excluded_wildcard(std::string_view constraint, std::string_view name) noexcept {
        if (name.empty() || name[0] != '*') {
            return false;
        }
        size_t nd = name.find('.');
        size_t cd = constraint.find('.');
        if (nd == std::string_view::npos || cd == std::string_view::npos) {
            return false;
        }
        return equal_fold(name.substr(nd), constraint.substr(cd));
    }
}
