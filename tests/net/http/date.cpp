//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::http: the Date line of the server's heads (response_writer.h:
// append_date_line), made once a second and kept in a sequence lock. Threads
// read it while the second changes under them, many times over: every line
// is whole, the date of one second the clock passed through, never a mix of
// two seconds' words. The time is time::now()'s, so the manual clock drives
// the seconds; a run on the real clock across one boundary as well. Run
// under the thread sanitizer too.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <atomic>
#include <chrono>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    // The second of a Date line, or INT64_MIN when it is not one whole
    int64_t second_of(const std::string& line) {
        if (line.size() < 8 || line.compare(0, 6, "Date: ") != 0 || line.compare(line.size() - 2, 2, "\r\n") != 0) {
            return INT64_MIN;
        }
        auto t = time::detail::parse_http(std::string_view(line).substr(6, line.size() - 8));
        return t ? t->unix() : INT64_MIN;
    }

    // Readers on n threads until stop, each line checked against the seconds
    // the clock may show; the count of lines read, and of lines that were
    // not one of them
    struct Readers {
        std::atomic<bool> stop = {false};
        std::atomic<long> lines = {0};
        std::atomic<long> bad = {0};
        std::vector<std::thread> threads;

        Readers(int n, int64_t first, int64_t last) {
            for (int i = 0; i < n; ++i) {
                threads.emplace_back([this, first, last] {
                    std::string head;
                    while (!stop.load(std::memory_order_relaxed)) {
                        head.clear();
                        net::http::detail::append_date_line(head);
                        auto s = second_of(head);
                        lines.fetch_add(1, std::memory_order_relaxed);
                        if (s < first || s > last) {
                            bad.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                });
            }
        }

        void join() {
            stop = true;
            for (auto& t : threads) {
                t.join();
            }
        }
    };
}

// The manual clock moved a second at a time, a hundred times, under eight
// readers, the writer of each new line held half way through its stores
// (the test hook): every line whole, of a second the clock was at
TEST(HttpDate_Tests, EveryLineIsOneSecondsWholeAcrossTheChanges) {
    async::manual_clock clock;
    clock.install();
    net::http::detail::date_line_test_hook.store([] { std::this_thread::sleep_for(50us); });
    int64_t first = time::now().unix();
    Readers readers(8, first, first + 100);
    for (int i = 0; i < 100; ++i) {
        std::this_thread::sleep_for(200us);
        clock.advance(1s);
    }
    std::this_thread::sleep_for(1ms);
    readers.join();
    net::http::detail::date_line_test_hook.store(nullptr);
    EXPECT_GT(readers.lines.load(), 1000);
    EXPECT_EQ(readers.bad.load(), 0);
    std::string head;
    net::http::detail::append_date_line(head);
    EXPECT_EQ(second_of(head), first + 100);                    // the line follows the clock
}

// The real clock across one boundary: the lines of the two seconds only
TEST(HttpDate_Tests, TheRealClockAcrossASecond) {
    int64_t first = time::now().unix();
    while (time::now().unix() == first) {                        // to the start of the next second
        std::this_thread::sleep_for(1ms);
    }
    first = time::now().unix();
    auto until = std::chrono::steady_clock::now() + 1100ms;      // across the next boundary
    Readers readers(4, first, first + 2);
    while (std::chrono::steady_clock::now() < until) {
        std::this_thread::sleep_for(5ms);
    }
    readers.join();
    EXPECT_GT(readers.lines.load(), 1000);
    EXPECT_EQ(readers.bad.load(), 0);
}
