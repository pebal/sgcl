//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/mail_auth.h"
#include "dns.h"
#include "error.h"
#include "ip.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/clock.h"
#include "../core/duration.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../time/datetime.h"

#include <cstdint>
#include <string>
#include <string_view>

// The Sender Policy Framework (RFC 7208): whether a domain lets a host
// send its mail, read from the domain's "v=spf1" record — every mechanism
// (all, include, a, mx, ptr, ip4, ip6, exists) and modifier (redirect,
// exp), the macros with their transformers, the limits of §4.6.4 (ten
// terms that ask DNS, two void lookups, ten MX and PTR names)
namespace sgcl::net::spf {
    // A check's result as RFC 7208 §2.6 names it
    enum class status : uint8_t {
        none,        // no record, or no domain to check
        neutral,     // the domain says nothing of the host ("?", or no term matched)
        pass,        // the host may send for the domain
        fail,        // it may not ("-")
        softfail,    // probably not ("~")
        temperror,   // DNS failed for now
        permerror    // the record cannot be read, or past a limit
    };

    // "pass", "softfail", ... as Received-SPF and Authentication-Results write it
    inline string to_string(status s) noexcept {
        switch (s) {
            case status::none: return string("none");
            case status::neutral: return string("neutral");
            case status::pass: return string("pass");
            case status::fail: return string("fail");
            case status::softfail: return string("softfail");
            case status::temperror: return string("temperror");
            case status::permerror: return string("permerror");
        }
        return string("none");
    }

    // A check's outcome: the status, the domain checked (the sender's, or
    // HELO's for the null sender), the term that decided it ("ip4:192.0.2.0/24",
    // "-all"; "default" when none did), exp='s explanation of a fail, why
    // a temperror or permerror came, and the terms that asked DNS
    struct result {
        spf::status status = spf::status::none;
        string domain;
        string mechanism;
        string explanation;
        string reason;
        size_t lookups = 0;
    };

    // How a check asks and what it allows: the resolver (the module's own;
    // servers empty: /etc/resolv.conf's), the time the whole check may take
    // (§4.6.4: at least 20 seconds), the terms that ask DNS and the void
    // lookups past which it is permerror, and the receiving host's name for
    // %{r} of an explanation
    struct options {
        net::dns::options dns;
        duration timeout = 20 * second;
        size_t max_lookups = 10;
        size_t max_void_lookups = 2;
        string receiver;
    };

    namespace detail {
        using namespace sgcl::net::detail;

        struct SpfState {
            ip_address ip;
            std::string sender;
            std::string local;
            std::string sender_domain;
            std::string helo;
            std::string receiver;
            DnsSettings settings;
            size_t lookups = 0;
            size_t voids = 0;
            size_t max_lookups = 10;
            size_t max_voids = 2;
            time_point deadline;
            int64_t now = 0;
            bool timed_out = false;
        };

        struct SpfOutcome {
            spf::status status = spf::status::neutral;
            std::string mechanism;
            std::string explanation;
            std::string reason;
        };

        enum class SpfKind : uint8_t { all, include, a, mx, ptr, ip4, ip6, exists };

        struct SpfTerm {
            char qualifier = '+';
            SpfKind kind = SpfKind::all;
            std::string domain;      // a macro string; empty: the current domain
            ip_address address;      // ip4, ip6
            int cidr4 = 32;
            int cidr6 = 128;
            std::string text;        // the term as written
        };

        struct SpfRecord {
            vector<SpfTerm> terms;
            std::string redirect;
            std::string exp;
            bool has_redirect = false;
            bool has_exp = false;
            bool has_all = false;
        };

        SGCL_INLINE_HOT constexpr bool spf_alpha(char c) noexcept {
            return unsigned((c | 0x20) - 'a') < 26u;
        }

        SGCL_INLINE_HOT constexpr bool spf_digit(char c) noexcept {
            return unsigned(c - '0') < 10u;
        }

