//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "connection.h"
#include "dns.h"
#include "error.h"
#include "ip.h"
#include "socket.h"
#include "socks5.h"
#include "../async/channel.h"
#include "../async/coroutine.h"
#include "../async/select.h"
#include "../async/stop_token.h"
#include "../async/timer.h"
#include "../async/wait_group.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/clock.h"
#include "../core/duration.h"
#include "../core/function.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// A SOCKS5 server (RFC 1928): CONNECT, BIND and UDP ASSOCIATE, with no
// authentication or a username and password (RFC 1929), the connections
// out made from here or by a function of the program's (through an SSH
// connection: ssh -D)
namespace sgcl::net {
    namespace detail {
        inline constexpr uint8_t Socks5Bind = 0x02;
        inline constexpr uint8_t Socks5Udp = 0x03;

        // The REP of a failure to reach the target (RFC 1928 §6)
        inline uint8_t socks5_rep_of(const io::error& e) noexcept {
            auto c = e.code();
            if (c == std::errc::connection_refused) {
                return 0x05;
            }
            if (c == std::errc::network_unreachable) {
                return 0x03;
            }
            if (c == std::errc::host_unreachable || c == make_error_code(net::errc::host_not_found)) {
                return 0x04;
            }
            if (c == std::errc::timed_out) {
                return 0x06;
            }
            return 0x01;
        }

        // An address as the protocol writes it: ATYP and the bytes, then the port
        inline void socks5_put_endpoint(std::string& out, const endpoint& e) {
            auto a = e.address();
            auto b = a.bytes();
            if (a.is_v4() || a.is_v4_mapped()) {
                out += char(Socks5Ipv4);
                out.append(reinterpret_cast<const char*>(b.data()) + 12, 4);
            } else {
                out += char(Socks5Ipv6);
                out.append(reinterpret_cast<const char*>(b.data()), 16);
            }
            out += char(e.port() >> 8);
            out += char(e.port());
        }

        inline std::string socks5_reply(uint8_t rep, const endpoint& bound) {
            std::string out;
            out += char(Socks5Version);
            out += char(rep);
            out += char(0);
            if (bound.is_valid()) {
                socks5_put_endpoint(out, bound);
            } else {
                out.append("\x01\x00\x00\x00\x00\x00\x00", 7);
            }
            return out;
        }

        // An address of a request or a datagram's head (ATYP, address,
        // port) at the front of the bytes: "host:port", and the bytes it
        // took; 0 when they are not whole; -1 for an ATYP of no meaning
        inline int socks5_read_address(std::string_view b, std::string& target, size_t& used) {
            if (b.empty()) {
                return 0;
            }
            uint8_t atyp = uint8_t(b[0]);
            size_t n;
            if (atyp == Socks5Ipv4) {
                n = 1 + 4 + 2;
            } else if (atyp == Socks5Ipv6) {
                n = 1 + 16 + 2;
            } else if (atyp == Socks5Domain) {
                if (b.size() < 2) {
                    return 0;
                }
                n = 2 + size_t(uint8_t(b[1])) + 2;
                if (uint8_t(b[1]) == 0) {
                    return -1;
                }
            } else {
                return -1;
            }
            if (b.size() < n) {
                return 0;
            }
            uint16_t port = uint16_t(uint8_t(b[n - 2]) << 8 | uint8_t(b[n - 1]));
            if (atyp == Socks5Ipv4) {
                target = std::to_string(uint8_t(b[1])) + "." + std::to_string(uint8_t(b[2])) + "." + std::to_string(uint8_t(b[3])) + "." + std::to_string(uint8_t(b[4]));
            } else if (atyp == Socks5Ipv6) {
                array<uint8_t, 16> bytes;
                for (size_t i = 0; i < 16; ++i) {
                    bytes[i] = uint8_t(b[1 + i]);
                }
                target = "[" + std::string(ip_address::v6(bytes).to_string().view()) + "]";
            } else {
                target.assign(b.data() + 2, uint8_t(b[1]));
                for (char c : target) {
                    if (uint8_t(c) <= ' ' || c == ':' || c == '[' || c == ']' || uint8_t(c) == 0x7F) {
                        return -1;   // a name that is no host
                    }
                }
            }
            target += ":" + std::to_string(port);
            used = n;
            return 1;
        }

