//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "dns_message.h"
#include "resolv_conf.h"
#include "../connection.h"
#include "../socket.h"
#include "../url.h"
#include "../../async/channel.h"
#include "../../async/coroutine.h"
#include "../../async/select.h"
#include "../../async/stop_token.h"
#include "../../core/array.h"
#include "../../core/make_tracked.h"
#include "../../crypto/detail/drbg.h"

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <memory>
#include <mutex>

// The stub resolver under net::dns's records (RFC 1035 §7, RFC 5452):
// a query of one name and one type to the servers of /etc/resolv.conf, or
// to the one a caller names, over UDP with the scheduler's sockets, over
// TCP when the answer comes truncated (TC, RFC 7766) or when use-vc says
// so.
//
// An exchange over UDP is a socket of its own, connected to the server,
// so that the kernel picks a random source port and drops a datagram from
// anyone else; the query's id is 16 random bits; an answer is taken only
// when it is a response with that id and echoes the question (its name,
// its type, class IN), anything else is ignored and the wait goes on to
// its deadline (RFC 5452 §9.1). A query carries an OPT record of RFC 6891
// with a UDP payload of 1232 bytes; a datagram longer than that is cut by
// the receive and taken as truncated.
//
// The servers are asked in their order (from the next one per lookup,
// with `rotate`), `attempts` rounds over them, each query waited for
// `timeout`. NXDOMAIN and NODATA are answers: the next server is not
// asked. SERVFAIL, REFUSED and the other codes, an answer that does not
// read, a lame referral (no answer, neither AA nor RA), a timeout and a
// socket's error are the server's failure, and the next one is asked.
//
// The names of the search list are tried as resolv.conf(5) has it: the
// name as given first when it has at least ndots dots, last otherwise,
// alone when it ends with a dot. The first name that answers wins; when
// none does, the error is the one the name as given came to.
//
// A CNAME is followed in the answer, from the question's name to the end
// of the chain, at most eight in all; when the chain's end has no record
// of the type in the answer, the end is asked again (a server that did
// not follow the chain itself), within the same eight.
namespace sgcl::net::detail {
    SGCL_INLINE_HOT uint16_t dns_random16() noexcept {
        unsigned char b[2];
        crypto::detail::drbg_fill(b, 2);
        return uint16_t(b[0] << 8 | b[1]);
    }

    // A uniform number in [0, n] for SRV's and MX's shuffles
    SGCL_INLINE_HOT uint32_t dns_random_upto(uint32_t n) noexcept {
        unsigned char b[8];
        crypto::detail::drbg_fill(b, 8);
        uint64_t v = 0;
        for (unsigned char c : b) {
            v = v << 8 | c;
        }
        return uint32_t(v % (uint64_t(n) + 1));
    }

    // How a server is asked: UDP with TCP for a truncated answer, TCP
    // alone, TLS (RFC 7858), HTTPS (RFC 8484)
    enum class DnsTransport : uint8_t {
        udp,
        tcp,
        tls,
        https
    };

    // One server of a lookup, as dns::server's text was read
    struct DnsServerSpec {
        DnsTransport transport = DnsTransport::udp;
        endpoint address;                               // udp, tcp; tls when its host is an address
        string host;                                    // tls: the host as written (a name or an address); https: the URL
        uint16_t port = 0;                              // tls
        string name;                                    // tls, https: the name the certificate is checked for; empty: the host's
        std::vector<std::array<uint8_t, 32>> pins;      // tls: SHA-256 of a SubjectPublicKeyInfo of the chain (RFC 7858 §4.2)
    };

    // What a lookup is asked to do
    struct DnsSettings {
        std::shared_ptr<const ResolvConf> conf;
        vector<DnsServerSpec> servers;  // not empty: these alone, in place of the file's
        duration timeout = {};          // per query; zero: the file's
        int attempts = 0;               // zero: the file's
        bool opportunistic = false;     // tls: no authentication required, clear text when TLS fails (RFC 8310 §5)
        bool https_get = false;         // https: GET with dns= in place of POST
        string roots_pem;               // tls, https: the roots a chain must lead to; empty: the system's
    };

    // One lookup's state, one managed object: the settings, the buffer
    // (the query at its start, the answers after), the sockets in use, so
    // that a stop closes them, and the channel the lookup's task answers
    // in when a stop is watched beside it
    struct DnsContext {
        static constexpr size_t QuerySize = 512;             // 2 + 12 + 255 + 4 + 11 at most
        static constexpr size_t AnswerSize = 1536;           // past DnsUdpPayload: a longer datagram is cut, and says so

