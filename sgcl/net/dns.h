//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/dns_resolver.h"
#include "detail/system_resolver.h"
#include "error.h"
#include "ip.h"
#include "../async/blocking.h"
#include "../async/coroutine.h"
#include "../async/stop_token.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/string.h"
#include "../encoding/base64.h"

namespace sgcl::net {
    namespace detail {
        using namespace sgcl::detail;
        struct DnsAccess;
    }
    // Names to addresses and back, and the records of a name.
    //
    // The addresses (lookup, reverse_lookup) go through the system's
    // resolver (getaddrinfo, getnameinfo): /etc/hosts, the search domains,
    // mDNS, the resolvers a VPN or a profile installs, and the system's
    // cache, which on macOS nothing else answers correctly. The call
    // blocks in the C library, so the async forms run it on the blocking
    // pool (spawn_blocking), bounded by config::blocking_threads; a stop of
    // the token ends the wait with ECANCELED, and the thread finishes the
    // call on its own. A numeric address is answered at once, with no pool
    // and no resolver.
    //
    // The records (lookup_mx, lookup_txt, lookup_srv, lookup_ns,
    // lookup_cname, lookup_https, lookup_svcb) go through the module's own stub resolver
    // (detail/dns_resolver.h): queries over UDP on the scheduler's sockets,
    // TCP for an answer truncated, to the servers of /etc/resolv.conf (its
    // search list, ndots, timeout, attempts, rotate) or to the one the
    // options name; the local machine's (127.0.0.1, ::1) when the file
    // names none. No cache, as in Go. A name outside ASCII is converted
    // first (txt::idna::to_ascii): dns does not guess the profile.
    struct dns {
        // A mail exchanger: its host, absolute ("mx1.example.com."), and its
        // preference, the lower the first (RFC 1035 §3.3.9, RFC 5321 §5.1).
        // A host "." is RFC 7505's null MX: the domain takes no mail
        struct mx {
            string host;
            uint16_t preference = 0;

            friend bool operator==(const mx&, const mx&) noexcept = default;
        };

        // A service's server (RFC 2782): its target, absolute, its port, its
        // priority (the lower the first) and its weight among the targets
        // of one priority. A target "." means the service is not offered
        struct srv {
            string target;
            uint16_t port = 0;
            uint16_t priority = 0;
            uint16_t weight = 0;

            friend bool operator==(const srv&, const srv&) noexcept = default;
        };

        // A SvcParam of a key svcb has no field for (RFC 9460 §14.3:
        // ohttp, tls-supported-groups, the keys of later RFCs, key65280…
        // of private use): its key and its value's bytes as they came
        struct svc_param {
            uint16_t key = 0;
            vector<byte> value;

            friend bool operator==(const svc_param&, const svc_param&) noexcept = default;
        };

        // A service binding (RFC 9460): an HTTPS or SVCB record in
        // ServiceMode, the endpoint of a service and how to reach it. Its
        // priority (the lower the first), its target, absolute (a record's
        // "." already its owner's name, §2.5.2), and its SvcParams typed:
        // the protocols (ALPN ids), whether the scheme's default protocol
        // is left out, the port (0: the scheme's), the addresses to try
        // before the target's own A and AAAA answer, the ECHConfigList of
        // Encrypted Client Hello (RFC 9848; tls::config::ech_config_list
        // takes it), DoH's URI template (RFC 9461), the keys a client must
        // understand to use the record; every other key raw. Priority 0
        // only when an alias led to a name with no records: connect to the
        // target as without a record (§2.4.2)
        struct svcb {
            uint16_t priority = 0;
            string target;
            vector<string> alpn;
            bool no_default_alpn = false;
            uint16_t port = 0;
            vector<ip_address> ipv4_hints;
            vector<ip_address> ipv6_hints;
            vector<byte> ech;
            string dohpath;
            vector<uint16_t> mandatory;
            vector<svc_param> params;

            friend bool operator==(const svcb&, const svcb&) noexcept = default;
        };

