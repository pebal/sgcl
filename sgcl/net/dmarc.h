//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/mail_auth.h"
#include "dkim.h"
#include "dns.h"
#include "error.h"
#include "http/public_suffix.h"
#include "ip.h"
#include "spf.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/string.h"
#include "../core/vector.h"
#include "../crypto/random.h"
#include "../encoding/xml.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

// Domain-based Message Authentication, Reporting and Conformance (RFC
// 7489): what the domain of a message's From asks a receiver to do with
// mail that neither SPF nor DKIM ties to it — its record found at
// "_dmarc.<domain>" or at its organizational domain's (the registrable
// domain of the Public Suffix List, net::http::registrable_domain), the
// identifiers aligned relaxed or strict, the policy for the domain, its
// subdomains (sp=) and its names that do not exist (np=, RFC 9091), the
// sample of pct=; aggregate reports read
namespace sgcl::net::dmarc {
    // What a record asks for mail that fails
    enum class policy : uint8_t {
        none,         // nothing: monitoring
        quarantine,   // treated as suspicious (a spam folder)
        reject        // refused
    };

    // A check's result as RFC 8601 names DMARC's
    enum class status : uint8_t {
        none,        // the domain has no record
        pass,        // SPF or DKIM passed for an aligned domain
        fail,        // neither did
        temperror,   // DNS failed for now
        permerror    // no domain to check (no From, or more than one of different domains)
    };

    inline string to_string(policy p) noexcept {
        switch (p) {
            case policy::none: return string("none");
            case policy::quarantine: return string("quarantine");
            case policy::reject: return string("reject");
        }
        return string("none");
    }

    inline string to_string(status s) noexcept {
        switch (s) {
            case status::none: return string("none");
            case status::pass: return string("pass");
            case status::fail: return string("fail");
            case status::temperror: return string("temperror");
            case status::permerror: return string("permerror");
        }
        return string("none");
    }

    namespace detail {
        using namespace sgcl::net::detail;

        inline bool dmarc_policy(std::string_view v, policy& out) noexcept {
            if (mail_iequal(v, "none")) {
                out = policy::none;
            } else if (mail_iequal(v, "quarantine")) {
                out = policy::quarantine;
            } else if (mail_iequal(v, "reject")) {
                out = policy::reject;
            } else {
                return false;
            }
            return true;
        }

        inline vector<string> dmarc_list(std::string_view v, char sep) {
            vector<string> out;
            size_t at = 0;
            while (at <= v.size()) {
                size_t c = v.find(sep, at);
                std::string_view item = mail_trim(v.substr(at, c == std::string_view::npos ? std::string_view::npos : c - at));
                if (!item.empty()) {
                    out.push_back(string(item));
                }
                if (c == std::string_view::npos) {
                    break;
                }
                at = c + 1;
            }
            return out;
        }
    }

    struct record;

    namespace detail {
        expected<record, io::error> dmarc_record_parse(std::string_view text);
    }

    // A DMARC record (RFC 7489 §6.3): the policy, the subdomains' and the
    // non-existent names' when they differ, the alignment modes, the
    // sample, where reports go and how. A plain value of the record's
    // fields; to_string writes it back, the tags at their defaults left out.
    struct record {
        dmarc::policy policy = dmarc::policy::none;   // p=
        optional<dmarc::policy> subdomain_policy;     // sp=: the subdomains' policy; none: policy
        optional<dmarc::policy> nonexistent_policy;   // np= (RFC 9091): the policy of names that do not exist; none: subdomain_policy, then policy
        bool strict_dkim = false;                     // adkim=s: d= equal to From's domain, not only of its organization
        bool strict_spf = false;                      // aspf=s: the same for SPF's domain
        int percent = 100;                            // pct=: the percentage of failing mail the policy is applied to
        string failure_options = string("0");         // fo=: when failure reports are asked for
        vector<string> aggregate_reports;             // rua=: where aggregate reports go ("mailto:dmarc@example.com")
        vector<string> failure_reports;               // ruf=: where failure reports go
        duration report_interval = 24 * hour;         // ri=: the time between aggregate reports, in whole seconds
        string report_format = string("afrf");        // rf=: the format of failure reports

        record() = default;

        // The text of a TXT record ("v=DMARC1; p=reject; rua=mailto:d@example.com").
        // A record whose p= is missing or unknown is p=none when it names
        // rua (§6.6.3), errc::malformed_dmarc otherwise, as is one that does
        // not start with v=DMARC1; a bad value of another tag leaves its
        // default (§6.3); unknown tags are ignored
        static expected<record, io::error> parse(const string& text) {
            return detail::dmarc_record_parse(text.view());
        }

