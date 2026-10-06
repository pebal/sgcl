//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The terminal (Go's golang.org/x/term): raw mode and the echo turned off,
// their guard, the size and its change by SIGWINCH. The terminal of the
// tests is a pseudo-terminal's end (io::pty), and the termios flags are read
// back from it. (crypto::read_password, a password read under
// disable_echo, is tests/crypto/read_password.cpp's.)
#include "tests/types.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <string>
#include <termios.h>
#include <thread>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;
    using namespace std::chrono_literals;

    struct termios modes_of(int fd) {
        struct termios t = {};
        EXPECT_EQ(::tcgetattr(fd, &t), 0);
        return t;
    }

    // Bytes written to the master after a pause, from a thread that holds
    // nothing of the library's (a descriptor and characters)
    std::thread type_later(int master, const char* text) {
        return std::thread([master, text] {
            std::this_thread::sleep_for(100ms);
            (void)!::write(master, text, std::strlen(text));
        });
    }

    sgcl::async::task<optional<terminal_size>> next_size(sgcl::async::channel<terminal_size> sizes) {
        co_return co_await sizes.receive();
    }

    std::string drain(const io::pty& p) {
        std::string out;
        for (;;) {
            byte b[256];
            ssize_t n = ::read(p.fd(), b, sizeof b);   // non-blocking: what is there now
            if (n <= 0) {
                return out;
            }
            out.append(reinterpret_cast<const char*>(b), size_t(n));
        }
    }
}

TEST(IoTerminal_Tests, RawModeAndItsGuard) {
    io::pty p = io::open_pty().value();
    int t = p.terminal().fd();
    auto before = modes_of(t);
    ASSERT_TRUE(before.c_lflag & ICANON);
    ASSERT_TRUE(before.c_lflag & ECHO);
    {
        auto raw = io::make_raw(t);
        ASSERT_TRUE(raw) << raw.error().message();
        EXPECT_TRUE(*raw);
        EXPECT_EQ(raw->fd(), t);
        auto now = modes_of(t);
        EXPECT_FALSE(now.c_lflag & (ICANON | ECHO | ISIG | IEXTEN));
        EXPECT_FALSE(now.c_iflag & (ICRNL | IXON | BRKINT));
        EXPECT_FALSE(now.c_oflag & OPOST);
        EXPECT_EQ(now.c_cflag & CSIZE, tcflag_t(CS8));
        EXPECT_EQ(now.c_cc[VMIN], 1);
        EXPECT_EQ(now.c_cc[VTIME], 0);
        // a byte without a line end is there at once, ^C is a byte, nothing echoed
        ASSERT_TRUE(p.write("a\x03"));
        char got[4] = {};
        std::this_thread::sleep_for(20ms);
        EXPECT_EQ(::read(t, got, sizeof got), 2);
        EXPECT_EQ(std::string(got, 2), "a\x03");
        EXPECT_EQ(drain(p), "");
        // and "\n" goes out as it is, no "\r" added
        EXPECT_EQ(::write(t, "x\n", 2), 2);
        std::this_thread::sleep_for(20ms);
        EXPECT_EQ(drain(p), "x\n");
    }   // the guard's end gives the terminal back
    auto after = modes_of(t);
    EXPECT_EQ(after.c_lflag, before.c_lflag);
    EXPECT_EQ(after.c_iflag, before.c_iflag);
    EXPECT_EQ(after.c_oflag, before.c_oflag);
    EXPECT_EQ(after.c_cflag, before.c_cflag);
}

TEST(IoTerminal_Tests, RestoreMoveAndTheEmptyGuard) {
    io::pty p = io::open_pty().value();
    int t = p.terminal().fd();
    auto before = modes_of(t);
    io::terminal_mode none;
    EXPECT_FALSE(none);
    EXPECT_EQ(none.fd(), -1);
    EXPECT_TRUE(none.restore());   // nothing to restore: success
    io::terminal_mode raw = io::make_raw(t).value();
    io::terminal_mode moved = std::move(raw);
    EXPECT_FALSE(raw);
    EXPECT_TRUE(moved);
    EXPECT_TRUE(raw.restore());    // the moved-from restores nothing
    EXPECT_FALSE(modes_of(t).c_lflag & ICANON);
    ASSERT_TRUE(moved.restore());
    EXPECT_FALSE(moved);
    EXPECT_EQ(modes_of(t).c_lflag, before.c_lflag);
    EXPECT_TRUE(moved.restore());  // a second does nothing
    // an assignment restores what the target held before taking the other's
    io::terminal_mode a = io::make_raw(t).value();
    io::terminal_mode b;
    b = std::move(a);
    EXPECT_FALSE(modes_of(t).c_lflag & ICANON);
    b = io::terminal_mode();
    EXPECT_EQ(modes_of(t).c_lflag, before.c_lflag);
    // the raw mode of a terminal raw already restores the raw modes, not the cooked ones
    io::terminal_mode outer = io::make_raw(t).value();
    {
        io::terminal_mode inner = io::make_raw(t).value();
    }
    EXPECT_FALSE(modes_of(t).c_lflag & ICANON);
    outer.restore();
    EXPECT_EQ(modes_of(t).c_lflag, before.c_lflag);
}

