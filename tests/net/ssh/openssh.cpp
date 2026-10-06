//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ssh against OpenSSH, the reference of the protocol: the client
// against /usr/sbin/sshd run unprivileged on a loopback port with a
// configuration, host keys and authorized_keys of its own in a temporary
// directory (every key exchange, cipher, MAC, host key and host
// certificate, user keys and certificates, sessions, a terminal, an exit
// by a signal, compression, rekeying forced by RekeyLimit, forwarding both
// ways, the agent and its forwarding); /usr/bin/ssh against the module's
// server (the same algorithms, -L, -R, -C, RekeyLimit); the agent client
// against /usr/bin/ssh-agent. Skipped where OpenSSH is not installed.
#include "tests/net/ssh/helpers.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

using namespace sgcl;
using namespace sgcl_ssh_test;

namespace {
    net::ssh::client::options sshd_options() {
        auto o = client_options();
        o.user = sgcl::string(user_name());
        return o;
    }

    std::string run(const net::ssh::client& c, const std::string& cmd) {
        auto r = c.run(sgcl::string(cmd));
        if (!r) {
            return "error: " + text(r.error().message());
        }
        return text(r->out) + "|" + text(r->err) + "|" + std::to_string(r->status.code);
    }

    bool openssh() {
        return have("/usr/sbin/sshd") && have("/usr/bin/ssh") && have("/usr/bin/ssh-keygen");
    }

    // Environment variables set for a test, restored after it
    struct Env {
        std::vector<std::pair<std::string, std::optional<std::string>>> saved;

        void set(const char* name, const std::string& value) {
            const char* old = std::getenv(name);
            saved.emplace_back(name, old ? std::optional<std::string>(old) : std::nullopt);
            ::setenv(name, value.c_str(), 1);
        }

        ~Env() {
            for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
                if (it->second) {
                    ::setenv(it->first.c_str(), it->second->c_str(), 1);
                } else {
                    ::unsetenv(it->first.c_str());
                }
            }
        }
    };
}

TEST(SshOpenSsh, ClientAgainstSshdAlgorithms) {
    if (!openssh()) {
        GTEST_SKIP() << "no OpenSSH";
    }
    Sshd sshd;
    ASSERT_TRUE(sshd.running);
    for (const char* kex : {"mlkem768x25519-sha256", "curve25519-sha256", "curve25519-sha256@libssh.org", "ecdh-sha2-nistp256", "ecdh-sha2-nistp384", "ecdh-sha2-nistp521",
                            "diffie-hellman-group16-sha512", "diffie-hellman-group14-sha256"}) {
        auto o = sshd_options();
        o.kex = {sgcl::string(kex)};
        auto c = net::ssh::client::connect(sshd.address(), o);
        ASSERT_TRUE(c) << kex << ": " << text(c.error().message());
        EXPECT_EQ(run(*c, "echo $((6*7))"), "42\n||0") << kex;
    }
    for (const char* cipher : {"chacha20-poly1305@openssh.com", "aes128-gcm@openssh.com", "aes256-gcm@openssh.com", "aes128-ctr", "aes192-ctr", "aes256-ctr"}) {
        for (const char* mac : {"hmac-sha2-256-etm@openssh.com", "hmac-sha2-512-etm@openssh.com", "hmac-sha2-256", "hmac-sha2-512"}) {
            auto o = sshd_options();
            o.ciphers = {sgcl::string(cipher)};
            o.macs = {sgcl::string(mac)};
            auto c = net::ssh::client::connect(sshd.address(), o);
            ASSERT_TRUE(c) << cipher << " " << mac << ": " << text(c.error().message());
            EXPECT_EQ(run(*c, "echo ok"), "ok\n||0") << cipher << " " << mac;
            if (std::string_view(cipher).find("gcm") != std::string_view::npos || std::string_view(cipher).find("chacha") != std::string_view::npos) {
                break;
            }
        }
    }
    for (const char* hk : {"ssh-ed25519", "ecdsa-sha2-nistp256", "ecdsa-sha2-nistp521", "rsa-sha2-512", "rsa-sha2-256"}) {
        auto o = sshd_options();
        o.host_key_algorithms = {sgcl::string(hk)};
        auto c = net::ssh::client::connect(sshd.address(), o);
        ASSERT_TRUE(c) << hk << ": " << text(c.error().message());
        EXPECT_EQ(run(*c, "true"), "||0") << hk;
    }
    // the host certificate, through @cert-authority
    auto o = sshd_options();
    o.insecure_ignore_host_key = false;
    o.host_key_algorithms = {sgcl::string("ecdsa-sha2-nistp256-cert-v01@openssh.com")};
    o.known_hosts = net::ssh::known_hosts::parse(sgcl::string("@cert-authority [127.0.0.1]:* " + read_data("ca.pub")));
    auto c = net::ssh::client::connect(sshd.address(), o);
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_TRUE(c->host_key().is_certificate());
    // and known_hosts with the plain key
    o.host_key_algorithms = {sgcl::string("ssh-ed25519")};
    o.known_hosts = net::ssh::known_hosts::parse(sgcl::string("[127.0.0.1]:" + std::to_string(sshd.port) + " " + read_data("ed25519.pub")));
    EXPECT_TRUE(net::ssh::client::connect(sshd.address(), o));
    o.known_hosts = net::ssh::known_hosts::parse(sgcl::string("[127.0.0.1]:" + std::to_string(sshd.port) + " " + read_data("p256.pub")));
    o.host_key_algorithms = {sgcl::string("ssh-ed25519")};
    auto mismatch = net::ssh::client::connect(sshd.address(), o);
    EXPECT_FALSE(mismatch);
}

