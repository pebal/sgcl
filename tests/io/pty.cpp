//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// pty: a pseudo-terminal, a child started on it as its controlling
// terminal (posix_spawn with a session of its own and the terminal opened
// in the child), the size and SIGWINCH, the master as a stream. Go has no
// pty in its standard library (creack/pty is not on this machine): the
// oracle is the system's own tools in the child, /bin/sh, stty, tty.
#include "tests/types.h"

#include <chrono>
#include <csignal>
#include <string>

namespace {
    using namespace sgcl::io;
    namespace io = sgcl::io;
    using namespace std::chrono_literals;

    // The master read until its text holds `what` (or the end, or 10 s)
    std::string read_until(const io::pty& p, std::string& seen, std::string_view what) {
        auto deadline = clock::now() + 10s;
        while (seen.find(what) == std::string::npos && clock::now() < deadline) {
            byte b[512];
            auto n = p.read(slice<byte>(b, sizeof b));
            if (!n || *n == 0) {
                break;
            }
            seen.append(reinterpret_cast<const char*>(b), *n);
        }
        return seen;
    }

    std::string rest_of(const io::pty& p) {
        auto all = io::read_all_text(p);
        EXPECT_TRUE(all) << all.error().message();
        return all ? std::string(all->view()) : std::string();
    }

    bool holds(const std::string& text, std::string_view part) {
        return text.find(part) != std::string::npos;
    }
}

TEST(IoPty_Tests, OpenSizeAndName) {
    auto p = io::open_pty({.rows = 30, .columns = 100});
    ASSERT_TRUE(p) << p.error().message();
    EXPECT_TRUE(*p);
    EXPECT_TRUE(p->name().starts_with("/dev/"));
    EXPECT_GE(p->fd(), 0);
    EXPECT_FALSE(p->is_closed());
    EXPECT_EQ(value_of(p->size()), (terminal_size{30, 100, 0, 0}));
    io::file t = p->terminal();
    ASSERT_TRUE(t);
    EXPECT_TRUE(io::is_terminal(t.fd()));
    EXPECT_EQ(value_of(io::get_terminal_size(t.fd())), (terminal_size{30, 100, 0, 0}));   // the two ends share it
    ASSERT_TRUE(p->resize({.rows = 50, .columns = 132, .width_pixels = 640, .height_pixels = 480}));
    EXPECT_EQ(value_of(p->size()), (terminal_size{50, 132, 640, 480}));
    ASSERT_TRUE(io::set_terminal_size(t.fd(), {.rows = 10, .columns = 20}));
    EXPECT_EQ(value_of(p->size()), (terminal_size{10, 20, 0, 0}));
    io::pty copy = *p;
    EXPECT_TRUE(copy == *p);
    ASSERT_TRUE(p->close());
    EXPECT_TRUE(copy.is_closed());
    EXPECT_EQ(copy.fd(), -1);
    EXPECT_TRUE(t.is_closed());   // the terminal end goes with it
}

TEST(IoPty_Tests, AnEmptyHandle) {
    io::pty p;
    EXPECT_FALSE(p);
    io::pty q = io::open_pty().value();
    EXPECT_FALSE(p == q);
    EXPECT_EQ(value_of(q.size()), (terminal_size{}));   // 24 by 80
    io::pty moved = std::move(q);
    EXPECT_TRUE(moved);
    (void)moved.close();
}

TEST(IoPty_Tests, TheChildHasAControllingTerminal) {
    io::pty p = io::open_pty({.rows = 30, .columns = 100}).value();
    io::command sh("/bin/sh", "-c", "tty; stty size; test -t 0 && test -t 1 && test -t 2 && echo three-terminals; "
                                    "exec 3</dev/tty && echo controlling; ps -o sess= -o pgid= -p $$ >/dev/null && echo leader");
    auto started = p.start(sh);
    ASSERT_TRUE(started) << started.error().message();
    EXPECT_FALSE(p.terminal().fd() >= 0);   // the program's copy closed once the child holds the terminal
    std::string out = rest_of(p);
    EXPECT_TRUE(holds(out, std::string(p.name().view()) + "\r\n")) << out;
    EXPECT_TRUE(holds(out, "30 100\r\n")) << out;
    EXPECT_TRUE(holds(out, "three-terminals\r\n")) << out;
    EXPECT_TRUE(holds(out, "controlling\r\n")) << out;   // /dev/tty opens: the terminal is the child's controlling one
    auto w = sh.wait();
    EXPECT_TRUE(w) << w.error().message();
    EXPECT_TRUE(sh.state->success());
}

