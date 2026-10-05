//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ssh's client against its server on the loopback: every key
// exchange, cipher, MAC and host key algorithm (certificates included),
// every authentication method and its failures, sessions (exec, shell,
// subsystem, a terminal and its size, the environment, signals, an exit by
// a signal, streams of megabytes both ways, many at once), forwarding both
// ways, rekeying forced by a small limit, compression, keepalive, the
// host key's checks, the limits of the server, strict KEX against packets
// injected into the first exchange, a corrupted stream, timeouts, stops
// and closes.
#include "tests/net/ssh/helpers.h"

#include <atomic>
#include <chrono>
#include <thread>

using namespace sgcl;
using namespace sgcl_ssh_test;
namespace d = sgcl::net::ssh::detail;

namespace {
    std::string run_echo(const net::ssh::client& c, const std::string& cmd = "hello") {
        auto r = c.run(sgcl::string(cmd));
        if (!r) {
            return "error: " + text(r.error().message());
        }
        return text(r->out) + "|" + text(r->err) + "|" + std::to_string(r->status.code);
    }

    uint64_t kexes(const net::ssh::client& c) {
        return d::ClientAccess::conn(c)->kex_count();
    }
}

TEST(SshLoopback, RunOneLine) {
    LocalServer s(echo_server());
    auto c = net::ssh::client::connect(s.address(), client_options());
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(run_echo(*c), "cmd=hello sub= in=|err|0");
    EXPECT_EQ(run_echo(*c, "exit 42"), "cmd=exit 42 sub= in=|err|42");
    EXPECT_EQ(c->user(), "user");
    EXPECT_EQ(text(c->server_version()), std::string(d::SoftwareVersion));
    EXPECT_EQ(c->host_key(), key("ed25519").public_key());
    EXPECT_FALSE(c->is_closed());
    EXPECT_TRUE(c->close());
    EXPECT_TRUE(c->is_closed());
    EXPECT_FALSE(c->run(sgcl::string("x")));
}

TEST(SshLoopback, EveryKexCipherMacAndHostKey) {
    net::ssh::server srv = echo_server();
    srv.host_keys = {key("ed25519"), key("p256"), key("p384_enc", "correct horse"), key("rsa")};
    srv.kex = {};
    for (const auto& k : d::kex_table) {
        srv.kex.push_back(sgcl::string(k.name));
    }
    LocalServer s(srv);
    for (const auto& k : d::kex_table) {
        auto o = client_options();
        o.kex = {sgcl::string(k.name)};
        auto c = net::ssh::client::connect(s.address(), o);
        ASSERT_TRUE(c) << k.name << ": " << text(c.error().message());
        EXPECT_EQ(run_echo(*c), "cmd=hello sub= in=|err|0") << k.name;
    }
    for (const auto& ci : d::cipher_table) {
        for (const auto& m : d::mac_table) {
            auto o = client_options();
            o.ciphers = {sgcl::string(ci.name)};
            o.macs = {sgcl::string(m.name)};
            auto c = net::ssh::client::connect(s.address(), o);
            ASSERT_TRUE(c) << ci.name << " " << m.name << ": " << text(c.error().message());
            EXPECT_EQ(run_echo(*c), "cmd=hello sub= in=|err|0") << ci.name << " " << m.name;
            if (ci.aead) {
                break;
            }
        }
    }
    for (const char* hk : {"ssh-ed25519", "ecdsa-sha2-nistp256", "ecdsa-sha2-nistp384", "rsa-sha2-512", "rsa-sha2-256"}) {
        auto o = client_options();
        o.host_key_algorithms = {sgcl::string(hk)};
        auto c = net::ssh::client::connect(s.address(), o);
        ASSERT_TRUE(c) << hk << ": " << text(c.error().message());
        EXPECT_EQ(text(c->host_key().type_name()), std::string(std::string_view(hk).substr(0, 4) == "rsa-" ? "ssh-rsa" : hk));
        EXPECT_EQ(run_echo(*c), "cmd=hello sub= in=|err|0") << hk;
    }
}

TEST(SshLoopback, ClientKeysOfEveryKind) {
    LocalServer s(echo_server());
    for (const char* k : {"ed25519", "p256", "rsa", "rsa_pkcs8.pem", "p256_sec1.pem", "ed25519_pkcs8.pem"}) {
        auto o = client_options();
        o.keys = {key(k)};
        auto c = net::ssh::client::connect(s.address(), o);
        ASSERT_TRUE(c) << k << ": " << text(c.error().message());
        EXPECT_EQ(run_echo(*c), "cmd=hello sub= in=|err|0") << k;
    }
}

