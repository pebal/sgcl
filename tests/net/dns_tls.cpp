//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::dns over TLS (RFC 7858, RFC 8310), against the server of
// tests/net/dns_tls_server.h on the loopback and against Go's crypto/tls
// (tools/dns_secure_oracle.go): every lookup over a "tls://" server, the
// queries padded to 128 bytes (RFC 8467), one connection kept for the
// lookups that follow and shared by those in flight at once, answers out
// of their order, the strict profile (the test CA trusted or not, the
// name checked, an address checked as one), SPKI pins (RFC 7858 §4.2),
// the opportunistic profile and its clear text, the faults (an answer of
// another id, a message too short, a connection closed under the queries,
// no answer), the stop, and the transport missing. Nothing leaves the
// machine.
#include "tests/types.h"
#include "tests/net/dns_tls_server.h"

#include "sgcl/io/exec.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
    using namespace dns_test;
    using namespace std::chrono_literals;
    using sgcl::net::dns;

    std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    std::vector<Rr> zone() {
        return {
            rr_mx("example.test.", 10, "mx1.example.test."),
            rr_mx("example.test.", 20, "mx2.example.test."),
            rr_txt("example.test.", {"v=spf1 -all"}),
            rr_named("example.test.", type::ns, "ns1.example.test."),
            rr_srv("_sip._tcp.example.test.", 10, 0, 5060, "sip.example.test."),
            rr_named("www.example.test.", type::cname, "host.example.test."),
            rr_a("host.example.test.", 192, 0, 2, 1),
            rr_aaaa("host.example.test."),
            rr_named("1.2.0.192.in-addr.arpa.", type::ptr, "host.example.test."),
        };
    }

    struct Fixture {
        Behaviour b;

        Fixture() {
            b.zone = zone();
        }
    };
}

TEST(NetDnsTls_Tests, EveryLookupOverTls) {
    Fixture f;
    DotServer s(f.b);
    ASSERT_TRUE(s.ok());
    auto o = s.options();
    auto mx = dns::lookup_mx("example.test", o);
    ASSERT_TRUE(mx.has_value()) << str(mx.error().message());
    ASSERT_EQ(mx->size(), 2u);
    EXPECT_EQ(str((*mx)[0].host), "mx1.example.test.");
    auto txt = dns::lookup_txt("example.test", o);
    ASSERT_TRUE(txt.has_value());
    EXPECT_EQ(str((*txt)[0]), "v=spf1 -all");
    auto srv = dns::lookup_srv("sip", "tcp", "example.test", o);
    ASSERT_TRUE(srv.has_value());
    EXPECT_EQ((*srv)[0].port, 5060);
    auto ns = dns::lookup_ns("example.test", o);
    ASSERT_TRUE(ns.has_value());
    EXPECT_EQ(str((*ns)[0]), "ns1.example.test.");
    auto cname = dns::lookup_cname("www.example.test", o);
    ASSERT_TRUE(cname.has_value());
    EXPECT_EQ(str(*cname), "host.example.test.");
    auto ips = dns::lookup("host.example.test", o);
    ASSERT_TRUE(ips.has_value()) << str(ips.error().message());
    ASSERT_EQ(ips->size(), 2u);
    EXPECT_EQ(str((*ips)[0].to_string()), "192.0.2.1");
    EXPECT_EQ(str((*ips)[1].to_string()), "2001:db8::1");
    auto names = dns::reverse_lookup(net::ip_address("192.0.2.1"), o);
    ASSERT_TRUE(names.has_value()) << str(names.error().message());
    EXPECT_EQ(str((*names)[0]), "host.example.test.");
    auto missing = dns::lookup_mx("nothing.example.test", o);
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code(), net::errc::host_not_found);
    auto nodata = dns::lookup_txt("host.example.test", o);
    ASSERT_FALSE(nodata.has_value());
    EXPECT_EQ(nodata.error().code(), net::errc::no_data);
    // the task forms
    auto t = dns::async_lookup_mx("example.test", o).wait();
    ASSERT_TRUE(t.has_value());
    auto a = dns::async_lookup("host.example.test", o).wait();
    ASSERT_TRUE(a.has_value());
    // every query padded, all on one connection
    EXPECT_GT(s.d.padded.load(), 10);
    EXPECT_EQ(s.d.unpadded.load(), 0);
    EXPECT_EQ(s.d.connections.load(), 1);
    EXPECT_EQ(f.b.udp_queries.load() + f.b.tcp_queries.load(), 0);
}

