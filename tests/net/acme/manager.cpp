//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// acme::manager against acme::test_server: a certificate obtained at the
// first handshake for a name (tls-alpn-01 on the server's own port), by
// http-01 through http_handler() on a port of its own, by dns-01 for a
// wildcard through the program's publisher; the cache (the files 0600) read
// by a second manager without an order; renewal by the CA's window (ARI)
// and at two thirds of a short lifetime without it; many first handshakes
// for one name making one order; the host policy, a hello without SNI, the
// terms of service, a failure's backoff, close(); every kind of
// certificate key.
#include "acme_test.h"

#include <atomic>
#include <sys/stat.h>
#include <thread>

using namespace sgcl;
using namespace acme_test;

namespace {
    // The server a manager feeds: a TCP listener made first (the CA must
    // know its port), TLS over it with the manager's config once the
    // manager is made, answering "ok"
    struct Site {
        optional<net::listener> tcp;
        optional<net::listener> tls;
        optional<async::task<>> serving;

        Site() {
            tcp = *net::tcp::listen("127.0.0.1:0");
        }

        void start(const net::acme::manager& m) {
            tls = *net::tls::detail::make_listener(*tcp, m.tls_config());
            serving = async::spawn([](net::listener l) -> async::task<> {
                for (;;) {
                    auto c = co_await l.async_accept();
                    if (!c) {
                        co_return;
                    }
                    (void)co_await c->async_write("ok\n");
                    (void)co_await c->async_close();
                }
            }(*tls));
        }

        ~Site() {
            if (tls) {
                (void)tls->close();
                serving->wait();
            } else {
                (void)tcp->close();
            }
        }

        uint16_t port() const {
            return tcp->local_endpoint().port();
        }

        // A handshake for the name, the chain verified against the CA's
        // roots: the leaf served
        expected<crypto::x509::certificate, io::error> connect(const net::acme::test_server& ca, const std::string& name) const {
            net::tls::config c;
            c.server_name = string(name);
            c.roots = ca.roots();
            auto conn = net::tls::connect(string("127.0.0.1:" + std::to_string(port())), c);
            if (!conn) {
                return unexpected(conn.error());
            }
            auto line = conn->read_line();
            EXPECT_TRUE(line && *line && **line == "ok");
            return net::tls::state_of(*conn)->peer_certificates[0];
        }
    };

    net::acme::manager::options manager_options(const net::acme::test_server& ca) {
        net::acme::manager::options o;
        o.directory_url = ca.directory_url();
        o.accept_terms = true;
        o.contact = {"mailto:admin@example.test"};
        return o;
    }

    // A directory of the test's own, removed at its end
    struct TempDir {
        string path = io::make_temp_dir().value();

        ~TempDir() {
            (void)io::remove_all(path);
        }
    };

