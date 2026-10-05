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
#include "../async/channel.h"
#include "../async/coroutine.h"
#include "../async/select.h"
#include "../async/stop_token.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/clock.h"
#include "../core/duration.h"
#include "../core/make_tracked.h"
#include "../core/string.h"

#include <cerrno>
#include <cstdint>
#include <string>
#include <string_view>

// SOCKS5 (RFC 1928) with username and password (RFC 1929): a connection
// to a target made through a proxy. The proxy is asked to CONNECT; a
// target's name is sent to the proxy as it is and resolved there (the
// address type DOMAINNAME, what curl calls socks5h), an IPv4 or IPv6
// address as its bytes. What comes back is the connection to the proxy,
// which from then on carries the target's bytes: a net::connection as any
// other (TLS, HTTP on top).
//
//   auto c = net::socks5::connect("127.0.0.1:1080", "example.com:80");
namespace sgcl::net {
    namespace detail {
        using namespace sgcl::detail;

        // The protocol's numbers (RFC 1928 §3-6, RFC 1929 §2)
        inline constexpr uint8_t Socks5Version = 5;
        inline constexpr uint8_t Socks5NoAuth = 0x00;
        inline constexpr uint8_t Socks5UserPass = 0x02;
        inline constexpr uint8_t Socks5NoAcceptable = 0xFF;
        inline constexpr uint8_t Socks5Connect = 0x01;
        inline constexpr uint8_t Socks5Ipv4 = 0x01;
        inline constexpr uint8_t Socks5Domain = 0x03;
        inline constexpr uint8_t Socks5Ipv6 = 0x04;

        // The error of a reply's REP field other than succeeded: the three
        // the system has a number for as that number, so that a connection
        // refused through the proxy reads as one refused directly; the
        // rest as net's own. A TTL expired is the proxy's attempt timing
        // out (ETIMEDOUT); a value RFC 1928 does not define is a failure
        SGCL_INLINE_HOT error_code socks5_reply_error(uint8_t rep) noexcept {
            switch (rep) {
                case 0x02: return make_error_code(errc::proxy_refused);
                case 0x03: return error_code(ENETUNREACH, std::system_category());
                case 0x04: return error_code(EHOSTUNREACH, std::system_category());
                case 0x05: return error_code(ECONNREFUSED, std::system_category());
                case 0x06: return error_code(ETIMEDOUT, std::system_category());
                case 0x07:
                case 0x08: return make_error_code(errc::proxy_unsupported);
                default: return make_error_code(errc::proxy_failure);
            }
        }

        // The CONNECT request of a target, "host:port" (RFC 1928 §4): an
        // IPv4 or IPv6 address as its bytes, anything else as a name of
        // 1 to 255 bytes for the proxy to resolve; nullopt for a target
        // that is not "host:port", an empty host, a name past 255 bytes,
        // an address with a zone (the protocol has no room for one)
        inline optional<std::string> socks5_request(std::string_view target) noexcept {
            auto hp = split_host_port(target);
            if (!hp || hp->host.empty()) {
                return nullopt;
            }
            auto port = parse_port(hp->port);
            if (!port) {
                return nullopt;
            }
            std::string r;
            r.reserve(7 + hp->host.size());
            r += char(Socks5Version);
            r += char(Socks5Connect);
            r += char(0);
            if (auto a = IpText::parse(hp->host)) {
                if (a->has_zone() || a->is_v6() != hp->bracketed) {
                    return nullopt;
                }
                auto b = a->bytes();
                if (a->is_v4()) {
                    r += char(Socks5Ipv4);
                    for (int i = 12; i < 16; ++i) {
                        r += char(b[size_t(i)]);
                    }
                } else {
                    r += char(Socks5Ipv6);
                    for (int i = 0; i < 16; ++i) {
                        r += char(b[size_t(i)]);
                    }
                }
            } else {
                if (hp->bracketed || hp->host.size() > 255) {
                    return nullopt;
                }
                r += char(Socks5Domain);
                r += char(hp->host.size());
                r += hp->host;
            }
            r += char(*port >> 8);
            r += char(*port & 0xFF);
            return r;
        }

