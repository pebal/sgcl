//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// An SSH server that runs /bin/sh on an io::pty, the shape of an sshd: the
// client's pty-req is the terminal's size and TERM, its input typed on the
// terminal, the terminal's screen its output, its window-change the
// terminal's resize (the shell's SIGWINCH), the shell's exit status the
// session's. The client is net::ssh's own; OpenSSH's ssh against it where
// the machine has one.
#include "tests/net/ssh/helpers.h"

#include <chrono>
#include <thread>

using namespace sgcl;
using namespace sgcl_ssh_test;

namespace {
    io::terminal_size size_of(const net::ssh::pty& p) {
        auto clamp = [](uint32_t v) { return uint16_t(v > 0xFFFF ? 0xFFFF : v); };
        return io::terminal_size{clamp(p.rows), clamp(p.columns), clamp(p.width_pixels), clamp(p.height_pixels)};
    }

    // The client's input typed on the terminal, to its end
    async::task<> type_input(io::reader in, io::pty term) {
        (void)co_await io::async_copy(term, in);
    }

    // Every window-change the terminal's resize, until the session ends
    async::task<> follow_window(net::ssh::server_session s, io::pty term) {
        auto changes = s.window_changes();
        while (co_await changes.receive()) {
            if (auto p = s.pty()) {
                (void)term.resize(size_of(*p));
            }
        }
    }

    // A session's shell (or its command) on a pseudo-terminal of the size
    // the client asked for; a session without a pty-req gets the pipes
    async::task<> shell_on_pty(net::ssh::server_session s) {
        io::command sh("/bin/sh");
        if (s.kind() == net::ssh::session_kind::exec) {
            sh.args = {string("-c"), s.command()};
        }
        auto req = s.pty();
        if (!req) {
            sh.in = s.input();
            sh.out = s.output();
            sh.err = s.error_output();
            auto r = co_await sh.async_run();
            co_await s.async_exit(sh.state ? sh.state->exit_code() : 1);
            co_return;
        }
        auto opened = io::open_pty(size_of(*req));
        if (!opened) {
            co_await s.async_exit(1);
            co_return;
        }
        io::pty term = *opened;
        vector<pair<string, string>> env = {{"TERM", req->term}, {"PATH", "/usr/bin:/bin"}};
        for (auto& e : s.env()) {
            env.push_back(e);
        }
        sh.env = env;
        if (auto r = term.start(sh); !r) {
            co_await s.error_output().async_write(r.error().message() + "\n");
            co_await s.async_exit(127);
            co_return;
        }
        async::go(type_input(s.input(), term));
        async::go(follow_window(s, term));
        (void)co_await io::async_copy(s.output(), term);   // the screen, until the shell and what it left let go of the terminal
        (void)co_await sh.async_wait();
        (void)term.close();
        if (sh.state && sh.state->signaled()) {
            co_await s.async_exit_signal(string(sh.state->signal() == SIGHUP ? "HUP" : sh.state->signal() == SIGINT ? "INT" : "TERM"));
        } else {
            co_await s.async_exit(sh.state ? sh.state->exit_code() : 1);
        }
    }

    net::ssh::server pty_server() {
        net::ssh::server srv = echo_server();
        srv.handle(shell_on_pty);
        return srv;
    }

    std::string read_until(io::buffered_reader& out, std::string& seen, std::string_view what) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (seen.find(what) == std::string::npos && std::chrono::steady_clock::now() < deadline) {
            vector<byte> b(512);
            auto n = out.read(b);
            if (!n || *n == 0) {
                break;
            }
            seen.append(reinterpret_cast<const char*>(b.data()), *n);
        }
        return seen;
    }
}