TEST(SshLoopback, AuthenticationMethodsAndFailures) {
    net::ssh::server srv = echo_server();
    srv.check_public_key = [](const sgcl::string& user, const net::ssh::public_key& k) {
        return user == "user" && k == net::ssh::public_key::parse(sgcl::string(read_data("p256.pub")))->with_comment(sgcl::string());
    };
    srv.check_keyboard_interactive = [](const sgcl::string& user, const vector<sgcl::string>& answers) {
        return user == "kbd" && answers.size() == 2 && answers[0] == "one" && answers[1] == "two";
    };
    srv.prompts = {net::ssh::prompt{sgcl::string("First: "), true}, net::ssh::prompt{sgcl::string("Second: "), false}};
    srv.max_auth_tries = 3;
    LocalServer s(srv);
    // a key the server takes, after one it does not
    auto o = client_options();
    o.keys = {key("ed25519"), key("p256")};
    auto c = net::ssh::client::connect(s.address(), o);
    ASSERT_TRUE(c) << text(c.error().message());
    // a password
    o = client_options();
    o.keys = {};
    o.password = "secret";
    ASSERT_TRUE(net::ssh::client::connect(s.address(), o));
    // keyboard-interactive: the prompts as the server sent them
    o = client_options();
    o.keys = {};
    o.user = "kbd";
    std::string seen;
    o.keyboard_interactive = [&seen](const sgcl::string&, const sgcl::string&, const vector<net::ssh::prompt>& p) {
        for (auto& x : p) {
            seen += text(x.text) + (x.echo ? "+" : "-");
        }
        return vector<sgcl::string>{sgcl::string("one"), sgcl::string("two")};
    };
    ASSERT_TRUE(net::ssh::client::connect(s.address(), o));
    EXPECT_EQ(seen, "First: +Second: -");
    // failures: what was tried and what the server takes
    o = client_options();
    o.password = "wrong";
    auto f = net::ssh::client::connect(s.address(), o);
    ASSERT_FALSE(f);
    EXPECT_EQ(f.error().code(), net::errc::ssh_auth_failed);
    EXPECT_NE(text(f.error().message()).find("publickey, password"), std::string::npos) << text(f.error().message());
    EXPECT_NE(text(f.error().message()).find("keyboard-interactive"), std::string::npos);
    // nothing to try
    o = client_options();
    o.keys = {};
    auto n = net::ssh::client::connect(s.address(), o);
    ASSERT_FALSE(n);
    EXPECT_EQ(n.error().code(), net::errc::ssh_auth_failed);
}

TEST(SshLoopback, TooManyFailuresEndTheConnection) {
    net::ssh::server srv = echo_server();
    srv.check_public_key = [](const sgcl::string&, const net::ssh::public_key&) { return false; };
    srv.max_auth_tries = 2;
    LocalServer s(srv);
    auto o = client_options();
    o.keys = {key("ed25519"), key("p256"), key("rsa")};
    o.password = "secret";   // never reached: the server ends it after two failures
    auto c = net::ssh::client::connect(s.address(), o);
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().code(), net::errc::ssh_auth_failed) << text(c.error().message());
}

TEST(SshLoopback, NoClientAuth) {
    net::ssh::server srv = echo_server();
    srv.no_client_auth = true;
    srv.check_public_key = nullptr;
    srv.check_password = nullptr;
    LocalServer s(srv);
    auto o = client_options();
    o.keys = {};
    auto c = net::ssh::client::connect(s.address(), o);
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(run_echo(*c), "cmd=hello sub= in=|err|0");
}

TEST(SshLoopback, UserCertificates) {
    net::ssh::server srv = echo_server();
    auto keys = net::ssh::authorized_keys::parse(sgcl::string("cert-authority " + read_data("ca.pub")));
    srv.check_public_key = [keys](const sgcl::string& user, const net::ssh::public_key& k) { return keys.allows(user, k); };
    LocalServer s(srv);
    auto o = client_options();
    o.user = "alice";
    o.certificates = {pub("ed25519-cert.pub")};
    auto c = net::ssh::client::connect(s.address(), o);
    ASSERT_TRUE(c) << text(c.error().message());
    // a user the certificate is not for: the certificate is refused, the plain key too
    o.user = "mallory";
    EXPECT_FALSE(net::ssh::client::connect(s.address(), o));
    // without the certificate the plain key is not in authorized_keys
    o.user = "alice";
    o.certificates = {};
    EXPECT_FALSE(net::ssh::client::connect(s.address(), o));
}