TEST(SshOpenSsh, ClientAgainstSshdUserKeysAndCertificates) {
    if (!openssh()) {
        GTEST_SKIP() << "no OpenSSH";
    }
    Sshd sshd;
    ASSERT_TRUE(sshd.running);
    for (const char* k : {"ed25519", "p256", "p521", "rsa"}) {
        auto o = sshd_options();
        o.keys = {key(k)};
        auto c = net::ssh::client::connect(sshd.address(), o);
        ASSERT_TRUE(c) << k << ": " << text(c.error().message());
        EXPECT_EQ(run(*c, "true"), "||0");
    }
    // a key sshd does not know, then a user certificate for this user
    auto user_key = net::ssh::private_key::generate();
    auto dir = temp_dir("usercert");
    ASSERT_TRUE(user_key.save(sgcl::string((dir / "id").string())));
    std::ofstream(dir / "id.pub") << text(user_key.public_key().to_string()) << "\n";
    copy_key(dir, "ca");
    shell("ssh-keygen -q -s " + (dir / "ca").string() + " -I test -n " + user_name() + " -V -5m:+1h " + (dir / "id.pub").string() + " 2>&1");
    auto o = sshd_options();
    o.keys = {user_key};
    EXPECT_EQ(net::ssh::client::connect(sshd.address(), o).error().code(), net::errc::ssh_auth_failed);
    o.certificates = {*net::ssh::public_key::parse(sgcl::string(read_file((dir / "id-cert.pub").string())))};
    auto c = net::ssh::client::connect(sshd.address(), o);
    ASSERT_TRUE(c) << text(c.error().message());
    EXPECT_EQ(run(*c, "true"), "||0");
    std::filesystem::remove_all(dir);
}