        SGCL_INLINE_HOT explicit DnsContext(DnsSettings s, async::stop_token t) noexcept
        : settings(std::move(s))
        , stop(std::move(t))
        , done(1) {
        }

        // The socket of the exchange in progress, held for a stop; false
        // when the stop came first
        bool hold(const udp::socket& s) noexcept {
            std::lock_guard lock(m);
            if (stopped) {
                return false;
            }
            udp = s;
            return true;
        }

        bool hold(const connection& c) noexcept {
            std::lock_guard lock(m);
            if (stopped) {
                return false;
            }
            tcp = c;
            return true;
        }

        void let_go() noexcept {
            std::lock_guard lock(m);
            if (udp) {
                (void)udp.close();
                udp = udp::socket();
            }
            if (tcp) {
                (void)tcp.close();
                tcp = connection();
            }
        }

        void cancel() noexcept {
            std::lock_guard lock(m);
            stopped = true;
            if (udp) {
                (void)udp.close();
            }
            if (tcp) {
                (void)tcp.close();
            }
        }

        bool is_stopped() noexcept {
            std::lock_guard lock(m);
            return stopped;
        }

        SGCL_INLINE_HOT uint8_t* query() noexcept {
            return reinterpret_cast<uint8_t*>(buffer.data());
        }

        SGCL_INLINE_HOT uint8_t* answer() noexcept {
            return reinterpret_cast<uint8_t*>(buffer.data() + QuerySize);
        }

        DnsSettings settings;
        async::stop_token stop;
        std::mutex m;
        bool stopped = false;
        udp::socket udp;
        connection tcp;
        async::detail::ChannelState<DnsAnswer> done;
        array<byte, QuerySize + AnswerSize> buffer;
    };

    inline DnsAnswer dns_outcome(DnsStatus s) noexcept {
        DnsAnswer a;
        a.status = s;
        return a;
    }

    // A socket's error as an exchange's outcome
    inline DnsAnswer dns_socket_failure(DnsContext& ctx, const io::error& e) noexcept {
        if (e.is_timeout()) {
            return dns_outcome(DnsStatus::timeout);
        }
        if (ctx.is_stopped() || e.code() == std::errc::operation_canceled) {
            return dns_outcome(DnsStatus::cancelled);
        }
        DnsAnswer a = dns_outcome(DnsStatus::failed);
        a.error = e;
        return a;
    }

    // One exchange over TCP (RFC 1035 §4.2.2, RFC 7766): the message after
    // its length in two bytes, both ways, all of it before the deadline
    inline async::task<DnsAnswer> dns_exchange_tcp(tracked_ptr<DnsContext> ctx, endpoint server, DnsName qname, uint16_t qtype, time_point deadline) noexcept {
        if (ctx->is_stopped()) {
            co_return dns_outcome(DnsStatus::cancelled);
        }
        vector<endpoint> one;
        one.push_back(server);
        auto c = co_await dial_race(std::move(one), ctx->stop, deadline, AttemptDelay, DialOne(_co_dial_tcp), string("dns"));
        if (!c) {
            co_return dns_socket_failure(*ctx, c.error());
        }
        if (!ctx->hold(*c)) {
            (void)c->close();
            co_return dns_outcome(DnsStatus::cancelled);
        }
        c->set_deadline(deadline);
        uint16_t id = dns_random16();
        uint8_t* q = ctx->query();
        size_t n = dns_write_query(q + 2, DnsContext::QuerySize - 2, id, qname, qtype, true);
        q[0] = uint8_t(n >> 8);
        q[1] = uint8_t(n);
        auto w = co_await c->async_write(slice<const byte>(ctx, reinterpret_cast<const byte*>(q), n + 2));
        if (!w) {
            ctx->let_go();
            co_return dns_socket_failure(*ctx, w.error());
        }
        uint8_t* a = ctx->answer();
        auto r = co_await c->async_read_full(slice<byte>(ctx, reinterpret_cast<byte*>(a), 2));
        if (!r) {
            ctx->let_go();
            co_return dns_socket_failure(*ctx, r.error());
        }
        if (*r != 2) {
            ctx->let_go();
            co_return dns_outcome(DnsStatus::misbehaving);   // closed before the answer: read_full's 0 at the end of the stream
        }
        size_t len = size_t(a[0]) << 8 | a[1];
        DnsAnswer out;
        out.status = DnsStatus::misbehaving;
        if (len <= DnsContext::AnswerSize) {
            r = co_await c->async_read_full(slice<byte>(ctx, reinterpret_cast<byte*>(a), len));
            if (r && *r == len) {
                dns_read_answer(a, len, id, qname, qtype, true, out);
            }
        } else {
            vector<byte> big(len);
            r = co_await c->async_read_full(big.as_slice());
            if (r && *r == len) {
                dns_read_answer(reinterpret_cast<const uint8_t*>(big.data()), len, id, qname, qtype, true, out);
            }
        }
        ctx->let_go();
        if (!r) {
            co_return dns_socket_failure(*ctx, r.error());
        }
        if (out.status == DnsStatus::foreign || out.status == DnsStatus::truncated) {
            out.status = DnsStatus::misbehaving;   // over TCP the one answer is the answer: one not to the query is a fault
        }
        co_return out;
    }

