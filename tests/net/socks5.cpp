//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::socks5 against a SOCKS5 server written here from RFC 1928 and RFC
// 1929 (socks5_server.h): the three address types, username/password, every
// REP code as its error, the replies that break the protocol, a reply with
// the target's first bytes behind it, the timeout and the stop, targets and
// credentials refused before a byte is sent, the handshake over a
// connection there is. Interop: curl's --socks5 and --socks5-hostname
// through the test server (which the client's tests rest on), the client
// through a SOCKS5 server written in Go, and Go's net/http through the test
// server (go_proxy/main.go; skipped where there is no go or curl).
#include "tests/types.h"
#include "tests/source_root.h"
#include "tests/net/socks5_server.h"
#include "sgcl/net/net.h"
#include "sgcl/net/socks5.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

using namespace sgcl;
using sgcl_test::EchoServer;
using sgcl_test::Socks5TestServer;

namespace {
    using namespace std::chrono_literals;

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    sgcl::string local(uint16_t port) {
        return sgcl::string("127.0.0.1:" + std::to_string(port));
    }

    // a line there and back through the connection
    std::string round_trip(const net::connection& c, const std::string& line) {
        auto w = c.write(sgcl::string(std::string_view(line)));
        if (!w) {
            return "write: " + text(w.error().message());
        }
        vector<byte> buf(line.size());
        auto r = c.read_full(buf.as_slice());
        if (!r) {
            return "read: " + text(r.error().message());
        }
        return std::string(reinterpret_cast<const char*>(buf.data()), *r);
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

    // The Go helper built once; "" when there is no go to build it with
    const std::string& go_proxy() {
        static std::string path = [] {
            if (!have("go")) {
                return std::string();
            }
            auto src = source_root() / "tests/net/go_proxy/main.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_go_proxy";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    std::string last_host(Socks5TestServer& s) {
        std::lock_guard g(s.lock);
        return s.last_host;
    }

    int last_atyp(Socks5TestServer& s) {
        std::lock_guard g(s.lock);
        return s.last_atyp;
    }

    std::string last_methods(Socks5TestServer& s) {
        std::lock_guard g(s.lock);
        return s.last_methods;
    }

    net::socks5::options credentials(const char* user, const char* pass) {
        net::socks5::options o;
        o.username = user;
        o.password = pass;
        return o;
    }
}

TEST(Socks5_Tests, ConnectsToAnIpv4Address) {
    auto echo = EchoServer::start();
    auto proxy = Socks5TestServer::start();
    auto c = net::socks5::connect(proxy->address(), local(echo->port()));
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(round_trip(*c, "hello through socks"), "hello through socks");
    EXPECT_EQ(last_atyp(*proxy), 1);
    EXPECT_EQ(last_host(*proxy), "127.0.0.1");
    EXPECT_EQ(last_methods(*proxy), std::string(1, '\0'));   // no credentials: no authentication alone
    EXPECT_EQ(c->remote_endpoint().port(), proxy->port());   // the connection is the proxy's
    EXPECT_EQ(c->read_deadline(), time_point());              // the handshake's deadline removed
    EXPECT_EQ(c->write_deadline(), time_point());
    (void)c->close();
    proxy->close();
    echo->close();
}

TEST(Socks5_Tests, SendsANameForTheProxyToResolve) {
    auto echo = EchoServer::start();
    auto proxy = Socks5TestServer::start();
    auto c = net::socks5::connect(proxy->address(), sgcl::string("localhost:" + std::to_string(echo->port())));
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(last_atyp(*proxy), 3);
    EXPECT_EQ(last_host(*proxy), "localhost");
    EXPECT_EQ(round_trip(*c, "by name"), "by name");
    (void)c->close();
    proxy->close();
    echo->close();
}

TEST(Socks5_Tests, ConnectsToAnIpv6Address) {
    auto echo = EchoServer::start("[::1]:0");
    if (!echo->listener) {
        GTEST_SKIP() << "no IPv6 loopback";
    }
    auto proxy = Socks5TestServer::start();
    auto c = net::socks5::connect(proxy->address(), sgcl::string("[::1]:" + std::to_string(echo->port())));
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(last_atyp(*proxy), 4);
    EXPECT_EQ(last_host(*proxy), "::1");
    EXPECT_EQ(round_trip(*c, "six"), "six");
    (void)c->close();
    proxy->close();
    echo->close();
}

TEST(Socks5_Tests, UsernameAndPassword) {
    auto echo = EchoServer::start();
    Socks5TestServer::Behavior b;
    b.username = "alice";
    b.password = "s3cret";
    auto proxy = Socks5TestServer::start(b);
    auto c = net::socks5::connect(proxy->address(), local(echo->port()), credentials("alice", "s3cret"));
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(last_methods(*proxy), std::string("\0\2", 2));   // both offered
    EXPECT_EQ(round_trip(*c, "authenticated"), "authenticated");
    (void)c->close();

    auto wrong = net::socks5::connect(proxy->address(), local(echo->port()), credentials("alice", "nope"));
    ASSERT_FALSE(wrong);
    EXPECT_EQ(wrong.error().code(), net::errc::proxy_auth_required);
    EXPECT_EQ(wrong.error().op(), "socks5");
    EXPECT_EQ(text(wrong.error().path()), text(proxy->address()) + "->" + text(local(echo->port())));

    auto none = net::socks5::connect(proxy->address(), local(echo->port()));   // the server finds no method it takes
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), net::errc::proxy_auth_required);
    EXPECT_EQ(text(none.error().message()), "socks5 " + text(proxy->address()) + "->" + text(local(echo->port())) + ": proxy authentication required");
    proxy->close();
    echo->close();
}

TEST(Socks5_Tests, CredentialsTheServerDoesNotAskFor) {
    auto echo = EchoServer::start();
    auto proxy = Socks5TestServer::start();   // chooses no authentication
    auto c = net::socks5::connect(proxy->address(), local(echo->port()), credentials("bob", ""));
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(round_trip(*c, "x"), "x");
    (void)c->close();
    proxy->close();
    echo->close();
}

TEST(Socks5_Tests, AnEmptyPasswordAndCredentialsAtTheirLimits) {
    auto echo = EchoServer::start();
    Socks5TestServer::Behavior b;
    b.username = std::string(255, 'u');
    b.password = std::string(255, 'p');
    auto proxy = Socks5TestServer::start(b);
    auto c = net::socks5::connect(proxy->address(), local(echo->port()), credentials(b.username.c_str(), b.password.c_str()));
    ASSERT_TRUE(c) << text(c.error().message());
    (void)c->close();
    proxy->close();

    Socks5TestServer::Behavior e;
    e.username = "u";
    auto empty = Socks5TestServer::start(e);
    auto d = net::socks5::connect(empty->address(), local(echo->port()), credentials("u", ""));
    ASSERT_TRUE(d) << text(d.error().message());
    (void)d->close();
    empty->close();

    // one byte past: refused before anything is dialed
    auto over = Socks5TestServer::start();
    auto long_user = net::socks5::connect(over->address(), local(echo->port()), credentials(std::string(256, 'u').c_str(), "p"));
    ASSERT_FALSE(long_user);
    EXPECT_EQ(long_user.error().code(), std::errc::invalid_argument);
    auto long_pass = net::socks5::connect(over->address(), local(echo->port()), credentials("u", std::string(256, 'p').c_str()));
    ASSERT_FALSE(long_pass);
    EXPECT_EQ(long_pass.error().code(), std::errc::invalid_argument);
    auto pass_alone = net::socks5::connect(over->address(), local(echo->port()), credentials("", "p"));
    ASSERT_FALSE(pass_alone);
    EXPECT_EQ(pass_alone.error().code(), std::errc::invalid_argument);
    EXPECT_EQ(over->connections.load(), 0);
    over->close();
    echo->close();
}

TEST(Socks5_Tests, EveryReplyCodeAsItsError) {
    struct Case {
        int rep;
        error_code code;
    };
    const Case cases[] = {
        {1, net::errc::proxy_failure},
        {2, net::errc::proxy_refused},
        {3, error_code(ENETUNREACH, std::system_category())},
        {4, error_code(EHOSTUNREACH, std::system_category())},
        {5, error_code(ECONNREFUSED, std::system_category())},
        {6, error_code(ETIMEDOUT, std::system_category())},
        {7, net::errc::proxy_unsupported},
        {8, net::errc::proxy_unsupported},
        {9, net::errc::proxy_failure},
        {255, net::errc::proxy_failure},
    };
    for (auto& k : cases) {
        Socks5TestServer::Behavior b;
        b.reply = k.rep;
        auto proxy = Socks5TestServer::start(b);
        auto c = net::socks5::connect(proxy->address(), "example.com:80");
        ASSERT_FALSE(c) << k.rep;
        EXPECT_EQ(c.error().code(), k.code) << k.rep;
        EXPECT_EQ(c.error().op(), "socks5");
        proxy->close();
    }
    // the timeout one reads as a timeout, the refusal as a refusal
    Socks5TestServer::Behavior b;
    b.reply = 6;
    auto proxy = Socks5TestServer::start(b);
    auto c = net::socks5::connect(proxy->address(), "example.com:80");
    EXPECT_TRUE(c.error().is_timeout());
    proxy->close();
}

TEST(Socks5_Tests, ATargetTheProxyCannotReach) {
    auto closed = EchoServer::start();
    uint16_t port = closed->port();
    closed->close();
    auto proxy = Socks5TestServer::start();
    auto c = net::socks5::connect(proxy->address(), local(port));
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().code(), std::errc::connection_refused);
    proxy->close();
}

