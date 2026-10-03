//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// go_blocking: a blocking call on the pool with no handle. It runs to its end
// with nobody waiting, what it returns is dropped, what it throws goes to
// on_unhandled's handler once, on the pool's thread (the default prints one
// line naming the pool and ends the program: death tests); the call throws
// as spawn_blocking's does (a move of F, a pool that cannot start a thread
// and has none), and the end of the program runs the jobs still queued, as
// it does a spawn_blocking whose handle was dropped.
#include "tests/types.h"
#include "sgcl/async/async.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <type_traits>

using namespace std::chrono_literals;

namespace go_blocking_test {
    struct custom_error : std::runtime_error {
        using std::runtime_error::runtime_error;
    };
}

namespace {
    std::atomic<int> calls = 0;
    std::atomic<int> calls_off_the_pool = 0;   // the handler called on a thread that is not the pool's
    std::mutex what_lock;
    std::string last_what;

    void record(std::exception_ptr e) {
        if (sgcl::async::detail::thread_place != sgcl::async::detail::PlaceBlockingPool) {
            ++calls_off_the_pool;
        }
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& x) {
            std::lock_guard l(what_lock);
            last_what = x.what();
        } catch (...) {
            std::lock_guard l(what_lock);
            last_what = "?";
        }
        ++calls;
    }

    std::string what() {
        std::lock_guard l(what_lock);
        return last_what;
    }

    struct Node {
        explicit Node(int v) : value(v) { ++alive; }
        ~Node() { value = -1; --alive; }
        int value;
        inline static sgcl::atomic<int> alive = {0};
    };

    SGCL_ALWAYS_INLINE void settle() {
        sgcl::collector::clear_stack();
        for (int i = 0; i < 3; ++i) {
            sgcl::collector::force_collect(true);
        }
    }

    void fail_thread_start() {
        throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again), "thread");
    }

    // A callable whose copy succeeds and whose move throws once armed: the
    // move into the job is the one go_blocking makes
    struct ThrowingMove {
        explicit ThrowingMove(std::atomic<bool>& ran) : ran(&ran) {}
        ThrowingMove(const ThrowingMove&) = default;
        ThrowingMove(ThrowingMove&& other) : ran(other.ran) {
            if (armed) {
                throw std::length_error("move");
            }
        }
        void operator()() const { *ran = true; }
        std::atomic<bool>* ran;
        inline static bool armed = false;
    };

    struct GoBlocking_Tests : ::testing::Test {
        void SetUp() override {
            calls = 0;
            calls_off_the_pool = 0;
            last_what.clear();
            _previous = sgcl::async::on_unhandled(&record);
        }

        void TearDown() override {
            sgcl::async::blocking_pool::wait_idle();
            sgcl::async::on_unhandled(_previous);
            sgcl::async::blocking_pool::set_threads(0);
            sgcl::async::blocking_pool::stop();
        }

        void (*_previous)(std::exception_ptr) = nullptr;
    };
}

TEST_F(GoBlocking_Tests, RunsToTheEndWithNobodyWaiting) {
    std::atomic<int> ran = {0};
    sgcl::async::go_blocking([&] {
        EXPECT_FALSE(sgcl::async::scheduler::on_worker());   // a thread of the pool
        std::this_thread::sleep_for(20ms);
        ++ran;
    });
    EXPECT_EQ(ran.load(), 0);                                // the call returned before the job's end
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_EQ(ran.load(), 1);
    // from a task on a worker: the call does not wait for the job
    sgcl::async::spawn([](std::atomic<int>& ran) -> sgcl::async::task<> {
        sgcl::async::go_blocking([&] { ++ran; });
        co_return;
    }(ran)).wait();
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_EQ(ran.load(), 2);
    EXPECT_EQ(calls.load(), 0);
    sgcl::async::scheduler::stop();
}

TEST_F(GoBlocking_Tests, TheClosureMayCaptureATrackedPtr) {
    settle();
    const int before = Node::alive.load();
    std::atomic<bool> release = {false};
    std::atomic<int> seen = {0};
    off_frame([&] {
        sgcl::tracked_ptr node = sgcl::make_tracked<Node>(8);
        sgcl::async::go_blocking([node, &release, &seen] {   // the closure lives in the job, a managed object
            while (!release.load()) {
                std::this_thread::sleep_for(1ms);
            }
            seen = node->value;
        });
    });
    settle();                                                // while the job runs: the node held by the closure alone
    EXPECT_EQ(Node::alive.load(), before + 1);
    release = true;
    sgcl::async::blocking_pool::stop();
    settle();
    EXPECT_EQ(seen.load(), 8);
    EXPECT_EQ(Node::alive.load(), before);                   // the job gone, the closure and its node with it
}