TEST(SshOpenSsh, ClientAgainstSshdSessions) {
    if (!openssh()) {
        GTEST_SKIP() << "no OpenSSH";
    }
    Sshd sshd;
    ASSERT_TRUE(sshd.running);
    auto c = *net::ssh::client::connect(sshd.address(), sshd_options());
    EXPECT_EQ(run(c, "echo out; echo err 1>&2; exit 7"), "out\n|err\n|7");
    // input through cat, a megabyte
    auto s = *c.open_session();
    ASSERT_TRUE(s.exec(sgcl::string("wc -c")));
    vector<byte> mb(1 << 20, byte('x'));
    ASSERT_TRUE(s.input().write(mb.as_slice()));
    ASSERT_TRUE(s.close_input());
    EXPECT_EQ(std::atoi(text(*s.output().read_all_text()).c_str()), 1 << 20);
    EXPECT_EQ(s.wait()->code, 0);
    // the environment AcceptEnv lets through, and a terminal
    auto e = *c.open_session();
    ASSERT_TRUE(e.set_env(sgcl::string("SGCL_TEST"), sgcl::string("hello")));
    net::ssh::pty p;
    p.columns = 101;
    p.rows = 33;
    ASSERT_TRUE(e.request_pty(p));
    ASSERT_TRUE(e.exec(sgcl::string("echo $SGCL_TEST; stty size; tty")));
    std::string out = text(*e.output().read_all_text());
    EXPECT_NE(out.find("hello"), std::string::npos) << out;
    EXPECT_NE(out.find("33 101"), std::string::npos) << out;
    EXPECT_NE(out.find("/dev/"), std::string::npos) << out;
    (void)e.wait();
    // an end by a signal
    auto k = *c.open_session();
    ASSERT_TRUE(k.exec(sgcl::string("kill -TERM $$")));
    auto st = k.wait();
    ASSERT_TRUE(st);
    EXPECT_EQ(st->signal, "TERM");
    EXPECT_EQ(st->code, -1);
    // a signal sent to the program
    auto sig = *c.open_session();
    ASSERT_TRUE(sig.exec(sgcl::string("trap 'echo got; exit 3' TERM; echo ready; while :; do sleep 0.05; done")));
    auto r = io::buffered_reader(sig.output());
    auto line = r.read_line();
    ASSERT_TRUE(line && *line);
    ASSERT_TRUE(sig.signal(sgcl::string("TERM")));
    line = r.read_line();
    ASSERT_TRUE(line && *line);
    EXPECT_EQ(std::string(**line), "got");
    EXPECT_EQ(sig.wait()->code, 3);
    // the shell
    auto sh = *c.open_session();
    ASSERT_TRUE(sh.shell());
    ASSERT_TRUE(sh.input().write(sgcl::string("echo from-shell\nexit 4\n")));
    EXPECT_NE(text(*sh.output().read_all_text()).find("from-shell"), std::string::npos);
    EXPECT_EQ(sh.wait()->code, 4);
}

TEST(SshOpenSsh, ClientAgainstSshdCompressionAndRekey) {
    if (!openssh()) {
        GTEST_SKIP() << "no OpenSSH";
    }
    Sshd sshd("RekeyLimit 64K\n");
    ASSERT_TRUE(sshd.running);
    auto o = sshd_options();
    o.compression = true;
    o.rekey_bytes = 100 * 1000;
    auto c = *net::ssh::client::connect(sshd.address(), o);
    auto s = *c.open_session();
    ASSERT_TRUE(s.exec(sgcl::string("cat")));
    const size_t size = 3 << 20;
    vector<byte> data(size);
    for (size_t i = 0; i < size; ++i) {
        data[i] = byte(uint8_t((i * 7) ^ (i >> 9)));
    }
    std::atomic<bool> same{true};
    std::atomic<size_t> got{0};
    thread t([&] {
        vector<byte> buf(65536);
        auto out = s.output();
        size_t at = 0;
        for (;;) {
            auto n = out.read(buf.as_slice());
            if (!n || *n == 0) {
                break;
            }
            for (size_t i = 0; i < *n; ++i) {
                if (buf[i] != data[at + i]) {
                    same = false;
                }
            }
            at += *n;
        }
        got = at;
    });
    auto w = s.input().write(data.as_slice());
    EXPECT_TRUE(w) << text(w.error().message());
    EXPECT_TRUE(s.close_input());
    t.join();
    EXPECT_EQ(got.load(), size);
    EXPECT_TRUE(same.load());
    EXPECT_GT(net::ssh::detail::ClientAccess::conn(c)->kex_count(), 3u);   // the limits count the compressed bytes
}

