//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::socks5::server: the module's own client through it (CONNECT, names
// and addresses, credentials, refusals), BIND and UDP ASSOCIATE by hand from
// RFC 1928, and the clients of others: curl's --socks5 and
// --socks5-hostname, Go's net/http through tests/net/go_proxy (skipped
// where there is no curl or go).
#include "tests/types.h"
#include "tests/source_root.h"
#include "tests/net/socks5_server.h"
#include "sgcl/net/http.h"
#include "sgcl/net/net.h"
#include "sgcl/net/socks5.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace sgcl;
using sgcl_test::EchoServer;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.view());
    }

    std::string run(const std::string& cmd) {
        std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    }

    bool have(const char* tool) {
        return std::system((std::string("command -v ") + tool + " > /dev/null 2>&1").c_str()) == 0;
    }

    std::string round_trip(const net::connection& c, const std::string& line) {
        if (!c.write(sgcl::string(line))) {
            return "write failed";
        }
        vector<byte> buf(line.size());
        auto r = c.read_full(buf.as_slice());
        if (!r) {
            return "read: " + text(r.error().message());
        }
        return std::string(reinterpret_cast<const char*>(buf.data()), *r);
    }

    // A proxy of the module's on the loopback for as long as the object lives
    struct Proxy {
        net::socks5::server srv;
        net::listener listener;
        async::task<expected<void, io::error>> serving;

        explicit Proxy(net::socks5::server s = {}) : srv(s) {
            listener = net::tcp::listen("127.0.0.1:0").value();
            serving = async::spawn(srv.async_serve(listener));
        }

        ~Proxy() {
            srv.close();
            (void)serving.wait();
        }

        sgcl::string address() const {
            return listener.local_endpoint().to_string();
        }

        uint16_t port() const {
            return listener.local_endpoint().port();
        }
    };

    // An HTTP server of one route for the clients of others
    struct Site {
        net::http::server srv;
        net::listener listener;
        async::task<expected<void, io::error>> serving;

        Site() {
            srv.route("GET /hi", [](net::http::request, net::http::response_writer w) { w.write(sgcl::string("hi from the target\n")); });
            listener = net::tcp::listen("127.0.0.1:0").value();
            serving = async::spawn(srv.async_serve(listener));
        }

        ~Site() {
            srv.close();
            (void)serving.wait();
        }

        std::string port() const {
            return std::to_string(listener.local_endpoint().port());
        }
    };

    // The raw handshake of RFC 1928 to the proxy: no authentication, then a request; the reply's head
    std::string raw_request(net::connection& c, uint8_t cmd, const std::string& addr_part) {
        std::string hello("\x05\x01\x00", 3);
        if (!c.write(sgcl::string(hello))) {
            return "";
        }
        vector<byte> two(2);
        if (!c.read_full(two.as_slice())) {
            return "";
        }
        std::string req{char(5), char(cmd), char(0)};
        req += addr_part;
        if (!c.write(sgcl::string(req))) {
            return "";
        }
        vector<byte> head(10);   // VER REP RSV ATYP=1 ADDR(4) PORT(2)
        auto r = c.read_full(head.as_slice());
        if (!r) {
            return "";
        }
        return std::string(reinterpret_cast<const char*>(head.data()), head.size());
    }

    std::string ipv4_part(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port) {
        return std::string{char(1), char(a), char(b), char(c), char(d), char(port >> 8), char(port & 0xFF)};
    }

    net::endpoint endpoint_of(const std::string& reply) {
        auto b = reinterpret_cast<const uint8_t*>(reply.data());
        return net::endpoint(net::ip_address::v4(b[4], b[5], b[6], b[7]), uint16_t(b[8] << 8 | b[9]));
    }
}

TEST(Socks5Server, ConnectThroughIt) {
    auto echo = EchoServer::start();
    Proxy p;
    auto c = net::socks5::connect(p.address(), echo->listener.local_endpoint().to_string());
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(round_trip(*c, "ping through the proxy"), "ping through the proxy");
    // a name the proxy resolves
    auto byname = net::socks5::connect(p.address(), sgcl::string("localhost:" + std::to_string(echo->listener.local_endpoint().port())));
    ASSERT_TRUE(byname) << text(byname.error().message());
    EXPECT_EQ(round_trip(*byname, "by name"), "by name");
    // the end of one side reaches the other: close_write through the proxy, the echo closes, its end comes back
    ASSERT_TRUE(c->write(sgcl::string("last")));
    ASSERT_TRUE(c->close_write());
    EXPECT_EQ(text(*c->read_all_text()), "last");
    // many at once
    vector<async::task<bool>> all;
    for (int i = 0; i < 32; ++i) {
        all.push_back(async::spawn([](sgcl::string proxy, sgcl::string target, int i) -> async::task<bool> {
            auto c = co_await net::socks5::async_connect(proxy, target);
            if (!c) {
                co_return false;
            }
            std::string line = "line " + std::to_string(i);
            if (!co_await c->async_write(sgcl::string(line))) {
                co_return false;
            }
            vector<byte> buf(line.size());
            auto r = co_await c->async_read_full(buf.as_slice());
            (void)c->close();
            co_return r && std::string(reinterpret_cast<const char*>(buf.data()), *r) == line;
        }(p.address(), echo->listener.local_endpoint().to_string(), i)));
    }
    for (auto& t : all) {
        EXPECT_TRUE(t.wait());
    }
}

