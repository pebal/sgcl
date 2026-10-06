//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The middlewares and the shape they share: server::use (around the
// routing, the first the outermost; a filter, an around of the program's, a
// middleware of the library), wrap() of one route, handler; cors by the
// Fetch standard; recovery (a plain and a task handler's exception, the
// program's answer, a response broken off after its head); request_log;
// body_limit (a Content-Length and a chunked body past it); sessions in a
// sealed cookie and in memory (values, renew, destroy, expiry, a key
// rotated, a cookie tampered with, the head of a flushed response); csrf
// (the origin check held against Go's http.CrossOriginProtection case by
// case, synchronizer and double-submit tokens in a field and in a form).
// Through a server on the loopback (HTTP/1.1 and h2c) and through a
// response recorder.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    namespace http = sgcl::net::http;

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    struct Running {
        http::server server;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        std::string base;

        explicit Running(http::server s)
        : server(s) {
            listener = *net::tcp::listen("127.0.0.1:0");
            base = "http://127.0.0.1:" + std::to_string(listener.local_endpoint().port());
            serving = async::spawn(server.async_serve(listener));
        }

        ~Running() {
            server.close();
            (void)serving.wait();
        }

        sgcl::string url(const std::string& path) const {
            return sgcl::string(base + path);
        }

        std::string host() const {
            return "127.0.0.1:" + std::to_string(listener.local_endpoint().port());
        }
    };

    using Fields = std::initializer_list<std::pair<const char*, std::string>>;

    expected<http::response, io::error> send(const http::client& c, const char* method, const sgcl::string& url, Fields fields = {},
                                             const char* body = nullptr) {
        http::request req(method, url);
        for (auto& f : fields) {
            req.headers().add(f.first, sgcl::string(f.second));
        }
        if (body) {
            req.set_body(sgcl::string(body));
        }
        return c.send(req);
    }

    // A request served by the server through a recorder: the fields given
    struct Recorded {
        int status = 0;
        http::headers fields;
        std::string body;
    };

    Recorded record(const http::server& s, const char* method, const char* target, Fields fields = {}, const char* body = nullptr) {
        auto req = http::test_request(method, target, body ? sgcl::string(body) : sgcl::string());
        for (auto& f : fields) {
            req.headers().add(f.first, sgcl::string(f.second));
        }
        http::response_recorder rec;
        rec.serve(s, req);
        return Recorded{rec.status(), rec.headers(), text(rec.body())};
    }

    std::string text_of(const slog::record& r, std::string_view key) {
        for (auto a : r) {
            if (std::string_view(a.key().data(), a.key().size()) == key) {
                auto t = a.value().text();
                return std::string(t.data(), t.size());
            }
        }
        return "<none>";
    }

    void wait_for(const slog::memory& kept, size_t n) {
        for (int i = 0; i < 2000 && kept.size() < n; ++i) {
            std::this_thread::sleep_for(1ms);
        }
    }

    // The value of a Set-Cookie of the name, "" when none; its attributes in `attrs`
    std::string set_cookie(const http::headers& h, const std::string& name, std::string* attrs = nullptr) {
        for (auto v : h.get_all("Set-Cookie")) {
            std::string s = text(v);
            if (s.rfind(name + "=", 0) == 0) {
                auto semi = s.find(';');
                if (attrs) {
                    *attrs = semi == std::string::npos ? "" : s.substr(semi);
                }
                return s.substr(name.size() + 1, semi == std::string::npos ? std::string::npos : semi - name.size() - 1);
            }
        }
        return "";
    }
}

// --- the shape -------------------------------------------------------------------------------

TEST(HttpMiddleware_Tests, UseRunsAroundTheRoutingInOrder) {
    tracked_ptr trace = make_tracked<std::vector<std::string>>();
    tracked_ptr lock = make_tracked<std::mutex>();
    auto note = [trace, lock](const std::string& s) {
        std::lock_guard<std::mutex> g(*lock);
        trace->push_back(s);
    };
    http::server s;
    s.use([note](http::request r, http::response_writer w, const http::handler& next) -> async::task<> {
        note("outer in");
        w.add_header("X-Seen", "outer");
        co_await next(r, w);
        note("outer out " + std::to_string(w.status()));
    });
    s.use([note](http::request& r, http::response_writer& w) {
        note("filter " + text(r.url().path()));
        if (r.header("X-Stop") == "1") {
            w.error(http::status::unauthorized);
            return false;
        }
        return true;
    });
    s.route("GET /plain", [note](http::request, http::response_writer w) {
        note("plain");
        w.write("plain");
    });
    s.route("POST /task", [note](http::request r, http::response_writer w) -> async::task<> {
        auto body = co_await r.async_text();
        note("task");
        w.write("got " + *body);
    });
    Running run(s);
    http::client c;
    auto plain = c.get(run.url("/plain"));
    ASSERT_TRUE(plain);
    EXPECT_EQ(*plain->text(), "plain");
    EXPECT_EQ(plain->header("X-Seen"), "outer");
    auto task = send(c, "POST", run.url("/task"), {}, "body");
    ASSERT_TRUE(task);
    EXPECT_EQ(*task->text(), "got body");
    // a request of no route goes through them: 404 and 405 carry the outer's field
    auto missing = c.get(run.url("/none"));
    ASSERT_TRUE(missing);
    EXPECT_EQ(missing->status(), 404);
    EXPECT_EQ(missing->header("X-Seen"), "outer");
    (void)missing->text();
    auto wrong = send(c, "DELETE", run.url("/plain"));
    ASSERT_TRUE(wrong);
    EXPECT_EQ(wrong->status(), 405);
    EXPECT_EQ(wrong->header("X-Seen"), "outer");
    (void)wrong->text();
    // a filter that stops: the handler never runs
    auto stopped = send(c, "GET", run.url("/plain"), {{"X-Stop", "1"}});
    ASSERT_TRUE(stopped);
    EXPECT_EQ(stopped->status(), 401);
    (void)stopped->text();
    std::lock_guard<std::mutex> g(*lock);
    const std::vector<std::string> expected = {"outer in", "filter /plain", "plain", "outer out 200", "outer in", "filter /task", "task",
                                               "outer out 200", "outer in", "filter /none", "outer out 404", "outer in", "filter /plain",
                                               "outer out 405", "outer in", "filter /plain", "outer out 401"};
    EXPECT_EQ(*trace, expected);
}

