//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::dns over HTTPS (RFC 8484), against a DoH server made here of the
// module's http::server over TLS on the loopback (the zone and answers of
// tests/net/dns_server.h, the test CA of tests/net/tls_testdata) and
// against Go's net/http (tools/dns_secure_oracle.go): every lookup over an
// "https://" server by POST and by GET, id 0 and padding in every query,
// HTTP/2 chosen and one connection for the lookups, the answers that are
// no answers (another status, another type, a body past 65535 bytes, a
// body that does not read, another question), the timeout and the stop,
// the certificate checked, and the next server after a failure. Nothing
// leaves the machine.
#include "tests/types.h"
#include "tests/source_root.h"
#include "tests/net/dns_server.h"
#include "sgcl/io/exec.h"
#include "sgcl/net/http/http.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace {
    using namespace dns_test;
    using namespace std::chrono_literals;
    using sgcl::net::dns;

    std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    std::string testdata(const std::string& name) {
        std::ifstream in(source_root() / "tests/net/tls_testdata" / name);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
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

    // What the DoH server does and saw
    struct DohBehaviour {
        Behaviour b;
        std::atomic<int> status{0};          // answered with this status in place of 200
        std::atomic<int> wrong_type{0};      // answered as text/plain
        std::atomic<int> oversize{0};        // a body of 70000 bytes
        std::atomic<int> garbage{0};         // a body of junk
        std::atomic<int> other_question{0};  // the answer to another name
        std::atomic<int> delay_ms{0};
        std::atomic<int> posts{0};
        std::atomic<int> gets{0};
        std::atomic<int> h2{0};
        std::atomic<int> nonzero_id{0};
        std::atomic<int> padded{0};
        std::atomic<int> unpadded{0};
        std::atomic<int> in_flight{0};   // handlers running: the server's object outlives them

        DohBehaviour() {
            b.zone = zone();
        }
    };

    bool padded(const uint8_t* q, size_t n) {
        nd::DnsReader r(q, n);
        nd::DnsHeader h;
        nd::DnsName name;
        uint16_t t = 0, k = 0;
        if (!r.header(h) || !r.question(name, t, k)) {
            return false;
        }
        nd::DnsReader::Record rec;
        for (size_t i = 0; i < size_t(h.answers) + h.authorities + h.additionals; ++i) {
            if (!r.record(rec)) {
                return false;
            }
            if (rec.type == type::opt && rec.length >= 4 && r.u16_at(rec.rdata) == nd::DnsOptionPadding) {
                return n % 128 == 0;
            }
        }
        return false;
    }

    struct InFlight {
        DohBehaviour* d;

        explicit InFlight(DohBehaviour* b)
        : d(b) {
            ++d->in_flight;
        }

        ~InFlight() {
            --d->in_flight;
        }
    };

    async::task<> answer_doh(DohBehaviour* d, net::http::request req, net::http::response_writer w, sgcl::vector<byte> q) {
        if (req.proto() == "HTTP/2.0") {
            ++d->h2;
        }
        const uint8_t* qb = reinterpret_cast<const uint8_t*>(q.data());
        Query query;
        if (!read_query(qb, q.size(), query)) {
            w.error(net::http::status::bad_request);
            co_return;
        }
        (padded(qb, q.size()) ? d->padded : d->unpadded)++;
        if (query.id != 0) {
            ++d->nonzero_id;
        }
        note(d->b, query);
        if (int ms = d->delay_ms.load()) {
            co_await async::sleep(std::chrono::milliseconds(ms));
        }
        if (int s = d->status.exchange(0)) {
            w.error(s);
            co_return;
        }
        sgcl::vector<byte> out(80000);
        uint8_t* o = reinterpret_cast<uint8_t*>(out.data());
        size_t n = 0;
        if (d->b.take(d->oversize)) {
            n = answer(d->b, query, o, out.size(), true);
            n = 70000;   // the answer and zeros after it
        } else if (d->b.take(d->garbage)) {
            n = 7;
            for (size_t i = 0; i < n; ++i) {
                o[i] = 0xA5;
            }
        } else if (d->b.take(d->other_question)) {
            nd::DnsName elsewhere = wire("elsewhere.example.test.");
            n = answer(d->b, query, o, out.size(), true, &elsewhere);
        } else {
            n = answer(d->b, query, o, out.size(), true);
        }
        w.set_header("Content-Type", d->b.take(d->wrong_type) ? "text/plain" : "application/dns-message");
        w.set_header("Cache-Control", "max-age=300");
        w.write(static_cast<const sgcl::vector<byte>&>(out).as_slice().first(n));
    }

    net::http::server doh_server(DohBehaviour* d) {
        net::http::server s;
        s.route("POST /dns-query", [d](net::http::request req, net::http::response_writer w) -> async::task<> {
            InFlight counted(d);
            ++d->posts;
            if (req.header("Content-Type") != "application/dns-message") {
                w.error(415);
                co_return;
            }
            auto body = co_await req.async_bytes();
            if (!body) {
                w.error(400);
                co_return;
            }
            co_await answer_doh(d, req, w, std::move(*body));
        });
        s.route("GET /dns-query", [d](net::http::request req, net::http::response_writer w) -> async::task<> {
            InFlight counted(d);
            ++d->gets;
            auto q = encoding::base64::raw_url.decode(req.url().query_params().get("dns"));
            if (!q) {
                w.error(400);
                co_return;
            }
            co_await answer_doh(d, req, w, std::move(*q));
        });
        return s;
    }

    net::tls::config server_tls() {
        net::tls::config c;
        c.identities = {net::tls::identity(sgcl::string(testdata("ecdsa.pem")), sgcl::string(testdata("ecdsa.key")))};
        c.alpn = {sgcl::string("h2"), sgcl::string("http/1.1")};
        return c;
    }

    struct DohServer {
        DohBehaviour d;
        net::http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        uint16_t port = 0;

        explicit DohServer(net::tls::config c = server_tls()) {
            server = doh_server(&d);
            auto l = net::tls::listen("127.0.0.1:0", c);
            EXPECT_TRUE(l.has_value());
            listener = *l;
            port = listener.local_endpoint().port();
            serving = async::spawn(server.async_serve(listener));
        }

        ~DohServer() {
            server.close();
            (void)serving.wait();
            for (int i = 0; i < 500 && d.in_flight.load() > 0; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));   // a handler still asleep (a delay) reads d
            }
        }

        std::string url() const {
            return "https://127.0.0.1:" + std::to_string(port) + "/dns-query";
        }

        dns::options options(std::chrono::milliseconds timeout = 2000ms, int attempts = 1) const {
            dns::options o;
            o.servers = {sgcl::string(url())};
            o.timeout = timeout;
            o.attempts = attempts;
            o.roots_pem = sgcl::string(testdata("ca.pem"));
            return o;
        }
    };
}

