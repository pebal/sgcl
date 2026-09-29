//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The resolver's wait with its task destroyed (tests/async/dropped_waits.cpp
// for the other primitives): a lookup runs the system's resolver on the
// blocking pool and waits for the job (net::detail::await_job), plainly
// or in a select with the caller's stop and deadline. Here the job is held
// until released, so that the task that waits for it is dropped first; the
// frame's locals must be destroyed once (the task runs on to its end:
// DESIGN 302).
#include "tests/types.h"
#include "sgcl/net/net.h"

#include <atomic>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace {
    std::atomic<int> destroyed = 0;
    std::atomic<bool> entered = false;
    std::atomic<bool> released = false;

    struct Guard {
        ~Guard() {
            ++destroyed;
        }
    };

    // A lookup's wait, the job held until released
    sgcl::async::task<> lookup(sgcl::async::stop_token stop, bool with_deadline) {
        Guard g;
        entered = true;
        auto job = sgcl::async::spawn_blocking([]() -> expected<int, io::error> {
            while (!released) {
                std::this_thread::sleep_for(1ms);
            }
            return 1;
        });
        (void)co_await sgcl::net::detail::await_job(std::move(job), stop, with_deadline ? sgcl::clock::now() + 10s : sgcl::time_point(), sgcl::string("held"));
    }

    void dropped(bool with_stop, bool with_deadline) {
        destroyed = 0;
        entered = false;
        released = false;
        sgcl::async::stop_source src;
        {
            auto t = sgcl::async::spawn(lookup(with_stop ? src.token() : sgcl::async::stop_token(), with_deadline));
            for (int i = 0; i < 2000 && !entered; ++i) {
                std::this_thread::sleep_for(1ms);
            }
            std::this_thread::sleep_for(20ms);
            ASSERT_TRUE(entered);
        }
        released = true;
        std::this_thread::sleep_for(100ms);
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
        EXPECT_EQ(destroyed.load(), 1) << "the locals destroyed " << destroyed.load() << " times";
    }
}

TEST(DroppedWaits_Tests, DnsPlainWait) {
    dropped(false, false);
}

TEST(DroppedWaits_Tests, DnsSelectWithStopAndDeadline) {
    dropped(true, true);
}