TEST(HttpMiddleware_Tests, WrapOneRouteAndTheHandlerType) {
    http::server s;
    http::handler hello = [](http::request, http::response_writer w) { w.write("hello"); };
    http::handler later = [](http::request r, http::response_writer w) -> async::task<> {
        auto t = co_await r.async_text();
        w.write("later " + *t);
    };
    auto mark = http::cors({.origins = {sgcl::string("https://a.example")}});
    s.route("GET /wrapped", mark.wrap(hello));
    s.route("POST /wrapped-task", mark.wrap(later));
    s.route("GET /bare", hello);
    s.route("GET /empty", http::handler());
    Running run(s);
    http::client c;
    auto wrapped = send(c, "GET", run.url("/wrapped"), {{"Origin", "https://a.example"}});
    ASSERT_TRUE(wrapped);
    EXPECT_EQ(*wrapped->text(), "hello");
    EXPECT_EQ(wrapped->header("Access-Control-Allow-Origin"), "https://a.example");
    auto task = send(c, "POST", run.url("/wrapped-task"), {{"Origin", "https://a.example"}}, "x");
    ASSERT_TRUE(task);
    EXPECT_EQ(*task->text(), "later x");
    EXPECT_EQ(task->header("Access-Control-Allow-Origin"), "https://a.example");
    auto bare = send(c, "GET", run.url("/bare"), {{"Origin", "https://a.example"}});
    ASSERT_TRUE(bare);
    EXPECT_EQ(*bare->text(), "hello");
    EXPECT_TRUE(bare->header("Access-Control-Allow-Origin").empty());   // the middleware is the other route's
    auto empty = c.get(run.url("/empty"));
    ASSERT_TRUE(empty);
    EXPECT_EQ(empty->status(), 404);   // a default handler
    (void)empty->text();
    // a handler called as a task, a copy the same function
    http::handler copy = hello;
    http::response_recorder rec;
    copy.task(http::test_request("GET", "/"), rec.writer()).wait();
    EXPECT_EQ(text(rec.body()), "hello");
    http::response_recorder none;
    http::handler().task(http::test_request("GET", "/"), none.writer()).wait();
    EXPECT_EQ(none.status(), 404);
}

TEST(HttpMiddleware_Tests, OverH2cAndThroughARecorder) {
    http::server s;
    s.h2c = true;
    s.use([](http::request&, http::response_writer& w) {
        w.set_header("X-Mw", "yes");
        return true;
    });
    s.route("GET /a", [](http::request r, http::response_writer w) { w.write(r.proto()); });
    Running run(s);
    http::client c;
    c.h2c = true;
    auto res = c.get(run.url("/a"));
    ASSERT_TRUE(res) << text(res.error().message());
    EXPECT_EQ(*res->text(), "HTTP/2.0");
    EXPECT_EQ(res->header("X-Mw"), "yes");
    auto rec = record(s, "GET", "/a");
    EXPECT_EQ(rec.body, "HTTP/1.1");
    EXPECT_EQ(text(rec.fields.get("X-Mw")), "yes");
}

// --- cors ------------------------------------------------------------------------------------

TEST(HttpCors_Tests, ActualRequests) {
    http::server s;
    s.use(http::cors({.origins = {sgcl::string("https://a.example"), sgcl::string("null")},
                      .exposed_headers = {sgcl::string("X-Total"), sgcl::string("X-Page")},
                      .credentials = true}));
    s.route("/r", [](http::request, http::response_writer w) { w.write("r"); });
    auto allowed = record(s, "GET", "/r", {{"Origin", "https://a.example"}});
    EXPECT_EQ(allowed.body, "r");
    EXPECT_EQ(text(allowed.fields.get("Access-Control-Allow-Origin")), "https://a.example");
    EXPECT_EQ(text(allowed.fields.get("Access-Control-Allow-Credentials")), "true");
    EXPECT_EQ(text(allowed.fields.get("Access-Control-Expose-Headers")), "X-Total, X-Page");
    EXPECT_EQ(text(allowed.fields.get("Vary")), "Origin");
    auto other = record(s, "GET", "/r", {{"Origin", "https://evil.example"}});
    EXPECT_EQ(other.body, "r");   // served: the browser keeps it from the script
    EXPECT_FALSE(other.fields.contains("Access-Control-Allow-Origin"));
    EXPECT_EQ(text(other.fields.get("Vary")), "Origin");
    auto none = record(s, "GET", "/r");
    EXPECT_FALSE(none.fields.contains("Access-Control-Allow-Origin"));
    auto opaque = record(s, "GET", "/r", {{"Origin", "null"}});
    EXPECT_EQ(text(opaque.fields.get("Access-Control-Allow-Origin")), "null");   // named, so allowed

    http::server any;
    any.use(http::cors());
    any.route("/r", [](http::request, http::response_writer w) { w.write("r"); });
    auto star = record(any, "GET", "/r", {{"Origin", "https://x.example"}});
    EXPECT_EQ(text(star.fields.get("Access-Control-Allow-Origin")), "*");
    EXPECT_FALSE(star.fields.contains("Vary"));   // the answer is the same for every origin
    EXPECT_FALSE(star.fields.contains("Access-Control-Allow-Credentials"));
    auto null_any = record(any, "GET", "/r", {{"Origin", "null"}});
    EXPECT_FALSE(null_any.fields.contains("Access-Control-Allow-Origin"));   // * is every origin but the opaque one

    http::server creds;
    creds.use(http::cors({.credentials = true}));
    creds.route("/r", [](http::request, http::response_writer w) { w.write("r"); });
    auto echoed = record(creds, "GET", "/r", {{"Origin", "https://x.example"}});
    EXPECT_EQ(text(echoed.fields.get("Access-Control-Allow-Origin")), "https://x.example");   // never * with credentials

    http::server pred;
    pred.use(http::cors({.allow_origin = [](const sgcl::string& o) { return o.view().ends_with(".corp.example"); }}));
    pred.route("/r", [](http::request, http::response_writer w) { w.write("r"); });
    EXPECT_EQ(text(record(pred, "GET", "/r", {{"Origin", "https://app.corp.example"}}).fields.get("Access-Control-Allow-Origin")),
              "https://app.corp.example");
    EXPECT_FALSE(record(pred, "GET", "/r", {{"Origin", "https://corp.example.evil"}}).fields.contains("Access-Control-Allow-Origin"));
}