        // A macro string's syntax (§7.1), letters c, r, t only where exp allows them
        inline bool spf_macro_valid(std::string_view s, bool exp) noexcept {
            for (size_t i = 0; i < s.size(); ++i) {
                char c = s[i];
                if (c != '%') {
                    if (unsigned(c) < 0x21 || unsigned(c) > 0x7E) {
                        if (!(exp && c == ' ')) {
                            return false;
                        }
                    }
                    continue;
                }
                if (i + 1 >= s.size()) {
                    return false;
                }
                char n = s[++i];
                if (n == '%' || n == '_' || n == '-') {
                    continue;
                }
                if (n != '{' || i + 1 >= s.size()) {
                    return false;
                }
                char l = mail_lower(s[++i]);
                bool letter = l == 's' || l == 'l' || l == 'o' || l == 'd' || l == 'i' || l == 'p' || l == 'h' || l == 'v';
                bool exp_letter = l == 'c' || l == 'r' || l == 't';
                if (!letter && !(exp && exp_letter)) {
                    return false;
                }
                ++i;
                while (i < s.size() && spf_digit(s[i])) {
                    ++i;
                }
                if (i < s.size() && mail_lower(s[i]) == 'r') {
                    ++i;
                }
                while (i < s.size() && std::string_view(".-+,/_=").find(s[i]) != std::string_view::npos) {
                    ++i;
                }
                if (i >= s.size() || s[i] != '}') {
                    return false;
                }
            }
            return true;
        }

        // A domain-spec (§8.1): a macro string ending in a macro or in a
        // top label that is not all digits
        inline bool spf_domain_spec_valid(std::string_view s) noexcept {
            if (s.empty() || !spf_macro_valid(s, false)) {
                return false;
            }
            if (s.back() == '}') {
                return true;
            }
            std::string_view t = s;
            if (t.back() == '.') {
                t.remove_suffix(1);
            }
            size_t dot = t.rfind('.');
            if (dot == std::string_view::npos) {
                return false;
            }
            std::string_view top = t.substr(dot + 1);
            if (top.empty() || top.front() == '-' || top.back() == '-') {
                return false;
            }
            bool alpha = false;
            for (char c : top) {
                if (!spf_alpha(c) && !spf_digit(c) && c != '-') {
                    return false;
                }
                alpha |= spf_alpha(c) || c == '-';
            }
            return alpha;
        }

        inline bool spf_cidr(std::string_view s, int max, int& out) noexcept {
            if (s.empty() || s.size() > 3 || (s.size() > 1 && s[0] == '0')) {
                return false;
            }
            int n = 0;
            for (char c : s) {
                if (!spf_digit(c)) {
                    return false;
                }
                n = n * 10 + (c - '0');
            }
            if (n > max) {
                return false;
            }
            out = n;
            return true;
        }

        // "[domain-spec][/cidr4][//cidr6]" after a, mx
        inline bool spf_dual(std::string_view rest, SpfTerm& t) noexcept {
            size_t slash = rest.find('/');
            std::string_view spec = rest.substr(0, slash);
            std::string_view cidr = slash == std::string_view::npos ? std::string_view() : rest.substr(slash);
            if (!spec.empty()) {
                if (spec[0] != ':' || !spf_domain_spec_valid(spec.substr(1))) {
                    return false;
                }
                t.domain = std::string(spec.substr(1));
            }
            if (cidr.empty()) {
                return true;
            }
            if (cidr.substr(0, 2) == "//") {
                return spf_cidr(cidr.substr(2), 128, t.cidr6);
            }
            size_t six = cidr.find("//");
            if (!spf_cidr(cidr.substr(1, six == std::string_view::npos ? std::string_view::npos : six - 1), 32, t.cidr4)) {
                return false;
            }
            return six == std::string_view::npos || spf_cidr(cidr.substr(six + 2), 128, t.cidr6);
        }

