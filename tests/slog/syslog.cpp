//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// slog::syslog and slog::journald: RFC 5424 messages checked field by
// field against the ABNF (PRI, VERSION, TIMESTAMP, HOSTNAME, APP-NAME,
// PROCID, MSGID, STRUCTURED-DATA and its escapes, MSG with slog's text of
// the attributes), RFC 3164 messages, the severities of every level and
// the facilities, octet counting, a datagram server standing in for the
// local daemon and for journald (the journal's native fields, the binary
// form of a value with a newline, the names upper-cased), the system's
// own syslog socket when there is one, a writer that fails (dropped), and
// many threads at once over one stream.
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {
    // A datagram socket bound at a path of its own: the daemon's stand-in
    struct Daemon {
        int fd = -1;
        std::string path;

        Daemon() {
            char tmpl[] = "/tmp/sgcl-syslog-XXXXXX";
            path = std::string(mkdtemp(tmpl)) + "/s";
            fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
            sockaddr_un a{};
            a.sun_family = AF_UNIX;
            std::snprintf(a.sun_path, sizeof a.sun_path, "%s", path.c_str());
            EXPECT_EQ(::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof a), 0);
            timeval tv{2, 0};
            ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        }

        ~Daemon() {
            ::close(fd);
            ::unlink(path.c_str());
            ::rmdir(path.substr(0, path.size() - 2).c_str());
        }

        std::string receive() const {
            char buf[65536];
            ssize_t n = ::recv(fd, buf, sizeof buf, 0);
            return n > 0 ? std::string(buf, size_t(n)) : std::string();
        }
    };

    std::string text(const io::buffer& b) {
        string t = b.text();
        return std::string(t.data(), t.size());
    }

    const std::regex Rfc5424(R"(^<(\d{1,3})>1 (\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{6}(?:Z|[+-]\d\d:\d\d)) (\S{1,255}) (\S{1,48}) (\d{1,128}) (\S{1,32}) (-|\[.*\]) (.*)$)");
}

// RFC 5424: every field, the message and slog's text of the attributes
TEST(Syslog_Test, Rfc5424) {
    io::buffer b;
    slog::logger log(slog::syslog(io::writer(b), {.facility = slog::syslog::facility::local3, .app_name = "api", .hostname = "web-1"}));
    log.warn("disk almost full", "free", 1024, "path", "/var/data", slog::group("req", "id", 7));
    std::string m = text(b);
    std::smatch g;
    ASSERT_TRUE(std::regex_match(m, g, Rfc5424)) << m;
    EXPECT_EQ(g[1], std::to_string(19 * 8 + 4));
    EXPECT_EQ(g[3], "web-1");
    EXPECT_EQ(g[4], "api");
    EXPECT_EQ(g[5], std::to_string(io::pid()));
    EXPECT_EQ(g[6], "-");
    EXPECT_EQ(g[7], "-");
    EXPECT_EQ(g[8], "disk almost full free=1024 path=/var/data req.id=7");
}

// The severities of the levels, between the named ones the one below
TEST(Syslog_Test, Severities) {
    for (auto [l, sev] : std::vector<std::pair<int, int>>{{-8, 7}, {-4, 7}, {-1, 7}, {0, 6}, {2, 6}, {4, 4}, {7, 4}, {8, 3}, {12, 3}}) {
        io::buffer b;
        slog::logger log(slog::syslog(io::writer(b), {.app_name = "x", .hostname = "h"}), slog::level(-8));
        log.log(slog::level(l), "m");
        std::smatch g;
        std::string m = text(b);
        ASSERT_TRUE(std::regex_match(m, g, Rfc5424)) << m;
        EXPECT_EQ(std::stoi(g[1]), 8 + sev) << l;
    }
}

// Structured data under an SD-ID: names flattened, values escaped
TEST(Syslog_Test, StructuredData) {
    io::buffer b;
    slog::logger log(slog::syslog(io::writer(b), {.app_name = "api", .hostname = "h", .structured_data_id = "app@32473"}));
    log.error("payment failed", "order", 42, "note", "a \"quote\" \\ and ] bracket", slog::group("req", "path", "/x"));
    std::string m = text(b);
    std::smatch g;
    ASSERT_TRUE(std::regex_match(m, g, Rfc5424)) << m;
    EXPECT_EQ(g[7], R"([app@32473 order="42" note="a \"quote\" \\ and \] bracket" req.path="/x"])");
    EXPECT_EQ(g[8], "payment failed");
    io::buffer e;
    slog::logger empty(slog::syslog(io::writer(e), {.app_name = "api", .hostname = "h", .structured_data_id = "app@32473"}));
    empty.info("bare");
    EXPECT_NE(text(e).find(" [app@32473] bare"), std::string::npos);
}

