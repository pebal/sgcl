//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// slog::rotating_file: opened for appending (the size of what is there),
// rotated by size before the write that would pass it (no line split,
// every byte in one file, in order), by a cron's times under a manual
// clock, by hand; the names and their order; keep; the gzip of a rotated
// file and its bytes back; reopen after an outside move, at SIGHUP too;
// close and the writes after it; an open that fails; a logger over it;
// eight threads writing through rotations, every line once, whole.
#include "tests/types.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace {
    using namespace std::chrono_literals;

    struct TempDir {
        string path;

        TempDir() : path(io::make_temp_dir({}, "rotating-*").value()) {}

        ~TempDir() {
            (void)io::remove_all(path);
        }

        string operator/(const char* name) const {
            return io::path::join(path, string(name));
        }
    };

    std::vector<std::string> names(const string& dir) {
        std::vector<std::string> out;
        auto entries = io::read_dir(dir);
        for (auto& e : *entries) {
            out.push_back(std::string(e.name.data(), e.name.size()));
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    std::string text_of(const string& path) {
        auto b = io::read_file(path);
        if (!b) {
            return "<none>";
        }
        std::string_view n(path.data(), path.size());
        if (n.size() > 3 && n.substr(n.size() - 3) == ".gz") {
            auto d = compress::gzip::decompress(*b);
            return std::string(reinterpret_cast<const char*>(d->data()), d->size());
        }
        return std::string(reinterpret_cast<const char*>(b->data()), b->size());
    }

    // Every file of the log, oldest first by the library's own order
    std::string all_text(const string& dir) {
        std::string out;
        std::vector<std::string> ns = names(dir);
        std::vector<std::string> rotated, current;
        for (auto& n : ns) {
            (n == "app.log" ? current : rotated).push_back(n);
        }
        auto key = [](const std::string& n) {
            std::string stamp = n.substr(4, 23);
            size_t k = 0;
            if (n.size() > 27 && n[27] == '-') {
                k = std::stoul(n.substr(28));
            }
            return std::pair(stamp, k);
        };
        std::sort(rotated.begin(), rotated.end(), [&](auto& a, auto& b) { return key(a) < key(b); });
        for (auto& n : rotated) {
            out += text_of(io::path::join(dir, string(n.c_str())));
        }
        for (auto& n : current) {
            out += text_of(io::path::join(dir, string(n.c_str())));
        }
        return out;
    }

    void wait_for(auto&& done) {
        for (int i = 0; i < 2000 && !done(); ++i) {
            std::this_thread::sleep_for(5ms);
        }
    }
}

// Opened for appending: what is there stays, its size counted
TEST(RotatingFile_Test, OpensForAppending) {
    TempDir d;
    ASSERT_TRUE(io::write_file(d / "app.log", string("old\n")));
    auto f = slog::rotating_file::open(d / "app.log");
    ASSERT_TRUE(f);
    EXPECT_EQ(f->size(), 4u);
    EXPECT_EQ(f->path(), d / "app.log");
    ASSERT_TRUE(f->write(string("new\n")));
    EXPECT_EQ(f->size(), 8u);
    EXPECT_EQ(text_of(d / "app.log"), "old\nnew\n");
    auto bad = slog::rotating_file::open(d / "no/such/dir/app.log");
    ASSERT_FALSE(bad);
    EXPECT_TRUE(bad.error().is_not_found());
    // no constructor from a path: open is the one way, its error a value
    static_assert(!std::is_constructible_v<slog::rotating_file, string>);
    static_assert(!std::is_constructible_v<slog::rotating_file, string, slog::rotation>);
    async::scheduler::stop();
}

// By size: rotated before the write that would pass max_size, every line
// in one file, in order; the names
TEST(RotatingFile_Test, BySize) {
    TempDir d;
    slog::rotating_file f = slog::rotating_file::open(d / "app.log", {.max_size = 100, .keep = 0}).value();
    std::string want;
    for (int i : range(50)) {
        std::string line = "line " + std::to_string(i) + " xxxxxxxxxx\n";   // 17 or 18 bytes
        want += line;
        ASSERT_TRUE(f.write(string(line.c_str())));
        EXPECT_LE(f.size(), 100u);
    }
    std::vector<std::string> ns = names(d.path);
    EXPECT_GE(ns.size(), 9u);
    for (auto& n : ns) {
        if (n == "app.log") {
            continue;
        }
        EXPECT_EQ(n.substr(0, 4), "app-");
        EXPECT_EQ(n.substr(n.size() - 4), ".log");
        EXPECT_EQ(n[8], '-');
        EXPECT_EQ(n[14], 'T');
        EXPECT_EQ(n[23], '.');
        auto info = io::stat(io::path::join(d.path, string(n.c_str())));
        EXPECT_LE(info->size, 100u);
    }
    EXPECT_EQ(all_text(d.path), want);
    // a line longer than the limit goes alone
    std::string big(300, 'z');
    ASSERT_TRUE(f.write(string(big.c_str())));
    EXPECT_EQ(f.size(), 300u);
    ASSERT_TRUE(f.write(string("after\n")));
    EXPECT_EQ(text_of(d / "app.log"), "after\n");
    async::scheduler::stop();
}

// keep: the oldest removed; compress: gzipped, the bytes back
TEST(RotatingFile_Test, KeepAndCompress) {
    TempDir d;
    slog::rotating_file f = slog::rotating_file::open(d / "app.log", {.max_size = 50, .keep = 3, .compress = true}).value();
    std::string want;
    for (int i : range(40)) {
        std::string line = "entry " + std::to_string(i) + "\n";
        want += line;
        ASSERT_TRUE(f.write(string(line.c_str())));
    }
    ASSERT_TRUE(f.rotate());
    wait_for([&] {
        auto ns = names(d.path);
        return ns.size() == 4 && std::all_of(ns.begin(), ns.end(), [](auto& n) { return n == "app.log" || n.ends_with(".log.gz"); });
    });
    std::vector<std::string> ns = names(d.path);
    ASSERT_EQ(ns.size(), 4u);   // three rotated and the current
    std::string kept = all_text(d.path);
    EXPECT_TRUE(want.ends_with(kept));   // the newest three files: the end of what was written
    EXPECT_GT(kept.size(), 0u);
    EXPECT_EQ(text_of(d / "app.log"), "");
    async::scheduler::stop();
}

// By a cron's times, under a manual clock that moves the wall clock too
TEST(RotatingFile_Test, ByCron) {
    TempDir d;
    async::manual_clock clock;
    clock.install();
    int64_t s = time::now().unix();
    clock.advance(std::chrono::seconds(60 - s % 60 + 1));   // a second into a minute: the steps below stay where they say
    slog::rotating_file f = slog::rotating_file::open(d / "app.log", {.max_size = 0, .at = time::cron("* * * * *", time::zone::utc()), .keep = 0}).value();
    ASSERT_TRUE(f.write(string("first\n")));
    ASSERT_TRUE(f.write(string("still first\n")));
    EXPECT_EQ(names(d.path).size(), 1u);
    clock.advance(60s);
    ASSERT_TRUE(f.write(string("second\n")));
    EXPECT_EQ(names(d.path).size(), 2u);
    EXPECT_EQ(text_of(d / "app.log"), "second\n");
    clock.advance(10s);
    ASSERT_TRUE(f.write(string("same minute\n")));
    clock.advance(60s);
    ASSERT_TRUE(f.write(string("third\n")));
    EXPECT_EQ(names(d.path).size(), 3u);
    EXPECT_EQ(all_text(d.path), "first\nstill first\nsecond\nsame minute\nthird\n");
    clock.uninstall();
    async::scheduler::stop();
}

// reopen after an outside move; close and the writes after it
TEST(RotatingFile_Test, ReopenAndClose) {
    TempDir d;
    slog::rotating_file f = slog::rotating_file::open(d / "app.log", {.max_size = 0}).value();
    ASSERT_TRUE(f.write(string("before\n")));
    ASSERT_TRUE(io::rename(d / "app.log", d / "moved.log"));
    ASSERT_TRUE(f.write(string("still the moved one\n")));   // the descriptor goes on
    ASSERT_TRUE(f.reopen());
    EXPECT_EQ(f.size(), 0u);
    ASSERT_TRUE(f.write(string("after\n")));
    EXPECT_EQ(text_of(d / "moved.log"), "before\nstill the moved one\n");
    EXPECT_EQ(text_of(d / "app.log"), "after\n");
    ASSERT_TRUE(f.close());
    ASSERT_TRUE(f.close());   // a second does nothing
    auto w = f.write(string("late\n"));
    ASSERT_FALSE(w);
    EXPECT_TRUE(w.error().is_closed());
    EXPECT_FALSE(f.rotate());
    EXPECT_FALSE(f.reopen());
    slog::rotating_file g = f;
    EXPECT_TRUE(g == f);
    async::scheduler::stop();
}

// SIGHUP reopens the path
TEST(RotatingFile_Test, Sighup) {
    TempDir d;
    slog::rotating_file f = slog::rotating_file::open(d / "app.log", {.max_size = 0, .reopen_on_sighup = true}).value();
    ASSERT_TRUE(f.write(string("one\n")));
    ASSERT_TRUE(io::rename(d / "app.log", d / "app.log.1"));
    std::raise(SIGHUP);   // the handler in place since open
    wait_for([&] { return io::exists(d / "app.log"); });
    ASSERT_TRUE(io::exists(d / "app.log"));
    ASSERT_TRUE(f.write(string("two\n")));
    EXPECT_EQ(text_of(d / "app.log"), "two\n");
    EXPECT_EQ(text_of(d / "app.log.1"), "one\n");
    ASSERT_TRUE(f.close());
    async::reset_signals({SIGHUP});
    async::scheduler::stop();
}

// A logger over it
TEST(RotatingFile_Test, UnderALogger) {
    TempDir d;
    slog::rotating_file out = slog::rotating_file::open(d / "app.log", {.max_size = 200, .keep = 0}).value();
    slog::logger log(slog::options{.out = out, .json = true});
    for (int i : range(20)) {
        log.info("tick", "i", i);
    }
    std::string text = all_text(d.path);
    size_t lines = std::count(text.begin(), text.end(), '\n');
    EXPECT_EQ(lines, 20u);
    EXPECT_NE(text.find("\"i\":19"), std::string::npos);
    EXPECT_GE(names(d.path).size(), 5u);
    async::scheduler::stop();
}

// Eight threads writing through rotations: every line once, whole
TEST(RotatingFile_Test, ManyThreads) {
    TempDir d;
    slog::rotating_file f = slog::rotating_file::open(d / "app.log", {.max_size = 4096, .keep = 0}).value();
    std::vector<std::thread> ts;
    std::atomic<int> failures{0};
    for (int t : range(8)) {
        ts.emplace_back([&, t] {
            slog::rotating_file mine = f;   // on this thread's stack
            for (int i : range(500)) {
                std::string line = "t" + std::to_string(t) + " i" + std::to_string(i) + "\n";
                if (!mine.write(string(line.c_str()))) {
                    ++failures;
                }
            }
        });
    }
    for (auto& t : ts) {
        t.join();
    }
    EXPECT_EQ(failures.load(), 0);
    std::string text = all_text(d.path);
    std::map<std::string, int> seen;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            ++seen[text.substr(start, i - start)];
            start = i + 1;
        }
    }
    EXPECT_EQ(seen.size(), 4000u);
    for (auto& [line, n] : seen) {
        ASSERT_EQ(n, 1) << line;
        ASSERT_EQ(line[0], 't');
    }
    EXPECT_GT(names(d.path).size(), 5u);
    async::scheduler::stop();
}