TEST(HttpCors_Tests, Preflights) {
    http::server s;
    s.use(http::cors({.origins = {sgcl::string("https://a.example")}, .credentials = true, .max_age = 10min}));
    tracked_ptr ran = make_tracked<std::atomic<int>>(0);
    s.route("POST /items", [ran](http::request, http::response_writer w) {
        ++*ran;
        w.write("made");
    });
    Running run(s);
    http::client c;
    // a preflight of a POST route: answered by cors, not 405 by the router
    auto ok = send(c, "OPTIONS", run.url("/items"),
                   {{"Origin", "https://a.example"}, {"Access-Control-Request-Method", "PUT"}, {"Access-Control-Request-Headers", "x-token, content-type"}});
    ASSERT_TRUE(ok);
    EXPECT_EQ(ok->status(), 204);
    EXPECT_EQ(ok->header("Access-Control-Allow-Origin"), "https://a.example");
    EXPECT_EQ(ok->header("Access-Control-Allow-Credentials"), "true");
    EXPECT_EQ(ok->header("Access-Control-Allow-Methods"), "GET, HEAD, PUT, PATCH, POST, DELETE");
    EXPECT_EQ(ok->header("Access-Control-Allow-Headers"), "x-token, content-type");   // what it asked, none configured
    EXPECT_EQ(ok->header("Access-Control-Max-Age"), "600");
    EXPECT_EQ(ok->header("Vary"), "Origin, Access-Control-Request-Method, Access-Control-Request-Headers");
    EXPECT_EQ(*ok->text(), "");
    EXPECT_EQ(ran->load(), 0);
    // refused: another origin, a method not allowed; 204 with no permissions
    for (auto [origin, method] : {std::pair<const char*, const char*>{"https://evil.example", "PUT"}, {"https://a.example", "PURGE"}}) {
        auto no = send(c, "OPTIONS", run.url("/items"), {{"Origin", origin}, {"Access-Control-Request-Method", method}});
        ASSERT_TRUE(no);
        EXPECT_EQ(no->status(), 204);
        EXPECT_TRUE(no->header("Access-Control-Allow-Origin").empty()) << origin << " " << method;
        EXPECT_TRUE(no->header("Access-Control-Allow-Methods").empty());
        (void)no->text();
    }
    // OPTIONS without the request method is no preflight: the router's
    auto plain = send(c, "OPTIONS", run.url("/items"), {{"Origin", "https://a.example"}});
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->status(), 405);
    (void)plain->text();
    // the actual request after it
    auto made = send(c, "POST", run.url("/items"), {{"Origin", "https://a.example"}});
    ASSERT_TRUE(made);
    EXPECT_EQ(*made->text(), "made");
    EXPECT_EQ(ran->load(), 1);

    // headers configured: one not among them refused, a safelisted one allowed
    http::server h;
    h.use(http::cors({.headers = {sgcl::string("X-Token")}, .max_age = duration::zero()}));
    h.route("/x", [](http::request, http::response_writer w) { w.write("x"); });
    auto asked = record(h, "OPTIONS", "/x", {{"Origin", "https://b.example"}, {"Access-Control-Request-Method", "POST"},
                                             {"Access-Control-Request-Headers", "x-token,accept"}});
    EXPECT_EQ(asked.status, 204);
    EXPECT_EQ(text(asked.fields.get("Access-Control-Allow-Origin")), "*");
    EXPECT_EQ(text(asked.fields.get("Access-Control-Allow-Headers")), "X-Token");
    EXPECT_FALSE(asked.fields.contains("Access-Control-Max-Age"));   // zero: none
    auto refused = record(h, "OPTIONS", "/x", {{"Origin", "https://b.example"}, {"Access-Control-Request-Method", "POST"},
                                               {"Access-Control-Request-Headers", "x-other"}});
    EXPECT_EQ(refused.status, 204);
    EXPECT_FALSE(refused.fields.contains("Access-Control-Allow-Origin"));
}

// --- recovery --------------------------------------------------------------------------------

TEST(HttpRecovery_Tests, ThrownAndAnswered) {
    slog::memory kept;
    http::server s;
    s.on_error = [](const sgcl::string&) {};
    s.use(http::recovery({.log = slog::logger(kept)}));
    s.route("GET /plain", [](http::request, http::response_writer w) {
        w.set_header("X-Partial", "1");
        throw std::runtime_error("plain broke");
    });
    s.route("GET /task", [](http::request r, http::response_writer) -> async::task<> {
        (void)co_await r.async_text();
        throw std::runtime_error("task broke");
    });
    s.route("GET /odd", [](http::request, http::response_writer) { throw 7; });
    s.route("GET /flushed", [](http::request, http::response_writer w) -> async::task<> {
        w.write("half of it");
        (void)co_await w.async_flush();
        throw std::runtime_error("after the head");
    });
    Running run(s);
    http::client c;
    auto plain = c.get(run.url("/plain"));
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->status(), 500);
    EXPECT_TRUE(plain->header("X-Partial").empty());   // the handler's fields dropped
    EXPECT_EQ(*plain->text(), "Internal Server Error\n");
    auto task = c.get(run.url("/task"));
    ASSERT_TRUE(task);
    EXPECT_EQ(task->status(), 500);
    (void)task->text();
    auto odd = c.get(run.url("/odd"));
    ASSERT_TRUE(odd);
    EXPECT_EQ(odd->status(), 500);
    (void)odd->text();
    auto flushed = c.get(run.url("/flushed"));
    ASSERT_TRUE(flushed);
    EXPECT_EQ(flushed->status(), 200);
    auto rest = flushed->text();
    EXPECT_FALSE(rest);   // broken off: never taken for a whole body
    wait_for(kept, 4);
    auto records = kept.records();
    ASSERT_EQ(records.size(), 4u);
    EXPECT_EQ(records[0].level(), slog::level::error);
    EXPECT_EQ(text_of(records[0], "path"), "/plain");
    EXPECT_EQ(text_of(records[0], "error"), "plain broke");
    EXPECT_EQ(text_of(records[1], "error"), "task broke");
    EXPECT_EQ(text_of(records[2], "error"), "an exception not of std::exception");
    EXPECT_EQ(text_of(records[3], "error"), "after the head");

    http::server own;
    own.use(http::recovery({.log = slog::logger(kept), .answer = [](const http::request&, http::response_writer& w, const sgcl::string& what) {
                                w.set_status(503);
                                w.write("sorry: " + what);
                            }}));
    own.route("GET /x", [](http::request, http::response_writer) { throw std::runtime_error("boom"); });
    auto answered = record(own, "GET", "/x");
    EXPECT_EQ(answered.status, 503);
    EXPECT_EQ(answered.body, "sorry: boom");
}

