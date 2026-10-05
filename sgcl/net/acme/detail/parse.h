//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../types.h"
#include "../../../encoding/json.h"
#include "../../../time/layout.h"

#include <string_view>

// The objects of RFC 8555 §7.1 read from the JSON of a response: what the
// RFC requires of each is required (a field of the wrong kind or a status of
// another set is errc::malformed_response), what it leaves optional may be
// missing, and a field no version of the RFC has is passed over.
namespace sgcl::net::acme::detail {
    using encoding::json;

    inline io::error malformed(const char* what, const char* op = "acme") noexcept {
        return acme_error(errc::malformed_response, op, string(what));
    }

    // A string field: the value, or nullopt for one absent; false for one
    // of another kind
    inline bool text_field(const json& o, const char* key, string& out, bool required) noexcept {
        const json& v = o[key];
        if (v.is_null()) {
            return !required;
        }
        auto s = v.as_string();
        if (!s) {
            return false;
        }
        out = *s;
        return true;
    }

    inline bool time_field(const json& o, const char* key, optional<time::datetime>& out) noexcept {
        const json& v = o[key];
        if (v.is_null()) {
            return true;
        }
        auto s = v.as_string();
        if (!s) {
            return false;
        }
        auto t = time::datetime::parse(*s, time::rfc3339);
        if (!t) {
            return false;
        }
        out = *t;
        return true;
    }

    inline bool strings_field(const json& o, const char* key, vector<string>& out, bool required) noexcept {
        const json& v = o[key];
        if (v.is_null()) {
            return !required;
        }
        if (!v.is_array()) {
            return false;
        }
        for (const json& e : v.elements()) {
            auto s = e.as_string();
            if (!s) {
                return false;
            }
            out.push_back(*s);
        }
        return true;
    }

