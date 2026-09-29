//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Many threads logging into one file: tasks on every worker and threads
// of their own, through one logger and the loggers made from it, the
// default one swapped meanwhile; unbuffered (a record is one write, the
// file opened for appending) and buffered (per worker batches, a warn now
// and then, flush at the end). Every line comes out whole and exactly
// once. Run under TSan as well (DESIGN 283).
#include "common.h"

#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {
    constexpr int Tasks = 8;
    constexpr int Threads = 3;
    constexpr int Lines = 1500;

    struct SlogThreads_Tests : testing::Test {
        std::string _dir;

        void SetUp() override {
            auto d = io::make_temp_dir({}, "sgcl-slog-*");
            ASSERT_TRUE(d) << d.error().message();
            _dir = std::string(d->data(), d->size());
        }

        void TearDown() override {
            (void)io::remove_all(string(_dir));
        }

        string path() const {
            return string(_dir + "/log.txt");
        }
    };

    // Every line of the file whole: `... msg=line who=<w> i=<i> pad=<pad>`,
    // each (who, i) once
    void check(const std::string& text, int writers) {
        std::set<std::pair<int, int>> seen;
        size_t start = 0;
        const std::string pad(40, 'p');
        while (start < text.size()) {
            size_t end = text.find('\n', start);
            ASSERT_NE(end, std::string::npos) << "a line without its end";
            std::string line = text.substr(start, end - start);
            start = end + 1;
            ASSERT_EQ(line.rfind("time=", 0), 0u) << line;
            if (line.find("msg=line ") == std::string::npos) {
                continue;
            }
            int who = -1, i = -1;
            auto w = line.find(" who=");
            auto at = line.find(" i=");
            auto p = line.find(" pad=");
            ASSERT_TRUE(w != std::string::npos && at != std::string::npos && p != std::string::npos) << line;
            who = std::stoi(line.substr(w + 5));
            i = std::stoi(line.substr(at + 3));
            ASSERT_EQ(line.substr(p + 5), pad) << line;
            EXPECT_TRUE(seen.insert({who, i}).second) << "twice: " << line;
        }
        EXPECT_EQ(seen.size(), size_t(writers) * Lines);
    }

    void write_all(const slog::logger& lg) {
        const std::string pad(40, 'p');
        vector<async::task<>> tasks;
        for (int t = 0; t < Tasks; ++t) {
            tasks.push_back(async::spawn([lg, t, pad]() -> async::task<> {
                auto mine = lg.with("worker", true);
                for (int i = 0; i < Lines; ++i) {
                    if (i % 2) {
                        lg.info("line", "who", t, "i", i, "pad", pad);
                    } else {
                        mine.info("line", "who", t, "i", i, "pad", pad);
                    }
                    if (i % 100 == 0) {
                        lg.warn("mark", "who", t);
                        co_await async::sleep(0ms);
                    }
                }
            }));
        }
        std::vector<std::thread> threads;
        for (int t = 0; t < Threads; ++t) {
            threads.emplace_back([&lg, t, pad] {   // by reference: a std::thread's closure lies in plain memory
                for (int i = 0; i < Lines; ++i) {
                    lg.info("line", "who", Tasks + t, "i", i, "pad", pad);
                    if (i % 300 == 0) {
                        slog::set_default(lg.with("default", t));
                        slog::info("default");
                    }
                }
            });
        }
        for (auto& t : tasks) {
            t.wait();
        }
        for (auto& t : threads) {
            t.join();
        }
    }
}

TEST_F(SlogThreads_Tests, OneWritePerRecord) {
    auto before = slog::default_logger();
    {
        auto f = io::open(path(), io::open_flags::write | io::open_flags::create | io::open_flags::append);
        ASSERT_TRUE(f);
        write_all(slog::logger(*f));
        slog::set_default(before);
        ASSERT_TRUE(f->close());
    }
    auto text = io::read_text(path());
    ASSERT_TRUE(text);
    check(std::string(text->data(), text->size()), Tasks + Threads);
}

TEST_F(SlogThreads_Tests, Buffered) {
    auto before = slog::default_logger();
    {
        auto f = io::open(path(), io::open_flags::write | io::open_flags::create | io::open_flags::append);
        ASSERT_TRUE(f);
        auto lg = slog::logger(slog::options{.out = *f, .buffered = true});
        write_all(lg);
        slog::set_default(before);
        lg.flush();
        ASSERT_TRUE(f->close());
    }
    auto text = io::read_text(path());
    ASSERT_TRUE(text);
    check(std::string(text->data(), text->size()), Tasks + Threads);
}
