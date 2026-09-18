//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "tests/types.h"

#include <chrono>
#include <thread>
#include <unistd.h>

namespace {
    using namespace std::chrono_literals;

    struct Pipe {
        int fd[2];
        Pipe() {
            [[maybe_unused]] int r = ::pipe(fd);
            assert(r == 0);
        }
        ~Pipe() {
            ::close(fd[0]);
            ::close(fd[1]);
        }
    };
}

TEST(Reactor_Test, ReadableWakesATaskWhenDataComes) {
    Pipe p;
    auto t = sgcl::spawn([](int fd) -> sgcl::task<int> {
        co_await sgcl::readable(fd)->async_receive();       // suspended, no thread held
        char c = 0;
        [[maybe_unused]] auto n = ::read(fd, &c, 1);
        co_return c;
    }(p.fd[0]));
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(t.done());
    [[maybe_unused]] auto n = ::write(p.fd[1], "x", 1);
    EXPECT_EQ(t.join(), 'x');
    sgcl::scheduler::stop();
}

TEST(Reactor_Test, WritableIsAtOnceOnAPipeWithRoom) {
    Pipe p;
    EXPECT_TRUE(sgcl::writable(p.fd[1])->receive());
    // and readable once the end of the stream comes
    ::close(p.fd[1]);
    p.fd[1] = ::dup(p.fd[0]);                                 // the destructor closes two descriptors
    EXPECT_TRUE(sgcl::readable(p.fd[0])->receive());          // readable: the end of the stream
    sgcl::scheduler::stop();
}

TEST(Reactor_Test, AWaitBoundedByATimeoutAndCancelledByAToken) {
    Pipe p;
    bool timed = false;
    EXPECT_EQ(sgcl::select(sgcl::readable(p.fd[0])->on_receive([] {}), sgcl::timeout(20ms, [&] { timed = true; })), 1u);
    EXPECT_TRUE(timed);
    sgcl::stop_source src;
    auto t = sgcl::spawn([](int fd, sgcl::stop_token tok) -> sgcl::task<int> {
        co_return (int)co_await sgcl::async_select(sgcl::readable(fd)->on_receive([] {}), tok.on_stop([] {}));
    }(p.fd[0], src.token()));
    std::this_thread::sleep_for(10ms);
    src.request_stop();
    EXPECT_EQ(t.join(), 1);
    [[maybe_unused]] auto n = ::write(p.fd[1], "y", 1);        // the registrations left behind fire and are dropped
    std::this_thread::sleep_for(10ms);
    sgcl::scheduler::stop();                                   // or are ended by the stop
}

TEST(Reactor_Test, ManyTasksOnManyPipes) {
    constexpr int N = 32;
    std::vector<Pipe> pipes(N);
    std::vector<sgcl::task<int>> tasks;
    for (int i = 0; i < N; ++i) {
        tasks.push_back(sgcl::spawn([](int fd) -> sgcl::task<int> {
            co_await sgcl::readable(fd)->async_receive();
            char c = 0;
            [[maybe_unused]] auto n = ::read(fd, &c, 1);
            co_return c;
        }(pipes[i].fd[0])));
    }
    for (int i = N - 1; i >= 0; --i) {
        char c = char('a' + i % 26);
        [[maybe_unused]] auto n = ::write(pipes[i].fd[1], &c, 1);
    }
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(tasks[i].join(), 'a' + i % 26);
    }
    sgcl::scheduler::stop();
}

// Two waits on one descriptor in one direction are both signalled by its
// readiness: the kernel keeps one entry per descriptor and filter, and a
// second registration would have replaced the first (never signalled)
TEST(Reactor_Test, TwoWaitsOnOneDescriptorAreBothSignalled) {
    Pipe p;
    auto waiter = [](int fd) -> sgcl::task<bool> {
        co_return co_await sgcl::readable(fd)->async_receive();   // true: signalled, not closed with nothing
    };
    auto a = sgcl::spawn(waiter(p.fd[0]));
    auto b = sgcl::spawn(waiter(p.fd[0]));
    std::this_thread::sleep_for(20ms);
    EXPECT_FALSE(a.done());
    EXPECT_FALSE(b.done());
    [[maybe_unused]] auto n = ::write(p.fd[1], "x", 1);
    EXPECT_TRUE(a.join());
    EXPECT_TRUE(b.join());
    sgcl::scheduler::stop();
}
