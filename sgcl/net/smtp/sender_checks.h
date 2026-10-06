//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "envelope.h"
#include "../detail/mail_auth.h"
#include "../dkim.h"
#include "../dmarc.h"
#include "../dns.h"
#include "../error.h"
#include "../spf.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../encoding/email.h"

#include <stdexcept>
#include <string>
#include <string_view>

// What a receiver of mail checks of a message's origin — SPF of its
// envelope, DKIM of its signatures, DMARC of its From — and how it tells
// the readers downstream: the Authentication-Results field (RFC 8601)
namespace sgcl::net::smtp {
    struct authentication_results;

    namespace detail {
        expected<authentication_results, io::error> results_parse(std::string_view s);
    }

    // An Authentication-Results field's value (RFC 8601 §2.2): the
    // receiver that checked (authserv-id) and a result per method, each
    // with its reason and properties ("smtp.mailfrom", "header.d"). A
    // plain value; parse reads one, to_string writes it on one line.
    struct authentication_results {
        // One method's result: "dkim=pass reason=\"ok\" header.d=example.com"
        struct method_result {
            string method;                            // "spf", "dkim", "dmarc", "auth", ...
            string result;                            // "pass", "fail", "none", ...
            string reason;                            // reason=, "" for none
            vector<pair<string, string>> properties;  // ("header.d", "example.com"), in their order

            friend bool operator==(const method_result&, const method_result&) noexcept = default;
        };

        string authserv_id;
        vector<method_result> results;   // empty: the value says "none"

        authentication_results() = default;

        // The value of a field (what follows "Authentication-Results:"),
        // comments and folding passed over; a version after the id must be
        // 1. errc::malformed_message for anything else
        static expected<authentication_results, io::error> parse(const string& value) {
            return detail::results_parse(value.view());
        }

        // The value a literal spells: parse's value or its
        // bad_expected_access<io::error> (DESIGN 234)
        explicit authentication_results(const string& value)
        : authentication_results(parse(value).value()) {
        }

        // "mx.example.org; spf=pass smtp.mailfrom=example.com; dkim=pass
        // header.d=example.com", on one line; "mx.example.org; none" with
        // no results. A value that is not a token is quoted.
        string to_string() const {
            auto token = [](std::string_view v) {
                if (v.empty()) {
                    return false;
                }
                for (char c : v) {
                    if (unsigned(c) <= 0x20 || unsigned(c) >= 0x7F || std::string_view("()<>,;:\\\"/[]?=").find(c) != std::string_view::npos) {
                        return false;
                    }
                }
                return true;
            };
            auto put = [&](std::string& out, std::string_view v, bool address) {
                // a property's value may be an address or a domain, dots and "@" included
                std::string_view check = v;
                std::string plain;
                if (address) {
                    for (char c : v) {
                        plain += c == '@' ? 'a' : c;
                    }
                    check = plain;
                }
                if (token(check)) {
                    out += v;
                    return;
                }
                out += '"';
                for (char c : v) {
                    if (c == '"' || c == '\\') {
                        out += '\\';
                    }
                    out += c;
                }
                out += '"';
            };
            std::string out;
            put(out, authserv_id.view(), true);
            if (results.empty()) {
                out += "; none";
                return string(out);
            }
            for (auto& m : results) {
                out += "; ";
                out += m.method.view();
                out += '=';
                out += m.result.view();
                if (!m.reason.empty()) {
                    out += " reason=";
                    put(out, m.reason.view(), false);
                }
                for (auto& p : m.properties) {
                    out += ' ';
                    out += p.first.view();
                    out += '=';
                    put(out, p.second.view(), true);
                }
            }
            return string(out);
        }

        friend bool operator==(const authentication_results&, const authentication_results&) noexcept = default;
    };

