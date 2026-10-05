//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// tls::config::identity_for, a server's identity chosen per hello (Go's
// GetCertificate): the name and the protocols the hello asked for given to
// the function, its identity served (by the listener, by server() on a
// thread, by async_server() in a task), an error or an exception of it
// ending the handshake with internal_error, a slow one bounded by the
// handshake's timeout, many hellos at once, resumption beside it, a hello
// without SNI, the identities of the config left for it; acme-tls/1 as
// tls-alpn-01 negotiates it.
#include "tests/types.h"
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

#include <atomic>
#include <string>
#include <thread>

using namespace sgcl;

namespace {
    // A CA and identities of names it issues, made here
    struct Ca {
        crypto::p256::private_key key = crypto::p256::private_key::generate();
        optional<crypto::x509::certificate> root;
        crypto::x509::certificate_pool pool;

        Ca() {
            crypto::x509::certificate_template t;
            t.common_name = "identity_for test root";
            t.is_ca = true;
            root = crypto::x509::create_certificate(t, key);
            pool.add(*root);
        }

        net::tls::identity issue(const std::string& name) const {
            auto k = crypto::p256::private_key::generate();
            crypto::x509::certificate_template t;
            t.dns_names = {string(name)};
            auto spki = k.public_key().to_pkix_der();
            auto leaf = crypto::x509::create_certificate(t, spki.as_slice(), *root, key);
            auto pem = encoding::pem("CERTIFICATE", vector<byte>(leaf.raw().begin(), leaf.raw().end())).to_string();
            auto key_pem = k.to_pem();
            return net::tls::identity(pem, key_pem.as_slice());
        }
    };

    net::tls::config client_for(const Ca& ca, const std::string& name) {
        net::tls::config c;
        c.server_name = string(name);
        c.roots = ca.pool;
        return c;
    }

    // The leaf's first DNS name of what the server sent
    std::string served(const net::connection& c) {
        auto st = net::tls::state_of(c);
        if (!st || st->peer_certificates.empty() || st->peer_certificates[0].dns_names().empty()) {
            return "";
        }
        auto& n = st->peer_certificates[0].dns_names()[0];
        return std::string(n.data(), n.size());
    }

    // An accept loop answering "ok" on every connection
    async::task<> answer(net::listener l) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            (void)co_await c->async_write("ok\n");
            (void)co_await c->async_close();
        }
    }

    // One line of a connection, "" at its end or on an error
    std::string line_of(const net::connection& c) {
        auto l = c.read_line();
        if (!l || !*l) {
            return "";
        }
        return std::string((*l)->data(), (*l)->size());
    }

    std::string at(const net::listener& l) {
        return "127.0.0.1:" + std::to_string(l.local_endpoint().port());
    }
}

TEST(TlsIdentityFor, TheListenerServesEachNameItsIdentity) {
    Ca ca;
    auto a = ca.issue("a.test");
    auto b = ca.issue("b.test");
    std::atomic<int> calls = 0;
    net::tls::config sc;
    sc.alpn = {"h2", "http/1.1"};
    sc.identity_for = [a, b, &calls](const net::tls::client_hello& h) -> async::task<expected<net::tls::identity, io::error>> {
        calls.fetch_add(1);
        EXPECT_EQ(h.alpn.size(), 1u);
        if (h.alpn.size() == 1) {
            EXPECT_EQ(std::string(h.alpn[0].view()), "http/1.1");
        }
        co_return h.server_name == "a.test" ? a : b;
    };
    auto l = net::tls::listen("127.0.0.1:0", sc);
    ASSERT_TRUE(l.has_value()) << l.error().message().view();
    auto serving = async::spawn(answer(*l));
    for (std::string name : {"a.test", "b.test", "a.test"}) {
        auto cc = client_for(ca, name);
        cc.alpn = {"http/1.1"};
        auto c = net::tls::connect(string(at(*l)), cc);
        ASSERT_TRUE(c.has_value()) << c.error().message().view();
        EXPECT_EQ(served(*c), name);
        EXPECT_EQ(line_of(*c), "ok");
        EXPECT_EQ(net::tls::state_of(*c)->alpn, "http/1.1");
    }
    EXPECT_EQ(calls.load(), 3);
    (void)l->close();
    serving.wait();
}