        // A record (§4.6.1, §12) into terms; false for any syntax error
        inline bool spf_parse(std::string_view text, SpfRecord& rec) {
            size_t at = 6;   // past "v=spf1"
            while (at < text.size()) {
                while (at < text.size() && text[at] == ' ') {
                    ++at;
                }
                if (at >= text.size()) {
                    break;
                }
                size_t end = text.find(' ', at);
                std::string_view term = text.substr(at, end == std::string_view::npos ? std::string_view::npos : end - at);
                at = end == std::string_view::npos ? text.size() : end;
                // a modifier: name "=" macro-string, the name ALPHA *( ALPHA / DIGIT / "-" / "_" / "." )
                size_t eq = term.find('=');
                size_t sep = term.find_first_of(":/");
                if (eq != std::string_view::npos && (sep == std::string_view::npos || eq < sep)) {
                    std::string_view name = term.substr(0, eq);
                    std::string_view value = term.substr(eq + 1);
                    if (name.empty() || !spf_alpha(name[0])) {
                        return false;
                    }
                    for (char c : name) {
                        if (!spf_alpha(c) && !spf_digit(c) && c != '-' && c != '_' && c != '.') {
                            return false;
                        }
                    }
                    if (mail_iequal(name, "redirect")) {
                        if (rec.has_redirect || !spf_domain_spec_valid(value)) {
                            return false;
                        }
                        rec.has_redirect = true;
                        rec.redirect = std::string(value);
                    } else if (mail_iequal(name, "exp")) {
                        if (rec.has_exp || !spf_domain_spec_valid(value)) {
                            return false;
                        }
                        rec.has_exp = true;
                        rec.exp = std::string(value);
                    } else if (!spf_macro_valid(value, false)) {
                        return false;
                    }
                    continue;
                }
                SpfTerm t;
                t.text = std::string(term);
                std::string_view m = term;
                if (!m.empty() && std::string_view("+-~?").find(m[0]) != std::string_view::npos) {
                    t.qualifier = m[0];
                    m.remove_prefix(1);
                }
                size_t name_end = m.find_first_of(":/");
                std::string name = mail_lowered(m.substr(0, name_end));
                std::string_view rest = name_end == std::string_view::npos ? std::string_view() : m.substr(name_end);
                if (name == "all") {
                    if (!rest.empty()) {
                        return false;
                    }
                    t.kind = SpfKind::all;
                    rec.has_all = true;
                } else if (name == "include" || name == "exists") {
                    if (rest.size() < 2 || rest[0] != ':' || !spf_domain_spec_valid(rest.substr(1))) {
                        return false;
                    }
                    t.kind = name == "include" ? SpfKind::include : SpfKind::exists;
                    t.domain = std::string(rest.substr(1));
                } else if (name == "a" || name == "mx") {
                    t.kind = name == "a" ? SpfKind::a : SpfKind::mx;
                    if (!spf_dual(rest, t)) {
                        return false;
                    }
                } else if (name == "ptr") {
                    t.kind = SpfKind::ptr;
                    if (!rest.empty()) {
                        if (rest[0] != ':' || !spf_domain_spec_valid(rest.substr(1))) {
                            return false;
                        }
                        t.domain = std::string(rest.substr(1));
                    }
                } else if (name == "ip4" || name == "ip6") {
                    bool four = name == "ip4";
                    if (rest.size() < 2 || rest[0] != ':') {
                        return false;
                    }
                    std::string_view addr = rest.substr(1);
                    size_t slash = addr.find('/');
                    std::string_view ip = addr.substr(0, slash);
                    auto parsed = IpText::parse(ip);
                    if (!parsed || parsed->has_zone() || (four ? !parsed->is_v4() : !parsed->is_v6())) {
                        return false;
                    }
                    t.kind = four ? SpfKind::ip4 : SpfKind::ip6;
                    t.address = *parsed;
                    if (slash != std::string_view::npos && !spf_cidr(addr.substr(slash + 1), four ? 32 : 128, four ? t.cidr4 : t.cidr6)) {
                        return false;
                    }
                } else {
                    return false;
                }
                rec.terms.push_back(std::move(t));
            }
            return true;
        }

        // The address as %{i} writes it: IPv4's four numbers, IPv6's 32
        // nibbles split by dots
        inline std::string spf_ip_dotted(const ip_address& ip) {
            auto b = ip.bytes();
            std::string out;
            if (ip.is_v4()) {
                for (int i = 12; i < 16; ++i) {
                    if (i > 12) {
                        out += '.';
                    }
                    out += std::to_string(unsigned(b[size_t(i)]));
                }
                return out;
            }
            static constexpr char hex[] = "0123456789abcdef";
            for (size_t i = 0; i < 16; ++i) {
                if (i) {
                    out += '.';
                }
                out += hex[b[i] >> 4];
                out += '.';
                out += hex[b[i] & 15];
            }
            return out;
        }

