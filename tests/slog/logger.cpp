//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// slog's behaviour beside the formats (oracle.cpp): levels and level_var,
// the logger as a value, one write per record, the buffered batches, a
// failed write, the source, the local time, the handlers of the program
// and memory with the record's views and clone(), the kinds of values,
// the default logger, sampling.
#include "common.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using slog_test::text_of;

namespace {
    // A writer that counts its writes and keeps what they wrote
    struct Counting {
        std::atomic<int> writes{0};
        std::string bytes;

        expected<size_t, io::error> write(const slice<const byte>& d) {
            bytes.append(reinterpret_cast<const char*>(d.data()), d.size());
            ++writes;   // after the bytes: a reader that saw the count sees them
            return d.size();
        }
    };

    struct Failing {
        expected<size_t, io::error> write(const slice<const byte>&) {
            return unexpected(io::error(std::make_error_code(std::errc::broken_pipe), "write"));
        }
    };

    size_t lines_in(const std::string& s) {
        size_t n = 0;
        for (char c : s) {
            n += c == '\n';
        }
        return n;
    }

    struct Req {
        int id = 5;
        string path = "/a";

        void describe(encoding::field_list& f) {
            f.add("id", id);
            f.add("path", path);
        }
    };

    // A type that makes its text as a string of the library, and one as
    // a std::string, and one that writes itself (format_value)
    struct Tagged {
        int n;

        string to_text() const {
            return string("tag-") + string(std::to_string(n));
        }
    };

    struct Named {
        std::string to_string() const {
            return "named value";
        }
    };

    struct Point {
        int x, y;
    };

    void format_value(txt::format_sink& out, const Point& p, const txt::format_spec&) {
        out.put('(');
        auto x = std::to_string(p.x);
        out.put(x.data(), x.size());
        out.put(',');
        auto y = std::to_string(p.y);
        out.put(y.data(), y.size());
        out.put(')');
    }

    // A handler of the program: what it was given
    struct Collect {
        std::vector<std::string> messages;
        slog::level least = slog::level::debug;

        void handle(const slog::record& r) const {
            const_cast<Collect*>(this)->messages.emplace_back(r.message().data(), r.message().size());
        }

        bool enabled(slog::level l) const {
            return int(l) >= int(least);
        }
    };

    // The part of a line after " msg=", without the new line
    std::string after_msg(const std::string& line) {
        auto at = line.find(" msg=");
        auto end = line.find('\n', at);
        return at == std::string::npos ? line : line.substr(at + 5, end - at - 5);
    }
}

TEST(Slog_Tests, LevelsAndEnabled) {
    io::buffer out;
    auto lg = slog::logger(out, slog::level::warn);
    EXPECT_FALSE(lg.enabled(slog::level::info));
    EXPECT_TRUE(lg.enabled(slog::level::warn));
    EXPECT_TRUE(lg.enabled(slog::level(5)));
    lg.info("dropped");
    lg.debug("dropped");
    lg.warn("kept");
    lg.error("kept too");
    lg.log(slog::level(6), "between");
    auto text = text_of(out);
    EXPECT_EQ(lines_in(text), 3u);
    EXPECT_NE(text.find("level=WARN msg=kept\n"), std::string::npos);
    EXPECT_NE(text.find("level=ERROR msg=\"kept too\"\n"), std::string::npos);
    EXPECT_NE(text.find("level=WARN+2 msg=between\n"), std::string::npos);
}

TEST(Slog_Tests, ALevelVarChangesEveryLoggerMadeWithIt) {
    io::buffer out;
    slog::level_var least(slog::level::error);
    auto a = slog::logger(slog::options{.out = out, .level_var = least});
    auto b = a.with("who", "b");
    a.info("no");
    b.warn("no");
    EXPECT_EQ(lines_in(text_of(out)), 0u);
    least.set(slog::level::debug);
    EXPECT_EQ(least.get(), slog::level::debug);
    a.debug("yes");
    b.info("yes");
    EXPECT_EQ(lines_in(text_of(out)), 2u);
    // a logger made without the variable keeps its fixed level
    auto c = slog::logger(out, slog::level::error);
    least.set(slog::level::debug);
    EXPECT_FALSE(c.enabled(slog::level::info));
}