        // The record a literal spells: parse's value or its
        // bad_expected_access<io::error> (DESIGN 234)
        explicit record(const string& text)
        : record(parse(text).value()) {
        }

        // "v=DMARC1; p=reject; sp=none; adkim=s; pct=50; rua=mailto:d@example.com"
        string to_string() const {
            std::string out = "v=DMARC1; p=";
            out += dmarc::to_string(policy).view();
            if (subdomain_policy) {
                out += "; sp=";
                out += dmarc::to_string(*subdomain_policy).view();
            }
            if (nonexistent_policy) {
                out += "; np=";
                out += dmarc::to_string(*nonexistent_policy).view();
            }
            if (strict_dkim) {
                out += "; adkim=s";
            }
            if (strict_spf) {
                out += "; aspf=s";
            }
            if (percent != 100) {
                out += "; pct=" + std::to_string(percent);
            }
            if (failure_options.view() != "0") {
                out += "; fo=";
                out += failure_options.view();
            }
            auto list = [&](const char* name, const vector<string>& v) {
                if (v.empty()) {
                    return;
                }
                out += "; ";
                out += name;
                out += '=';
                for (size_t i = 0; i < v.size(); ++i) {
                    if (i) {
                        out += ',';
                    }
                    out += v[i].view();
                }
            };
            list("rua", aggregate_reports);
            list("ruf", failure_reports);
            int64_t ri = report_interval.milliseconds() / 1000;
            if (ri != 86400) {
                out += "; ri=" + std::to_string(ri);
            }
            if (report_format.view() != "afrf") {
                out += "; rf=";
                out += report_format.view();
            }
            return string(out);
        }

        friend bool operator==(const record&, const record&) noexcept = default;
    };

    namespace detail {
        // record::parse on the text's bytes
        inline expected<record, io::error> dmarc_record_parse(std::string_view text) {
            vector<net::detail::MailTag> tags;
            auto bad = [&](const char* why) { return unexpected(net::detail::net_error(net::errc::malformed_dmarc, "dmarc", string(why))); };
            if (!net::detail::mail_tag_list(text, tags, true) || tags.empty() || tags[0].name != "v" || tags[0].value != "DMARC1") {
                return bad("not a DMARC1 record");
            }
            record r;
            bool has_p = false;
            for (auto& t : tags) {
                std::string_view n = t.name, v = t.value;
                if (n == "p") {
                    has_p = detail::dmarc_policy(v, r.policy);
                } else if (n == "sp") {
                    dmarc::policy x;
                    if (detail::dmarc_policy(v, x)) {
                        r.subdomain_policy = x;
                    }
                } else if (n == "np") {
                    dmarc::policy x;
                    if (detail::dmarc_policy(v, x)) {
                        r.nonexistent_policy = x;
                    }
                } else if (n == "adkim" || n == "aspf") {
                    bool& mode = n == "adkim" ? r.strict_dkim : r.strict_spf;
                    if (net::detail::mail_iequal(v, "s")) {
                        mode = true;
                    } else if (net::detail::mail_iequal(v, "r")) {
                        mode = false;
                    }
                } else if (n == "pct") {
                    int x = 0;
                    bool ok = !v.empty() && v.size() <= 3;
                    for (char c : ok ? v : std::string_view()) {
                        ok &= c >= '0' && c <= '9';
                        x = x * 10 + (c - '0');
                    }
                    if (ok && x <= 100) {
                        r.percent = x;
                    }
                } else if (n == "fo") {
                    r.failure_options = string(v);
                } else if (n == "rf") {
                    r.report_format = string(v);
                } else if (n == "ri") {
                    uint64_t x = 0;
                    bool ok = !v.empty() && v.size() <= 10;
                    for (char c : ok ? v : std::string_view()) {
                        ok &= c >= '0' && c <= '9';
                        x = x * 10 + uint64_t(c - '0');
                    }
                    if (ok && x <= UINT32_MAX) {
                        r.report_interval = std::chrono::seconds(int64_t(x));
                    }
                } else if (n == "rua") {
                    r.aggregate_reports = detail::dmarc_list(v, ',');
                } else if (n == "ruf") {
                    r.failure_reports = detail::dmarc_list(v, ',');
                }
            }
            if (!has_p) {
                if (r.aggregate_reports.empty()) {
                    return bad("no valid p= tag");
                }
                r.policy = dmarc::policy::none;
            }
            return r;
        }
    }

