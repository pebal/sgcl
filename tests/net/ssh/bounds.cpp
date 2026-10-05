//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ssh at its boundaries: default and moved-from handles, empty input,
// the client's limit of channels, writes of nothing, a connection that dies
// half-way through a transfer, a server that stops answering keepalives,
// calls after a close, the public types' values at their ends.
#include "tests/net/ssh/helpers.h"

#include <atomic>
#include <chrono>
#include <thread>

using namespace sgcl;
using namespace sgcl_ssh_test;

TEST(SshBounds, DefaultAndMovedFromHandles) {
    EXPECT_FALSE(net::ssh::client());
    EXPECT_FALSE(net::ssh::session());
    EXPECT_FALSE(net::ssh::server_session());
    EXPECT_FALSE(net::ssh::private_key());
    EXPECT_FALSE(net::ssh::public_key());
    EXPECT_FALSE(net::ssh::agent());
    EXPECT_EQ(net::ssh::known_hosts().size(), 0u);
    EXPECT_EQ(net::ssh::authorized_keys().size(), 0u);
    EXPECT_TRUE(net::ssh::public_key() == net::ssh::public_key());
    EXPECT_EQ(net::ssh::public_key().type_name(), "");
    EXPECT_TRUE(net::ssh::public_key().bytes().empty());
    EXPECT_FALSE(net::ssh::public_key().is_certificate());
    EXPECT_FALSE(net::ssh::public_key().certificate());
    EXPECT_FALSE(net::ssh::public_key().verify(slice<const byte>("x"), slice<const byte>("y")));
    auto k = net::ssh::private_key::generate();
    auto moved = std::move(k);
    EXPECT_TRUE(moved);
    auto pub = moved.public_key();
    auto pub2 = std::move(pub);
    EXPECT_EQ(pub2, moved.public_key());
    // a known_hosts and an authorized_keys know nothing of an empty key
    EXPECT_EQ(net::ssh::known_hosts().check(sgcl::string("h"), net::ssh::public_key()).error().code(), net::errc::ssh_host_key_unknown);
    EXPECT_FALSE(net::ssh::authorized_keys().allows(sgcl::string("u"), net::ssh::public_key()));
}

TEST(SshBounds, EmptyInput) {
    EXPECT_FALSE(net::ssh::private_key::parse(slice<const byte>()));
    EXPECT_FALSE(net::ssh::public_key::parse(sgcl::string()));
    EXPECT_FALSE(net::ssh::public_key::from_bytes(slice<const byte>()));
    EXPECT_EQ(net::ssh::known_hosts::parse(sgcl::string()).size(), 0u);
    EXPECT_EQ(net::ssh::authorized_keys::parse(sgcl::string()).size(), 0u);
    LocalServer s(echo_server());
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto r = c.run(sgcl::string());
    ASSERT_TRUE(r);
    EXPECT_EQ(text(r->out), "cmd= sub= in=");
    EXPECT_EQ(c.dial(sgcl::string()).error().code(), net::errc::invalid_address);
    EXPECT_EQ(c.listen(sgcl::string()).error().code(), net::errc::invalid_address);
    auto sess = *c.open_session();
    EXPECT_TRUE(sess.set_env(sgcl::string(), sgcl::string()));   // the server takes it; the handler decides
    ASSERT_TRUE(sess.exec(sgcl::string("x")));
    EXPECT_EQ(*sess.input().write(slice<const byte>()), 0u);     // nothing written: no packet
    ASSERT_TRUE(sess.close_input());
    ASSERT_TRUE(sess.close_input());   // twice: nothing
    vector<byte> none;
    EXPECT_EQ(*sess.output().read(none.as_slice()), 0u);
    EXPECT_EQ(sess.wait()->code, 0);
}

TEST(SshBounds, TheClientsLimitOfChannels) {
    net::ssh::server srv = echo_server();
    srv.max_sessions = 1000;
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        (void)co_await s.input().async_read_all();
    });
    LocalServer s(srv);
    auto c = *net::ssh::client::connect(s.address(), client_options());
    vector<net::ssh::session> open;
    for (int i = 0; i < 64; ++i) {
        auto x = c.open_session();
        ASSERT_TRUE(x) << i;
        open.push_back(*x);
    }
    auto over = c.open_session();
    ASSERT_FALSE(over);
    EXPECT_EQ(over.error().code(), net::errc::ssh_channel_refused);
    // one closed: its id is free again once the server's CLOSE is through
    ASSERT_TRUE(open[0].close());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(c.open_session());
}

TEST(SshBounds, AConnectionThatDiesHalfWay) {
    net::ssh::server srv = echo_server();
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        vector<byte> buf(65536);
        size_t got = 0;
        for (;;) {
            auto n = co_await s.input().async_read(buf.as_slice());
            if (!n || *n == 0) {
                break;
            }
            got += *n;
        }
    });
    LocalServer s(srv);
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto sess = *c.open_session();
    ASSERT_TRUE(sess.exec(sgcl::string("sink")));
    thread killer([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        s.srv.close();
    });
    vector<byte> block(1 << 20);
    expected<size_t, io::error> w = size_t(0);
    for (int i = 0; i < 1000 && w; ++i) {
        w = sess.input().write(block.as_slice());
    }
    killer.join();
    ASSERT_FALSE(w);   // a write failed once the server was gone
    EXPECT_FALSE(sess.wait());
    EXPECT_TRUE(c.is_closed());
    EXPECT_FALSE(c.open_session());
    EXPECT_FALSE(c.keepalive());
}

