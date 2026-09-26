//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net: connection::in_memory, the pair of ends in memory (Go's net.Pipe): a write
// handed over whole to reads of any size, the end of the stream after a
// close or a close_write, deadlines on the manual clock, a close from
// another task ending a read and a write, lines and copies over it, the
// blocking forms from threads.
#include "tests/types.h"
#include "sgcl/net/net.h"

using namespace sgcl::net;
using namespace sgcl::async;

#include <atomic>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>

namespace {
    using namespace std::chrono_literals;

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    task<expected<size_t, io::error>> write(net::connection c, std::string s) {
        co_return co_await c.async_write(sgcl::string(s));
    }

    // Everything read in pieces of `piece` bytes
    task<std::string> read_in_pieces(net::connection c, size_t piece) {
        std::string out;
        vector<byte> buf;
        buf.resize(piece);
        for (;;) {
            auto r = co_await c.async_read(buf.as_slice());
            if (!r || *r == 0) {
                co_return out;
            }
            out.append(reinterpret_cast<const char*>(buf.data()), *r);
        }
    }
}

TEST(NetMemory_Tests, WritesReachReadsOfAnySize) {
    for (size_t piece : {1, 2, 3, 7, 64, 100000}) {
        auto [a, b] = net::connection::in_memory();
        std::string payload;
        for (int i = 0; i < 1000; ++i) {
            payload += std::to_string(i) + ",";
        }
        auto reader = spawn(read_in_pieces(b, piece));
        auto w = spawn(write(a, payload)).wait();
        ASSERT_TRUE(w);
        EXPECT_EQ(*w, payload.size());   // returned only after the reads took it all
        a.close();
        EXPECT_EQ(reader.wait(), payload) << piece;
        EXPECT_FALSE(a.local_endpoint().is_valid());
        EXPECT_TRUE(a.path().empty());
        b.close();
    }
}

TEST(NetMemory_Tests, EndsAndErrors) {
    auto [a, b] = net::connection::in_memory();
    ASSERT_TRUE(a.close_write());
    byte buf[8];
    auto end = spawn(b.async_read(buf)).wait();   // the writer is done: the end of the stream
    ASSERT_TRUE(end);
    EXPECT_EQ(*end, 0u);
    auto w = a.write("x");
    ASSERT_FALSE(w);
    EXPECT_TRUE(w.error().is_closed());           // this end's writing half is closed
    auto reply = spawn(read_in_pieces(a, 3));      // the other direction still open
    ASSERT_TRUE(spawn(write(b, "reply")).wait());
    b.close();
    EXPECT_EQ(reply.wait(), "reply");
    auto after = a.read(buf);
    ASSERT_TRUE(after);
    EXPECT_EQ(*after, 0u);                        // the peer closed: the end
    auto own = b.read(buf);
    ASSERT_FALSE(own);
    EXPECT_TRUE(own.error().is_closed());         // this end closed: an error
    auto gone = a.write(sgcl::string("to nobody"));
    EXPECT_FALSE(gone);
    auto [c, d] = net::connection::in_memory();
    d.close();
    auto broken = c.write("x");
    ASSERT_FALSE(broken);
    EXPECT_EQ(broken.error().code(), std::errc::broken_pipe);
    EXPECT_TRUE(c.write(sgcl::string()));         // nothing to hand over: no wait
    c.close();
    EXPECT_TRUE(c.close());
    EXPECT_FALSE(c.set_no_delay(true));           // not TCP
}

TEST(NetMemory_Tests, CloseFromAnotherTaskEndsAReadAndAWrite) {
    auto [a, b] = net::connection::in_memory();
    auto reader = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
        byte buf[8];
        co_return co_await c.async_read(buf);
    }(b));
    auto writer = spawn(write(a, "nobody reads this"));   // b's reader takes 8 bytes, then nobody
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto r = reader.wait();
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 8u);
    EXPECT_FALSE(writer.done());
    a.close();
    auto w = writer.wait();
    ASSERT_FALSE(w);
    EXPECT_TRUE(w.error().is_closed());
    auto waiting = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
        byte buf[8];
        co_return co_await c.async_read(buf);
    }(b));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto end = waiting.wait();                     // a's close is b's end of stream
    ASSERT_TRUE(end);
    EXPECT_EQ(*end, 0u);
    auto [c, d] = net::connection::in_memory();
    auto blocked = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
        byte buf[8];
        co_return co_await c.async_read(buf);
    }(d));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    d.close();                                     // its own close
    auto cut = blocked.wait();
    ASSERT_FALSE(cut);
    EXPECT_TRUE(cut.error().is_closed());
    c.close();
}