    // One exchange over UDP: the query sent, the datagrams read until the
    // answer to it comes or the deadline passes; a truncated one asks
    // again over TCP, within the same deadline
    inline async::task<DnsAnswer> dns_exchange_udp(tracked_ptr<DnsContext> ctx, endpoint server, DnsName qname, uint16_t qtype, time_point deadline) noexcept {
        if (ctx->is_stopped()) {
            co_return dns_outcome(DnsStatus::cancelled);
        }
        auto s = connect_udp_to(server, string());
        if (!s) {
            co_return dns_socket_failure(*ctx, s.error());
        }
        if (!ctx->hold(*s)) {
            (void)s->close();
            co_return dns_outcome(DnsStatus::cancelled);
        }
        uint16_t id = dns_random16();
        uint8_t* q = ctx->query();
        size_t n = dns_write_query(q, DnsContext::QuerySize, id, qname, qtype, true);
        auto w = co_await s->async_send(slice<const byte>(ctx, reinterpret_cast<const byte*>(q), n));
        if (!w) {
            ctx->let_go();
            co_return dns_socket_failure(*ctx, w.error());
        }
        s->set_read_deadline(deadline);
        DnsAnswer out;
        bool truncated = false;
        for (;;) {
            auto d = co_await s->async_receive_from(slice<byte>(ctx, ctx->buffer.data() + DnsContext::QuerySize, DnsContext::AnswerSize));
            if (!d) {
                ctx->let_go();
                co_return dns_socket_failure(*ctx, d.error());
            }
            if (d->truncated) {
                truncated = true;
                break;
            }
            auto st = dns_read_answer(ctx->answer(), d->size, id, qname, qtype, false, out);
            if (st == DnsStatus::foreign) {
                continue;
            }
            truncated = st == DnsStatus::truncated;
            break;
        }
        ctx->let_go();
        if (truncated) {
            co_return co_await dns_exchange_tcp(ctx, server, qname, qtype, deadline);
        }
        co_return out;
    }

    // The transports of TLS and HTTPS: installed by the headers that have
    // them (detail/dns_tls.h through tls.h, http/detail/doh.h through
    // the http client), so that dns.h needs neither TLS nor HTTP. Each
    // asks the settings' server of the index given
    using DnsSecureExchange = async::task<DnsAnswer> (*)(tracked_ptr<DnsContext> ctx, size_t server, DnsName qname, uint16_t qtype, time_point deadline);

    struct DnsHooks {
        std::atomic<DnsSecureExchange> tls{nullptr};
        std::atomic<DnsSecureExchange> https{nullptr};
    };

    inline DnsHooks& dns_hooks() noexcept {
        static DnsHooks hooks;
        return hooks;
    }

    // A server of a transport no header installed: its failure, the
    // protocol not supported
    inline DnsAnswer dns_unsupported() noexcept {
        DnsAnswer a = dns_outcome(DnsStatus::failed);
        a.error = io::error(error_code(EPROTONOSUPPORT, std::system_category()), "lookup", "");
        return a;
    }