    inline optional<status> status_of(std::string_view s) noexcept {
        static constexpr std::string_view names[] = {"pending", "ready", "processing", "valid", "invalid", "revoked", "deactivated", "expired"};
        for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
            if (names[i] == s) {
                return status(i);
            }
        }
        return nullopt;
    }

    inline bool status_field(const json& o, status& out) noexcept {
        auto s = o["status"].as_string();
        if (!s) {
            return false;
        }
        auto st = status_of(s->view());
        if (!st) {
            return false;
        }
        out = *st;
        return true;
    }

    inline bool identifier_of(const json& v, identifier& out) noexcept {
        return v.is_object() && text_field(v, "type", out.type, true) && text_field(v, "value", out.value, true);
    }

    // A problem document (RFC 7807): a type ("about:blank" when absent,
    // §4.2 of RFC 7807), the detail, the status, the instance, an
    // identifier and subproblems (RFC 8555 §6.7.1)
    inline expected<problem, io::error> parse_problem(const json& v) noexcept {
        if (!v.is_object()) {
            return unexpected(malformed("a problem document that is not an object"));
        }
        problem p;
        if (!text_field(v, "type", p.type, false) || !text_field(v, "detail", p.detail, false) || !text_field(v, "instance", p.instance, false)) {
            return unexpected(malformed("a problem document with a field of the wrong kind"));
        }
        if (p.type.empty()) {
            p.type = string("about:blank");
        }
        if (const json& s = v["status"]; !s.is_null()) {
            auto n = s.as_int();
            if (!n || *n < 0 || *n > 999) {
                return unexpected(malformed("a problem document whose status is not an HTTP status"));
            }
            p.status = int(*n);
        }
        if (const json& id = v["identifier"]; !id.is_null()) {
            identifier i;
            if (!identifier_of(id, i)) {
                return unexpected(malformed("a problem document with a malformed identifier"));
            }
            p.identifier = i;
        }
        if (const json& subs = v["subproblems"]; !subs.is_null()) {
            if (!subs.is_array()) {
                return unexpected(malformed("subproblems that are not an array"));
            }
            for (const json& s : subs.elements()) {
                subproblem sp;
                if (!s.is_object() || !text_field(s, "type", sp.type, false) || !text_field(s, "detail", sp.detail, false)) {
                    return unexpected(malformed("a malformed subproblem"));
                }
                if (const json& id = s["identifier"]; !id.is_null() && !identifier_of(id, sp.identifier)) {
                    return unexpected(malformed("a subproblem with a malformed identifier"));
                }
                p.subproblems.push_back(std::move(sp));
            }
        }
        return p;
    }

    inline bool problem_field(const json& o, const char* key, optional<problem>& out) noexcept {
        const json& v = o[key];
        if (v.is_null()) {
            return true;
        }
        auto p = parse_problem(v);
        if (!p) {
            return false;
        }
        out = std::move(*p);
        return true;
    }

    // The directory (§7.1.1): newNonce, newAccount and newOrder required
    inline expected<directory, io::error> parse_directory(const json& v) noexcept {
        if (!v.is_object()) {
            return unexpected(malformed("a directory that is not an object"));
        }
        directory d;
        if (!text_field(v, "newNonce", d.new_nonce, true) || !text_field(v, "newAccount", d.new_account, true) || !text_field(v, "newOrder", d.new_order, true)
            || !text_field(v, "newAuthz", d.new_authz, false) || !text_field(v, "revokeCert", d.revoke_cert, false) || !text_field(v, "keyChange", d.key_change, false)
            || !text_field(v, "renewalInfo", d.renewal_info, false)) {
            return unexpected(malformed("a directory without newNonce, newAccount or newOrder, or with a resource that is not a string"));
        }
        if (const json& m = v["meta"]; !m.is_null()) {
            if (!m.is_object() || !text_field(m, "termsOfService", d.terms_of_service, false) || !text_field(m, "website", d.website, false)
                || !strings_field(m, "caaIdentities", d.caa_identities, false)) {
                return unexpected(malformed("a directory's meta with a field of the wrong kind"));
            }
            if (const json& e = m["externalAccountRequired"]; !e.is_null()) {
                auto b = e.as_bool();
                if (!b) {
                    return unexpected(malformed("a directory's externalAccountRequired that is not a boolean"));
                }
                d.external_account_required = *b;
            }
            if (const json& p = m["profiles"]; !p.is_null()) {
                if (!p.is_object()) {
                    return unexpected(malformed("a directory's profiles that are not an object"));
                }
                for (const auto& member : p.members()) {
                    d.profiles[member.key] = member.value.as_string(string());
                }
            }
        }
        return d;
    }

    // An account (§7.1.2): its status required; contact and
    // termsOfServiceAgreed when given
    inline expected<account, io::error> parse_account(const json& v, const string& url) noexcept {
        account a;
        a.url = url;
        if (!v.is_object() || !status_field(v, a.status) || !strings_field(v, "contact", a.contact, false) || !text_field(v, "orders", a.orders, false)) {
            return unexpected(malformed("an account without its status, or with a field of the wrong kind"));
        }
        if (const json& t = v["termsOfServiceAgreed"]; !t.is_null()) {
            auto b = t.as_bool();
            if (!b) {
                return unexpected(malformed("an account's termsOfServiceAgreed that is not a boolean"));
            }
            a.terms_agreed = *b;
        }
        return a;
    }

    // A challenge (§7.1.5): type, url and status required; a token for
    // the types of §8 that have one
    inline expected<challenge, io::error> parse_challenge(const json& v) noexcept {
        challenge c;
        if (!v.is_object() || !text_field(v, "type", c.type, true) || !text_field(v, "url", c.url, true) || !status_field(v, c.status)
            || !text_field(v, "token", c.token, false) || !time_field(v, "validated", c.validated) || !problem_field(v, "error", c.error)) {
            return unexpected(malformed("a challenge without its type, url or status, or with a field of the wrong kind"));
        }
        if ((c.type == "http-01" || c.type == "dns-01" || c.type == "tls-alpn-01") && c.token.empty()) {
            return unexpected(malformed("a challenge of RFC 8555 §8 without a token"));
        }
        return c;
    }

    // An authorization (§7.1.4): identifier, status and challenges required
    inline expected<authorization, io::error> parse_authorization(const json& v, const string& url) noexcept {
        authorization a;
        a.url = url;
        if (!v.is_object() || !identifier_of(v["identifier"], a.identifier) || !status_field(v, a.status) || !time_field(v, "expires", a.expires)) {
            return unexpected(malformed("an authorization without its identifier or status, or with a field of the wrong kind"));
        }
        const json& list = v["challenges"];
        if (!list.is_array()) {
            return unexpected(malformed("an authorization without challenges"));
        }
        for (const json& c : list.elements()) {
            auto ch = parse_challenge(c);
            if (!ch) {
                return unexpected(ch.error());
            }
            a.challenges.push_back(std::move(*ch));
        }
        if (const json& w = v["wildcard"]; !w.is_null()) {
            auto b = w.as_bool();
            if (!b) {
                return unexpected(malformed("an authorization's wildcard that is not a boolean"));
            }
            a.wildcard = *b;
        }
        return a;
    }

    // An order (§7.1.3): status, identifiers, authorizations and finalize
    // required
    inline expected<order, io::error> parse_order(const json& v, const string& url) noexcept {
        order o;
        o.url = url;
        if (!v.is_object() || !status_field(v, o.status) || !time_field(v, "expires", o.expires) || !time_field(v, "notBefore", o.not_before)
            || !time_field(v, "notAfter", o.not_after) || !problem_field(v, "error", o.error) || !strings_field(v, "authorizations", o.authorizations, true)
            || !text_field(v, "finalize", o.finalize, true) || !text_field(v, "certificate", o.certificate, false) || !text_field(v, "replaces", o.replaces, false)
            || !text_field(v, "profile", o.profile, false)) {
            return unexpected(malformed("an order without its status, authorizations or finalize, or with a field of the wrong kind"));
        }
        const json& ids = v["identifiers"];
        if (!ids.is_array()) {
            return unexpected(malformed("an order without identifiers"));
        }
        for (const json& i : ids.elements()) {
            identifier id;
            if (!identifier_of(i, id)) {
                return unexpected(malformed("an order with a malformed identifier"));
            }
            o.identifiers.push_back(std::move(id));
        }
        return o;
    }

    // The renewal information of a certificate (RFC 9773 §4.2): the
    // suggested window, its start not after its end
    inline expected<renewal_info, io::error> parse_renewal_info(const json& v) noexcept {
        if (!v.is_object()) {
            return unexpected(malformed("renewal information that is not an object"));
        }
        const json& w = v["suggestedWindow"];
        optional<time::datetime> start, end;
        if (!w.is_object() || !time_field(w, "start", start) || !time_field(w, "end", end) || !start || !end || end->unix_nano() < start->unix_nano()) {
            return unexpected(malformed("renewal information without a suggested window whose start is not after its end"));
        }
        renewal_info r;
        r.start = *start;
        r.end = *end;
        if (!text_field(v, "explanationURL", r.explanation_url, false)) {
            return unexpected(malformed("renewal information whose explanationURL is not a string"));
        }
        return r;
    }

    // A text of a response parsed as JSON, the error the parse's
    inline expected<json, io::error> parse_json(const string& text) noexcept {
        auto v = json::parse(text);
        if (!v) {
            return unexpected(acme_error(errc::malformed_response, "acme", string::concat("a response that is not JSON: ", v.error().message())));
        }
        return std::move(*v);
    }
}