TEST(NetDnsHttps_Tests, EveryLookupOverHttps) {
    DohServer s;
    for (bool get : {false, true}) {
        SCOPED_TRACE(get ? "GET" : "POST");
        auto o = s.options();
        o.https_get = get;
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
        auto cname = dns::lookup_cname("www.example.test", o);
        ASSERT_TRUE(cname.has_value());
        EXPECT_EQ(str(*cname), "host.example.test.");
        auto ips = dns::lookup("host.example.test", o);
        ASSERT_TRUE(ips.has_value()) << str(ips.error().message());
        ASSERT_EQ(ips->size(), 2u);
        EXPECT_EQ(str((*ips)[1].to_string()), "2001:db8::1");
        auto names = dns::reverse_lookup(net::ip_address("192.0.2.1"), o);
        ASSERT_TRUE(names.has_value());
        EXPECT_EQ(str((*names)[0]), "host.example.test.");
        auto missing = dns::lookup_mx("nothing.example.test", o);
        ASSERT_FALSE(missing.has_value());
        EXPECT_EQ(missing.error().code(), net::errc::host_not_found);
        auto nodata = dns::async_lookup_txt("host.example.test", o).wait();
        ASSERT_FALSE(nodata.has_value());
        EXPECT_EQ(nodata.error().code(), net::errc::no_data);
    }
    EXPECT_GT(s.d.posts.load(), 5);
    EXPECT_GT(s.d.gets.load(), 5);
    EXPECT_EQ(s.d.nonzero_id.load(), 0);
    EXPECT_EQ(s.d.unpadded.load(), 0);
    EXPECT_EQ(s.d.h2.load(), s.d.posts.load() + s.d.gets.load());   // HTTP/2 chosen by ALPN
}

TEST(NetDnsHttps_Tests, Http11WhereTheServerHasNoHttp2) {
    net::tls::config c = server_tls();
    c.alpn = {sgcl::string("http/1.1")};
    DohServer s(c);
    auto o = s.options();
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(dns::lookup_mx("example.test", o).has_value());
    }
    EXPECT_EQ(s.d.h2.load(), 0);
    EXPECT_EQ(s.d.posts.load(), 3);
}

TEST(NetDnsHttps_Tests, LookupsAtOnce) {
    DohServer s;
    auto o = s.options(5000ms);
    sgcl::vector<async::task<expected<sgcl::vector<dns::mx>, io::error>>> all;
    for (int i = 0; i < 24; ++i) {
        all.push_back(async::spawn(dns::async_lookup_mx("example.test", o)));
    }
    for (auto& t : all) {
        auto r = t.wait();
        ASSERT_TRUE(r.has_value()) << str(r.error().message());
    }
    EXPECT_EQ(s.d.posts.load(), 24);
}

