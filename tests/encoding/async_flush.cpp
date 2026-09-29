//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The streaming writers of JSON and XML in a task: async_flush hands the
// text it gathered to the stream, and a stream with no async_write of its
// own (a file, a writer of the program's) is written on the blocking pool,
// which may outlive the frame of a task let go of. The pool's write is
// given a slice that holds its owner, never one into the writer's plain
// memory, so the text lives as long as the write that reads it. The first
// cases look at the slice the pool is given; the last lets the task go
// while its flush waits on the pool, the writer then collected, and the
// write reads its bytes after that (under ASan a read of freed memory,
// before the change).
#include "common.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

using namespace sgcl::encoding;
using namespace std::chrono_literals;

namespace {
    namespace io = sgcl::io;

    // A writer with no async_write: a task's write of it runs on the pool.
    // Whether each slice it was given held an owner; with `hold`, its first
    // write waits until released and then reads every byte it was given
    struct PoolWriter {
        bool hold = false;
        std::atomic<bool> entered = false;
        std::atomic<bool> released = false;
        std::atomic<bool> done = false;
        std::atomic<bool> owned = true;
        std::atomic<size_t> written = 0;
        std::atomic<uint64_t> sum = 0;

        expected<size_t, io::error> write(const slice<const byte>& d) {
            if (!d.owner()) {
                owned = false;
            }
            entered = true;
            while (hold && !released) {
                std::this_thread::sleep_for(1ms);
            }
            uint64_t s = 0;
            for (auto b : d) {
                s += uint8_t(b);
            }
            sum += s;
            written += d.size();
            done = true;
            return d.size();
        }
    };

    std::string long_text(char c) {
        return std::string(200000, c);
    }

    sgcl::async::task<bool> json_in_a_task(PoolWriter& out) {
        json::writer w(out);
        w.begin_object().key("text").value(sgcl::string(long_text('j'))).end_object();
        co_return (bool)co_await w.async_flush();
    }

    sgcl::async::task<bool> xml_in_a_task(PoolWriter& out) {
        xml::writer w(out);
        w.start("doc").text(sgcl::string(long_text('x'))).end();
        co_return (bool)co_await w.async_flush();
    }
}

TEST(EncodingAsyncFlush_Tests, JsonHandsThePoolAnOwnedSlice) {
    PoolWriter out;
    EXPECT_TRUE(sgcl::async::spawn(json_in_a_task(out)).wait());
    EXPECT_GT(out.written.load(), 200000u);
    EXPECT_TRUE(out.owned) << "json::writer::async_flush gave the pool a slice with no owner";
}

TEST(EncodingAsyncFlush_Tests, XmlHandsThePoolAnOwnedSlice) {
    PoolWriter out;
    EXPECT_TRUE(sgcl::async::spawn(xml_in_a_task(out)).wait());
    EXPECT_GT(out.written.load(), 200000u);
    EXPECT_TRUE(out.owned) << "xml::writer::async_flush gave the pool a slice with no owner";
}

// The task let go of while its flush waits on the pool: the task, its
// frame and the writer in it become garbage and are collected; then the
// write goes on and reads every byte it was given. Before the change that
// was the writer's plain memory, freed with it (ASan: heap-use-after-free).
// A dropped task was then destroyed where it waited, and the job's end
// resumed the destroyed coroutine (DESIGN 300); a started task let go of
// runs on to its end now (DESIGN 302), and its flush with it
TEST(EncodingAsyncFlush_Tests, APoolWriteOutlivesTheTaskLetGoOf) {
    for (int kind = 0; kind < 2; ++kind) {
        SCOPED_TRACE(kind ? "xml" : "json");
        static PoolWriter outs[2];   // outlives the pool's write, whatever becomes of the task
        PoolWriter& out = outs[kind];
        out.hold = true;
        {
            auto t = sgcl::async::spawn(kind ? xml_in_a_task(out) : json_in_a_task(out));
            for (int i = 0; i < 2000 && !out.entered; ++i) {
                std::this_thread::sleep_for(1ms);
            }
            ASSERT_TRUE(out.entered);
        }   // the task let go of, its flush on the pool
        for (int i = 0; i < 3; ++i) {
            collector::force_collect(true);
        }
        out.released = true;
        for (int i = 0; i < 5000 && !out.done; ++i) {
            std::this_thread::sleep_for(1ms);
        }
        ASSERT_TRUE(out.done);
        EXPECT_GT(out.written.load(), 200000u);
        const uint64_t letters = kind ? uint64_t('x') * 200000u : uint64_t('j') * 200000u;
        EXPECT_GE(out.sum.load(), letters);   // the text read whole
    }
}
