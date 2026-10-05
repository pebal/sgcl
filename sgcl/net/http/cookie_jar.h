//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cookie.h"
#include "public_suffix.h"
#include "../error.h"
#include "../url.h"
#include "../../async/coroutine.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../core/detail/handle_word.h"
#include "../../encoding/hex.h"
#include "../../encoding/json.h"
#include "../../io/file.h"
#include "../../io/fs.h"
#include "../../time/datetime.h"
#include "../../time/layout.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace sgcl::net::http {
    class cookie_jar;

    namespace detail {
        // A cookie as the jar keeps it (RFC 6265bis §5.7): the domain without
        // a dot in front, the path the cookie applies to, the times in
        // nanoseconds since 1970, the order of creation for the ties
        struct JarEntry {
            string name;
            string value;
            string domain;
            string path;
            int64_t expiry = 0;          // persistent ones'
            int64_t created = 0;
            int64_t last_access = 0;
            uint64_t sequence = 0;       // creation's order: the same as created's, without its ties
            bool persistent = false;
            bool host_only = true;
            bool secure = false;
            bool http_only = false;
            bool partitioned = false;
            uint8_t same_site = 0;       // 0 none given, 1 Strict, 2 Lax, 3 None
        };

        // The jar: its cookies in buckets by the registrable domain of their
        // domain (the domain itself when it has none: an address, a public
        // suffix's own host-only cookies), under one lock
        struct JarState {
            std::mutex lock;
            map<string, vector<JarEntry>> buckets;
            size_t count = 0;
            uint64_t next_sequence = 0;
            size_t max_cookies = 3000;
            size_t max_cookies_per_domain = 180;
        };

        struct CookieJarAccess {
            static const tracked_ptr<JarState>& state(const cookie_jar& j) noexcept;
        };

        // 400 days: RFC 6265bis §5.5's limit of Expires and Max-Age
        inline constexpr int64_t JarAgeLimit = int64_t(400) * 86400 * 1000000000;

        SGCL_INLINE_HOT int64_t jar_add(int64_t a, int64_t b) noexcept {
            return a > INT64_MAX - b ? INT64_MAX : a + b;
        }

        SGCL_INLINE_HOT const char* same_site_text(uint8_t s) noexcept {
            return s == 1 ? "Strict" : s == 2 ? "Lax" : s == 3 ? "None" : "";
        }

        SGCL_INLINE_HOT uint8_t same_site_of(std::string_view s) noexcept {
            return iequal(s, "strict") ? 1 : iequal(s, "lax") ? 2 : iequal(s, "none") ? 3 : 0;
        }

        // Whether s holds a control a cookie may not (RFC 6265bis §5.6:
        // %x00-08, %x0A-1F, %x7F; a tab is allowed)
        inline bool jar_has_control(std::string_view s) noexcept {
            uint8_t bad = 0;
            for (unsigned char c : s) {
                bad |= uint8_t((c < 0x20 && c != 0x09) | (c == 0x7F));
            }
            return bad != 0;
        }

        // RFC 6265 §5.1.3: the host is the domain, or a name under it (an
        // address only the same)
        SGCL_INLINE_HOT bool domain_match(std::string_view host, std::string_view domain, bool host_is_ip) noexcept {
            if (host == domain) {
                return true;
            }
            return !host_is_ip && host.size() > domain.size() && host[host.size() - domain.size() - 1] == '.'
                && host.substr(host.size() - domain.size()) == domain;
        }

        // §5.1.4: the request's path is the cookie's, or under it
        SGCL_INLINE_HOT bool path_match(std::string_view request, std::string_view cookie) noexcept {
            if (request.size() < cookie.size() || request.substr(0, cookie.size()) != cookie) {
                return false;
            }
            return request.size() == cookie.size() || cookie.back() == '/' || request[cookie.size()] == '/';
        }

        // §5.1.4: the default path of a request's path, its directory
        inline std::string_view default_path(std::string_view path) noexcept {
            if (path.empty() || path.front() != '/') {
                return "/";
            }
            size_t last = path.rfind('/');
            return last == 0 ? std::string_view("/") : path.substr(0, last);
        }

        // The bucket of a host or a domain: its registrable domain, itself
        // for an address or a public suffix
        inline std::string_view jar_bucket(std::string_view name, bool is_ip) noexcept {
            if (is_ip) {
                return name;
            }
            size_t n = public_suffix_length(name);
            if (n == 0 || n >= name.size()) {
                return name;
            }
            size_t from = name.rfind('.', name.size() - n - 2);
            return name.substr(from == std::string_view::npos ? 0 : from + 1);
        }

        // What of a request URL the jar asks: the host as cookies compare it
        // (lower case, A-labels, no dot at its end), whether it is an address,
        // whether the origin is secure, and the path
        struct JarTarget {
            std::string host;
            bool is_ip = false;
            bool secure = false;
            string path;
        };

        // Secure: https and wss, and the loopback as browsers take it
        // (RFC 6265bis §5.8.3's note: localhost is trusted): localhost, a
        // name under it, 127.0.0.0/8 and ::1
        inline optional<JarTarget> jar_target(const net::url& u) noexcept {
            auto scheme = u.scheme();
            auto s = scheme.view();
            bool tls = s == "https" || s == "wss";
            if (!tls && s != "http" && s != "ws") {
                return nullopt;
            }
            JarTarget t;
            t.host.assign(u.hostname().view());
            if (!t.host.empty() && t.host.back() == '.') {
                t.host.pop_back();
            }
            auto address = u.host_address();
            t.is_ip = address.has_value();
            if (t.host.empty() || (!t.is_ip && (t.host.front() == '.' || t.host.back() == '.' || t.host.find("..") != std::string::npos))) {
                return nullopt;   // a name with an empty label is no host a cookie could name
            }
            bool loopback = t.is_ip ? address->is_loopback()
                                    : (t.host == "localhost" || (t.host.size() > 10 && std::string_view(t.host).substr(t.host.size() - 10) == ".localhost"));
            t.secure = tls || loopback;
            t.path = u.path();
            if (t.path.empty()) {
                t.path = "/";
            }
            return t;
        }

        // A Domain attribute as the jar compares it: a dot in front left
        // out, lower case, A-labels; "" for none, nullopt for one that
        // cannot be (a dot at its end, a name IDNA refuses)
        inline optional<std::string> jar_domain(std::string_view d) noexcept {
            if (!d.empty() && d.front() == '.') {
                d.remove_prefix(1);
            }
            if (d.empty() || d.size() > 1024) {
                return std::string();   // none, or past 1024 bytes: the attribute ignored (RFC 6265bis §5.6)
            }
            if (d.back() == '.') {
                return nullopt;
            }
            bool ascii = true;
            for (char c : d) {
                ascii = ascii && uint8_t(c) < 0x80;
            }
            std::string out;
            if (ascii) {
                out.assign(d);
                for (auto& c : out) {
                    c = ascii_lower(c);
                }
            } else {
                auto r = txt::idna::to_ascii(string(d), txt::idna::options::whatwg());
                if (!r || r->empty()) {
                    return nullopt;
                }
                out.assign(r->view());
            }
            if (out.empty() || out.back() == '.' || out.front() == '.') {
                return nullopt;
            }
            return out;
        }

        SGCL_INLINE_HOT bool jar_prefix(std::string_view name, std::string_view prefix) noexcept {
            return name.size() >= prefix.size() && iequal(name.substr(0, prefix.size()), prefix);
        }

        SGCL_INLINE_HOT bool jar_same(const JarEntry& a, const JarEntry& b) noexcept {
            return a.host_only == b.host_only && a.name == b.name && a.domain == b.domain && a.path == b.path;
        }

        // The cookie of an entry: the domain with a dot in front for a
        // domain cookie, as browsers show it, the host alone for a
        // host-only one; the expiry as Expires
        inline cookie jar_cookie(const JarEntry& e) noexcept {
            cookie c(e.name, e.value);
            c.domain = e.host_only ? e.domain : string::concat('.', e.domain);
            c.path = e.path;
            if (e.persistent) {
                c.expires = time::datetime::from_unix_nano(e.expiry, time::zone::utc());
            }
            c.secure = e.secure;
            c.http_only = e.http_only;
            c.partitioned = e.partitioned;
            c.same_site = same_site_text(e.same_site);
            return c;
        }

        // Every lock taken is the jar's own, and the work under it is a few
        // comparisons per cookie of one bucket
        class Jar {
        public:
            // An entry put in, replacing the one of its name, domain,
            // host-only and path (keeping its creation), the limits then
            // held; an expired one removes that one and stays out
            static void store(JarState& s, JarEntry e, const string& bucket, int64_t now, bool keep_created) noexcept {
                auto it = s.buckets.find(bucket);
                if (it != s.buckets.end()) {
                    auto& list = it->second;
                    for (size_t i = 0; i < list.size(); ++i) {
                        if (jar_same(list[i], e)) {
                            if (!keep_created) {
                                e.created = list[i].created;
                                e.sequence = list[i].sequence;
                            }
                            list.erase(list.begin() + i);
                            --s.count;
                            break;
                        }
                    }
                }
                if (e.persistent && e.expiry <= now) {
                    if (it != s.buckets.end() && it->second.empty()) {
                        s.buckets.erase(it);
                    }
                    return;
                }
                if (it == s.buckets.end()) {
                    it = s.buckets.insert_or_assign(bucket, vector<JarEntry>()).first;
                }
                it->second.push_back(std::move(e));
                ++s.count;
                limit_bucket(s, it->second, now);
                if (it->second.empty()) {
                    s.buckets.erase(it);
                }
                if (s.count > s.max_cookies) {
                    limit_total(s, now);
                }
            }

            // The expired cookies of a bucket out
            static size_t purge(JarState& s, vector<JarEntry>& list, int64_t now) noexcept {
                size_t kept = 0, n = list.size();
                for (size_t i = 0; i < n; ++i) {
                    if (list[i].persistent && list[i].expiry <= now) {
                        continue;
                    }
                    if (kept != i) {
                        list[kept] = std::move(list[i]);
                    }
                    ++kept;
                }
                size_t gone = n - kept;
                list.erase(list.begin() + kept, list.end());
                s.count -= gone;
                return gone;
            }

            // RFC 6265bis §5.7's eviction within a domain: the expired, then
            // the cookies without Secure, then the rest, the least recently
            // used first in each
            static void limit_bucket(JarState& s, vector<JarEntry>& list, int64_t now) noexcept {
                if (list.size() <= s.max_cookies_per_domain) {
                    return;
                }
                purge(s, list, now);
                for (int pass = 0; pass < 2 && list.size() > s.max_cookies_per_domain; ++pass) {
                    while (list.size() > s.max_cookies_per_domain) {
                        size_t victim = list.size();
                        for (size_t i = 0; i < list.size(); ++i) {
                            if ((pass == 0 && list[i].secure) || (victim < list.size() && !older(list[i], list[victim]))) {
                                continue;
                            }
                            victim = i;
                        }
                        if (victim == list.size()) {
                            break;
                        }
                        list.erase(list.begin() + victim);
                        --s.count;
                    }
                }
            }

            // Past the jar's limit: the expired everywhere, then the least
            // recently used of all
            static void limit_total(JarState& s, int64_t now) noexcept {
                for (auto& kv : s.buckets) {
                    purge(s, kv.second, now);
                }
                while (s.count > s.max_cookies) {
                    vector<JarEntry>* where = nullptr;
                    size_t victim = 0;
                    for (auto& kv : s.buckets) {
                        auto& list = kv.second;
                        for (size_t i = 0; i < list.size(); ++i) {
                            if (!where || older(list[i], (*where)[victim])) {
                                where = &list;
                                victim = i;
                            }
                        }
                    }
                    if (!where) {
                        break;
                    }
                    where->erase(where->begin() + victim);
                    --s.count;
                }
                drop_empty(s);
            }

            static void drop_empty(JarState& s) noexcept {
                vector<string> empty;
                for (auto& kv : s.buckets) {
                    if (kv.second.empty()) {
                        empty.push_back(kv.first);
                    }
                }
                for (auto& k : empty) {
                    s.buckets.erase(k);
                }
            }

            // The order of creation: the time, then the order the jar took
            // them in (a file's cookies loaded in their times' order)
            SGCL_INLINE_HOT static bool created_before(const JarEntry& a, const JarEntry& b) noexcept {
                return a.created != b.created ? a.created < b.created : a.sequence < b.sequence;
            }

            SGCL_INLINE_HOT static bool older(const JarEntry& a, const JarEntry& b) noexcept {
                return a.last_access < b.last_access || (a.last_access == b.last_access && a.sequence < b.sequence);
            }

            // RFC 6265bis §5.7 for one cookie a response to u set
            static void set(JarState& s, const JarTarget& t, const cookie& c, int64_t now) noexcept {
                auto name = c.name.view();
                auto value = c.value.view();
                if (!is_token(name) || name.size() + value.size() > 4096 || jar_has_control(value)) {
                    return;   // a name that is not a token (as cookie::parse reads one), a control in the value
                }
                JarEntry e;
                e.name = c.name;
                e.value = c.value;
                if (c.max_age) {
                    e.persistent = true;
                    int64_t age = c.max_age->nanoseconds();
                    e.expiry = age <= 0 ? INT64_MIN : jar_add(now, std::min(age, JarAgeLimit));
                } else if (c.expires) {
                    e.persistent = true;
                    e.expiry = std::min(c.expires->unix_nano(), jar_add(now, JarAgeLimit));
                }
                auto domain = jar_domain(c.domain.view());
                if (!domain) {
                    return;
                }
                if (!domain->empty()) {
                    if (t.is_ip) {
                        if (*domain != t.host) {
                            return;   // an address's cookie is its own alone
                        }
                        domain->clear();
                    } else if (public_suffix_length(*domain) >= domain->size()) {
                        // a public suffix (§5.7 step 5): the host's own when it is the host
                        if (*domain != t.host) {
                            return;
                        }
                        domain->clear();
                    } else if (!domain_match(t.host, *domain, false)) {
                        return;
                    }
                }
                e.host_only = domain->empty();
                e.domain = e.host_only ? string(t.host) : string(*domain);
                auto path = c.path.view();
                // a Path that does not begin with '/' (§5.2.4), is past 1024 bytes
                // or holds a control is none: the default path
                bool given = !path.empty() && path.front() == '/' && path.size() <= 1024 && !jar_has_control(path);
                e.path = given ? c.path : string(default_path(t.path.view()));
                e.secure = c.secure;
                if (e.secure && !t.secure) {
                    return;   // Secure from an origin that is not
                }
                e.http_only = c.http_only;
                e.partitioned = c.partitioned;
                e.same_site = same_site_of(c.same_site.view());
                if (e.same_site == 3 && !e.secure) {
                    return;   // SameSite=None without Secure
                }
                if (jar_prefix(name, "__Secure-") && !e.secure) {
                    return;
                }
                if (jar_prefix(name, "__Host-") && (!e.secure || !e.host_only || c.path.view() != "/")) {
                    return;
                }
                string bucket(jar_bucket(e.domain.view(), t.is_ip));
                if (!e.secure && !t.secure) {
                    // a Secure cookie is left alone by an insecure origin
                    auto it = s.buckets.find(bucket);
                    if (it != s.buckets.end()) {
                        for (auto& old : it->second) {
                            if (old.secure && old.name == e.name
                                && (domain_match(old.domain.view(), e.domain.view(), t.is_ip) || domain_match(e.domain.view(), old.domain.view(), t.is_ip))
                                && path_match(e.path.view(), old.path.view())) {
                                return;
                            }
                        }
                    }
                }
                e.created = now;
                e.last_access = now;
                e.sequence = s.next_sequence++;
                store(s, std::move(e), bucket, now, false);
            }

            // §5.8.3: the cookies for t, longer paths first, then earlier
            // creation; their last access now
            static vector<JarEntry*> matching(JarState& s, const JarTarget& t, int64_t now) noexcept {
                vector<JarEntry*> out;
                auto it = s.buckets.find(string(jar_bucket(t.host, t.is_ip)));
                if (it == s.buckets.end()) {
                    return out;
                }
                auto& list = it->second;
                purge(s, list, now);
                for (auto& e : list) {
                    bool host = e.host_only ? e.domain.view() == t.host : domain_match(t.host, e.domain.view(), t.is_ip);
                    if (host && path_match(t.path.view(), e.path.view()) && (!e.secure || t.secure)) {
                        out.push_back(&e);
                    }
                }
                if (out.size() > 1) {
                    std::sort(out.begin(), out.end(), [](const JarEntry* a, const JarEntry* b) {
                        if (a->path.size() != b->path.size()) {
                            return a->path.size() > b->path.size();
                        }
                        return created_before(*a, *b);
                    });
                }
                for (auto* e : out) {
                    e->last_access = now;
                }
                if (list.empty()) {
                    s.buckets.erase(it);
                }
                return out;
            }
        };

        // A cookie's value in a Cookie field: as it came, in quotes when it
        // holds a space or a comma (the quotes cookie::parse took off), as Go
        // writes it
        SGCL_INLINE_HOT bool jar_quoted(std::string_view v) noexcept {
            return v.find_first_of(" ,") != std::string_view::npos;
        }

        // A cookie of the file save() writes: the times as RFC 3339 in UTC,
        // Expires "" for a session cookie; a value or a path that is not
        // UTF-8 (a cookie's bytes are any) in hexadecimal instead, which JSON
        // text could not carry as it is
        struct JarRecord {
            string name;
            string value;
            string value_hex;
            string domain;
            bool host_only = true;
            string path;
            string path_hex;
            string expires;
            bool secure = false;
            bool http_only = false;
            string same_site;
            bool partitioned = false;
            string created;
            string last_access;

            void describe(encoding::field_list& f) {
                f.add("name", name).required();
                f.add("value", value);
                f.add("value_hex", value_hex).omit_empty();
                f.add("domain", domain).required();
                f.add("host_only", host_only);
                f.add("path", path);
                f.add("path_hex", path_hex).omit_empty();
                f.add("expires", expires).omit_empty();
                f.add("secure", secure);
                f.add("http_only", http_only);
                f.add("same_site", same_site).omit_empty();
                f.add("partitioned", partitioned).omit_empty();
                f.add("created", created);
                f.add("last_access", last_access);
            }
        };

        struct JarFile {
            int64_t version = 0;         // 1; a text that sets none is no jar's
            vector<JarRecord> cookies;

            void describe(encoding::field_list& f) {
                f.add("version", version).required();
                f.add("cookies", cookies).required();
            }
        };

        SGCL_INLINE_HOT string jar_time(int64_t ns) noexcept {
            return time::datetime::from_unix_nano(ns, time::zone::utc()).format(time::rfc3339_nano);
        }

        inline optional<int64_t> jar_parse_time(const string& text) noexcept {
            if (text.size() > 64) {
                return nullopt;
            }
            auto t = time::datetime::parse(text, time::rfc3339);
            if (!t) {
                return nullopt;
            }
            return t->unix_nano();
        }

        // A record of a file as an entry, held to what set() would have
        // stored; nullopt for one that could not have been
        // A field written as text or, when not UTF-8, in hexadecimal: the
        // bytes, nullopt for both given or a broken hexadecimal
        inline optional<string> jar_bytes(const string& as_text, const string& as_hex) noexcept {
            if (as_hex.empty()) {
                return as_text;
            }
            auto b = encoding::hex::decode(as_hex);
            if (!as_text.empty() || !b) {
                return nullopt;
            }
            return string(std::string_view(reinterpret_cast<const char*>(b->data()), b->size()));
        }

        inline optional<JarEntry> jar_entry(const JarRecord& r, int64_t now) noexcept {
            JarEntry e;
            auto name = r.name.view();
            auto value = jar_bytes(r.value, r.value_hex);
            auto path = jar_bytes(r.path, r.path_hex);
            if (!value || !path || !is_token(name) || name.size() + value->size() > 4096 || jar_has_control(value->view())) {
                return nullopt;
            }
            e.name = r.name;
            e.value = *value;
            auto d = r.domain.view();
            if (d.empty() || d.size() > 1024 || d.front() == '.' || d.back() == '.' || d.find("..") != std::string_view::npos) {
                return nullopt;
            }
            for (char c : d) {
                if (uint8_t(c) >= 0x7F || (c >= 'A' && c <= 'Z') || uint8_t(c) <= 0x20) {
                    return nullopt;   // a host as a URL gives it: ASCII, lower case, no space or control
                }
            }
            auto address = ip_address::parse(r.domain);
            const bool is_ip = address.has_value();
            e.host_only = r.host_only || is_ip;
            if (!e.host_only && public_suffix_length(d) >= d.size()) {
                return nullopt;
            }
            e.domain = r.domain;
            auto p = path->view();
            if (p.empty() || p.front() != '/' || jar_has_control(p)) {   // a default path may pass 1024 bytes
                return nullopt;
            }
            e.path = *path;
            if (!r.expires.empty()) {
                auto x = jar_parse_time(r.expires);
                if (!x) {
                    return nullopt;
                }
                e.persistent = true;
                e.expiry = std::min(*x, jar_add(now, JarAgeLimit));
            }
            e.secure = r.secure;
            e.http_only = r.http_only;
            e.partitioned = r.partitioned;
            if (!r.same_site.empty()) {
                e.same_site = same_site_of(r.same_site.view());
                if (e.same_site == 0) {
                    return nullopt;
                }
            }
            if ((e.same_site == 3 && !e.secure) || (jar_prefix(name, "__Secure-") && !e.secure)
                || (jar_prefix(name, "__Host-") && (!e.secure || !e.host_only || p != "/"))) {
                return nullopt;
            }
            auto created = r.created.empty() ? optional<int64_t>(now) : jar_parse_time(r.created);
            auto access = r.last_access.empty() ? created : jar_parse_time(r.last_access);
            if (!created || !access) {
                return nullopt;
            }
            e.created = *created;
            e.last_access = *access;
            return e;
        }
    }

    // A cookie jar: the cookies the responses to a client set, kept by RFC
    // 6265's storage model and handed to the requests they belong to (Go's
    // net/http/cookiejar with x/net/publicsuffix in one). A one-word handle,
    // thread-safe: a copy is the same jar, shared by a client's concurrent
    // requests and by any number of clients.
    //
    // Stored (RFC 6265 §5.3, RFC 6265bis §5.7): a cookie of a domain only
    // from a host under it, and never for a public suffix (the embedded
    // list, public_suffix.h; a Domain equal to a suffix that is the host
    // makes a host-only cookie); the default path from the URL's path;
    // Max-Age before Expires, both held to 400 days, zero, a negative age or
    // a past date deleting the cookie; a cookie of the same name, domain,
    // host-only flag and path replaced, its creation kept. RFC 6265bis's
    // rules as browsers follow them: Secure set only from a secure origin
    // (https, wss, or the loopback: localhost and 127.0.0.0/8, ::1), a
    // cookie from an insecure origin never shadowing a Secure one, the
    // prefixes __Secure- (Secure) and __Host- (Secure, no Domain, Path=/),
    // SameSite=None only with Secure, a name and value of 4096 bytes at
    // most and no controls; SameSite is kept and shown, but the client
    // sends what matches regardless (a client has no site of its own).
    //
    // Sent (§5.4): the cookies whose domain and path match the URL, Secure
    // ones over a secure origin, the longer paths first and then the older
    // ones, their last access recorded. Limits: 180 cookies a registrable
    // domain and 3000 in all by default; past one the expired go first,
    // then (within a domain) those without Secure, the least recently used
    // first.
    //
    // save() and load() keep the jar in a JSON file across runs (session
    // cookies only when asked), which Go's jar has no way to do.
    class cookie_jar {
    public:
        // The limits of a jar
        struct options {
            size_t max_cookies = 3000;               // in all
            size_t max_cookies_per_domain = 180;     // a registrable domain's
        };

        // An empty jar with the default limits
        SGCL_INLINE_HOT cookie_jar() noexcept
        : _s(make_tracked<detail::JarState>()) {
        }

        // An empty jar with the limits of o
        SGCL_INLINE_HOT explicit cookie_jar(const options& o) noexcept
        : cookie_jar() {
            _s->max_cookies = o.max_cookies;
            _s->max_cookies_per_domain = o.max_cookies_per_domain;
        }

        // The cookies a response from u set (Go's SetCookies): each stored,
        // replaced, deleted or refused by the rules above. A URL other than
        // http, https, ws and wss, or without a host, sets nothing
        void set_cookies(const net::url& u, const vector<cookie>& cookies) const noexcept {
            if (cookies.empty()) {
                return;
            }
            auto t = detail::jar_target(u);
            if (!t) {
                return;
            }
            int64_t now = time::detail::now_nanos();
            std::lock_guard<std::mutex> g(_s->lock);
            for (auto& c : cookies) {
                detail::Jar::set(*_s, *t, c, now);
            }
        }

        // The cookies a request to u carries (Go's Cookies), in the order
        // they are sent, each with its domain (a dot in front for a domain
        // cookie), path, expiry and flags
        vector<cookie> cookies(const net::url& u) const noexcept {
            vector<cookie> out;
            auto t = detail::jar_target(u);
            if (!t) {
                return out;
            }
            int64_t now = time::detail::now_nanos();
            std::lock_guard<std::mutex> g(_s->lock);
            auto found = detail::Jar::matching(*_s, *t, now);
            out.reserve(found.size());
            for (auto* e : found) {
                out.push_back(detail::jar_cookie(*e));
            }
            return out;
        }

        // The value of the Cookie field of a request to u, "a=1; b=2", ""
        // when no cookie matches
        string header(const net::url& u) const noexcept {
            auto t = detail::jar_target(u);
            if (!t) {
                return string();
            }
            int64_t now = time::detail::now_nanos();
            std::lock_guard<std::mutex> g(_s->lock);
            if (_s->count == 0) {
                return string();
            }
            auto found = detail::Jar::matching(*_s, *t, now);
            if (found.empty()) {
                return string();
            }
            size_t n = 0;
            for (auto* e : found) {
                n += e->name.size() + 1 + e->value.size() + 2 + (detail::jar_quoted(e->value.view()) ? 2 : 0);
            }
            n -= 2;
            return sgcl::detail::StringAccess::filled<string>(n, [&](char* at) {
                bool first = true;
                for (auto* e : found) {
                    if (!first) {
                        *at++ = ';';
                        *at++ = ' ';
                    }
                    first = false;
                    sgcl::detail::copy_bytes(at, e->name.data(), e->name.size());
                    at += e->name.size();
                    *at++ = '=';
                    bool quote = detail::jar_quoted(e->value.view());
                    if (quote) {
                        *at++ = '"';
                    }
                    sgcl::detail::copy_bytes(at, e->value.data(), e->value.size());
                    at += e->value.size();
                    if (quote) {
                        *at++ = '"';
                    }
                }
            });
        }

        // Every cookie of the jar that has not expired, by domain and then
        // in the order they were created
        vector<cookie> all() const noexcept {
            vector<const detail::JarEntry*> list;
            vector<cookie> out;
            int64_t now = time::detail::now_nanos();
            std::lock_guard<std::mutex> g(_s->lock);
            for (auto& kv : _s->buckets) {
                for (auto& e : kv.second) {
                    if (!e.persistent || e.expiry > now) {
                        list.push_back(&e);
                    }
                }
            }
            std::sort(list.begin(), list.end(), [](const detail::JarEntry* a, const detail::JarEntry* b) {
                int c = a->domain.view().compare(b->domain.view());
                return c != 0 ? c < 0 : detail::Jar::created_before(*a, *b);
            });
            out.reserve(list.size());
            for (auto* e : list) {
                out.push_back(detail::jar_cookie(*e));
            }
            return out;
        }

        // The cookies held, the expired among them until they are found
        SGCL_INLINE_HOT size_t size() const noexcept {
            std::lock_guard<std::mutex> g(_s->lock);
            return _s->count;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return size() == 0;
        }

        // The cookie of the name and path set for domain (a host-only one of
        // that host and a domain cookie of it both; a dot in front is
        // ignored); whether one was there
        bool remove(const string& domain, const string& path, const string& name) const noexcept {
            auto d = detail::jar_domain(domain.view());
            if (!d || d->empty()) {
                return false;
            }
            std::lock_guard<std::mutex> g(_s->lock);
            bool any = false;
            for (auto& kv : _s->buckets) {
                auto& list = kv.second;
                for (size_t i = 0; i < list.size();) {
                    if (list[i].domain.view() == *d && list[i].path == path && list[i].name == name) {
                        list.erase(list.begin() + i);
                        --_s->count;
                        any = true;
                    } else {
                        ++i;
                    }
                }
            }
            detail::Jar::drop_empty(*_s);
            return any;
        }

        // Every cookie of domain and of the names under it ("example.com":
        // www.example.com's too); how many
        size_t remove(const string& domain) const noexcept {
            auto d = detail::jar_domain(domain.view());
            if (!d || d->empty()) {
                return 0;
            }
            std::lock_guard<std::mutex> g(_s->lock);
            size_t gone = 0;
            for (auto& kv : _s->buckets) {
                auto& list = kv.second;
                for (size_t i = 0; i < list.size();) {
                    auto address = ip_address::parse(list[i].domain);
                    if (detail::domain_match(list[i].domain.view(), *d, address.has_value())) {
                        list.erase(list.begin() + i);
                        --_s->count;
                        ++gone;
                    } else {
                        ++i;
                    }
                }
            }
            detail::Jar::drop_empty(*_s);
            return gone;
        }

        // Every cookie out
        SGCL_INLINE_HOT void clear() const noexcept {
            std::lock_guard<std::mutex> g(_s->lock);
            _s->buckets.clear();
            _s->count = 0;
        }

        // The expired cookies out (the jar drops them as it finds them
        // anyway); how many
        size_t clear_expired() const noexcept {
            int64_t now = time::detail::now_nanos();
            std::lock_guard<std::mutex> g(_s->lock);
            size_t gone = 0;
            for (auto& kv : _s->buckets) {
                gone += detail::Jar::purge(*_s, kv.second, now);
            }
            detail::Jar::drop_empty(*_s);
            return gone;
        }

        // The session cookies out, as a browser's restart does; how many
        size_t clear_session() const noexcept {
            std::lock_guard<std::mutex> g(_s->lock);
            size_t gone = 0;
            for (auto& kv : _s->buckets) {
                auto& list = kv.second;
                size_t before = list.size();
                for (size_t i = 0; i < list.size();) {
                    if (!list[i].persistent) {
                        list.erase(list.begin() + i);
                    } else {
                        ++i;
                    }
                }
                gone += before - list.size();
            }
            _s->count -= gone;
            detail::Jar::drop_empty(*_s);
            return gone;
        }

        // The jar as JSON text: the persistent cookies that have not
        // expired, and the session ones too when session_cookies is true
        string to_json(bool session_cookies = false) const noexcept {
            detail::JarFile file;
            file.version = 1;
            int64_t now = time::detail::now_nanos();
            {
                std::lock_guard<std::mutex> g(_s->lock);
                vector<const detail::JarEntry*> list;
                for (auto& kv : _s->buckets) {
                    for (auto& e : kv.second) {
                        if (e.persistent ? e.expiry > now : session_cookies) {
                            list.push_back(&e);
                        }
                    }
                }
                std::sort(list.begin(), list.end(), [](const detail::JarEntry* a, const detail::JarEntry* b) {
                    int c = a->domain.view().compare(b->domain.view());
                    return c != 0 ? c < 0 : detail::Jar::created_before(*a, *b);
                });
                file.cookies.reserve(list.size());
                for (auto* e : list) {
                    detail::JarRecord r;
                    r.name = e->name;
                    if (e->value.is_valid_utf8()) {
                        r.value = e->value;
                    } else {
                        r.value_hex = encoding::hex::encode(e->value);
                    }
                    r.domain = e->domain;
                    r.host_only = e->host_only;
                    if (e->path.is_valid_utf8()) {
                        r.path = e->path;
                    } else {
                        r.path_hex = encoding::hex::encode(e->path);
                    }
                    if (e->persistent) {
                        r.expires = detail::jar_time(e->expiry);
                    }
                    r.secure = e->secure;
                    r.http_only = e->http_only;
                    r.same_site = detail::same_site_text(e->same_site);
                    r.partitioned = e->partitioned;
                    r.created = detail::jar_time(e->created);
                    r.last_access = detail::jar_time(e->last_access);
                    file.cookies.push_back(std::move(r));
                }
            }
            auto text = encoding::json::stringify(file, encoding::json::pretty);
            return text ? *text : string();
        }

        // The cookies of text, which to_json() or save() wrote, put in the
        // jar beside the ones there (each replacing one of its name, domain,
        // host-only flag and path), the expired left out; the limits held.
        // net::errc::invalid_cookie, and nothing put in, for a text that is
        // not such a file or holds a cookie the jar could not have stored
        expected<void, io::error> load_json(const string& text) const noexcept {
            return _load(text, "load cookie jar", string());
        }

        // to_json() written to the file at path, readable by its owner alone
        // (0600), through path + ".tmp" renamed over it, so that a crash
        // leaves the old file or the new one
        expected<void, io::error> save(const string& path, bool session_cookies = false) const {
            string part = string::concat(path, ".tmp");
            auto w = io::write_file(part, to_json(session_cookies), io::permissions(0600));
            if (!w) {
                return w;
            }
            return io::rename(part, path);
        }

        async::task<expected<void, io::error>> async_save(string path, bool session_cookies = false) const noexcept {
            return _co_save(*this, std::move(path), session_cookies);
        }

        // The cookies of the file at path, as load_json takes them; the
        // file's error (ENOENT for a jar never saved), or invalid_cookie
        expected<void, io::error> load(const string& path) const {
            auto text = io::read_text(path);
            if (!text) {
                return unexpected(text.error());
            }
            return _load(*text, "load cookie jar", path);
        }

        async::task<expected<void, io::error>> async_load(string path) const noexcept {
            return _co_load(*this, std::move(path));
        }

    private:
        friend struct detail::CookieJarAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT cookie_jar(sgcl::detail::FromWord, const tracked_ptr<detail::JarState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::JarState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::JarState>& _handle_word() const noexcept {
            return _s;
        }

        static async::task<expected<void, io::error>> _co_save(cookie_jar jar, string path, bool session_cookies) noexcept {
            string part = string::concat(path, ".tmp");
            auto w = co_await io::async_write_file(part, jar.to_json(session_cookies), io::permissions(0600));
            if (!w) {
                co_return w;
            }
            co_return co_await io::async_rename(part, path);
        }

        static async::task<expected<void, io::error>> _co_load(cookie_jar jar, string path) noexcept {
            auto text = co_await io::async_read_text(path);
            if (!text) {
                co_return unexpected(text.error());
            }
            co_return jar._load(*text, "load cookie jar", path);
        }

        expected<void, io::error> _load(const string& text, const char* op, const string& path) const noexcept {
            auto bad = [&] {
                return unexpected(net::detail::net_error(net::errc::invalid_cookie, op, path));
            };
            encoding::json::options o;
            o.max_depth = 8;
            auto file = encoding::json::parse<detail::JarFile>(text, o);
            if (!file || file->version != 1) {
                return bad();
            }
            int64_t now = time::detail::now_nanos();
            vector<detail::JarEntry> entries;
            entries.reserve(file->cookies.size());
            for (auto& r : file->cookies) {
                auto e = detail::jar_entry(r, now);
                if (!e) {
                    return bad();
                }
                e->sequence = entries.size();   // the file's order, for the ties
                entries.push_back(std::move(*e));
            }
            // in the order they were created, so that the jar's order of
            // creation is theirs (std::sort: stable_sort's buffer is no
            // place for the entries' strings)
            std::sort(entries.begin(), entries.end(), [](const detail::JarEntry& a, const detail::JarEntry& b) {
                return a.created != b.created ? a.created < b.created : a.sequence < b.sequence;
            });
            std::lock_guard<std::mutex> g(_s->lock);
            for (auto& e : entries) {
                if (e.persistent && e.expiry <= now) {
                    continue;
                }
                e.sequence = _s->next_sequence++;
                auto address = ip_address::parse(e.domain);
                string bucket(detail::jar_bucket(e.domain.view(), address.has_value()));
                detail::Jar::store(*_s, std::move(e), bucket, now, true);
            }
            return {};
        }

        tracked_ptr<detail::JarState> _s;
    };

    namespace detail {
        SGCL_INLINE_HOT const tracked_ptr<JarState>& CookieJarAccess::state(const cookie_jar& j) noexcept {
            return j._s;
        }
    }
}