TEST(IoPty_Tests, AnyProgramGetsTheControllingTerminal) {
    // not only a shell (bash opens its terminal again of its own accord,
    // which makes it the controlling one): dd opens /dev/tty, which only a
    // process with a controlling terminal can
    io::pty p = io::open_pty().value();
    io::command dd("dd", "if=/dev/tty", "count=0");
    ASSERT_TRUE(p.start(dd));
    std::string out = rest_of(p);
    auto w = dd.wait();
    EXPECT_TRUE(w) << out;
    for (const char* shell : {"/bin/dash", "/bin/zsh"}) {   // shells that do not
        if (!io::exists(shell)) {
            continue;
        }
        io::pty q = io::open_pty().value();
        io::command sh(shell, "-c", "exec 3</dev/tty && echo controlling");
        ASSERT_TRUE(q.start(sh));
        EXPECT_TRUE(holds(rest_of(q), "controlling\r\n")) << shell;
        ASSERT_TRUE(sh.wait());
    }
}

TEST(IoPty_Tests, AnExecThatFailsInTheChild) {
    auto dir = io::make_temp_dir({}, "sgcl-pty-*").value();
    string script = io::path::join(dir, "bad-interpreter");
    ASSERT_TRUE(io::write_file(script, "#!/no/such/interpreter-sgcl\n", io::permissions(0755)));
    io::pty p = io::open_pty().value();
    io::command bad(script);
    auto r = p.start(bad);
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_not_found()) << r.error().message();
    EXPECT_FALSE(bad.process);
    EXPECT_FALSE(bad.in);
    io::command elsewhere("/bin/sh", "-c", "true");
    elsewhere.dir = io::path::join(dir, "no-such-dir");
    auto d = p.start(elsewhere);
    ASSERT_FALSE(d);
    EXPECT_TRUE(d.error().is_not_found()) << d.error().message();
    io::command there("/bin/sh", "-c", "pwd");
    there.dir = dir;
    ASSERT_TRUE(p.start(there));
    auto out = rest_of(p);
    ASSERT_TRUE(there.wait());
    EXPECT_TRUE(holds(out, "sgcl-pty-")) << out;
    io::remove_all(dir);
}

TEST(IoPty_Tests, TheChildLeadsASessionOfItsOwn) {
    io::pty p = io::open_pty().value();
    // the shell's session id is its own pid, and the terminal's foreground group is its group
    io::command sh("/bin/sh", "-c", "s=$(ps -o sess= -p $$ | tr -d ' '); g=$(ps -o tpgid= -p $$ | tr -d ' '); "
                                    "test \"$g\" = \"$$\" && echo foreground; echo pid=$$");
    ASSERT_TRUE(p.start(sh));
    std::string out = rest_of(p);
    EXPECT_TRUE(holds(out, "foreground\r\n")) << out;
    ASSERT_TRUE(sh.wait());
    EXPECT_NE(::getsid(0), sh.process.pid());   // not the test's session
}

TEST(IoPty_Tests, WhatIsWrittenIsTyped) {
    io::pty p = io::open_pty().value();
    io::command sh("/bin/sh", "-c", "echo ready; read x; echo got:$x");
    ASSERT_TRUE(p.start(sh));
    std::string seen;
    read_until(p, seen, "ready\r\n");
    ASSERT_TRUE(p.write("hello\n"));
    std::string out = seen + rest_of(p);
    EXPECT_TRUE(holds(out, "hello\r\n")) << out;       // the echo of the line discipline
    EXPECT_TRUE(holds(out, "got:hello\r\n")) << out;
    ASSERT_TRUE(sh.wait());
}