TEST(Socks5_Tests, RepliesThatBreakTheProtocol) {
    auto echo = EchoServer::start();
    auto failing = [&](auto set) {
        Socks5TestServer::Behavior b;
        set(b);
        auto proxy = Socks5TestServer::start(b);
        auto c = net::socks5::connect(proxy->address(), local(echo->port()), credentials("u", "p"));
        proxy->close();
        return c ? error_code() : c.error().code();
    };
    EXPECT_EQ(failing([](auto& b) { b.method_version = 4; }), net::errc::malformed_proxy_response);
    EXPECT_EQ(failing([](auto& b) { b.force_method = 1; }), net::errc::malformed_proxy_response);   // GSSAPI, never offered
    EXPECT_EQ(failing([](auto& b) { b.force_method = 0xFF; }), net::errc::proxy_auth_required);
    EXPECT_EQ(failing([](auto& b) { b.reply_version = 4; }), net::errc::malformed_proxy_response);
    EXPECT_EQ(failing([](auto& b) { b.bound_atyp = 2; }), net::errc::malformed_proxy_response);
    EXPECT_EQ(failing([](auto& b) { b.close_after_greeting = true; }), io::errc::unexpected_eof);
    EXPECT_EQ(failing([](auto& b) { b.short_reply = true; }), io::errc::unexpected_eof);
    // the version byte of RFC 1929's reply is not judged, as curl and Go
    // servers answer 1 or 5
    EXPECT_EQ(failing([](auto& b) { b.username = "u"; b.password = "p"; b.auth_version = 5; }), error_code());
    // username/password chosen without credentials offered
    Socks5TestServer::Behavior b;
    b.force_method = 2;
    auto proxy = Socks5TestServer::start(b);
    auto c = net::socks5::connect(proxy->address(), local(echo->port()));
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().code(), net::errc::malformed_proxy_response);
    proxy->close();
    echo->close();
}