TEST(SshLoopback, HostKeyChecks) {
    net::ssh::server srv = echo_server();
    srv.host_keys = {key("p256")};
    LocalServer s(srv);
    auto o = client_options();
    o.insecure_ignore_host_key = false;
    o.known_hosts = net::ssh::known_hosts::parse(sgcl::string("[127.0.0.1]:" + std::to_string(s.port) + " " + read_data("p256.pub")));
    EXPECT_TRUE(net::ssh::client::connect(s.address(), o));
    o.known_hosts = net::ssh::known_hosts::parse(sgcl::string("[127.0.0.1]:" + std::to_string(s.port) + " " + read_data("host.pub")));
    auto m = net::ssh::client::connect(s.address(), o);
    ASSERT_FALSE(m);
    EXPECT_EQ(m.error().code(), net::errc::ssh_host_key_mismatch);
    o.known_hosts = net::ssh::known_hosts();
    auto u = net::ssh::client::connect(s.address(), o);
    ASSERT_FALSE(u);
    EXPECT_EQ(u.error().code(), net::errc::ssh_host_key_unknown);
    // the callback decides, with the address and the key
    std::string asked;
    o.host_key_callback = [&asked](const sgcl::string& address, const net::ssh::public_key& k) -> expected<void, io::error> {
        asked = text(address) + " " + text(k.type_name());
        return {};
    };
    EXPECT_TRUE(net::ssh::client::connect(s.address(), o));
    EXPECT_EQ(asked, "127.0.0.1:" + std::to_string(s.port) + " ecdsa-sha2-nistp256");
    o.host_key_callback = [](const sgcl::string&, const net::ssh::public_key&) -> expected<void, io::error> {
        return unexpected(io::error(net::errc::ssh_host_key_unknown, "test"));
    };
    EXPECT_EQ(net::ssh::client::connect(s.address(), o).error().code(), net::errc::ssh_host_key_unknown);
}

TEST(SshLoopback, HostCertificates) {
    net::ssh::server srv = echo_server();
    srv.host_keys = {key("host")};
    srv.host_certificates = {pub("host-cert.pub")};
    LocalServer s(srv);
    auto o = client_options();
    o.insecure_ignore_host_key = false;
    o.known_hosts = net::ssh::known_hosts::parse(sgcl::string("@cert-authority [127.0.0.1]:* " + read_data("ca.pub")));
    auto c = net::ssh::client::connect(s.address(), o);
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_TRUE(c->host_key().is_certificate());
    // the plain key offered alone is not known
    o.host_key_algorithms = {sgcl::string("ecdsa-sha2-nistp256")};
    EXPECT_EQ(net::ssh::client::connect(s.address(), o).error().code(), net::errc::ssh_host_key_unknown);
}

TEST(SshLoopback, SessionRequestsAndStreams) {
    net::ssh::server srv = echo_server();
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        std::string info = std::to_string(int(s.kind())) + " " + text(s.command()) + text(s.subsystem());
        if (auto p = s.pty()) {
            info += " pty " + text(p->term) + " " + std::to_string(p->columns) + "x" + std::to_string(p->rows) + " modes " + std::to_string(p->modes.size());
        }
        for (auto& e : s.env()) {
            info += " " + text(e.first) + "=" + text(e.second);
        }
        co_await s.output().async_write(sgcl::string(info + "\n"));
        // the input echoed line by line until its end; a window change and a
        // signal reported as they come
        vector<byte> buf(4096);
        auto in = s.input();
        for (;;) {
            auto n = co_await in.async_read(buf.as_slice());
            if (!n || *n == 0) {
                break;
            }
            std::string got(reinterpret_cast<const char*>(buf.data()), *n);
            if (got.find("size?") != std::string::npos) {
                auto p = s.pty();
                got = std::to_string(p->columns) + "x" + std::to_string(p->rows) + "\n";
            } else if (got.find("signal?") != std::string::npos) {
                got = text(s.last_signal()) + "\n";
            }
            co_await s.output().async_write(slice<const byte>(std::string_view(got)));
        }
        if (s.kind() == net::ssh::session_kind::shell) {
            co_await s.async_exit_signal(sgcl::string("TERM"), true, sgcl::string("stopped"));
        }
    });
    LocalServer s(srv);
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto sess = c.open_session();
    ASSERT_TRUE(sess);
    net::ssh::pty p;
    p.term = "vt100";
    p.columns = 132;
    p.rows = 43;
    p.modes = {{53, 0}, {50, 1}};
    ASSERT_TRUE(sess->request_pty(p));
    ASSERT_TRUE(sess->set_env(sgcl::string("LANG"), sgcl::string("C")));
    ASSERT_TRUE(sess->shell());
    EXPECT_FALSE(sess->exec(sgcl::string("again")));   // a second start refused
    auto out = io::buffered_reader(sess->output());
    auto line = out.read_line();
    ASSERT_TRUE(line && *line);
    EXPECT_EQ(std::string(**line), "1  pty vt100 132x43 modes 2 LANG=C");
    ASSERT_TRUE(sess->window_change(200, 50));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));   // the request is not answered: give it time to arrive
    ASSERT_TRUE(sess->input().write(sgcl::string("size?")));
    line = out.read_line();
    EXPECT_EQ(std::string(**line), "200x50");
    ASSERT_TRUE(sess->signal(sgcl::string("INT")));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(sess->input().write(sgcl::string("signal?")));
    line = out.read_line();
    EXPECT_EQ(std::string(**line), "INT");
    ASSERT_TRUE(sess->close_input());
    auto st = sess->wait();
    ASSERT_TRUE(st);
    EXPECT_EQ(st->code, -1);
    EXPECT_EQ(st->signal, "TERM");
    EXPECT_TRUE(st->core_dumped);
    EXPECT_EQ(st->message, "stopped");
    // a subsystem
    auto sub = c.open_session();
    ASSERT_TRUE(sub->subsystem(sgcl::string("sftp")));
    ASSERT_TRUE(sub->close_input());
    auto all = sub->output().read_all_text();
    EXPECT_EQ(text(*all), "2 sftp\n");
    EXPECT_EQ(sub->wait()->code, 0);
}

