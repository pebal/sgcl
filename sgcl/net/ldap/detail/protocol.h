//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "filter.h"
#include "../error.h"
#include "../types.h"
#include "../../../core/aliases.h"
#include "../../../core/string.h"
#include "../../../core/vector.h"
#include "../../../encoding/asn1.h"

#include <cstdint>
#include <string>
#include <string_view>

// LDAP's messages (RFC 4511 §4): the envelope, the operations' tags, the
// result, the controls; the frame of a BER element in a stream
namespace sgcl::net::ldap::detail {
    using encoding::asn1;

    namespace op {
        inline constexpr uint32_t bind_request = 0;
        inline constexpr uint32_t bind_response = 1;
        inline constexpr uint32_t unbind_request = 2;
        inline constexpr uint32_t search_request = 3;
        inline constexpr uint32_t search_entry = 4;
        inline constexpr uint32_t search_done = 5;
        inline constexpr uint32_t modify_request = 6;
        inline constexpr uint32_t modify_response = 7;
        inline constexpr uint32_t add_request = 8;
        inline constexpr uint32_t add_response = 9;
        inline constexpr uint32_t del_request = 10;
        inline constexpr uint32_t del_response = 11;
        inline constexpr uint32_t moddn_request = 12;
        inline constexpr uint32_t moddn_response = 13;
        inline constexpr uint32_t compare_request = 14;
        inline constexpr uint32_t compare_response = 15;
        inline constexpr uint32_t abandon_request = 16;
        inline constexpr uint32_t search_reference = 19;
        inline constexpr uint32_t extended_request = 23;
        inline constexpr uint32_t extended_response = 24;
        inline constexpr uint32_t intermediate_response = 25;
    }

    inline constexpr std::string_view OidStartTls = "1.3.6.1.4.1.1466.20037";
    inline constexpr std::string_view OidWhoAmI = "1.3.6.1.4.1.4203.1.11.3";
    inline constexpr std::string_view OidNoticeOfDisconnection = "1.3.6.1.4.1.1466.20036";
    inline constexpr std::string_view OidPagedResults = "1.2.840.113556.1.4.319";

    // The size of the BER element at the buffer's front: 1 with its total
    // size, 0 when its header is not whole, -1 for one that cannot be a
    // message (an indefinite length, a length of more than four bytes)
    inline int ldap_frame(std::string_view b, size_t& total) noexcept {
        if (b.size() < 2) {
            return 0;
        }
        size_t at = 1;
        if ((uint8_t(b[0]) & 0x1F) == 0x1F) {
            return -1;   // a tag of high number: no message has one
        }
        uint8_t l = uint8_t(b[at++]);
        uint64_t length = 0;
        if (l < 0x80) {
            length = l;
        } else {
            size_t n = l & 0x7F;
            if (n == 0 || n > 4) {
                return -1;
            }
            if (b.size() < at + n) {
                return 0;
            }
            for (size_t i = 0; i < n; ++i) {
                length = length << 8 | uint8_t(b[at++]);
            }
        }
        total = at + size_t(length);
        return 1;
    }

    // Whether a message's bytes hold only definite lengths, each element
    // inside the one that holds it (RFC 4511 §5.1: LDAP has no indefinite
    // form); checked before the BER parse, one pass over the headers
    inline bool ldap_definite(const uint8_t* p, size_t n) noexcept {
        size_t ends[64];
        size_t depth = 0;
        size_t at = 0;
        size_t limit = n;   // the end of the innermost element open
        while (at < n) {
            while (depth && at == ends[depth - 1]) {
                limit = --depth ? ends[depth - 1] : n;
            }
            uint8_t t = p[at++];
            if ((t & 0x1F) == 0x1F) {   // a tag of high number: its bytes until one below 0x80
                size_t k = 0;
                do {
                    if (at >= limit || ++k > 5) {
                        return false;
                    }
                } while (p[at++] & 0x80);
            }
            if (at >= limit) {
                return false;
            }
            uint8_t l = p[at++];
            uint64_t length = l;
            if (l >= 0x80) {
                size_t k = l & 0x7F;
                if (k == 0 || k > 4 || limit - at < k) {
                    return false;   // the indefinite form, or a length past 4 bytes
                }
                length = 0;
                for (size_t i = 0; i < k; ++i) {
                    length = length << 8 | p[at++];
                }
            }
            if (length > limit - at) {
                return false;
            }
            if (t & 0x20) {
                if (depth == 64) {
                    return false;
                }
                ends[depth++] = at + size_t(length);
                limit = at + size_t(length);
            } else {
                at += size_t(length);
            }
        }
        while (depth && ends[depth - 1] == at) {
            --depth;
        }
        return at == n && depth == 0;
    }