    // A check's outcome: the status; From's domain and where its record
    // was found (the domain, or its organizational domain); the record;
    // which of DKIM and SPF passed aligned; the policy the record sets for
    // this domain (p, sp or np), and what to do (the policy, one step
    // milder for a failing message outside pct's sample; none for a pass)
    struct result {
        dmarc::status status = dmarc::status::none;
        string domain;
        string record_domain;
        optional<dmarc::record> record;
        bool dkim_aligned = false;
        bool spf_aligned = false;
        dmarc::policy policy = dmarc::policy::none;
        dmarc::policy disposition = dmarc::policy::none;
        string reason;
    };

    // The resolver of the records (the module's own; servers empty:
    // /etc/resolv.conf's)
    struct options {
        net::dns::options dns;
    };

    namespace detail {
        // The records of a name that start with v=DMARC1: 0, 1 or more
        enum class DmarcFound : uint8_t { none, one, many, error };

        inline async::task<DmarcFound> dmarc_fetch(string name, net::dns::options o, string* text) {
            auto txt = co_await net::dns::async_lookup_txt(string::concat(name, "."), o);
            if (!txt) {
                auto c = txt.error().code();
                co_return c == net::errc::host_not_found || c == net::errc::no_data ? DmarcFound::none : DmarcFound::error;
            }
            size_t n = 0;
            for (auto& t : *txt) {
                std::string_view v = t.view();
                if (v.substr(0, 8) == "v=DMARC1" && (v.size() == 8 || v[8] == ';' || mail_wsp(v[8]))) {
                    ++n;
                    *text = t;
                }
            }
            co_return n == 0 ? DmarcFound::none : n == 1 ? DmarcFound::one : DmarcFound::many;
        }

        struct DmarcDiscovery {
            DmarcFound found = DmarcFound::none;
            string domain;
            string text;
        };

        // §6.6.3: the domain's own record, else its organizational domain's
        inline async::task<DmarcDiscovery> dmarc_discover(std::string domain, net::dns::options o) {
            DmarcDiscovery d;
            d.domain = string(domain);
            d.found = co_await dmarc_fetch(string::concat("_dmarc.", d.domain), o, &d.text);
            if (d.found != DmarcFound::none) {
                co_return d;
            }
            string org = net::http::registrable_domain(string(domain));
            if (org.empty() || org.view() == domain) {
                co_return d;
            }
            d.domain = org;
            d.found = co_await dmarc_fetch(string::concat("_dmarc.", org), o, &d.text);
            co_return d;
        }

        inline bool dmarc_aligned(std::string_view id, std::string_view from, bool strict) {
            std::string a = mail_domain(id), b = mail_domain(from);
            if (a.empty() || b.empty()) {
                return false;
            }
            if (strict || a == b) {
                return a == b;
            }
            string oa = net::http::registrable_domain(string(a));
            string ob = net::http::registrable_domain(string(b));
            return !oa.empty() && oa == ob;
        }

        // RFC 9091 §4.1: a name with no A, AAAA or MX that the DNS says
        // does not exist (NXDOMAIN of all three)
        inline async::task<bool> dmarc_nonexistent(std::string domain, net::dns::options o) {
            auto settings = DnsAccess::settings(o);
            if (!settings) {
                co_return false;
            }
            for (uint16_t t : {dns_type::a, dns_type::aaaa, dns_type::mx}) {
                DnsAnswer a = co_await dns_lookup(string(domain + "."), t, false, *settings, async::stop_token());
                if (a.status != DnsStatus::nxdomain) {
                    co_return false;
                }
            }
            co_return true;
        }

        inline async::task<expected<dmarc::record, io::error>> dmarc_lookup(string domain, options o) {
            std::string d = mail_domain(domain.view());
            if (!mail_valid_domain(d, false)) {
                co_return unexpected(net_error(net::errc::invalid_address, "dmarc", domain));
            }
            DmarcDiscovery found = co_await dmarc_discover(d, o.dns);
            switch (found.found) {
                case DmarcFound::none:
                case DmarcFound::many:
                    co_return unexpected(net_error(net::errc::no_data, "dmarc", string::concat("_dmarc.", domain)));
                case DmarcFound::error:
                    co_return unexpected(net_error(net::errc::server_failure, "dmarc", string::concat("_dmarc.", domain)));
                case DmarcFound::one:
                    break;
            }
            co_return dmarc::record::parse(found.text);
        }