TEST(SshLoopback, MegabytesBothWaysAtOnce) {
    net::ssh::server srv = echo_server();
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        // what comes in goes out on both streams, as it comes
        vector<byte> buf(65536);
        auto in = s.input();
        auto out = s.output();
        auto err = s.error_output();
        for (;;) {
            auto n = co_await in.async_read(buf.as_slice());
            if (!n || *n == 0) {
                break;
            }
            co_await out.async_write(buf.as_slice().subslice(0, *n));
            co_await err.async_write(buf.as_slice().subslice(0, *n));
        }
    });
    LocalServer s(srv);
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto sess = *c.open_session();
    ASSERT_TRUE(sess.exec(sgcl::string("cat")));
    const size_t size = 8 * 1024 * 1024;
    vector<byte> data(size);
    for (size_t i = 0; i < size; ++i) {
        data[i] = byte(uint8_t(i * 131 + (i >> 13)));
    }
    std::atomic<size_t> out_bytes{0}, err_bytes{0};
    std::atomic<bool> out_ok{true}, err_ok{true};
    auto reader = [&](io::reader r, std::atomic<size_t>& count, std::atomic<bool>& ok) {
        vector<byte> buf(100000);
        size_t at = 0;
        for (;;) {
            auto n = r.read(buf.as_slice());
            if (!n || *n == 0) {
                break;
            }
            for (size_t i = 0; i < *n; ++i) {
                if (buf[i] != data[at + i]) {
                    ok = false;
                }
            }
            at += *n;
        }
        count = at;
    };
    thread t1([&] { reader(sess.output(), out_bytes, out_ok); });
    thread t2([&] { reader(sess.error_output(), err_bytes, err_ok); });
    ASSERT_TRUE(sess.input().write(data.as_slice()));
    ASSERT_TRUE(sess.close_input());
    t1.join();
    t2.join();
    EXPECT_EQ(out_bytes.load(), size);
    EXPECT_EQ(err_bytes.load(), size);
    EXPECT_TRUE(out_ok.load());
    EXPECT_TRUE(err_ok.load());
    EXPECT_EQ(sess.wait()->code, 0);
}

TEST(SshLoopback, WaitDropsWhatIsNotRead) {
    net::ssh::server srv = echo_server();
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        vector<byte> block(65536);
        for (int i = 0; i < 100; ++i) {   // 6.5 MB: past any window
            if (!co_await s.output().async_write(block.as_slice())) {
                break;
            }
        }
        co_await s.async_exit(5);
    });
    LocalServer s(srv);
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto sess = *c.open_session();
    ASSERT_TRUE(sess.exec(sgcl::string("flood")));
    auto st = sess.wait();
    ASSERT_TRUE(st);
    EXPECT_EQ(st->code, 5);
}

TEST(SshLoopback, ManySessionsAndTheLimit) {
    net::ssh::server srv = echo_server();
    srv.max_sessions = 4;
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        (void)co_await s.input().async_read_all();
        co_await s.output().async_write(s.command());
    });
    LocalServer s(srv);
    auto c = *net::ssh::client::connect(s.address(), client_options());
    // four held open, the fifth refused
    vector<net::ssh::session> held;
    for (int i = 0; i < 4; ++i) {
        held.push_back(*c.open_session());
        ASSERT_TRUE(held.back().exec(sgcl::string(std::to_string(i))));
    }
    auto fifth = c.open_session();
    ASSERT_FALSE(fifth);
    EXPECT_EQ(fifth.error().code(), net::errc::ssh_channel_refused);
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(held[i].close_input());
        EXPECT_EQ(text(*held[i].output().read_all_text()), std::to_string(i));
        EXPECT_EQ(held[i].wait()->code, 0);
    }
    // in tasks, many after one another and side by side
    std::atomic<int> good{0};
    std::mutex m;
    std::string first_error;
    vector<thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.push_back(thread([&, t] {
            for (int i = 0; i < 10; ++i) {
                auto r = c.run(sgcl::string("t" + std::to_string(t) + "-" + std::to_string(i)));
                if (r && text(r->out) == "t" + std::to_string(t) + "-" + std::to_string(i)) {
                    ++good;
                } else if (!r) {
                    std::lock_guard<std::mutex> g(m);
                    first_error = text(r.error().message());
                }
            }
        }));
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(good.load(), 40) << first_error;
}