// --- request_log -----------------------------------------------------------------------------

TEST(HttpRequestLog_Tests, ARecordOnceTheResponseHasGone) {
    slog::memory kept;
    http::server s;
    s.route("GET /logged", http::request_log(slog::logger(kept)).wrap([](http::request, http::response_writer w) { w.write("12345"); }));
    s.route("GET /fail", http::request_log(slog::logger(kept)).wrap([](http::request, http::response_writer w) { w.error(503); }));
    s.route("GET /quiet", [](http::request, http::response_writer w) { w.write("q"); });
    Running run(s);
    http::client c;
    auto logged = send(c, "GET", run.url("/logged?x=1"), {{"X-Request-ID", "id-7"}, {"User-Agent", "t/1"}});
    ASSERT_TRUE(logged);
    EXPECT_EQ(*logged->text(), "12345");
    wait_for(kept, 1);
    auto quiet = c.get(run.url("/quiet"));
    ASSERT_TRUE(quiet);
    (void)quiet->text();
    auto fail = c.get(run.url("/fail"));
    ASSERT_TRUE(fail);
    (void)fail->text();
    wait_for(kept, 2);
    auto records = kept.records();
    ASSERT_EQ(records.size(), 2u);   // the quiet route has none
    EXPECT_EQ(records[0].level(), slog::level::info);
    EXPECT_EQ(text_of(records[0], "method"), "GET");
    EXPECT_EQ(text_of(records[0], "path"), "/logged");
    EXPECT_EQ(text_of(records[0], "proto"), "HTTP/1.1");
    EXPECT_EQ(text_of(records[0], "status"), "200");
    EXPECT_EQ(text_of(records[0], "bytes"), "5");
    EXPECT_EQ(text_of(records[0], "request_id"), "id-7");
    EXPECT_EQ(text_of(records[0], "user_agent"), "t/1");
    EXPECT_NE(text_of(records[0], "duration"), "<none>");
    EXPECT_EQ(records[1].level(), slog::level::error);
    EXPECT_EQ(text_of(records[1], "status"), "503");
}

// --- body_limit ------------------------------------------------------------------------------

TEST(HttpBodyLimit_Tests, LengthAndChunks) {
    http::server s;
    tracked_ptr ran = make_tracked<std::atomic<int>>(0);
    s.route("POST /up", http::body_limit(10).wrap([ran](http::request r, http::response_writer w) -> async::task<> {
        ++*ran;
        auto t = co_await r.async_text();
        if (t) {
            w.write("got " + *t);
        }
    }));
    s.route("POST /free", [](http::request r, http::response_writer w) -> async::task<> {
        auto t = co_await r.async_text();
        w.write(sgcl::string(std::to_string(t->size())));
    });
    Running run(s);
    http::client c;
    auto small = send(c, "POST", run.url("/up"), {}, "0123456789");
    ASSERT_TRUE(small);
    EXPECT_EQ(*small->text(), "got 0123456789");
    auto big = send(c, "POST", run.url("/up"), {}, "0123456789A");
    ASSERT_TRUE(big);
    EXPECT_EQ(big->status(), 413);
    EXPECT_EQ(ran->load(), 1);   // refused before the handler
    (void)big->text();
    http::request chunked("POST", run.url("/up"));
    chunked.set_body(io::reader(make_tracked<io::buffer>(sgcl::string("0123456789ABCDEF"))));   // no length: chunked
    auto streamed = c.send(chunked);
    ASSERT_TRUE(streamed);
    EXPECT_EQ(streamed->status(), 413);
    (void)streamed->text();
    auto free = send(c, "POST", run.url("/free"), {}, "0123456789ABCDEF");
    ASSERT_TRUE(free);
    EXPECT_EQ(*free->text(), "16");   // the other route keeps the server's limit
}

// --- sessions --------------------------------------------------------------------------------

namespace {
    http::server session_routes(http::sessions store) {
        http::server s;
        s.use(store);
        s.route("POST /login", [](http::request r, http::response_writer w) {
            http::session ses(r);
            ses.renew();
            ses.set("user", r.query("name"));
            w.write("in");
        });
        s.route("GET /me", [](http::request r, http::response_writer w) {
            http::session ses(r);
            w.write(ses.get("user") + (ses.is_new() ? " new" : " old"));
        });
        s.route("POST /logout", [](http::request r, http::response_writer w) {
            http::session(r).destroy();
            w.write("out");
        });
        s.route("GET /count", [](http::request r, http::response_writer w) {
            http::session ses(r);
            auto n = ses.get("n");
            ses.set("n", sgcl::string(std::to_string(n.empty() ? 1 : std::atoi(n.c_str()) + 1)));
            w.write(ses.get("n"));
        });
        s.route("GET /stream", [](http::request r, http::response_writer w) -> async::task<> {
            http::session(r).set("streamed", "yes");
            w.write("a");
            (void)co_await w.async_flush();
            w.write("b");
        });
        return s;
    }
}