    namespace detail {
        // authentication_results::parse on the value's bytes
        inline expected<authentication_results, io::error> results_parse(std::string_view s) {
                size_t i = 0;
                auto bad = [] { return unexpected(net::detail::net_error(net::errc::malformed_message, "authentication results", string("malformed Authentication-Results"))); };
                auto cfws = [&]() -> bool {
                    for (;;) {
                        while (i < s.size() && (net::detail::mail_wsp(s[i]) || s[i] == '\r' || s[i] == '\n')) {
                            ++i;
                        }
                        if (i >= s.size() || s[i] != '(') {
                            return true;
                        }
                        int depth = 0;
                        while (i < s.size()) {
                            char c = s[i++];
                            if (c == '\\') {
                                ++i;
                            } else if (c == '(') {
                                ++depth;
                            } else if (c == ')' && --depth == 0) {
                                break;
                            }
                        }
                        if (depth != 0) {
                            return false;
                        }
                    }
                };
                auto special = [](char c) { return c == ';' || c == '=' || c == '(' || c == ')' || c == '"' || net::detail::mail_wsp(c) || c == '\r' || c == '\n'; };
                // a value: a quoted string or a run of characters up to a special
                auto value_of = [&](std::string& out, bool stop_at_dot) -> bool {
                    out.clear();
                    if (i < s.size() && s[i] == '"') {
                        ++i;
                        while (i < s.size() && s[i] != '"') {
                            if (s[i] == '\\' && i + 1 < s.size()) {
                                ++i;
                            }
                            out += s[i++];
                        }
                        if (i >= s.size()) {
                            return false;
                        }
                        ++i;
                        return true;
                    }
                    while (i < s.size() && !special(s[i]) && !(stop_at_dot && (s[i] == '.' || s[i] == '/'))) {
                        if (uint8_t(s[i]) <= 0x20 || uint8_t(s[i]) >= 0x7F) {
                            return false;   // a token is visible ASCII; anything else is quoted
                        }
                        out += s[i++];
                    }
                    return !out.empty();
                };
                // a method's, a ptype's, a property's name: letters, digits, "-" and "_"
                auto keyword = [](const std::string& w) {
                    for (char c : w) {
                        if (!((c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || c == '-' || c == '_')) {
                            return false;
                        }
                    }
                    return !w.empty();
                };
                authentication_results r;
                std::string word;
                if (!cfws() || !value_of(word, false)) {
                    return bad();
                }
                r.authserv_id = string(word);
                if (!cfws()) {
                    return bad();
                }
                if (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                    value_of(word, false);
                    if (word != "1") {
                        return bad();
                    }
                    if (!cfws()) {
                        return bad();
                    }
                }
                bool none = false;
                while (i < s.size()) {
                    if (s[i] != ';') {
                        return bad();
                    }
                    ++i;
                    if (!cfws()) {
                        return bad();
                    }
                    if (i >= s.size()) {
                        break;   // a ";" at the end
                    }
                    std::string method;
                    if (!value_of(method, true) || !keyword(method)) {
                        return bad();
                    }
                    if (!cfws()) {
                        return bad();
                    }
                    if (i < s.size() && s[i] == '/') {   // a method's version
                        ++i;
                        cfws();
                        value_of(word, true);
                        cfws();
                    }
                    if (net::detail::mail_iequal(method, "none") && (i >= s.size() || s[i] == ';')) {
                        none = true;
                        continue;
                    }
                    if (i >= s.size() || s[i] != '=') {
                        return bad();
                    }
                    ++i;
                    authentication_results::method_result m;
                    m.method = string(net::detail::mail_lowered(method));
                    if (!cfws() || i >= s.size() || s[i] == '"' || !value_of(word, false) || !keyword(word)) {
                        return bad();   // a result is a keyword (RFC 8601 §2.2)
                    }
                    m.result = string(net::detail::mail_lowered(word));
                    for (;;) {
                        if (!cfws()) {
                            return bad();
                        }
                        if (i >= s.size() || s[i] == ';') {
                            break;
                        }
                        std::string ptype;
                        if (!value_of(ptype, true) || !keyword(ptype)) {
                            return bad();
                        }
                        cfws();
                        if (net::detail::mail_iequal(ptype, "reason") && i < s.size() && s[i] == '=') {
                            ++i;
                            cfws();
                            if (!value_of(word, false)) {
                                return bad();
                            }
                            m.reason = string(word);
                            continue;
                        }
                        if (i >= s.size() || s[i] != '.') {
                            return bad();
                        }
                        ++i;
                        cfws();
                        std::string property;
                        if (!value_of(property, true) || !keyword(property)) {
                            return bad();
                        }
                        cfws();
                        if (i >= s.size() || s[i] != '=') {
                            return bad();
                        }
                        ++i;
                        cfws();
                        if (!value_of(word, false)) {
                            return bad();
                        }
                        m.properties.push_back({string(net::detail::mail_lowered(ptype) + "." + net::detail::mail_lowered(property)), string(word)});
                    }
                    r.results.push_back(std::move(m));
                }
                if (none && !r.results.empty()) {
                    return bad();
                }
                return r;
        }
    }

