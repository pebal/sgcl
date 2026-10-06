//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The slog module: what a record costs, one case a run (benchmarks/go/slog
// has the Go side, the same records through log/slog). Prints one line of
// named fields: case=<name> impl=sgcl ns=<per record> allocs=<per record>
// (allocs: the program's operator new, plain memory; managed objects are
// tests/slog/heap.cpp's).
//
//   slog <case> [path]
//
//   text_info3          info with three attributes (a text, an int, a duration), text, to io::discard
//   text_disabled       a debug record under a logger at info
//   text_with5          a logger with five attributes of with(), info with one more
//   json_info3          text_info3 as JSON
//   json_described      JSON, a type described by three fields (Go: slog.Group of the same three)
//   text_buffered       text_info3 through options::buffered, to io::discard
//   text_file           text_info3 to a file opened for appending (path)
//   text_file_buffered  the same through options::buffered
//   syslog_unix         text_info3 through slog::syslog to a datagram socket at path that a thread drains
//                       (RFC 3164, the local format; Go: log/syslog.Dial("unixgram", path) under slog's text)
//   text_rotating       text_info3 to a rotating_file at path (16 MB a file, 3 kept: rotations within the run;
//                       Go: a rotating writer of lumberjack's shape written out, a mutex, a size, a rename)
//   parallel_text       text_info3 from 8 threads at once, to io::discard: ns per record of the whole
//
// The loop runs for about two seconds after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace {
    std::atomic<uint64_t> news{0};
}

void* operator new(size_t n) {
    news.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1)) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, size_t) noexcept {
    std::free(p);
}

using namespace sgcl;

namespace {
    struct Req {
        int id = 42;
        string path = "/users/42";
        double score = 0.75;

        void describe(encoding::field_list& f) {
            f.add("id", id);
            f.add("path", path);
            f.add("score", score);
        }
    };

    // Records for about `seconds` after a warm-up: ns per record, allocs per record
    template<class F>
    void run(const char* name, F&& record, double seconds = 2.0) {
        auto t0 = bench::Clock::now();
        while (bench::seconds_since(t0) < 0.25) {
            for (int k = 0; k < 256; ++k) {
                record();
            }
        }
        uint64_t n = 0;
        uint64_t a0 = news.load();
        t0 = bench::Clock::now();
        double wall = 0;
        while (wall < seconds) {
            for (int k = 0; k < 1024; ++k) {
                record();
            }
            n += 1024;
            wall = bench::seconds_since(t0);
        }
        uint64_t a = news.load() - a0;
        std::printf("case=%s impl=sgcl ns=%.2f allocs=%.3f\n", name, wall * 1e9 / double(n), double(a) / double(n));
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: slog <case> [path]\n");
        return 2;
    }
    const std::string what = argv[1];
    const auto took = 1500 * microsecond;
    const string method = "GET";
    if (what == "text_info3") {
        auto log = slog::logger(io::discard);
        run("text_info3", [&] { log.info("request", "method", method, "status", 200, "took", took); });
    } else if (what == "text_disabled") {
        auto log = slog::logger(io::discard);
        run("text_disabled", [&] { log.debug("request", "method", method, "status", 200, "took", took); });
    } else if (what == "text_with5") {
        auto log = slog::logger(io::discard).with("service", "api", "node", "n1", "version", 3, "region", "eu-central", "tls", true);
        run("text_with5", [&] { log.info("request", "status", 200); });
    } else if (what == "json_info3") {
        auto log = slog::logger(slog::options{.out = io::discard, .json = true});
        run("json_info3", [&] { log.info("request", "method", method, "status", 200, "took", took); });
    } else if (what == "json_described") {
        auto log = slog::logger(slog::options{.out = io::discard, .json = true});
        const Req req;
        run("json_described", [&] { log.info("request", "req", req); });
    } else if (what == "text_buffered") {
        auto log = slog::logger(slog::options{.out = io::discard, .buffered = true});
        run("text_buffered", [&] { log.info("request", "method", method, "status", 200, "took", took); });
        log.flush();
    } else if (what == "text_file" || what == "text_file_buffered") {
        if (argc < 3) {
            std::fprintf(stderr, "a path for the file\n");
            return 2;
        }
        auto f = io::open(string(argv[2]), io::open_flags::write | io::open_flags::create | io::open_flags::append);
        if (!f) {
            std::fprintf(stderr, "%s\n", f.error().message().data());
            return 1;
        }
        auto log = slog::logger(slog::options{.out = *f, .buffered = (what == "text_file_buffered")});
        run(what.c_str(), [&] { log.info("request", "method", method, "status", 200, "took", took); }, 1.0);
        log.flush();
        (void)f->close();
    } else if (what == "syslog_unix") {
        if (argc < 3) {
            std::fprintf(stderr, "a path for the socket\n");
            return 2;
        }
        int fd = ::socket(AF_UNIX, SOCK_DGRAM, 0);
        sockaddr_un a{};
        a.sun_family = AF_UNIX;
        std::snprintf(a.sun_path, sizeof a.sun_path, "%s", argv[2]);
        ::unlink(argv[2]);
        if (::bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) {
            std::perror("bind");
            return 1;
        }
        std::atomic<bool> stop{false};
        std::thread drain([&] {
            char buf[4096];
            timeval tv{0, 100000};
            ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
            while (!stop.load()) {
                (void)::recv(fd, buf, sizeof buf, 0);
            }
        });
        auto h = slog::detail::SyslogAccess::at(argv[2], {.app_name = "bench", .format = slog::syslog::format::rfc3164});
        if (!h) {
            std::fprintf(stderr, "%s\n", h.error().message().data());
            return 1;
        }
        auto log = slog::logger(*h);
        run("syslog_unix", [&] { log.info("request", "method", method, "status", 200, "took", took); }, 1.0);
        stop.store(true);
        drain.join();
        ::close(fd);
        ::unlink(argv[2]);
    } else if (what == "text_rotating") {
        if (argc < 3) {
            std::fprintf(stderr, "a path for the file\n");
            return 2;
        }
        auto f = slog::rotating_file::open(string(argv[2]), {.max_size = 16 << 20, .keep = 3});
        if (!f) {
            std::fprintf(stderr, "%s\n", f.error().message().data());
            return 1;
        }
        auto log = slog::logger(slog::options{.out = *f});
        run(what.c_str(), [&] { log.info("request", "method", method, "status", 200, "took", took); }, 1.0);
        (void)f->close();
    } else if (what == "parallel_text") {
        auto log = slog::logger(io::discard);
        constexpr int Threads = 8;
        std::atomic<bool> stop{false};
        std::atomic<uint64_t> total{0};
        auto t0 = bench::Clock::now();
        std::vector<std::thread> threads;
        for (int t = 0; t < Threads; ++t) {
            threads.emplace_back([&log, &stop, &total] {
                uint64_t n = 0;
                while (!stop.load(std::memory_order_relaxed)) {
                    for (int k = 0; k < 256; ++k) {
                        log.info("request", "method", "GET", "status", 200, "took", 1500 * microsecond);
                    }
                    n += 256;
                }
                total.fetch_add(n);
            });
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        stop.store(true);
        for (auto& t : threads) {
            t.join();
        }
        double wall = bench::seconds_since(t0);
        std::printf("case=parallel_text impl=sgcl ns=%.2f allocs=0\n", wall * 1e9 / double(total.load()));
    } else {
        std::fprintf(stderr, "no such case: %s\n", what.c_str());
        return 2;
    }
    return 0;
}