TEST(SshLoopback, DirectTcpip) {
    EchoServer echo;
    net::ssh::server srv = echo_server();
    std::string asked;
    srv.allow_direct_tcpip = [&asked](const sgcl::string& user, const sgcl::string& host, uint16_t port) {
        asked = text(user) + "@" + text(host) + ":" + std::to_string(port);
        return port != 1;
    };
    LocalServer s(srv);
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto conn = c.dial(sgcl::string("127.0.0.1:" + std::to_string(echo.port)));
    ASSERT_TRUE(conn) << text(conn.error().message());
    EXPECT_EQ(asked, "user@127.0.0.1:" + std::to_string(echo.port));
    EXPECT_EQ(round_trip(*conn, "ping through the server"), "ping through the server");
    EXPECT_EQ(conn->remote_endpoint().port(), echo.port);
    // a deadline
    conn->set_read_deadline(sgcl::clock::now() + 50 * millisecond);
    vector<byte> one(1);
    auto r = conn->read(one.as_slice());
    ASSERT_FALSE(r);
    EXPECT_TRUE(r.error().is_timeout());
    conn->set_read_deadline(time_point());
    // close_write: the echo ends, its end reaches us
    ASSERT_TRUE(conn->write(sgcl::string("last")));
    ASSERT_TRUE(conn->close_write());
    EXPECT_EQ(text(*conn->read_all_text()), "last");
    EXPECT_TRUE(conn->close());
    // refused by the callback, and a port nobody listens on
    auto refused = c.dial(sgcl::string("127.0.0.1:1"));
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), net::errc::ssh_channel_refused);
    auto bad = c.dial(sgcl::string("no port"));
    EXPECT_EQ(bad.error().code(), net::errc::invalid_address);
    // without the callback nothing is forwarded
    LocalServer plain(echo_server());
    auto c2 = *net::ssh::client::connect(plain.address(), client_options());
    EXPECT_EQ(c2.dial(sgcl::string("127.0.0.1:" + std::to_string(echo.port))).error().code(), net::errc::ssh_channel_refused);
}

TEST(SshLoopback, TcpipForward) {
    net::ssh::server srv = echo_server();
    srv.allow_tcpip_forward = [](const sgcl::string&, const sgcl::string& host, uint16_t) { return host == "127.0.0.1"; };
    LocalServer s(srv);
    auto c = *net::ssh::client::connect(s.address(), client_options());
    auto l = c.listen(sgcl::string("127.0.0.1:0"));
    ASSERT_TRUE(l) << text(l.error().message());
    uint16_t port = l->local_endpoint().port();
    ASSERT_NE(port, 0);
    // a connection to the server's port comes here
    thread t([port] {
        auto conn = net::tcp::connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), port));
        if (conn) {
            (void)round_trip(*conn, "");
            (void)conn->write(sgcl::string("hello from outside"));
            (void)conn->close_write();
            (void)conn->read_all();
            (void)conn->close();
        }
    });
    auto in = l->accept();
    ASSERT_TRUE(in) << text(in.error().message());
    vector<byte> buf(18);
    ASSERT_TRUE(in->read_full(buf.as_slice()));
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(buf.data()), 18), "hello from outside");
    ASSERT_TRUE(in->write(sgcl::string("bye")));
    (void)in->close();
    t.join();
    // closed: the port is given back
    ASSERT_TRUE(l->close());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(net::tcp::connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), port)));
    // refused by the callback
    auto refused = c.listen(sgcl::string("0.0.0.0:0"));
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), net::errc::ssh_request_refused);
}

TEST(SshLoopback, RekeyingByBytesBothSides) {
    net::ssh::server srv = echo_server();
    srv.rekey_bytes = 64 * 1024;
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        vector<byte> buf(32768);
        for (;;) {
            auto n = co_await s.input().async_read(buf.as_slice());
            if (!n || *n == 0) {
                break;
            }
            co_await s.output().async_write(buf.as_slice().subslice(0, *n));
        }
    });
    LocalServer s(srv);
    for (int side = 0; side < 2; ++side) {
        auto o = client_options();
        if (side == 1) {
            o.rekey_bytes = 50 * 1000;   // the client starts some of them
        }
        auto c = *net::ssh::client::connect(s.address(), o);
        auto sess = *c.open_session();
        ASSERT_TRUE(sess.exec(sgcl::string("cat")));
        const size_t size = 2 * 1024 * 1024;
        vector<byte> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = byte(uint8_t(i % 251));
        }
        size_t got = 0;
        bool same = true;
        thread t([&] {
            vector<byte> buf(65536);
            auto out = sess.output();
            for (;;) {
                auto n = out.read(buf.as_slice());
                if (!n || *n == 0) {
                    break;
                }
                for (size_t i = 0; i < *n; ++i) {
                    same &= buf[i] == data[got + i];
                }
                got += *n;
            }
        });
        ASSERT_TRUE(sess.input().write(data.as_slice()));
        ASSERT_TRUE(sess.close_input());
        t.join();
        EXPECT_EQ(got, size);
        EXPECT_TRUE(same);
        EXPECT_GT(kexes(c), 10u) << side;
    }
}