    // What the checks of one message found: SPF of its envelope, DKIM of
    // each signature, DMARC of its From; nullopt for a check not made
    struct sender_verdict {
        optional<spf::result> spf;
        optional<vector<dkim::result>> dkim;
        optional<dmarc::result> dmarc;

        // The results as an Authentication-Results field of the receiver
        // authserv_id: spf with smtp.mailfrom, dkim with header.d, header.i,
        // header.s, header.a and header.b (b='s first eight characters),
        // dmarc with header.from; a reason for each result that is not pass
        authentication_results results(const string& authserv_id) const {
            authentication_results out;
            out.authserv_id = authserv_id;
            using method_result = authentication_results::method_result;
            if (spf) {
                method_result m{string("spf"), spf::to_string(spf->status), string(), {}};
                if (spf->status != spf::status::pass && spf->status != spf::status::none) {
                    m.reason = spf->reason.empty() ? spf->mechanism : spf->reason;
                }
                if (!spf->domain.empty()) {
                    m.properties.push_back({string("smtp.mailfrom"), spf->domain});
                }
                out.results.push_back(std::move(m));
            }
            if (dkim) {
                if (dkim->empty()) {
                    out.results.push_back(method_result{string("dkim"), string("none"), string(), {}});
                }
                for (auto& d : *dkim) {
                    method_result m{string("dkim"), dkim::to_string(d.status), d.reason, {}};
                    if (!d.domain.empty()) {
                        m.properties.push_back({string("header.d"), d.domain});
                    }
                    if (!d.identity.empty()) {
                        m.properties.push_back({string("header.i"), d.identity});
                    }
                    if (!d.selector.empty()) {
                        m.properties.push_back({string("header.s"), d.selector});
                    }
                    if (!d.algorithm.empty()) {
                        m.properties.push_back({string("header.a"), d.algorithm});
                    }
                    if (!d.signature.empty()) {
                        m.properties.push_back({string("header.b"), string(d.signature.view().substr(0, 8))});
                    }
                    out.results.push_back(std::move(m));
                }
            }
            if (dmarc) {
                method_result m{string("dmarc"), dmarc::to_string(dmarc->status), string(), {}};
                if (dmarc->status != dmarc::status::pass && dmarc->status != dmarc::status::none) {
                    m.reason = dmarc->reason;
                }
                if (dmarc->record) {
                    m.properties.push_back({string("policy.dmarc"), dmarc::to_string(dmarc->disposition)});
                }
                if (!dmarc->domain.empty()) {
                    m.properties.push_back({string("header.from"), dmarc->domain});
                }
                out.results.push_back(std::move(m));
            }
            return out;
        }
    };

    // Which checks a receiver makes, how it asks DNS, and what it does
    // with what they find: the A-R field added (fields of the same
    // authserv-id that came with the message taken out, RFC 8601 §5), a
    // message whose DMARC fails under p=reject refused (550 5.7.1)
    struct sender_checks {
        bool spf = true;
        bool dkim = true;
        bool dmarc = true;
        net::dns::options dns;
        string authserv_id;         // the receiver's name in the field; empty: the server's hostname
        bool add_header = true;
        bool reject = false;
    };