        // The client's side of the handshake without its I/O: greeting()
        // is what is sent first; then each feed takes exactly want() bytes
        // of what the proxy sent and says whether more are wanted (with
        // bytes to send in `out`, the authentication), the tunnel is up,
        // or the handshake failed (error()). Never reads past the reply,
        // so the target's first bytes stay where they are
        class Socks5Handshake {
        public:
            enum class step : uint8_t { more, done, failed };

            // credentials: offered when the username is not empty (1 to
            // 255 bytes each; checked by the caller)
            SGCL_INLINE_HOT Socks5Handshake(std::string request, std::string_view username, std::string_view password) noexcept
            : _request(std::move(request))
            , _username(username)
            , _password(password) {
            }

            // The version and the methods offered: no authentication, and
            // username/password when there are credentials
            std::string greeting() const noexcept {
                std::string g;
                g += char(Socks5Version);
                if (_username.empty()) {
                    g += char(1);
                    g += char(Socks5NoAuth);
                } else {
                    g += char(2);
                    g += char(Socks5NoAuth);
                    g += char(Socks5UserPass);
                }
                return g;
            }

            SGCL_INLINE_HOT size_t want() const noexcept {
                return _want;
            }

            SGCL_INLINE_HOT error_code error() const noexcept {
                return _error;
            }

            // The address the proxy bound for the connection (BND.ADDR and
            // BND.PORT), once done: an endpoint for an address, none for a
            // name
            SGCL_INLINE_HOT optional<endpoint> bound() const noexcept {
                return _bound;
            }

            step feed(const uint8_t* p, size_t n, std::string& out) noexcept {
                out.clear();
                if (n != _want || _state == State::done || _state == State::failed) {
                    return _fail(errc::malformed_proxy_response);
                }
                switch (_state) {
                    case State::method:
                        // VER METHOD (§3)
                        if (p[0] != Socks5Version) {
                            return _fail(errc::malformed_proxy_response);
                        }
                        if (p[1] == Socks5NoAcceptable) {
                            return _fail(errc::proxy_auth_required);
                        }
                        if (p[1] == Socks5NoAuth) {
                            out = _request;
                            return _expect(State::head, 4);
                        }
                        if (p[1] == Socks5UserPass && !_username.empty()) {
                            // VER ULEN UNAME PLEN PASSWD (RFC 1929 §2)
                            out += char(1);
                            out += char(_username.size());
                            out += _username;
                            out += char(_password.size());
                            out += _password;
                            return _expect(State::auth, 2);
                        }
                        return _fail(errc::malformed_proxy_response);   // a method never offered
                    case State::auth:
                        // VER STATUS: any status but zero is a refusal; the
                        // version byte is not judged (servers answer 1 or 5)
                        if (p[1] != 0) {
                            return _fail(errc::proxy_auth_required);
                        }
                        out = _request;
                        return _expect(State::head, 4);
                    case State::head:
                        // VER REP RSV ATYP (§6); RSV not judged, as curl and Go
                        if (p[0] != Socks5Version) {
                            return _fail(errc::malformed_proxy_response);
                        }
                        if (p[1] != 0) {
                            _error = socks5_reply_error(p[1]);
                            _state = State::failed;
                            _want = 0;
                            return step::failed;
                        }
                        _atyp = p[3];
                        if (_atyp == Socks5Ipv4) {
                            return _expect(State::address, 4 + 2);
                        }
                        if (_atyp == Socks5Ipv6) {
                            return _expect(State::address, 16 + 2);
                        }
                        if (_atyp == Socks5Domain) {
                            return _expect(State::domain_length, 1);
                        }
                        return _fail(errc::malformed_proxy_response);
                    case State::domain_length:
                        return _expect(State::address, size_t(p[0]) + 2);
                    case State::address: {
                        uint16_t port = uint16_t((p[n - 2] << 8) | p[n - 1]);
                        if (_atyp == Socks5Ipv4) {
                            _bound = endpoint(ip_address::v4(p[0], p[1], p[2], p[3]), port);
                        } else if (_atyp == Socks5Ipv6) {
                            array<uint8_t, 16> b;
                            for (size_t i = 0; i < 16; ++i) {
                                b[i] = p[i];
                            }
                            _bound = endpoint(ip_address::v6(b), port);
                        }
                        _state = State::done;
                        _want = 0;
                        return step::done;
                    }
                    default:
                        return _fail(errc::malformed_proxy_response);
                }
            }

        private:
            enum class State : uint8_t { method, auth, head, domain_length, address, done, failed };