TEST(Socks5_Tests, EveryBoundAddressTypeAndTheTargetsFirstBytesKept) {
    auto echo = EchoServer::start();
    for (int atyp : {1, 3, 4}) {
        Socks5TestServer::Behavior b;
        b.bound_atyp = atyp;
        b.with_reply = "early";   // in the reply's own write: the client must not read past the reply
        auto proxy = Socks5TestServer::start(b);
        auto c = net::socks5::connect(proxy->address(), local(echo->port()));
        ASSERT_TRUE(c) << atyp << ": " << text(c.error().message());
        vector<byte> first(5);
        auto r = c->read_full(first.as_slice());
        ASSERT_TRUE(r);
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(first.data()), 5), "early") << atyp;
        (void)c->close();
        proxy->close();
    }
    echo->close();
}

TEST(Socks5_Tests, TargetsRefusedBeforeAByteIsSent) {
    auto proxy = Socks5TestServer::start();
    for (const char* bad : {"", "nohost", ":80", "example.com:", "example.com:65536", "example.com:http", "[::1:80", "::1:80",
                            "[example.com]:80", "[fe80::1%en0]:80"}) {
        auto c = net::socks5::connect(proxy->address(), bad);
        ASSERT_FALSE(c) << bad;
        EXPECT_EQ(c.error().code(), net::errc::invalid_address) << bad;
        EXPECT_EQ(c.error().op(), "socks5") << bad;
    }
    auto too_long = net::socks5::connect(proxy->address(), sgcl::string(std::string(256, 'a') + ":80"));
    ASSERT_FALSE(too_long);
    EXPECT_EQ(too_long.error().code(), net::errc::invalid_address);
    EXPECT_EQ(proxy->connections.load(), 0);

    auto longest = net::socks5::connect(proxy->address(), sgcl::string(std::string(255, 'a') + ":80"));   // sent; the proxy fails to resolve it
    ASSERT_FALSE(longest);
    EXPECT_EQ(longest.error().code(), std::errc::host_unreachable);
    EXPECT_EQ(last_host(*proxy), std::string(255, 'a'));
    proxy->close();
}

