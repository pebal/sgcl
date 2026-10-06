//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// tls::config::ech_from_dns: the ECHConfigList taken from the server name's
// DNS HTTPS record (RFC 9460, RFC 9848) and Encrypted Client Hello made with
// it, against our server with ECH keys and the DNS server of
// tests/net/dns_server.h, both on the loopback: accepted (connect and
// client, the thread's and the task's forms), the name of a port other
// than 443 ("_port._https.name"), off by default, the first usable record,
// and the cases that go on without ECH — no record, a name that does not
// exist, a DNS server that does not answer or refuses, a list of no config
// the module can use, a mandatory key the client does not know, a server
// name that is an address, a config without TLS 1.3.
#include "tests/types.h"
#include "tests/net/dns_server.h"

#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

#include <string>

using namespace sgcl;
namespace tls = sgcl::net::tls;

namespace {
    using dns_test::Behaviour;
    using dns_test::Rr;
    using dns_test::Server;

    // A CA and identities of the names it issues, made here
    struct Ca {
        crypto::p256::private_key key = crypto::p256::private_key::generate();
        optional<crypto::x509::certificate> root;
        crypto::x509::certificate_pool pool;

        Ca() {
            crypto::x509::certificate_template t;
            t.common_name = "ech dns test root";
            t.is_ca = true;
            root = crypto::x509::create_certificate(t, key);
            pool.add(*root);
        }

        tls::identity issue(const std::string& name) const {
            auto k = crypto::p256::private_key::generate();
            crypto::x509::certificate_template t;
            t.dns_names = {string(name)};
            auto spki = k.public_key().to_pkix_der();
            auto leaf = crypto::x509::create_certificate(t, spki.as_slice(), *root, key);
            auto pem = encoding::pem("CERTIFICATE", vector<byte>(leaf.raw().begin(), leaf.raw().end())).to_string();
            return tls::identity(pem, k.to_pem().as_slice());
        }
    };

    std::string text(const string& s) {
        return std::string(s.data(), s.size());
    }

    std::string u16(unsigned v) {
        return std::string{char(v >> 8), char(v & 0xFF)};
    }

    std::string param(unsigned key, const std::string& value) {
        return u16(key) + u16(unsigned(value.size())) + value;
    }

    std::string bytes(const vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    // An HTTPS record: priority 1, target ".", the SvcParams given
    Rr https(const std::string& owner, const std::string& params, unsigned priority = 1) {
        Rr r;
        r.owner = owner;
        r.type = sgcl::net::detail::dns_type::https;
        r.raw = u16(priority) + std::string(1, '\0') + params;
        return r;
    }

    async::task<> serve(net::listener l) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            (void)co_await c->async_write(string("hi\n"));
            (void)co_await c->async_close();
        }
    }

    // Our server of public.test and secret.test with one ECH key, and the
    // name of its HTTPS record
    struct EchSite {
        Ca ca;
        tls::ech_key key = tls::ech_key::generate("public.test");
        net::listener listener;
        async::task<> serving;
        uint16_t port = 0;

        EchSite() {
            tls::config sc;
            sc.identities = {ca.issue("public.test"), ca.issue("secret.test")};
            sc.ech_keys = {key};
            listener = tls::listen("127.0.0.1:0", sc).value();
            port = listener.local_endpoint().port();
            serving = async::spawn(serve(listener));
        }

        ~EchSite() {
            (void)listener.close();
            serving.wait();
        }

        std::string address() const {
            return "127.0.0.1:" + std::to_string(port);
        }

        std::string record_name() const {
            return "_" + std::to_string(port) + "._https.secret.test.";
        }

        std::string ech() const {
            return bytes(tls::ech_config_list({key}));
        }

        tls::config client(const Server& dns) const {
            tls::config c;
            c.server_name = "secret.test";
            c.roots = ca.pool;
            c.ech_from_dns = true;
            c.ech_dns = dns.options();
            return c;
        }
    };

    bool accepted(const expected<net::connection, io::error>& c) {
        auto st = c ? tls::state_of(*c) : nullopt;
        return st && st->ech_accepted;
    }

    int https_queries(Behaviour& b) {
        int n = 0;
        for (auto& q : b.queries()) {
            n += q.ends_with(" 65");
        }
        return n;
    }
}