TEST(SshLoopback, Compression) {
    net::ssh::server srv = echo_server();
    srv.compression = true;
    LocalServer s(srv);
    auto o = client_options();
    o.compression = true;
    auto c = *net::ssh::client::connect(s.address(), o);
    std::string big(200000, 'z');
    auto r = c.run(sgcl::string(big));
    ASSERT_TRUE(r) << text(r.error().message());
    EXPECT_EQ(text(r->out), "cmd=" + big + " sub= in=");
    EXPECT_EQ(run_echo(c), "cmd=hello sub= in=|err|0");
    // with key exchanges every 64 KB: the streams start again at each one
    net::ssh::server rs = echo_server();
    rs.compression = true;
    rs.rekey_bytes = 64 * 1024;
    rs.handle([](net::ssh::server_session s) -> async::task<> {
        auto all = co_await s.input().async_read_all();
        co_await s.output().async_write(all->as_slice());
    });
    LocalServer rekeying(rs);
    auto c3 = *net::ssh::client::connect(rekeying.address(), o);
    auto sess = *c3.open_session();
    ASSERT_TRUE(sess.exec(sgcl::string("cat")));
    vector<byte> data(1 << 20);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = byte(uint8_t((i * 7) ^ (i >> 9)));
    }
    thread writer([&] {
        (void)sess.input().write(data.as_slice());
        (void)sess.close_input();
    });
    auto back = sess.output().read_all();
    writer.join();
    ASSERT_TRUE(back);
    EXPECT_TRUE(*back == data);
    EXPECT_GT(kexes(c3), 2u);
    // offered by one side alone: none
    auto o2 = client_options();
    o2.compression = true;
    LocalServer plain(echo_server());
    auto c2 = *net::ssh::client::connect(plain.address(), o2);
    EXPECT_EQ(run_echo(c2), "cmd=hello sub= in=|err|0");
}

TEST(SshLoopback, KeepaliveAndWait) {
    net::ssh::server srv = echo_server();
    srv.keepalive_interval = 50 * millisecond;
    LocalServer s(srv);
    auto o = client_options();
    o.keepalive_interval = 50 * millisecond;
    auto c = *net::ssh::client::connect(s.address(), o);
    EXPECT_TRUE(c.keepalive());
    std::this_thread::sleep_for(std::chrono::milliseconds(300));   // keepalives both ways while idle
    EXPECT_EQ(run_echo(c), "cmd=hello sub= in=|err|0");
    thread t([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        s.srv.close();
    });
    auto w = c.wait();
    t.join();
    EXPECT_FALSE(w);
    EXPECT_TRUE(c.is_closed());
}

TEST(SshLoopback, LoginGraceAndSilentPeers) {
    net::ssh::server srv = echo_server();
    srv.login_grace_time = 200 * millisecond;
    LocalServer s(srv);
    // a client that says nothing is dropped
    auto conn = *net::tcp::connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), s.port));
    auto start = std::chrono::steady_clock::now();
    auto all = conn.read_all();
    auto took = std::chrono::steady_clock::now() - start;
    EXPECT_LT(took, std::chrono::seconds(3));
    // a server that says nothing: the client's timeout
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto o = client_options();
    o.timeout = 200 * millisecond;
    auto c = net::ssh::client::connect(sgcl::string("127.0.0.1:" + std::to_string(l.local_endpoint().port())), o);
    ASSERT_FALSE(c);
    EXPECT_TRUE(c.error().is_timeout()) << text(c.error().message());
    (void)l.close();
}

TEST(SshLoopback, StopsAndCloses) {
    LocalServer s(echo_server());
    // a stop requested before
    auto o = client_options();
    async::stop_source stop;
    stop.request_stop();
    o.stop = stop.token();
    auto c = net::ssh::client::connect(s.address(), o);
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().code(), std::errc::operation_canceled);
    // a stop during the handshake of a server that never answers
    auto l = *net::tcp::listen("127.0.0.1:0");
    async::stop_source later;
    auto o2 = client_options();
    o2.stop = later.token();
    thread t([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        later.request_stop();
    });
    auto c2 = net::ssh::client::connect(sgcl::string("127.0.0.1:" + std::to_string(l.local_endpoint().port())), o2);
    t.join();
    ASSERT_FALSE(c2);
    EXPECT_EQ(c2.error().code(), std::errc::operation_canceled) << text(c2.error().message());
    (void)l.close();
    // a session in progress ends when the client closes
    auto c3 = *net::ssh::client::connect(s.address(), client_options());
    auto sess = *c3.open_session();
    ASSERT_TRUE(sess.exec(sgcl::string("x")));
    (void)c3.close();
    vector<byte> buf(10);
    EXPECT_FALSE(sess.output().read(buf.as_slice()));
    EXPECT_FALSE(sess.wait());
    // the server's close ends its connections
    auto c4 = *net::ssh::client::connect(s.address(), client_options());
    s.srv.close();
    EXPECT_FALSE(c4.wait());
    EXPECT_FALSE(net::ssh::client::connect(s.address(), client_options()));
}