        // §7.3: a value split by its delimiters, reversed with "r", its
        // last n parts joined by "."
        inline std::string spf_transform(std::string_view value, size_t n, bool reverse, std::string_view delimiters) {
            if (delimiters.empty()) {
                delimiters = ".";
            }
            vector<std::string_view> parts;
            size_t at = 0;
            for (size_t i = 0; i <= value.size(); ++i) {
                if (i == value.size() || delimiters.find(value[i]) != std::string_view::npos) {
                    parts.push_back(value.substr(at, i - at));
                    at = i + 1;
                }
            }
            if (reverse) {
                for (size_t i = 0, j = parts.size() - 1; i < j; ++i, --j) {
                    std::swap(parts[i], parts[j]);
                }
            }
            size_t from = n && n < parts.size() ? parts.size() - n : 0;
            std::string out;
            for (size_t i = from; i < parts.size(); ++i) {
                if (i > from) {
                    out += '.';
                }
                out += parts[i];
            }
            return out;
        }

        inline std::string spf_url_escape(std::string_view s) {
            static constexpr char hex[] = "0123456789ABCDEF";
            std::string out;
            for (char c : s) {
                if (spf_alpha(c) || spf_digit(c) || c == '-' || c == '.' || c == '_' || c == '~') {
                    out += c;
                } else {
                    out += '%';
                    out += hex[uint8_t(c) >> 4];
                    out += hex[uint8_t(c) & 15];
                }
            }
            return out;
        }

        inline async::task<DnsAnswer> spf_query(tracked_ptr<SpfState> st, std::string name, uint16_t type) {
            if (sgcl::clock::now() > st->deadline) {
                st->timed_out = true;
                co_return dns_outcome(DnsStatus::timeout);
            }
            if (name.empty() || name.back() != '.') {
                name += '.';
            }
            co_return co_await dns_lookup(string(name), type, false, st->settings, async::stop_token());
        }

        // %{p} (§7.3): a name of the address's PTR whose A or AAAA holds
        // the address, the current domain's or one under it first;
        // "unknown" for none
        inline async::task<std::string> spf_validated_name(tracked_ptr<SpfState> st, std::string domain) {
            const ip_address& ip = st->ip;
            std::string rev;
            auto b = ip.bytes();
            if (ip.is_v4()) {
                rev = std::to_string(unsigned(b[15])) + "." + std::to_string(unsigned(b[14])) + "." + std::to_string(unsigned(b[13])) + "." + std::to_string(unsigned(b[12])) + ".in-addr.arpa";
            } else {
                static constexpr char hex[] = "0123456789abcdef";
                for (size_t i = 16; i-- > 0;) {
                    rev += hex[b[i] & 15];
                    rev += '.';
                    rev += hex[b[i] >> 4];
                    rev += '.';
                }
                rev += "ip6.arpa";
            }
            DnsAnswer ptr = co_await spf_query(st, rev, dns_type::ptr);
            if (ptr.status != DnsStatus::ok) {
                co_return std::string();
            }
            std::string fallback;
            size_t n = 0;
            for (auto& r : ptr.records) {
                if (++n > 10) {
                    break;
                }
                std::string name = mail_domain(r.name.view());
                DnsAnswer a = co_await spf_query(st, name, ip.is_v4() ? dns_type::a : dns_type::aaaa);
                if (a.status != DnsStatus::ok) {
                    continue;
                }
                bool found = false;
                for (auto& x : a.records) {
                    found |= x.address_size == (ip.is_v4() ? 4 : 16) && std::equal(x.address, x.address + x.address_size, b.data() + (ip.is_v4() ? 12 : 0));
                }
                if (!found) {
                    continue;
                }
                if (mail_under(name, domain)) {
                    co_return name;
                }
                if (fallback.empty()) {
                    fallback = name;
                }
            }
            co_return fallback;
        }

