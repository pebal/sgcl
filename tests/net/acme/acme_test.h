//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the ACME tests share: the solvers of the three challenges on the
// loopback (an http server for http-01, a TLS listener for tls-alpn-01, a
// board of TXT records for dns-01) and a test_server that validates at
// them.
#pragma once

#include "tests/types.h"
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/acme.h"
#include "sgcl/net/http.h"
#include "sgcl/net/tls.h"

#include <mutex>
#include <string>

namespace acme_test {
    using namespace sgcl;

    inline std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // What the solvers answer: http-01's key authorizations by token,
    // tls-alpn-01's identities by name, dns-01's TXT records by name
    struct Board {
        std::mutex lock;
        map<string, string> http;
        map<string, optional<net::tls::identity>> alpn;
        map<string, vector<string>> txt;
        int http_requests = 0;
        int alpn_hellos = 0;
    };

    // The three solvers on the loopback, their ports known before the CA
    // is made
    struct Solvers {
        tracked_ptr<Board> board = make_tracked<Board>();
        net::http::server http;
        optional<net::listener> http_listener;
        optional<net::listener> tls_listener;
        optional<async::task<expected<void, io::error>>> http_serving;
        optional<async::task<>> tls_serving;

        Solvers() {
            tracked_ptr<Board> b = board;
            http.route("GET /.well-known/acme-challenge/{token}", [b](net::http::request req, net::http::response_writer w) {
                std::lock_guard<std::mutex> g(b->lock);
                ++b->http_requests;
                auto it = b->http.find(req.path_value("token"));
                if (it == b->http.end()) {
                    w.error(404);
                    return;
                }
                w.write(it->second);
            });
            http.on_error = [](const string&) {};
            http_listener = *net::tcp::listen("127.0.0.1:0");
            http_serving = async::spawn(http.async_serve(*http_listener));
            net::tls::config c;
            c.alpn = {"acme-tls/1"};
            c.identity_for = [b](const net::tls::client_hello& h) -> async::task<expected<net::tls::identity, io::error>> {
                std::lock_guard<std::mutex> g(b->lock);
                ++b->alpn_hellos;
                auto it = b->alpn.find(h.server_name);
                if (it == b->alpn.end() || !it->second) {
                    co_return unexpected(io::error(std::make_error_code(std::errc::no_such_file_or_directory), "alpn", h.server_name));
                }
                co_return *it->second;
            };
            tls_listener = *net::tls::listen("127.0.0.1:0", c);
            tls_serving = async::spawn([](net::listener l) -> async::task<> {
                for (;;) {
                    auto c = co_await l.async_accept();
                    if (!c) {
                        co_return;
                    }
                    (void)co_await c->async_close();
                }
            }(*tls_listener));
        }

        ~Solvers() {
            http.close();
            (void)http_serving->wait();
            (void)tls_listener->close();
            tls_serving->wait();
        }

        uint16_t http_port() const {
            return http_listener->local_endpoint().port();
        }

        uint16_t tls_port() const {
            return tls_listener->local_endpoint().port();
        }

        // A test_server's options validating at these solvers
        net::acme::test_server::options server_options() const {
            net::acme::test_server::options o;
            o.http_port = http_port();
            o.tls_port = tls_port();
            tracked_ptr<Board> b = board;
            o.lookup_txt = [b](const string& name) -> async::task<expected<vector<string>, io::error>> {
                std::lock_guard<std::mutex> g(b->lock);
                auto it = b->txt.find(name);
                if (it == b->txt.end()) {
                    co_return vector<string>();
                }
                co_return it->second;
            };
            return o;
        }