// close() and shutdown() at once after the serve is started, before its
// accept loop has its listener: the serve still ends (it hung once, found
// by a page's example); a server closed stays closed
TEST(Socks5Server, ClosedBeforeItsLoopStarts) {
    for (int i = 0; i < 200; ++i) {
        net::socks5::server proxy;
        auto l = net::tcp::listen("127.0.0.1:0").value();
        auto serving = async::spawn(proxy.async_serve(l));
        if (i % 2) {
            proxy.close();
        } else {
            proxy.shutdown();
        }
        auto r = serving.wait();
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), net::errc::server_closed);
        EXPECT_FALSE(net::tcp::connect(l.local_endpoint().to_string()));   // the listener closed too
        auto again = proxy.serve(sgcl::string("127.0.0.1:0"));
        ASSERT_FALSE(again);
        EXPECT_EQ(again.error().code(), net::errc::server_closed);
    }
}

TEST(Socks5Server, CredentialsAndRules) {
    auto echo = EchoServer::start();
    net::socks5::server s;
    s.authenticate = [](const sgcl::string& u, const sgcl::string& pw) { return u == "alice" && pw == "secret"; };
    s.allow = [](const net::endpoint&, const sgcl::string& target) { return !target.view().ends_with(":1"); };
    Proxy p(s);
    net::socks5::options good;
    good.username = "alice";
    good.password = "secret";
    auto c = net::socks5::connect(p.address(), echo->listener.local_endpoint().to_string(), good);
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(round_trip(*c, "authenticated"), "authenticated");
    net::socks5::options bad = good;
    bad.password = "wrong";
    auto refused = net::socks5::connect(p.address(), echo->listener.local_endpoint().to_string(), bad);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), net::errc::proxy_auth_required);
    auto none = net::socks5::connect(p.address(), echo->listener.local_endpoint().to_string());   // no credentials offered
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), net::errc::proxy_auth_required);
    // a rule: every target but port 1 (not allowed: REP 2)
    auto ruled = net::socks5::connect(p.address(), sgcl::string("127.0.0.1:1"), good);
    ASSERT_FALSE(ruled);
    EXPECT_EQ(ruled.error().code(), net::errc::proxy_refused);
}

TEST(Socks5Server, TargetsThatCannotBeReached) {
    Proxy p;
    auto refused = net::socks5::connect(p.address(), sgcl::string("127.0.0.1:1"));
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), std::errc::connection_refused);
    auto unknown = net::socks5::connect(p.address(), sgcl::string("no-such-host.invalid:80"));
    ASSERT_FALSE(unknown);
    EXPECT_EQ(unknown.error().code(), std::errc::host_unreachable);
    // a command it does not have, an address type of no meaning
    auto c = net::tcp::connect(p.address()).value();
    std::string reply = raw_request(c, 9, ipv4_part(127, 0, 0, 1, 80));
    ASSERT_EQ(reply.size(), 10u);
    EXPECT_EQ(uint8_t(reply[1]), 0x07);
    auto d = net::tcp::connect(p.address()).value();
    std::string atyp = raw_request(d, 1, std::string("\x09\x00\x00", 3));
    ASSERT_EQ(atyp.size(), 10u);
    EXPECT_EQ(uint8_t(atyp[1]), 0x08);
    // not SOCKS5 at all: closed
    auto e = net::tcp::connect(p.address()).value();
    ASSERT_TRUE(e.write(sgcl::string("GET / HTTP/1.1\r\n\r\n")));
    vector<byte> one(1);
    auto r = e.read(one.as_slice());
    EXPECT_TRUE(!r || *r == 0);
}

TEST(Socks5Server, Bind) {
    Proxy p;
    auto c = net::tcp::connect(p.address()).value();
    std::string first = raw_request(c, 2, ipv4_part(127, 0, 0, 1, 0));
    ASSERT_EQ(first.size(), 10u);
    ASSERT_EQ(uint8_t(first[1]), 0x00);
    net::endpoint listening = endpoint_of(first);
    EXPECT_NE(listening.port(), 0);
    // the other side connects to the address the proxy gave
    auto other = net::tcp::connect(listening.to_string()).value();
    vector<byte> second(10);
    ASSERT_TRUE(c.read_full(second.as_slice()));
    EXPECT_EQ(uint8_t(second[1]), 0x00);
    std::string sec(reinterpret_cast<const char*>(second.data()), 10);
    EXPECT_EQ(endpoint_of(sec).port(), other.local_endpoint().port());   // the peer that came
    EXPECT_TRUE(other.write(sgcl::string("from the far side")));
    vector<byte> buf(17);
    ASSERT_TRUE(c.read_full(buf.as_slice()));
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), 17), "from the far side");
    EXPECT_TRUE(c.write(sgcl::string("back")));
    vector<byte> b2(4);
    ASSERT_TRUE(other.read_full(b2.as_slice()));
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(b2.data()), 4), "back");
}