TEST(TlsIdentityFor, AnErrorEndsTheHandshakeAndTheListenerGoesOn) {
    Ca ca;
    auto a = ca.issue("a.test");
    net::tls::config sc;
    sc.identity_for = [a](const net::tls::client_hello& h) -> async::task<expected<net::tls::identity, io::error>> {
        if (h.server_name == "refused.test") {
            co_return unexpected(io::error(std::make_error_code(std::errc::permission_denied), "identity_for", h.server_name));
        }
        if (h.server_name == "throws.test") {
            throw std::runtime_error("an identity_for that throws");
        }
        co_return a;
    };
    auto l = net::tls::listen("127.0.0.1:0", sc);
    ASSERT_TRUE(l.has_value());
    auto serving = async::spawn(answer(*l));
    for (std::string name : {"refused.test", "throws.test"}) {
        auto c = net::tls::connect(string(at(*l)), client_for(ca, name));
        ASSERT_FALSE(c.has_value());
        EXPECT_EQ(net::tls::alert_of(c.error()), net::tls::alert::internal_error) << c.error().message().view();
        EXPECT_TRUE(net::tls::is_remote(c.error()));
    }
    auto ok = net::tls::connect(string(at(*l)), client_for(ca, "a.test"));
    ASSERT_TRUE(ok.has_value()) << ok.error().message().view();
    EXPECT_EQ(served(*ok), "a.test");
    (void)l->close();
    serving.wait();
}

TEST(TlsIdentityFor, ServerOnAThreadAndAsyncServerInATask) {
    Ca ca;
    auto a = ca.issue("a.test");
    net::tls::config sc;
    sc.identity_for = [a](const net::tls::client_hello&) -> async::task<expected<net::tls::identity, io::error>> {
        co_await async::sleep(std::chrono::milliseconds(5));   // a choice that waits
        co_return a;
    };
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l.has_value());
    // server() blocking on a thread of its own
    std::thread t([&] {
        auto c = l->accept();
        ASSERT_TRUE(c.has_value());
        auto s = net::tls::server(*c, sc);
        ASSERT_TRUE(s.has_value()) << s.error().message().view();
        EXPECT_EQ(net::tls::state_of(*s)->server_name, "a.test");
        (void)s->write("one\n");
        (void)s->close();
    });
    auto c1 = net::tls::connect(string(at(*l)), client_for(ca, "a.test"));
    ASSERT_TRUE(c1.has_value()) << c1.error().message().view();
    EXPECT_EQ(line_of(*c1), "one");
    t.join();
    // async_server in a task
    auto serving = async::spawn([](net::listener l, net::tls::config sc) -> async::task<> {
        auto c = co_await l.async_accept();
        if (!c) {
            co_return;
        }
        auto s = co_await net::tls::async_server(*c, sc);
        EXPECT_TRUE(s.has_value());
        if (s) {
            (void)co_await s->async_write("two\n");
            (void)co_await s->async_close();
        }
    }(*l, sc));
    auto c2 = net::tls::connect(string(at(*l)), client_for(ca, "a.test"));
    ASSERT_TRUE(c2.has_value()) << c2.error().message().view();
    EXPECT_EQ(line_of(*c2), "two");
    serving.wait();
    (void)l->close();
}

TEST(TlsIdentityFor, AConfigNeedsIdentitiesOrIdentityFor) {
    net::tls::config sc;
    auto l = net::tls::listen("127.0.0.1:0", sc);
    ASSERT_FALSE(l.has_value());
    EXPECT_EQ(l.error().code(), std::errc::invalid_argument);
    sc.identity_for = [](const net::tls::client_hello&) -> async::task<expected<net::tls::identity, io::error>> {
        co_return unexpected(io::error(std::make_error_code(std::errc::permission_denied), "identity_for"));
    };
    auto l2 = net::tls::listen("127.0.0.1:0", sc);
    ASSERT_TRUE(l2.has_value());
    (void)l2->close();
}

TEST(TlsIdentityFor, ASlowChoiceIsBoundedByTheHandshakeTimeout) {
    Ca ca;
    auto a = ca.issue("a.test");
    net::tls::config sc;
    sc.handshake_timeout = std::chrono::milliseconds(300);
    sc.identity_for = [a](const net::tls::client_hello&) -> async::task<expected<net::tls::identity, io::error>> {
        co_await async::sleep(std::chrono::milliseconds(1500));
        co_return a;
    };
    auto l = net::tcp::listen("127.0.0.1:0");
    ASSERT_TRUE(l.has_value());
    auto serving = async::spawn([](net::listener l, net::tls::config sc) -> async::task<expected<net::connection, io::error>> {
        auto c = co_await l.async_accept();
        if (!c) {
            co_return unexpected(c.error());
        }
        co_return co_await net::tls::async_server(*c, sc);
    }(*l, sc));
    auto cc = client_for(ca, "a.test");
    cc.handshake_timeout = std::chrono::seconds(5);
    auto c = net::tls::connect(string(at(*l)), cc);
    EXPECT_FALSE(c.has_value());
    auto s = serving.wait();
    ASSERT_FALSE(s.has_value());
    (void)l->close();
}

