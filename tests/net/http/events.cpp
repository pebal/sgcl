//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: Server-Sent Events (WHATWG HTML §9.2). The stream's interpretation
// on the standard's own examples and every rule of §9.2.6 (the three line
// ends, the BOM, comments, a field without a colon, the one space, retry
// of digits alone, an id with NUL, the last event id kept, empty data not
// dispatched, an event cut by the end dropped, the limits), the same
// bytes in pieces of every size; an event's text; event_stream over
// HTTP/1.1 and HTTP/2, each event flushed as it is sent; event_reader and
// event_source (reconnection with Last-Event-ID and the stream's retry,
// 204 and another type failing it, the stop, close); interop with Go's
// net/http both ways (go_sse/main.go) and with curl -N.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;
using namespace sgcl::net::http::detail;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    // Every event of the bytes, fed `piece` bytes at a time (0: whole):
    // "type|id|data", data's LF as \n; "<too large>" when the limit was passed
    std::vector<std::string> parse(const std::string& bytes, size_t piece = 0, size_t limit = 1 << 20) {
        EventParser p(limit);
        std::deque<ParsedEvent> out;
        size_t step = piece ? piece : std::max<size_t>(bytes.size(), 1);
        for (size_t i = 0; i < bytes.size(); i += step) {
            if (!p.feed(bytes.data() + i, std::min(step, bytes.size() - i), out)) {
                std::vector<std::string> r;
                for (auto& e : out) {
                    r.push_back(e.type + "|" + e.id + "|" + e.data);
                }
                r.push_back("<too large>");
                return r;
            }
        }
        p.finish();
        std::vector<std::string> r;
        for (auto& e : out) {
            std::string d;
            for (char c : e.data) {
                d += c == '\n' ? std::string("\\n") : std::string(1, c);
            }
            r.push_back(e.type + "|" + e.id + "|" + d + (e.retry ? " retry=" + std::to_string(*e.retry) : ""));
        }
        return r;
    }

    using V = std::vector<std::string>;

    bool have(const char* tool) {
        return std::system((std::string("command -v ") + tool + " > /dev/null 2>&1").c_str()) == 0;
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

    const std::string& go_sse() {
        static std::string path = [] {
            if (!have("go")) {
                return std::string();
            }
            auto src = source_root() / "tests/net/http/go_sse/main.go";
            auto out = std::filesystem::temp_directory_path() / "sgcl_go_sse";
            std::string cmd = "go build -o '" + out.string() + "' '" + src.string() + "' 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    net::http::client direct() {
        net::http::client c;
        c.proxy = net::http::proxy();
        return c;
    }

    // What a server of the tests saw of its requests
    struct Seen {
        std::mutex lock;
        std::vector<std::string> last_ids;
        std::atomic<int> requests{0};
    };

    struct Server {
        net::http::server srv;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        tracked_ptr<Seen> seen = make_tracked<Seen>();

        explicit Server(bool h2c = false) {
            srv.h2c = h2c;
            tracked_ptr<Seen> s = seen;
            // three events, then the end; resumed after Last-Event-ID
            srv.route("GET /events", [s](net::http::request req, net::http::response_writer w) -> async::task<> {
                ++s->requests;
                {
                    std::lock_guard g(s->lock);
                    s->last_ids.push_back(text(req.header("Last-Event-ID")));
                }
                net::http::event_stream events(w);
                int from = req.header("Last-Event-ID").empty() ? 0 : std::stoi(text(req.header("Last-Event-ID"))) + 1;
                for (int i = from; i < from + 3; ++i) {
                    net::http::event e;
                    e.type = "tick";
                    e.id = to_string(i);
                    e.data = "event " + to_string(i) + "\nsecond line";
                    if (i == 0) {
                        e.retry = 50 * millisecond;
                    }
                    if (!co_await events.async_send(e)) {
                        co_return;
                    }
                }
            });
            // one event, then a pause, then another: what was sent arrives before the rest is written
            srv.route("GET /slow", [](net::http::request, net::http::response_writer w) -> async::task<> {
                net::http::event_stream events(w);
                co_await events.async_comment("open");
                co_await events.async_send("first");
                co_await async::sleep(300 * millisecond);
                co_await events.async_send("second");
            });
            srv.route("GET /nothing", [](net::http::request, net::http::response_writer w) { w.set_status(204); });
            srv.route("GET /plain", [](net::http::request, net::http::response_writer w) { w.write("data: not an event stream\n\n"); });
            srv.route("GET /forever", [](net::http::request req, net::http::response_writer w) -> async::task<> {
                net::http::event_stream events(w);
                for (int i = 0;; ++i) {
                    if (!co_await events.async_send(to_string(i))) {
                        co_return;
                    }
                    co_await async::sleep(10 * millisecond);
                }
            });
            listener = *net::tcp::listen("127.0.0.1:0");
            serving = async::spawn(srv.async_serve(listener));
        }

        ~Server() {
            srv.close();
            (void)serving.wait();
        }

        sgcl::string url(const std::string& path) const {
            return sgcl::string("http://127.0.0.1:" + std::to_string(listener.local_endpoint().port()) + path);
        }
    };
}

TEST(HttpEventsParse_Tests, TheStandardsExamples) {
    // §9.2.6's examples
    EXPECT_EQ(parse("data: YHOO\ndata: +2\ndata: 10\n\n"), V{"message||YHOO\\n+2\\n10"});
    EXPECT_EQ(parse(": test stream\n\ndata: first event\nid: 1\n\ndata:second event\nid\n\ndata:  third event\n"),
              (V{"message|1|first event", "message||second event"}));   // the third has no empty line
    EXPECT_EQ(parse("data\n\ndata\ndata\n\ndata:"), (V{"message||", "message||\\n"}));
    EXPECT_EQ(parse("data:test\n\ndata: test\n\n"), (V{"message||test", "message||test"}));
    EXPECT_EQ(parse("event: add\ndata: 73857293\n\nevent: remove\ndata: 2153\n\nevent: add\ndata: 113411\n\n"),
              (V{"add||73857293", "remove||2153", "add||113411"}));
}

TEST(HttpEventsParse_Tests, EveryRuleOfTheInterpretation) {
    // the three line ends, mixed
    EXPECT_EQ(parse("data: a\r\ndata: b\rdata: c\n\r\n"), V{"message||a\\nb\\nc"});
    EXPECT_EQ(parse("data: cr\r\r"), V{"message||cr"});
    // the BOM, once, and only at the start
    EXPECT_EQ(parse("\xEF\xBB\xBF" "data: bom\n\n"), V{"message||bom"});
    EXPECT_EQ(parse("\xEF\xBB\xBF\xEF\xBB\xBF" "data: two\n\n"), V{});   // the second BOM starts a field's name
    EXPECT_EQ(parse("data: \xEF\xBB\xBF\n\n"), V{"message||\xEF\xBB\xBF"});
    // one space dropped, no more
    EXPECT_EQ(parse("data:  two spaces\n\n"), V{"message|| two spaces"});
    EXPECT_EQ(parse("data:\tx\n\n"), V{"message||\tx"});
    // a field without a colon: its value empty
    EXPECT_EQ(parse("data\ndata: x\n\n"), V{"message||\\nx"});
    // fields not known, comments: dropped
    EXPECT_EQ(parse("foo: bar\n:comment\nData: no\ndata: yes\n\n"), V{"message||yes"});
    // event: the type of the next dispatch only
    EXPECT_EQ(parse("event: one\ndata: 1\n\ndata: 2\n\n"), (V{"one||1", "message||2"}));
    EXPECT_EQ(parse("event: lost\n\ndata: x\n\n"), V{"message||x"});   // empty data: nothing dispatched, the type reset
    // id: kept across events, set to empty by "id", ignored with NUL
    EXPECT_EQ(parse("id: 7\ndata: a\n\ndata: b\n\nid\ndata: c\n\n"), (V{"message|7|a", "message|7|b", "message||c"}));
    EXPECT_EQ(parse(std::string("id: 1\ndata: a\n\nid: 2\0x\ndata: b\n\n", 32)), (V{"message|1|a", "message|1|b"}));
    EXPECT_EQ(parse("id: 3\n\ndata: x\n\n"), V{"message|3|x"});   // an id without data still sets it
    // retry: ASCII digits alone
    EXPECT_EQ(parse("retry: 1500\ndata: x\n\n"), V{"message||x retry=1500"});
    for (const char* bad : {"retry: 15a\n", "retry: -1\n", "retry:\n", "retry: 1.5\n", "retry:  10\n"}) {
        EXPECT_EQ(parse(std::string(bad) + "data: x\n\n"), V{"message||x"}) << bad;
    }
    EXPECT_EQ(parse("retry: 99999999999999999999999\ndata: x\n\n"), V{"message||x retry=9223372036854"});
    // an event cut by the end: dropped
    EXPECT_EQ(parse("data: whole\n\ndata: cut"), V{"message||whole"});
    EXPECT_EQ(parse("data: cut\n"), V{});
    EXPECT_EQ(parse(""), V{});
    // the limits: a line, an event
    EXPECT_EQ(parse("data: " + std::string(100, 'x') + "\n\n", 0, 50), V{"<too large>"});
    EXPECT_EQ(parse("data: 0123456789\ndata: 0123456789\ndata: 0123456789\n\n", 0, 30), V{"<too large>"});
    EXPECT_EQ(parse("data: 0123456789\n\ndata: 0123456789\n\n", 0, 30), (V{"message||0123456789", "message||0123456789"}));
}

TEST(HttpEventsParse_Tests, TheSameBytesInPiecesOfEverySize) {
    const std::string bytes = "\xEF\xBB\xBF: c\r\nevent: a\r\nid: 9\r\nretry: 20\r\ndata: one\r\ndata\r\n\r\ndata: two\rdata: x\r\rdata: three\n\n"
                              "event: e\ndata:" + std::string(3000, 'z') + "\n\n";
    auto whole = parse(bytes);
    ASSERT_EQ(whole.size(), 4u);
    for (size_t piece : {1, 2, 3, 4, 5, 7, 13, 64, 1000}) {
        EXPECT_EQ(parse(bytes, piece), whole) << piece;
    }
    // a BOM cut over three pieces, and one that is not a BOM
    EXPECT_EQ(parse("\xEF\xBB\xBF" "data: x\n\n", 1), V{"message||x"});
    EXPECT_EQ(parse("\xEF\xBB" "data: x\n\n", 1), V{});
}

TEST(HttpEventsWrite_Tests, AnEventsText) {
    net::http::event e;
    e.data = "one";
    EXPECT_EQ(*event_text(e), "data: one\n\n");
    e.type = "update";
    e.id = "42";
    e.retry = 2500 * millisecond;
    e.data = "a\r\nb\rc\nd";
    EXPECT_EQ(*event_text(e), "event: update\nid: 42\nretry: 2500\ndata: a\ndata: b\ndata: c\ndata: d\n\n");
    EXPECT_EQ(parse(*event_text(e)), V{"update|42|a\\nb\\nc\\nd retry=2500"});
    net::http::event empty;
    EXPECT_EQ(*event_text(empty), "data: \n\n");   // an empty message is dispatched
    EXPECT_EQ(parse(*event_text(empty)), V{"message||"});
    net::http::event trailing;
    trailing.data = "ends\n";
    EXPECT_EQ(parse(*event_text(trailing)), V{"message||ends\\n"});
    net::http::event bad;
    bad.type = "a\nb";
    EXPECT_FALSE(event_text(bad));
    bad.type = "";
    bad.id = "1\r";
    EXPECT_FALSE(event_text(bad));
    bad.id = sgcl::string(std::string("1\0", 2));
    EXPECT_FALSE(event_text(bad));
    EXPECT_EQ(comment_text("keep-alive"), ": keep-alive\n\n");
    EXPECT_EQ(comment_text(""), ":\n\n");
    EXPECT_EQ(comment_text("two\nlines"), ": two\n: lines\n\n");
    EXPECT_EQ(parse(comment_text("x") + "data: y\n\n"), V{"message||y"});
}

TEST(HttpEvents_Tests, AStreamOverHttp1AndHttp2) {
    for (bool h2 : {false, true}) {
        Server server(h2);
        net::http::client c = direct();
        c.h2c = h2;
        auto res = c.get(server.url("/events"));
        ASSERT_TRUE(res) << text(res.error().message());
        EXPECT_EQ(text(res->proto()), h2 ? "HTTP/2.0" : "HTTP/1.1");
        EXPECT_EQ(res->header("Content-Type"), "text/event-stream");
        EXPECT_EQ(res->header("Cache-Control"), "no-cache");
        if (!h2) {
            EXPECT_EQ(res->header("Transfer-Encoding"), "chunked");
        }
        net::http::event_reader r(*res);
        std::vector<std::string> got;
        while (auto e = r.next()) {
            if (!*e) {
                break;
            }
            got.push_back(text((*e)->type) + "|" + text((*e)->id) + "|" + text((*e)->data));
        }
        EXPECT_EQ(got, (V{"tick|0|event 0\nsecond line", "tick|1|event 1\nsecond line", "tick|2|event 2\nsecond line"}));
        EXPECT_EQ(text(r.last_event_id()), "2");
        ASSERT_TRUE(r.retry());
        EXPECT_EQ(*r.retry(), 50 * millisecond);
        auto end = r.next();
        ASSERT_TRUE(end);
        EXPECT_FALSE(*end);   // and again at the end
    }
}

TEST(HttpEvents_Tests, EachEventFlushedAsItIsSent) {
    Server server;
    auto res = direct().get(server.url("/slow"));
    ASSERT_TRUE(res);
    net::http::event_reader r(*res);
    auto t0 = std::chrono::steady_clock::now();
    auto first = r.next();
    auto took = std::chrono::steady_clock::now() - t0;
    ASSERT_TRUE(first && *first);
    EXPECT_EQ(text((*first)->data), "first");
    EXPECT_LT(took, 250ms);   // before the server's pause
    auto second = r.next();
    ASSERT_TRUE(second && *second);
    EXPECT_EQ(text((*second)->data), "second");
    // the task form
    auto reading = [](net::http::client c, sgcl::string url) -> async::task<std::string> {
        auto res = co_await c.async_get(url);
        net::http::event_reader r(*res);
        std::string all;
        while (auto e = co_await r.async_next()) {
            if (!*e) {
                break;
            }
            all += text((*e)->data) + ";";
        }
        co_return all;
    };
    EXPECT_EQ(async::spawn(reading(direct(), server.url("/slow"))).wait(), "first;second;");
    net::http::event_reader none;
    EXPECT_FALSE(none);
}

TEST(HttpEvents_Tests, AReaderOverAnyStreamAndItsLimit) {
    tracked_ptr b = make_tracked<io::buffer>(sgcl::string("data: in memory\n\nevent: x\ndata: " + std::string(5000, 'q') + "\n\n"));
    net::http::event_reader r(io::reader(b), 1000);
    auto e = r.next();
    ASSERT_TRUE(e && *e);
    EXPECT_EQ(text((*e)->type), "message");
    auto big = r.next();
    ASSERT_FALSE(big);
    EXPECT_EQ(big.error().code(), net::errc::body_too_large);
    EXPECT_FALSE(r.next());   // kept
}

TEST(HttpEvents_Tests, ASourceReconnectsWithLastEventId) {
    Server server;
    net::http::event_source::options o;
    o.max_reconnects = 2;
    o.retry = 10 * second;   // the stream's retry (50 ms) takes its place
    o.headers.set("X-Client", "test");
    net::http::event_source src(direct(), server.url("/events"), o);
    std::vector<std::string> got;
    for (int i = 0; i < 7; ++i) {
        auto e = src.next();
        if (!e) {
            got.push_back("<" + text(e.error().message()) + ">");
            break;
        }
        got.push_back(text(e->id));
    }
    EXPECT_EQ(got, (V{"0", "1", "2", "3", "4", "5", "6"}));
    EXPECT_EQ(src.retry(), 50 * millisecond);
    EXPECT_EQ(text(src.last_event_id()), "6");
    std::lock_guard g(server.seen->lock);
    ASSERT_GE(server.seen->last_ids.size(), 3u);
    EXPECT_EQ(server.seen->last_ids[0], "");
    EXPECT_EQ(server.seen->last_ids[1], "2");
    EXPECT_EQ(server.seen->last_ids[2], "5");
}

TEST(HttpEvents_Tests, ASourceGivesUp) {
    Server server;
    auto failed = [&](const char* path, int reconnects = 2) {
        net::http::event_source::options o;
        o.max_reconnects = reconnects;
        o.retry = 20 * millisecond;
        net::http::event_source src(direct(), server.url(path), o);
        auto e = src.next();
        return e ? std::string("event") : text(e.error().message());
    };
    EXPECT_NE(failed("/nothing").find("(204)"), std::string::npos);   // 204: the server's way to stop it
    EXPECT_NE(failed("/missing").find("(404)"), std::string::npos);
    EXPECT_NE(failed("/plain").find("(Content-Type"), std::string::npos);
    // a server that is not there: tries, then no more
    net::http::event_source::options o;
    o.max_reconnects = 2;
    o.retry = 20 * millisecond;
    net::http::event_source gone(direct(), "http://127.0.0.1:1/events", o);
    auto t0 = std::chrono::steady_clock::now();
    auto e = gone.next();
    ASSERT_FALSE(e);
    EXPECT_NE(text(e.error().message()).find("no more reconnections"), std::string::npos);
    EXPECT_GE(std::chrono::steady_clock::now() - t0, 40ms);
    EXPECT_FALSE(gone.next());   // kept
    // the stop, and close
    async::stop_source stop;
    net::http::event_source::options so;
    so.stop = stop.token();
    net::http::event_source forever(direct(), server.url("/forever"), so);
    EXPECT_TRUE(forever.next());
    stop.request_stop();
    for (int i = 0; i < 100; ++i) {
        auto x = forever.next();
        if (!x) {
            EXPECT_EQ(x.error().code(), std::errc::operation_canceled);
            break;
        }
    }
    net::http::event_source closing(direct(), server.url("/forever"), net::http::event_source::options());
    EXPECT_TRUE(closing.next());
    closing.close();
    auto after = closing.next();
    ASSERT_FALSE(after);
    EXPECT_EQ(after.error().code(), io::errc::closed);
    net::http::event_source none;
    EXPECT_FALSE(none);
}

TEST(HttpEvents_Tests, AWriterRefusesWhatWouldBreakALine) {
    Server server;
    net::http::server s;
    tracked_ptr<std::string> got = make_tracked<std::string>();
    s.route("GET /", [got](net::http::request, net::http::response_writer w) -> async::task<> {
        net::http::event_stream events(w);
        net::http::event bad;
        bad.type = "a\nb";
        auto r = co_await events.async_send(bad);
        *got = r ? "sent" : text(r.error().message());
        co_await events.async_send("fine");
    });
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(s.async_serve(l));
    auto res = direct().get(sgcl::string("http://127.0.0.1:" + std::to_string(l.local_endpoint().port()) + "/"));
    EXPECT_EQ(text(*res->text()), "data: fine\n\n");
    EXPECT_EQ(*got, "events an event's type or id: Invalid argument");
    s.close();
    (void)serving.wait();
}

TEST(HttpEventsInterop_Tests, GoBothWaysAndCurl) {
    Server server;
    if (have("curl")) {
        auto out = run("curl -sN --max-time 5 " + text(server.url("/events")));
        EXPECT_EQ(out, "event: tick\nid: 0\nretry: 50\ndata: event 0\ndata: second line\n\n"
                       "event: tick\nid: 1\ndata: event 1\ndata: second line\n\n"
                       "event: tick\nid: 2\ndata: event 2\ndata: second line\n\n");
    }
    if (go_sse().empty()) {
        GTEST_SKIP() << "no go to build the peer with";
    }
    // Go's client reads this server's stream
    EXPECT_EQ(run("'" + go_sse() + "' client " + text(server.url("/events"))),
              "tick|0|event 0\\nsecond line\ntick|1|event 1\\nsecond line\ntick|2|event 2\\nsecond line\n");
    // this client reads Go's
    FILE* p = popen(("'" + go_sse() + "' server").c_str(), "r");
    ASSERT_TRUE(p);
    char line[64] = {};
    ASSERT_TRUE(fgets(line, sizeof(line), p));
    int port = std::atoi(line + 5);
    ASSERT_GT(port, 0);
    for (const char* path : {"/events", "/cr"}) {
        auto res = direct().get(sgcl::string("http://127.0.0.1:" + std::to_string(port) + path));
        ASSERT_TRUE(res);
        net::http::event_reader r(*res);
        std::string all;
        while (auto e = r.next()) {
            if (!*e) {
                break;
            }
            all += text((*e)->type) + "|" + text((*e)->id) + "|" + text((*e)->data) + ";";
        }
        EXPECT_EQ(all, "tick|0|first line 0\nsecond line;tick|1|first line 1\nsecond line;tick|2|first line 2\nsecond line;tick|3|first line 3\nsecond line;")
            << path;
        EXPECT_EQ(*r.retry(), 50 * millisecond);
    }
    // a source resumed by Go after Last-Event-ID
    net::http::event_source::options o;
    o.max_reconnects = 1;
    net::http::event_source src(direct(), sgcl::string("http://127.0.0.1:" + std::to_string(port) + "/events"), o);
    for (int i = 0; i < 4; ++i) {
        auto e = src.next();
        ASSERT_TRUE(e);
        EXPECT_EQ(text(e->id), std::to_string(i));
    }
    auto e = src.next();   // the stream ended at 3: resumed after 3, nothing more, then no more tries
    ASSERT_FALSE(e);
    (void)direct().get(sgcl::string("http://127.0.0.1:" + std::to_string(port) + "/quit"));
    pclose(p);
}
