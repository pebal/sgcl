//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A SOCKS5 server for the tests, written from RFC 1928 and RFC 1929 alone
// (not from the module's client): CONNECT to IPv4, IPv6 and names (resolved
// here), no authentication or username/password, then the bytes relayed
// both ways. Its behaviour can be bent to test the client's failures: a
// reply code, a method never offered, versions that are wrong, a server
// that says nothing, a reply cut short, bytes of the target sent with the
// reply. It records what it was asked for. Shared by tests_net (socks5.cpp)
// and tests_http (the client's proxies).
#pragma once

#include "sgcl/net/net.h"

#include <atomic>
#include <mutex>
#include <string>

namespace sgcl_test {
    using namespace sgcl;

    struct Socks5TestServer {
        // what the server does; set before connections come
        struct Behavior {
            std::string username;              // username/password required when not empty
            std::string password;
            int reply = 0;                     // REP of the CONNECT's reply (0: succeeded, the target dialed)
            int method_version = 5;            // the version of the method selection reply
            int force_method = -1;             // the method chosen whatever was offered
            int auth_version = 1;              // the version of RFC 1929's reply
            int reply_version = 5;             // the version of the CONNECT's reply
            int bound_atyp = 1;                // the address type of BND.ADDR (1, 3, 4; anything else sent as it is)
            bool silent = false;               // the greeting read, nothing answered
            bool close_after_greeting = false; // the greeting read, the connection closed
            bool short_reply = false;          // the reply's head sent, then the connection closed
            std::string with_reply;            // bytes sent right after the reply, in its write (the target's first)
        };

        Behavior behavior;
        net::listener listener;
        std::atomic<int> connections{0};
        std::atomic<int> tunnels{0};
        std::mutex lock;
        int last_atyp = 0;                     // the CONNECT's ATYP
        std::string last_host;                 // its host: an address as text, a name as sent
        int last_port = 0;
        std::string last_methods;              // the methods the greeting offered

        // "127.0.0.1:port"
        sgcl::string address() const {
            return listener.local_endpoint().to_string();
        }

        uint16_t port() const {
            return listener.local_endpoint().port();
        }

        void close() {
            if (listener) {
                listener.close();
            }
        }

        static sgcl::tracked_ptr<Socks5TestServer> start() {
            return start(Behavior());
        }

        static sgcl::tracked_ptr<Socks5TestServer> start(const Behavior& b) {
            sgcl::tracked_ptr s = make_tracked<Socks5TestServer>();
            s->behavior = b;
            auto l = net::tcp::listen("127.0.0.1:0");
            if (!l) {
                return s;
            }
            s->listener = *l;
            async::go(accept_loop(s));
            return s;
        }

        static async::task<> accept_loop(sgcl::tracked_ptr<Socks5TestServer> s) {
            for (;;) {
                auto c = co_await s->listener.async_accept();
                if (!c) {
                    co_return;
                }
                ++s->connections;
                async::go(serve(s, *c));
            }
        }

        static async::task<> pipe(net::connection from, net::connection to) {
            (void)co_await from.async_copy_to(to);
            (void)to.close_write();
        }

        static async::task<bool> send(net::connection c, const std::string& bytes) {
            auto w = co_await c.async_write(sgcl::string(std::string_view(bytes)));
            co_return (bool)w;
        }