TEST(TlsIdentityFor, ManyHellosAtOnce) {
    Ca ca;
    auto a = ca.issue("a.test");
    std::atomic<int> calls = 0;
    net::tls::config sc;
    sc.identity_for = [a, &calls](const net::tls::client_hello&) -> async::task<expected<net::tls::identity, io::error>> {
        calls.fetch_add(1);
        co_await async::yield();
        co_return a;
    };
    auto l = net::tls::listen("127.0.0.1:0", sc);
    ASSERT_TRUE(l.has_value());
    auto serving = async::spawn(answer(*l));
    const std::string where = at(*l);
    std::atomic<int> ok = 0;
    {
        async::wait_group wg;
        for (int i = 0; i < 32; ++i) {
            wg.add();
            async::go([](net::tls::config cc, string where, async::wait_group wg, std::atomic<int>* ok) -> async::task<> {
                auto c = co_await net::tls::async_connect(where, cc);
                if (c) {
                    auto line = co_await c->async_read_line();
                    if (line && *line && **line == "ok") {
                        ok->fetch_add(1);
                    }
                }
                wg.done();
            }(client_for(ca, "a.test"), string(where), wg, &ok));
        }
        wg.wait();
    }
    EXPECT_EQ(ok.load(), 32);
    EXPECT_EQ(calls.load(), 32);
    (void)l->close();
    serving.wait();
}

TEST(TlsIdentityFor, ResumptionAndAHelloWithoutSni) {
    Ca ca;
    auto a = ca.issue("a.test");
    std::atomic<int> no_sni = 0;
    net::tls::config sc;
    sc.identity_for = [a, &no_sni](const net::tls::client_hello& h) -> async::task<expected<net::tls::identity, io::error>> {
        if (h.server_name.empty()) {
            no_sni.fetch_add(1);
        }
        co_return a;
    };
    auto l = net::tls::listen("127.0.0.1:0", sc);
    ASSERT_TRUE(l.has_value());
    auto serving = async::spawn(answer(*l));
    auto cc = client_for(ca, "a.test");
    cc.session_cache = net::tls::session_cache();
    for (int i = 0; i < 2; ++i) {
        auto c = net::tls::connect(string(at(*l)), cc);
        ASSERT_TRUE(c.has_value()) << c.error().message().view();
        EXPECT_EQ(line_of(*c), "ok");
        EXPECT_EQ(net::tls::state_of(*c)->resumed, i == 1);
    }
    // an address as the client's name: no SNI is sent (RFC 6066 §3)
    net::tls::config by_address;
    by_address.server_name = "127.0.0.1";
    by_address.insecure_skip_verify = true;
    auto c = net::tls::connect(string(at(*l)), by_address);
    ASSERT_TRUE(c.has_value()) << c.error().message().view();
    EXPECT_EQ(no_sni.load(), 1);
    (void)l->close();
    serving.wait();
}

TEST(TlsIdentityFor, AcmeTls1IsNegotiatedForTheValidator) {
    Ca ca;
    auto a = ca.issue("a.test");
    auto challenge = ca.issue("a.test");
    net::tls::config sc;
    sc.alpn = {"h2", "http/1.1", "acme-tls/1"};
    sc.identity_for = [a, challenge](const net::tls::client_hello& h) -> async::task<expected<net::tls::identity, io::error>> {
        co_return h.alpn.size() == 1 && h.alpn[0] == "acme-tls/1" ? challenge : a;
    };
    auto l = net::tls::listen("127.0.0.1:0", sc);
    ASSERT_TRUE(l.has_value());
    auto serving = async::spawn(answer(*l));
    auto validator = client_for(ca, "a.test");
    validator.alpn = {"acme-tls/1"};
    auto c = net::tls::connect(string(at(*l)), validator);
    ASSERT_TRUE(c.has_value()) << c.error().message().view();
    auto st = net::tls::state_of(*c);
    EXPECT_EQ(st->alpn, "acme-tls/1");
    EXPECT_TRUE(st->peer_certificates[0] == challenge.certificates()[0]);
    auto browser = client_for(ca, "a.test");
    browser.alpn = {"h2", "http/1.1"};
    auto b = net::tls::connect(string(at(*l)), browser);
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(net::tls::state_of(*b)->alpn, "h2");
    EXPECT_TRUE(net::tls::state_of(*b)->peer_certificates[0] == a.certificates()[0]);
    (void)l->close();
    serving.wait();
}