TEST(IoTerminal_Tests, NoTerminal) {
    auto [r, w] = io::pipe().value();
    auto raw = io::make_raw(r.fd());
    ASSERT_FALSE(raw);
    EXPECT_EQ(raw.error().code(), std::errc::inappropriate_io_control_operation);   // ENOTTY
    EXPECT_EQ(raw.error().op(), "make_raw");
    auto bad = io::make_raw(-1);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), std::errc::bad_file_descriptor);
    auto size = io::get_terminal_size(r.fd());
    ASSERT_FALSE(size);
    EXPECT_EQ(size.error().code(), std::errc::inappropriate_io_control_operation);
    EXPECT_FALSE(io::set_terminal_size(w.fd(), {}));
}

TEST(IoTerminal_Tests, TheEchoTurnedOff) {
    io::pty p = io::open_pty().value();
    int t = p.terminal().fd();
    auto before = modes_of(t);
    {
        auto quiet = io::disable_echo(t);
        ASSERT_TRUE(quiet) << quiet.error().message();
        EXPECT_EQ(quiet->fd(), t);
        auto now = modes_of(t);
        EXPECT_FALSE(now.c_lflag & ECHO);
        EXPECT_TRUE(now.c_lflag & ICANON);   // by lines still
        EXPECT_TRUE(now.c_lflag & ISIG);
        EXPECT_TRUE(now.c_iflag & ICRNL);
        auto typing = type_later(p.fd(), "s3cret\n");
        char line[64] = {};
        ssize_t n = ::read(t, line, sizeof line);   // the line at its Enter
        typing.join();
        EXPECT_EQ(std::string(line, size_t(std::max<ssize_t>(n, 0))), "s3cret\n");
        EXPECT_EQ(drain(p).find("s3cret"), std::string::npos);   // never echoed
        // a "\r" ends a line as a "\n" does
        typing = type_later(p.fd(), "pw\r");
        n = ::read(t, line, sizeof line);
        typing.join();
        EXPECT_EQ(std::string(line, size_t(std::max<ssize_t>(n, 0))), "pw\n");
    }
    EXPECT_EQ(modes_of(t).c_lflag, before.c_lflag);   // the echo back on
    // over a raw terminal: lines and no echo, then raw again
    io::terminal_mode raw = io::make_raw(t).value();
    {
        io::terminal_mode quiet = io::disable_echo(t).value();
        EXPECT_TRUE(modes_of(t).c_lflag & ICANON);
    }
    EXPECT_FALSE(modes_of(t).c_lflag & ICANON);
    auto [r, w] = io::pipe().value();
    auto none = io::disable_echo(r.fd());
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), std::errc::inappropriate_io_control_operation);
    EXPECT_EQ(none.error().op(), "disable_echo");
}

TEST(IoTerminal_Tests, SizeChangesBySigwinch) {
    io::pty p = io::open_pty({.rows = 24, .columns = 80}).value();
    int t = p.terminal().fd();
    sgcl::async::stop_source stop;
    sgcl::async::channel<terminal_size> sizes = io::size_changes(t, stop.token());
    ASSERT_TRUE(io::set_terminal_size(t, {.rows = 30, .columns = 100}));
    ::kill(::getpid(), SIGWINCH);
    auto first = sgcl::async::spawn(sgcl::async::with_timeout(next_size(sizes), 5s)).wait();
    ASSERT_TRUE(first && *first);
    EXPECT_EQ(**first, (terminal_size{30, 100, 0, 0}));
    // two changes before a receive: the newer replaces the older
    ASSERT_TRUE(io::set_terminal_size(t, {.rows = 31, .columns = 101}));
    ::kill(::getpid(), SIGWINCH);
    std::this_thread::sleep_for(100ms);
    ASSERT_TRUE(io::set_terminal_size(t, {.rows = 32, .columns = 102}));
    ::kill(::getpid(), SIGWINCH);
    std::this_thread::sleep_for(100ms);
    auto newest = sizes.try_receive();
    ASSERT_TRUE(newest);
    EXPECT_EQ(*newest, (terminal_size{32, 102, 0, 0}));
    EXPECT_FALSE(sizes.try_receive());
    // the stop ends it: the channel closed
    stop.request_stop();
    auto end = sgcl::async::spawn(sgcl::async::with_timeout(next_size(sizes), 5s)).wait();
    ASSERT_TRUE(end);         // not timed out
    EXPECT_FALSE(*end);       // closed
    EXPECT_TRUE(sizes.closed());
}

TEST(IoTerminal_Tests, SizeChangesEndWithTheirChannel) {
    io::pty p = io::open_pty().value();
    int t = p.terminal().fd();
    sgcl::async::channel<terminal_size> sizes = io::size_changes(t);   // no stop: ends when closed and the next signal comes
    sizes.close();
    ::kill(::getpid(), SIGWINCH);
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(sizes.closed());
    EXPECT_FALSE(sizes.try_receive());
    // a terminal gone: the change after it is no size, and the channel stays
    sgcl::async::stop_source stop;
    sgcl::async::channel<terminal_size> gone = io::size_changes(-1, stop.token());
    ::kill(::getpid(), SIGWINCH);
    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(gone.try_receive());
    EXPECT_FALSE(gone.closed());
    stop.request_stop();
}