TEST(Socks5_Tests, AProxyThatIsNotThere) {
    auto gone = Socks5TestServer::start();
    auto address = gone->address();
    gone->close();
    auto c = net::socks5::connect(address, "example.com:80");
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().code(), std::errc::connection_refused);
    EXPECT_EQ(c.error().op(), "dial tcp");
    auto bad = net::socks5::connect("noport", "example.com:80");
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), net::errc::invalid_address);
}

TEST(Socks5_Tests, TheTimeoutBoundsTheHandshake) {
    Socks5TestServer::Behavior b;
    b.silent = true;
    auto proxy = Socks5TestServer::start(b);
    net::socks5::options o;
    o.timeout = 150 * millisecond;
    auto t0 = std::chrono::steady_clock::now();
    auto c = net::socks5::connect(proxy->address(), "example.com:80", o);
    auto took = std::chrono::steady_clock::now() - t0;
    ASSERT_FALSE(c);
    EXPECT_TRUE(c.error().is_timeout()) << text(c.error().message());
    EXPECT_EQ(c.error().op(), "socks5");
    EXPECT_GE(took, 140ms);
    EXPECT_LT(took, 5s);

    o.timeout = duration::zero();   // none: the stop ends it
    async::stop_source source;
    o.stop = source.token();
    std::thread stopper([&] {
        std::this_thread::sleep_for(100ms);
        source.request_stop();
    });
    auto stopped = net::socks5::connect(proxy->address(), "example.com:80", o);
    stopper.join();
    ASSERT_FALSE(stopped);
    EXPECT_EQ(stopped.error().code(), std::errc::operation_canceled);

    async::stop_source before;
    before.request_stop();
    o.stop = before.token();
    auto at_once = net::socks5::connect(proxy->address(), "example.com:80", o);
    ASSERT_FALSE(at_once);
    EXPECT_EQ(at_once.error().code(), std::errc::operation_canceled);
    proxy->close();
}

TEST(Socks5_Tests, TheStopReachesAHandshakeOverATransport) {
    Socks5TestServer::Behavior b;
    b.silent = true;
    auto proxy = Socks5TestServer::start(b);
    auto transport = net::tcp::connect(proxy->listener.local_endpoint());
    ASSERT_TRUE(transport);
    net::socks5::options o;
    async::stop_source source;
    o.stop = source.token();
    o.timeout = duration::zero();
    std::thread stopper([&] {
        std::this_thread::sleep_for(50ms);
        source.request_stop();
    });
    auto c = net::socks5::client(*transport, "example.com:80", o);
    stopper.join();
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().code(), std::errc::operation_canceled);
    EXPECT_TRUE(transport->is_closed());   // closed with the failure
    proxy->close();
}

TEST(Socks5_Tests, TheHandshakeOverAConnectionThereIs) {
    auto echo = EchoServer::start();
    auto proxy = Socks5TestServer::start();
    auto transport = net::tcp::connect(proxy->listener.local_endpoint());
    ASSERT_TRUE(transport);
    transport->set_deadline(sgcl::clock::now() + 60 * second);
    auto c = net::socks5::client(*transport, local(echo->port()));
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(*c, *transport);                       // the same connection
    EXPECT_EQ(c->read_deadline(), time_point());     // replaced, then removed
    EXPECT_EQ(round_trip(*c, "over a transport"), "over a transport");
    (void)c->close();

    // a failure closes the transport
    Socks5TestServer::Behavior b;
    b.reply = 2;
    auto refusing = Socks5TestServer::start(b);
    auto t2 = net::tcp::connect(refusing->listener.local_endpoint());
    ASSERT_TRUE(t2);
    auto r = net::socks5::client(*t2, "example.com:80");
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), net::errc::proxy_refused);
    EXPECT_EQ(text(r.error().path()), text(refusing->address()) + "->example.com:80");
    EXPECT_TRUE(t2->is_closed());
    // and so does a target refused
    auto t3 = net::tcp::connect(refusing->listener.local_endpoint());
    ASSERT_TRUE(t3);
    auto bad = net::socks5::client(*t3, "nohost");
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), net::errc::invalid_address);
    EXPECT_TRUE(t3->is_closed());
    refusing->close();
    proxy->close();
    echo->close();
}