TEST(NetDnsTls_Tests, QueriesInFlightShareOneConnection) {
    Fixture f;
    DotServer s(f.b);
    ASSERT_TRUE(s.ok());
    auto o = s.options(3000ms);
    ASSERT_TRUE(dns::lookup_mx("example.test", o).has_value());   // the connection made
    s.d.hold = 3;   // the next three answers held back: those after them come first
    sgcl::vector<async::task<expected<sgcl::vector<dns::mx>, io::error>>> mxs;
    sgcl::vector<async::task<expected<sgcl::vector<sgcl::string>, io::error>>> txts;
    for (int i = 0; i < 16; ++i) {
        mxs.push_back(async::spawn(dns::async_lookup_mx("example.test", o)));
        txts.push_back(async::spawn(dns::async_lookup_txt("example.test", o)));
    }
    for (auto& m : mxs) {
        auto r = m.wait();
        ASSERT_TRUE(r.has_value()) << str(r.error().message());
        EXPECT_EQ(str((*r)[0].host), "mx1.example.test.");
    }
    for (auto& x : txts) {
        auto r = x.wait();
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(str((*r)[0]), "v=spf1 -all");
    }
    EXPECT_EQ(s.d.connections.load(), 1);
    EXPECT_GT(s.d.in_flight_max.load(), 1);
}

TEST(NetDnsTls_Tests, StrictChecksTheCertificate) {
    Fixture f;
    DotServer s(f.b);
    ASSERT_TRUE(s.ok());
    // the test CA not trusted: the system's roots do not have it
    auto o = s.options();
    o.roots_pem = sgcl::string();
    auto r = dns::lookup_mx("example.test", o);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code().category(), net::tls::category());
    // another name than the certificate's
    auto named = s.options();
    named.servers[0].name = "other.example";
    auto n = dns::lookup_mx("example.test", named);
    ASSERT_FALSE(n.has_value());
    EXPECT_EQ(n.error().code().category(), net::tls::category());
    // the name of the certificate: checked as given, with the address dialed
    auto right = s.options();
    right.servers[0].name = "localhost";
    EXPECT_TRUE(dns::lookup_mx("example.test", right).has_value());
    // by name: "tls://localhost:port" dials the name and checks it
    auto by_name = s.options();
    by_name.servers = {sgcl::string("tls://localhost:" + std::to_string(s.port))};
    auto bn = dns::lookup_mx("example.test", by_name);
    EXPECT_TRUE(bn.has_value()) << str(bn.error().message());
    // a strict failure moves to the next server
    auto two = s.options();
    two.servers = {sgcl::string("tls://127.0.0.1:" + std::to_string(s.port)), sgcl::string(s.address())};
    two.servers[0].name = "other.example";
    EXPECT_TRUE(dns::lookup_mx("example.test", two).has_value());
}