    // One query to the settings' server of index i, by its transport
    inline async::task<DnsAnswer> dns_exchange(tracked_ptr<DnsContext> ctx, size_t i, DnsName qname, uint16_t qtype, time_point deadline) noexcept {
        const DnsServerSpec& spec = ctx->settings.servers[i];
        switch (spec.transport) {
            case DnsTransport::udp:
                co_return co_await dns_exchange_udp(ctx, spec.address, qname, qtype, deadline);
            case DnsTransport::tcp:
                co_return co_await dns_exchange_tcp(ctx, spec.address, qname, qtype, deadline);
            case DnsTransport::tls:
                if (auto f = dns_hooks().tls.load(std::memory_order_acquire)) {
                    co_return co_await f(ctx, i, qname, qtype, deadline);
                }
                co_return dns_unsupported();
            case DnsTransport::https:
                if (auto f = dns_hooks().https.load(std::memory_order_acquire)) {
                    co_return co_await f(ctx, i, qname, qtype, deadline);
                }
                co_return dns_unsupported();
        }
        co_return dns_unsupported();
    }

    // The servers, `attempts` rounds over them: the first answer (ok,
    // nxdomain, nodata), else the last failure
    inline async::task<DnsAnswer> dns_ask(tracked_ptr<DnsContext> ctx, DnsName qname, uint16_t qtype, size_t start) noexcept {
        const ResolvConf& conf = *ctx->settings.conf;
        bool given = !ctx->settings.servers.empty();
        size_t n = given ? ctx->settings.servers.size() : conf.servers.size();
        int attempts = ctx->settings.attempts > 0 ? ctx->settings.attempts : conf.attempts;
        duration timeout = ctx->settings.timeout > duration::zero() ? ctx->settings.timeout : duration(std::chrono::seconds(conf.timeout));
        DnsAnswer last = dns_outcome(DnsStatus::timeout);
        for (int round = 0; round < attempts; ++round) {
            for (size_t i = 0; i < n; ++i) {
                time_point deadline = sgcl::clock::now() + timeout;
                DnsAnswer a;
                if (given) {
                    a = co_await dns_exchange(ctx, i, qname, qtype, deadline);
                } else {
                    endpoint server = conf.servers[(start + i) % n];
                    a = conf.tcp ? co_await dns_exchange_tcp(ctx, server, qname, qtype, deadline)
                                 : co_await dns_exchange_udp(ctx, server, qname, qtype, deadline);
                }
                switch (a.status) {
                    case DnsStatus::ok:
                    case DnsStatus::nxdomain:
                    case DnsStatus::nodata:
                    case DnsStatus::cancelled:
                        co_return a;
                    default:
                        last = std::move(a);
                }
            }
        }
        co_return last;
    }

    // One name of the search list: asked, and asked again at the end of a
    // chain the server left unfollowed. `canonical` (lookup_cname): a
    // name that exists is the answer, with or without records of the type
    inline async::task<DnsAnswer> dns_resolve_name(tracked_ptr<DnsContext> ctx, DnsName qname, uint16_t qtype, bool canonical, size_t start) noexcept {
        DnsName current = qname;
        int budget = DnsMaxHops;
        int chased = 0;
        for (;;) {
            DnsAnswer a = co_await dns_ask(ctx, current, qtype, start);
            if (a.status == DnsStatus::nodata && a.hops > 0 && a.hops < budget) {
                budget -= a.hops;
                chased += a.hops;
                current = a.end;
                continue;
            }
            if (a.status == DnsStatus::nodata && a.hops > 0) {
                a.status = DnsStatus::misbehaving;   // the bound reached
            }
            chased += a.hops;
            if (canonical && (a.status == DnsStatus::nodata || (a.status == DnsStatus::nxdomain && chased > 0))) {
                a.status = DnsStatus::ok;
            }
            a.hops = chased;
            co_return a;
        }
    }

    // Multicast DNS (mdns_engine.h, included at the end): a name under
    // .local, and the reverse zones of link-local addresses, go to it alone
    // (RFC 6762 §3, §4)
    inline bool mdns_is_local(const DnsName& n) noexcept;
    inline async::task<DnsAnswer> mdns_resolve(tracked_ptr<DnsContext> ctx, DnsName qname, uint16_t qtype) noexcept;