TEST(Socks5_Tests, TheTaskForms) {
    auto echo = EchoServer::start();
    Socks5TestServer::Behavior b;
    b.username = "u";
    b.password = "p";
    auto proxy = Socks5TestServer::start(b);
    auto run = [](sgcl::string proxy_at, sgcl::string target) -> async::task<std::string> {
        auto c = co_await net::socks5::async_connect(proxy_at, target, credentials("u", "p"));
        if (!c) {
            co_return text(c.error().message());
        }
        (void)co_await c->async_write("in a task");
        vector<byte> buf(9);
        auto r = co_await c->async_read_full(buf.as_slice());
        (void)c->close();
        co_return r ? std::string(reinterpret_cast<const char*>(buf.data()), *r) : std::string("read failed");
    };
    EXPECT_EQ(async::spawn(run(proxy->address(), local(echo->port()))).wait(), "in a task");
    auto refused = [](sgcl::string proxy_at) -> async::task<error_code> {
        auto c = co_await net::socks5::async_connect(proxy_at, "example.com:80");
        co_return c ? error_code() : c.error().code();
    };
    EXPECT_EQ(async::spawn(refused(proxy->address())).wait(), net::errc::proxy_auth_required);
    auto over = [](net::endpoint at, sgcl::string target) -> async::task<std::string> {
        auto t = co_await net::tcp::async_connect(at);
        if (!t) {
            co_return "dial";
        }
        auto c = co_await net::socks5::async_client(*t, target, credentials("u", "p"));
        if (!c) {
            co_return text(c.error().message());
        }
        (void)c->close();
        co_return "ok";
    };
    EXPECT_EQ(async::spawn(over(proxy->listener.local_endpoint(), local(echo->port()))).wait(), "ok");
    auto plain = [](net::endpoint at, sgcl::string target) -> async::task<error_code> {
        auto t = co_await net::tcp::async_connect(at);
        auto c = co_await net::socks5::async_client(*t, target);
        co_return c ? error_code() : c.error().code();
    };
    EXPECT_EQ(async::spawn(plain(proxy->listener.local_endpoint(), local(echo->port()))).wait(), net::errc::proxy_auth_required);
    proxy->close();
    echo->close();
}

TEST(Socks5_Tests, ManyConnectionsAtOnce) {
    auto echo = EchoServer::start();
    auto proxy = Socks5TestServer::start();
    auto one = [](sgcl::string proxy_at, sgcl::string target, int i) -> async::task<bool> {
        auto c = co_await net::socks5::async_connect(proxy_at, target);
        if (!c) {
            co_return false;
        }
        std::string line = "line " + std::to_string(i);
        (void)co_await c->async_write(sgcl::string(std::string_view(line)));
        vector<byte> buf(line.size());
        auto r = co_await c->async_read_full(buf.as_slice());
        (void)c->close();
        co_return r && std::string(reinterpret_cast<const char*>(buf.data()), *r) == line;
    };
    auto all = [&](sgcl::string proxy_at, sgcl::string target) -> async::task<int> {
        vector<async::task<bool>> tasks;
        for (int i : range(64)) {
            tasks.push_back(async::spawn(one(proxy_at, target, i)));
        }
        int ok = 0;
        for (auto& t : tasks) {
            ok += co_await t;
        }
        co_return ok;
    };
    EXPECT_EQ(async::spawn(all(proxy->address(), local(echo->port()))).wait(), 64);
    EXPECT_EQ(proxy->tunnels.load(), 64);
    proxy->close();
    echo->close();
}

TEST(Socks5_Tests, OptionsByDefault) {
    net::socks5::options o;
    EXPECT_TRUE(o.username.empty());
    EXPECT_TRUE(o.password.empty());
    EXPECT_EQ(o.timeout, 30 * second);
    EXPECT_FALSE(o.stop.stop_possible());
}

