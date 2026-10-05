//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "headers.h"
#include "detail/parser.h"
#include "../error.h"
#include "../ip.h"
#include "../url.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../encoding/base64.h"
#include "../../io/error.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

// The proxies of the HTTP client: which one a request goes through (the
// environment's http_proxy, HTTPS_PROXY, ALL_PROXY and NO_PROXY by default,
// as curl and Go read them, or one given), and the pieces the client's
// dial is made of: the proxy's URL read, NO_PROXY's list matched, the
// CONNECT of a tunnel written and its answer read (RFC 9110 §9.3.6).
namespace sgcl::net::http {
    namespace detail {
        using namespace sgcl::detail;

        enum class ProxyKind : uint8_t { http, https, socks5, socks5h };

        // A proxy's URL as the client uses it: the kind, where it is, the
        // credentials (percent-decoded), the key its connections are
        // pooled under, and how it is named in an error (no credentials)
        struct ProxyTarget {
            ProxyKind kind = ProxyKind::http;
            string host;          // as written: an IPv6 address in its brackets
            uint16_t port = 0;
            string username;
            string password;
            string key;           // "http://user:password@host:port"
            string shown;         // "http://host:port"

            SGCL_INLINE_HOT bool has_credentials() const noexcept {
                return !username.empty() || !password.empty();
            }

            // "host:port", the address dialed
            SGCL_INLINE_HOT string address() const noexcept {
                return string::concat(host, ':', std::to_string(port));
            }

            // Basic credentials (RFC 7617) of the Proxy-Authorization field
            string basic() const {
                return string::concat("Basic ", encoding::base64::standard.encode(string::concat(username, ':', password)));
            }
        };

        inline io::error proxy_url_error(errc e, const string& text) noexcept {
            return net::detail::net_error(e, "proxy", text);
        }

        // A proxy's URL: http://, https://, socks5:// (the target resolved
        // here) or socks5h:// (resolved by the proxy), the scheme http://
        // when none is written (curl and Go take "host:port" so), the port
        // the scheme's (80, 443, 1080) when none is, user:password@ the
        // credentials; a path, a query and a fragment are ignored. A text
        // that is not a URL, has no host or names another scheme is an
        // error: invalid_url, unsupported_scheme
        inline expected<ProxyTarget, io::error> parse_proxy_url(const string& text) noexcept {
            std::string_view v = text.view();
            string full = v.find("://") == std::string_view::npos ? string::concat("http://", v) : text;
            auto u = net::url::parse(full);
            if (!u || !u->has_host() || net::detail::UrlAccess::host_as_written(*u).empty()) {
                return unexpected(proxy_url_error(errc::invalid_url, text));
            }
            ProxyTarget t;
            auto scheme = u->scheme();
            uint16_t default_port = 0;
            if (scheme == "http") {
                t.kind = ProxyKind::http;
                default_port = 80;
            } else if (scheme == "https") {
                t.kind = ProxyKind::https;
                default_port = 443;
            } else if (scheme == "socks5") {
                t.kind = ProxyKind::socks5;
                default_port = 1080;
            } else if (scheme == "socks5h") {
                t.kind = ProxyKind::socks5h;
                default_port = 1080;
            } else {
                return unexpected(proxy_url_error(errc::unsupported_scheme, text));
            }
            t.host = net::detail::UrlAccess::host_as_written(*u);
            t.port = u->port() ? *u->port() : default_port;
            if (t.port == 0) {
                return unexpected(proxy_url_error(errc::invalid_url, text));
            }
            t.username = string(std::string_view(net::detail::url_unescape(u->username().view())));
            t.password = string(std::string_view(net::detail::url_unescape(u->password().view())));
            std::string port = std::to_string(t.port);
            t.shown = string::concat(scheme, "://", t.host, ':', port);
            t.key = t.has_credentials() ? string::concat(scheme, "://", u->username(), ':', u->password(), '@', t.host, ':', port) : t.shown;
            return t;
        }