    // The search list's candidates in turn: the first that answers, else
    // what the name as given came to (else the first candidate's)
    inline async::task<DnsAnswer> dns_resolve(tracked_ptr<DnsContext> ctx, string name, uint16_t qtype, bool canonical) noexcept {
        DnsName given;
        DnsNameText info;
        if (!dns_name_from_text(name.view(), given, &info)) {
            co_return dns_outcome(DnsStatus::invalid);
        }
        if (mdns_is_local(given)) {
            co_return co_await mdns_resolve(ctx, given, qtype);
        }
        const ResolvConf& conf = *ctx->settings.conf;
        size_t start = 0;
        if (conf.rotate && ctx->settings.servers.empty() && conf.servers.size() > 1) {
            static std::atomic<size_t> next{0};
            start = next.fetch_add(1, std::memory_order_relaxed) % conf.servers.size();
        }
        DnsSearchOrder order = dns_search_order(info, conf);
        optional<DnsAnswer> as_given, first;
        for (size_t i = 0; i < order.count; ++i) {
            DnsName candidate;
            if (order.order[i] < 0) {
                candidate = given;
            } else {
                DnsName domain;
                if (!dns_name_from_text(conf.search[size_t(order.order[i])], domain) || !dns_name_join(given, domain, candidate)) {
                    continue;
                }
            }
            DnsAnswer a = co_await dns_resolve_name(ctx, candidate, qtype, canonical, start);
            if (a.status == DnsStatus::ok || a.status == DnsStatus::cancelled) {
                co_return a;
            }
            if (order.order[i] < 0) {
                as_given = std::move(a);
            } else if (!first) {
                first = std::move(a);
            }
        }
        if (as_given) {
            co_return std::move(*as_given);
        }
        co_return first ? std::move(*first) : dns_outcome(DnsStatus::invalid);
    }

    inline async::task<void> dns_resolve_into(tracked_ptr<DnsContext> ctx, string name, uint16_t qtype, bool canonical) noexcept {
        DnsAnswer a = co_await dns_resolve(ctx, name, qtype, canonical);
        ctx->done.try_send(std::move(a));
    }

    // A lookup with its stop: without one possible, the resolution itself;
    // with one, the resolution as a task of its own beside the stop, which
    // ends the wait with cancelled at once and closes the socket in use
    inline async::task<DnsAnswer> dns_lookup(string name, uint16_t qtype, bool canonical, DnsSettings settings, async::stop_token stop) noexcept {
        if (stop.stop_requested()) {
            co_return dns_outcome(DnsStatus::cancelled);
        }
        if (!settings.conf) {
            settings.conf = resolv_conf_cache().get();
        }
        bool watched = stop.stop_possible();
        tracked_ptr<DnsContext> ctx = make_tracked<DnsContext>(std::move(settings), stop);
        if (!watched) {
            co_return co_await dns_resolve(ctx, name, qtype, canonical);
        }
        async::go(dns_resolve_into(ctx, name, qtype, canonical));
        optional<DnsAnswer> got;
        bool stopped = false;
        co_await sgcl::async::select(ctx->done.on_receive([&](optional<DnsAnswer> a) { got = std::move(a); }), stop.on_stop([&] { stopped = true; }));
        if (stopped || stop.stop_requested() || !got) {   // the stop wins over an answer that came with it
            ctx->cancel();
            co_return dns_outcome(DnsStatus::cancelled);
        }
        co_return std::move(*got);
    }

    // A lookup's outcome as the error the public functions return: the
    // operation "lookup", the name as asked
    inline io::error dns_error(const DnsAnswer& a, const string& name) noexcept {
        switch (a.status) {
            case DnsStatus::nxdomain:
            case DnsStatus::invalid:
                return net_error(errc::host_not_found, "lookup", name);
            case DnsStatus::nodata:
                return net_error(errc::no_data, "lookup", name);
            case DnsStatus::servfail:
                return net_error(errc::server_failure, "lookup", name);
            case DnsStatus::timeout:
                return system_error(ETIMEDOUT, "lookup", name);
            case DnsStatus::cancelled:
                return system_error(ECANCELED, "lookup", name);
            case DnsStatus::failed:
                if (a.error) {
                    return io::error(a.error->code(), "lookup", name);
                }
                [[fallthrough]];
            default:
                return net_error(errc::server_misbehaving, "lookup", name);
        }
    }

    // An address with an optional port, `port` when it has none:
    // "10.0.0.1", "10.0.0.1:5353", "::1", "[::1]:53", "fe80::1%en0"
    inline optional<endpoint> dns_address(const string& text, uint16_t port) noexcept {
        if (auto a = IpText::parse(IpText::view(text))) {
            return endpoint(*a, port);
        }
        if (auto e = parse_endpoint(text)) {
            if (e->port() != 0) {
                return *e;
            }
        }
        return nullopt;
    }