TEST(Slog_Tests, ALoggerIsAValue) {
    io::buffer out;
    auto base = slog::logger(out);
    auto child = base.with("service", "api");
    auto grouped = base.group("req");
    base.info("base", "k", 1);
    child.info("child", "k", 1);
    grouped.info("grouped", "k", 1);
    auto text = text_of(out);
    EXPECT_NE(text.find("msg=base k=1\n"), std::string::npos);
    EXPECT_NE(text.find("msg=child service=api k=1\n"), std::string::npos);
    EXPECT_NE(text.find("msg=grouped req.k=1\n"), std::string::npos);
    // an empty group's name is no group
    out.clear();
    base.group("").info("m", "k", 1);
    EXPECT_NE(text_of(out).find("msg=m k=1\n"), std::string::npos);
    // a copy is the same logger
    auto copy = child;
    copy.warn("copy");
    EXPECT_NE(text_of(out).find("msg=copy service=api\n"), std::string::npos);
    static_assert(sizeof(slog::logger) == sizeof(void*));
    static_assert(sgcl::req::handle<slog::logger>);
}

TEST(Slog_Tests, ARecordIsOneWriteOfOneLine) {
    Counting w;
    auto lg = slog::logger(w);
    for (int i = 0; i < 10; ++i) {
        lg.info("line", "i", i, "text", "a b c", "more", 1.5);
    }
    auto js = slog::logger(slog::options{.out = w, .json = true});
    for (int i = 0; i < 5; ++i) {
        js.info("line", "i", i);
    }
    EXPECT_EQ(w.writes.load(), 15);
    EXPECT_EQ(lines_in(w.bytes), 15u);
}

TEST(Slog_Tests, BufferedLinesWaitForTheBatch) {
    Counting w;
    auto lg = slog::logger(slog::options{.out = w, .buffered = true});
    for (int i = 0; i < 10; ++i) {
        lg.info("batched", "i", i);
    }
    EXPECT_EQ(w.writes.load(), 0);
    lg.flush();
    EXPECT_EQ(w.writes.load(), 1);
    EXPECT_EQ(lines_in(w.bytes), 10u);
    // a warn writes its batch at once, itself included
    lg.info("before");
    lg.warn("now");
    EXPECT_EQ(w.writes.load(), 2);
    EXPECT_EQ(lines_in(w.bytes), 12u);
    // a full batch is written before the line that does not fit
    std::string big(1000, 'x');
    for (int i = 0; i < 40; ++i) {
        lg.info("big", "x", big);
    }
    EXPECT_GE(w.writes.load(), 3);
    lg.flush();
    EXPECT_EQ(lines_in(w.bytes), 52u);
    // a line longer than a batch is written alone
    std::string huge(40000, 'y');
    lg.info("huge", "y", huge);
    lg.flush();
    EXPECT_EQ(lines_in(w.bytes), 53u);
    // the loggers made from it share its batches
    auto child = lg.with("c", 1);
    child.info("one");
    lg.info("two");
    int before = w.writes.load();
    child.flush();
    EXPECT_EQ(w.writes.load(), before + 1);
    EXPECT_EQ(lines_in(w.bytes), 55u);
}