        // §7: a macro string expanded; false for a letter past its place
        inline async::task<bool> spf_expand(tracked_ptr<SpfState> st, std::string domain, std::string spec, bool exp, std::string* out) {
            std::string r;
            for (size_t i = 0; i < spec.size(); ++i) {
                char c = spec[i];
                if (c != '%') {
                    r += c;
                    continue;
                }
                char n = spec[++i];
                if (n == '%') {
                    r += '%';
                    continue;
                }
                if (n == '_') {
                    r += ' ';
                    continue;
                }
                if (n == '-') {
                    r += "%20";
                    continue;
                }
                char letter = spec[++i];
                bool upper = letter >= 'A' && letter <= 'Z';
                letter = mail_lower(letter);
                ++i;
                size_t digits = 0;
                bool has_digits = false;
                while (spf_digit(spec[i])) {
                    digits = digits * 10 + size_t(spec[i] - '0');
                    if (digits > 128) {
                        digits = 128;
                    }
                    has_digits = true;
                    ++i;
                }
                if (has_digits && digits == 0) {
                    co_return false;
                }
                bool reverse = false;
                if (mail_lower(spec[i]) == 'r') {
                    reverse = true;
                    ++i;
                }
                size_t delim_from = i;
                while (spec[i] != '}') {
                    ++i;
                }
                std::string_view delimiters = std::string_view(spec).substr(delim_from, i - delim_from);
                std::string value;
                switch (letter) {
                    case 's': value = st->sender; break;
                    case 'l': value = st->local; break;
                    case 'o': value = st->sender_domain; break;
                    case 'd': value = domain; break;
                    case 'i': value = spf_ip_dotted(st->ip); break;
                    case 'p': {
                        if (++st->lookups > st->max_lookups) {
                            co_return false;
                        }
                        value = co_await spf_validated_name(st, domain);
                        if (value.empty()) {
                            value = "unknown";
                        }
                        break;
                    }
                    case 'v': value = st->ip.is_v4() ? "in-addr" : "ip6"; break;
                    case 'h': value = st->helo; break;
                    case 'c': value = std::string(st->ip.to_string().view()); break;
                    case 'r': value = st->receiver.empty() ? std::string("unknown") : st->receiver; break;
                    case 't': value = std::to_string(st->now); break;
                    default: co_return false;
                }
                value = spf_transform(value, digits, reverse, delimiters);
                r += upper ? spf_url_escape(value) : value;
            }
            if (!exp) {
                // a domain past 253 characters loses labels from its left (§7.3)
                while (r.size() > 253) {
                    size_t dot = r.find('.');
                    if (dot == std::string::npos) {
                        break;
                    }
                    r.erase(0, dot + 1);
                }
            }
            *out = std::move(r);
            co_return true;
        }

        inline bool spf_in(const ip_address& ip, const uint8_t* address, uint8_t size, int cidr4, int cidr6) {
            if ((ip.is_v4() ? 4 : 16) != size) {
                return false;
            }
            ip_address other = size == 4 ? ip_address::v4(address[0], address[1], address[2], address[3]) : [&] {
                array<uint8_t, 16> b;
                std::copy(address, address + 16, b.data());
                return ip_address::v6(b);
            }();
            return ip_network(other, size == 4 ? cidr4 : cidr6).contains(ip);
        }

        inline SpfOutcome spf_error(spf::status s, std::string why) {
            SpfOutcome o;
            o.status = s;
            o.reason = std::move(why);
            return o;
        }

        // A query that did not answer as a check needs: temperror for a
        // DNS failure; a void lookup counted, permerror past the limit
        inline bool spf_void(SpfState& st, const DnsAnswer& a, SpfOutcome& err) {
            if (a.status == DnsStatus::nxdomain || a.status == DnsStatus::nodata || (a.status == DnsStatus::ok && a.records.empty())) {
                if (++st.voids > st.max_voids) {
                    err = spf_error(status::permerror, "too many void DNS lookups");
                    return false;
                }
                return true;
            }
            if (a.status != DnsStatus::ok) {
                err = spf_error(status::temperror, st.timed_out ? "the check took too long" : "DNS lookup failed");
                return false;
            }
            return true;
        }

        inline status spf_qualified(char q) noexcept {
            switch (q) {
                case '-': return status::fail;
                case '~': return status::softfail;
                case '?': return status::neutral;
                default: return status::pass;
            }
        }