    template<class F>
    bool eventually(F f, std::chrono::milliseconds limit = std::chrono::milliseconds(15000)) {
        auto end = std::chrono::steady_clock::now() + limit;
        while (std::chrono::steady_clock::now() < end) {
            if (f()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return f();
    }
}

TEST(AcmeManager, ACertificateAtTheFirstHandshake) {
    Site site;
    net::acme::test_server::options so;
    so.tls_port = site.port();
    net::acme::test_server ca(so);
    net::acme::manager m({"example.test", "www.example.test"}, manager_options(ca));
    site.start(m);
    auto leaf = site.connect(ca, "example.test");
    ASSERT_TRUE(leaf.has_value()) << text(leaf.error().message());
    ASSERT_EQ(leaf->dns_names().size(), 1u);
    EXPECT_EQ(text(leaf->dns_names()[0]), "example.test");
    EXPECT_EQ(ca.orders(), 1u);
    // the second handshake from memory, another name its own order
    auto again = site.connect(ca, "example.test");
    ASSERT_TRUE(again.has_value());
    EXPECT_TRUE(*again == *leaf);
    EXPECT_EQ(ca.orders(), 1u);
    auto www = site.connect(ca, "WWW.example.test");
    ASSERT_TRUE(www.has_value()) << text(www.error().message());
    EXPECT_EQ(text(www->dns_names()[0]), "www.example.test");
    EXPECT_EQ(ca.orders(), 2u);
    // certificate() gives the same identity
    auto id = m.certificate("example.test");
    ASSERT_TRUE(id.has_value());
    EXPECT_TRUE(id->certificates()[0] == *leaf);
    // the account's client
    auto c = m.client();
    ASSERT_TRUE(c.has_value());
    EXPECT_FALSE(c->account_url().empty());
    m.close();
}

TEST(AcmeManager, Http01ThroughTheHandler) {
    net::http::server http;
    auto l = *net::tcp::listen("127.0.0.1:0");
    net::acme::test_server::options so;
    so.http_port = l.local_endpoint().port();
    net::acme::test_server ca(so);
    auto mo = manager_options(ca);
    mo.challenges = {"http-01"};
    net::acme::manager m({"example.test"}, mo);
    http.route("/", m.http_handler());
    auto serving = async::spawn(http.async_serve(l));
    auto id = m.certificate("example.test");
    ASSERT_TRUE(id.has_value()) << text(id.error().message());
    EXPECT_EQ(text(id->certificates()[0].dns_names()[0]), "example.test");
    // what else the handler answers: a redirect to https, 400, 404
    net::http::client web;
    web.max_redirects = 0;
    web.proxy = net::http::proxy();
    std::string base = "http://127.0.0.1:" + std::to_string(l.local_endpoint().port());
    // the redirect read as it comes (the client would follow it)
    auto raw = net::tcp::connect(string("127.0.0.1:" + std::to_string(l.local_endpoint().port())));
    ASSERT_TRUE(raw.has_value());
    ASSERT_TRUE(raw->write("GET /a/b?x=1 HTTP/1.1\r\nHost: example.test:8080\r\nConnection: close\r\n\r\n").has_value());
    auto answer = io::read_all(*raw);
    ASSERT_TRUE(answer.has_value());
    std::string head(reinterpret_cast<const char*>(answer->data()), answer->size());
    EXPECT_EQ(head.substr(0, 12), "HTTP/1.1 302");
    EXPECT_NE(head.find("Location: https://example.test/a/b?x=1\r\n"), std::string::npos) << head;
    auto post = web.post(string(base + "/form"), "text/plain", "x");
    ASSERT_TRUE(post.has_value());
    EXPECT_EQ(post->status(), 400);
    auto unknown = web.get(string(base + "/.well-known/acme-challenge/nothing"));
    ASSERT_TRUE(unknown.has_value());
    EXPECT_EQ(unknown->status(), 404);
    m.close();
    http.close();
    (void)serving.wait();
}

TEST(AcmeManager, AWildcardByDns01) {
    tracked_ptr<Board> board = make_tracked<Board>();
    Site site;
    net::acme::test_server::options so;
    so.tls_port = site.port();
    tracked_ptr<Board> b = board;
    so.lookup_txt = [b](const string& name) -> async::task<expected<vector<string>, io::error>> {
        std::lock_guard<std::mutex> g(b->lock);
        auto it = b->txt.find(name);
        co_return it == b->txt.end() ? vector<string>() : it->second;
    };
    net::acme::test_server ca(so);
    auto mo = manager_options(ca);
    std::atomic<int> published = 0, removed = 0;
    mo.dns_publish = [b, &published](const string& name, const string& value) -> async::task<expected<void, io::error>> {
        std::lock_guard<std::mutex> g(b->lock);
        b->txt[name].push_back(value);
        published.fetch_add(1);
        co_return expected<void, io::error>();
    };
    mo.dns_cleanup = [b, &removed](const string& name, const string&) -> async::task<expected<void, io::error>> {
        std::lock_guard<std::mutex> g(b->lock);
        b->txt.erase(name);
        removed.fetch_add(1);
        co_return expected<void, io::error>();
    };
    net::acme::manager m({"*.example.test"}, mo);
    site.start(m);
    auto a = site.connect(ca, "a.example.test");
    ASSERT_TRUE(a.has_value()) << text(a.error().message());
    EXPECT_EQ(text(a->dns_names()[0]), "*.example.test");
    auto other = site.connect(ca, "b.example.test");
    ASSERT_TRUE(other.has_value());
    EXPECT_TRUE(*other == *a);
    EXPECT_EQ(ca.orders(), 1u);
    EXPECT_EQ(published.load(), 1);
    EXPECT_EQ(removed.load(), 1);
    // two labels below is not the wildcard's
    auto deep = site.connect(ca, "x.y.example.test");
    EXPECT_FALSE(deep.has_value());
    // without a publisher a wildcard cannot be had
    auto no_dns = manager_options(ca);
    net::acme::manager m2({"*.other.test"}, no_dns);
    auto none = m2.certificate("a.other.test");
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code(), net::acme::errc::no_challenge);
    m.close();
    m2.close();
}

TEST(AcmeManager, TheCacheIsReadOnTheNextStart) {
    TempDir dir;
    Site site;
    net::acme::test_server::options so;
    so.tls_port = site.port();
    net::acme::test_server ca(so);
    auto mo = manager_options(ca);
    mo.cache = io::path::join(dir.path, "certs");
    optional<crypto::x509::certificate> first;
    {
        net::acme::manager m({"example.test"}, mo);
        site.start(m);
        auto leaf = site.connect(ca, "example.test");
        ASSERT_TRUE(leaf.has_value()) << text(leaf.error().message());
        first = *leaf;
        m.close();
    }
    EXPECT_EQ(ca.orders(), 1u);
    // the files: the account's key and the certificate's key and chain, 0600
    for (const char* f : {"acme_account+key", "example.test"}) {
        struct stat st;
        std::string path = text(io::path::join(mo.cache, f));
        ASSERT_EQ(::stat(path.c_str(), &st), 0) << path;
        EXPECT_EQ(st.st_mode & 0777, 0600u) << path;
    }
    struct stat d;
    ASSERT_EQ(::stat(text(mo.cache).c_str(), &d), 0);
    EXPECT_EQ(d.st_mode & 0777, 0700u);
    // a second manager of the cache: the same certificate, no order, the same account
    net::acme::manager again({"example.test"}, mo);
    auto id = again.certificate("example.test");
    ASSERT_TRUE(id.has_value()) << text(id.error().message());
    EXPECT_TRUE(id->certificates()[0] == *first);
    EXPECT_EQ(ca.orders(), 1u);
    auto c = again.client();
    ASSERT_TRUE(c.has_value());
    auto key = crypto::read_secret(io::path::join(mo.cache, "acme_account+key"));
    ASSERT_TRUE(key.has_value());
    EXPECT_TRUE(c->key() == net::acme::account_key::from_pem(*key).value());
    again.close();
}

TEST(AcmeManager, RenewalByTheCasWindow) {
    Site site;
    net::acme::test_server::options so;
    so.tls_port = site.port();
    net::acme::test_server ca(so);
    // every certificate due now
    auto now = time::now();
    ca.set_renewal_window(now - std::chrono::hours(2), now - std::chrono::hours(1));
    net::acme::manager m({"example.test"}, manager_options(ca));
    site.start(m);
    auto first = site.connect(ca, "example.test");
    ASSERT_TRUE(first.has_value()) << text(first.error().message());
    // renewed in the background, the new certificate served
    ASSERT_TRUE(eventually([&] { return ca.certificates() >= 2; }));
    optional<crypto::x509::certificate> renewed;
    ASSERT_TRUE(eventually([&] {
        auto leaf = site.connect(ca, "example.test");
        if (leaf && !(*leaf == *first)) {
            renewed = *leaf;
            return true;
        }
        return false;
    }));
    EXPECT_EQ(text(renewed->dns_names()[0]), "example.test");
    m.close();
}

TEST(AcmeManager, RenewalAtTwoThirdsWithoutRenewalInformation) {
    Site site;
    net::acme::test_server::options so;
    so.tls_port = site.port();
    so.renewal_info = false;
    so.certificate_lifetime = std::chrono::seconds(3);
    net::acme::test_server ca(so);
    net::acme::manager m({"example.test"}, manager_options(ca));
    site.start(m);
    auto first = site.connect(ca, "example.test");
    ASSERT_TRUE(first.has_value()) << text(first.error().message());
    auto start = std::chrono::steady_clock::now();
    ASSERT_TRUE(eventually([&] { return ca.certificates() >= 2; }));
    EXPECT_GE(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(1000));
    m.close();
}

TEST(AcmeManager, ManyFirstHandshakesMakeOneOrder) {
    Site site;
    net::acme::test_server::options so;
    so.tls_port = site.port();
    net::acme::test_server ca(so);
    net::acme::manager m({"example.test"}, manager_options(ca));
    site.start(m);
    std::atomic<int> ok = 0;
    {
        async::wait_group wg;
        for (int i = 0; i < 16; ++i) {
            wg.add();
            net::tls::config c;
            c.server_name = "example.test";
            c.roots = ca.roots();
            c.handshake_timeout = std::chrono::seconds(30);
            async::go([](net::tls::config c, string where, async::wait_group wg, std::atomic<int>* ok) -> async::task<> {
                auto conn = co_await net::tls::async_connect(where, c);
                if (conn) {
                    ok->fetch_add(1);
                }
                wg.done();
            }(c, string("127.0.0.1:" + std::to_string(site.port())), wg, &ok));
        }
        wg.wait();
    }
    EXPECT_EQ(ok.load(), 16);
    EXPECT_EQ(ca.orders(), 1u);
    m.close();
}

TEST(AcmeManager, TheHostPolicy) {
    Site site;
    net::acme::test_server::options so;
    so.tls_port = site.port();
    net::acme::test_server ca(so);
    auto mo = manager_options(ca);
    mo.host_policy = [](const string& host) {
        return host.view().ends_with(".allowed.test");
    };
    mo.default_name = "example.test";
    net::acme::manager m({"example.test"}, mo);
    site.start(m);
    auto refused = site.connect(ca, "other.test");
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(net::tls::alert_of(refused.error()), net::tls::alert::internal_error);
    auto direct = m.certificate("other.test");
    ASSERT_FALSE(direct.has_value());
    EXPECT_EQ(direct.error().code(), net::acme::errc::host_not_allowed);
    auto allowed = site.connect(ca, "x.allowed.test");
    ASSERT_TRUE(allowed.has_value()) << text(allowed.error().message());
    EXPECT_EQ(ca.orders(), 1u);
    // a hello without SNI: default_name's certificate
    net::tls::config by_address;
    by_address.server_name = "127.0.0.1";
    by_address.insecure_skip_verify = true;
    auto c = net::tls::connect(string("127.0.0.1:" + std::to_string(site.port())), by_address);
    ASSERT_TRUE(c.has_value()) << text(c.error().message());
    EXPECT_EQ(text(net::tls::state_of(*c)->peer_certificates[0].dns_names()[0]), "example.test");
    // a manager with neither names nor policy refuses everything
    net::acme::manager empty(manager_options(ca));
    auto e = empty.certificate("example.test");
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code(), net::acme::errc::host_not_allowed);
    m.close();
}

