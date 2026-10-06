//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../../core/aliases.h"
#include "../../../core/string.h"
#include "../../../core/vector.h"
#include "../../../encoding/asn1.h"

#include <string>
#include <string_view>

// A search filter's text (RFC 4515) read into its BER (RFC 4511 §4.5.1.7)
namespace sgcl::net::ldap::detail {
    using encoding::asn1;

    // A constructed element of the tag whose content is the elements given
    // in their order (a SET OF of a filter keeps the order it was written in)
    inline asn1 ldap_cons(asn1::tag_class c, uint32_t number, const vector<asn1>& items) {
        std::string content;
        for (auto& i : items) {
            auto b = i.bytes();
            content.append(reinterpret_cast<const char*>(b.data()), b.size());
        }
        return asn1::raw(c, number, true, slice<const byte>(reinterpret_cast<const byte*>(content.data()), content.size()));
    }

    inline asn1 ldap_prim(asn1::tag_class c, uint32_t number, std::string_view content) {
        return asn1::raw(c, number, false, slice<const byte>(reinterpret_cast<const byte*>(content.data()), content.size()));
    }

    inline asn1 ldap_str(std::string_view s) {
        return asn1::octet_string(slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size()));
    }

    // An element's header and content appended: the tag byte (a context-
    // specific tag below 31), the definite length, the content
    inline void ldap_tlv(std::string& out, uint8_t tag, std::string_view content) {
        out += char(tag);
        size_t n = content.size();
        if (n < 128) {
            out += char(n);
        } else if (n < 256) {
            out += char(0x81);
            out += char(n);
        } else if (n < 65536) {
            out += char(0x82);
            out += char(n >> 8);
            out += char(n);
        } else {
            out += char(0x84);
            out += char(n >> 24);
            out += char(n >> 16);
            out += char(n >> 8);
            out += char(n);
        }
        out.append(content);
    }

    // An element opened in place: its tag and a length byte written, the
    // content written after them, the length set when it is closed (bytes
    // inserted only for a content of 128 bytes or more)
    inline size_t ldap_open(std::string& out, uint8_t tag) {
        out += char(tag);
        out += char(0);
        return out.size();
    }

    inline void ldap_close(std::string& out, size_t content_at) {
        size_t n = out.size() - content_at;
        if (n < 128) {
            out[content_at - 1] = char(n);
            return;
        }
        char head[5];
        size_t k = n < 256 ? 1 : n < 65536 ? 2 : 4;
        head[0] = char(0x80 | k);
        for (size_t i = 0; i < k; ++i) {
            head[1 + i] = char(n >> (8 * (k - 1 - i)));
        }
        out.replace(content_at - 1, 1, head, k + 1);
    }

    // The parser writes the filter's BER as it reads, into one string (no
    // element made per node)
    struct LdapFilterParser {
        std::string_view s;
        size_t at = 0;
        int depth = 0;
        bool bad = false;

        static int hex(char c) noexcept {
            if (c >= '0' && c <= '9') {
                return c - '0';
            }
            c = char(c | 0x20);
            if (c >= 'a' && c <= 'f') {
                return c - 'a' + 10;
            }
            return -1;
        }

        // An attribute description: a descr or a numeric OID with options
        // (RFC 4512 §2.5)
        bool attr(std::string_view a) const noexcept {
            if (a.empty()) {
                return false;
            }
            for (char c : a) {
                bool ok = (c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || c == '-' || c == '.' || c == ';';
                if (!ok) {
                    return false;
                }
            }
            return true;
        }

        // A value up to the next unescaped "*" or ")", its \XX undone
        bool value(std::string& out, bool stop_at_star) {
            out.clear();
            while (at < s.size()) {
                char c = s[at];
                if (c == ')' || (stop_at_star && c == '*')) {
                    return true;
                }
                if (c == '(' || (!stop_at_star && c == '*')) {
                    return false;
                }
                if (c == '\\') {
                    if (at + 2 >= s.size()) {
                        return false;
                    }
                    int h = hex(s[at + 1]), l = hex(s[at + 2]);
                    if (h < 0 || l < 0) {
                        return false;
                    }
                    out += char(h * 16 + l);
                    at += 3;
                    continue;
                }
                out += c;
                ++at;
            }
            return true;
        }

        bool filter(std::string& out) {
            if (bad || at >= s.size() || s[at] != '(' || ++depth > 64) {
                bad = true;
                return false;
            }
            ++at;
            if (!component(out) || at >= s.size() || s[at] != ')') {
                bad = true;
                return false;
            }
            ++at;
            --depth;
            return true;
        }

        bool component(std::string& out) {
            if (at >= s.size()) {
                return false;
            }
            char c = s[at];
            if (c == '&' || c == '|') {
                ++at;
                size_t open = ldap_open(out, c == '&' ? 0xA0 : 0xA1);
                while (at < s.size() && s[at] == '(') {
                    if (!filter(out)) {
                        return false;
                    }
                }
                ldap_close(out, open);   // RFC 4526's absolute true and false: "(&)" and "(|)"
                return true;
            }
            if (c == '!') {
                ++at;
                size_t open = ldap_open(out, 0xA2);
                if (!filter(out)) {
                    return false;
                }
                ldap_close(out, open);
                return true;
            }
            return item(out);
        }

        static void pair(std::string& out, uint8_t tag, std::string_view a, std::string_view v) {
            size_t open = ldap_open(out, tag);
            ldap_tlv(out, 0x04, a);
            ldap_tlv(out, 0x04, v);
            ldap_close(out, open);
        }

        bool item(std::string& out) {
            size_t start = at;
            while (at < s.size() && s[at] != '=' && s[at] != '~' && s[at] != '>' && s[at] != '<' && s[at] != ')' && s[at] != '(') {
                ++at;
            }
            if (at >= s.size() || s[at] == ')' || s[at] == '(') {
                return false;
            }
            std::string_view left = s.substr(start, at - start);
            char op = s[at];
            std::string v;
            if (op != '=') {
                if (at + 1 >= s.size() || s[at + 1] != '=' || !attr(left)) {
                    return false;
                }
                at += 2;
                if (!value(v, false)) {
                    return false;
                }
                pair(out, op == '>' ? 0xA5 : op == '<' ? 0xA6 : 0xA8, left, v);
                return true;
            }
            ++at;   // past "="
            if (!left.empty() && left.back() == ':') {
                return extensible(out, left.substr(0, left.size() - 1));
            }
            if (!attr(left)) {
                return false;
            }
            // present, equality or substrings
            if (s.substr(at, 2) == "*)") {
                ++at;
                ldap_tlv(out, 0x87, left);
                return true;
            }
            if (!value(v, true)) {
                return false;
            }
            if (at < s.size() && s[at] == ')') {
                pair(out, 0xA3, left, v);
                return true;
            }
            // a "*": substrings
            size_t open = ldap_open(out, 0xA4);
            ldap_tlv(out, 0x04, left);
            size_t seq = ldap_open(out, 0x30);
            if (!v.empty()) {
                ldap_tlv(out, 0x80, v);
            }
            while (at < s.size() && s[at] == '*') {
                ++at;
                if (!value(v, true)) {
                    return false;
                }
                if (at < s.size() && s[at] == ')') {
                    if (!v.empty()) {
                        ldap_tlv(out, 0x82, v);
                    }
                    break;
                }
                if (v.empty()) {
                    return false;   // "**"
                }
                ldap_tlv(out, 0x81, v);
            }
            if (out.size() == seq) {
                return false;   // "(cn=*)" is present, handled above: nothing here is no substring
            }
            ldap_close(out, seq);
            ldap_close(out, open);
            return true;
        }

        // attr [":dn"] [":" rule] ":=" value, or [":dn"] ":" rule ":=" value
        bool extensible(std::string& out, std::string_view left) {
            std::string_view type = left, rule;
            bool dn = false;
            size_t colon = left.find(':');
            if (colon != std::string_view::npos) {
                type = left.substr(0, colon);
                std::string_view rest = left.substr(colon + 1);
                if (rest.size() >= 2 && (rest.substr(0, 2) == "dn" || rest.substr(0, 2) == "DN") && (rest.size() == 2 || rest[2] == ':')) {
                    dn = true;
                    rest = rest.size() == 2 ? std::string_view() : rest.substr(3);
                }
                rule = rest;
            }
            if ((type.empty() && rule.empty()) || (!type.empty() && !attr(type)) || (!rule.empty() && !attr(rule))) {
                return false;
            }
            std::string v;
            if (!value(v, false)) {
                return false;
            }
            size_t open = ldap_open(out, 0xA9);
            if (!rule.empty()) {
                ldap_tlv(out, 0x81, rule);
            }
            if (!type.empty()) {
                ldap_tlv(out, 0x82, type);
            }
            ldap_tlv(out, 0x83, v);
            if (dn) {
                ldap_tlv(out, 0x84, std::string_view("\xff", 1));
            }
            ldap_close(out, open);
            return true;
        }
    };

    // A filter's text into its element; asn1() for one that breaks RFC 4515.
    // A text without its outer parentheses ("cn=alice") is taken as if it
    // had them, as ldapsearch takes it
    inline asn1 ldap_filter(std::string_view text) {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
            text.remove_prefix(1);
        }
        while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
            text.remove_suffix(1);
        }
        std::string wrapped;
        if (text.empty() || text.front() != '(') {
            wrapped = "(" + std::string(text) + ")";
            text = wrapped;
        }
        LdapFilterParser p{text};
        std::string bytes;
        bytes.reserve(text.size() + 16);
        if (!p.filter(bytes) || p.bad || p.at != text.size()) {
            return asn1();
        }
        vector<byte> v(reinterpret_cast<const byte*>(bytes.data()), reinterpret_cast<const byte*>(bytes.data()) + bytes.size());
        auto f = asn1::parse(v);
        return f ? *f : asn1();
    }
}