        // check_host() of §4: the record of the domain read and evaluated
        inline async::task<SpfOutcome> spf_check_host(tracked_ptr<SpfState> st, std::string domain, bool top) {
            if (!mail_valid_domain(domain)) {
                co_return spf_error(status::none, "no valid domain");
            }
            DnsAnswer txt = co_await spf_query(st, domain, dns_type::txt);
            if (txt.status == DnsStatus::nxdomain || txt.status == DnsStatus::nodata) {
                co_return spf_error(status::none, "no SPF record");
            }
            if (txt.status != DnsStatus::ok) {
                co_return spf_error(status::temperror, st->timed_out ? "the check took too long" : "DNS lookup of the record failed");
            }
            const string* record = nullptr;
            for (auto& r : txt.records) {
                std::string_view v = r.text.view();
                if (v.size() >= 6 && mail_iequal(v.substr(0, 6), "v=spf1") && (v.size() == 6 || v[6] == ' ')) {
                    if (record) {
                        co_return spf_error(status::permerror, "more than one SPF record");
                    }
                    record = &r.text;
                }
            }
            if (!record) {
                co_return spf_error(status::none, "no SPF record");
            }
            SpfRecord rec;
            if (!spf_parse(record->view(), rec)) {
                co_return spf_error(status::permerror, "SPF record syntax error");
            }
            SpfOutcome err;
            for (auto& t : rec.terms) {
                bool match = false;
                switch (t.kind) {
                    case SpfKind::all:
                        match = true;
                        break;
                    case SpfKind::ip4:
                    case SpfKind::ip6: {
                        auto b = t.address.bytes();
                        match = spf_in(st->ip, t.kind == SpfKind::ip4 ? b.data() + 12 : b.data(), t.kind == SpfKind::ip4 ? 4 : 16, t.cidr4, t.cidr6);
                        break;
                    }
                    case SpfKind::include: {
                        if (++st->lookups > st->max_lookups) {
                            co_return spf_error(status::permerror, "too many DNS lookups");
                        }
                        std::string target;
                        if (!co_await spf_expand(st, domain, t.domain, false, &target)) {
                            co_return spf_error(status::permerror, "macro error");
                        }
                        SpfOutcome inner = co_await spf_check_host(st, target, false);
                        switch (inner.status) {
                            case status::pass: match = true; break;
                            case status::fail:
                            case status::softfail:
                            case status::neutral: break;
                            case status::temperror: co_return inner;
                            case status::permerror: co_return inner;
                            case status::none: co_return spf_error(status::permerror, "include of a domain without an SPF record");
                        }
                        break;
                    }
                    case SpfKind::a:
                    case SpfKind::mx:
                    case SpfKind::exists: {
                        if (++st->lookups > st->max_lookups) {
                            co_return spf_error(status::permerror, "too many DNS lookups");
                        }
                        std::string target = domain;
                        if (!t.domain.empty() && !co_await spf_expand(st, domain, t.domain, false, &target)) {
                            co_return spf_error(status::permerror, "macro error");
                        }
                        if (t.kind == SpfKind::exists) {
                            DnsAnswer a = co_await spf_query(st, target, dns_type::a);
                            if (!spf_void(*st, a, err)) {
                                co_return err;
                            }
                            match = a.status == DnsStatus::ok && !a.records.empty();
                            break;
                        }
                        uint16_t type = st->ip.is_v4() ? dns_type::a : dns_type::aaaa;
                        vector<string> hosts;
                        if (t.kind == SpfKind::a) {
                            hosts.push_back(string(target));
                        } else {
                            DnsAnswer mx = co_await spf_query(st, target, dns_type::mx);
                            if (!spf_void(*st, mx, err)) {
                                co_return err;
                            }
                            if (mx.records.size() > 10) {
                                co_return spf_error(status::permerror, "more than 10 MX records");
                            }
                            for (auto& r : mx.records) {
                                if (r.name.view() != "." && !r.name.empty()) {
                                    hosts.push_back(r.name);
                                }
                            }
                        }
                        for (auto& h : hosts) {
                            DnsAnswer a = co_await spf_query(st, std::string(h.view()), type);
                            if (t.kind == SpfKind::a) {
                                if (!spf_void(*st, a, err)) {
                                    co_return err;
                                }
                            } else if (a.status != DnsStatus::ok && a.status != DnsStatus::nxdomain && a.status != DnsStatus::nodata) {
                                co_return spf_error(status::temperror, st->timed_out ? "the check took too long" : "DNS lookup failed");
                            }
                            for (auto& r : a.records) {
                                match |= spf_in(st->ip, r.address, r.address_size, t.cidr4, t.cidr6);
                            }
                            if (match) {
                                break;
                            }
                        }
                        break;
                    }
                    case SpfKind::ptr: {
                        if (++st->lookups > st->max_lookups) {
                            co_return spf_error(status::permerror, "too many DNS lookups");
                        }
                        std::string target = domain;
                        if (!t.domain.empty() && !co_await spf_expand(st, domain, t.domain, false, &target)) {
                            co_return spf_error(status::permerror, "macro error");
                        }
                        std::string name = co_await spf_validated_name(st, mail_domain(target));
                        match = !name.empty() && mail_under(name, mail_domain(target));
                        break;
                    }
                }
                if (match) {
                    SpfOutcome o;
                    o.status = spf_qualified(t.qualifier);
                    o.mechanism = t.text;
                    if (o.status == status::fail && top && rec.has_exp) {
                        std::string where;
                        if (co_await spf_expand(st, domain, rec.exp, false, &where) && mail_valid_domain(where)) {
                            DnsAnswer e = co_await spf_query(st, where, dns_type::txt);
                            if (e.status == DnsStatus::ok && e.records.size() == 1 && spf_macro_valid(e.records[0].text.view(), true)) {
                                std::string text;
                                if (co_await spf_expand(st, domain, std::string(e.records[0].text.view()), true, &text)) {
                                    o.explanation = std::move(text);
                                }
                            }
                        }
                    }
                    co_return o;
                }
            }
            if (rec.has_redirect && !rec.has_all) {
                if (++st->lookups > st->max_lookups) {
                    co_return spf_error(status::permerror, "too many DNS lookups");
                }
                std::string target;
                if (!co_await spf_expand(st, domain, rec.redirect, false, &target)) {
                    co_return spf_error(status::permerror, "macro error");
                }
                SpfOutcome r = co_await spf_check_host(st, target, top);
                if (r.status == status::none) {
                    co_return spf_error(status::permerror, "redirect to a domain without an SPF record");
                }
                co_return r;
            }
            SpfOutcome o;
            o.status = status::neutral;
            o.mechanism = "default";
            co_return o;
        }