        // The server's handshake read without I/O, one message at a time
        // at the buffer's front: 1 with the bytes it took, 0 when it is not
        // whole, -1 for one that breaks RFC 1928 / RFC 1929

        // The greeting: VER NMETHODS METHODS; the method chosen (0xFF: none of ours offered)
        inline int socks5_take_greeting(std::string_view b, bool user_pass, size_t& used, uint8_t& method) noexcept {
            if (!b.empty() && uint8_t(b[0]) != Socks5Version) {
                return -1;   // not SOCKS5: told at its first byte
            }
            if (b.size() < 2 || b.size() < 2 + size_t(uint8_t(b[1]))) {
                return 0;
            }
            std::string_view methods = b.substr(2, uint8_t(b[1]));
            uint8_t wanted = user_pass ? Socks5UserPass : Socks5NoAuth;
            method = methods.find(char(wanted)) != std::string_view::npos ? wanted : Socks5NoAcceptable;
            used = 2 + methods.size();
            return 1;
        }

        // RFC 1929's request: VER=1 ULEN UNAME PLEN PASSWD
        inline int socks5_take_userpass(std::string_view b, size_t& used, std::string& user, std::string& password) {
            if (b.size() < 2) {
                return 0;
            }
            if (uint8_t(b[0]) != 1) {
                return -1;
            }
            size_t ulen = uint8_t(b[1]);
            if (b.size() < 3 + ulen) {
                return 0;
            }
            size_t plen = uint8_t(b[2 + ulen]);
            if (b.size() < 3 + ulen + plen) {
                return 0;
            }
            user.assign(b.data() + 2, ulen);
            password.assign(b.data() + 3 + ulen, plen);
            used = 3 + ulen + plen;
            return 1;
        }

        // The request: VER CMD RSV ATYP DST.ADDR DST.PORT; the target "host:port"
        inline int socks5_take_request(std::string_view b, size_t& used, uint8_t& cmd, std::string& target) {
            if (!b.empty() && uint8_t(b[0]) != Socks5Version) {
                return -1;
            }
            if (b.size() < 4) {
                return 0;
            }
            size_t n = 0;
            int r = socks5_read_address(b.substr(3), target, n);
            if (r <= 0) {
                return r;
            }
            cmd = uint8_t(b[1]);
            used = 3 + n;
            return 1;
        }

        using Socks5Dial = function<async::task<expected<net::connection, io::error>>(string target, async::stop_token stop)>;

        struct Socks5ServerSettings {
            function<bool(const string&, const string&)> authenticate;
            function<bool(const endpoint&, const string&)> allow;
            Socks5Dial dial;
            bool bind = true;
            bool udp = true;
            duration timeout = std::chrono::seconds(30);
            duration idle_timeout = std::chrono::minutes(10);
            size_t max_connections = 0;
            function<void(const string&)> on_error;
        };

        struct Socks5ServerImpl {
            std::mutex lock;
            vector<net::listener> listeners;
            vector<net::connection> conns;
            std::atomic<size_t> count = {0};
            std::atomic<bool> closing = {false};
            async::wait_group running;
        };

        // The bytes one way, until the end of the reading side: then the
        // writing side's write half closed (a TCP FIN through)
        inline async::task<> socks5_pump(net::connection from, net::connection to, duration idle) noexcept {
            tracked_ptr<array<std::byte, 32768>> block = make_tracked<array<std::byte, 32768>>();
            for (;;) {
                if (idle > duration::zero()) {
                    from.set_read_deadline(sgcl::clock::now() + idle);
                }
                auto r = co_await from.async_read(slice<byte>(block->data(), block->size()));
                if (!r || *r == 0) {
                    break;
                }
                if (!co_await to.async_write(slice<const byte>(block->data(), *r))) {
                    break;
                }
            }
            (void)to.close_write();
        }