    // A server's text read: an address with an optional port, asked over
    // UDP (port 53; "udp://" may stand before it), "tcp://" the same over
    // TCP alone, "tls://" a host (a name or an address) with an optional
    // port (853), "https://" a URL of RFC 8484's template. `what` names
    // the server in an error
    inline expected<DnsServerSpec, io::error> dns_server_spec(const string& text) noexcept {
        std::string_view v = text.view();
        DnsServerSpec spec;
        auto bad = [&]() { return fail(net_error(errc::invalid_address, "lookup", text)); };
        auto starts = [&](std::string_view p) {
            if (v.size() < p.size()) {
                return false;
            }
            for (size_t i = 0; i < p.size(); ++i) {
                char c = v[i];
                if (c >= 'A' && c <= 'Z') {
                    c = char(c + 32);
                }
                if (c != p[i]) {
                    return false;
                }
            }
            return true;
        };
        if (starts("https://")) {
            auto u = url::parse(text);
            if (!u || u->scheme() != "https" || u->host().empty()) {
                return bad();
            }
            spec.transport = DnsTransport::https;
            spec.host = u->to_string();
            return spec;
        }
        uint16_t port = 53;
        if (starts("tls://")) {
            spec.transport = DnsTransport::tls;
            port = 853;
            v.remove_prefix(6);
        } else if (starts("tcp://")) {
            spec.transport = DnsTransport::tcp;
            v.remove_prefix(6);
        } else if (starts("udp://")) {
            v.remove_prefix(6);
        }
        if (v.empty()) {
            return bad();
        }
        string rest{v};
        if (auto e = dns_address(rest, port)) {
            spec.address = *e;
            spec.host = string(e->address().with_zone(string()).to_string());
            spec.port = e->port();
            return spec;
        }
        if (spec.transport != DnsTransport::tls) {
            return bad();   // a name is no server's address: it would need a resolver to be asked
        }
        // a name, with or without a port
        std::string_view host = v;
        uint16_t p = port;
        if (auto colon = v.rfind(':'); colon != std::string_view::npos) {
            auto q = parse_port(v.substr(colon + 1));
            if (!q || *q == 0) {
                return bad();
            }
            host = v.substr(0, colon);
            p = *q;
        }
        DnsName check;
        if (host.empty() || host.find_first_of(" []/%") != std::string_view::npos || !dns_name_from_text(host, check)) {
            return bad();
        }
        spec.host = string(host);
        spec.port = p;
        return spec;
    }

    // RFC 2782's order: by priority, and within one priority by weight,
    // at random: the records of weight 0 first, then each pick a uniform
    // number in [0, the sum of the weights left] and the first record whose
    // running sum reaches it
    template<class Srv>
    void dns_order_srv(vector<Srv>& v) noexcept {
        // in place: the sorts and partitions of std that keep the order take a
        // buffer of plain memory, where a record's strings may not live; and
        // RFC 2782 leaves the order of one priority's records free
        std::sort(v.begin(), v.end(), [](const Srv& a, const Srv& b) { return a.priority < b.priority; });
        size_t i = 0;
        while (i < v.size()) {
            size_t j = i;
            uint32_t sum = 0;
            while (j < v.size() && v[j].priority == v[i].priority) {
                sum += v[j].weight;
                ++j;
            }
            std::partition(v.begin() + ptrdiff_t(i), v.begin() + ptrdiff_t(j), [](const Srv& s) { return s.weight == 0; });
            for (size_t k = i; k + 1 < j; ++k) {
                uint32_t pick = dns_random_upto(sum);
                uint32_t running = 0;
                size_t at = k;
                for (size_t m = k; m < j; ++m) {
                    running += v[m].weight;
                    if (running >= pick) {
                        at = m;
                        break;
                    }
                }
                sum -= v[at].weight;
                if (at != k) {
                    Srv chosen = v[at];
                    for (size_t m = at; m > k; --m) {
                        v[m] = v[m - 1];   // the others keep their order: the zeros stay first
                    }
                    v[k] = chosen;
                }
            }
            i = j;
        }
    }

    // RFC 5321 §5.1: by preference, the ones of one preference at random:
    // shuffled, then sorted in place, which leaves the ones of one
    // preference in an order as random as the shuffle's
    template<class Mx>
    void dns_order_mx(vector<Mx>& v) noexcept {
        for (size_t i = v.size(); i > 1; --i) {
            size_t k = dns_random_upto(uint32_t(i - 1));
            if (k != i - 1) {
                std::swap(v[k], v[i - 1]);
            }
        }
        std::sort(v.begin(), v.end(), [](const Mx& a, const Mx& b) { return a.preference < b.preference; });
    }
}

#include "mdns_engine.h"   // multicast DNS: the lookups of names under .local