TEST(TlsEchDns_Tests, AcceptedFromTheRecord) {
    EchSite s;
    Behaviour b;
    b.zone = {https(s.record_name(), param(1, "\x02h2") + param(5, s.ech()))};
    Server dns(b);
    ASSERT_TRUE(dns.ok());
    auto c = tls::connect(string(s.address()), s.client(dns));
    ASSERT_TRUE(c.has_value()) << text(c.error().message());
    EXPECT_TRUE(accepted(c));
    EXPECT_EQ(text(tls::state_of(*c)->server_name), "secret.test");
    EXPECT_EQ(b.queries(), (std::vector<std::string>{s.record_name() + " 65"}));   // the port's name (RFC 9460 §9.1)
    (void)c->close();
    // the task's form
    auto t = tls::async_connect(string(s.address()), s.client(dns)).wait();
    ASSERT_TRUE(t.has_value()) << text(t.error().message());
    EXPECT_TRUE(accepted(t));
    (void)t->close();
    // over a transport there is: the remote port's name
    auto transport = net::tcp::connect(string(s.address()));
    ASSERT_TRUE(transport.has_value());
    auto over = tls::client(*transport, s.client(dns));
    ASSERT_TRUE(over.has_value()) << text(over.error().message());
    EXPECT_TRUE(accepted(over));
    (void)over->close();
    auto transport2 = net::tcp::connect(string(s.address()));
    ASSERT_TRUE(transport2.has_value());
    auto over2 = tls::async_client(*transport2, s.client(dns)).wait();
    ASSERT_TRUE(over2.has_value());
    EXPECT_TRUE(accepted(over2));
    (void)over2->close();
    EXPECT_EQ(https_queries(b), 4);
    // a list given wins: no lookup
    auto given = s.client(dns);
    given.ech_config_list = tls::ech_config_list({s.key});
    auto g = tls::connect(string(s.address()), given);
    ASSERT_TRUE(g.has_value());
    EXPECT_TRUE(accepted(g));
    EXPECT_EQ(https_queries(b), 4);
}

TEST(TlsEchDns_Tests, OffByDefault) {
    EchSite s;
    Behaviour b;
    b.zone = {https(s.record_name(), param(5, s.ech()))};
    Server dns(b);
    ASSERT_TRUE(dns.ok());
    tls::config c = s.client(dns);
    c.ech_from_dns = false;
    EXPECT_FALSE(tls::config().ech_from_dns);
    auto conn = tls::connect(string(s.address()), c);
    ASSERT_TRUE(conn.has_value());
    EXPECT_FALSE(accepted(conn));
    EXPECT_TRUE(b.queries().empty());
}

// The first record with a list the client can use, in priority order
TEST(TlsEchDns_Tests, TheFirstUsableRecord) {
    EchSite s;
    Behaviour b;
    b.zone = {
        https(s.record_name(), param(1, "\x02h2"), 1),                                     // no ech
        https(s.record_name(), param(0, u16(9)) + param(5, s.ech()) + param(9, "x"), 2),   // a mandatory key not known
        https(s.record_name(), param(5, "\x00\x04garb"), 3),                               // no config that reads
        https(s.record_name(), param(5, s.ech()), 4),
    };
    Server dns(b);
    ASSERT_TRUE(dns.ok());
    auto c = tls::connect(string(s.address()), s.client(dns));
    ASSERT_TRUE(c.has_value()) << text(c.error().message());
    EXPECT_TRUE(accepted(c));
}