TEST(HttpSessions_Tests, InACookie) {
    auto key = crypto::random::secret(32);
    auto s = session_routes(http::sessions::in_cookie(key));
    auto anonymous = record(s, "GET", "/me");
    EXPECT_EQ(anonymous.body, " new");
    EXPECT_FALSE(anonymous.fields.contains("Set-Cookie"));   // nothing to keep: no cookie
    auto login = record(s, "POST", "/login?name=ann");
    std::string attrs;
    const std::string sealed = set_cookie(login.fields, "__Host-session", &attrs);
    ASSERT_FALSE(sealed.empty());
    EXPECT_NE(sealed.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"), 0u);
    EXPECT_NE(attrs.find("Path=/"), std::string::npos) << attrs;
    EXPECT_NE(attrs.find("Secure"), std::string::npos);
    EXPECT_NE(attrs.find("HttpOnly"), std::string::npos);
    EXPECT_NE(attrs.find("SameSite=Lax"), std::string::npos);
    EXPECT_NE(attrs.find("Max-Age=86400"), std::string::npos) << attrs;
    EXPECT_EQ(sealed.find("ann"), std::string::npos);   // sealed: not readable
    auto me = record(s, "GET", "/me", {{"Cookie", "__Host-session=" + sealed}});
    EXPECT_EQ(me.body, "ann old");
    EXPECT_FALSE(me.fields.contains("Set-Cookie"));   // unchanged: no cookie again
    // tampered with, or sealed under another key: a new session
    std::string bent = sealed;
    bent[bent.size() / 2] = bent[bent.size() / 2] == 'A' ? 'B' : 'A';
    EXPECT_EQ(record(s, "GET", "/me", {{"Cookie", "__Host-session=" + bent}}).body, " new");
    auto other = session_routes(http::sessions::in_cookie(crypto::random::secret(32)));
    EXPECT_EQ(record(other, "GET", "/me", {{"Cookie", "__Host-session=" + sealed}}).body, " new");
    // a key rotated: the previous one still opens
    auto rotated = session_routes(http::sessions::in_cookie(crypto::random::secret(32), key, {}));
    EXPECT_EQ(record(rotated, "GET", "/me", {{"Cookie", "__Host-session=" + sealed}}).body, "ann old");
    // values that change from request to request
    std::string jar;
    for (int i : range(3)) {
        auto n = record(s, "GET", "/count", jar.empty() ? Fields{} : Fields{{"Cookie", "__Host-session=" + jar}});
        EXPECT_EQ(n.body, std::to_string(i + 1));
        jar = set_cookie(n.fields, "__Host-session");
    }
    // destroyed: the cookie expired
    auto out = record(s, "POST", "/logout", {{"Cookie", "__Host-session=" + sealed}});
    std::string out_attrs;
    EXPECT_EQ(set_cookie(out.fields, "__Host-session", &out_attrs), "");
    EXPECT_NE(out_attrs.find("Max-Age=0"), std::string::npos) << out_attrs;
    // saved with the head of a flushed response
    Running run(s);
    http::client c;
    auto streamed = c.get(run.url("/stream"));
    ASSERT_TRUE(streamed);
    EXPECT_EQ(*streamed->text(), "ab");
    const std::string after = set_cookie(streamed->headers(), "__Host-session");
    ASSERT_FALSE(after.empty());
    auto read_back = send(c, "GET", run.url("/me"), {{"Cookie", "__Host-session=" + after}});
    ASSERT_TRUE(read_back);
    EXPECT_EQ(*read_back->text(), " old");
}

TEST(HttpSessions_Tests, InMemory) {
    auto store = http::sessions::in_memory();
    auto s = session_routes(store);
    EXPECT_EQ(store.size(), 0u);
    auto login = record(s, "POST", "/login?name=bob");
    const std::string id = set_cookie(login.fields, "__Host-session");
    ASSERT_EQ(id.size(), 22u);   // 128 bits, base64url
    EXPECT_EQ(store.size(), 1u);
    EXPECT_EQ(record(s, "GET", "/me", {{"Cookie", "__Host-session=" + id}}).body, "bob old");
    // renewed: a new id, the old one worth nothing
    auto again = record(s, "POST", "/login?name=bob", {{"Cookie", "__Host-session=" + id}});
    const std::string id2 = set_cookie(again.fields, "__Host-session");
    ASSERT_FALSE(id2.empty());
    EXPECT_NE(id2, id);
    EXPECT_EQ(record(s, "GET", "/me", {{"Cookie", "__Host-session=" + id}}).body, " new");
    EXPECT_EQ(record(s, "GET", "/me", {{"Cookie", "__Host-session=" + id2}}).body, "bob old");
    EXPECT_EQ(store.size(), 1u);
    // a value changed keeps the id: no new cookie
    auto counted = record(s, "GET", "/count", {{"Cookie", "__Host-session=" + id2}});
    EXPECT_EQ(counted.body, "1");
    EXPECT_FALSE(counted.fields.contains("Set-Cookie"));
    EXPECT_EQ(record(s, "GET", "/count", {{"Cookie", "__Host-session=" + id2}}).body, "2");
    // an id never issued: a new session
    EXPECT_EQ(record(s, "GET", "/me", {{"Cookie", "__Host-session=AAAAAAAAAAAAAAAAAAAAAA"}}).body, " new");
    auto out = record(s, "POST", "/logout", {{"Cookie", "__Host-session=" + id2}});
    EXPECT_EQ(out.body, "out");
    EXPECT_EQ(store.size(), 0u);
    EXPECT_EQ(record(s, "GET", "/me", {{"Cookie", "__Host-session=" + id2}}).body, " new");
}

TEST(HttpSessions_Tests, TheirTime) {
    async::manual_clock clock;
    clock.install();
    auto store = http::sessions::in_memory({.max_age = 1h, .idle_timeout = 10min});
    auto s = session_routes(store);
    const std::string id = set_cookie(record(s, "POST", "/login?name=cy").fields, "__Host-session");
    clock.advance(9min);
    EXPECT_EQ(record(s, "GET", "/me", {{"Cookie", "__Host-session=" + id}}).body, "cy old");   // used: its idle time from now
    clock.advance(9min);
    EXPECT_EQ(record(s, "GET", "/me", {{"Cookie", "__Host-session=" + id}}).body, "cy old");
    clock.advance(11min);
    EXPECT_EQ(record(s, "GET", "/me", {{"Cookie", "__Host-session=" + id}}).body, " new");   // idle too long
    const std::string id2 = set_cookie(record(s, "POST", "/login?name=dee").fields, "__Host-session");
    for (int i : range(7)) {
        (void)i;
        clock.advance(9min);
        (void)record(s, "GET", "/me", {{"Cookie", "__Host-session=" + id2}});
    }
    EXPECT_EQ(record(s, "GET", "/me", {{"Cookie", "__Host-session=" + id2}}).body, " new");   // past max_age, used or not

    auto sealed_store = http::sessions::in_cookie(crypto::random::secret(32), {.max_age = 1h, .idle_timeout = 10min});
    auto c = session_routes(sealed_store);
    std::string cookie = set_cookie(record(c, "POST", "/login?name=eve").fields, "__Host-session");
    clock.advance(9min);
    auto used = record(c, "GET", "/me", {{"Cookie", "__Host-session=" + cookie}});
    EXPECT_EQ(used.body, "eve old");
    std::string attrs;
    const std::string refreshed = set_cookie(used.fields, "__Host-session", &attrs);
    ASSERT_FALSE(refreshed.empty());   // the last use is inside: sealed anew
    EXPECT_NE(attrs.find("Max-Age=3060"), std::string::npos) << attrs;   // what is left of the hour
    clock.advance(11min);
    EXPECT_EQ(record(c, "GET", "/me", {{"Cookie", "__Host-session=" + cookie}}).body, " new");   // the old cookie: idle too long
    EXPECT_EQ(record(c, "GET", "/me", {{"Cookie", "__Host-session=" + refreshed}}).body, " new");
    clock.uninstall();
}

// Many clients at once on one store: each its own session, none of another's
TEST(HttpSessions_Tests, ManyAtOnce) {
    for (bool sealed : {false, true}) {
        auto store = sealed ? http::sessions::in_cookie(crypto::random::secret(32)) : http::sessions::in_memory();
        Running run(session_routes(store));
        http::client c;
        c.max_idle_per_host = 64;
        auto one = [&run, c](int k) -> async::task<int> {
            auto login = co_await c.async_send(http::request("POST", run.url("/login?name=u" + std::to_string(k))));
            if (!login) {
                co_return -1;
            }
            (void)co_await login->async_text();
            const std::string cookie = "__Host-session=" + set_cookie(login->headers(), "__Host-session");
            std::string last;
            std::string jar = cookie;
            for (int i : range(5)) {
                (void)i;
                http::request req("GET", run.url("/count"));
                req.set_header("Cookie", sgcl::string(jar));
                auto n = co_await c.async_send(req);
                if (!n) {
                    co_return -2;
                }
                last = text(*co_await n->async_text());
                if (auto fresh = set_cookie(n->headers(), "__Host-session"); !fresh.empty()) {
                    jar = "__Host-session=" + fresh;   // the sealed store's cookie changes with its values
                }
            }
            http::request me("GET", run.url("/me"));
            me.set_header("Cookie", sgcl::string(jar));
            auto who = co_await c.async_send(me);
            if (!who) {
                co_return -3;
            }
            const std::string name = text(*co_await who->async_text());
            co_return last == "5" && name == "u" + std::to_string(k) + " old" ? 1 : 0;
        };
        vector<async::task<int>> all;
        for (int k : range(32)) {
            all.push_back(async::spawn(one(k)));
        }
        int good = 0;
        for (auto& t : all) {
            good += t.wait() == 1;
        }
        EXPECT_EQ(good, 32) << (sealed ? "in a cookie" : "in memory");
        if (!sealed) {
            EXPECT_EQ(store.size(), 32u);
        }
    }
}

// DESIGN 408: options a browser refuses, keys of the wrong length, a
// session asked for without the middleware, values too large for a cookie
TEST(HttpSessions_Tests, Boundaries) {
    auto key = crypto::random::secret(32);
    EXPECT_THROW((void)http::sessions::in_cookie(crypto::random::secret(16)), std::invalid_argument);
    EXPECT_THROW((void)http::sessions::in_memory({.domain = "example.com"}), std::invalid_argument);   // __Host- with a domain
    EXPECT_THROW((void)http::sessions::in_memory({.secure = false}), std::invalid_argument);
    EXPECT_THROW((void)http::sessions::in_memory({.cookie = "__Secure-s", .secure = false}), std::invalid_argument);
    EXPECT_THROW((void)http::sessions::in_memory({.cookie = "s", .secure = false, .same_site = "None"}), std::invalid_argument);
    EXPECT_THROW((void)http::sessions::in_memory({.cookie = "bad name"}), std::invalid_argument);
    EXPECT_NO_THROW((void)http::sessions::in_memory({.cookie = "sid", .domain = "example.com", .secure = false}));
    EXPECT_THROW(http::session(http::test_request("GET", "/")), std::invalid_argument);
    http::server s;
    s.use(http::sessions::in_cookie(key, {.cookie = "s", .max_age = duration::zero()}));
    s.route("GET /big", [](http::request r, http::response_writer w) {
        http::session(r).set("blob", sgcl::string(std::string(5000, 'x')));
        w.write("big");
    });
    s.route("GET /small", [](http::request r, http::response_writer w) {
        http::session ses(r);
        ses.set("a", "1");
        ses.set("a", "2");
        ses.erase("none");
        w.write(ses.get("a") + (ses.contains("a") ? "+" : "-") + (ses.id().empty() ? " noid" : " id"));
    });
    auto big = record(s, "GET", "/big");
    EXPECT_EQ(big.body, "big");
    EXPECT_FALSE(big.fields.contains("Set-Cookie"));   // too large to set: logged, not sent
    auto small = record(s, "GET", "/small");
    EXPECT_EQ(small.body, "2+ noid");
    std::string attrs;
    EXPECT_FALSE(set_cookie(small.fields, "s", &attrs).empty());
    EXPECT_EQ(attrs.find("Max-Age"), std::string::npos) << attrs;   // max_age zero: the browser's session
}

// --- csrf ------------------------------------------------------------------------------------

namespace {
    const std::string& go_csrf_peer() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto src = source_root() / "tests/net/http/go_csrf/main.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_http_go_csrf";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }
}