        // A server to ask: its address with the transport as a scheme, and
        // for TLS the name its certificate is checked for and the pins of
        // its keys. "10.0.0.1", "[::1]:5353", "udp://10.0.0.1" over UDP
        // (TCP for a truncated answer, port 53); "tcp://10.0.0.1" over TCP
        // alone; "tls://1.1.1.1", "tls://dns.google:853" DNS over TLS
        // (RFC 7858, port 853); "https://cloudflare-dns.com/dns-query" DNS
        // over HTTPS (RFC 8484). Made from its text where a server is
        // wanted: options::servers = {"tls://1.1.1.1"}
        struct server {
            string address;
            string name;            // tls, https: the name checked; empty: the address's host
            vector<string> pins;    // tls: base64 SHA-256 of a key of the chain (RFC 7858 §4.2); any matches

            server() noexcept = default;

            SGCL_INLINE_HOT server(const char* text) noexcept
            : address(text) {
            }

            SGCL_INLINE_HOT server(const string& text) noexcept
            : address(text) {
            }

            friend bool operator==(const server&, const server&) noexcept = default;
        };

        // Public resolvers, in their own words: plain, over TLS, over HTTPS
        static constexpr const char* cloudflare = "1.1.1.1";
        static constexpr const char* cloudflare_tls = "tls://1.1.1.1";
        static constexpr const char* cloudflare_https = "https://cloudflare-dns.com/dns-query";
        static constexpr const char* google = "8.8.8.8";
        static constexpr const char* google_tls = "tls://8.8.8.8";
        static constexpr const char* google_https = "https://dns.google/dns-query";
        static constexpr const char* quad9 = "9.9.9.9";
        static constexpr const char* quad9_tls = "tls://9.9.9.9";
        static constexpr const char* quad9_https = "https://dns.quad9.net/dns-query";

        // How a lookup goes through the module's own resolver: the servers
        // to ask in place of /etc/resolv.conf's, in order; the wait for one
        // answer of one server, and the rounds over the servers (zero is
        // the file's: 5 s, 2 rounds by default); whether TLS may go
        // unauthenticated and fall back to clear text (RFC 8310's
        // opportunistic profile; strict by default); DoH by GET in place
        // of POST; the roots a TLS server's chain must lead to, as PEM
        // (empty: the system's)
        struct options {
            vector<dns::server> servers;
            duration timeout = {};
            int attempts = 0;
            bool opportunistic = false;
            bool https_get = false;
            string roots_pem;
        };

        // The addresses of the host in the resolver's order; a host with
        // none is net::errc::host_not_found, a failure of the resolver an
        // EAI_* code in net::lookup_category()
        // `dns::lookup(...)` on this thread, `co_await dns::async_lookup(...)` in a task;
        // the stop ends a task's wait. The thread's form takes none: the
        // system's resolver cannot be interrupted, so the call goes on to
        // its end
        SGCL_INLINE_HOT static expected<vector<ip_address>, io::error> lookup(const string& host) noexcept {
            return _block_lookup(host);
        }

        SGCL_INLINE_HOT static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host, async::stop_token stop = {}) noexcept {
            return _co_lookup(host, stop);
        }