namespace {
    // A proxy that stops passing the server's bytes when `mute` is set
    async::task<> one_way(net::connection from, net::connection to, std::atomic<bool>* mute) {
        vector<byte> buf(16384);
        for (;;) {
            auto n = co_await from.async_read(buf.as_slice());
            if (!n || *n == 0) {
                break;
            }
            if (mute && mute->load()) {
                continue;
            }
            if (!co_await to.async_write(buf.as_slice().subslice(0, *n))) {
                break;
            }
        }
        (void)to.close();
    }

    async::task<> muting_proxy(net::listener l, uint16_t target, std::atomic<bool>* mute) {
        auto in = co_await l.async_accept();
        if (!in) {
            co_return;
        }
        auto out = co_await net::tcp::async_connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), target));
        if (!out) {
            co_return;
        }
        async::go(one_way(*out, *in, mute));
        co_await one_way(*in, *out, nullptr);
    }
}

TEST(SshBounds, AServerThatStopsAnsweringKeepalives) {
    LocalServer s(echo_server());
    std::atomic<bool> mute{false};
    auto l = *net::tcp::listen("127.0.0.1:0");
    async::go(muting_proxy(l, s.port, &mute));
    auto o = client_options();
    o.keepalive_interval = 50 * millisecond;
    auto c = *net::ssh::client::connect(sgcl::string("127.0.0.1:" + std::to_string(l.local_endpoint().port())), o);
    mute = true;
    auto w = c.wait();
    ASSERT_FALSE(w);
    EXPECT_EQ(w.error().code(), net::errc::ssh_disconnected) << text(w.error().message());
    EXPECT_NE(text(w.error().message()).find("keepalive"), std::string::npos);
    (void)l.close();
}

TEST(SshBounds, CallsAfterAClose) {
    LocalServer s(echo_server());
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto sess = *c.open_session();
    ASSERT_TRUE(sess.exec(sgcl::string("x")));
    ASSERT_TRUE(sess.close());
    ASSERT_TRUE(sess.close());   // twice: nothing
    EXPECT_EQ(sess.input().write(sgcl::string("x")).error().code(), io::errc::closed);
    EXPECT_EQ(sess.signal(sgcl::string("TERM")).error().code(), io::errc::closed);
    ASSERT_TRUE(c.close());
    ASSERT_TRUE(c.close());
    EXPECT_EQ(c.wait().error().code(), io::errc::closed);
    EXPECT_EQ(c.run(sgcl::string("x")).error().code(), io::errc::closed);
    EXPECT_EQ(c.dial(sgcl::string("127.0.0.1:1")).error().code(), io::errc::closed);
}

TEST(SshBounds, PublicValuesAtTheirEnds) {
    net::ssh::exit_status st;
    EXPECT_EQ(st.code, -1);
    EXPECT_TRUE(st.signal.empty());
    net::ssh::pty p;
    EXPECT_EQ(p.term, "xterm");
    EXPECT_EQ(p.columns, 80u);
    EXPECT_EQ(p.rows, 24u);
    net::ssh::prompt q;
    EXPECT_FALSE(q.echo);
    net::ssh::client::options o;
    EXPECT_TRUE(o.agent);
    EXPECT_FALSE(o.insecure_ignore_host_key);
    EXPECT_EQ(o.rekey_bytes, uint64_t(1) << 30);
    net::ssh::server srv;
    EXPECT_EQ(srv.max_auth_tries, 6);
    EXPECT_EQ(srv.max_sessions, 10u);
    EXPECT_EQ(srv.max_packet, 32768u);
    // a terminal of the largest size and many modes goes through
    LocalServer s(echo_server());
    net::ssh::server ps = echo_server();
    ps.handle([](net::ssh::server_session x) {
        auto t = x.pty();
        (void)x.output().write(sgcl::string(std::to_string(t->columns) + " " + std::to_string(t->modes.size())));
    });
    LocalServer s2(ps);
    auto c = *net::ssh::client::connect(s2.address(), client_options());
    auto sess = *c.open_session();
    net::ssh::pty big;
    big.columns = UINT32_MAX;
    for (uint8_t op = 1; op < 160; ++op) {
        big.modes.push_back(pair<uint8_t, uint32_t>(op, UINT32_MAX));
    }
    ASSERT_TRUE(sess.request_pty(big));
    ASSERT_TRUE(sess.shell());
    EXPECT_EQ(text(*sess.output().read_all_text()), std::to_string(UINT32_MAX) + " 159");
    // a private key of RSA's smallest and largest made sizes is refused past them
    EXPECT_THROW(net::ssh::private_key::generate(net::ssh::key_type::rsa, 1024), std::invalid_argument);
}

TEST(SshBounds, SessionsClosedBeforeTheyStart) {
    // a session opened and closed without a command gives its room back
    net::ssh::server srv = echo_server();
    srv.max_sessions = 2;
    LocalServer s(srv);
    auto c = *net::ssh::client::connect(s.address(), client_options());
    for (int i = 0; i < 20; ++i) {
        auto sess = c.open_session();
        ASSERT_TRUE(sess) << i << ": " << text(sess.error().message());
        ASSERT_TRUE(sess->close());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_EQ(text(c.run(sgcl::string("still"))->out), "cmd=still sub= in=");
}
