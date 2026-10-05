//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// https with client certificates and resumed sessions: the module's client
// carrying an identity in its TLS settings, the server's handler reading
// the client's chain and the resumption through request::tls(), over
// HTTP/1.1 and HTTP/2; the client's own session cache shared by its
// connections, none (nullopt), one given to two clients; request::tls() of
// a plain request and of a client's.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/http.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace sgcl;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    std::string testdata(const std::string& name) {
        return (source_root() / "tests/net/tls_testdata" / name).string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    net::tls::identity identity_of(const std::string& name) {
        return net::tls::identity(sgcl::string(slurp(testdata(name + ".pem"))), sgcl::string(slurp(testdata(name + ".key"))));
    }

    crypto::x509::certificate_pool ca() {
        return crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
    }

    // The handler: who the client is, whether its session was resumed, the protocol
    net::http::server whoami() {
        net::http::server s;
        s.route("GET /whoami", [](net::http::request req, net::http::response_writer w) {
            auto st = req.tls();
            if (!st) {
                w.write("plain\n");
                return;
            }
            std::string who = st->peer_certificates.empty() ? "nobody" : text(st->peer_certificates[0].subject().common_name());
            w.write(sgcl::string(who + (st->resumed ? " resumed " : " full ") + text(req.proto()) + "\n"));
        });
        return s;
    }

    struct Running {
        net::http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        uint16_t port = 0;

        Running(const net::tls::config& cfg, bool secure = true)
        : server(whoami()) {
            if (secure) {
                auto l = net::tls::listen("127.0.0.1:0", cfg);
                EXPECT_TRUE(l.has_value());
                listener = *l;
            } else {
                listener = *net::tcp::listen("127.0.0.1:0");
            }
            port = listener.local_endpoint().port();
            serving = async::spawn(server.async_serve(listener));
        }

        sgcl::string url(const char* scheme = "https") const {
            return sgcl::string(std::string(scheme) + "://localhost:" + std::to_string(port) + "/whoami");
        }

        ~Running() {
            server.close();
            (void)serving.wait();
        }
    };

    net::tls::config server_tls(bool http2) {
        net::tls::config c;
        c.identities = {identity_of("ecdsa")};
        c.alpn = http2 ? vector<sgcl::string>{sgcl::string("h2"), sgcl::string("http/1.1")} : vector<sgcl::string>{sgcl::string("http/1.1")};
        c.client_auth = net::tls::client_auth::require;
        c.client_roots = ca();
        return c;
    }

    std::string get(const net::http::client& c, const sgcl::string& url) {
        auto res = c.get(url);
        if (!res) {
            return "error: " + text(res.error().message());
        }
        auto body = res->text();
        return body ? text(*body) : "error: body";
    }
}

TEST(HttpTlsAuth, TheClientsCertificateAndItsSessionInTheHandler) {
    for (bool http2 : {false, true}) {
        SCOPED_TRACE(http2);
        Running r(server_tls(http2));
        net::http::client c;
        c.tls.roots = ca();
        c.tls.identities = {identity_of("client_ecdsa")};
        c.http2 = http2;
        const std::string proto = http2 ? "HTTP/2.0" : "HTTP/1.1";
        EXPECT_EQ(get(c, r.url()), "sgcl test client ecdsa full " + proto + "\n");
        EXPECT_EQ(get(c, r.url()), "sgcl test client ecdsa full " + proto + "\n");   // the pool's connection
        c.close_idle_connections();
        // a new connection: the session of the first resumed, the client's chain kept
        EXPECT_EQ(get(c, r.url()), "sgcl test client ecdsa resumed " + proto + "\n");
    }
}

TEST(HttpTlsAuth, AClientWithoutACertificate) {
    Running r(server_tls(false));
    net::http::client c;
    c.tls.roots = ca();
    std::string got = get(c, r.url());
    EXPECT_NE(got.find("certificate required"), std::string::npos) << got;
}

TEST(HttpTlsAuth, TheSessionCacheOfTheClient) {
    net::tls::config cfg = server_tls(false);
    cfg.client_auth = net::tls::client_auth::none;
    Running r(cfg);
    // none: every connection a full handshake
    net::http::client none;
    none.tls.roots = ca();
    none.tls.session_cache = nullopt;
    EXPECT_EQ(get(none, r.url()), "nobody full HTTP/1.1\n");
    none.close_idle_connections();
    EXPECT_EQ(get(none, r.url()), "nobody full HTTP/1.1\n");
    // the default: a cache of the client's own, two clients apart
    net::http::client a, b;
    a.tls.roots = ca();
    b.tls.roots = ca();
    EXPECT_EQ(get(a, r.url()), "nobody full HTTP/1.1\n");
    EXPECT_EQ(get(b, r.url()), "nobody full HTTP/1.1\n");
    // one given to both: the second resumes the first's session
    net::tls::session_cache shared;
    net::http::client x, y;
    x.tls.roots = ca();
    y.tls.roots = ca();
    x.tls.session_cache = shared;
    y.tls.session_cache = shared;
    EXPECT_EQ(get(x, r.url()), "nobody full HTTP/1.1\n");
    EXPECT_EQ(shared.size(), 1u);
    EXPECT_EQ(get(y, r.url()), "nobody resumed HTTP/1.1\n");
    // a copy of a client is the same client, its cache the same
    net::http::client copy = a;
    a.close_idle_connections();
    EXPECT_EQ(get(copy, r.url()), "nobody resumed HTTP/1.1\n");
}

TEST(HttpTlsAuth, TheTlsOfAPlainRequestAndOfAClientsRequest) {
    Running r(net::tls::config(), false);
    net::http::client c;
    EXPECT_EQ(get(c, r.url("http")), "plain\n");
    net::http::request req("GET", "https://localhost/");
    EXPECT_FALSE(req.tls().has_value());
}