        // The addresses through the module's own resolver, its A and AAAA
        // records asked at once (the IPv4 addresses first): the servers of
        // the options, over UDP, TLS or HTTPS; a name under .local by
        // multicast DNS (RFC 6762)
        SGCL_INLINE_HOT static expected<vector<ip_address>, io::error> lookup(const string& host, const options& o) {
            return _co_lookup_ip(host, o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<vector<ip_address>, io::error>> async_lookup(const string& host, const options& o, async::stop_token stop = {}) noexcept {
            return _co_lookup_ip(host, o, std::move(stop));
        }

        // The names of the address (getnameinfo: one, the system's); an
        // address with none is net::errc::host_not_found
        // `dns::reverse_lookup(...)` on this thread, `co_await dns::async_reverse_lookup(...)` in a task;
        // the stop ends a task's wait (the thread's form takes none, as lookup)
        SGCL_INLINE_HOT static expected<vector<string>, io::error> reverse_lookup(const ip_address& address) noexcept {
            return _block_reverse_lookup(address);
        }

        SGCL_INLINE_HOT static async::task<expected<vector<string>, io::error>> async_reverse_lookup(const ip_address& address, async::stop_token stop = {}) noexcept {
            return _co_reverse_lookup(address, stop);
        }

        // The names of the address through the module's own resolver: its
        // PTR records (in-addr.arpa, ip6.arpa; RFC 1035 §3.5, RFC 3596
        // §2.5), absolute, in the answer's order
        SGCL_INLINE_HOT static expected<vector<string>, io::error> reverse_lookup(const ip_address& address, const options& o) {
            return _co_lookup_texts(_reverse_name(address), net::detail::dns_type::ptr, o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<vector<string>, io::error>> async_reverse_lookup(const ip_address& address, const options& o, async::stop_token stop = {}) noexcept {
            return _co_lookup_texts(_reverse_name(address), net::detail::dns_type::ptr, o, std::move(stop));
        }

        // The mail exchangers of the domain, by preference, the ones of one
        // preference in a random order (RFC 5321 §5.1): Go's LookupMX
        // `dns::lookup_mx(...)` on this thread, `co_await dns::async_lookup_mx(...)` in a task
        SGCL_INLINE_HOT static expected<vector<mx>, io::error> lookup_mx(const string& name) {
            return _co_lookup_mx(name, options(), async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static expected<vector<mx>, io::error> lookup_mx(const string& name, const options& o) {
            return _co_lookup_mx(name, o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<vector<mx>, io::error>> async_lookup_mx(const string& name, async::stop_token stop = {}) noexcept {
            return _co_lookup_mx(name, options(), std::move(stop));
        }

        SGCL_INLINE_HOT static async::task<expected<vector<mx>, io::error>> async_lookup_mx(const string& name, const options& o, async::stop_token stop = {}) noexcept {
            return _co_lookup_mx(name, o, std::move(stop));
        }

        // The TXT records of the name, in the answer's order, each the
        // bytes of its strings joined: Go's LookupTXT
        // `dns::lookup_txt(...)` on this thread, `co_await dns::async_lookup_txt(...)` in a task
        SGCL_INLINE_HOT static expected<vector<string>, io::error> lookup_txt(const string& name) {
            return _co_lookup_texts(name, detail::dns_type::txt, options(), async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static expected<vector<string>, io::error> lookup_txt(const string& name, const options& o) {
            return _co_lookup_texts(name, detail::dns_type::txt, o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<vector<string>, io::error>> async_lookup_txt(const string& name, async::stop_token stop = {}) noexcept {
            return _co_lookup_texts(name, detail::dns_type::txt, options(), std::move(stop));
        }

        SGCL_INLINE_HOT static async::task<expected<vector<string>, io::error>> async_lookup_txt(const string& name, const options& o, async::stop_token stop = {}) noexcept {
            return _co_lookup_texts(name, detail::dns_type::txt, o, std::move(stop));
        }

        // The servers of _service._proto.name in RFC 2782's order (by
        // priority, by weight at random within one): Go's LookupSRV; with
        // service and proto both empty, of the name itself
        // `dns::lookup_srv(...)` on this thread, `co_await dns::async_lookup_srv(...)` in a task
        SGCL_INLINE_HOT static expected<vector<srv>, io::error> lookup_srv(const string& service, const string& proto, const string& name) {
            return _co_lookup_srv(_srv_name(service, proto, name), options(), async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static expected<vector<srv>, io::error> lookup_srv(const string& service, const string& proto, const string& name, const options& o) {
            return _co_lookup_srv(_srv_name(service, proto, name), o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<vector<srv>, io::error>> async_lookup_srv(const string& service, const string& proto, const string& name, async::stop_token stop = {}) noexcept {
            return _co_lookup_srv(_srv_name(service, proto, name), options(), std::move(stop));
        }

        SGCL_INLINE_HOT static async::task<expected<vector<srv>, io::error>> async_lookup_srv(const string& service, const string& proto, const string& name, const options& o, async::stop_token stop = {}) noexcept {
            return _co_lookup_srv(_srv_name(service, proto, name), o, std::move(stop));
        }

        // The name servers of the domain, absolute, in the answer's order:
        // Go's LookupNS
        // `dns::lookup_ns(...)` on this thread, `co_await dns::async_lookup_ns(...)` in a task
        SGCL_INLINE_HOT static expected<vector<string>, io::error> lookup_ns(const string& name) {
            return _co_lookup_texts(name, detail::dns_type::ns, options(), async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static expected<vector<string>, io::error> lookup_ns(const string& name, const options& o) {
            return _co_lookup_texts(name, detail::dns_type::ns, o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<vector<string>, io::error>> async_lookup_ns(const string& name, async::stop_token stop = {}) noexcept {
            return _co_lookup_texts(name, detail::dns_type::ns, options(), std::move(stop));
        }

        SGCL_INLINE_HOT static async::task<expected<vector<string>, io::error>> async_lookup_ns(const string& name, const options& o, async::stop_token stop = {}) noexcept {
            return _co_lookup_texts(name, detail::dns_type::ns, o, std::move(stop));
        }

        // The canonical name, absolute: the end of the CNAME chain from the
        // name, or the name itself when it is no alias (Go's LookupCNAME)
        // `dns::lookup_cname(...)` on this thread, `co_await dns::async_lookup_cname(...)` in a task
        SGCL_INLINE_HOT static expected<string, io::error> lookup_cname(const string& name) {
            return _co_lookup_cname(name, options(), async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static expected<string, io::error> lookup_cname(const string& name, const options& o) {
            return _co_lookup_cname(name, o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<string, io::error>> async_lookup_cname(const string& name, async::stop_token stop = {}) noexcept {
            return _co_lookup_cname(name, options(), std::move(stop));
        }

        SGCL_INLINE_HOT static async::task<expected<string, io::error>> async_lookup_cname(const string& name, const options& o, async::stop_token stop = {}) noexcept {
            return _co_lookup_cname(name, o, std::move(stop));
        }

        // The HTTPS records of the name (RFC 9460 §9): the service bindings
        // of https://name, by priority, those of one priority in a random
        // order (§2.4.1); AliasMode followed to the records of its target.
        // The name for another port than 443 is "_8443._https.name" (§9.1)
        // `dns::lookup_https(...)` on this thread, `co_await dns::async_lookup_https(...)` in a task
        SGCL_INLINE_HOT static expected<vector<svcb>, io::error> lookup_https(const string& name) {
            return _co_lookup_svcb(name, net::detail::dns_type::https, options(), async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static expected<vector<svcb>, io::error> lookup_https(const string& name, const options& o) {
            return _co_lookup_svcb(name, net::detail::dns_type::https, o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<vector<svcb>, io::error>> async_lookup_https(const string& name, async::stop_token stop = {}) noexcept {
            return _co_lookup_svcb(name, net::detail::dns_type::https, options(), std::move(stop));
        }

        SGCL_INLINE_HOT static async::task<expected<vector<svcb>, io::error>> async_lookup_https(const string& name, const options& o, async::stop_token stop = {}) noexcept {
            return _co_lookup_svcb(name, net::detail::dns_type::https, o, std::move(stop));
        }

        // The SVCB records of the name, as a service's protocol names it
        // ("_dns.resolver.arpa" of RFC 9462, "_8443._foo.api.example.com"
        // of RFC 9460 §2.3): as lookup_https, of type SVCB
        // `dns::lookup_svcb(...)` on this thread, `co_await dns::async_lookup_svcb(...)` in a task
        SGCL_INLINE_HOT static expected<vector<svcb>, io::error> lookup_svcb(const string& name) {
            return _co_lookup_svcb(name, net::detail::dns_type::svcb, options(), async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static expected<vector<svcb>, io::error> lookup_svcb(const string& name, const options& o) {
            return _co_lookup_svcb(name, net::detail::dns_type::svcb, o, async::stop_token()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<vector<svcb>, io::error>> async_lookup_svcb(const string& name, async::stop_token stop = {}) noexcept {
            return _co_lookup_svcb(name, net::detail::dns_type::svcb, options(), std::move(stop));
        }

        SGCL_INLINE_HOT static async::task<expected<vector<svcb>, io::error>> async_lookup_svcb(const string& name, const options& o, async::stop_token stop = {}) noexcept {
            return _co_lookup_svcb(name, net::detail::dns_type::svcb, o, std::move(stop));
        }

    private:
        friend struct net::detail::DnsAccess;

        // A name under .local the system's resolver does not know (a Linux
        // without nss-mdns) asked again by the module's multicast DNS; on
        // macOS mDNSResponder answers those itself, and is left alone
        SGCL_INLINE_HOT static bool _local_fallback(const string& host, const expected<vector<ip_address>, io::error>& r) noexcept {
#if defined(__APPLE__)
            (void)host;
            (void)r;
            return false;
#else
            std::string_view v = host.view();
            if (!v.empty() && v.back() == '.') {
                v.remove_suffix(1);
            }
            return !r && r.error().code() == errc::host_not_found && v.size() > 6 && v.ends_with(".local");
#endif
        }

        // the two halves of the operations above: a thread's and a task's
        SGCL_INLINE_HOT static expected<vector<ip_address>, io::error> _block_lookup(const string& host) noexcept {
            auto r = net::detail::lookup_now(host);
            if (_local_fallback(host, r)) {
                return _co_lookup_ip(host, options(), async::stop_token()).wait();
            }
            return r;
        }

#if defined(__APPLE__)
        SGCL_INLINE_HOT static async::task<expected<vector<ip_address>, io::error>> _co_lookup(const string& host, async::stop_token stop) noexcept {
            return net::detail::lookup_until(host, std::move(stop), time_point());
        }
#else
        static async::task<expected<vector<ip_address>, io::error>> _co_lookup(string host, async::stop_token stop) noexcept {
            auto r = co_await net::detail::lookup_until(host, stop, time_point());
            if (_local_fallback(host, r)) {
                co_return co_await _co_lookup_ip(host, options(), std::move(stop));
            }
            co_return r;
        }
#endif

        SGCL_INLINE_HOT static expected<vector<string>, io::error> _block_reverse_lookup(const ip_address& address) noexcept {
            return net::detail::resolve_name(address);
        }

        // a coroutine, as lookup_until: the pool's job starts when the task
        // first runs, never at the call
        static async::task<expected<vector<string>, io::error>> _co_reverse_lookup(ip_address address, async::stop_token stop) noexcept {
            co_return co_await net::detail::await_job(sgcl::async::spawn_blocking([address, stop]() -> expected<vector<string>, io::error> {
                if (auto e = net::detail::given_up(stop, time_point(), address.to_string())) {
                    return io::detail::fail(*e);
                }
                return net::detail::resolve_name(address);
            }), stop, time_point(), address.to_string());
        }

        // The records' halves: the settings made of the options, the
        // resolver's answer made the public type
        static expected<net::detail::DnsSettings, io::error> _settings(const options& o) noexcept {
            net::detail::DnsSettings s;
            s.servers.reserve(o.servers.size());
            for (auto& given : o.servers) {
                auto spec = net::detail::dns_server_spec(given.address);
                if (!spec) {
                    return net::detail::fail(spec);
                }
                spec->name = given.name;
                for (auto& pin : given.pins) {
                    auto bytes = encoding::base64::standard.decode(pin);
                    if (!bytes || bytes->size() != 32) {
                        return net::detail::fail(net::detail::system_error(EINVAL, "lookup", pin));
                    }
                    std::array<uint8_t, 32> digest;
                    net::detail::copy_bytes(digest.data(), bytes->data(), 32);
                    spec->pins.push_back(digest);
                }
                s.servers.push_back(std::move(*spec));
            }
            s.timeout = o.timeout;
            s.attempts = o.attempts;
            s.opportunistic = o.opportunistic;
            s.https_get = o.https_get;
            s.roots_pem = o.roots_pem;
            return s;
        }

        // The name of an address's PTR: the four bytes of IPv4 backwards
        // under in-addr.arpa, the 32 nibbles of IPv6 under ip6.arpa
        static string _reverse_name(const ip_address& address) noexcept {
            ip_address a = address.unmap();
            auto b = a.bytes();
            std::string s;
            if (a.is_v4()) {
                for (int i = 15; i >= 12; --i) {
                    s += std::to_string(unsigned(b[size_t(i)]));
                    s += '.';
                }
                s += "in-addr.arpa.";
            } else {
                static constexpr char hex[] = "0123456789abcdef";
                for (int i = 15; i >= 0; --i) {
                    s += hex[b[size_t(i)] & 0xF];
                    s += '.';
                    s += hex[b[size_t(i)] >> 4];
                    s += '.';
                }
                s += "ip6.arpa.";
            }
            return string(std::string_view(s));
        }

        static string _srv_name(const string& service, const string& proto, const string& name) noexcept {
            if (service.empty() && proto.empty()) {
                return name;
            }
            std::string s;
            s.reserve(service.size() + proto.size() + name.size() + 4);
            s += '_';
            s += service.view();
            s += "._";
            s += proto.view();
            s += '.';
            s += name.view();
            return string(std::string_view(s));
        }

        static async::task<expected<vector<mx>, io::error>> _co_lookup_mx(string name, options o, async::stop_token stop) noexcept {
            auto settings = _settings(o);
            if (!settings) {
                co_return net::detail::fail(settings);
            }
            auto a = co_await net::detail::dns_lookup(name, net::detail::dns_type::mx, false, std::move(*settings), std::move(stop));
            if (a.status != net::detail::DnsStatus::ok) {
                co_return net::detail::fail(net::detail::dns_error(a, name));
            }
            vector<mx> out;
            out.reserve(a.records.size());
            for (auto& r : a.records) {
                out.push_back(mx{r.name, r.first});
            }
            net::detail::dns_order_mx(out);
            co_return out;
        }

        static async::task<expected<vector<srv>, io::error>> _co_lookup_srv(string name, options o, async::stop_token stop) noexcept {
            auto settings = _settings(o);
            if (!settings) {
                co_return net::detail::fail(settings);
            }
            auto a = co_await net::detail::dns_lookup(name, net::detail::dns_type::srv, false, std::move(*settings), std::move(stop));
            if (a.status != net::detail::DnsStatus::ok) {
                co_return net::detail::fail(net::detail::dns_error(a, name));
            }
            vector<srv> out;
            out.reserve(a.records.size());
            for (auto& r : a.records) {
                out.push_back(srv{r.name, r.third, r.first, r.second});
            }
            net::detail::dns_order_srv(out);
            co_return out;
        }

        // TXT's texts, NS's hosts
        static async::task<expected<vector<string>, io::error>> _co_lookup_texts(string name, uint16_t type, options o, async::stop_token stop) noexcept {
            auto settings = _settings(o);
            if (!settings) {
                co_return net::detail::fail(settings);
            }
            auto a = co_await net::detail::dns_lookup(name, type, false, std::move(*settings), std::move(stop));
            if (a.status != net::detail::DnsStatus::ok) {
                co_return net::detail::fail(net::detail::dns_error(a, name));
            }
            vector<string> out;
            out.reserve(a.records.size());
            for (auto& r : a.records) {
                out.push_back(type == net::detail::dns_type::txt ? r.text : r.name);   // NS's host, PTR's name
            }
            co_return out;
        }

        // A and AAAA at once, the second spawned beside the first; the
        // addresses of both, IPv4's first, or the error of both (NODATA
        // of both: no such host, as getaddrinfo has it)
        static async::task<expected<vector<ip_address>, io::error>> _co_lookup_ip(string host, options o, async::stop_token stop) noexcept {
            if (auto a = net::detail::IpText::parse(net::detail::IpText::view(host))) {
                vector<ip_address> one;
                one.push_back(*a);
                co_return one;
            }
            auto settings = _settings(o);
            if (!settings) {
                co_return net::detail::fail(settings);
            }
            auto six = sgcl::async::spawn(net::detail::dns_lookup(host, net::detail::dns_type::aaaa, false, *settings, stop));
            auto four = co_await net::detail::dns_lookup(host, net::detail::dns_type::a, false, std::move(*settings), stop);
            auto v6 = co_await six;
            vector<ip_address> out;
            for (auto* answer : {&four, &v6}) {
                if (answer->status != net::detail::DnsStatus::ok) {
                    continue;
                }
                for (auto& r : answer->records) {
                    if (r.address_size == 4) {
                        out.push_back(ip_address::v4(r.address[0], r.address[1], r.address[2], r.address[3]));
                    } else if (r.address_size == 16) {
                        array<uint8_t, 16> b;
                        net::detail::copy_bytes(b.data(), r.address, 16);
                        out.push_back(ip_address::v6(b));
                    }
                }
            }
            if (!out.empty()) {
                co_return out;
            }
            const auto& worse = (four.status == net::detail::DnsStatus::nodata || four.status == net::detail::DnsStatus::ok) ? v6 : four;
            if (worse.status == net::detail::DnsStatus::nodata || worse.status == net::detail::DnsStatus::ok) {
                co_return net::detail::fail(net::detail::net_error(errc::host_not_found, "lookup", host));
            }
            co_return net::detail::fail(net::detail::dns_error(worse, host));
        }

        // A ServiceMode record typed: its priority, its target ("." its
        // owner's name), its SvcParams (p, n), checked by the resolver
        static svcb _svcb_of(uint16_t priority, const string& target, const net::detail::DnsName& owner, const uint8_t* p, size_t n) noexcept {
            namespace key = net::detail::dns_svc_key;
            svcb out;
            out.priority = priority;
            out.target = target.view() == "." ? net::detail::dns_name_string(owner) : target;
            auto bytes = [](const uint8_t* v, size_t n) {
                return vector<byte>(reinterpret_cast<const byte*>(v), reinterpret_cast<const byte*>(v) + n);
            };
            net::detail::dns_svc_params_each(p, n, [&](uint16_t k, const uint8_t* v, size_t n) {
                switch (k) {
                    case key::mandatory:
                        out.mandatory.reserve(n / 2);
                        for (size_t i = 0; i < n; i += 2) {
                            out.mandatory.push_back(net::detail::dns_u16(v + i));
                        }
                        break;
                    case key::alpn:
                        for (size_t i = 0; i < n; i += size_t(v[i]) + 1) {
                            out.alpn.push_back(string(std::string_view(reinterpret_cast<const char*>(v + i + 1), v[i])));
                        }
                        break;
                    case key::no_default_alpn:
                        out.no_default_alpn = true;
                        break;
                    case key::port:
                        out.port = net::detail::dns_u16(v);
                        break;
                    case key::ipv4hint:
                        out.ipv4_hints.reserve(n / 4);
                        for (size_t i = 0; i < n; i += 4) {
                            out.ipv4_hints.push_back(ip_address::v4(v[i], v[i + 1], v[i + 2], v[i + 3]));
                        }
                        break;
                    case key::ipv6hint:
                        out.ipv6_hints.reserve(n / 16);
                        for (size_t i = 0; i < n; i += 16) {
                            array<uint8_t, 16> a;
                            net::detail::copy_bytes(a.data(), v + i, 16);
                            out.ipv6_hints.push_back(ip_address::v6(a));
                        }
                        break;
                    case key::ech:
                        out.ech = bytes(v, n);
                        break;
                    case key::dohpath:
                        out.dohpath = string(std::string_view(reinterpret_cast<const char*>(v), n));
                        break;
                    default:
                        out.params.push_back(svc_param{k, bytes(v, n)});
                }
            });
            return out;
        }

        // SVCB and HTTPS: an AliasMode record (one in an RRset is enough:
        // its ServiceMode records are then ignored, RFC 9460 §2.4.2)
        // followed to its target, at most DnsMaxHops of them; the
        // ServiceMode records of the name reached, ordered
        static async::task<expected<vector<svcb>, io::error>> _co_lookup_svcb(string name, uint16_t type, options o, async::stop_token stop) noexcept {
            auto settings = _settings(o);
            if (!settings) {
                co_return net::detail::fail(settings);
            }
            string current = name;
            for (int aliases = 0;; ++aliases) {
                auto a = co_await net::detail::dns_lookup(current, type, false, *settings, stop);
                if (a.status == net::detail::DnsStatus::nodata && aliases > 0) {
                    vector<svcb> one;   // the alias's target has no records: the target itself (§2.4.2)
                    svcb s;
                    s.target = current;
                    one.push_back(std::move(s));
                    co_return one;
                }
                if (a.status != net::detail::DnsStatus::ok) {
                    co_return net::detail::fail(net::detail::dns_error(a, name));
                }
                const net::detail::DnsRecordData* alias = nullptr;
                for (auto& r : a.records) {
                    if (r.first == 0) {
                        alias = &r;
                        break;
                    }
                }
                if (alias) {
                    if (alias->name.view() == ".") {   // the service is not offered (§2.4.2)
                        co_return net::detail::fail(net::detail::net_error(errc::no_data, "lookup", name));
                    }
                    if (aliases == net::detail::DnsMaxHops) {
                        co_return net::detail::fail(net::detail::net_error(errc::server_misbehaving, "lookup", name));
                    }
                    current = alias->name;
                    continue;
                }
                vector<svcb> out;
                out.reserve(a.records.size());
                for (auto& r : a.records) {
                    out.push_back(_svcb_of(r.first, r.name, a.end, reinterpret_cast<const uint8_t*>(r.text.data()), r.text.size()));
                }
                net::detail::dns_order_svcb(out);
                co_return out;
            }
        }

        // asked as A: a recursive server follows the chain to its end in
        // one answer, and a name that exists is its own canonical name
        static async::task<expected<string, io::error>> _co_lookup_cname(string name, options o, async::stop_token stop) noexcept {
            auto settings = _settings(o);
            if (!settings) {
                co_return net::detail::fail(settings);
            }
            auto a = co_await net::detail::dns_lookup(name, net::detail::dns_type::a, true, std::move(*settings), std::move(stop));
            if (a.status != net::detail::DnsStatus::ok) {
                co_return net::detail::fail(net::detail::dns_error(a, name));
            }
            co_return net::detail::dns_name_string(a.end);
        }
    };

    namespace detail {
        // The settings of the module's resolver made of dns::options, for
        // the checks of mail (spf, dkim, dmarc) that ask for records of
        // their own types; the typing of a ServiceMode record, for the
        // tests, the benchmark and the fuzzer (which gives the SvcParams in
        // a buffer of their own)
        struct DnsAccess {
            static expected<DnsSettings, io::error> settings(const dns::options& o) noexcept {
                return dns::_settings(o);
            }

            static dns::svcb svcb_of(uint16_t priority, const string& target, const DnsName& owner, const uint8_t* params, size_t size) noexcept {
                return dns::_svcb_of(priority, target, owner, params, size);
            }

            static dns::svcb svcb_of(const DnsRecordData& r, const DnsName& owner) noexcept {
                return dns::_svcb_of(r.first, r.name, owner, reinterpret_cast<const uint8_t*>(r.text.data()), r.text.size());
            }
        };
    }
}