TEST(SshOpenSsh, ClientAgainstSshdForwarding) {
    if (!openssh()) {
        GTEST_SKIP() << "no OpenSSH";
    }
    Sshd sshd;
    ASSERT_TRUE(sshd.running);
    EchoServer echo;
    auto c = *net::ssh::client::connect(sshd.address(), sshd_options());
    auto conn = c.dial(sgcl::string("127.0.0.1:" + std::to_string(echo.port)));
    ASSERT_TRUE(conn) << text(conn.error().message());
    EXPECT_EQ(round_trip(*conn, "through sshd"), "through sshd");
    (void)conn->close();
    // a port sshd listens on for us
    auto l = c.listen(sgcl::string("127.0.0.1:0"));
    ASSERT_TRUE(l) << text(l.error().message());
    uint16_t port = l->local_endpoint().port();
    thread t([port] {
        auto x = net::tcp::connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), port));
        if (x) {
            (void)x->write(sgcl::string("reverse"));
            (void)x->close_write();
            (void)x->read_all();
        }
    });
    auto in = l->accept();
    ASSERT_TRUE(in) << text(in.error().message());
    EXPECT_EQ(text(*in->read_all_text()), "reverse");
    (void)in->close();
    t.join();
    EXPECT_TRUE(l->close());
}