            SGCL_INLINE_HOT step _expect(State s, size_t n) noexcept {
                _state = s;
                _want = n;
                return step::more;
            }

            SGCL_INLINE_HOT step _fail(errc e) noexcept {
                _error = make_error_code(e);
                _state = State::failed;
                _want = 0;
                return step::failed;
            }

            std::string _request;
            std::string _username;
            std::string _password;
            State _state = State::method;
            size_t _want = 2;
            uint8_t _atyp = 0;
            error_code _error;
            optional<endpoint> _bound;
        };

        // "proxy->target", the path of the handshake's errors
        SGCL_INLINE_HOT string socks5_what(const string& proxy, const string& target) noexcept {
            return string::concat(proxy, "->", target);
        }

        inline io::error socks5_error(error_code code, const string& what) noexcept {
            return io::error(code, "socks5", what);
        }

        // What of a request is refused before a byte is sent: a target
        // that cannot be put in a CONNECT (net::errc::invalid_address),
        // credentials past RFC 1929's 255 bytes or a password without a
        // username (std::errc::invalid_argument)
        inline optional<io::error> socks5_refused(const string& target, const string& username, const string& password, const string& what) noexcept {
            if (!socks5_request(target.view())) {
                return net_error(errc::invalid_address, "socks5", what);
            }
            if (username.size() > 255 || password.size() > 255 || (username.empty() && !password.empty())) {
                return socks5_error(std::make_error_code(std::errc::invalid_argument), what);
            }
            return nullopt;
        }

        // The handshake over the connection to the proxy, within the
        // connection's deadlines; the connection closed when it fails
        inline async::task<expected<connection, io::error>> socks5_exchange(connection c, string target, string username, string password, string what) noexcept {
            if (auto e = socks5_refused(target, username, password, what)) {
                (void)c.close();
                co_return fail(*e);
            }
            auto request = socks5_request(target.view());
            Socks5Handshake h(std::move(*request), username.view(), password.view());
            tracked_ptr block = make_tracked<array<byte, 512>>();   // the longest reply: 4 + 1 + 255 + 2
            std::string out = h.greeting();
            for (;;) {
                if (!out.empty()) {
                    auto w = co_await c.async_write(string(std::string_view(out)));
                    if (!w) {
                        (void)c.close();
                        co_return fail(socks5_error(w.error().code(), what));
                    }
                }
                slice<byte> buf(block, block->data(), h.want());
                auto r = co_await c.async_read_full(buf);
                if (!r || *r == 0) {
                    (void)c.close();
                    co_return fail(socks5_error(r ? make_error_code(io::errc::unexpected_eof) : r.error().code(), what));
                }
                auto s = h.feed(reinterpret_cast<const uint8_t*>(block->data()), *r, out);
                if (s == Socks5Handshake::step::failed) {
                    (void)c.close();
                    co_return fail(socks5_error(h.error(), what));
                }
                if (s == Socks5Handshake::step::done) {
                    co_return c;
                }
            }
        }

        // What carries the handshake's result past a stop: the exchange's
        // task sends it here, room for it so that a send never waits
        struct Socks5Result {
            async::detail::ChannelState<expected<connection, io::error>> done{1};
        };

        inline async::task<void> socks5_run(tracked_ptr<Socks5Result> r, connection c, string target, string username, string password, string what) noexcept {
            auto got = co_await socks5_exchange(c, std::move(target), std::move(username), std::move(password), std::move(what));
            r->done.try_send(std::move(got));
        }

        // The handshake within the deadline (the connection's own, removed
        // at the end) and the stop: a stop closes the connection, which
        // ends the exchange, and is the error, ECANCELED
        inline async::task<expected<connection, io::error>> socks5_handshake(connection c, string target, string username, string password, async::stop_token stop, time_point deadline, string what) noexcept {
            if (stop.stop_requested()) {
                (void)c.close();
                co_return fail(socks5_error(error_code(ECANCELED, std::system_category()), what));
            }
            if (deadline != time_point() && sgcl::clock::now() >= deadline) {
                (void)c.close();
                co_return fail(socks5_error(error_code(ETIMEDOUT, std::system_category()), what));
            }
            c.set_deadline(deadline);
            expected<connection, io::error> got = connection();
            if (!stop.stop_possible()) {
                got = co_await socks5_exchange(c, std::move(target), std::move(username), std::move(password), what);
            } else {
                tracked_ptr r = make_tracked<Socks5Result>();
                async::go(socks5_run(r, c, std::move(target), std::move(username), std::move(password), what));
                bool stopped = false;
                optional<expected<connection, io::error>> result;
                co_await sgcl::async::select(r->done.on_receive([&](optional<expected<connection, io::error>> x) { result = std::move(x); }),
                                            stop.on_stop([&] { stopped = true; }));
                if (stopped || stop.stop_requested()) {   // the stop wins over a result that came with it
                    (void)c.close();
                    co_return fail(socks5_error(error_code(ECANCELED, std::system_category()), what));
                }
                got = std::move(*result);
            }
            if (got) {
                c.set_deadline(time_point());
            }
            co_return got;
        }