TEST(SshLoopback, ShutdownWaitsForConnections) {
    net::ssh::server srv = echo_server();
    auto l = *net::tcp::listen("127.0.0.1:0");
    uint16_t port = l.local_endpoint().port();
    async::go(serve_task(srv, l));
    auto c = *net::ssh::client::connect(sgcl::string("127.0.0.1:" + std::to_string(port)), client_options());
    std::atomic<bool> done{false};
    thread t([&] {
        srv.shutdown();
        done = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(done.load());   // the connection holds it
    EXPECT_EQ(run_echo(c), "cmd=hello sub= in=|err|0");
    (void)c.close();
    t.join();
    EXPECT_TRUE(done.load());
}

TEST(SshLoopback, ServerWithoutHostKeysRefusesToServe) {
    net::ssh::server srv;
    auto l = *net::tcp::listen("127.0.0.1:0");
    auto r = srv.serve(l);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), std::errc::invalid_argument);
}

TEST(SshLoopback, OverAConnectionInMemory) {
    auto [a, b] = net::connection::in_memory();
    net::ssh::server srv = echo_server();
    auto l = *net::tcp::listen("127.0.0.1:0");
    LocalServer s(srv);
    // the transport handed over: through a TCP connection the program made
    auto t = *net::tcp::connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), s.port));
    auto c = net::ssh::client::connect(t, client_options());
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(run_echo(*c), "cmd=hello sub= in=|err|0");
    // a dial function of the program's
    auto o = client_options();
    std::string dialed;
    o.dial = [&dialed](const sgcl::string& address, async::stop_token) -> async::task<expected<net::connection, io::error>> {
        dialed = text(address);
        return net::tcp::async_connect(address);
    };
    auto c2 = net::ssh::client::connect(s.address(), o);
    ASSERT_TRUE(c2);
    EXPECT_EQ(dialed, text(s.address()));
    (void)l.close();
    (void)a.close();
    (void)b.close();
}

namespace {
    // A plaintext packet of the first exchange
    std::string packet(const d::Bytes& payload) {
        d::PacketKeys none;
        d::Bytes out;
        none.seal(0, payload.data(), payload.size(), out);
        return std::string(reinterpret_cast<const char*>(out.data()), out.size());
    }

    // Whether the server ends a connection that sent `bytes` after its
    // version line, within a second
    bool server_drops(uint16_t port, const std::string& bytes) {
        auto c = *net::tcp::connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), port));
        (void)c.write(sgcl::string("SSH-2.0-test\r\n" + bytes));
        c.set_read_deadline(sgcl::clock::now() + second);
        vector<byte> buf(65536);
        for (;;) {
            auto n = c.read(buf.as_slice());
            if (!n) {
                return !n.error().is_timeout();
            }
            if (*n == 0) {
                return true;
            }
        }
    }
}

TEST(SshLoopback, StrictKexRefusesInjectedPackets) {
    LocalServer s(echo_server());
    auto prefs = d::default_preferences();
    d::Bytes ignore = {d::MsgIgnore, 0, 0, 0, 0};
    // a packet before the KEXINIT, with strict KEX offered
    EXPECT_TRUE(server_drops(s.port, packet(ignore) + packet(d::make_kexinit(prefs, true, true))));
    // a packet after the KEXINIT, before NEWKEYS
    EXPECT_TRUE(server_drops(s.port, packet(d::make_kexinit(prefs, true, true)) + packet(ignore)));
    // without strict KEX the same IGNORE is passed over: the server waits
    // for the exchange to go on
    EXPECT_FALSE(server_drops(s.port, packet(d::make_kexinit(prefs, true, false)) + packet(ignore)));
    // garbage and versions the server does not speak
    EXPECT_TRUE(server_drops(s.port, std::string(64, '\xff')));
    auto c = *net::tcp::connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), s.port));
    (void)c.write(sgcl::string("SSH-1.5-old\r\n"));
    c.set_read_deadline(sgcl::clock::now() + second);
    auto all = c.read_all();
    EXPECT_TRUE(all);   // the server's version line, then the end
}

namespace {
    // A proxy that flips a byte of the client's stream after `after` bytes
    async::task<> corrupt_one(net::connection from, net::connection to, size_t after) {
        vector<byte> buf(4096);
        size_t seen = 0;
        for (;;) {
            auto n = co_await from.async_read(buf.as_slice());
            if (!n || *n == 0) {
                break;
            }
            if (seen <= after && seen + *n > after) {
                buf[after - seen] = byte(uint8_t(buf[after - seen]) ^ 0x40);
            }
            seen += *n;
            if (!co_await to.async_write(buf.as_slice().subslice(0, *n))) {
                break;
            }
        }
        (void)to.close();
        (void)from.close();
    }