        // The challenge of the type answered by the solver: the value put
        // where the CA looks, the challenge accepted
        expected<net::acme::challenge, io::error> solve(const net::acme::client& c, const net::acme::authorization& az, const std::string& type) const {
            for (auto& ch : az.challenges) {
                if (text(ch.type) != type) {
                    continue;
                }
                {
                    std::lock_guard<std::mutex> g(board->lock);
                    if (type == "http-01") {
                        board->http[ch.token] = c.key_authorization(ch.token);
                    } else if (type == "dns-01") {
                        board->txt[net::acme::client::dns01_name(az.identifier.value)].push_back(c.dns01_value(ch.token));
                    }
                }
                if (type == "tls-alpn-01") {
                    auto id = c.tls_alpn01_identity(ch.token, az.identifier.value);
                    if (!id) {
                        return unexpected(id.error());
                    }
                    std::lock_guard<std::mutex> g(board->lock);
                    board->alpn[az.identifier.value] = *id;
                }
                return c.accept(ch);
            }
            return unexpected(io::error(net::acme::errc::no_challenge, "solve", string(type)));
        }
    };

    // A CSR of the names on a new P-256 key
    inline crypto::x509::certificate_request csr_of(const std::vector<std::string>& names) {
        auto key = crypto::p256::private_key::generate();
        crypto::x509::certificate_request_template t;
        for (auto& n : names) {
            if (auto ip = net::ip_address::parse(string(n))) {
                crypto::x509::ip_address a;
                auto b = ip->bytes();
                size_t from = ip->is_v4() ? 12 : 0;
                a.size = uint8_t(16 - from);
                for (size_t i = from; i < 16; ++i) {
                    a.bytes[i - from] = byte(b[i]);
                }
                t.ip_addresses.push_back(a);
            } else {
                t.dns_names.push_back(string(n));
            }
        }
        return crypto::x509::create_certificate_request(t, key);
    }

    // A client with an account registered at the server
    inline net::acme::client registered(const net::acme::test_server& ca, const net::acme::account_key& key = net::acme::account_key()) {
        net::acme::client c(ca.directory_url(), key);
        auto a = c.register_account({.contact = {"mailto:test@example.test"}, .terms_agreed = true});
        EXPECT_TRUE(a.has_value()) << (a ? "" : text(a.error().message()));
        return c;
    }

    // An order of the names taken to its certificate, each authorization
    // by the type
    inline expected<net::acme::certificate_chain, io::error> issue(const net::acme::client& c, const Solvers& solvers, const std::vector<std::string>& names,
                                                                   const std::string& type, const net::acme::order_options& oo = {}) {
        vector<string> list;
        for (auto& n : names) {
            list.push_back(string(n));
        }
        auto o = c.new_order(list, oo);
        if (!o) {
            return unexpected(o.error());
        }
        for (auto& url : o->authorizations) {
            auto az = c.authorization(url);
            if (!az) {
                return unexpected(az.error());
            }
            if (az->status == net::acme::status::valid) {
                continue;
            }
            auto ch = solvers.solve(c, *az, az->wildcard ? "dns-01" : type);
            if (!ch) {
                return unexpected(ch.error());
            }
            auto done = c.wait_authorization(url);
            if (!done) {
                return unexpected(done.error());
            }
        }
        auto ready = c.wait_order(o->url);
        if (!ready) {
            return unexpected(ready.error());
        }
        auto fin = c.finalize(*ready, csr_of(names));
        if (!fin) {
            return unexpected(fin.error());
        }
        return c.certificate(fin->certificate);
    }

    inline bool verifies(const net::acme::test_server& ca, const net::acme::certificate_chain& chain, const std::string& name) {
        crypto::x509::verify_options o;
        o.roots = ca.roots();
        crypto::x509::certificate_pool inter;
        for (size_t i = 1; i < chain.certificates.size(); ++i) {
            inter.add(chain.certificates[i]);
        }
        o.intermediates = inter;
        array<uint8_t, 16> b = {};
        size_t from = 0;
        if (auto ip = net::ip_address::parse(string(name))) {
            b = ip->bytes();
            from = ip->is_v4() ? 12 : 0;
            o.ip = slice<const byte>(reinterpret_cast<const byte*>(b.data()) + from, 16 - from);
        } else {
            o.dns_name = string(name);
        }
        auto v = chain.certificates[0].verify(o);
        return v.has_value();
    }
}