// Whatever DNS says or fails to say, the handshake goes on without ECH
TEST(TlsEchDns_Tests, WithoutARecordNoEch) {
    EchSite s;
    Behaviour b;
    b.zone = {
        dns_test::rr_a("secret.test.", 127, 0, 0, 1),
        https("_" + std::to_string(s.port) + "._https.unusable.test.", param(5, "\x00\x04garb")),
        https("_" + std::to_string(s.port) + "._https.required.test.", param(0, u16(9)) + param(5, s.ech()) + param(9, "")),
    };
    b.rcode["_" + std::to_string(s.port) + "._https.failing.test."] = sgcl::net::detail::dns_rcode::server_failure;
    Server dns(b);
    ASSERT_TRUE(dns.ok());
    for (const char* name : {"secret.test", "missing.test", "unusable.test", "required.test", "failing.test"}) {
        tls::config c = s.client(dns);
        c.server_name = name;
        c.insecure_skip_verify = true;   // the names have no certificate: the handshake is what is tested
        auto conn = tls::connect(string(s.address()), c);
        ASSERT_TRUE(conn.has_value()) << name << ": " << text(conn.error().message());
        EXPECT_FALSE(accepted(conn)) << name;
    }
    EXPECT_EQ(https_queries(b), 5);
    // a DNS server nobody answers at: refused at once
    auto gone = net::udp::bind("127.0.0.1:0");
    ASSERT_TRUE(gone.has_value());
    tls::config refused = s.client(dns);
    refused.ech_dns.servers = {gone->local_endpoint().to_string()};
    (void)gone->close();
    auto r = tls::connect(string(s.address()), refused);
    ASSERT_TRUE(r.has_value()) << text(r.error().message());
    EXPECT_FALSE(accepted(r));
    // one that drops the queries: its timeout, then without ECH
    Behaviour quiet;
    quiet.drop = 100;
    Server mute(quiet);
    ASSERT_TRUE(mute.ok());
    tls::config slow = s.client(mute);
    slow.ech_dns.timeout = 200 * millisecond;
    slow.ech_dns.attempts = 1;
    auto t0 = std::chrono::steady_clock::now();
    auto late = tls::connect(string(s.address()), slow);
    ASSERT_TRUE(late.has_value()) << text(late.error().message());
    EXPECT_FALSE(accepted(late));
    EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::seconds(3));
    // one slower than the handshake's whole time: the lookup given up at
    // half of it, the handshake made in the other half
    Behaviour sluggish;
    sluggish.delay = std::chrono::milliseconds(3000);
    Server sleepy(sluggish);
    ASSERT_TRUE(sleepy.ok());
    tls::config bounded = s.client(sleepy);
    bounded.handshake_timeout = 1 * second;
    bounded.ech_dns.timeout = 10 * second;
    t0 = std::chrono::steady_clock::now();
    auto cut = tls::connect(string(s.address()), bounded);
    ASSERT_TRUE(cut.has_value()) << text(cut.error().message());
    EXPECT_FALSE(accepted(cut));
    EXPECT_LT(std::chrono::steady_clock::now() - t0, std::chrono::milliseconds(900));
    EXPECT_GE(std::chrono::steady_clock::now() - t0, std::chrono::milliseconds(450));
}

// No lookup for an address, nor for a config that cannot offer TLS 1.3
TEST(TlsEchDns_Tests, NoLookup) {
    EchSite s;
    Behaviour b;
    Server dns(b);
    ASSERT_TRUE(dns.ok());
    tls::config address = s.client(dns);
    address.server_name = "127.0.0.1";
    address.insecure_skip_verify = true;
    auto c = tls::connect(string(s.address()), address);
    ASSERT_TRUE(c.has_value()) << text(c.error().message());
    EXPECT_FALSE(accepted(c));
    tls::config old = s.client(dns);
    old.max_version = tls::version::tls12;
    (void)tls::connect(string(s.address()), old);   // our server speaks 1.3 alone: it fails, unasked
    EXPECT_TRUE(b.queries().empty());
    EXPECT_EQ(net::tls::detail::ech_query_name("a.test", 443), string("a.test."));
    EXPECT_EQ(net::tls::detail::ech_query_name("a.test.", 443), string("a.test."));
    EXPECT_EQ(net::tls::detail::ech_query_name("a.test", 8443), string("_8443._https.a.test."));
}
