//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http/2: the module's client over HTTP/2 (detail/h2/transport.h) against
// Go's net/http server (tools/h2_oracle.go, built with go; skipped without
// it): h2 chosen by ALPN over TLS, h2c by prior knowledge, HTTP/1.1 when
// the server or the client leaves h2 out; bodies both ways within the
// windows (in memory and a stream), a hundred requests at once on one
// connection, a second connection only past the server's
// MAX_CONCURRENT_STREAMS, GOAWAY, a stream reset by the server, a
// request's deadline that resets its stream and leaves the connection, a
// body nobody reads beside one that flows. Every request has a timeout:
// nothing here waits unbounded.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <arpa/inet.h>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    std::string testdata(const std::string& name) {
        return (std::filesystem::path(__FILE__).parent_path().parent_path() / "tls_testdata" / name).string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    const std::string& oracle() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
            auto src = root / "tools" / "h2_oracle.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_h2_oracle_client_tests";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    // The oracle's server, stopped (SIGTERM) when the test ends
    struct Server {
        FILE* p = nullptr;
        int pid = 0;
        int port = 0;

        explicit Server(const std::string& args) {
            // the shell's pid first: exec keeps it for the oracle
            p = popen(("echo $$; exec '" + oracle() + "' " + args + " 2>/dev/null").c_str(), "r");
            char line[128] = {};
            if (p && fgets(line, sizeof(line), p)) {
                pid = std::atoi(line);
            }
            if (p && fgets(line, sizeof(line), p) && std::string(line).rfind("LISTEN ", 0) == 0) {
                port = std::atoi(line + 7);
            }
        }

        ~Server() {
            if (pid > 0) {
                ::kill(pid, SIGTERM);
            }
            if (p) {
                char buf[256];
                while (fgets(buf, sizeof(buf), p)) {
                }
                pclose(p);
            }
        }

        sgcl::string url(bool tls, const std::string& path) const {
            return sgcl::string((tls ? "https://localhost:" : "http://127.0.0.1:") + std::to_string(port) + path);
        }
    };

    std::string tls_args(const std::string& more = "") {
        return "-cert '" + testdata("ecdsa.pem") + "' -key '" + testdata("ecdsa.key") + "' " + more;
    }

    net::http::client tls_client() {
        net::http::client c;
        c.tls.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
        c.timeout = 20s;
        return c;
    }

    // The server's counters, from /stats
    long stat(net::http::client& c, const Server& s, bool tls, const std::string& name) {
        auto r = c.get(s.url(tls, "/stats"));
        if (!r) {
            return -1;
        }
        auto t = r->text();
        if (!t) {
            return -1;
        }
        std::string body = text(*t);
        auto at = body.find(name + "=");
        return at == std::string::npos ? -1 : std::atol(body.c_str() + at + name.size() + 1);
    }

    uint32_t crc32(const std::string& s) {
        uint32_t c = 0xFFFFFFFFu;
        for (unsigned char b : s) {
            c ^= b;
            for (int k = 0; k < 8; ++k) {
                c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
            }
        }
        return ~c;
    }

    std::string pattern(size_t n, size_t at = 0) {
        std::string s(n, '\0');
        for (size_t i = 0; i < n; ++i) {
            s[i] = char((at + i) % 251);
        }
        return s;
    }

    std::string size_answer(const std::string& body) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "bytes=%zu crc32=%08x\n", body.size(), crc32(body));
        return buf;
    }
}