        // Both ways, then both closed
        inline async::task<> socks5_relay(net::connection a, net::connection b, duration idle) noexcept {
            auto there = async::spawn(socks5_pump(a, b, idle));
            co_await socks5_pump(b, a, idle);
            co_await there;
            (void)a.close();
            (void)b.close();
        }

        // The UDP relay of an association (RFC 1928 §7): a datagram of the
        // client's, its head read, sent on to its target; one from anywhere
        // else sent to the client with the head of its sender. Ends with
        // the TCP connection
        inline async::task<> socks5_udp_relay(net::udp::socket u, net::connection control, ip_address client_ip, endpoint client_hint, duration idle) noexcept {
            endpoint client = client_hint.port() ? client_hint : endpoint();
            async::stop_source ended;
            auto watch = async::spawn([](net::connection control, async::stop_source ended, net::udp::socket u) -> async::task<> {
                char b[256];
                for (;;) {   // the association lasts as long as its TCP connection (§7)
                    auto r = co_await control.async_read(slice<byte>(reinterpret_cast<byte*>(b), sizeof b));
                    if (!r || *r == 0) {
                        break;
                    }
                }
                ended.request_stop();
                (void)u.close();
            }(control, ended, u));
            std::vector<char> block(65536);   // a datagram whole: past a managed array's page; reactor I/O, the frame keeps it
            for (;;) {
                auto d = co_await u.async_receive_from(slice<byte>(reinterpret_cast<byte*>(block.data()), block.size()));
                if (!d) {
                    break;
                }
                std::string_view data(block.data(), d->size);
                bool from_client = d->from.address() == client_ip && (!client.port() || d->from.port() == client.port());
                if (from_client) {
                    client = d->from;
                    // RSV RSV FRAG ATYP DST.ADDR DST.PORT DATA; a fragment (FRAG != 0) is dropped (§7)
                    if (data.size() < 4 || data[0] != 0 || data[1] != 0 || data[2] != 0) {
                        continue;
                    }
                    std::string target;
                    size_t used = 0;
                    if (socks5_read_address(data.substr(3), target, used) != 1) {
                        continue;
                    }
                    auto hp = split_host_port(target);
                    if (!hp) {
                        continue;
                    }
                    std::string host(hp->host);
                    auto ip = ip_address::parse(string(host));
                    if (!ip) {
                        auto found = co_await net::dns::async_lookup(string(host));
                        if (!found || found->empty()) {
                            continue;
                        }
                        ip = (*found)[0];
                    }
                    auto port = parse_port(hp->port);
                    if (!port) {
                        continue;
                    }
                    (void)co_await u.async_send_to(slice<const byte>(reinterpret_cast<const byte*>(data.data()) + 3 + used, data.size() - 3 - used), endpoint(*ip, *port));
                } else if (client.port()) {
                    std::string out("\x00\x00\x00", 3);
                    socks5_put_endpoint(out, d->from);
                    out.append(data.data(), data.size());
                    (void)co_await u.async_send_to(slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size()), client);
                }
            }
            (void)u.close();
            (void)control.close();
            co_await watch;
            (void)idle;
        }

        inline async::task<bool> socks5_read_more(net::connection& c, std::string& buf) noexcept {
            char block[512];
            auto r = co_await c.async_read(slice<byte>(reinterpret_cast<byte*>(block), sizeof block));
            if (!r || *r == 0) {
                co_return false;
            }
            buf.append(block, *r);
            co_return true;
        }

