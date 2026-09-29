//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http::server::access_log (DESIGN 283): a record per exchange with the
// method, path, protocol, status, the body's bytes, the time, the remote
// address, the user agent and the request id; info, error for a 5xx; over
// HTTP/1.1 and HTTP/2 (h2c); nothing managed per request.
#include "tests/types.h"
#include "tests/managed_pages.h"
#include "sgcl/net/http/http.h"
#include "sgcl/slog/slog.h"

#include <string>
#include <string_view>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    namespace http = sgcl::net::http;

    struct Serving {
        http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;

        explicit Serving(http::server s)
        : server(s) {
            listener = *net::tcp::listen("127.0.0.1:0");
            serving = async::spawn(server.async_serve(listener));
        }

        ~Serving() {
            server.close();
            (void)serving.wait();
        }

        string url(const std::string& path) const {
            return string("http://127.0.0.1:" + std::to_string(listener.local_endpoint().port()) + path);
        }
    };

    http::server routes() {
        http::server s;
        s.route("GET /hello", [](http::request, http::response_writer w) {
            w.write("hello");
        });
        s.route("GET /fail", [](http::request, http::response_writer w) {
            w.error(503);
        });
        s.route("GET /flushed", [](http::request, http::response_writer w) -> async::task<> {
            w.write("abc");
            (void)co_await w.async_flush();
            w.write("defg");
        });
        return s;
    }

    // The attributes of a kept record, by key
    std::string text_of(const slog::record& r, std::string_view key) {
        for (auto a : r) {
            if (std::string_view(a.key().data(), a.key().size()) == key) {
                auto t = a.value().text();
                return std::string(t.data(), t.size());
            }
        }
        return "<none>";
    }

    // Waits for the handler's record: the log is written after the
    // response is sent, which the client may see first
    void wait_for(const slog::memory& kept, size_t n) {
        for (int i = 0; i < 2000 && kept.size() < n; ++i) {
            std::this_thread::sleep_for(1ms);
        }
    }
}

TEST(HttpAccessLog_Tests, ARecordPerExchange) {
    slog::memory kept;
    auto s = routes();
    s.access_log(slog::logger(kept));
    Serving r(s);
    http::client c;
    http::request req("GET", r.url("/hello"));
    req.headers().set("User-Agent", "sgcl-test/1");
    req.headers().set("X-Request-ID", "abc-123");
    auto res = c.send(req);
    ASSERT_TRUE(res);
    EXPECT_EQ(*res->text(), "hello");
    wait_for(kept, 1);
    auto records = kept.records();
    ASSERT_EQ(records.size(), 1u);
    const auto& rec = records[0];
    EXPECT_EQ(rec.level(), slog::level::info);
    EXPECT_EQ(std::string_view(rec.message().data(), rec.message().size()), "request");
    EXPECT_EQ(text_of(rec, "method"), "GET");
    EXPECT_EQ(text_of(rec, "path"), "/hello");
    EXPECT_EQ(text_of(rec, "proto"), "HTTP/1.1");
    EXPECT_EQ(text_of(rec, "status"), "200");
    EXPECT_EQ(text_of(rec, "bytes"), "5");
    EXPECT_EQ(text_of(rec, "user_agent"), "sgcl-test/1");
    EXPECT_EQ(text_of(rec, "request_id"), "abc-123");
    EXPECT_EQ(text_of(rec, "remote").rfind("127.0.0.1:", 0), 0u);
    EXPECT_NE(text_of(rec, "duration"), "<none>");
}

TEST(HttpAccessLog_Tests, AFiveHundredIsAnErrorAndNoIdNoField) {
    slog::memory kept;
    auto s = routes();
    s.access_log(slog::logger(kept));
    Serving r(s);
    http::client c;
    auto res = c.get(r.url("/fail"));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status(), 503);
    wait_for(kept, 1);
    auto records = kept.records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].level(), slog::level::error);
    EXPECT_EQ(text_of(records[0], "status"), "503");
    EXPECT_EQ(text_of(records[0], "request_id"), "<none>");
}

TEST(HttpAccessLog_Tests, TheBytesOfTheBody) {
    slog::memory kept;
    auto s = routes();
    s.access_log(slog::logger(kept));
    Serving r(s);
    http::client c;
    ASSERT_TRUE(c.get(r.url("/flushed")));
    ASSERT_TRUE(c.head(r.url("/hello")));
    auto missing = c.get(r.url("/none"));
    ASSERT_TRUE(missing);
    wait_for(kept, 3);
    auto records = kept.records();
    ASSERT_EQ(records.size(), 3u);
    EXPECT_EQ(text_of(records[0], "bytes"), "7");   // 3 flushed, 4 after
    EXPECT_EQ(text_of(records[1], "bytes"), "0");   // HEAD
    EXPECT_EQ(text_of(records[2], "status"), "404");
}

TEST(HttpAccessLog_Tests, HttpTwo) {
    slog::memory kept;
    auto s = routes();
    s.h2c = true;
    s.access_log(slog::logger(kept));
    Serving r(s);
    http::client c;
    c.h2c = true;
    auto res = c.get(r.url("/hello"));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->proto(), "HTTP/2.0");
    wait_for(kept, 1);
    auto records = kept.records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(text_of(records[0], "proto"), "HTTP/2.0");
    EXPECT_EQ(text_of(records[0], "path"), "/hello");
    EXPECT_EQ(text_of(records[0], "bytes"), "5");
}