TEST(NetDnsTls_Tests, Pins) {
    Fixture f;
    DotServer s(f.b);
    ASSERT_TRUE(s.ok());
    // the leaf's key pinned: no roots needed
    auto o = s.options();
    o.roots_pem = sgcl::string();
    o.servers[0].pins = {sgcl::string(leaf_pin())};
    auto r = dns::lookup_mx("example.test", o);
    ASSERT_TRUE(r.has_value()) << str(r.error().message());
    // any of the pins matches
    o.servers[0].pins = {sgcl::string(std::string(43, 'A') + "="), sgcl::string(leaf_pin())};
    EXPECT_TRUE(dns::lookup_mx("example.test", o).has_value());
    // none matches: bad certificate
    auto wrong = s.options();
    wrong.servers[0].pins = {sgcl::string(std::string(43, 'A') + "=")};
    auto w = dns::lookup_mx("example.test", wrong);
    ASSERT_FALSE(w.has_value());
    EXPECT_EQ(w.error().code(), net::tls::alert::bad_certificate);
    // opportunistic: the pin is no condition
    wrong.opportunistic = true;
    EXPECT_TRUE(dns::lookup_mx("example.test", wrong).has_value());
    // a pin that is not base64 of 32 bytes
    for (const char* bad : {"", "not base64!", "AAAA", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"}) {
        SCOPED_TRACE(bad);
        auto p = s.options();
        p.servers[0].pins = {sgcl::string(bad)};
        auto e = dns::lookup_mx("example.test", p);
        ASSERT_FALSE(e.has_value());
        EXPECT_EQ(e.error().code(), std::errc::invalid_argument);
    }
}

TEST(NetDnsTls_Tests, OpportunisticTakesWhatItGets) {
    Fixture f;
    DotServer s(f.b);
    ASSERT_TRUE(s.ok());
    auto o = s.options();
    o.roots_pem = sgcl::string();
    o.opportunistic = true;
    EXPECT_TRUE(dns::lookup_mx("example.test", o).has_value());
    // no TLS at the port: clear text at the address (the port of the test's
    // UDP server in place of 53)
    Fixture g;
    Server plain(g.b);
    ASSERT_TRUE(plain.ok());
    uint16_t closed = 0;
    {
        auto l = net::tcp::listen("127.0.0.1:0");
        ASSERT_TRUE(l.has_value());
        closed = l->local_endpoint().port();
        (void)l->close();
    }
    uint16_t before = net::detail::dot_clear_port().exchange(plain.port);
    auto to = s.options();
    to.servers = {sgcl::string("tls://127.0.0.1:" + std::to_string(closed))};
    to.opportunistic = true;
    auto r = dns::lookup_mx("example.test", to);
    EXPECT_TRUE(r.has_value()) << str(r.error().message());
    EXPECT_EQ(g.b.udp_queries.load(), 1);
    // strict: no clear text, the TLS dial's error
    to.opportunistic = false;
    auto strict = dns::lookup_mx("example.test", to);
    ASSERT_FALSE(strict.has_value());
    EXPECT_EQ(strict.error().code(), std::errc::connection_refused);
    EXPECT_EQ(g.b.udp_queries.load(), 1);
    net::detail::dot_clear_port() = before;
}

TEST(NetDnsTls_Tests, Faults) {
    Fixture f;
    DotServer s(f.b);
    ASSERT_TRUE(s.ok());
    auto o = s.options(400ms);
    // an answer of another id: nobody's, the query waits to its deadline
    s.d.wrong_id = 1;
    auto t0 = std::chrono::steady_clock::now();
    auto r = dns::lookup_mx("example.test", o);
    ASSERT_FALSE(r.has_value());
    EXPECT_TRUE(r.error().is_timeout());
    EXPECT_GE(std::chrono::steady_clock::now() - t0, 350ms);
    // the connection goes on
    EXPECT_TRUE(dns::lookup_mx("example.test", o).has_value());
    EXPECT_EQ(s.d.connections.load(), 1);
    // a message too short: the connection given up, the query asked again
    // on a new one
    s.d.runt = 1;
    auto again = dns::lookup_mx("example.test", o);
    EXPECT_TRUE(again.has_value()) << str(again.error().message());
    EXPECT_EQ(s.d.connections.load(), 2);
    // the server closes after every answer: a new connection for each
    s.d.close_after = 1;
    for (int i = 0; i < 3; ++i) {
        auto x = dns::lookup_txt("example.test", o);
        EXPECT_TRUE(x.has_value()) << str(x.error().message());
    }
    EXPECT_GE(s.d.connections.load(), 4);
    s.d.close_after = -1;
    // the server closes every connection between two lookups
    ASSERT_TRUE(dns::lookup_mx("example.test", o).has_value());
    s.drop_connections();
    std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(dns::lookup_mx("example.test", o).has_value());
    // no answer at all: the timeout, then the next attempt
    s.d.silent = 1;
    auto o2 = s.options(300ms, 2);
    EXPECT_TRUE(dns::lookup_mx("example.test", o2).has_value());
    s.d.silent = 2;
    auto none = dns::lookup_mx("example.test", o2);
    ASSERT_FALSE(none.has_value());
    EXPECT_TRUE(none.error().is_timeout());
}

TEST(NetDnsTls_Tests, TheStopEndsTheWait) {
    Fixture f;
    DotServer s(f.b);
    ASSERT_TRUE(s.ok());
    auto o = s.options(5000ms);
    ASSERT_TRUE(dns::lookup_mx("example.test", o).has_value());
    s.d.silent = 1;
    async::stop_source stop;
    stop.stop_after(100ms);
    auto t0 = std::chrono::steady_clock::now();
    auto r = dns::async_lookup_mx("example.test", o, stop.token()).wait();
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code(), std::errc::operation_canceled);
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 2000ms);
    // stopped before it starts
    async::stop_source early;
    early.request_stop();
    auto e = dns::async_lookup_mx("example.test", o, early.token()).wait();
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code(), std::errc::operation_canceled);
    // the connection still answers
    EXPECT_TRUE(dns::lookup_mx("example.test", o).has_value());
}