TEST(HttpCsrf_Tests, TheOriginCheckAsGo) {
    if (go_csrf_peer().empty()) {
        GTEST_SKIP() << "no go to build the peer with";
    }
    FILE* p = popen(("'" + go_csrf_peer() + "'").c_str(), "r");
    ASSERT_TRUE(p);
    char line[64] = {};
    ASSERT_TRUE(fgets(line, sizeof(line), p));
    const int port = std::atoi(line + 5);
    ASSERT_GT(port, 0);
    const std::string go = "http://127.0.0.1:" + std::to_string(port);
    http::server s;
    s.use(http::csrf({.trusted_origins = {sgcl::string("https://trusted.example")}}));
    s.route("/", [](http::request, http::response_writer w) { w.write("ok"); });
    Running ours(s);
    struct Case {
        const char* method;
        std::vector<std::pair<std::string, std::string>> fields;   // "{host}" replaced by the server's host
    };
    const std::vector<Case> cases = {
        {"GET", {{"Sec-Fetch-Site", "cross-site"}}},
        {"HEAD", {{"Sec-Fetch-Site", "cross-site"}}},
        {"OPTIONS", {{"Sec-Fetch-Site", "cross-site"}}},
        {"POST", {}},
        {"POST", {{"Sec-Fetch-Site", "same-origin"}}},
        {"POST", {{"Sec-Fetch-Site", "none"}}},
        {"POST", {{"Sec-Fetch-Site", "same-site"}}},
        {"POST", {{"Sec-Fetch-Site", "cross-site"}}},
        {"POST", {{"Sec-Fetch-Site", "cross-site"}, {"Origin", "https://trusted.example"}}},
        {"POST", {{"Sec-Fetch-Site", "cross-site"}, {"Origin", "https://evil.example"}}},
        {"POST", {{"Sec-Fetch-Site", "same-origin"}, {"Origin", "https://evil.example"}}},
        {"POST", {{"Origin", "https://evil.example"}}},
        {"POST", {{"Origin", "https://trusted.example"}}},
        {"POST", {{"Origin", "http://{host}"}}},
        {"POST", {{"Origin", "https://{host}"}}},
        {"POST", {{"Origin", "null"}}},
        {"PUT", {{"Sec-Fetch-Site", "cross-site"}}},
        {"DELETE", {{"Origin", "https://evil.example"}}},
        {"PATCH", {{"Sec-Fetch-Site", "same-origin"}}},
    };
    http::client c;
    for (auto& k : cases) {
        auto one = [&](const std::string& base, const std::string& host) {
            http::request req(k.method, sgcl::string(base + "/"));
            std::string what = std::string(k.method);
            for (auto& f : k.fields) {
                std::string v = f.second;
                if (auto at = v.find("{host}"); at != std::string::npos) {
                    v.replace(at, 6, host);
                }
                req.headers().add(sgcl::string(f.first), sgcl::string(v));
            }
            auto res = c.send(req);
            EXPECT_TRUE(res);
            int st = res ? res->status() : 0;
            if (res) {
                (void)res->text();
            }
            return st;
        };
        const int theirs = one(go, "127.0.0.1:" + std::to_string(port));
        const int mine = one(ours.base, ours.host());
        std::string what = k.method;
        for (auto& f : k.fields) {
            what += " " + f.first + ": " + f.second;
        }
        EXPECT_EQ(mine, theirs) << what;
    }
    (void)c.get(sgcl::string(go + "/quit"));
    pclose(p);
}