TEST(IoPty_Tests, ResizeIsTheChildsSigwinch) {
    io::pty p = io::open_pty({.rows = 24, .columns = 80}).value();
    io::command sh("/bin/sh", "-c", "trap 'stty size; exit 0' WINCH; echo ready; while :; do sleep 0.05; done");
    ASSERT_TRUE(p.start(sh));
    std::string seen;
    read_until(p, seen, "ready\r\n");
    ASSERT_TRUE(p.resize({.rows = 40, .columns = 120}));
    std::string out = seen + rest_of(p);
    EXPECT_TRUE(holds(out, "40 120\r\n")) << out;
    ASSERT_TRUE(sh.wait());
}

TEST(IoPty_Tests, ControlCIsTheForegroundsSigint) {
    io::pty p = io::open_pty().value();
    io::command sh("/bin/sh", "-c", "echo ready; sleep 30");
    ASSERT_TRUE(p.start(sh));
    std::string seen;
    read_until(p, seen, "ready\r\n");
    auto started = clock::now();
    ASSERT_TRUE(p.write("\x03"));
    (void)rest_of(p);
    auto w = sh.wait();
    EXPECT_FALSE(w);
    ASSERT_TRUE(sh.state);
    EXPECT_TRUE(sh.state->signaled());
    EXPECT_EQ(sh.state->signal(), SIGINT);
    EXPECT_LT(clock::now() - started, 10s);
}

TEST(IoPty_Tests, TheSessionHasTheDefaultSignals) {
    // a program started in the background by a shell ignores SIGINT, and an
    // ignored signal stays ignored in its children: the terminal's session
    // gets every signal back to its default, and none blocked
    struct sigaction ignore = {};
    struct sigaction old = {};
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    ::sigaction(SIGINT, &ignore, &old);
    sigset_t quit;
    sigset_t mask;
    sigemptyset(&quit);
    sigaddset(&quit, SIGQUIT);
    ::pthread_sigmask(SIG_BLOCK, &quit, &mask);
    io::pty p = io::open_pty().value();
    io::command sh("/bin/sh", "-c", "echo ready; sleep 30");
    auto started = p.start(sh);
    ::sigaction(SIGINT, &old, nullptr);
    ::pthread_sigmask(SIG_SETMASK, &mask, nullptr);
    ASSERT_TRUE(started);
    std::string seen;
    read_until(p, seen, "ready\r\n");
    ASSERT_TRUE(p.write("\x03"));
    (void)rest_of(p);
    (void)sh.wait();
    ASSERT_TRUE(sh.state);
    EXPECT_EQ(sh.state->signal(), SIGINT);
    // a plain command keeps what the program gives it, as Go's does
    ::sigaction(SIGINT, &ignore, &old);
    io::command plain("/bin/sh", "-c", "kill -INT $$; echo survived");
    auto out = plain.output();
    ::sigaction(SIGINT, &old, nullptr);
    ASSERT_TRUE(out);
    EXPECT_EQ(*out, "survived\n");
}

TEST(IoPty_Tests, CloseHangsUp) {
    io::pty p = io::open_pty().value();
    io::command sh("/bin/sh", "-c", "echo ready; sleep 30");
    ASSERT_TRUE(p.start(sh));
    std::string seen;
    read_until(p, seen, "ready\r\n");
    ASSERT_TRUE(p.close());
    (void)sh.wait();
    ASSERT_TRUE(sh.state);
    EXPECT_TRUE(sh.state->signaled());
    EXPECT_EQ(sh.state->signal(), SIGHUP);
    // the closed pseudo-terminal answers errc::closed
    byte b[4];
    auto r = p.read(slice<byte>(b, 4));
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_closed());
    EXPECT_FALSE(p.write("x"));
    auto rs = p.resize({});
    ASSERT_FALSE(rs);
    EXPECT_EQ(rs.error().code(), errc::closed);
    EXPECT_FALSE(p.size());
    io::command again("/bin/sh", "-c", "true");
    auto st = p.start(again);
    ASSERT_FALSE(st);
    EXPECT_EQ(st.error().code(), errc::closed);
    EXPECT_FALSE(again.process);
}