TEST(NetDnsHttps_Tests, AnswersThatAreNoAnswers) {
    DohServer s;
    auto o = s.options(1000ms);
    auto fails = [&](std::atomic<int>& fault, int value = 1) {
        fault = value;
        auto r = dns::lookup_mx("example.test", o);
        EXPECT_FALSE(r.has_value());
        return r.has_value() ? error_code() : r.error().code();
    };
    EXPECT_EQ(fails(s.d.status, 500), net::errc::server_misbehaving);
    EXPECT_EQ(fails(s.d.status, 404), net::errc::server_misbehaving);
    EXPECT_EQ(fails(s.d.wrong_type), net::errc::server_misbehaving);
    EXPECT_EQ(fails(s.d.oversize), net::errc::server_misbehaving);
    EXPECT_EQ(fails(s.d.garbage), net::errc::server_misbehaving);
    EXPECT_EQ(fails(s.d.other_question), net::errc::server_misbehaving);
    // and after each, the server answers again
    EXPECT_TRUE(dns::lookup_mx("example.test", o).has_value());
    // a failure moves to the next attempt
    s.d.status = 503;
    auto two = s.options(1000ms, 2);
    EXPECT_TRUE(dns::lookup_mx("example.test", two).has_value());
}

TEST(NetDnsHttps_Tests, TimeoutStopAndCertificate) {
    DohServer s;
    s.d.delay_ms = 800;
    auto o = s.options(200ms);
    auto t0 = std::chrono::steady_clock::now();
    auto r = dns::lookup_mx("example.test", o);
    ASSERT_FALSE(r.has_value());
    EXPECT_TRUE(r.error().is_timeout());
    EXPECT_LT(std::chrono::steady_clock::now() - t0, 700ms);
    async::stop_source stop;
    stop.stop_after(100ms);
    auto st = dns::async_lookup_mx("example.test", s.options(5000ms), stop.token()).wait();
    ASSERT_FALSE(st.has_value());
    EXPECT_EQ(st.error().code(), std::errc::operation_canceled);
    s.d.delay_ms = 0;
    // the test CA not trusted
    auto untrusted = s.options();
    untrusted.roots_pem = sgcl::string();
    auto u = dns::lookup_mx("example.test", untrusted);
    ASSERT_FALSE(u.has_value());
    EXPECT_EQ(u.error().code().category(), net::tls::category());
    // another name checked
    auto named = s.options();
    named.servers[0].name = "other.example";
    EXPECT_FALSE(dns::lookup_mx("example.test", named).has_value());
    named.servers[0].name = "localhost";
    EXPECT_TRUE(dns::lookup_mx("example.test", named).has_value());
    // the first server fails, the second answers
    auto two = s.options();
    two.servers = {sgcl::string("https://127.0.0.1:1/dns-query"), sgcl::string(s.url())};
    EXPECT_TRUE(dns::lookup_mx("example.test", two).has_value());
}

TEST(NetDnsHttps_Tests, TheTransportMissing) {
    auto& hook = net::detail::dns_hooks().https;
    auto installed = hook.exchange(nullptr);
    dns::options o;
    o.servers = {"https://127.0.0.1:1/dns-query"};
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

    struct GoServers {
        std::string dir;
        std::string log;
        std::optional<io::command> cmd;
        bool started = false;
        uint16_t doh = 0;

        GoServers() {
            dir = (std::filesystem::temp_directory_path() / ("sgcl_doh_" + std::to_string(::getpid()))).string();
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
            for (int i = 0; started && i < 1000; ++i) {
                std::string s = slurp_file(log);
                auto at = s.find("DOH ");
                if (at != std::string::npos && s.find('\n', at) != std::string::npos) {
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

TEST(NetDnsHttps_Tests, GoServesTheSameRecords) {
    if (go_path().empty()) {
        GTEST_SKIP() << "no go (Go's net/http is the oracle)";
    }
    GoServers go;
    ASSERT_TRUE(go.started && go.doh) << go.output();
    for (bool get : {false, true}) {
        SCOPED_TRACE(get ? "GET" : "POST");
        dns::options o;
        o.servers = {sgcl::string("https://localhost:" + std::to_string(go.doh) + "/dns-query")};
        o.roots_pem = sgcl::string(testdata("ca.pem"));
        o.timeout = 3s;
        o.attempts = 1;
        o.https_get = get;
        auto ips = dns::lookup("host.example.test", o);
        ASSERT_TRUE(ips.has_value()) << str(ips.error().message());
        ASSERT_EQ(ips->size(), 2u);
        EXPECT_EQ(str((*ips)[0].to_string()), "192.0.2.1");
        EXPECT_EQ(str((*ips)[1].to_string()), "2001:db8::1");
        auto mx = dns::lookup_mx("example.test", o);
        ASSERT_TRUE(mx.has_value());
        ASSERT_EQ(mx->size(), 2u);
        auto txt = dns::lookup_txt("example.test", o);
        ASSERT_TRUE(txt.has_value());
        EXPECT_EQ(str((*txt)[0]), "v=spf1 -all");
        auto none = dns::lookup_mx("nothing.example.test", o);
        ASSERT_FALSE(none.has_value());
        EXPECT_EQ(none.error().code(), net::errc::host_not_found);
    }
    std::string out = go.output();
    EXPECT_NE(out.find("DOH HTTP/2.0 POST padded host.example.test. 1"), std::string::npos) << out;
    EXPECT_NE(out.find("DOH HTTP/2.0 GET padded example.test. 15"), std::string::npos) << out;
    EXPECT_EQ(out.find("unpadded"), std::string::npos) << out;
}