        SGCL_INLINE_HOT time_point socks5_deadline(duration timeout) noexcept {
            return timeout > duration::zero() ? no_deadline_at_max(sgcl::clock::now() + timeout) : time_point();
        }
    }

    // SOCKS5: connections through a proxy. A structure of static
    // functions, as tcp is: connect dials the proxy and asks it for the
    // target; client does the asking over a connection to a proxy there
    // is (one made through TLS, or through another proxy).
    struct socks5 {
        // How a connection is asked for: the credentials of RFC 1929
        // (offered when the username is not empty: 1 to 255 bytes, the
        // password 0 to 255), the longest the whole of it may take, a stop
        struct options {
            string username;
            string password;
            duration timeout = 30 * second;   // the dial and the handshake together; zero or less: none
            async::stop_token stop;
        };

        // The target, "host:port", through the proxy at "host:port": a name
        // resolved by the proxy, an address sent as one
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        SGCL_INLINE_HOT static expected<net::connection, io::error> connect(const string& proxy, const string& target) {
            return _co_connect(proxy, target, options()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> async_connect(string proxy, string target) noexcept {
            return _co_connect(std::move(proxy), std::move(target), options());
        }

        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        SGCL_INLINE_HOT static expected<net::connection, io::error> connect(const string& proxy, const string& target, const options& o) {
            return _co_connect(proxy, target, o).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> async_connect(string proxy, string target, options o) noexcept {
            return _co_connect(std::move(proxy), std::move(target), std::move(o));
        }

        // The handshake over a connection to a proxy there is (its
        // deadlines replaced by the handshake's and then removed); the
        // transport is closed when it fails
        // `client(...)` on this thread, `co_await async_client(...)` in a task
        SGCL_INLINE_HOT static expected<net::connection, io::error> client(const net::connection& transport, const string& target) {
            return _co_client(transport, target, options()).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> async_client(net::connection transport, string target) noexcept {
            return _co_client(std::move(transport), std::move(target), options());
        }

        // `client(...)` on this thread, `co_await async_client(...)` in a task
        SGCL_INLINE_HOT static expected<net::connection, io::error> client(const net::connection& transport, const string& target, const options& o) {
            return _co_client(transport, target, o).wait();
        }

        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> async_client(net::connection transport, string target, options o) noexcept {
            return _co_client(std::move(transport), std::move(target), std::move(o));
        }

    private:
        SGCL_INLINE_HOT static async::task<expected<net::connection, io::error>> _co_client(net::connection transport, string target, options o) noexcept {
            string what = net::detail::socks5_what(transport.remote_endpoint().to_string(), target);
            return net::detail::socks5_handshake(std::move(transport), std::move(target), std::move(o.username), std::move(o.password), std::move(o.stop),
                                                 net::detail::socks5_deadline(o.timeout), std::move(what));
        }

        static async::task<expected<net::connection, io::error>> _co_connect(string proxy, string target, options o) noexcept {
            if (auto e = net::detail::socks5_refused(target, o.username, o.password, net::detail::socks5_what(proxy, target))) {
                co_return net::detail::fail(*e);
            }
            const time_point deadline = net::detail::socks5_deadline(o.timeout);
            auto c = co_await net::detail::dial(proxy, o.stop, deadline);
            if (!c) {
                co_return net::detail::fail(c);
            }
            co_return co_await net::detail::socks5_handshake(*c, std::move(target), std::move(o.username), std::move(o.password), std::move(o.stop),
                                                             deadline, net::detail::socks5_what(proxy, target));
        }
    };
}