TEST(NetDnsTls_Tests, TheTransportMissing) {
    // a program without tls.h has no DoT: the server's failure, the
    // protocol not supported
    auto& hook = net::detail::dns_hooks().tls;
    auto installed = hook.exchange(nullptr);
    dns::options o;
    o.servers = {"tls://127.0.0.1:1"};
    o.timeout = 200ms;
    o.attempts = 1;
    auto r = dns::lookup_mx("example.test", o);
    hook = installed;
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code(), std::errc::protocol_not_supported);
}

namespace {
    std::string go_path() {
        for (const char* p : {"/opt/homebrew/bin/go", "/usr/local/go/bin/go", "/usr/local/bin/go"}) {
            if (::access(p, X_OK) == 0) {
                return p;
            }
        }
        return "";
    }

    std::string slurp_file(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    // tools/dns_secure_oracle.go built once, run with the test leaf
    struct GoServers {
        std::string dir;
        std::string log;
        std::optional<io::command> cmd;
        bool started = false;
        uint16_t dot = 0;
        uint16_t doh = 0;

        GoServers() {
            dir = (std::filesystem::temp_directory_path() / ("sgcl_dns_secure_" + std::to_string(::getpid()))).string();
            std::filesystem::create_directories(dir);
            std::string bin = dir + "/dns_secure_oracle";
            if (!std::filesystem::exists(bin)) {
                io::command b(sgcl::string(go_path()), sgcl::string("build"), sgcl::string("-o"), sgcl::string(bin),
                              sgcl::string((source_root() / "tools/dns_secure_oracle.go").string()));
                if (!b.combined_output()) {
                    return;
                }
            }
            log = dir + "/out.log";
            auto file = io::create(sgcl::string(log));
            if (!file) {
                return;
            }
            sgcl::vector<sgcl::string> args;
            args.push_back(sgcl::string("-cert"));
            args.push_back(sgcl::string((source_root() / "tests/net/tls_testdata/ecdsa.pem").string()));
            args.push_back(sgcl::string("-key"));
            args.push_back(sgcl::string((source_root() / "tests/net/tls_testdata/ecdsa.key").string()));
            cmd.emplace(sgcl::string(bin), std::move(args));
            cmd->out = *file;
            cmd->err = *file;
            started = (bool)cmd->start();
            (void)file->close();
            for (int i = 0; started && i < 1000 && !doh; ++i) {
                std::string s = slurp_file(log);
                auto at = s.find("DOH ");
                if (at != std::string::npos && s.find('\n', at) != std::string::npos) {
                    dot = uint16_t(std::stoi(s.substr(s.find("DOT ") + 4)));
                    doh = uint16_t(std::stoi(s.substr(at + 4)));
                    break;
                }
                std::this_thread::sleep_for(10ms);
            }
        }

        std::string output() const {
            return slurp_file(log);
        }

        // the server killed and its directory removed; a test process that
        // dies first leaves the server to end by itself (it watches its parent)
        ~GoServers() {
            if (started) {
                (void)cmd->process.kill();
                (void)cmd->wait();
            }
            std::error_code e;
            std::filesystem::remove_all(dir, e);
        }
    };
}

TEST(NetDnsTls_Tests, GoServesTheSameRecords) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's crypto/tls is the oracle)";
    }
    GoServers go;
    ASSERT_TRUE(go.started && go.dot) << go.output();
    dns::options o;
    o.servers = {sgcl::string("tls://127.0.0.1:" + std::to_string(go.dot))};
    o.roots_pem = sgcl::string(ca_pem());
    o.timeout = 3s;
    o.attempts = 1;
    auto ips = dns::lookup("host.example.test", o);
    ASSERT_TRUE(ips.has_value()) << str(ips.error().message());
    ASSERT_EQ(ips->size(), 2u);
    EXPECT_EQ(str((*ips)[0].to_string()), "192.0.2.1");
    EXPECT_EQ(str((*ips)[1].to_string()), "2001:db8::1");
    auto mx = dns::lookup_mx("example.test", o);
    ASSERT_TRUE(mx.has_value());
    ASSERT_EQ(mx->size(), 2u);
    EXPECT_EQ(str((*mx)[1].host), "mx2.example.test.");
    auto txt = dns::lookup_txt("example.test", o);
    ASSERT_TRUE(txt.has_value());
    EXPECT_EQ(str((*txt)[0]), "v=spf1 -all");
    auto none = dns::lookup_mx("nothing.example.test", o);
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code(), net::errc::host_not_found);
    auto nodata = dns::lookup_txt("host.example.test", o);
    ASSERT_FALSE(nodata.has_value());
    EXPECT_EQ(nodata.error().code(), net::errc::no_data);
    std::string out = go.output();
    EXPECT_NE(out.find("DOT padded host.example.test. 1"), std::string::npos) << out;
    EXPECT_EQ(out.find("DOT unpadded"), std::string::npos) << out;
}