        // One entry of NO_PROXY against the host (a name, or an address
        // without brackets) and the port of a request
        inline bool no_proxy_entry(std::string_view e, std::string_view host, const optional<ip_address>& address, uint16_t port) noexcept {
            if (e == "*") {
                return true;
            }
            if (e.find('/') != std::string_view::npos) {   // an IP network, CIDR: an address in it
                if (!address) {
                    return false;
                }
                auto n = net::detail::parse_network(string(e));
                return n && n->contains(address->unmap());
            }
            // an address alone ("10.1.2.3", "::1", "[::1]"), or with a port
            std::string_view name = e;
            optional<uint16_t> want_port;
            if (auto a = net::detail::IpText::parse(e)) {   // a bare IPv6 address has colons of its own
                return address && a->unmap() == address->unmap();
            }
            if (!e.empty() && e.front() == '[') {
                auto close = e.find(']');
                if (close == std::string_view::npos) {
                    return false;
                }
                name = e.substr(1, close - 1);
                auto rest = e.substr(close + 1);
                if (!rest.empty()) {
                    if (rest.front() != ':') {
                        return false;
                    }
                    auto p = net::detail::parse_port(rest.substr(1));
                    if (!p) {
                        return false;
                    }
                    want_port = *p;
                }
            } else if (auto colon = e.rfind(':'); colon != std::string_view::npos) {
                auto p = net::detail::parse_port(e.substr(colon + 1));
                if (!p) {
                    return false;
                }
                want_port = *p;
                name = e.substr(0, colon);
            }
            if (want_port && *want_port != port) {
                return false;
            }
            if (auto a = net::detail::IpText::parse(name)) {
                return address && a->unmap() == address->unmap();
            }
            if (address) {
                return false;   // a name never matches an address
            }
            // a domain: "example.com", ".example.com" and "*.example.com"
            // each match the name and every name under it, as curl reads
            // them; a dot at the end of either is not counted
            if (name.size() >= 2 && name[0] == '*' && name[1] == '.') {
                name.remove_prefix(2);
            } else if (!name.empty() && name[0] == '.') {
                name.remove_prefix(1);
            }
            if (!name.empty() && name.back() == '.') {
                name.remove_suffix(1);
            }
            if (name.empty()) {
                return false;
            }
            if (host.size() == name.size()) {
                return iequal(host, name);
            }
            return host.size() > name.size() && host[host.size() - name.size() - 1] == '.' && iequal(host.substr(host.size() - name.size()), name);
        }

        // Whether NO_PROXY's list names the host of a request: entries
        // apart by commas or white space, each a name with what is under
        // it, an address, an IP network, any of them with a port, or "*"
        // for every host. Nothing is resolved: an address matches only a
        // URL that is written with one
        inline bool no_proxy_matches(std::string_view list, std::string_view host, uint16_t port) noexcept {
            if (list.empty()) {
                return false;
            }
            if (!host.empty() && host.back() == '.') {
                host.remove_suffix(1);
            }
            if (host.size() >= 2 && host.front() == '[' && host.back() == ']') {
                host = host.substr(1, host.size() - 2);
            }
            optional<ip_address> address = net::detail::IpText::parse(host);
            size_t i = 0;
            while (i < list.size()) {
                while (i < list.size() && (list[i] == ',' || list[i] == ' ' || list[i] == '\t')) {
                    ++i;
                }
                size_t j = i;
                while (j < list.size() && list[j] != ',' && list[j] != ' ' && list[j] != '\t') {
                    ++j;
                }
                if (j > i && no_proxy_entry(list.substr(i, j - i), host, address, port)) {
                    return true;
                }
                i = j;
            }
            return false;
        }

        // The environment's value of the first name set and not empty
        SGCL_INLINE_HOT string env_first(const char* a, const char* b = nullptr, const char* c = nullptr, const char* d = nullptr) noexcept {
            for (const char* name : {a, b, c, d}) {
                if (!name) {
                    break;
                }
                const char* v = std::getenv(name);
                if (v && *v) {
                    return string(v);
                }
            }
            return string();
        }

        // The CONNECT of a tunnel to "host:port" (RFC 9110 §9.3.6): the
        // request-target in authority-form and Host the same
        // (RFC 9112 §3.2.3), the proxy's credentials when there are
        inline std::string connect_request(const string& authority, const string& authorization) noexcept {
            std::string s;
            s.reserve(64 + 2 * authority.size() + authorization.size());
            s += "CONNECT ";
            s += authority.view();
            s += " HTTP/1.1\r\nHost: ";
            s += authority.view();
            s += "\r\n";
            if (!authorization.empty()) {
                s += "Proxy-Authorization: ";
                s += authorization.view();
                s += "\r\n";
            }
            s += "\r\n";
            return s;
        }