TEST(H2Client_Tests, OverTlsByAlpn) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build the oracle with";
    }
    Server s(tls_args());
    ASSERT_GT(s.port, 0);
    auto c = tls_client();
    auto r = c.get(s.url(true, "/echo"));
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(r->status(), 200);
    EXPECT_EQ(r->proto(), "HTTP/2.0");
    EXPECT_EQ(r->header("x-alpn"), "h2");
    EXPECT_EQ(r->header("x-proto"), "HTTP/2.0");
    EXPECT_EQ(r->header("x-req-user-agent"), "");   // nothing the client did not set
    EXPECT_EQ(*r->text(), "");
    // a body of 1 MB each way, past every window
    const std::string big = pattern(1 << 20);
    auto echo = c.post(s.url(true, "/echo"), "application/octet-stream", sgcl::string(big));
    ASSERT_TRUE(echo) << text(echo.error().message());
    EXPECT_EQ(echo->header("x-body-bytes"), "1048576");
    EXPECT_EQ(echo->header("x-req-content-type"), "application/octet-stream");
    EXPECT_EQ(text(*echo->text()), big);
    // 10 MB up, read whole by the server
    const std::string huge = pattern(10 << 20);
    auto size = c.post(s.url(true, "/size"), "application/octet-stream", sgcl::string(huge));
    ASSERT_TRUE(size) << text(size.error().message());
    EXPECT_EQ(text(*size->text()), size_answer(huge));
    // a stream without a length
    net::http::request streamed("PUT", s.url(true, "/size"));
    const std::string two = pattern(2 << 20);
    streamed.set_body(io::reader(make_tracked<io::buffer>(sgcl::string(two))));
    auto put = c.send(streamed);
    ASSERT_TRUE(put) << text(put.error().message());
    EXPECT_EQ(text(*put->text()), size_answer(two));
    // a response flushed in pieces
    auto pieces = c.get(s.url(true, "/stream?n=50&size=16384"));
    ASSERT_TRUE(pieces);
    EXPECT_EQ(text(*pieces->text()), pattern(50 * 16384));
    // HEAD: no body
    auto head = c.head(s.url(true, "/echo"));
    ASSERT_TRUE(head);
    EXPECT_EQ(*head->text(), "");
    // a status of the server's choosing, fields both ways
    net::http::request teapot("GET", s.url(true, "/echo?status=418"));
    teapot.set_header("X-Mixed-Case", "Value");
    auto t = c.send(teapot);
    ASSERT_TRUE(t);
    EXPECT_EQ(t->status(), 418);
    EXPECT_EQ(t->header("x-req-x-mixed-case"), "Value");
    // one connection for all of it
    EXPECT_EQ(stat(c, s, true, "conns"), 1);
}

TEST(H2Client_Tests, HundredAtOnceOnOneConnection) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build the oracle with";
    }
    Server s(tls_args());
    ASSERT_GT(s.port, 0);
    auto c = tls_client();
    std::vector<async::task<expected<net::http::response, io::error>>> tasks;
    for (int i = 0; i < 100; ++i) {
        tasks.push_back(c.async_get(s.url(true, "/echo?delay=200")));
        tasks.back().spawn();
    }
    int ok = 0;
    for (auto& t : tasks) {
        auto r = t.wait();
        if (r && r->status() == 200 && r->proto() == "HTTP/2.0") {
            ++ok;
        }
    }
    EXPECT_EQ(ok, 100);
    EXPECT_EQ(stat(c, s, true, "conns"), 1);
    EXPECT_GE(stat(c, s, true, "inflight_max"), 50);
}

TEST(H2Client_Tests, H2cByPriorKnowledge) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build the oracle with";
    }
    Server s("");   // HTTP/1.1 and h2c on one port
    ASSERT_GT(s.port, 0);
    net::http::client c;
    c.timeout = 20s;
    c.h2c = true;
    auto r = c.post(s.url(false, "/echo"), "text/plain", "over h2c");
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(r->proto(), "HTTP/2.0");
    EXPECT_EQ(r->header("x-alpn"), "-");
    EXPECT_EQ(*r->text(), "over h2c");
    net::http::client plain;
    plain.timeout = 20s;
    auto h1 = plain.get(s.url(false, "/echo"));
    ASSERT_TRUE(h1);
    EXPECT_EQ(h1->proto(), "HTTP/1.1");
}

TEST(H2Client_Tests, Http11WhenH2IsLeftOut) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build the oracle with";
    }
    // the server without h2
    Server h1(tls_args("-protocols h1"));
    ASSERT_GT(h1.port, 0);
    auto c = tls_client();
    auto r = c.get(h1.url(true, "/echo"));
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(r->proto(), "HTTP/1.1");
    EXPECT_EQ(r->header("x-alpn"), "http/1.1");
    auto again = c.get(h1.url(true, "/echo"));   // from the pool of HTTP/1.1 connections
    ASSERT_TRUE(again);
    EXPECT_EQ(stat(c, h1, true, "conns"), 1);
    // the client without it
    Server both(tls_args());
    ASSERT_GT(both.port, 0);
    auto off = tls_client();
    off.http2 = false;
    auto o = off.get(both.url(true, "/echo"));
    ASSERT_TRUE(o) << text(o.error().message());
    EXPECT_EQ(o->proto(), "HTTP/1.1");
    EXPECT_EQ(o->header("x-alpn"), "http/1.1");
}