        inline async::task<dmarc::result> dmarc_check(string from_domain, spf::result spf, vector<dkim::result> dkim, options o) {
            dmarc::result r;
            std::string from = mail_domain(from_domain.view());
            r.domain = string(from);
            if (!mail_valid_domain(from, false)) {
                r.status = status::permerror;
                r.reason = string("no valid From domain");
                co_return r;
            }
            DmarcDiscovery found = co_await dmarc_discover(from, o.dns);
            if (found.found == DmarcFound::error) {
                r.status = status::temperror;
                r.reason = string("DNS lookup of the record failed");
                co_return r;
            }
            if (found.found != DmarcFound::one) {
                r.status = status::none;
                r.reason = string(found.found == DmarcFound::many ? "more than one record" : "no record");
                co_return r;
            }
            auto rec = dmarc::record::parse(found.text);
            if (!rec) {
                r.status = status::none;
                r.reason = string("no valid record");
                co_return r;
            }
            r.record_domain = found.domain;
            r.record = *rec;
            r.spf_aligned = spf.status == spf::status::pass && dmarc_aligned(spf.domain.view(), from, rec->strict_spf);
            for (auto& d : dkim) {
                r.dkim_aligned |= d.status == dkim::status::pass && dmarc_aligned(d.domain.view(), from, rec->strict_dkim);
            }
            // the policy for this name (§6.6.3, RFC 9091 §4.1)
            r.policy = rec->policy;
            if (found.domain.view() != from) {
                r.policy = rec->subdomain_policy ? *rec->subdomain_policy : rec->policy;
                if (rec->nonexistent_policy && !r.spf_aligned && !r.dkim_aligned && co_await dmarc_nonexistent(from, o.dns)) {
                    r.policy = *rec->nonexistent_policy;
                }
            }
            if (r.spf_aligned || r.dkim_aligned) {
                r.status = status::pass;
                r.disposition = policy::none;
                co_return r;
            }
            r.status = status::fail;
            r.disposition = r.policy;
            if (rec->percent < 100) {
                uint32_t roll = 0;
                crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(&roll), sizeof roll));
                if (int(roll % 100) >= rec->percent) {   // outside the sample: the next milder (§6.6.4)
                    r.disposition = r.policy == policy::reject ? policy::quarantine : policy::none;
                }
            }
            r.reason = string("neither SPF nor DKIM passed for an aligned domain");
            co_return r;
        }
    }

    // The record that applies to mail from the domain: its own, else its
    // organizational domain's. net::errc::no_data when there is none (or
    // more than one), server_failure when DNS failed, malformed_dmarc
    // for one that does not parse, invalid_address for no domain
    // `lookup(...)` on this thread, `co_await async_lookup(...)` in a task
    inline expected<record, io::error> lookup(const string& domain, const options& o = {}) {
        return detail::dmarc_lookup(domain, o).wait();
    }

    inline async::task<expected<record, io::error>> async_lookup(string domain, options o = {}) noexcept {
        return detail::dmarc_lookup(std::move(domain), std::move(o));
    }

    // DMARC for a message whose From is of from_domain, given what SPF
    // and DKIM found for it: the record discovered, the identifiers
    // aligned, the policy and the disposition. A check never fails: what
    // went wrong is its status and reason
    // `check(...)` on this thread, `co_await async_check(...)` in a task
    inline result check(const string& from_domain, const spf::result& spf, const vector<dkim::result>& dkim, const options& o = {}) {
        return detail::dmarc_check(from_domain, spf, dkim, o).wait();
    }

    inline async::task<result> async_check(string from_domain, spf::result spf, vector<dkim::result> dkim, options o = {}) noexcept {
        return detail::dmarc_check(std::move(from_domain), std::move(spf), std::move(dkim), std::move(o));
    }

    // An aggregate report (RFC 7489 §7.2, Appendix C): who sent it, for
    // which period and domain, the policy it saw published, and a row per
    // source address with what was done and what SPF and DKIM found.
    // Reports come zipped or gzipped in mail; the XML inside is parsed.
    struct report {
        // One result of SPF or DKIM in a row: the domain, DKIM's selector
        // or SPF's scope ("mfrom", "helo"), the result ("pass", "fail", ...)
        struct auth_result {
            string domain;
            string selector;
            string scope;
            string result;

            friend bool operator==(const auth_result&, const auth_result&) noexcept = default;
        };

        // A source and what was done with its messages
        struct row {
            ip_address source;              // source_ip
            uint64_t count = 0;             // the messages
            dmarc::policy disposition = dmarc::policy::none;
            string dkim;                    // policy_evaluated's DKIM: "pass" or "fail"
            string spf;                     // and SPF's
            vector<string> reasons;         // the overrides' types ("forwarded", "local_policy", ...)
            string header_from;             // the identifiers
            string envelope_from;
            string envelope_to;
            vector<auth_result> dkim_results;
            vector<auth_result> spf_results;

            friend bool operator==(const row&, const row&) noexcept = default;
        };

        string org_name;                    // report_metadata
        string email;
        string report_id;
        int64_t begin = 0;                  // the period, seconds since the epoch
        int64_t end = 0;
        vector<string> errors;
        string domain;                      // policy_published
        dmarc::record policy;
        vector<row> rows;

        // The XML of a report; errc::malformed_dmarc when it is not XML or
        // not a feedback document with its metadata and policy
        static expected<report, io::error> parse(const string& xml_text) {
            auto bad = [](const char* why) { return unexpected(net::detail::net_error(net::errc::malformed_dmarc, "dmarc report", string(why))); };
            auto doc = encoding::xml::parse(xml_text);
            if (!doc || doc->local_name().view() != "feedback") {
                return bad("not a feedback document");
            }
            auto text = [](const encoding::xml& e, const char* name) { return string(net::detail::mail_trim(e.child(name).text().view())); };
            auto number = [&](const encoding::xml& e, const char* name, int64_t& out) {
                std::string_view v = net::detail::mail_trim(e.child(name).text().view());
                int64_t n = 0;
                if (v.empty() || v.size() > 18) {
                    return false;
                }
                for (char c : v) {
                    if (c < '0' || c > '9') {
                        return false;
                    }
                    n = n * 10 + (c - '0');
                }
                out = n;
                return true;
            };
            report r;
            auto meta = doc->child("report_metadata");
            auto pub = doc->child("policy_published");
            if (!meta.exists() || !pub.exists()) {
                return bad("no report_metadata or policy_published");
            }
            r.org_name = text(meta, "org_name");
            r.email = text(meta, "email");
            r.report_id = text(meta, "report_id");
            auto range = meta.child("date_range");
            if (!number(range, "begin", r.begin) || !number(range, "end", r.end)) {
                return bad("no date_range");
            }
            for (auto e : meta.children("error")) {
                r.errors.push_back(string(net::detail::mail_trim(e.text().view())));
            }
            r.domain = text(pub, "domain");
            detail::dmarc_policy(text(pub, "p").view(), r.policy.policy);
            dmarc::policy sp;
            if (detail::dmarc_policy(text(pub, "sp").view(), sp)) {
                r.policy.subdomain_policy = sp;
            }
            dmarc::policy np;
            if (detail::dmarc_policy(text(pub, "np").view(), np)) {
                r.policy.nonexistent_policy = np;
            }
            r.policy.strict_dkim = text(pub, "adkim").view() == "s";
            r.policy.strict_spf = text(pub, "aspf").view() == "s";
            int64_t pct = 100;
            if (number(pub, "pct", pct) && pct <= 100) {
                r.policy.percent = int(pct);
            }
            if (string fo = text(pub, "fo"); !fo.empty()) {
                r.policy.failure_options = fo;
            }
            for (auto rec : doc->children("record")) {
                row w;
                auto rw = rec.child("row");
                if (auto ip = ip_address::parse(text(rw, "source_ip"))) {
                    w.source = *ip;
                } else {
                    return bad("a row without a source_ip");
                }
                int64_t count = 0;
                if (!number(rw, "count", count)) {
                    return bad("a row without a count");
                }
                w.count = uint64_t(count);
                auto pe = rw.child("policy_evaluated");
                detail::dmarc_policy(text(pe, "disposition").view(), w.disposition);
                w.dkim = text(pe, "dkim");
                w.spf = text(pe, "spf");
                for (auto reason : pe.children("reason")) {
                    w.reasons.push_back(text(reason, "type"));
                }
                auto ids = rec.child("identifiers");
                w.header_from = text(ids, "header_from");
                w.envelope_from = text(ids, "envelope_from");
                w.envelope_to = text(ids, "envelope_to");
                auto ar = rec.child("auth_results");
                for (auto d : ar.children("dkim")) {
                    w.dkim_results.push_back(auth_result{text(d, "domain"), text(d, "selector"), string(), text(d, "result")});
                }
                for (auto s : ar.children("spf")) {
                    w.spf_results.push_back(auth_result{text(s, "domain"), string(), text(s, "scope"), text(s, "result")});
                }
                r.rows.push_back(std::move(w));
            }
            return r;
        }
    };
}