        static async::task<> serve(sgcl::tracked_ptr<Socks5TestServer> s, net::connection c) {
            const Behavior& b = s->behavior;
            sgcl::tracked_ptr block = make_tracked<array<byte, 1024>>();
            auto at = [&](size_t i) { return uint8_t((*block)[i]); };
            auto read = [&](size_t n) -> async::task<bool> {
                if (n == 0) {
                    co_return true;
                }
                auto r = co_await c.async_read_full(slice<byte>(block, block->data(), n));
                co_return r && *r == n;
            };
            // VER NMETHODS METHODS
            if (!co_await read(2) || at(0) != 5) {
                (void)c.close();
                co_return;
            }
            size_t nmethods = at(1);
            if (!co_await read(nmethods)) {
                (void)c.close();
                co_return;
            }
            std::string methods;
            for (size_t i = 0; i < nmethods; ++i) {
                methods += char(at(i));
            }
            {
                std::lock_guard g(s->lock);
                s->last_methods = methods;
            }
            if (b.close_after_greeting) {
                (void)c.close();
                co_return;
            }
            if (b.silent) {
                (void)co_await c.async_read_all();   // until the client goes
                (void)c.close();
                co_return;
            }
            int method = 0xFF;
            if (b.force_method >= 0) {
                method = b.force_method;
            } else if (!b.username.empty()) {
                method = methods.find(char(2)) != std::string::npos ? 2 : 0xFF;
            } else {
                method = methods.find(char(0)) != std::string::npos ? 0 : 0xFF;
            }
            if (!co_await send(c, std::string{char(b.method_version), char(method)}) || method == 0xFF) {
                (void)c.close();
                co_return;
            }
            if (method == 2 && b.force_method < 0) {
                // VER ULEN UNAME PLEN PASSWD
                if (!co_await read(2)) {
                    (void)c.close();
                    co_return;
                }
                size_t ulen = at(1);
                if (!co_await read(ulen + 1)) {
                    (void)c.close();
                    co_return;
                }
                std::string user;
                for (size_t i = 0; i < ulen; ++i) {
                    user += char(at(i));
                }
                size_t plen = at(ulen);
                if (!co_await read(plen)) {
                    (void)c.close();
                    co_return;
                }
                std::string pass;
                for (size_t i = 0; i < plen; ++i) {
                    pass += char(at(i));
                }
                bool ok = user == b.username && pass == b.password;
                if (!co_await send(c, std::string{char(b.auth_version), char(ok ? 0 : 1)}) || !ok) {
                    (void)c.close();
                    co_return;
                }
            }
            // VER CMD RSV ATYP DST.ADDR DST.PORT
            if (!co_await read(4)) {
                (void)c.close();
                co_return;
            }
            int cmd = at(1);
            int atyp = at(3);
            std::string host;
            if (atyp == 1) {
                if (!co_await read(4)) {
                    (void)c.close();
                    co_return;
                }
                host = net::ip_address::v4(at(0), at(1), at(2), at(3)).to_string().view();
            } else if (atyp == 4) {
                if (!co_await read(16)) {
                    (void)c.close();
                    co_return;
                }
                array<uint8_t, 16> a;
                for (size_t i = 0; i < 16; ++i) {
                    a[i] = at(i);
                }
                host = net::ip_address::v6(a).to_string().view();
            } else if (atyp == 3) {
                if (!co_await read(1)) {
                    (void)c.close();
                    co_return;
                }
                size_t len = at(0);
                if (!co_await read(len)) {
                    (void)c.close();
                    co_return;
                }
                for (size_t i = 0; i < len; ++i) {
                    host += char(at(i));
                }
            } else {
                (void)co_await send(c, std::string{5, 8, 0, 1, 0, 0, 0, 0, 0, 0});
                (void)c.close();
                co_return;
            }
            if (!co_await read(2)) {
                (void)c.close();
                co_return;
            }
            int port = (at(0) << 8) | at(1);
            {
                std::lock_guard g(s->lock);
                s->last_atyp = atyp;
                s->last_host = host;
                s->last_port = port;
            }
            int rep = b.reply;
            if (cmd != 1) {
                rep = 7;
            }
            net::connection target;
            if (rep == 0) {
                std::string address = atyp == 4 ? "[" + host + "]:" + std::to_string(port) : host + ":" + std::to_string(port);
                auto t = co_await net::tcp::async_connect(sgcl::string(std::string_view(address)), 5 * second);
                if (!t) {
                    rep = t.error().code() == std::errc::connection_refused ? 5 : 4;
                } else {
                    target = *t;
                }
            }
            std::string reply{char(b.reply_version), char(rep), 0};
            if (b.bound_atyp == 1) {
                reply += std::string{1, 127, 0, 0, 1};
            } else if (b.bound_atyp == 4) {
                reply += char(4);
                reply += std::string(15, '\0');
                reply += char(1);
            } else if (b.bound_atyp == 3) {
                reply += std::string{3, 9};
                reply += "localhost";
            } else {
                reply += char(b.bound_atyp);
            }
            if (b.short_reply) {
                (void)co_await send(c, reply.substr(0, 4));
                (void)c.close();
                if (target) {
                    (void)target.close();
                }
                co_return;
            }
            reply += std::string{char(0x1F), char(0x90)};   // BND.PORT 8080
            if (rep != 0) {
                (void)co_await send(c, reply);
                (void)c.close();
                co_return;
            }
            reply += b.with_reply;
            if (!co_await send(c, reply)) {
                (void)c.close();
                (void)target.close();
                co_return;
            }
            ++s->tunnels;
            auto back = async::spawn(pipe(target, c));
            co_await pipe(c, target);
            co_await back;
            (void)c.close();
            (void)target.close();
        }
    };

    // An echo server on the loopback: what each connection sends comes back
    struct EchoServer {
        net::listener listener;

        static async::task<> echo(net::connection c) {
            (void)co_await c.async_copy_to(c);
            (void)c.close();
        }

        static async::task<> loop(sgcl::tracked_ptr<EchoServer> s) {
            for (;;) {
                auto c = co_await s->listener.async_accept();
                if (!c) {
                    co_return;
                }
                async::go(echo(*c));
            }
        }

        static sgcl::tracked_ptr<EchoServer> start(const char* address = "127.0.0.1:0") {
            sgcl::tracked_ptr s = make_tracked<EchoServer>();
            auto l = net::tcp::listen(address);
            if (!l) {
                return s;
            }
            s->listener = *l;
            async::go(loop(s));
            return s;
        }

        uint16_t port() const {
            return listener.local_endpoint().port();
        }

        void close() {
            if (listener) {
                listener.close();
            }
        }
    };
}