TEST(Socks5Server, UdpAssociate) {
    // a UDP echo on the loopback
    auto echo = net::udp::bind(sgcl::string("127.0.0.1:0")).value();
    auto echoing = async::spawn([](net::udp::socket u) -> async::task<> {
        vector<byte> buf(2048);
        for (;;) {
            auto d = co_await u.async_receive_from(buf.as_slice());
            if (!d) {
                co_return;
            }
            (void)co_await u.async_send_to(slice<const byte>(buf.data(), d->size), d->from);
        }
    }(echo));
    Proxy p;
    auto c = net::tcp::connect(p.address()).value();
    std::string reply = raw_request(c, 3, ipv4_part(0, 0, 0, 0, 0));
    ASSERT_EQ(reply.size(), 10u);
    ASSERT_EQ(uint8_t(reply[1]), 0x00);
    net::endpoint relay = endpoint_of(reply);
    auto u = net::udp::bind(sgcl::string("127.0.0.1:0")).value();
    uint16_t port = echo.local_endpoint().port();
    std::string dgram = std::string("\x00\x00\x00", 3) + ipv4_part(127, 0, 0, 1, port) + "datagram through the relay";
    ASSERT_TRUE(u.send_to(slice<const byte>(reinterpret_cast<const byte*>(dgram.data()), dgram.size()), relay));
    vector<byte> in(2048);
    u.set_read_deadline(sgcl::clock::now() + std::chrono::seconds(5));
    auto d = u.receive_from(in.as_slice());
    ASSERT_TRUE(d) << text(d.error().message());
    std::string got(reinterpret_cast<const char*>(in.data()), d->size);
    ASSERT_GE(got.size(), 10u);
    EXPECT_EQ(got.substr(0, 3), std::string("\x00\x00\x00", 3));
    EXPECT_EQ(uint8_t(got[3]), 1);   // the sender's address, IPv4
    EXPECT_EQ(uint16_t(uint8_t(got[8]) << 8 | uint8_t(got[9])), port);
    EXPECT_EQ(got.substr(10), "datagram through the relay");
    // a fragment is dropped (§7)
    std::string frag = std::string("\x00\x00\x01", 3) + ipv4_part(127, 0, 0, 1, port) + "x";
    ASSERT_TRUE(u.send_to(slice<const byte>(reinterpret_cast<const byte*>(frag.data()), frag.size()), relay));
    u.set_read_deadline(sgcl::clock::now() + std::chrono::milliseconds(200));
    EXPECT_FALSE(u.receive_from(in.as_slice()));
    // the association ends with its TCP connection
    (void)c.close();
    (void)echo.close();
    echoing.wait();
}

TEST(Socks5Server, CurlThroughIt) {
    if (!have("curl")) {
        GTEST_SKIP() << "no curl";
    }
    Site site;
    Proxy p;
    std::string port = std::to_string(p.port());
    EXPECT_EQ(run("curl -s --max-time 10 --socks5 127.0.0.1:" + port + " http://127.0.0.1:" + site.port() + "/hi"), "hi from the target\n");
    EXPECT_EQ(run("curl -s --max-time 10 --socks5-hostname 127.0.0.1:" + port + " http://localhost:" + site.port() + "/hi"), "hi from the target\n");
    net::socks5::server s;
    s.authenticate = [](const sgcl::string& u, const sgcl::string& pw) { return u == "carol" && pw == "pw"; };
    Proxy a(s);
    std::string ap = std::to_string(a.port());
    EXPECT_EQ(run("curl -s --max-time 10 --socks5-hostname 127.0.0.1:" + ap + " --proxy-user carol:pw http://localhost:" + site.port() + "/hi"),
              "hi from the target\n");
    EXPECT_NE(run("curl -s --max-time 10 --socks5-hostname 127.0.0.1:" + ap + " --proxy-user carol:no http://localhost:" + site.port() + "/hi"),
              "hi from the target\n");
}

TEST(Socks5Server, GoClientThroughIt) {
    if (!have("go")) {
        GTEST_SKIP() << "no go";
    }
    auto src = source_root() / "tests/net/go_proxy/main.go";
    auto bin = std::filesystem::temp_directory_path() / "sgcl_go_proxy_server_test";
    if (std::system(("go build -o '" + bin.string() + "' '" + src.string() + "' > /dev/null 2>&1").c_str()) != 0) {
        GTEST_SKIP() << "go_proxy did not build";
    }
    Site site;
    net::socks5::server s;
    s.authenticate = [](const sgcl::string& u, const sgcl::string& pw) { return u == "gopher" && pw == "pw"; };
    Proxy p(s);
    auto out = run("'" + bin.string() + "' get socks5://gopher:pw@127.0.0.1:" + std::to_string(p.port()) + " http://localhost:" + site.port() + "/hi");
    EXPECT_EQ(out, "200 hi from the target\n");
}