TEST(NetDnsTls_Tests, QueriesArePaddedToBlocksOf128) {
    // RFC 8467 §4.1: the query's size a multiple of 128, the Padding option
    // of RFC 7830 its zeros, whatever the name's length
    for (size_t len : {size_t(1), size_t(10), size_t(60), size_t(63)}) {
        for (int labels = 1; labels <= 3; ++labels) {
            std::string name;
            for (int l = 0; l < labels; ++l) {
                name += std::string(len, char('a' + l)) + ".";
            }
            SCOPED_TRACE(name);
            uint8_t q[512];
            size_t n = nd::dns_write_query(q, sizeof q, 0x1234, wire(name), type::a, true, nd::DnsQueryPadBlock);
            ASSERT_GT(n, 0u);
            EXPECT_EQ(n % 128, 0u);
            EXPECT_TRUE(padded_query(q, n));
            nd::DnsReader r(q, n);
            nd::DnsHeader h;
            ASSERT_TRUE(r.header(h));
            nd::DnsName qn;
            uint16_t t = 0, k = 0;
            ASSERT_TRUE(r.question(qn, t, k));
            nd::DnsReader::Record rec;
            ASSERT_TRUE(r.record(rec));
            EXPECT_EQ(rec.type, type::opt);
            EXPECT_EQ(r.position(), n);   // the option fills the message to its end
            for (size_t i = rec.rdata + 4; i < n; ++i) {
                EXPECT_EQ(q[i], 0);
            }
        }
    }
    // the unpadded query of plain UDP and TCP is as before
    uint8_t q[512];
    size_t plain = nd::dns_write_query(q, sizeof q, 1, wire("example.com."), type::mx, true);
    EXPECT_EQ(plain, 12u + 13u + 4u + 11u);
    // a buffer too small for the padding: nothing written
    EXPECT_EQ(nd::dns_write_query(q, 100, 1, wire("example.com."), type::mx, true, nd::DnsQueryPadBlock), 0u);
}