// RFC 3164: the header, the tag and the pid, the hostname for a server
TEST(Syslog_Test, Rfc3164) {
    io::buffer b;
    slog::logger log(slog::syslog(io::writer(b), {.facility = slog::syslog::facility::daemon, .app_name = "cron", .hostname = "box", .format = slog::syslog::format::rfc3164}));
    log.info("job done", "n", 3);
    std::string m = text(b);
    std::regex r(R"(^<30>(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) [ 1-3]\d \d\d:\d\d:\d\d box cron\[(\d+)\]: job done n=3$)");
    std::smatch g;
    ASSERT_TRUE(std::regex_match(m, g, r)) << m;
    EXPECT_EQ(g[2], std::to_string(io::pid()));
}

// Octet counting: the length, a space, the message; several in a row
TEST(Syslog_Test, OctetCounting) {
    io::buffer b;
    slog::logger log(slog::syslog(io::writer(b), {.app_name = "a", .hostname = "h", .octet_counting = true}));
    log.info("one");
    log.info("two", "k", "v");
    std::string all = text(b);
    size_t at = 0;
    std::vector<std::string> messages;
    while (at < all.size()) {
        size_t sp = all.find(' ', at);
        size_t len = std::stoul(all.substr(at, sp - at));
        messages.push_back(all.substr(sp + 1, len));
        at = sp + 1 + len;
    }
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_TRUE(messages[0].ends_with(" - one"));
    EXPECT_TRUE(messages[1].ends_with(" - two k=v"));
    for (auto& m : messages) {
        EXPECT_TRUE(std::regex_match(m, Rfc5424)) << m;
    }
}

// The local path: a datagram a record, to the daemon standing in
TEST(Syslog_Test, LocalDatagrams) {
    Daemon d;
    auto s = slog::detail::SyslogAccess::at(d.path.c_str(), {.app_name = "svc", .format = slog::syslog::format::rfc3164});
    ASSERT_TRUE(s);
    slog::logger log(*s);
    log.warn("hello", "x", 1);
    log.error("again");
    std::string m1 = d.receive(), m2 = d.receive();
    EXPECT_TRUE(m1.starts_with("<12>")) << m1;
    EXPECT_TRUE(m1.ends_with(" svc[" + std::to_string(io::pid()) + "]: hello x=1")) << m1;
    EXPECT_EQ(m1.find(" h "), std::string::npos);   // no hostname: the daemon adds it
    EXPECT_TRUE(m2.ends_with("]: again"));
    EXPECT_EQ(s->dropped(), 0u);
    auto none = slog::detail::SyslogAccess::at("/nonexistent/socket", {});
    ASSERT_FALSE(none);
    EXPECT_TRUE(none.error().is_not_found());
    // the machine's own syslog, when it has one: a message goes
    auto system = slog::syslog::local();
    if (system) {
        slog::logger sys(*system);
        sys.debug("sgcl tests: a syslog probe");
        EXPECT_EQ(system->dropped(), 0u);
    }
}

// format::automatic, the default: RFC 3164 to the local daemon and RFC 5424
// to a writer, whatever else the options say (an app_name given alone once
// dropped local()'s default and sent RFC 5424 to the daemon); an explicit
// format is kept on either
TEST(Syslog_Test, AutomaticFormat) {
    EXPECT_EQ(slog::syslog::options().format, slog::syslog::format::automatic);
    Daemon d;
    auto local = slog::detail::SyslogAccess::at(d.path.c_str(), {.app_name = "api"});
    ASSERT_TRUE(local);
    slog::logger to_local(*local);
    to_local.info("local");
    std::string m = d.receive();
    std::regex rfc3164(R"(^<14>(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) [ 1-3]\d \d\d:\d\d:\d\d api\[\d+\]: local$)");
    EXPECT_TRUE(std::regex_match(m, rfc3164)) << m;
    auto explicit5424 = slog::detail::SyslogAccess::at(d.path.c_str(), {.app_name = "api", .hostname = "h", .format = slog::syslog::format::rfc5424});
    ASSERT_TRUE(explicit5424);
    slog::logger to_5424(*explicit5424);
    to_5424.info("local 5424");
    m = d.receive();
    EXPECT_TRUE(std::regex_match(m, Rfc5424)) << m;
    io::buffer b;
    slog::logger remote(slog::syslog(io::writer(b), {.app_name = "api", .hostname = "h"}));
    remote.info("remote");
    EXPECT_TRUE(std::regex_match(text(b), Rfc5424)) << text(b);
    io::buffer b3164;
    slog::logger remote3164(slog::syslog(io::writer(b3164), {.app_name = "api", .hostname = "h", .format = slog::syslog::format::rfc3164}));
    remote3164.info("remote 3164");
    EXPECT_TRUE(text(b3164).starts_with("<14>")) << text(b3164);
    EXPECT_NE(text(b3164).find(" h api["), std::string::npos) << text(b3164);
}