TEST(HttpAccessLog_Tests, TextLinesToAWriter) {
    // the line is written on a worker after the response has gone, while
    // this thread waits for it: a writer that takes it under a lock (an
    // io::buffer is one thread's at a time)
    struct Locked {
        std::mutex lock;
        std::string text;

        expected<size_t, io::error> write(const slice<const byte>& d) {
            std::lock_guard<std::mutex> g(lock);
            text.append(reinterpret_cast<const char*>(d.data()), d.size());
            return d.size();
        }

        std::string get() {
            std::lock_guard<std::mutex> g(lock);
            return text;
        }
    };
    Locked out;
    auto s = routes();
    s.access_log(slog::logger(slog::options{.out = out, .utc = true}));
    Serving r(s);
    http::client c;
    ASSERT_TRUE(c.get(r.url("/hello")));
    for (int i = 0; i < 2000 && out.get().empty(); ++i) {
        std::this_thread::sleep_for(1ms);
    }
    std::string line = out.get();
    EXPECT_NE(line.find(" level=INFO msg=request method=GET path=/hello proto=HTTP/1.1 status=200 bytes=5 duration="), std::string::npos) << line;
    EXPECT_NE(line.find(" remote=127.0.0.1:"), std::string::npos) << line;
}

// Many clients at once, over HTTP/1.1 and h2c, through a buffered text
// log and a handler that keeps the records: a record for every request
// (under TSan as well)
TEST(HttpAccessLog_Tests, ManyClientsAtOnce) {
    slog::memory kept;
    auto s = routes();
    s.h2c = true;
    s.access_log(slog::logger(kept));
    auto t = routes();
    t.h2c = true;
    t.access_log(slog::logger(slog::options{.out = io::discard, .buffered = true}));
    Serving a(s);
    Serving b(t);
    constexpr int Threads = 6;
    constexpr int Each = 50;
    std::vector<std::thread> threads;
    std::atomic<int> ok{0};
    for (int i = 0; i < Threads; ++i) {
        threads.emplace_back([&, i] {
            http::client c;
            c.h2c = i % 2 == 1;
            const string ua = a.url("/hello");
            const string ub = b.url("/hello");
            for (int k = 0; k < Each; ++k) {
                auto ra = c.get(ua);
                auto rb = c.get(ub);
                if (ra && ra->text() && rb && rb->text()) {
                    ++ok;
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_EQ(ok.load(), Threads * Each);
    wait_for(kept, size_t(Threads * Each));
    EXPECT_EQ(kept.size(), size_t(Threads * Each));
}

// What the log adds to a request on the managed heap: nothing. The
// record itself, as the server makes it (detail::log_access over a
// request and a writer as a finished exchange leaves them), twenty
// thousand times with the collector parked: at most a page, where one
// managed byte a record would be one. A whole exchange cannot show it: a
// request over loopback takes some 20 KB of managed memory of its own
// (the request, the writer, the client's response), and 5000 of them vary
// by eight pages from run to run (measured: 1583 to 1594 without the log,
// 1583 to 1596 with it).
//
// The pages are the whole process's: what the tests before this one leave
// running (their connections' last tasks, a timer) may take a page while a
// round is counted. Each case is counted three times and the least kept: a
// cost of the record is in every round, a page of something else in one
// (under TSan after the whole suite: 2 pages in one round of 20 000
// records, 0 in a round of 60 000; 0 alone)
namespace {
    template<class F>
    size_t least_pages(size_t n, F&& f) {
        size_t least = SIZE_MAX;
        for (int i = 0; i < 3; ++i) {
            least = std::min(least, heap_count::pages_of(n, f));
        }
        return least;
    }
}

TEST(HttpAccessLog_Tests, NothingManagedPerRecord) {
    namespace hd = sgcl::net::http::detail;
    sgcl::async::detail::wait_for_idle_workers();
    tracked_ptr<hd::RequestImpl> req = make_tracked<hd::RequestImpl>();
    req->method = "GET";
    req->fields.add("User-Agent", "sgcl-test/1");
    req->fields.add("X-Request-ID", "abc-123");
    req->remote = net::endpoint(net::ip_address::v6({0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}), 52811);
    tracked_ptr<hd::WriterImpl> w = make_tracked<hd::WriterImpl>();
    w->status = 200;
    tracked_ptr<hd::ServerSettings> cfg = make_tracked<hd::ServerSettings>();
    const auto start = sgcl::clock::now();
    cfg->access_log.emplace(slog::logger(slog::options{.out = io::discard, .json = true}));
    size_t pages = least_pages(20000, [&] {
        hd::log_access(*cfg, *req, *w, 1234, "/users/42", "HTTP/1.1", start);
    });
    EXPECT_LE(pages, 1u) << "json";
    cfg->access_log.emplace(slog::logger(slog::options{.out = io::discard, .buffered = true}));
    pages = least_pages(20000, [&] {
        hd::log_access(*cfg, *req, *w, 1234, "/users/42", "HTTP/1.1", start);
    });
    cfg->access_log->flush();
    EXPECT_LE(pages, 1u) << "text, buffered";
    // the probe's control: a handler that keeps a copy of each record
    slog::memory kept;
    cfg->access_log.emplace(slog::logger(kept));
    pages = least_pages(20000, [&] {
        hd::log_access(*cfg, *req, *w, 1234, "/users/42", "HTTP/1.1", start);
    });
    EXPECT_GT(pages, 1u) << "the control";
}