TEST(AcmeManager, AFailureIsKeptForItsBackoff) {
    Site site;
    net::acme::test_server::options so;
    so.tls_port = site.port();
    net::acme::test_server ca(so);
    net::acme::manager m({"example.test"}, manager_options(ca));
    site.start(m);
    ca.fail_next(net::acme::errc::server_internal, 1, "new-order");
    auto first = m.certificate("example.test");
    ASSERT_FALSE(first.has_value());
    EXPECT_EQ(first.error().code(), net::acme::errc::server_internal);
    // within the backoff: the same error, no request
    auto second = m.certificate("example.test");
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error().code(), net::acme::errc::server_internal);
    EXPECT_EQ(ca.orders(), 0u);
    m.close();
}

TEST(AcmeManager, TheTermsAndClose) {
    net::acme::test_server::options so;
    so.terms_of_service = "https://ca.test/terms";
    so.skip_validation = true;
    net::acme::test_server ca(so);
    auto mo = manager_options(ca);
    mo.accept_terms = false;
    net::acme::manager m({"example.test"}, mo);
    auto refused = m.certificate("example.test");
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code(), net::acme::errc::malformed);
    mo.accept_terms = true;
    net::acme::manager ok({"example.test", "two.test"}, mo);
    auto id = ok.certificate("example.test");
    ASSERT_TRUE(id.has_value()) << text(id.error().message());
    ok.close();
    // after close: what is held is served, nothing new is obtained
    auto held = ok.certificate("example.test");
    ASSERT_TRUE(held.has_value());
    auto none = ok.certificate("two.test");
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code(), io::errc::closed);
}

TEST(AcmeManager, EveryKindOfCertificateKey) {
    net::acme::test_server::options so;
    so.skip_validation = true;
    net::acme::test_server ca(so);
    for (auto k : {net::acme::key_algorithm::es256, net::acme::key_algorithm::es384, net::acme::key_algorithm::eddsa, net::acme::key_algorithm::rs256}) {
        auto mo = manager_options(ca);
        mo.certificate_key = k;
        mo.key = net::acme::account_key(net::acme::key_algorithm::es384);
        net::acme::manager m({"example.test"}, mo);
        auto id = m.certificate("example.test");
        ASSERT_TRUE(id.has_value()) << text(id.error().message());
        auto kind = id->certificates()[0].public_key().kind();
        EXPECT_EQ(kind, k == net::acme::key_algorithm::es256 ? crypto::x509::key_kind::p256
                      : k == net::acme::key_algorithm::es384 ? crypto::x509::key_kind::p384
                      : k == net::acme::key_algorithm::eddsa ? crypto::x509::key_kind::ed25519
                                                             : crypto::x509::key_kind::rsa);
        m.close();
    }
}