        // One client: the greeting, the authentication, the request, then
        // the relay of its command
        inline async::task<> socks5_serve(tracked_ptr<Socks5ServerImpl> s, tracked_ptr<Socks5ServerSettings> cfg, net::connection c) noexcept {
            const endpoint peer = c.remote_endpoint();
            auto send = [&](std::string bytes) -> async::task<bool> {
                co_return bool(co_await c.async_write(slice<const byte>(reinterpret_cast<const byte*>(bytes.data()), bytes.size())));
            };
            if (cfg->timeout > duration::zero()) {
                c.set_deadline(sgcl::clock::now() + cfg->timeout);   // the handshake's whole
            }
            std::string buf;
            size_t used = 0;
            // the greeting: VER NMETHODS METHODS
            uint8_t method = 0;
            for (;;) {
                int r = socks5_take_greeting(buf, bool(cfg->authenticate), used, method);
                if (r < 0) {
                    co_return;
                }
                if (r > 0) {
                    break;
                }
                if (!co_await socks5_read_more(c, buf)) {
                    co_return;
                }
            }
            buf.erase(0, used);
            if (!co_await send(std::string{char(Socks5Version), char(method)}) || method == Socks5NoAcceptable) {
                co_return;
            }
            if (method == Socks5UserPass) {
                std::string user, password;
                for (;;) {
                    int r = socks5_take_userpass(buf, used, user, password);
                    if (r < 0) {
                        co_return;
                    }
                    if (r > 0) {
                        break;
                    }
                    if (!co_await socks5_read_more(c, buf)) {
                        co_return;
                    }
                }
                buf.erase(0, used);
                bool ok = cfg->authenticate(string(user), string(password));
                (void)co_await send(std::string{char(1), char(ok ? 0 : 1)});
                if (!ok) {
                    co_return;   // RFC 1929 §2: the connection closed after a failure
                }
            }
            // the request: VER CMD RSV ATYP DST.ADDR DST.PORT
            std::string target;
            uint8_t cmd = 0;
            for (;;) {
                int r = socks5_take_request(buf, used, cmd, target);
                if (r < 0) {
                    (void)co_await send(socks5_reply(0x08, endpoint()));   // address type not supported
                    co_return;
                }
                if (r > 0) {
                    break;
                }
                if (!co_await socks5_read_more(c, buf)) {
                    co_return;
                }
            }
            buf.erase(0, used);
            c.set_deadline(time_point());
            if (cfg->allow && !cfg->allow(peer, string(target))) {
                (void)co_await send(socks5_reply(0x02, endpoint()));   // not allowed by the ruleset
                co_return;
            }
            if (cmd == Socks5Connect) {
                async::stop_source dial_stop;
                if (cfg->timeout > duration::zero()) {
                    dial_stop.stop_after(cfg->timeout);
                }
                expected<net::connection, io::error> out = cfg->dial ? co_await cfg->dial(string(target), dial_stop.token())
                                                                     : co_await net::detail::dial(string(target), dial_stop.token(), time_point());
                dial_stop.request_stop();
                if (!out) {
                    (void)co_await send(socks5_reply(socks5_rep_of(out.error()), endpoint()));
                    co_return;
                }
                endpoint bound = out->local_endpoint();
                if (!co_await send(socks5_reply(0x00, bound.is_valid() ? bound : c.local_endpoint()))) {
                    (void)out->close();
                    co_return;
                }
                if (!buf.empty()) {   // the client's bytes that came with the request
                    if (!co_await out->async_write(slice<const byte>(reinterpret_cast<const byte*>(buf.data()), buf.size()))) {
                        (void)out->close();
                        co_return;
                    }
                }
                co_await socks5_relay(c, *out, cfg->idle_timeout);
                co_return;
            }
            if (cmd == Socks5Bind && cfg->bind && !cfg->dial) {
                // a listener on the address the client reached us by; its address told (the first reply), the
                // connection that comes (the second, with its peer), then the relay
                endpoint here = c.local_endpoint();
                auto l = net::tcp::listen(string(endpoint(here.address(), 0).to_string()));
                if (!l) {
                    (void)co_await send(socks5_reply(0x01, endpoint()));
                    co_return;
                }
                if (!co_await send(socks5_reply(0x00, l->local_endpoint()))) {
                    (void)l->close();
                    co_return;
                }
                async::stop_source wait_stop;
                if (cfg->timeout > duration::zero()) {
                    wait_stop.stop_after(cfg->timeout);
                }
                auto closer = async::spawn([](net::listener l, async::stop_token t) -> async::task<> {
                    co_await async::select(t.on_stop([] {}));
                    (void)l.close();
                }(*l, wait_stop.token()));
                auto in = co_await l->async_accept();
                wait_stop.request_stop();
                co_await closer;
                if (!in) {
                    (void)co_await send(socks5_reply(0x06, endpoint()));
                    co_return;
                }
                if (!co_await send(socks5_reply(0x00, in->remote_endpoint()))) {
                    (void)in->close();
                    co_return;
                }
                co_await socks5_relay(c, *in, cfg->idle_timeout);
                co_return;
            }
            if (cmd == Socks5Udp && cfg->udp && !cfg->dial) {
                endpoint here = c.local_endpoint();
                auto u = net::udp::bind(string(endpoint(here.address(), 0).to_string()));
                if (!u) {
                    (void)co_await send(socks5_reply(0x01, endpoint()));
                    co_return;
                }
                if (!co_await send(socks5_reply(0x00, u->local_endpoint()))) {
                    (void)u->close();
                    co_return;
                }
                // the client's own address and port when it gave them (zeros: learnt from its first datagram)
                endpoint hint;
                if (auto e = endpoint::parse(string(target)); e && !e->address().is_unspecified()) {
                    hint = *e;
                }
                co_await socks5_udp_relay(*u, c, peer.address(), hint, cfg->idle_timeout);
                co_return;
            }
            (void)co_await send(socks5_reply(0x07, endpoint()));   // command not supported
        }