TEST(H2Client_Tests, SecondConnectionPastMaxStreams) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build the oracle with";
    }
    Server s(tls_args("-max-streams 5"));
    ASSERT_GT(s.port, 0);
    auto c = tls_client();
    ASSERT_TRUE(c.get(s.url(true, "/echo")));   // the server's SETTINGS known
    std::vector<async::task<expected<net::http::response, io::error>>> tasks;
    for (int i = 0; i < 20; ++i) {
        tasks.push_back(c.async_get(s.url(true, "/echo?delay=300")));
        tasks.back().spawn();
    }
    int ok = 0;
    for (auto& t : tasks) {
        auto r = t.wait();
        ok += r && r->status() == 200;
    }
    EXPECT_EQ(ok, 20);
    EXPECT_EQ(stat(c, s, true, "conns"), 4);   // 20 streams, 5 a connection
}

TEST(H2Client_Tests, Goaway) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build the oracle with";
    }
    Server s(tls_args("-goaway-after 2"));
    ASSERT_GT(s.port, 0);
    auto c = tls_client();
    for (int i = 0; i < 5; ++i) {
        auto r = c.get(s.url(true, "/echo"));
        ASSERT_TRUE(r) << i << ": " << text(r.error().message());
        EXPECT_EQ(r->status(), 200);
        EXPECT_EQ(*r->text(), "");
    }
    EXPECT_EQ(stat(c, s, true, "conns"), 3);   // requests 1-2, 3-4, 5 and this one
}

TEST(H2Client_Tests, StreamResetByTheServer) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build the oracle with";
    }
    Server s(tls_args());
    ASSERT_GT(s.port, 0);
    auto c = tls_client();
    auto r = c.get(s.url(true, "/abort?after=1000"));
    ASSERT_TRUE(r) << text(r.error().message());   // the head came
    auto body = r->text();
    EXPECT_FALSE(body);
    // the connection lives
    auto next = c.get(s.url(true, "/echo"));
    ASSERT_TRUE(next);
    EXPECT_EQ(next->status(), 200);
    EXPECT_EQ(stat(c, s, true, "conns"), 1);
}

TEST(H2Client_Tests, DeadlineResetsTheStreamNotTheConnection) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build the oracle with";
    }
    Server s(tls_args());
    ASSERT_GT(s.port, 0);
    auto c = tls_client();
    ASSERT_TRUE(c.get(s.url(true, "/echo")));
    c.timeout = 300ms;
    auto slow = c.get(s.url(true, "/echo?delay=2000"));
    ASSERT_FALSE(slow);
    EXPECT_EQ(slow.error().code(), std::errc::timed_out);
    // the head's deadline alone
    c.timeout = 20s;
    c.response_header_timeout = 300ms;
    auto slow_head = c.get(s.url(true, "/echo?delay=2000"));
    ASSERT_FALSE(slow_head);
    EXPECT_EQ(slow_head.error().code(), std::errc::timed_out);
    c.response_header_timeout = duration::zero();
    // a body read past the deadline
    c.timeout = 500ms;
    auto trickle = c.get(s.url(true, "/stream?n=20&size=100&pause=100"));
    ASSERT_TRUE(trickle) << text(trickle.error().message());
    auto rest = trickle->text();
    ASSERT_FALSE(rest);
    EXPECT_EQ(rest.error().code(), std::errc::timed_out);
    c.timeout = 20s;
    auto next = c.get(s.url(true, "/echo"));
    ASSERT_TRUE(next);
    EXPECT_EQ(stat(c, s, true, "conns"), 1);
}

TEST(H2Client_Tests, UnreadBodyBesideOneThatFlows) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go to build the oracle with";
    }
    Server s(tls_args());
    ASSERT_GT(s.port, 0);
    auto c = tls_client();
    // 4 MB nobody reads for now: its stream stops at its window (1 MB)
    auto unread = c.get(s.url(true, "/stream?n=256&size=16384"));
    ASSERT_TRUE(unread);
    // the connection's window (16 MB) lets another flow
    auto flows = c.get(s.url(true, "/stream?n=512&size=16384"));
    ASSERT_TRUE(flows);
    EXPECT_EQ(text(*flows->text()), pattern(512 * 16384));
    // the first read at last, whole
    EXPECT_EQ(text(*unread->text()), pattern(256 * 16384));
    EXPECT_EQ(stat(c, s, true, "conns"), 1);
}