namespace {
    struct Failing {
        expected<size_t, io::error> write(const slice<const byte>&) {
            return unexpected(io::error(io::errc::closed, "write"));
        }
    };
}

// A writer that fails: counted, nothing thrown
TEST(Syslog_Test, Dropped) {
    Failing f;
    slog::syslog h(io::writer(f), {.app_name = "a", .hostname = "h"});
    slog::logger log(h);
    log.info("lost");
    log.info("lost too");
    EXPECT_EQ(h.dropped(), 2u);
    slog::syslog same = h;
    EXPECT_TRUE(same == h);
}

// Many threads over one stream: every message whole
TEST(Syslog_Test, ManyThreads) {
    io::buffer b;
    slog::syslog h(io::writer(b), {.app_name = "a", .hostname = "h", .octet_counting = true});
    std::vector<std::thread> ts;
    for (int t : range(8)) {
        ts.emplace_back([&, t] {
            slog::logger log(h);
            for (int i : range(200)) {
                log.info("m", "t", t, "i", i);
            }
        });
    }
    for (auto& t : ts) {
        t.join();
    }
    std::string all = text(b);
    size_t at = 0, count = 0;
    while (at < all.size()) {
        size_t sp = all.find(' ', at);
        size_t len = std::stoul(all.substr(at, sp - at));
        ASSERT_TRUE(std::regex_match(all.substr(sp + 1, len), Rfc5424));
        at = sp + 1 + len;
        ++count;
    }
    EXPECT_EQ(count, 1600u);
}

namespace {
    // The journal's fields of a datagram: KEY=value lines and the binary form
    std::vector<std::pair<std::string, std::string>> fields(const std::string& d) {
        std::vector<std::pair<std::string, std::string>> out;
        size_t at = 0;
        while (at < d.size()) {
            size_t nl = d.find('\n', at);
            size_t eq = d.find('=', at);
            if (eq != std::string::npos && eq < nl) {
                out.emplace_back(d.substr(at, eq - at), d.substr(eq + 1, nl - eq - 1));
                at = nl + 1;
            } else {
                std::string key = d.substr(at, nl - at);
                uint64_t n = 0;
                for (int i = 0; i < 8; ++i) {
                    n |= uint64_t(uint8_t(d[nl + 1 + i])) << (8 * i);
                }
                out.emplace_back(key, d.substr(nl + 9, n));
                at = nl + 9 + n + 1;
            }
        }
        return out;
    }
}

// journald: the native fields, names upper-cased, the binary form
TEST(Journald_Test, NativeProtocol) {
    Daemon d;
    auto j = slog::detail::SyslogAccess::journal_at(d.path.c_str(), "myapp");
    ASSERT_TRUE(j);
    slog::logger log(slog::options{.handler = *j, .source = true});
    log.warn("two\nlines", "user-id", 7, "path", "/a", slog::group("req", "method", "GET"), "_hidden", 1, "9lives", 2);
    auto f = fields(d.receive());
    std::map<std::string, std::string> m(f.begin(), f.end());
    EXPECT_EQ(m["MESSAGE"], "two\nlines");
    EXPECT_EQ(m["PRIORITY"], "4");
    EXPECT_EQ(m["SYSLOG_IDENTIFIER"], "myapp");
    EXPECT_EQ(m["SYSLOG_PID"], std::to_string(io::pid()));
    EXPECT_TRUE(m["CODE_FILE"].ends_with("syslog.cpp"));
    EXPECT_FALSE(m["CODE_LINE"].empty());
    EXPECT_EQ(m["USER_ID"], "7");
    EXPECT_EQ(m["PATH"], "/a");
    EXPECT_EQ(m["REQ_METHOD"], "GET");
    EXPECT_EQ(m["X_HIDDEN"], "1");
    EXPECT_EQ(m["X9LIVES"], "2");
    EXPECT_EQ(j->dropped(), 0u);
    auto real = slog::journald::open();
#if defined(__linux__)
    (void)real;   // a system with systemd has it; one without (a container) does not
#else
    EXPECT_FALSE(real);   // no journald here
#endif
}