TEST(IoPty_Tests, StreamsTheCommandSetStayItsOwn) {
    io::pty p = io::open_pty().value();
    io::buffer out;
    io::command sh("/bin/sh", "-c", "test -t 0 && echo in-is-a-terminal; test -t 1 || echo out-is-not; echo err-here >&2");
    sh.out = out;
    ASSERT_TRUE(p.start(sh));
    std::string term = rest_of(p);
    ASSERT_TRUE(sh.wait());
    EXPECT_EQ(out.text(), "in-is-a-terminal\nout-is-not\n");   // no \r: the buffer is no terminal
    EXPECT_TRUE(holds(term, "err-here\r\n")) << term;
}

TEST(IoPty_Tests, AFailedStartLeavesTheCommandAsItWas) {
    io::pty p = io::open_pty().value();
    io::command none("/no/such/program-sgcl");
    auto r = p.start(none);
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_not_found());
    EXPECT_FALSE(none.in);
    EXPECT_FALSE(none.out);
    EXPECT_FALSE(none.err);
    EXPECT_FALSE(none.process);
    EXPECT_TRUE(p.terminal().fd() >= 0);   // kept for the next start
    io::command sh("/bin/sh", "-c", "echo second");
    ASSERT_TRUE(p.start(sh));
    EXPECT_TRUE(holds(rest_of(p), "second\r\n"));
    ASSERT_TRUE(sh.wait());
}

TEST(IoPty_Tests, ASecondCommandOpensTheTerminalAgain) {
    io::pty p = io::open_pty().value();
    io::command first("/bin/sh", "-c", "echo one");
    ASSERT_TRUE(p.start(first));
    std::string seen;
    read_until(p, seen, "one\r\n");
    ASSERT_TRUE(first.wait());
    io::command second("/bin/sh", "-c", "echo two; stty size");
    auto r = p.start(second);
    ASSERT_TRUE(r) << r.error().message();
    std::string out = rest_of(p);
    EXPECT_TRUE(holds(out, "two\r\n")) << out;
    EXPECT_TRUE(holds(out, "24 80\r\n")) << out;   // the size the pty was given, though the terminal was opened again
    ASSERT_TRUE(second.wait());
    // a command started twice is refused by command itself
    auto again = p.start(second);
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().code(), errc::process_done);
}

TEST(IoPty_Tests, AsAnIoReaderAndWriter) {
    io::pty p = io::open_pty().value();
    io::command cat("/bin/cat");
    ASSERT_TRUE(p.start(cat));
    io::reader r = p;
    io::writer w = p;
    EXPECT_EQ(r.fd(), p.fd());
    ASSERT_TRUE(io::write(w, "abc\n\x04"));   // a line, then ^D: the end of cat's input
    auto all = io::read_all_text(r);
    ASSERT_TRUE(all) << all.error().message();
    std::string text(all->view());
    auto first = text.find("abc\r\n");
    ASSERT_NE(first, std::string::npos) << text;
    EXPECT_NE(text.find("abc\r\n", first + 1), std::string::npos) << text;   // the echo, then cat's copy
    ASSERT_TRUE(cat.wait());
}

namespace {
    sgcl::async::task<std::string> conversation() {
        io::pty p = io::open_pty({.rows = 12, .columns = 34}).value();
        io::command sh("/bin/sh", "-c", "read x; echo got:$x; stty size");
        if (auto s = p.start(sh); !s) {
            co_return "start: " + std::string(s.error().message().view());
        }
        if (auto w = co_await p.async_write(slice<const byte>(string("hi\n"))); !w) {
            co_return "write";
        }
        std::string out;
        for (;;) {
            vector<byte> b(256);
            auto n = co_await p.async_read(b);
            if (!n) {
                co_return "read: " + std::string(n.error().message().view());
            }
            if (*n == 0) {
                break;
            }
            out.append(reinterpret_cast<const char*>(b.data()), *n);
        }
        auto w = co_await sh.async_wait();
        co_return out + (w ? "ok" : "failed");
    }
}

TEST(IoPty_Tests, FromATask) {
    auto out = sgcl::async::spawn(conversation()).wait();
    EXPECT_TRUE(holds(out, "got:hi\r\n")) << out;
    EXPECT_TRUE(holds(out, "12 34\r\n")) << out;
    EXPECT_TRUE(out.ends_with("ok")) << out;
}