        inline async::task<> socks5_serve_one(tracked_ptr<Socks5ServerImpl> s, tracked_ptr<Socks5ServerSettings> cfg, net::connection c) noexcept {
            co_await socks5_serve(s, cfg, c);
            (void)c.close();
            {
                std::lock_guard g(s->lock);
                for (size_t i = 0; i < s->conns.size(); ++i) {
                    if (s->conns[i] == c) {
                        s->conns.erase(s->conns.begin() + long(i));
                        break;
                    }
                }
            }
            s->count.fetch_sub(1);
            s->running.done();
        }

        inline async::task<expected<void, io::error>> socks5_accept_loop(tracked_ptr<Socks5ServerImpl> s, tracked_ptr<Socks5ServerSettings> cfg, net::listener l) noexcept {
            {
                std::lock_guard g(s->lock);
                s->listeners.push_back(l);
            }
            // a close() or shutdown() that came first: its flag is set before
            // it takes the lock, so either it saw the listener or this sees it
            if (s->closing.load()) {
                (void)l.close();
                co_return unexpected(net_error(net::errc::server_closed, "socks5 serve", string()));
            }
            for (;;) {
                auto c = co_await l.async_accept();
                if (!c) {
                    if (s->closing.load() || c.error().code() == io::errc::closed) {   // closed by close(), shutdown() or the program
                        co_return unexpected(net_error(net::errc::server_closed, "socks5 serve", string()));
                    }
                    if (cfg->on_error) {
                        cfg->on_error(c.error().message());
                    }
                    co_return unexpected(c.error());
                }
                if (cfg->max_connections && s->count.load() >= cfg->max_connections) {
                    (void)c->close();
                    continue;
                }
                s->count.fetch_add(1);
                s->running.add();
                {
                    std::lock_guard g(s->lock);
                    s->conns.push_back(*c);
                }
                if (s->closing.load()) {
                    (void)c->close();   // accepted as close() ran: its relay ends at once
                }
                async::go(socks5_serve_one(s, cfg, *c));
            }
        }
    }

    // A SOCKS5 server (RFC 1928): its clients' connections made from here
    // and relayed both ways — CONNECT to a target, BIND for a connection
    // that comes to it, UDP ASSOCIATE for datagrams — with no authentication
    // or a username and password (RFC 1929). A function of the program's
    // may make the connections out instead (dial: through an SSH
    // connection, as ssh -D does; BIND and UDP ASSOCIATE are then refused).
    // A handle of one word: copies share the connections; the fields are
    // read when serve is called.
    //
    //     net::socks5::server proxy;
    //     proxy.authenticate = [](const string& u, const string& p) { return u == "alice" && p == "secret"; };
    //     proxy.serve("127.0.0.1:1080");
    class socks5::server {
    public:
        server()
        : _impl(make_tracked<detail::Socks5ServerImpl>()) {
        }

        server(const server&) = default;
        server& operator=(const server&) = default;

        // Listens on the address ("127.0.0.1:1080") and serves until
        // shutdown() or close(): then net::errc::server_closed
        // `serve(...)` on this thread, `co_await async_serve(...)` in a task
        expected<void, io::error> serve(const string& address) const {
            return async_serve(address).wait();
        }

        async::task<expected<void, io::error>> async_serve(const string& address) const noexcept {
            return _co_serve_address(_impl, _settings(), address);
        }

        // The connections of a listener the program made
        expected<void, io::error> serve(const net::listener& l) const {
            return async_serve(l).wait();
        }

        async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept {
            return detail::socks5_accept_loop(_impl, _settings(), l);
        }

        // Gracefully: the listeners closed, the relays in progress left to
        // end by themselves; returns when they have
        void shutdown() const {
            async_shutdown().wait();
        }

        async::task<> async_shutdown() const noexcept {
            return _co_shutdown(_impl);
        }

        // At once: every listener and connection closed; the server stays
        // closed (a serve after it ends with server_closed), as after shutdown()
        void close() const {
            _impl->closing.store(true);
            vector<net::listener> ls;
            vector<net::connection> cs;
            {
                std::lock_guard g(_impl->lock);
                ls = _impl->listeners;
                cs = _impl->conns;
            }
            for (auto& l : ls) {
                (void)l.close();
            }
            for (auto& c : cs) {
                (void)c.close();
            }
        }

        // The clients being served
        size_t connections() const noexcept {
            return _impl->count.load();
        }

        function<bool(const string& username, const string& password)> authenticate;   // set: username and password required (RFC 1929); empty: no authentication
        function<bool(const endpoint& client, const string& target)> allow;            // each request's target ("host:port") checked; empty: every one
        function<async::task<expected<net::connection, io::error>>(string target, async::stop_token stop)> dial;   // the connections out; empty: TCP from here
        bool bind = true;                                     // BIND taken (without dial)
        bool udp = true;                                      // UDP ASSOCIATE taken (without dial)
        duration timeout = std::chrono::seconds(30);          // the handshake, the connection out, a BIND's wait
        duration idle_timeout = std::chrono::minutes(10);     // a relay silent one way that long ended; zero: none
        size_t max_connections = 0;                           // past it a client is closed at once; zero: none
        function<void(const string&)> on_error;               // an accept's failure; none by default

    private:
        tracked_ptr<detail::Socks5ServerSettings> _settings() const {
            tracked_ptr cfg = make_tracked<detail::Socks5ServerSettings>();
            cfg->authenticate = authenticate;
            cfg->allow = allow;
            cfg->dial = dial;
            cfg->bind = bind;
            cfg->udp = udp;
            cfg->timeout = timeout;
            cfg->idle_timeout = idle_timeout;
            cfg->max_connections = max_connections;
            cfg->on_error = on_error;
            return cfg;
        }

        static async::task<expected<void, io::error>> _co_serve_address(tracked_ptr<detail::Socks5ServerImpl> s, tracked_ptr<detail::Socks5ServerSettings> cfg, string address) noexcept {
            auto l = net::tcp::listen(address);
            if (!l) {
                co_return unexpected(l.error());
            }
            co_return co_await detail::socks5_accept_loop(s, cfg, *l);
        }

        static async::task<> _co_shutdown(tracked_ptr<detail::Socks5ServerImpl> s) noexcept {
            s->closing.store(true);
            vector<net::listener> ls;
            {
                std::lock_guard g(s->lock);
                ls = s->listeners;
            }
            for (auto& l : ls) {
                (void)l.close();
            }
            co_await s->running;
        }

        tracked_ptr<detail::Socks5ServerImpl> _impl;
    };
}
