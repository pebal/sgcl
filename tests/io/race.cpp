//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io: the race of a close with an operation in progress on a file. A task
// reads a pipe a byte at a time, its data all 'A'; another thread closes
// the file and at once opens new pipes with 'B' waiting in them, so that
// the number just given back is taken again. A read on a stale number
// would return a 'B'. Ten thousand rounds; run under the thread sanitizer
// as well. As net's sockets (net/race.cpp), the file holds its descriptor
// for every operation, and the last one out makes the ::close.
#include "tests/types.h"

using namespace sgcl::async;

#include <atomic>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
    struct Round {
        std::atomic<size_t> foreign = {0};   // bytes read that were not the pipe's
        std::atomic<size_t> read = {0};
    };

    task<> read_until_closed(tracked_ptr<io::file> f, Round* round) {
        byte b[1];
        for (;;) {
            auto r = co_await f->async_read(b);
            if (!r || *r == 0) {
                co_return;
            }
            round->read.fetch_add(1, std::memory_order_relaxed);
            if (b[0] != byte('A')) {
                round->foreign.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    void fill(int fd, char c, size_t n) {
        std::vector<char> data(n, c);
        size_t done = 0;
        while (done < n) {
            ssize_t w = ::write(fd, data.data() + done, n - done);
            if (w <= 0) {
                break;
            }
            done += size_t(w);
        }
    }
}

TEST(IoRace_Tests, CloseAgainstAReadInProgress) {
    const int rounds = 10000;
    size_t foreign = 0, read = 0, closed_while_reading = 0;
    for (int i = 0; i < rounds; ++i) {
        auto p = io::pipe();
        ASSERT_TRUE(p);
        auto [r, w] = *p;
        fill(w->fd(), 'A', 512);   // enough for the reader to be inside its loop when the close comes
        Round round;
        auto reader = spawn(read_until_closed(r, &round));
        for (int spin = 0; spin < (i % 64); ++spin) {
            std::this_thread::yield();
        }
        (void)r->close();
        int fresh[4][2];
        for (auto& q : fresh) {   // the numbers taken again, 'B' waiting in each pipe
            ASSERT_EQ(::pipe(q), 0);
            fill(q[1], 'B', 64);
        }
        reader.wait();
        foreign += round.foreign;
        read += round.read;
        closed_while_reading += round.read < 512;
        for (auto& q : fresh) {
            ::close(q[0]);
            ::close(q[1]);
        }
        (void)w->close();
    }
    EXPECT_EQ(foreign, 0u);
    EXPECT_GT(closed_while_reading, size_t(rounds / 10));   // the close did land inside the reads, often
    EXPECT_GT(read, 0u);
}