    async::task<> corrupting_proxy(net::listener l, uint16_t target, size_t after) {
        auto in = co_await l.async_accept();
        if (!in) {
            co_return;
        }
        auto out = co_await net::tcp::async_connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), target));
        if (!out) {
            co_return;
        }
        async::go(corrupt_one(*out, *in, SIZE_MAX));
        co_await corrupt_one(*in, *out, after);
    }
}

TEST(SshLoopback, ACorruptedStreamIsAProtocolError) {
    net::ssh::server srv = echo_server();
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        (void)co_await s.input().async_read_all();
    });
    LocalServer s(srv);
    auto l = *net::tcp::listen("127.0.0.1:0");
    async::go(corrupting_proxy(l, s.port, 20000));
    auto c = net::ssh::client::connect(sgcl::string("127.0.0.1:" + std::to_string(l.local_endpoint().port())), client_options());
    ASSERT_TRUE(c) << text(c.error().message());
    auto sess = *c->open_session();
    ASSERT_TRUE(sess.exec(sgcl::string("sink")));
    vector<byte> block(30000);
    (void)sess.input().write(block.as_slice());   // the server reads a broken MAC and disconnects
    auto w = c->wait();
    ASSERT_FALSE(w);
    EXPECT_TRUE(w.error().code() == net::errc::ssh_disconnected || w.error().code() == net::errc::ssh_protocol) << text(w.error().message());
    (void)l.close();
}

TEST(SshLoopback, AsyncFormsInATask) {
    LocalServer s(echo_server());
    auto work = [](sgcl::string address) -> async::task<std::string> {
        auto c = co_await net::ssh::client::async_connect(address, client_options());
        if (!c) {
            co_return "connect";
        }
        auto r = co_await c->async_run(sgcl::string("async"));
        if (!r) {
            co_return "run";
        }
        auto sess = co_await c->async_open_session();
        co_await sess->async_exec(sgcl::string("exit 9"));
        co_await sess->async_close_input();
        auto st = co_await sess->async_wait();
        co_await c->async_keepalive();
        co_return text(r->out) + " " + std::to_string(st->code);
    };
    EXPECT_EQ(async::spawn(work(s.address())).wait(), "cmd=async sub= in= 9");
}

TEST(SshLoopback, BannerAndAgentNotForwarded) {
    net::ssh::server srv = echo_server();
    srv.banner = "welcome\n";
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        auto a = co_await s.async_agent();
        co_await s.output().async_write(a ? sgcl::string("agent") : sgcl::string(std::to_string(int(a.error().code() == net::errc::ssh_request_refused))));
    });
    LocalServer s(srv);
    auto o = client_options();
    std::string banner;
    o.banner = [&banner](const sgcl::string& t) { banner = text(t); };
    auto c = *net::ssh::client::connect(s.address(), o);
    EXPECT_EQ(banner, "welcome\n");
    auto r = c.run(sgcl::string("x"));
    EXPECT_EQ(text(r->out), "1");
}

TEST(SshLoopback, EveryCombinationOfAlgorithms) {
    // the whole product: every key exchange x every cipher (x every MAC for
    // the ciphers that take one) x every host key algorithm
    net::ssh::server srv = echo_server();
    srv.host_keys = {key("ed25519"), key("p256"), key("p384_enc", "correct horse"), key("rsa")};
    for (const auto& k : d::kex_table) {
        srv.kex.push_back(sgcl::string(k.name));
    }
    LocalServer s(srv);
    int done = 0;
    for (const auto& k : d::kex_table) {
        for (const auto& ci : d::cipher_table) {
            for (const auto& m : d::mac_table) {
                for (const char* hk : {"ssh-ed25519", "ecdsa-sha2-nistp256", "ecdsa-sha2-nistp384", "rsa-sha2-512", "rsa-sha2-256"}) {
                    auto o = client_options();
                    o.kex = {sgcl::string(k.name)};
                    o.ciphers = {sgcl::string(ci.name)};
                    o.macs = {sgcl::string(m.name)};
                    o.host_key_algorithms = {sgcl::string(hk)};
                    auto c = net::ssh::client::connect(s.address(), o);
                    ASSERT_TRUE(c) << k.name << " " << ci.name << " " << m.name << " " << hk << ": " << text(c.error().message());
                    auto r = c->run(sgcl::string("x"));
                    ASSERT_TRUE(r) << k.name << " " << ci.name << " " << m.name << " " << hk;
                    EXPECT_EQ(text(r->out), "cmd=x sub= in=");
                    (void)c->close();
                    ++done;
                }
                if (ci.aead) {
                    break;
                }
            }
        }
    }
    EXPECT_EQ(done, 7 * (3 + 3 * 4) * 5);
}