TEST(HttpCsrf_Tests, SynchronizerTokens) {
    http::server s;
    s.use(http::sessions::in_memory());
    s.use(http::csrf({.kind = http::csrf::tokens::synchronizer}));
    s.route("GET /form", [](http::request r, http::response_writer w) { w.write(http::csrf::token(r)); });
    s.route("POST /act", [](http::request r, http::response_writer w) -> async::task<> {
        auto f = co_await r.async_form();
        w.write("done " + (f ? f->get("item") : sgcl::string("?")));
    });
    auto page = record(s, "GET", "/form");
    const std::string token = page.body;
    EXPECT_EQ(token.size(), 43u);   // 256 bits, base64url
    const std::string id = set_cookie(page.fields, "__Host-session");
    ASSERT_FALSE(id.empty());   // the token lives in the session
    const std::string cookie = "__Host-session=" + id;
    EXPECT_EQ(record(s, "GET", "/form", {{"Cookie", cookie}}).body, token);   // the same while the session lasts
    EXPECT_EQ(record(s, "POST", "/act", {{"Cookie", cookie}, {"X-CSRF-Token", token}}).body, "done ");   // no form: no fields
    EXPECT_EQ(record(s, "POST", "/act", {{"Cookie", cookie}, {"X-CSRF-Token", token + "x"}}).status, 403);
    EXPECT_EQ(record(s, "POST", "/act", {{"Cookie", cookie}}).status, 403);
    EXPECT_EQ(record(s, "POST", "/act", {{"X-CSRF-Token", token}}).status, 403);   // another session: no token of its
    // in a urlencoded form's field: the body read for it, form() gives the fields after
    auto by_form = record(s, "POST", "/act", {{"Cookie", cookie}, {"Content-Type", "application/x-www-form-urlencoded"}},
                          ("item=book&csrf_token=" + token).c_str());
    EXPECT_EQ(by_form.status, 200);
    EXPECT_EQ(by_form.body, "done book");
    auto bad_form = record(s, "POST", "/act", {{"Cookie", cookie}, {"Content-Type", "application/x-www-form-urlencoded"}},
                           "item=book&csrf_token=nope");
    EXPECT_EQ(bad_form.status, 403);
    // the origin check comes first
    EXPECT_EQ(record(s, "POST", "/act", {{"Cookie", cookie}, {"X-CSRF-Token", token}, {"Sec-Fetch-Site", "cross-site"}}).status, 403);
    // without a sessions middleware: the program's mistake, a 500
    http::server bare;
    bare.on_error = [](const sgcl::string&) {};
    bare.use(http::csrf({.kind = http::csrf::tokens::synchronizer}));
    bare.route("GET /x", [](http::request, http::response_writer w) { w.write("x"); });
    Running run(bare);
    http::client c;
    auto broken = c.get(run.url("/x"));
    ASSERT_TRUE(broken);
    EXPECT_EQ(broken->status(), 500);
    (void)broken->text();
}