        // What a proxy answered a CONNECT: more bytes wanted, the tunnel up
        // (a 2xx, its head `head` bytes long), credentials wanted (407),
        // the tunnel refused (another status), or an answer that is not a
        // response. A 2xx ends at its head (RFC 9110 §9.3.6: no content,
        // whatever Content-Length says); bytes past it, which the proxy
        // had no reason to send before the client spoke, are malformed
        struct ConnectAnswer {
            enum class kind : uint8_t { more, established, auth_required, refused, malformed };
            kind result = kind::more;
            int status = 0;
            string reason;
            size_t head = 0;
        };

        inline ConnectAnswer read_connect_answer(std::string_view bytes, size_t max_head) noexcept {
            ConnectAnswer a;
            size_t end = find_head_end(bytes.data(), bytes.size());
            if (end == 0) {
                a.result = bytes.size() >= max_head ? ConnectAnswer::kind::malformed : ConnectAnswer::kind::more;
                return a;
            }
            if (end > max_head) {
                a.result = ConnectAnswer::kind::malformed;
                return a;
            }
            string head(bytes.substr(0, end));
            StatusLine line;
            headers fields;
            if (parse_response_head(head, line, fields)) {
                a.result = ConnectAnswer::kind::malformed;
                return a;
            }
            a.status = line.status;
            a.reason = string(head.view().substr(line.reason_at, line.reason_size));
            a.head = end;
            if (line.status >= 200 && line.status < 300) {
                a.result = end == bytes.size() ? ConnectAnswer::kind::established : ConnectAnswer::kind::malformed;
            } else if (line.status == 407) {
                a.result = ConnectAnswer::kind::auth_required;
            } else if (line.status < 200) {
                a.result = ConnectAnswer::kind::malformed;   // no 1xx before a tunnel: nothing was asked to continue
            } else {
                a.result = ConnectAnswer::kind::refused;
            }
            return a;
        }
    }

    // Which proxy a request of the client goes through: the URL of the
    // proxy of http:// requests and of https:// ones (empty: direct), and
    // NO_PROXY's list of hosts reached directly. A value of three strings,
    // read by each request; the URLs are checked when a request uses them.
    struct proxy {
        string http;       // http://, https://, socks5://, socks5h://, user:password@ for the credentials; empty: direct
        string https;
        string no_proxy;   // "localhost,.internal,10.0.0.0/8,example.com:8080", "*" for every host

        // None: every request direct
        proxy() noexcept = default;

        // Every request through the one at url, http:// and https:// alike
        SGCL_INLINE_HOT explicit proxy(const string& url) noexcept
        : http(url)
        , https(url) {
        }

        // The environment's, as curl reads it: http_proxy (in lower case
        // only: a CGI program gets a request's Proxy field as HTTP_PROXY)
        // for http://, https_proxy or HTTPS_PROXY for https://, all_proxy
        // or ALL_PROXY for either when its own is not set, no_proxy or
        // NO_PROXY; the lower case first, an empty value as none
        static proxy from_environment() noexcept {
            proxy p;
            string all = detail::env_first("all_proxy", "ALL_PROXY");
            p.http = detail::env_first("http_proxy");
            if (p.http.empty()) {
                p.http = all;
            }
            p.https = detail::env_first("https_proxy", "HTTPS_PROXY");
            if (p.https.empty()) {
                p.https = all;
            }
            p.no_proxy = detail::env_first("no_proxy", "NO_PROXY");
            return p;
        }

        // The proxy a request to target goes through: its URL (with the
        // scheme and port it is taken with), none for a direct one, or the
        // error of a URL that is not a proxy's (net::errc::invalid_url,
        // unsupported_scheme)
        expected<optional<net::url>, io::error> for_url(const net::url& target) const noexcept {
            auto scheme = target.scheme();
            const string& chosen = scheme == "https" ? https : scheme == "http" ? http : string();
            if (chosen.empty() || detail::no_proxy_matches(no_proxy.view(), net::detail::UrlAccess::host_as_written(target).view(), target.effective_port())) {
                return optional<net::url>();
            }
            auto t = detail::parse_proxy_url(chosen);
            if (!t) {
                return unexpected(t.error());
            }
            auto u = net::url::parse(t->key);
            if (!u) {
                return unexpected(detail::proxy_url_error(net::errc::invalid_url, chosen));
            }
            return optional<net::url>(std::move(*u));
        }
    };
}