// --- a scripted server: the cases Go's server cannot be made to show ----------

namespace {
    namespace h2 = net::http::detail::h2;

    // A server of HTTP/2 by prior knowledge on the loopback, written here
    // with the frames and HPACK of the module and plain sockets: what it
    // does to the first request of its first connection is the case's;
    // every other request is answered 200 "ok". Every wait has a timeout
    struct Scripted {
        enum class Case { refused, goaway, cut };

        Case what;
        int listener = -1;
        int port = 0;
        std::atomic<int> conns = 0;
        std::atomic<bool> stop = false;
        std::thread thread;

        explicit Scripted(Case c)
        : what(c) {
            listener = ::socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in a{};
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            ::bind(listener, reinterpret_cast<sockaddr*>(&a), sizeof(a));
            socklen_t n = sizeof(a);
            ::getsockname(listener, reinterpret_cast<sockaddr*>(&a), &n);
            port = ntohs(a.sin_port);
            ::listen(listener, 8);
            thread = std::thread([this] { run(); });
        }

        ~Scripted() {
            stop = true;
            thread.join();
            ::close(listener);
        }

        sgcl::string url() const {
            return sgcl::string("http://127.0.0.1:" + std::to_string(port) + "/x");
        }

        static void send_all(int fd, const std::string& s) {
            size_t at = 0;
            while (at < s.size()) {
                auto k = ::send(fd, s.data() + at, s.size() - at, 0);
                if (k <= 0) {
                    return;
                }
                at += size_t(k);
            }
        }

        void run() {
            while (!stop) {
                pollfd p{listener, POLLIN, 0};
                if (::poll(&p, 1, 50) <= 0) {
                    continue;
                }
                int fd = ::accept(listener, nullptr, nullptr);
                if (fd < 0) {
                    continue;
                }
                serve(fd, conns++);
                ::close(fd);
            }
        }

        void answer(int fd, h2::Encoder& enc, uint32_t id) {
            std::string out, block;
            enc.begin_block(block);
            enc.encode(block, ":status", "200");
            h2::FrameWriter w(out);
            w.headers(id, reinterpret_cast<const uint8_t*>(block.data()), block.size(), false, true);
            w.data(id, reinterpret_cast<const uint8_t*>("ok"), 2, true);
            send_all(fd, out);
        }

        void serve(int fd, int index) {
            timeval tv{5, 0};
            ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            std::string out;
            h2::FrameWriter(out).settings(nullptr, 0);
            send_all(fd, out);
            h2::Encoder enc;
            h2::Decoder dec;
            std::string in;
            bool preface = false;
            bool first = true;
            char buf[16384];
            for (;;) {
                // until the bytes, the test's end or 5 s
                int waited = 0;
                pollfd p{fd, POLLIN, 0};
                while (!stop && waited < 5000 && ::poll(&p, 1, 50) == 0) {
                    waited += 50;
                }
                if (stop || waited >= 5000) {
                    return;
                }
                auto k = ::recv(fd, buf, sizeof(buf), 0);
                if (k <= 0) {
                    return;
                }
                in.append(buf, size_t(k));
                if (!preface) {
                    if (in.size() < h2::PrefaceSize) {
                        continue;
                    }
                    in.erase(0, h2::PrefaceSize);
                    preface = true;
                }
                for (;;) {
                    auto f = h2::parse_frame(reinterpret_cast<const uint8_t*>(in.data()), in.size(), h2::LargestMaxFrameSize);
                    if (!f || f->size == 0) {
                        break;
                    }
                    const h2::Frame fr = f->frame;
                    const uint32_t id = fr.header.stream;
                    if (fr.type() == h2::FrameType::settings && !fr.ack()) {
                        std::string ack;
                        h2::FrameWriter(ack).settings_ack();
                        send_all(fd, ack);
                    } else if (fr.type() == h2::FrameType::headers) {
                        (void)dec.decode(fr.payload, 1 << 20);
                        if (first && index == 0) {
                            first = false;
                            std::string x;
                            if (what == Case::refused) {
                                h2::FrameWriter(x).rst_stream(id, h2::ErrorCode::refused_stream);
                                send_all(fd, x);
                            } else if (what == Case::goaway) {
                                h2::FrameWriter(x).goaway(id - 2, h2::ErrorCode::no_error);   // this stream never processed
                                send_all(fd, x);
                                ::usleep(100000);
                                return;
                            } else {
                                return;   // cut after the request's head
                            }
                        } else {
                            answer(fd, enc, id);
                        }
                    }
                    in.erase(0, f->size);
                }
            }
        }
    };