    inline std::string ldap_text(const asn1& e) {
        auto b = e.as_bytes();
        if (!b) {
            return std::string();
        }
        return std::string(reinterpret_cast<const char*>(b->data()), b->size());
    }

    inline asn1 ldap_app(uint32_t number, const vector<asn1>& items) {
        return ldap_cons(asn1::tag_class::application, number, items);
    }

    // The envelope: SEQUENCE { messageID, protocolOp, controls [0] OPTIONAL }
    inline vector<byte> ldap_message(int32_t id, const asn1& operation, const vector<asn1>& controls = {}) {
        vector<asn1> parts{asn1::integer(id), operation};
        if (!controls.empty()) {
            parts.push_back(ldap_cons(asn1::tag_class::context_specific, 0, controls));
        }
        auto b = ldap_cons(asn1::tag_class::universal, 16, parts).bytes();
        return vector<byte>(b.begin(), b.end());
    }

    inline asn1 ldap_control(std::string_view oid, bool critical, const asn1& value) {
        vector<asn1> parts{ldap_str(oid)};
        if (critical) {
            parts.push_back(asn1::boolean(true));
        }
        if (value) {
            auto b = value.bytes();
            parts.push_back(asn1::octet_string(b));
        }
        return asn1::sequence(parts);
    }

    // An LDAPResult's fields from the operation's element (RFC 4511 §4.1.9)
    inline bool ldap_read_result(const asn1& op, result& r) {
        asn1 code = op[0], matched = op[1], message = op[2];
        auto c = code.as_int();
        if (!c || !matched.is(asn1::type::octet_string) || !message.is(asn1::type::octet_string)) {
            return false;
        }
        r.code = int(*c);
        r.matched_dn = string(ldap_text(matched));
        r.message = string(ldap_text(message));
        for (size_t i = 3; i < op.size(); ++i) {
            asn1 e = op[i];
            if (e.is_context(3) && e.constructed()) {
                for (auto u : e) {
                    r.referrals.push_back(string(ldap_text(u)));
                }
            }
        }
        return true;
    }

    // A SearchResultEntry into an entry
    inline bool ldap_read_entry(const asn1& op, entry& out) {
        asn1 name = op[0], attrs = op[1];
        if (!name.is(asn1::type::octet_string) || !attrs.is(asn1::type::sequence)) {
            return false;
        }
        out.dn = string(ldap_text(name));
        for (auto a : attrs) {
            if (!a.is(asn1::type::sequence) || !a[0].is(asn1::type::octet_string) || !a[1].is(asn1::type::set)) {
                return false;
            }
            attribute at;
            at.name = string(ldap_text(a[0]));
            for (auto v : a[1]) {
                at.values.push_back(string(ldap_text(v)));
            }
            out.attributes.push_back(std::move(at));
        }
        return true;
    }

    inline asn1 ldap_attributes(const vector<attribute>& attrs) {
        vector<asn1> list;
        for (auto& a : attrs) {
            vector<asn1> vals;
            for (auto& v : a.values) {
                vals.push_back(ldap_str(v.view()));
            }
            list.push_back(asn1::sequence({ldap_str(a.name.view()), ldap_cons(asn1::tag_class::universal, 17, vals)}));
        }
        return asn1::sequence(list);
    }
}