TEST_F(GoBlocking_Tests, WhatItThrowsReachesTheHandlerOnce) {
    sgcl::async::go_blocking([] { throw go_blocking_test::custom_error("from the pool"); });
    sgcl::async::blocking_pool::wait_idle();                 // the handler is called before the job counts as run
    EXPECT_EQ(calls.load(), 1);
    EXPECT_EQ(what(), "from the pool");
    EXPECT_EQ(calls_off_the_pool.load(), 0);                 // on the thread of the pool that ran it
    std::this_thread::sleep_for(50ms);
    settle();
    EXPECT_EQ(calls.load(), 1);                              // once: the job collected calls nothing more
    // an exception not derived from std::exception
    sgcl::async::go_blocking([] { throw 42; });
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_EQ(calls.load(), 2);
    EXPECT_EQ(what(), "?");
    // a function returning a value that throws instead
    sgcl::async::go_blocking([]() -> int { throw std::logic_error("no value"); });
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_EQ(calls.load(), 3);
    EXPECT_EQ(what(), "no value");
}

TEST_F(GoBlocking_Tests, WhatItReturnsIsDropped) {
    std::atomic<int> ran = {0};
    sgcl::async::go_blocking([&] {
        ++ran;
        return 5;
    });
    sgcl::async::go_blocking([&] {                           // a move-only result
        ++ran;
        return std::make_unique<int>(6);
    });
    sgcl::async::go_blocking([&]() -> std::string {
        ++ran;
        return std::string(100, 'x');
    });
    sgcl::async::go_blocking([&]() -> sgcl::tracked_ptr<Node> {   // a managed result, left to the collector
        ++ran;
        return sgcl::make_tracked<Node>(1);
    });
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_EQ(ran.load(), 4);
    EXPECT_EQ(calls.load(), 0);
}

TEST_F(GoBlocking_Tests, AMoveOnlyFunction) {
    std::atomic<int> seen = {0};
    auto p = std::make_unique<int>(11);
    auto f = [p = std::move(p), &seen] { seen = *p; };
    static_assert(!std::is_copy_constructible_v<decltype(f)>);
    sgcl::async::go_blocking(std::move(f));
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_EQ(seen.load(), 11);
}

// The move of F into the job throws: the throw is the caller's, nothing runs,
// nothing is queued
TEST_F(GoBlocking_Tests, AThrowingMoveOfTheFunction) {
    std::atomic<bool> ran = {false};
    ThrowingMove f(ran);
    ThrowingMove::armed = true;
    EXPECT_THROW(sgcl::async::go_blocking(f), std::length_error);   // the copy into the parameter, then the move that throws
    ThrowingMove::armed = false;
    EXPECT_EQ(sgcl::async::blocking_pool::get_statistics().queued, 0u);
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_FALSE(ran.load());
    sgcl::async::go_blocking(f);                             // unarmed: runs
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_TRUE(ran.load());
    EXPECT_EQ(calls.load(), 0);
}

// More jobs than the cap at once, a quarter of them throwing: the pool stays
// at its cap, the rest wait in the queue, every job runs, every throw is
// reported once
TEST_F(GoBlocking_Tests, ManyAtOncePastTheCap) {
    sgcl::async::blocking_pool::stop();
    sgcl::async::blocking_pool::set_threads(4);
    constexpr int jobs = 64;
    std::atomic<int> ran = {0};
    std::atomic<unsigned> most = {0};
    for (int i = 0; i < jobs; ++i) {
        sgcl::async::go_blocking([&, i] {
            std::this_thread::sleep_for(2ms);
            unsigned threads = sgcl::async::blocking_pool::get_statistics().threads;
            unsigned m = most.load();
            while (threads > m && !most.compare_exchange_weak(m, threads)) {
            }
            ++ran;
            if (i % 4 == 0) {
                throw std::runtime_error("job " + std::to_string(i));
            }
        });
    }
    EXPECT_LE(sgcl::async::blocking_pool::get_statistics().threads, 4u);
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_EQ(ran.load(), jobs);
    EXPECT_EQ(calls.load(), jobs / 4);
    EXPECT_EQ(calls_off_the_pool.load(), 0);
    EXPECT_LE(most.load(), 4u);
    EXPECT_GE(most.load(), 1u);
    // from many tasks at once, with the default cap
    sgcl::async::blocking_pool::set_threads(0);
    std::atomic<int> count = {0};
    std::vector<sgcl::async::task<>> tasks;
    for (int t = 0; t < 8; ++t) {
        tasks.push_back(sgcl::async::spawn([](std::atomic<int>& count) -> sgcl::async::task<> {
            for (int k = 0; k < 200; ++k) {
                sgcl::async::go_blocking([&] { ++count; });
                if (k % 50 == 0) {
                    co_await sgcl::async::yield();
                }
            }
        }(count)));
    }
    for (auto& t : tasks) {
        t.wait();
    }
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_EQ(count.load(), 1600);
    EXPECT_LE(sgcl::async::blocking_pool::get_statistics().threads, sgcl::async::blocking_pool::max_threads());
    sgcl::async::scheduler::stop();
}