TEST(NetMemory_Tests, DeadlinesOnTheManualClock) {
    auto [a, b] = net::connection::in_memory();
    sgcl::async::manual_clock clock;
    clock.install();
    b.set_read_deadline(clock.now() + 2s);
    auto reader = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
        byte buf[8];
        co_return co_await c.async_read(buf);
    }(b));
    clock.advance(1s);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(reader.done());
    b.set_read_deadline(clock.now() + 5s);         // moved while the read waits
    clock.advance(2s);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(reader.done());
    clock.advance(3s);
    auto r = reader.wait();
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_timeout()) << r.error().message();
    a.set_write_deadline(clock.now() + 1s);        // nobody reads: the write times out
    auto writer = spawn(write(a, "stuck"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    clock.advance(1s);
    auto w = writer.wait();
    ASSERT_FALSE(w);
    EXPECT_TRUE(w.error().is_timeout());
    b.set_deadline(time_point());
    a.set_deadline(time_point());
    auto blocked = spawn([](net::connection c) -> task<expected<size_t, io::error>> {
        byte buf[8];
        co_return co_await c.async_read(buf);
    }(b));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    b.set_read_deadline(clock.now());              // shortened to now from another task: the read ends at once
    auto cut = blocked.wait();
    ASSERT_FALSE(cut);
    EXPECT_TRUE(cut.error().is_timeout());
    b.set_deadline(time_point());
    auto again = spawn(read_in_pieces(b, 4));
    ASSERT_TRUE(spawn(write(a, "after")).wait());
    a.close();
    EXPECT_EQ(again.wait(), "after");
    clock.uninstall();
    b.close();
    sgcl::async::scheduler::stop();
}

// Lines cut across writes of 1, 2, 3 and 7 bytes: every boundary of the
// reader's block falls somewhere else in each run
TEST(NetMemory_Tests, LinesAcrossPiecesOfEverySize) {
    std::string input;
    for (int i : range(200)) {
        input += "line " + std::to_string(i) + (i % 3 ? "\n" : "\r\n");
    }
    input += "last without an end";
    for (size_t piece : {1, 2, 3, 7}) {
        auto [a, b] = net::connection::in_memory();
        auto reader = spawn([](net::connection c) -> task<std::string> {
            std::string out;
            for (;;) {
                auto l = co_await c.async_read_line();
                if (!l || !*l) {
                    co_return out;
                }
                out += text(**l) + "|";
            }
        }(b));
        for (size_t at = 0; at < input.size(); at += piece) {
            ASSERT_TRUE(spawn(write(a, input.substr(at, piece))).wait());
        }
        a.close();
        std::string expected;
        for (int i : range(200)) {
            expected += "line " + std::to_string(i) + "|";
        }
        expected += "last without an end|";
        EXPECT_EQ(reader.wait(), expected) << piece;
        b.close();
    }
    auto [c, d] = net::connection::in_memory();
    d.set_max_line(8);   // taken by the next read_line
    auto too_long = spawn([](net::connection c) -> task<bool> {
        auto l = co_await c.async_read_line();
        co_return !l && l.error().code() == io::errc::line_too_long;
    }(d));
    ASSERT_TRUE(spawn(write(c, "0123456789abcdef\n")).wait());
    EXPECT_TRUE(too_long.wait());
    c.close();
    d.close();
}

TEST(NetMemory_Tests, LinesCopiesAndThreads) {
    auto [a, b] = net::connection::in_memory();
    auto [c, d] = net::connection::in_memory();
    // a proxy: what comes in at b goes out at c, and d reads it
    auto proxy = spawn([](net::connection from, net::connection to) -> task<size_t> {
        auto n = co_await from.async_copy_to(to);
        (void)to.close();
        co_return n ? *n : 0;
    }(b, c));
    auto lines = spawn([](net::connection c) -> task<std::string> {
        std::string out;
        for (;;) {
            auto l = co_await c.async_read_line();
            if (!l || !*l) {
                co_return out;
            }
            out += text(**l) + "|";
        }
    }(d));
    // the blocking forms, from a thread
    std::thread writer([&] {
        a.write("alpha\nbeta\r\n");
        a.write("gamma");
        a.close();
    });
    writer.join();
    EXPECT_EQ(proxy.wait(), 17u);
    EXPECT_EQ(lines.wait(), "alpha|beta|gamma|");
    auto [e, f] = net::connection::in_memory();
    std::string got;
    std::thread reader([&] {
        auto all = f.read_all_text();
        if (all) {
            got = text(*all);
        }
    });
    ASSERT_TRUE(e.write("from a thread to a thread"));
    e.close();
    reader.join();
    EXPECT_EQ(got, "from a thread to a thread");
    f.close();
    b.close();
    d.close();
}