        inline async::task<spf::result> spf_check(ip_address client, string sender, string helo, spf::options o) {
            spf::result out;
            auto settings = DnsAccess::settings(o.dns);
            tracked_ptr st = make_tracked<SpfState>();
            st->ip = client.is_v4_mapped() ? client.unmap() : client.with_zone(string());
            st->helo = mail_domain(helo.view());
            std::string s(sender.view());
            size_t at = s.rfind('@');
            if (s.empty() || at == std::string::npos) {
                st->local = "postmaster";
                st->sender_domain = s.empty() ? st->helo : mail_domain(s);
            } else {
                st->local = at == 0 ? std::string("postmaster") : s.substr(0, at);
                st->sender_domain = mail_domain(std::string_view(s).substr(at + 1));
            }
            st->sender = st->local + "@" + st->sender_domain;
            st->receiver = std::string(o.receiver.view());
            st->max_lookups = o.max_lookups;
            st->max_voids = o.max_void_lookups;
            st->deadline = sgcl::clock::now() + (o.timeout > duration::zero() ? o.timeout : 20 * second);
            st->now = time::now().unix();
            out.domain = string(st->sender_domain);
            if (!settings) {
                out.status = status::temperror;
                out.reason = settings.error().message();
                co_return out;
            }
            st->settings = std::move(*settings);
            if (!client.is_valid()) {
                out.status = status::none;
                out.reason = string("no client address");
                co_return out;
            }
            SpfOutcome r = co_await spf_check_host(st, st->sender_domain, true);
            out.status = r.status;
            out.mechanism = string(r.mechanism);
            out.explanation = string(r.explanation);
            out.reason = string(r.reason);
            out.lookups = st->lookups;
            co_return out;
        }
    }

    // check_host() for a message's sender: the domain of the sender
    // ("user@example.com"; the null sender "" takes postmaster@ of the
    // HELO name, §2.4), the client's address, the HELO name for %{h}. To
    // check HELO itself (§2.3), give the HELO name as the sender too. A
    // check never fails: what went wrong is its status and reason
    // `check(...)` on this thread, `co_await async_check(...)` in a task
    inline result check(const ip_address& client, const string& sender, const string& helo, const options& o = {}) {
        return detail::spf_check(client, sender, helo, o).wait();
    }

    inline async::task<result> async_check(ip_address client, string sender, string helo, options o = {}) noexcept {
        return detail::spf_check(client, std::move(sender), std::move(helo), std::move(o));
    }
}