TEST(SshOpenSsh, AgentClientAgainstSshAgent) {
    if (!have("/usr/bin/ssh-agent")) {
        GTEST_SKIP() << "no ssh-agent";
    }
    auto dir = temp_dir("agent");
    auto sock = (dir / "agent.sock").string();
    io::command agent(sgcl::string("/usr/bin/ssh-agent"), sgcl::string("-D"), sgcl::string("-a"), sgcl::string(sock));
    ASSERT_TRUE(agent.start());
    for (int i = 0; i < 300 && !std::filesystem::exists(sock); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    {
        auto a = net::ssh::agent::connect(sgcl::string(sock));
        ASSERT_TRUE(a) << text(a.error().message());
        EXPECT_EQ(a->list()->size(), 0u);
        for (const char* k : {"ed25519", "p256", "rsa", "p384_enc"}) {
            ASSERT_TRUE(a->add(key(k, std::string_view(k) == "p384_enc" ? "correct horse" : ""))) << k;
        }
        ASSERT_TRUE(a->add(net::ssh::private_key::generate().with_comment(sgcl::string("short-lived")), 60 * second));
        auto listed = a->list();
        ASSERT_TRUE(listed);
        ASSERT_EQ(listed->size(), 5u);
        EXPECT_EQ((*listed)[0], key("ed25519").public_key());
        EXPECT_EQ((*listed)[0].comment(), "test-ed25519");
        EXPECT_EQ((*listed)[4].comment(), "short-lived");
        for (auto& k : *listed) {
            auto sig = a->sign(k, slice<const byte>("signed by the agent"));
            ASSERT_TRUE(sig) << text(k.type_name());
            EXPECT_TRUE(k.verify(slice<const byte>("signed by the agent"), sig->as_slice())) << text(k.type_name());
        }
        ASSERT_TRUE(a->remove(key("p256").public_key()));
        EXPECT_EQ(a->list()->size(), 4u);
        EXPECT_EQ(a->remove(key("p256").public_key()).error().code(), net::errc::ssh_request_refused);
        EXPECT_FALSE(a->sign(key("p256").public_key(), slice<const byte>("x")));
        // the client authenticating with the agent's keys alone, against sshd
        if (openssh()) {
            Sshd sshd;
            Env env;
            env.set("SSH_AUTH_SOCK", sock);
            env.set("HOME", dir.string());   // no key files of the user's
            auto o = sshd_options();
            o.keys = {};
            o.agent = true;
            auto c = net::ssh::client::connect(sshd.address(), o);
            ASSERT_TRUE(c) << text(c.error().message());
            // and forwarding it: ssh-add on the server's side sees its keys
            auto f = sshd_options();
            f.forward_agent = true;
            auto c2 = *net::ssh::client::connect(sshd.address(), f);
            std::string out = run(c2, "ssh-add -l");
            EXPECT_NE(out.find(text(key("ed25519").public_key().fingerprint())), std::string::npos) << out;
        }
        // the server's side of the forwarding: a session's agent() lists the
        // client's keys, for the module's client and for ssh -A
        {
            Env env;
            env.set("SSH_AUTH_SOCK", sock);
            net::ssh::server srv = echo_server();
            srv.handle([](net::ssh::server_session s) -> async::task<> {
                auto ag = co_await s.async_agent();
                if (!ag) {
                    co_await s.output().async_write(sgcl::string("no agent: " + text(ag.error().message())));
                    co_return;
                }
                auto keys = co_await ag->async_list();
                std::string out;
                for (auto& k : *keys) {
                    out += text(k.fingerprint()) + "\n";
                }
                co_await s.output().async_write(sgcl::string(out));
            });
            LocalServer ls(srv);
            auto f = client_options();
            f.forward_agent = true;
            auto c = *net::ssh::client::connect(ls.address(), f);
            auto r = c.run(sgcl::string("keys"));
            EXPECT_NE(text(r->out).find(text(key("ed25519").public_key().fingerprint())), std::string::npos) << text(r->out);
            if (openssh()) {
                auto dir2 = temp_dir("agentfwd");
                copy_key(dir2, "ed25519");
                std::string out = shell("/usr/bin/ssh -F /dev/null -A -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=error -i " +
                                        (dir2 / "ed25519").string() + " -p " + std::to_string(ls.port) + " user@127.0.0.1 keys 2>&1");
                EXPECT_NE(out.find(text(key("ed25519").public_key().fingerprint())), std::string::npos) << out;
                std::filesystem::remove_all(dir2);
            }
        }
        ASSERT_TRUE(a->remove_all());
        EXPECT_EQ(a->list()->size(), 0u);
        EXPECT_TRUE(a->close());
    }
    EXPECT_FALSE(net::ssh::agent::connect(sgcl::string((dir / "none.sock").string())));
    {
        Env env;
        ::unsetenv("SSH_AUTH_SOCK");
        auto none = net::ssh::agent::connect();
        ASSERT_FALSE(none);
        EXPECT_TRUE(none.error().is_not_found());
    }
    (void)agent.process.kill();
    (void)agent.wait();
    std::filesystem::remove_all(dir);
}

namespace {
    struct OurServer {
        LocalServer s;
        std::filesystem::path dir;

        explicit OurServer(net::ssh::server srv)
        : s(srv) {
            dir = temp_dir("sshclient");
            copy_key(dir, "ed25519");
        }

        ~OurServer() {
            std::filesystem::remove_all(dir);
        }

        std::string ssh(const std::string& opts, const std::string& cmd, const std::string& input = "") {
            std::string line = "printf '%s' '" + input + "' | /usr/bin/ssh -F /dev/null -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "
                               "-o LogLevel=error -i " + (dir / "ed25519").string() + " -p " + std::to_string(s.port) + " " + opts + " user@127.0.0.1 '" + cmd + "' 2>&1; echo \"rc=$?\"";
            return shell(line);
        }
    };
}

TEST(SshOpenSsh, SshAgainstOurServer) {
    if (!openssh()) {
        GTEST_SKIP() << "no OpenSSH";
    }
    net::ssh::server srv = echo_server();
    srv.host_keys = {key("ed25519"), key("p256"), key("p521"), key("rsa")};
    srv.compression = true;
    srv.kex = {};
    for (const auto& k : net::ssh::detail::kex_table) {
        srv.kex.push_back(sgcl::string(k.name));
    }
    OurServer o(srv);
    EXPECT_EQ(o.ssh("", "hi", "input"), "cmd=hi sub= in=inputerrrc=0\n");
    EXPECT_EQ(o.ssh("", "exit 3"), "cmd=exit 3 sub= in=errrc=3\n");
    for (const char* kex : {"mlkem768x25519-sha256", "curve25519-sha256", "ecdh-sha2-nistp256", "ecdh-sha2-nistp384", "ecdh-sha2-nistp521", "diffie-hellman-group16-sha512",
                            "diffie-hellman-group14-sha256"}) {
        EXPECT_EQ(o.ssh(std::string("-o KexAlgorithms=") + kex, "k"), "cmd=k sub= in=errrc=0\n") << kex;
    }
    for (const char* c : {"chacha20-poly1305@openssh.com", "aes128-gcm@openssh.com", "aes256-gcm@openssh.com", "aes128-ctr", "aes192-ctr", "aes256-ctr"}) {
        EXPECT_EQ(o.ssh(std::string("-c ") + c, "c"), "cmd=c sub= in=errrc=0\n") << c;
    }
    for (const char* m : {"hmac-sha2-256", "hmac-sha2-512", "hmac-sha2-256-etm@openssh.com", "hmac-sha2-512-etm@openssh.com"}) {
        EXPECT_EQ(o.ssh(std::string("-c aes256-ctr -m ") + m, "m"), "cmd=m sub= in=errrc=0\n") << m;
    }
    for (const char* hk : {"ssh-ed25519", "ecdsa-sha2-nistp256", "ecdsa-sha2-nistp521", "rsa-sha2-512", "rsa-sha2-256"}) {
        EXPECT_EQ(o.ssh(std::string("-o HostKeyAlgorithms=") + hk, "h"), "cmd=h sub= in=errrc=0\n") << hk;
    }
    EXPECT_EQ(o.ssh("-C", "compressed", std::string(10000, 'a')), "cmd=compressed sub= in=" + std::string(10000, 'a') + "errrc=0\n");
    EXPECT_EQ(o.ssh("-s", "sftp"), "cmd= sub=sftp in=errrc=0\n");
}

TEST(SshOpenSsh, SshRekeyAgainstOurServer) {
    if (!openssh()) {
        GTEST_SKIP() << "no OpenSSH";
    }
    net::ssh::server srv = echo_server();
    srv.rekey_bytes = 100 * 1000;
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        auto all = co_await s.input().async_read_all();
        co_await s.output().async_write(sgcl::string(std::to_string(all ? all->size() : 0)));
    });
    OurServer o(srv);
    std::string line = "head -c 3000000 /dev/urandom | /usr/bin/ssh -F /dev/null -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null "
                       "-o LogLevel=error -o RekeyLimit=64K -i " + (o.dir / "ed25519").string() + " -p " + std::to_string(o.s.port) + " user@127.0.0.1 count 2>&1";
    EXPECT_EQ(shell(line), "3000000");
}