// curl's SOCKS5 client through the test server: what the client's tests
// rest on is a server curl agrees with
TEST(Socks5Interop_Tests, CurlThroughTheTestServer) {
    if (!have("curl")) {
        GTEST_SKIP() << "no curl";
    }
    net::http::server web;
    web.route("GET /hi", [](net::http::request, net::http::response_writer w) { w.write("hi from the target\n"); });
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(web.async_serve(l));
    std::string port = std::to_string(l.local_endpoint().port());

    auto proxy = Socks5TestServer::start();
    std::string p = std::to_string(proxy->port());
    EXPECT_EQ(run("curl -s --max-time 10 --socks5 127.0.0.1:" + p + " http://127.0.0.1:" + port + "/hi"), "hi from the target\n");
    EXPECT_EQ(last_atyp(*proxy), 1);
    EXPECT_EQ(run("curl -s --max-time 10 --socks5-hostname 127.0.0.1:" + p + " http://localhost:" + port + "/hi"), "hi from the target\n");
    EXPECT_EQ(last_atyp(*proxy), 3);
    EXPECT_EQ(last_host(*proxy), "localhost");
    proxy->close();

    Socks5TestServer::Behavior b;
    b.username = "carol";
    b.password = "pw";
    auto authed = Socks5TestServer::start(b);
    std::string a = std::to_string(authed->port());
    EXPECT_EQ(run("curl -s --max-time 10 --socks5-hostname 127.0.0.1:" + a + " --proxy-user carol:pw http://localhost:" + port + "/hi"),
              "hi from the target\n");
    EXPECT_NE(run("curl -s --max-time 10 --socks5-hostname 127.0.0.1:" + a + " --proxy-user carol:no http://localhost:" + port + "/hi"),
              "hi from the target\n");
    authed->close();
    web.close();
    (void)serving.wait();
}

// The client through a SOCKS5 server written in Go (go_proxy/main.go:
// socks5 mode), and Go's net/http client through the test server
TEST(Socks5Interop_Tests, GoServerAndGoClient) {
    if (go_proxy().empty()) {
        GTEST_SKIP() << "no go to build the helper with";
    }
    auto echo = EchoServer::start();
    FILE* p = popen(("'" + go_proxy() + "' socks5 alice pw").c_str(), "r");
    ASSERT_TRUE(p);
    char line[64] = {};
    ASSERT_TRUE(fgets(line, sizeof(line), p));
    int port = std::atoi(line + 5);
    ASSERT_GT(port, 0);
    auto proxy_at = local(uint16_t(port));
    auto c = net::socks5::connect(proxy_at, local(echo->port()), credentials("alice", "pw"));
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(round_trip(*c, "through go"), "through go");
    (void)c->close();
    auto named = net::socks5::connect(proxy_at, sgcl::string("localhost:" + std::to_string(echo->port())), credentials("alice", "pw"));
    ASSERT_TRUE(named) << text(named.error().message());
    EXPECT_EQ(round_trip(*named, "by name through go"), "by name through go");
    (void)named->close();
    auto wrong = net::socks5::connect(proxy_at, local(echo->port()), credentials("alice", "no"));
    ASSERT_FALSE(wrong);
    EXPECT_EQ(wrong.error().code(), net::errc::proxy_auth_required);
    auto refused = net::socks5::connect(proxy_at, "127.0.0.1:1", credentials("alice", "pw"));
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), std::errc::connection_refused);
    (void)net::socks5::connect(proxy_at, "quit.invalid:1", credentials("alice", "pw"));   // the helper's way out
    pclose(p);

    // Go's client (http.Transport with a socks5:// proxy) through the test server
    net::http::server web;
    web.route("GET /hi", [](net::http::request, net::http::response_writer w) { w.write("hi to go\n"); });
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(web.async_serve(l));
    Socks5TestServer::Behavior b;
    b.username = "gopher";
    b.password = "pw";
    auto proxy = Socks5TestServer::start(b);
    auto out = run("'" + go_proxy() + "' get socks5://gopher:pw@127.0.0.1:" + std::to_string(proxy->port()) + " http://localhost:" +
                   std::to_string(l.local_endpoint().port()) + "/hi");
    EXPECT_EQ(out, "200 hi to go\n");
    EXPECT_EQ(last_atyp(*proxy), 3);   // Go sends the name
    proxy->close();
    web.close();
    (void)serving.wait();
    echo->close();
}
