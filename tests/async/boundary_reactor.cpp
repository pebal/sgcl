//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of the world outside the tasks (DESIGN 408): a wait on a
// descriptor that is no descriptor or is closed, on a process that does
// not exist, an end of waits nobody waits on; signals of no number and of
// a channel that holds none.
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <climits>
#include <csignal>
#include <thread>

#include <fcntl.h>
#include <unistd.h>

namespace {
    using namespace std::chrono_literals;

    template<class F>
    bool soon(F&& f) {
        auto until = std::chrono::steady_clock::now() + 5s;
        while (!f()) {
            if (std::chrono::steady_clock::now() > until) {
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }
}

// A descriptor the kernel cannot watch (-1, a number closed) is ready at
// once: the call after the wait says what is wrong, so nothing waits for
// good; the same for a process that does not exist (exited's page)
TEST(ReactorBoundary_Tests, ADescriptorOrAProcessThatIsNotThere) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    ::close(fds[0]);
    ::close(fds[1]);
    for (int fd : {-1, fds[0], fds[1]}) {
        auto r = sgcl::async::readable(fd);
        auto w = sgcl::async::writable(fd);
        EXPECT_TRUE(soon([&] { return r.is_set() && w.is_set(); })) << "fd " << fd;
    }
    auto gone = sgcl::async::exited(INT_MAX);
    EXPECT_TRUE(soon([&] { return gone.is_set(); }));
    gone.wait();
    sgcl::async::scheduler::stop();
}

// An end of the waits on a descriptor nobody waits on does nothing, and
// the next wait on it is served as ever; a number that is no descriptor
// the same
TEST(ReactorBoundary_Tests, CancelWaitsWithNobodyWaiting) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    sgcl::async::cancel_waits(fds[0]);
    sgcl::async::cancel_waits(-1);
    auto r = sgcl::async::readable(fds[0]);
    EXPECT_FALSE(r.is_set());
    ASSERT_EQ(::write(fds[1], "x", 1), 1);
    r.wait();
    EXPECT_TRUE(r.is_set());
    ::close(fds[0]);
    ::close(fds[1]);
    sgcl::async::scheduler::stop();
}

// Signals of no number: a channel nothing is sent on. A channel of
// capacity zero takes a signal only while a receiver waits on it
TEST(ReactorBoundary_Tests, SignalsOfNoNumberAndOfARendezvous) {
    auto none = sgcl::async::signals({});
    EXPECT_FALSE(none.try_receive());
    EXPECT_FALSE(none.closed());
    sgcl::async::ignore_signals({});                          // nothing ignored
    auto meeting = sgcl::async::signals({SIGUSR2}, 0);
    EXPECT_EQ(meeting.capacity(), 0u);
    std::atomic<int> got = 0;
    std::thread receiver([&] {
        auto n = meeting.receive().wait();
        got = n ? *n : -1;
    });
    for (int i = 0; i < 500 && got.load() == 0; ++i) {       // raised until the waiting receiver has it: one raised before it waits is dropped
        ::raise(SIGUSR2);
        std::this_thread::sleep_for(10ms);
    }
    if (got.load() == 0) {
        meeting.close();                                      // never delivered: the receiver let go of
    }
    receiver.join();
    EXPECT_EQ(got.load(), SIGUSR2);
    sgcl::async::reset_signals({SIGUSR2});
    sgcl::async::scheduler::stop();
}
