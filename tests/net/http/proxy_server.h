//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// An HTTP forward proxy for the client's tests, on raw connections (its
// heads read and written here, so that a test sees the request line the
// client sent): a request in absolute-form forwarded to its origin as it
// came (in origin-form, one request a connection there) and its answer
// given back with a Content-Length, the client's connection kept; CONNECT tunnelled (RFC 9110 §9.3.6); Basic
// Proxy-Authorization required when credentials are set (407 without).
// What it does can be bent: a CONNECT refused with a status, answered with
// bytes of the test's own, a proxy that says nothing. Over TLS when it is
// given a listener of net::tls.
#pragma once

#include "sgcl/encoding.h"
#include "sgcl/net/http/http.h"
#include "sgcl/net/net.h"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace sgcl_test {
    using namespace sgcl;

    struct HttpProxyServer {
        struct Behavior {
            std::string username;              // Basic credentials required when not empty
            std::string password;
            int connect_status = 200;          // a CONNECT answered with this status (and its connection closed when not 2xx)
            std::string connect_answer;        // a CONNECT answered with these bytes, then the connection closed
            bool silent = false;               // a request read, nothing answered
        };

        Behavior behavior;
        net::listener listener;
        std::atomic<int> connections{0};
        std::atomic<int> connects{0};
        std::atomic<int> forwarded{0};
        std::mutex lock;
        std::vector<std::string> request_lines;
        std::string last_authorization;         // the last Proxy-Authorization received
        std::string last_host_field;

        uint16_t port() const {
            return listener.local_endpoint().port();
        }

        // "http://127.0.0.1:port"
        sgcl::string url(const char* scheme = "http", const char* host = "127.0.0.1") const {
            return sgcl::string(std::string(scheme) + "://" + host + ":" + std::to_string(port()));
        }

        void close() {
            if (listener) {
                listener.close();
            }
        }

        std::vector<std::string> lines() {
            std::lock_guard g(lock);
            return request_lines;
        }

        std::string authorization() {
            std::lock_guard g(lock);
            return last_authorization;
        }

        static sgcl::tracked_ptr<HttpProxyServer> start() {
            return start(Behavior());
        }

        static sgcl::tracked_ptr<HttpProxyServer> start(const Behavior& b) {
            auto l = net::tcp::listen("127.0.0.1:0");
            return l ? start(b, *l) : make_tracked<HttpProxyServer>();
        }

        // over a listener of the test's (one of net::tls for an https:// proxy)
        static sgcl::tracked_ptr<HttpProxyServer> start(const Behavior& b, const net::listener& l) {
            sgcl::tracked_ptr s = make_tracked<HttpProxyServer>();
            s->behavior = b;
            s->listener = l;
            async::go(accept_loop(s));
            return s;
        }

        static async::task<> accept_loop(sgcl::tracked_ptr<HttpProxyServer> s) {
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

        static std::string field(const std::vector<std::pair<std::string, std::string>>& fields, const char* name) {
            for (auto& f : fields) {
                if (net::http::detail::iequal(f.first, name)) {
                    return f.second;
                }
            }
            return std::string();
        }

        static async::task<> serve(sgcl::tracked_ptr<HttpProxyServer> s, net::connection c) {
            const Behavior& b = s->behavior;
            sgcl::tracked_ptr block = make_tracked<array<byte, 8192>>();
            std::string pending;
            auto more = [&]() -> async::task<bool> {
                auto n = co_await c.async_read(slice<byte>(block, block->data(), block->size()));
                if (!n || *n == 0) {
                    co_return false;
                }
                pending.append(reinterpret_cast<const char*>(block->data()), *n);
                co_return true;
            };
            for (;;) {
                size_t end;
                while ((end = pending.find("\r\n\r\n")) == std::string::npos) {
                    if (!co_await more()) {
                        (void)c.close();
                        co_return;
                    }
                }
                std::string head = pending.substr(0, end);
                pending.erase(0, end + 4);
                std::vector<std::pair<std::string, std::string>> fields;
                size_t eol = head.find("\r\n");
                std::string line = head.substr(0, eol);
                size_t at = eol == std::string::npos ? head.size() : eol + 2;
                while (at < head.size()) {
                    size_t next = head.find("\r\n", at);
                    if (next == std::string::npos) {
                        next = head.size();
                    }
                    std::string f = head.substr(at, next - at);
                    size_t colon = f.find(':');
                    if (colon != std::string::npos) {
                        size_t v = colon + 1;
                        while (v < f.size() && f[v] == ' ') {
                            ++v;
                        }
                        fields.emplace_back(f.substr(0, colon), f.substr(v));
                    }
                    at = next + 2;
                }
                {
                    std::lock_guard g(s->lock);
                    s->request_lines.push_back(line);
                    s->last_authorization = field(fields, "Proxy-Authorization");
                    s->last_host_field = field(fields, "Host");
                }
                if (b.silent) {
                    (void)co_await c.async_read_all();
                    (void)c.close();
                    co_return;
                }
                size_t sp1 = line.find(' '), sp2 = line.rfind(' ');
                if (sp1 == std::string::npos || sp2 <= sp1) {
                    (void)co_await send(c, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    (void)c.close();
                    co_return;
                }
                std::string method = line.substr(0, sp1);
                std::string target = line.substr(sp1 + 1, sp2 - sp1 - 1);
                // the body, by its length (the client's bodies in memory)
                size_t length = 0;
                if (auto cl = field(fields, "Content-Length"); !cl.empty()) {
                    length = std::stoul(cl);
                }
                while (pending.size() < length) {
                    if (!co_await more()) {
                        (void)c.close();
                        co_return;
                    }
                }
                std::string body = pending.substr(0, length);
                pending.erase(0, length);
                if (!b.username.empty()) {
                    std::string want = "Basic " + std::string(encoding::base64::standard.encode(sgcl::string(b.username + ":" + b.password)).view());
                    if (field(fields, "Proxy-Authorization") != want) {
                        if (!co_await send(c, "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm=\"test\"\r\nContent-Length: 0\r\n\r\n")) {
                            co_return;
                        }
                        continue;
                    }
                }
                if (method == "CONNECT") {
                    ++s->connects;
                    if (!b.connect_answer.empty()) {
                        (void)co_await send(c, b.connect_answer);
                        (void)c.close();
                        co_return;
                    }
                    if (b.connect_status < 200 || b.connect_status >= 300) {
                        (void)co_await send(c, "HTTP/1.1 " + std::to_string(b.connect_status) + " " + net::http::reason(b.connect_status) +
                                                   "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                        (void)c.close();
                        co_return;
                    }
                    auto t = co_await net::tcp::async_connect(sgcl::string(target), 5 * second);
                    if (!t) {
                        (void)co_await send(c, "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                        (void)c.close();
                        co_return;
                    }
                    if (!co_await send(c, "HTTP/1.1 " + std::to_string(b.connect_status) + " Connection established\r\n\r\n")) {
                        (void)t->close();
                        co_return;
                    }
                    if (!pending.empty()) {
                        (void)co_await send(*t, pending);
                    }
                    auto back = async::spawn(pipe(*t, c));
                    co_await pipe(c, *t);
                    co_await back;
                    (void)c.close();
                    (void)t->close();
                    co_return;
                }
                if (target.rfind("http://", 0) != 0) {
                    (void)co_await send(c, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    (void)c.close();
                    co_return;
                }
                ++s->forwarded;
                // forwarded as it came, in origin-form, one request a
                // connection there; the answer read to the close and given
                // back with its length (de-chunked), the client's connection kept
                std::string rest = target.substr(7);
                size_t slash = rest.find('/');
                std::string authority = slash == std::string::npos ? rest : rest.substr(0, slash);
                std::string path = slash == std::string::npos ? "/" : rest.substr(slash);
                if (authority.find(':') == std::string::npos || authority.back() == ']') {
                    authority += ":80";
                }
                std::string out = method + " " + path + " HTTP/1.1\r\n";
                for (auto& f : fields) {
                    if (net::http::detail::iequal(f.first, "proxy-authorization") || net::http::detail::iequal(f.first, "proxy-connection")
                        || net::http::detail::iequal(f.first, "connection")) {
                        continue;
                    }
                    out += f.first + ": " + f.second + "\r\n";
                }
                out += "Connection: close\r\n\r\n" + body;
                auto origin = co_await net::tcp::async_connect(sgcl::string(authority), 5 * second);
                if (!origin || !co_await send(*origin, out)) {
                    if (origin) {
                        (void)origin->close();
                    }
                    if (!co_await send(c, "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n")) {
                        co_return;
                    }
                    continue;
                }
                auto all = co_await origin->async_read_all();
                (void)origin->close();
                std::string raw = all ? std::string(reinterpret_cast<const char*>(all->data()), all->size()) : std::string();
                size_t head_end = raw.find("\r\n\r\n");
                std::string ahead = head_end == std::string::npos ? raw : raw.substr(0, head_end);
                std::string abody = head_end == std::string::npos ? std::string() : raw.substr(head_end + 4);
                std::string answer;
                bool chunked = false;
                std::string length_field;
                size_t p = 0;
                while (p < ahead.size()) {
                    size_t q = ahead.find("\r\n", p);
                    if (q == std::string::npos) {
                        q = ahead.size();
                    }
                    std::string l = ahead.substr(p, q - p);
                    p = q + 2;
                    if (answer.empty()) {
                        answer = l + "\r\n";
                        continue;
                    }
                    std::string name = l.substr(0, l.find(':'));
                    if (net::http::detail::iequal(name, "transfer-encoding")) {
                        chunked = true;
                        continue;
                    }
                    if (net::http::detail::iequal(name, "content-length")) {
                        length_field = l;
                        continue;
                    }
                    if (net::http::detail::iequal(name, "connection")) {
                        continue;
                    }
                    answer += l + "\r\n";
                }
                if (chunked) {
                    std::string decoded;
                    size_t at2 = 0;
                    for (;;) {
                        size_t eol2 = abody.find("\r\n", at2);
                        if (eol2 == std::string::npos) {
                            break;
                        }
                        size_t n = std::stoul(abody.substr(at2, eol2 - at2), nullptr, 16);
                        if (n == 0) {
                            break;
                        }
                        decoded += abody.substr(eol2 + 2, n);
                        at2 = eol2 + 2 + n + 2;
                    }
                    abody = decoded;
                }
                answer += "X-Forwarded-By: test-proxy\r\n";
                if (method == "HEAD") {
                    answer += (length_field.empty() ? std::string("Content-Length: 0") : length_field) + "\r\n\r\n";
                } else {
                    answer += "Content-Length: " + std::to_string(abody.size()) + "\r\n\r\n" + abody;
                }
                if (!co_await send(c, answer)) {
                    co_return;
                }
            }
        }
    };
}
