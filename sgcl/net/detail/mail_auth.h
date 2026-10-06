//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// What DKIM, SPF and DMARC read mail and records by: a message's head as
// fields (RFC 5322 §2.2, folded lines kept as they are), the tag lists of
// DKIM-Signature, of a key record and of a DMARC record (RFC 6376 §3.2),
// names compared without case, a domain's labels
namespace sgcl::net::detail {
    SGCL_INLINE_HOT constexpr char mail_lower(char c) noexcept {
        return char(c + (unsigned(c - 'A') < 26u) * 32);
    }

    inline bool mail_iequal(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) {
            return false;
        }
        unsigned diff = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            diff |= unsigned(mail_lower(a[i]) != mail_lower(b[i]));
        }
        return diff == 0;
    }

    inline std::string mail_lowered(std::string_view s) {
        std::string out(s);
        for (char& c : out) {
            c = mail_lower(c);
        }
        return out;
    }

    SGCL_INLINE_HOT constexpr bool mail_wsp(char c) noexcept {
        return c == ' ' || c == '\t';
    }

    // FWS and CR/LF at both ends taken off
    inline std::string_view mail_trim(std::string_view s) noexcept {
        size_t b = 0, e = s.size();
        while (b < e && (mail_wsp(s[b]) || s[b] == '\r' || s[b] == '\n')) {
            ++b;
        }
        while (e > b && (mail_wsp(s[e - 1]) || s[e - 1] == '\r' || s[e - 1] == '\n')) {
            --e;
        }
        return s.substr(b, e - b);
    }

    // The text without its whitespace (FWS inside base64 of b=, bh=, p=)
    inline std::string mail_no_space(std::string_view s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (!mail_wsp(c) && c != '\r' && c != '\n') {
                out += c;
            }
        }
        return out;
    }

    // A message in CRLF: as it is when it has no bare LF (what SMTP
    // carries), else a copy with every bare LF made CRLF (a file of LF lines)
    inline bool mail_has_bare_lf(std::string_view m) noexcept {
        size_t at = m.find('\n');
        while (at != std::string_view::npos) {
            if (at == 0 || m[at - 1] != '\r') {
                return true;
            }
            at = m.find('\n', at + 1);
        }
        return false;
    }

    inline std::string mail_crlf(std::string_view m) {
        std::string out;
        out.reserve(m.size() + m.size() / 32);
        for (size_t i = 0; i < m.size(); ++i) {
            if (m[i] == '\n' && (i == 0 || m[i - 1] != '\r')) {
                out += '\r';
            }
            out += m[i];
        }
        return out;
    }

    // One field of a head: its name as written, and its whole text from
    // the name to the CRLF that ends its last line, that CRLF included
    struct MailHeaderField {
        std::string_view name;
        std::string_view raw;
    };

    // A message (CRLF lines) cut into its fields and its body: the head
    // ends at the first empty line, the body is what follows it (none
    // without an empty line). A line of the head without a colon that is
    // no continuation ends the head there (as the readers of mail take
    // it); malformed says so
    struct MailSplit {
        vector<MailHeaderField> fields;
        std::string_view body;
        bool has_body = false;
        bool malformed = false;
    };

    inline MailSplit mail_split(std::string_view m) {
        MailSplit out;
        size_t at = 0;
        while (at < m.size()) {
            if (m.substr(at, 2) == "\r\n") {
                out.body = m.substr(at + 2);
                out.has_body = true;
                return out;
            }
            size_t start = at;
            size_t colon = std::string_view::npos;
            // the field's first line, then its continuations
            for (;;) {
                size_t eol = m.find("\r\n", at);
                if (eol == std::string_view::npos) {
                    eol = m.size();
                    if (colon == std::string_view::npos) {
                        colon = m.substr(start, eol - start).find(':');
                    }
                    at = eol;
                    break;
                }
                if (colon == std::string_view::npos) {
                    colon = m.substr(start, eol - start).find(':');
                }
                at = eol + 2;
                if (at >= m.size() || !mail_wsp(m[at])) {
                    break;
                }
            }
            std::string_view name = colon == std::string_view::npos ? std::string_view() : m.substr(start, colon);
            while (!name.empty() && mail_wsp(name.back())) {
                name.remove_suffix(1);
            }
            // a field name is printable ASCII without a colon (RFC 5322 §2.2);
            // a line that starts with whitespace and follows no field is none
            bool printable = !name.empty();
            for (char c : name) {
                printable &= c > 0x20 && c < 0x7F;
            }
            if (!printable) {
                out.malformed = true;
                out.body = m.substr(start);
                out.has_body = true;
                return out;
            }
            out.fields.push_back(MailHeaderField{name, m.substr(start, at - start)});
        }
        return out;
    }

    // A tag list (RFC 6376 §3.2): tag=value pairs split by ";", FWS
    // around both taken off; a tag without "=", an empty or repeated name,
    // a name that is not ALPHA *ALNUMPUNC is false (lenient: any name of
    // visible characters, for DMARC's unknown tags, RFC 7489 §6.3)
    struct MailTag {
        std::string_view name;
        std::string_view value;
    };

    inline bool mail_tag_list(std::string_view s, vector<MailTag>& out, bool lenient = false) {
        out.clear();
        size_t at = 0;
        while (at <= s.size()) {
            size_t semi = s.find(';', at);
            std::string_view spec = s.substr(at, semi == std::string_view::npos ? std::string_view::npos : semi - at);
            at = semi == std::string_view::npos ? s.size() + 1 : semi + 1;
            if (mail_trim(spec).empty()) {
                if (semi == std::string_view::npos) {
                    break;   // the ";" after the last spec is optional
                }
                if (at <= s.size() && !mail_trim(s.substr(at)).empty()) {
                    return false;   // an empty spec between two
                }
                continue;
            }
            size_t eq = spec.find('=');
            if (eq == std::string_view::npos) {
                return false;
            }
            std::string_view name = mail_trim(spec.substr(0, eq));
            std::string_view value = mail_trim(spec.substr(eq + 1));
            if (name.empty() || !((name[0] | 0x20) >= 'a' && (name[0] | 0x20) <= 'z')) {
                return false;
            }
            for (char c : name) {
                bool ok = (c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || c == '_' || (lenient && c > 0x20 && c < 0x7F);
                if (!ok) {
                    return false;
                }
            }
            for (auto& t : out) {
                if (t.name == name) {
                    return false;
                }
            }
            out.push_back(MailTag{name, value});
        }
        return true;
    }

    inline const MailTag* mail_tag(const vector<MailTag>& tags, std::string_view name) noexcept {
        for (auto& t : tags) {
            if (t.name == name) {
                return &t;
            }
        }
        return nullptr;
    }

    // A domain without a dot at its end, in lower case
    inline std::string mail_domain(std::string_view d) {
        while (!d.empty() && d.back() == '.') {
            d.remove_suffix(1);
        }
        return mail_lowered(d);
    }

    // Whether sub is domain or a name under it (both lower case, no dot at
    // the end)
    inline bool mail_under(std::string_view sub, std::string_view domain) noexcept {
        if (sub.size() == domain.size()) {
            return sub == domain;
        }
        return sub.size() > domain.size() && sub.substr(sub.size() - domain.size()) == domain && sub[sub.size() - domain.size() - 1] == '.';
    }

    // The domain of an address "local@domain" (the last "@"), in lower
    // case; "" without one
    inline std::string mail_address_domain(std::string_view a) {
        size_t at = a.rfind('@');
        if (at == std::string_view::npos) {
            return std::string();
        }
        return mail_domain(a.substr(at + 1));
    }

    // Whether the name is a domain a check may ask for: labels of 1 to 63
    // characters, 253 at most in all, at least two labels (RFC 7208 §4.3)
    inline bool mail_valid_domain(std::string_view d, bool multi_label = true) noexcept {
        while (!d.empty() && d.back() == '.') {
            d.remove_suffix(1);
        }
        if (d.empty() || d.size() > 253) {
            return false;
        }
        size_t label = 0, labels = 1;
        for (char c : d) {
            if (c == '.') {
                if (label == 0) {
                    return false;
                }
                label = 0;
                ++labels;
                continue;
            }
            if (++label > 63) {
                return false;
            }
        }
        return label > 0 && (!multi_label || labels >= 2);
    }
}