TEST(SshPtyShell, AnExecOnTheTerminal) {
    LocalServer s(pty_server());
    auto c = net::ssh::client::connect(s.address(), client_options());
    ASSERT_TRUE(c) << text(c.error().message());
    auto sess = c->open_session();
    ASSERT_TRUE(sess);
    net::ssh::pty p;
    p.term = "vt220";
    p.columns = 101;
    p.rows = 33;
    ASSERT_TRUE(sess->request_pty(p));
    ASSERT_TRUE(sess->exec(string("stty size; echo $TERM; tty -s && echo on-a-terminal; exec 3</dev/tty && echo controlling; exit 3")));
    auto all = sess->output().read_all_text();
    ASSERT_TRUE(all);
    std::string out = text(*all);
    EXPECT_NE(out.find("33 101\r\n"), std::string::npos) << out;
    EXPECT_NE(out.find("vt220\r\n"), std::string::npos) << out;
    EXPECT_NE(out.find("on-a-terminal\r\n"), std::string::npos) << out;
    EXPECT_NE(out.find("controlling\r\n"), std::string::npos) << out;
    auto st = sess->wait();
    ASSERT_TRUE(st);
    EXPECT_EQ(st->code, 3);
}

TEST(SshPtyShell, AnInteractiveShell) {
    LocalServer s(pty_server());
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto sess = c.open_session();
    ASSERT_TRUE(sess);
    net::ssh::pty p;
    p.columns = 80;
    p.rows = 24;
    ASSERT_TRUE(sess->request_pty(p));
    ASSERT_TRUE(sess->shell());
    io::buffered_reader out(sess->output());
    std::string seen;
    ASSERT_TRUE(sess->input().write(string("PS1='$ '; echo ready\n")));
    read_until(out, seen, "ready\r\n");
    ASSERT_NE(seen.find("ready\r\n"), std::string::npos) << seen;
    // the window-change is the terminal's new size (an interactive shell
    // runs its WINCH trap only once its line is read: the SIGWINCH itself
    // is tests/io/pty.cpp's)
    ASSERT_TRUE(sess->window_change(120, 40));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));   // the request is not answered: give it time to arrive
    ASSERT_TRUE(sess->input().write(string("stty size\n")));
    read_until(out, seen, "40 120\r\n");
    EXPECT_NE(seen.find("40 120\r\n"), std::string::npos) << seen;
    ASSERT_TRUE(sess->input().write(string("echo $((6*7))\n")));
    read_until(out, seen, "42\r\n");
    EXPECT_NE(seen.find("42\r\n"), std::string::npos) << seen;
    ASSERT_TRUE(sess->input().write(string("exit 5\n")));
    (void)out.read_all();
    auto st = sess->wait();
    ASSERT_TRUE(st);
    EXPECT_EQ(st->code, 5);
}

TEST(SshPtyShell, ControlCReachesTheShellsCommand) {
    LocalServer s(pty_server());
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto sess = c.open_session();
    ASSERT_TRUE(sess->request_pty(net::ssh::pty{}));
    ASSERT_TRUE(sess->exec(string("echo ready; sleep 30")));
    io::buffered_reader out(sess->output());
    std::string seen;
    read_until(out, seen, "ready\r\n");
    auto started = std::chrono::steady_clock::now();
    ASSERT_TRUE(sess->input().write(string("\x03")));   // ^C, the terminal's SIGINT to its foreground
    (void)out.read_all();
    auto st = sess->wait();
    ASSERT_TRUE(st);
    EXPECT_EQ(st->signal, "INT");
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(10));
}

TEST(SshPtyShell, WithoutATerminalThePipes) {
    LocalServer s(pty_server());
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto r = c.run(string("tty -s || echo no-terminal; exit 2"));
    ASSERT_TRUE(r);
    EXPECT_EQ(text(r->out), "no-terminal\n");
    EXPECT_EQ(r->status.code, 2);
}

TEST(SshPtyShell, OpenSshAgainstIt) {
    if (!have("/usr/bin/ssh")) {
        GTEST_SKIP() << "no /usr/bin/ssh";
    }
    LocalServer s(pty_server());
    auto dir = temp_dir("pty");
    auto keyfile = (dir / "id").string();
    std::filesystem::copy_file(data_path("ed25519"), keyfile);
    std::filesystem::permissions(keyfile, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    // ssh -tt asks for a terminal though its own input is none
    std::string cmd = "/usr/bin/ssh -tt -F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o BatchMode=yes"
                      " -i " + keyfile + " -p " + std::to_string(s.port) + " user@127.0.0.1 'stty size; tty -s && echo pty-ok' </dev/null 2>&1";
    std::string out = shell(cmd);
    EXPECT_NE(out.find("pty-ok"), std::string::npos) << out;
    std::filesystem::remove_all(dir);
}