// The pool cannot start a thread: with none running the call throws
// std::system_error and f never runs (nor is reported); with one running
// the job waits for it, as spawn_blocking's does
TEST_F(GoBlocking_Tests, ThePoolCannotStartAThread) {
    sgcl::async::blocking_pool::stop();
    sgcl::async::detail::blocking_start_test_hook.store(&fail_thread_start);
    std::atomic<bool> ran = {false};
    EXPECT_THROW(sgcl::async::go_blocking([&] { ran = true; }), std::system_error);
    auto st = sgcl::async::blocking_pool::get_statistics();
    sgcl::async::detail::blocking_start_test_hook.store(nullptr);
    ASSERT_EQ(st.threads, 0u);                               // (an entry left would hang the stop below)
    sgcl::async::blocking_pool::wait_idle();                 // the job that could not run is owed to nobody
    sgcl::async::blocking_pool::stop();
    std::atomic<int> after = {0};
    sgcl::async::go_blocking([&] { ++after; });              // the pool starts again
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_EQ(after.load(), 1);
    EXPECT_FALSE(ran.load());                                // never run, not even by the next thread
    // a thread running: the job waits for it instead of failing
    std::atomic<bool> release = {false};
    sgcl::async::go_blocking([&] {
        while (!release.load()) {
            std::this_thread::sleep_for(1ms);
        }
        ++after;
    });
    sgcl::async::detail::blocking_start_test_hook.store(&fail_thread_start);
    sgcl::async::go_blocking([&] { ++after; });              // the parked thread took the first: no idle one, no start
    sgcl::async::detail::blocking_start_test_hook.store(nullptr);
    EXPECT_EQ(sgcl::async::blocking_pool::get_statistics().threads, 1u);
    release = true;
    sgcl::async::blocking_pool::wait_idle();
    EXPECT_EQ(after.load(), 3);
    EXPECT_EQ(calls.load(), 0);
}

// The default handler: one line on stderr naming the pool, then
// std::terminate. Each statement runs in a child of its own (threadsafe:
// the child execs the test binary again)
using GoBlocking_DeathTest = GoBlocking_Tests;

TEST_F(GoBlocking_DeathTest, DefaultNamesThePool) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH({
        sgcl::async::on_unhandled(nullptr);
        sgcl::async::go_blocking([] { throw go_blocking_test::custom_error("no such host"); });
        std::this_thread::sleep_for(5s);
    }, "sgcl::async: unhandled exception in a detached task in the blocking pool: go_blocking_test::custom_error: no such host\n");
}

TEST_F(GoBlocking_DeathTest, DefaultOfANonStandardException) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH({
        sgcl::async::on_unhandled(nullptr);
        sgcl::async::go_blocking([] { throw 42; });
        std::this_thread::sleep_for(5s);
    }, "sgcl::async: unhandled exception in a detached task in the blocking pool: unknown exception\n");
}

// The end of the program with jobs running and queued (one thread: three
// wait behind the first): the pool's stop at exit runs them all to the end,
// as it does the jobs of spawn_blocking whose handles were dropped
TEST_F(GoBlocking_DeathTest, TheEndOfTheProgramRunsTheJobsQueued) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT({
        static std::atomic<int> ran = {0};
        sgcl::async::blocking_pool::set_threads(1);
        for (int i = 0; i < 4; ++i) {
            sgcl::async::go_blocking([] {
                std::this_thread::sleep_for(20ms);
                if (++ran == 4) {
                    std::fprintf(stderr, "go_blocking: 4 ran\n");
                }
            });
        }
        std::exit(0);
    }, ::testing::ExitedWithCode(0), "go_blocking: 4 ran\n");
    EXPECT_EXIT({
        static std::atomic<int> ran = {0};
        sgcl::async::blocking_pool::set_threads(1);
        for (int i = 0; i < 4; ++i) {
            (void)sgcl::async::spawn_blocking([] {
                std::this_thread::sleep_for(20ms);
                if (++ran == 4) {
                    std::fprintf(stderr, "spawn_blocking: 4 ran\n");
                }
            });
        }
        std::exit(0);
    }, ::testing::ExitedWithCode(0), "spawn_blocking: 4 ran\n");
}
