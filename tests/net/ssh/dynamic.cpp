//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Dynamic forwarding (ssh -D): client::serve_socks5 against the module's
// own server and OpenSSH's sshd (skipped where there is none), with the
// module's SOCKS5 client and curl's through it.
#include "helpers.h"

#include "sgcl/net/socks5.h"

#include <cstdio>
#include <cstdlib>

using namespace sgcl_ssh_test;

namespace {
    std::string run_out(const std::string& cmd) {
        std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    }

    // An HTTP-ish target: one line answered and the connection closed
    async::task<> hello_loop(net::listener l) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            async::go([](net::connection c) -> async::task<> {
                char buf[1024];
                (void)co_await c.async_read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf));
                std::string r = "HTTP/1.1 200 OK\r\nContent-Length: 6\r\nConnection: close\r\n\r\nhello\n";
                (void)co_await c.async_write(sgcl::string(r));
                (void)c.close();
            }(*c));
        }
    }

    void check_dynamic(const net::ssh::client& c, uint16_t echo_port) {
        auto l = net::tcp::listen("127.0.0.1:0").value();
        auto serving = async::spawn(c.async_serve_socks5(l));
        auto conn = net::socks5::connect(l.local_endpoint().to_string(), sgcl::string("127.0.0.1:" + std::to_string(echo_port)));
        ASSERT_TRUE(conn) << text(conn.error().message());
        EXPECT_EQ(round_trip(*conn, "through ssh -D"), "through ssh -D");
        (void)conn->close();
        // a name the SSH server resolves
        auto named = net::socks5::connect(l.local_endpoint().to_string(), sgcl::string("localhost:" + std::to_string(echo_port)));
        ASSERT_TRUE(named) << text(named.error().message());
        EXPECT_EQ(round_trip(*named, "by name"), "by name");
        (void)named->close();
        // a target the server cannot reach: the SOCKS reply says so
        auto refused = net::socks5::connect(l.local_endpoint().to_string(), sgcl::string("127.0.0.1:1"));
        EXPECT_FALSE(refused);
        if (std::system("command -v curl > /dev/null 2>&1") == 0) {
            auto site = net::tcp::listen("127.0.0.1:0").value();
            async::go(hello_loop(site));
            std::string out = run_out("curl -s --max-time 10 --socks5-hostname 127.0.0.1:" + std::to_string(l.local_endpoint().port()) + " http://localhost:" +
                                      std::to_string(site.local_endpoint().port()) + "/");
            EXPECT_EQ(out, "hello\n");
            (void)site.close();
        }
        // the end of the SSH connection ends the proxy, without an error
        (void)c.close();
        auto r = serving.wait();
        EXPECT_TRUE(r) << text(r.error().message());
    }
}

TEST(SshDynamic, AgainstTheModulesServer) {
    EchoServer echo;
    net::ssh::server srv = echo_server();
    srv.allow_direct_tcpip = [](const sgcl::string&, const sgcl::string&, uint16_t port) { return port != 1; };
    LocalServer s(srv);
    auto c = *net::ssh::client::connect(s.address(), client_options());
    check_dynamic(c, echo.port);
}

TEST(SshDynamic, TheProgramsListenerAndCredentials) {
    EchoServer echo;
    net::ssh::server srv = echo_server();
    srv.allow_direct_tcpip = [](const sgcl::string&, const sgcl::string&, uint16_t) { return true; };
    LocalServer s(srv);
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto l = net::tcp::listen("127.0.0.1:0").value();
    net::socks5::server proxy;
    proxy.authenticate = [](const sgcl::string& u, const sgcl::string& p) { return u == "me" && p == "pw"; };
    auto serving = async::spawn(c.async_serve_socks5(l, proxy));
    net::socks5::options o;
    o.username = "me";
    o.password = "pw";
    auto conn = net::socks5::connect(l.local_endpoint().to_string(), sgcl::string("127.0.0.1:" + std::to_string(echo.port)), o);
    ASSERT_TRUE(conn) << text(conn.error().message());
    EXPECT_EQ(round_trip(*conn, "with credentials"), "with credentials");
    EXPECT_FALSE(net::socks5::connect(l.local_endpoint().to_string(), sgcl::string("127.0.0.1:" + std::to_string(echo.port))));
    // the listener closed by the program: server_closed, the SSH connection goes on
    (void)l.close();
    auto r = serving.wait();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), net::errc::server_closed);
    EXPECT_TRUE(c.dial(sgcl::string("127.0.0.1:" + std::to_string(echo.port))));
    (void)c.close();
}

TEST(SshDynamic, AgainstOpenSshSshd) {
    if (!have("/usr/sbin/sshd")) {
        GTEST_SKIP() << "no sshd";
    }
    Sshd sshd;
    if (!sshd.running) {
        GTEST_SKIP() << "sshd did not start";
    }
    EchoServer echo;
    auto o = client_options();
    o.user = sgcl::string(user_name());
    auto c = net::ssh::client::connect(sshd.address(), o);
    ASSERT_TRUE(c) << text(c.error().message());
    check_dynamic(*c, echo.port);
}