TEST(HttpCsrf_Tests, DoubleSubmitTokens) {
    auto key = crypto::random::secret(32);
    http::server s;
    s.use(http::csrf(key, {.kind = http::csrf::tokens::double_submit, .deny = [](const http::request&, http::response_writer& w) {
                               w.set_status(400);
                               w.write("refused");
                           }}));
    s.route("GET /form", [](http::request r, http::response_writer w) { w.write(http::csrf::token(r)); });
    s.route("POST /act", [](http::request, http::response_writer w) { w.write("done"); });
    auto page = record(s, "GET", "/form");
    const std::string token = page.body;
    std::string attrs;
    EXPECT_EQ(set_cookie(page.fields, "__Host-csrf", &attrs), token);
    EXPECT_NE(attrs.find("Secure"), std::string::npos);
    EXPECT_NE(attrs.find("HttpOnly"), std::string::npos);
    const std::string cookie = "__Host-csrf=" + token;
    EXPECT_EQ(record(s, "GET", "/form", {{"Cookie", cookie}}).body, token);   // the cookie's token again, no new cookie
    EXPECT_FALSE(record(s, "GET", "/form", {{"Cookie", cookie}}).fields.contains("Set-Cookie"));
    EXPECT_EQ(record(s, "POST", "/act", {{"Cookie", cookie}, {"X-CSRF-Token", token}}).body, "done");
    auto mismatch = record(s, "POST", "/act", {{"Cookie", cookie}, {"X-CSRF-Token", "other"}});
    EXPECT_EQ(mismatch.status, 400);
    EXPECT_EQ(mismatch.body, "refused");
    // a cookie not signed by the key: refused even when the field matches it
    const std::string forged = "abc.def";
    EXPECT_EQ(record(s, "POST", "/act", {{"Cookie", "__Host-csrf=" + forged}, {"X-CSRF-Token", forged}}).status, 400);
    // another csrf of the same key takes the cookie (two processes behind one name)
    http::server twin;
    twin.use(http::csrf(key, {.kind = http::csrf::tokens::double_submit}));
    twin.route("POST /act", [](http::request, http::response_writer w) { w.write("done"); });
    EXPECT_EQ(record(twin, "POST", "/act", {{"Cookie", cookie}, {"X-CSRF-Token", token}}).body, "done");
    http::server stranger;
    stranger.use(http::csrf({.kind = http::csrf::tokens::double_submit}));
    stranger.route("POST /act", [](http::request, http::response_writer w) { w.write("done"); });
    EXPECT_EQ(record(stranger, "POST", "/act", {{"Cookie", cookie}, {"X-CSRF-Token", token}}).status, 403);
    // no tokens: token() is ""
    http::server plain;
    plain.use(http::csrf());
    plain.route("GET /t", [](http::request r, http::response_writer w) { w.write("[" + http::csrf::token(r) + "]"); });
    EXPECT_EQ(record(plain, "GET", "/t").body, "[]");
    EXPECT_EQ(http::csrf::token(http::test_request("GET", "/")), "");   // no middleware ran
}

// --- rate_limit ------------------------------------------------------------------------------

TEST(HttpRateLimit_Tests, PerKey) {
    async::manual_clock clock;
    clock.install();
    auto limit = http::rate_limit(1, 2);   // a token a second, two at once
    http::server s;
    s.use(limit);
    s.route("/x", [](http::request, http::response_writer w) { w.write("ok"); });
    auto from = [&](const char* ip) {
        auto req = http::test_request("GET", "/x");
        http::detail::RequestAccess::impl(req)->remote = net::endpoint::parse(sgcl::string(std::string(ip) + ":1000")).value();
        http::response_recorder rec;
        rec.serve(s, req);
        return rec;
    };
    EXPECT_EQ(from("10.0.0.1").status(), 200);
    EXPECT_EQ(from("10.0.0.1").status(), 200);
    auto refused = from("10.0.0.1");
    EXPECT_EQ(refused.status(), 429);
    EXPECT_EQ(text(refused.header("Retry-After")), "1");
    EXPECT_EQ(from("10.0.0.2").status(), 200);   // another key, another bucket
    EXPECT_EQ(limit.keys(), 2u);
    clock.advance(std::chrono::seconds(1));
    EXPECT_EQ(from("10.0.0.1").status(), 200);   // a token again
    EXPECT_EQ(from("10.0.0.1").status(), 429);
    // idle buckets dropped
    clock.advance(std::chrono::minutes(11));
    EXPECT_EQ(from("10.0.0.3").status(), 200);
    EXPECT_EQ(limit.keys(), 1u);
    clock.uninstall();
}

TEST(HttpRateLimit_Tests, KeysOfAFieldOrTheProgram) {
    http::server s;
    s.route("/h", http::rate_limit(0, 1, {.header = "X-API-Key"}).wrap([](http::request, http::response_writer w) { w.write("h"); }));
    s.route("/k", http::rate_limit(0, 1, {.key = [](const http::request& r) { return r.query("user"); }})
                      .wrap([](http::request, http::response_writer w) { w.write("k"); }));
    auto ask = [&](const char* target, const char* key) {
        auto req = http::test_request("GET", target);
        if (key) {
            req.set_header("X-API-Key", key);
        }
        http::response_recorder rec;
        rec.serve(s, req);
        return rec.status();
    };
    EXPECT_EQ(ask("/h", "a"), 200);
    EXPECT_EQ(ask("/h", "a"), 429);   // rate 0: the burst once, never again
    EXPECT_EQ(ask("/h", "b"), 200);
    EXPECT_EQ(ask("/h", nullptr), 200);   // no field: the key ""
    EXPECT_EQ(ask("/h", nullptr), 429);
    EXPECT_EQ(ask("/k?user=ann", nullptr), 200);
    EXPECT_EQ(ask("/k?user=ann", nullptr), 429);
    EXPECT_EQ(ask("/k?user=bob", nullptr), 200);
    // at most max_keys buckets
    auto few = http::rate_limit(100, 1, {.header = "X-API-Key", .max_keys = 3});
    http::server f;
    f.use(few);
    f.route("/", [](http::request, http::response_writer w) { w.write("f"); });
    for (int i : range(10)) {
        auto req = http::test_request("GET", "/");
        req.set_header("X-API-Key", sgcl::string(std::to_string(i)));
        http::response_recorder rec;
        rec.serve(f, req);
        EXPECT_EQ(rec.status(), 200);
    }
    EXPECT_LE(few.keys(), 3u);
    EXPECT_THROW(http::rate_limit(1, 0), std::invalid_argument);
    EXPECT_THROW(http::rate_limit(-1, 1), std::invalid_argument);
    EXPECT_THROW(http::rate_limit(std::nan(""), 1), std::invalid_argument);
}