TEST(Slog_Tests, AWorkerWritesItsBatchBeforeItSleeps) {
    Counting w;
    auto lg = slog::logger(slog::options{.out = w, .buffered = true});
    async::spawn([lg]() -> async::task<> {
        for (int i = 0; i < 5; ++i) {
            lg.info("from a task", "i", i);
        }
        co_return;
    }).wait();
    // the worker goes to sleep once it has nothing to run
    for (int i = 0; i < 2000 && w.writes.load() == 0; ++i) {   // the count first: the bytes are the writer's until its write is done
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(lines_in(w.bytes), 5u);
    EXPECT_EQ(w.writes.load(), 1);
}

TEST(Slog_Tests, AFailedWriteIsCounted) {
    Failing w;
    auto lg = slog::logger(w);
    lg.info("lost");
    lg.info("lost too");
    EXPECT_EQ(lg.dropped(), 2u);
    auto b = slog::logger(slog::options{.out = w, .buffered = true});
    b.info("a");
    b.info("b");
    b.flush();
    EXPECT_EQ(b.dropped(), 2u);
}

TEST(Slog_Tests, SourceIsWhereTheCallIs) {
    io::buffer out;
    auto lg = slog::logger(slog::options{.out = out, .source = true});
    const auto here = std::source_location::current();
    lg.info("m");
    auto text = text_of(out);
    std::string want = std::string("source=") + here.file_name() + ":" + std::to_string(here.line() + 1) + " msg=m\n";
    EXPECT_NE(text.find(want), std::string::npos) << text;
    out.clear();
    auto js = slog::logger(slog::options{.out = out, .json = true, .source = true});
    const auto there = std::source_location::current();
    js.info("m");
    auto json = encoding::json::parse(string(text_of(out)));
    ASSERT_TRUE(json.has_value());
    EXPECT_EQ(std::string((*json)["source"]["file"].as_string()->view()), there.file_name());
    EXPECT_EQ((*json)["source"]["line"].as_int(), int64_t(there.line() + 1));
    EXPECT_FALSE((*json)["source"]["function"].as_string()->empty());
}

TEST(Slog_Tests, TheTimeIsLocalUnlessUtc) {
    io::buffer out;
    slog::logger(out).info("m");
    auto text = text_of(out);
    // time=YYYY-MM-DDTHH:MM:SS.mmm then Z or +hh:mm, as the zone says now
    auto offset = time::now().offset();
    std::string zone = offset == sgcl::duration() ? "Z" : "";
    if (zone.empty()) {
        auto s = time::now().to_string();
        zone = std::string(s.data() + s.size() - 6, 6);
    }
    ASSERT_GT(text.size(), 29u + zone.size());
    EXPECT_EQ(text.substr(5 + 23, zone.size()), zone) << text;
    out.clear();
    slog::logger(slog::options{.out = out, .utc = true}).info("m");
    EXPECT_EQ(text_of(out)[5 + 23], 'Z');
}

TEST(Slog_Tests, TheKindsOfValues) {
    io::buffer out;
    auto lg = slog::logger(out);
    std::string s = "std string";
    std::string_view v = "a view";
    string ss = "sgcl string";
    lg.info("m", "s", s, "v", v, "ss", ss, "sl", slice<const char>(ss), "c", "literal");
    EXPECT_EQ(after_msg(text_of(out)), "m s=\"std string\" v=\"a view\" ss=\"sgcl string\" sl=\"sgcl string\" c=literal");
    out.clear();
    lg.info("m", "i8", int8_t(-5), "u8", uint8_t(200), "i16", short(-3), "u", 7u, "f", 2.5f, "b", false, "ms", 250ms, "null", nullptr);
    EXPECT_EQ(after_msg(text_of(out)), "m i8=-5 u8=200 i16=-3 u=7 f=2.5 b=false ms=250ms null=<nil>");
    out.clear();
    lg.info("m", "some", optional<int>(4), "none", optional<string>(), "opt", optional<string>(string("x y")));
    EXPECT_EQ(after_msg(text_of(out)), "m some=4 none=<nil> opt=\"x y\"");
    out.clear();
    auto ioe = io::error(std::make_error_code(std::errc::no_such_file_or_directory), "open", "log.txt");
    auto ec = std::make_error_code(std::errc::broken_pipe);
    lg.error("m", "err", ioe, "ec", ec);
    EXPECT_EQ(after_msg(text_of(out)), "m err=\"" + std::string(ioe.message().data(), ioe.message().size()) + "\" ec=\"" + ec.message() + "\"");
    out.clear();
    std::exception_ptr none;
    std::exception_ptr thrown;
    try {
        throw std::runtime_error("bad thing");
    } catch (...) {
        thrown = std::current_exception();
    }
    lg.error("m", "x", thrown, "none", none);
    EXPECT_EQ(after_msg(text_of(out)), "m x=\"std::runtime_error: bad thing\" none=<nil>");
    out.clear();
    lg.info("m", "t", Tagged{3}, "n", Named(), "p", Point{1, -2});
    EXPECT_EQ(after_msg(text_of(out)), "m t=tag-3 n=\"named value\" p=(1,-2)");
    out.clear();
    auto ep = net::endpoint(net::ip_address::v6({0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}), 443);
    auto ip = net::ip_address::v4(10, 0, 0, 1);
    lg.info("m", "remote", ep, "ip", ip, "net", net::ip_network(ip, 8), "bad", net::endpoint());
    EXPECT_EQ(after_msg(text_of(out)), "m remote=[2001:db8::1]:443 ip=10.0.0.1 net=10.0.0.1/8 bad=\"invalid AddrPort\"");
}

TEST(Slog_Tests, GroupsAndAttrs) {
    io::buffer out;
    auto lg = slog::logger(out);
    lg.info("m", slog::group("req", "id", 5, "path", "/a"), "k", 1, slog::group("peer", slog::group("addr", "port", 443)));
    EXPECT_EQ(after_msg(text_of(out)), "m req.id=5 req.path=/a k=1 peer.addr.port=443");
    out.clear();
    lg.with(slog::group("ctx", "user", "ala")).info("m");
    EXPECT_EQ(after_msg(text_of(out)), "m ctx.user=ala");
}

TEST(Slog_Tests, MessagesOfEveryTextType) {
    io::buffer out;
    auto lg = slog::logger(out);
    std::string a = "from std";
    string b = "from sgcl";
    const char* c = "from pointer";
    lg.info(a);
    lg.info(b);
    lg.info(c);
    lg.info(slice<const char>(b));
    auto text = text_of(out);
    EXPECT_NE(text.find("msg=\"from std\"\n"), std::string::npos);
    EXPECT_NE(text.find("msg=\"from sgcl\"\n"), std::string::npos);
    EXPECT_NE(text.find("msg=\"from pointer\"\n"), std::string::npos);
    EXPECT_EQ(lines_in(text), 4u);
}

TEST(Slog_Tests, MemoryKeepsTheRecordsAsTrees) {
    slog::memory mem;
    auto lg = slog::logger(mem, slog::level::debug);
    {
        auto req = Req{9, "/copied"};
        string text = "temporary text";
        lg.with("service", "api").group("http").with("id", 42).debug("request", "text", text, "req", req, "d", 2s);
    }
    lg.info("second");
    collector::force_collect(true);
    auto records = mem.records();
    ASSERT_EQ(records.size(), 2u);
    const auto& r = records[0];
    EXPECT_EQ(r.level(), slog::level::debug);
    EXPECT_EQ(std::string_view(r.message().data(), r.message().size()), "request");
    ASSERT_EQ(r.size(), 2u);
    auto it = r.begin();
    auto first = *it;
    EXPECT_EQ(std::string_view(first.key().data(), first.key().size()), "service");
    EXPECT_EQ(first.value().type(), slog::value::kind::string);
    EXPECT_EQ(first.value().as_string(), string("api"));
    auto http = *++it;
    EXPECT_EQ(http.value().type(), slog::value::kind::group);
    std::vector<std::string> keys;
    for (auto a : http.value().as_group()) {
        keys.emplace_back(a.key().data(), a.key().size());
    }
    EXPECT_EQ(keys, (std::vector<std::string>{"id", "text", "req", "d"}));
    EXPECT_EQ(http.value().text(), string("[id=42 text=temporary text req=[id=9 path=/copied] d=2s]"));
    EXPECT_EQ(http.value().json(), string("{\"id\":42,\"text\":\"temporary text\",\"req\":{\"id\":9,\"path\":\"/copied\"},\"d\":2000000000}"));
    for (auto a : http.value().as_group()) {
        if (std::string_view(a.key().data(), a.key().size()) == "id") {
            EXPECT_EQ(a.value().as_int(), 42);
            EXPECT_THROW((void)a.value().as_string(), std::logic_error);
        }
        if (std::string_view(a.key().data(), a.key().size()) == "d") {
            EXPECT_EQ(a.value().as_duration(), sgcl::duration(2s));
        }
    }
    EXPECT_EQ(records[1].size(), 0u);   // a logger without with() and group()
    mem.clear();
    EXPECT_EQ(mem.size(), 0u);
}

TEST(Slog_Tests, TheAttributesAreARangeOfTheirOwnName) {
    // A record and a group's value give their attributes as slog::attrs,
    // a type a program can name, and not one of detail
    static_assert(std::is_same_v<decltype(std::declval<const slog::record&>().begin()), slog::attrs::iterator>);
    static_assert(std::is_same_v<decltype(std::declval<const slog::record&>().end()), slog::attrs::iterator>);
    static_assert(std::is_same_v<decltype(std::declval<const slog::value&>().as_group()), slog::attrs>);
    static_assert(std::input_iterator<slog::attrs::iterator>);
    slog::memory mem;
    slog::logger(mem).group("g").info("m", "a", 1, "b", 2);
    slog::attrs group = (*mem.records()[0].begin()).value().as_group();
    EXPECT_EQ(group.size(), 2u);
    EXPECT_FALSE(group.empty());
    slog::attrs none;
    EXPECT_TRUE(none.empty());
    EXPECT_EQ(none.size(), 0u);
    EXPECT_TRUE(none.begin() == none.end());
}

TEST(Slog_Tests, AHandlerOfTheProgram) {
    Collect c;
    auto lg = slog::logger(c, slog::level::debug);
    lg.debug("one");
    lg.info("two", "k", 1);
    ASSERT_EQ(c.messages.size(), 2u);
    c.least = slog::level::warn;   // its enabled() is asked
    EXPECT_FALSE(lg.enabled(slog::level::info));
    lg.info("three");
    EXPECT_EQ(c.messages.size(), 2u);
    // by tracked_ptr
    tracked_ptr<Collect> held = make_tracked<Collect>();
    auto by_ptr = slog::logger(held);
    by_ptr.info("held");
    EXPECT_EQ(held->messages.size(), 1u);
    // a record's time, level and attributes as the handler sees them
    struct Check {
        int* seen;

        void handle(const slog::record& r) const {
            ++*seen;
            EXPECT_EQ(r.level(), slog::level::warn);
            EXPECT_TRUE(r.has_source());
            EXPECT_GT(r.source().line(), 0u);
            EXPECT_GT(r.time().unix(), 1700000000);
            size_t n = 0;
            for (auto a : r) {
                (void)a;
                ++n;
            }
            EXPECT_EQ(n, 2u);
        }
    };
    int seen = 0;
    slog::logger(slog::options{.handler = Check{&seen}, .source = true}).warn("m", "a", 1, "b", "two");
    EXPECT_EQ(seen, 1);
}

TEST(Slog_Tests, TheDefaultLogger) {
    io::buffer out;
    auto before = slog::default_logger();
    slog::set_default(slog::logger(out, slog::level::debug));
    slog::debug("d", "k", 1);
    slog::info("i");
    slog::warn("w");
    slog::error("e");
    slog::set_default(before);
    slog::info("to stderr, not the buffer");
    auto text = text_of(out);
    EXPECT_EQ(lines_in(text), 4u);
    EXPECT_NE(text.find("level=DEBUG msg=d k=1\n"), std::string::npos);
}

TEST(Slog_Tests, ALineMadeWhileAnotherIsMade) {
    // a value whose text logs: the inner record has lines of its own
    struct Loud {
        const slog::logger* lg;

        string to_text() const {
            lg->info("inner", "k", 1);
            return string("loud");
        }
    };
    io::buffer out;
    auto lg = slog::logger(out);
    lg.info("outer", "v", Loud{&lg});
    auto text = text_of(out);
    EXPECT_EQ(lines_in(text), 2u);
    EXPECT_NE(text.find("msg=inner k=1\n"), std::string::npos);
    EXPECT_NE(text.find("msg=outer v=loud\n"), std::string::npos);
}

TEST(Slog_Tests, SamplingKeepsTheFirstThenEveryNth) {
    slog_test::FixedTime at(1790604301123456789);
    io::buffer out;
    auto lg = slog::logger(slog::options{.out = out, .utc = true, .sample_first = 2, .sample_then = 3, .sample_per = 1s});
    for (int i = 0; i < 10; ++i) {
        lg.info("same", "i", i);
    }
    lg.info("other");
    auto text = text_of(out);
    // 1, 2, then 5 and 8 of the ten; "other" counted apart
    EXPECT_EQ(lines_in(text), 5u);
    EXPECT_NE(text.find("msg=same i=4\n"), std::string::npos);
    EXPECT_NE(text.find("msg=same i=7\n"), std::string::npos);
    EXPECT_EQ(text.find("msg=same i=2\n"), std::string::npos);
    // the next window: the six left out said first, then the record
    out.clear();
    at.clock.advance(1s);
    lg.info("same", "i", 10);
    text = text_of(out);
    EXPECT_EQ(lines_in(text), 2u);
    EXPECT_NE(text.find("level=WARN msg=\"records sampled out\" count=6\n"), std::string::npos) << text;
    EXPECT_NE(text.find("msg=same i=10\n"), std::string::npos);
}

TEST(Slog_Tests, ManyAttributesAndDeepGroups) {
    io::buffer out;
    auto lg = slog::logger(slog::options{.out = out, .json = true});
    lg.info("m", "a", 1, "b", 2, "c", 3, "d", 4, "e", 5, "f", 6, "g", 7, "h", 8, "i", 9, "j", 10, "k", 11, "l", 12, "m", 13,
            "n", 14, "o", 15, "p", 16, "q", 17, "r", 18, "s", 19, "t", 20);
    auto json = encoding::json::parse(string(text_of(out)));
    ASSERT_TRUE(json.has_value());
    EXPECT_EQ((*json)["t"].as_int(), 20);
    out.clear();
    auto deep = lg;
    for (int i = 0; i < 20; ++i) {
        deep = deep.group("g").with("i", i);
    }
    deep.info("m", "last", true);
    json = encoding::json::parse(string(text_of(out)));
    ASSERT_TRUE(json.has_value()) << text_of(out);
    auto node = (*json)["g"];
    for (int i = 1; i < 20; ++i) {
        node = node["g"];
    }
    EXPECT_EQ(node["i"].as_int(), 19);
    EXPECT_TRUE(node["last"].as_bool());
}