TEST(SshOpenSsh, SshForwardingThroughOurServer) {
    if (!openssh()) {
        GTEST_SKIP() << "no OpenSSH";
    }
    EchoServer echo;
    net::ssh::server srv = echo_server();
    srv.allow_direct_tcpip = [](const sgcl::string&, const sgcl::string&, uint16_t) { return true; };
    srv.allow_tcpip_forward = [](const sgcl::string&, const sgcl::string&, uint16_t) { return true; };
    OurServer o(srv);
    const uint16_t lport = free_port(), rport = free_port();
    io::command ssh(sgcl::string("/usr/bin/ssh"));
    ssh.args = {sgcl::string("-F"), sgcl::string("/dev/null"), sgcl::string("-N"), sgcl::string("-o"), sgcl::string("BatchMode=yes"), sgcl::string("-o"),
                sgcl::string("StrictHostKeyChecking=no"), sgcl::string("-o"), sgcl::string("UserKnownHostsFile=/dev/null"), sgcl::string("-o"),
                sgcl::string("ExitOnForwardFailure=yes"), sgcl::string("-i"), sgcl::string((o.dir / "ed25519").string()), sgcl::string("-p"),
                sgcl::string(std::to_string(o.s.port)), sgcl::string("-L"), sgcl::string(std::to_string(lport) + ":127.0.0.1:" + std::to_string(echo.port)),
                sgcl::string("-R"), sgcl::string("127.0.0.1:" + std::to_string(rport) + ":127.0.0.1:" + std::to_string(echo.port)), sgcl::string("user@127.0.0.1")};
    ASSERT_TRUE(ssh.start());
    ASSERT_TRUE(wait_port(lport));
    ASSERT_TRUE(wait_port(rport));
    auto a = *net::tcp::connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), lport));
    EXPECT_EQ(round_trip(a, "via -L"), "via -L");
    (void)a.close();
    auto b = *net::tcp::connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), rport));
    EXPECT_EQ(round_trip(b, "via -R"), "via -R");
    (void)b.close();
    (void)ssh.process.kill();
    (void)ssh.wait();
}
