//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// async: one contract for every wait on an event (event.h): an event happens
// once, set by the close of its channel alone, and after a wait on it
// (wait() on a thread, co_await in a task, a select's on_set case) is_set()
// is true. The reactor's events, the ones that raced it (a signal before
// the close woke a wait, and the close came a moment later): a descriptor
// ready at once, many times over. Whether a wait ended because its source
// was ready stays the library's to read (EventAccess::ready: a bit written
// before the close).
#include "tests/types.h"
#include "sgcl/async/async.h"

#include <cassert>
#include <unistd.h>

namespace {
    struct Pipe {
        int fd[2];
        Pipe() {
            [[maybe_unused]] int r = ::pipe(fd);
            assert(r == 0);
            [[maybe_unused]] auto n = ::write(fd[1], "x", 1);   // readable at once, and writable (room)
        }
        ~Pipe() {
            ::close(fd[0]);
            ::close(fd[1]);
        }
    };
}

TEST(EventSet_Tests, AfterWaitOnAThread) {
    Pipe p;
    int unset = 0;
    for (int i = 0; i < 200; ++i) {
        sgcl::async::event r = sgcl::async::readable(p.fd[0]);
        r.wait();
        unset += !r.is_set();
        sgcl::async::event w = sgcl::async::writable(p.fd[1]);
        w.wait();
        unset += !w.is_set();
    }
    EXPECT_EQ(unset, 0);
    sgcl::async::scheduler::stop();
}

TEST(EventSet_Tests, AfterCoAwaitInATask) {
    Pipe p;
    auto t = sgcl::async::spawn([](int rfd, int wfd) -> sgcl::async::task<int> {
        int unset = 0;
        for (int i = 0; i < 1000; ++i) {
            sgcl::async::event r = sgcl::async::readable(rfd);
            co_await r;
            unset += !r.is_set();
            sgcl::async::event w = sgcl::async::writable(wfd);
            co_await w;
            unset += !w.is_set();
        }
        co_return unset;
    }(p.fd[0], p.fd[1]));
    EXPECT_EQ(t.wait(), 0);
    sgcl::async::scheduler::stop();
}

TEST(EventSet_Tests, InASelectsCaseInATask) {
    // the select in a task (resumed on a worker at once, as a coroutine
    // woken by the reactor is): the case runs with the event set
    Pipe p;
    auto t = sgcl::async::spawn([](int rfd, int wfd) -> sgcl::async::task<int> {
        int unset = 0;
        sgcl::async::channel<int> never;
        for (int i = 0; i < 1000; ++i) {
            sgcl::async::event r = sgcl::async::readable(rfd);
            bool seen = false;
            (void)co_await sgcl::async::select(never.on_receive([](int) {}), r.on_set([&] { seen = r.is_set(); }));
            unset += !seen;
            sgcl::async::event w = sgcl::async::writable(wfd);
            seen = false;
            (void)co_await sgcl::async::select(never.on_receive([](int) {}), w.on_set([&] { seen = w.is_set(); }));
            unset += !seen;
        }
        co_return unset;
    }(p.fd[0], p.fd[1]));
    EXPECT_EQ(t.wait(), 0);
    sgcl::async::scheduler::stop();
}

TEST(EventSet_Tests, ReadyStaysTheLibrarysToRead) {
    Pipe p;
    sgcl::async::event r = sgcl::async::readable(p.fd[0]);
    r.wait();
    EXPECT_TRUE(r.is_set());
    EXPECT_TRUE(sgcl::async::detail::EventAccess::ready(r));   // set by the readiness
    sgcl::async::event plain;
    plain.set();
    EXPECT_FALSE(sgcl::async::detail::EventAccess::ready(plain));   // set, not by a source's readiness
    sgcl::async::event open;
    EXPECT_FALSE(sgcl::async::detail::EventAccess::ready(open));    // not set: never ready
    sgcl::async::scheduler::stop();
}