    namespace detail {
        // From's domain: the one domain of its addresses; "" for no From,
        // more than one From, or addresses of more than one domain
        inline std::string from_domain(const vector<net::detail::MailHeaderField>& fields) {
            const net::detail::MailHeaderField* from = nullptr;
            for (auto& f : fields) {
                if (net::detail::mail_iequal(f.name, "from")) {
                    if (from) {
                        return std::string();
                    }
                    from = &f;
                }
            }
            if (!from) {
                return std::string();
            }
            std::string value;
            for (char c : from->raw.substr(from->raw.find(':') + 1)) {
                if (c != '\r' && c != '\n') {
                    value += c;
                }
            }
            auto list = encoding::email::address::parse_list(string(net::detail::mail_trim(value)));
            if (!list || list->empty()) {
                return std::string();
            }
            std::string domain;
            for (auto& a : *list) {
                std::string d = net::detail::mail_domain(a.domain().view());
                if (d.empty() || (!domain.empty() && d != domain)) {
                    return std::string();
                }
                domain = d;
            }
            return domain;
        }

        inline async::task<smtp::sender_verdict> check_sender(string message, smtp::envelope e, smtp::sender_checks o) {
            smtp::sender_verdict a;
            if (o.spf) {
                spf::options so;
                so.dns = o.dns;
                so.receiver = o.authserv_id;
                a.spf = co_await spf::async_check(e.client.address(), e.from, e.helo, so);
            }
            if (o.dkim || o.dmarc) {
                dkim::verify_options vo;
                vo.dns = o.dns;
                a.dkim = co_await dkim::async_verify(message, vo);
            }
            if (o.dmarc) {
                net::dkim::detail::DkimText text(message.view());
                auto split = net::detail::mail_split(text.view);
                dmarc::options mo;
                mo.dns = o.dns;
                std::string domain = from_domain(split.fields);
                spf::result none;
                a.dmarc = co_await dmarc::async_check(string(domain), a.spf ? *a.spf : none, *a.dkim, mo);
            }
            if (!o.dkim) {
                a.dkim = nullopt;
            }
            co_return a;
        }

        // The message with the field put first, the fields of the same
        // authserv-id taken out
        inline std::string with_results(std::string_view message, const authentication_results& r) {
            std::string out = "Authentication-Results: ";
            std::string value(r.to_string().view());
            // folded at a space outside quotes where a line would pass 78 columns
            size_t col = out.size();
            size_t at = 0;
            while (at < value.size()) {
                size_t end = at;
                bool quoted = false;
                while (end < value.size() && (quoted || value[end] != ' ')) {
                    if (value[end] == '\\' && quoted) {
                        ++end;
                    } else if (value[end] == '"') {
                        quoted = !quoted;
                    }
                    ++end;
                }
                std::string_view word = std::string_view(value).substr(at, std::min(end, value.size()) - at);
                if (at > 0) {
                    if (col + 1 + word.size() > 78) {
                        out += "\r\n\t";
                        col = 8;
                    } else {
                        out += ' ';
                        ++col;
                    }
                }
                out += word;
                col += word.size();
                at = end + 1;
            }
            out += "\r\n";
            net::dkim::detail::DkimText text(message);
            auto split = net::detail::mail_split(text.view);
            size_t kept = 0;
            for (auto& f : split.fields) {
                if (net::detail::mail_iequal(f.name, "Authentication-Results")) {
                    auto old = authentication_results::parse(string(f.raw.substr(f.raw.find(':') + 1)));
                    if (old && net::detail::mail_iequal(old->authserv_id.view(), r.authserv_id.view())) {
                        // a forged field of this receiver's: out
                        out.append(text.view.data() + kept, size_t(f.raw.data() - text.view.data()) - kept);
                        kept = size_t(f.raw.data() - text.view.data()) + f.raw.size();
                    }
                }
            }
            out.append(text.view.substr(kept));
            return out;
        }
    }

    // The checks of a message as a receiver makes them, on the envelope
    // its server got (the client's address, MAIL FROM, HELO) and the
    // message's bytes: SPF, DKIM, DMARC as the options say; nullopt for a
    // check not made. A check never fails: what went wrong is its status
    // `check_sender(...)` on this thread, `co_await async_check_sender(...)` in a task
    inline sender_verdict check_sender(const string& message, const envelope& e, const sender_checks& o = {}) {
        return detail::check_sender(message, e, o).wait();
    }

    inline async::task<sender_verdict> async_check_sender(string message, envelope e, sender_checks o = {}) noexcept {
        return detail::check_sender(std::move(message), std::move(e), std::move(o));
    }
}