    net::http::client h2c_client() {
        net::http::client c;
        c.h2c = true;
        c.timeout = 10s;
        return c;
    }
}

TEST(H2Client_Tests, RefusedStreamRetriedForAnyMethod_8_7) {
    Scripted s(Scripted::Case::refused);
    auto c = h2c_client();
    auto r = c.post(s.url(), "text/plain", "x");
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(r->status(), 200);
    EXPECT_EQ(*r->text(), "ok");
    EXPECT_EQ(s.conns.load(), 1);   // the same connection
}

TEST(H2Client_Tests, AfterGoawayOnlyIdempotentRetried_6_8) {
    {
        Scripted s(Scripted::Case::goaway);
        auto c = h2c_client();
        auto r = c.get(s.url());
        ASSERT_TRUE(r) << text(r.error().message());
        EXPECT_EQ(*r->text(), "ok");
        EXPECT_EQ(s.conns.load(), 2);
    }
    {
        Scripted s(Scripted::Case::goaway);
        auto c = h2c_client();
        auto r = c.post(s.url(), "text/plain", "x");
        EXPECT_FALSE(r);   // not in v1: a POST is not sent again (sketch-http2.md, H5)
        EXPECT_EQ(s.conns.load(), 1);
    }
}

TEST(H2Client_Tests, ConnectionCutAfterTheHead) {
    {
        Scripted s(Scripted::Case::cut);
        auto c = h2c_client();
        auto r = c.get(s.url());
        ASSERT_TRUE(r) << text(r.error().message());
        EXPECT_EQ(*r->text(), "ok");
        EXPECT_EQ(s.conns.load(), 2);
    }
    {
        Scripted s(Scripted::Case::cut);
        auto c = h2c_client();
        auto r = c.post(s.url(), "text/plain", "x");
        EXPECT_FALSE(r);
        EXPECT_EQ(s.conns.load(), 1);
    }
}

// A connection closed is garbage at once: nothing of the client's (its
// ticker asleep for a second among them) keeps it, its TLS state or its
// buffers alive past its end
TEST(H2Client_Tests, AClosedConnectionIsGarbageAtOnce) {
    auto live_connections = [] {
        size_t n = 0;
        for (auto& s : collector::get_type_statistics()) {
            if (!s.buffers && *s.type == typeid(net::http::detail::h2::ClientH2)) {
                n += s.live_objects;
            }
        }
        return n;
    };
    net::http::server srv;
    srv.h2c = true;
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.write("ok");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    const auto url = sgcl::string("http://127.0.0.1:" + std::to_string(listener.local_endpoint().port()) + "/");
    {
        net::http::client web;
        web.h2c = true;
        web.timeout = 10s;   // the stream's timers must not keep it either
        {
            auto r = web.get(url);
            ASSERT_TRUE(r) << text(r.error().message());
            EXPECT_EQ(r->proto(), "HTTP/2.0");
            EXPECT_EQ(*r->text(), "ok");
        }
        EXPECT_GE(live_connections(), 1u);
        web.close_idle_connections();   // GOAWAY, the server closes, the reader ends
    }
    // gone once the connection has ended (the GOAWAY's round trip, a few
    // ms): looked for over 300 ms of the clock, well within the ticker's
    // second, so a ticker that held the connection through its sleep fails
    const auto until = std::chrono::steady_clock::now() + 300ms;
    size_t left = 1;
    while (left && std::chrono::steady_clock::now() < until) {
        std::this_thread::sleep_for(5ms);
        collector::force_collect(true);
        left = live_connections();
    }
    EXPECT_EQ(left, 0u);
    srv.close();
    (void)serving.wait();
}
