//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ssh's client against its server in one process on the loopback; the
// same cases in Go over golang.org/x/crypto/ssh (benchmarks/go/ssh/main.go).
// Prints "ssh <case> ns/op=… ops/s=… wall=…s cpu=…s MB/s=…".
//
//   ssh handshake sgcl [n]          a TCP connection, curve25519-sha256 (x/crypto has no ML-KEM yet),
//                                   an Ed25519 host key and user key, the authentication, closed:
//                                   per connection
//   ssh handshake_mlkem sgcl [n]    the same with the module's default, mlkem768x25519-sha256 (no
//                                   Go side)
//   ssh exec sgcl [n]               a session that runs "true" on one kept connection: open, exec,
//                                   the exit status, closed: per session
//   ssh throughput_gcm sgcl [MB]    MB megabytes (1024 by default) written to a session's input in
//                                   writes of 32 KB, the server reading them to their end,
//                                   aes128-gcm@openssh.com: per write, and MB/s
//   ssh throughput_chacha sgcl [MB] the same with chacha20-poly1305@openssh.com
//   ssh sftp_upload sgcl <client> <server> [MB]
//                                   a file of MB megabytes (512 by default) put through SFTP, the
//                                   connection included: client sgcl (net::sftp::client::upload) or
//                                   openssh (/usr/bin/sftp -b), server sgcl (net::sftp::serve in this
//                                   process) or openssh (/usr/sbin/sshd run unprivileged on the
//                                   loopback, /usr/libexec/sftp-server); curve25519-sha256,
//                                   aes128-gcm@openssh.com, Ed25519 keys: per file, and MB/s
//   ssh sftp_download sgcl <client> <server> [MB]
//                                   the same file got back
#include "benchmarks/common.h"
#include "sgcl/sgcl.h"
#include "sgcl/net/sftp.h"
#include "sgcl/net/ssh.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include <signal.h>
#include <unistd.h>

namespace {
    using namespace sgcl;

    void report(const char* what, double wall, double ops, double mb = 0) {
        char extra[64] = "";
        if (mb > 0) {
            std::snprintf(extra, sizeof extra, " MB/s=%.1f", mb / wall);
        }
        std::printf("ssh %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs%s\n", what, wall * 1e9 / ops, ops / wall, wall, bench::cpu_seconds(), extra);
    }

    async::task<> serve(net::ssh::server srv, net::listener l) {
        (void)co_await srv.async_serve(l);
    }

    net::ssh::server make_server() {
        net::ssh::server srv;
        srv.host_keys = {net::ssh::private_key::generate()};
        srv.check_public_key = [](const string&, const net::ssh::public_key&) { return true; };
        srv.handle([](net::ssh::server_session s) -> async::task<> {
            if (s.command() == "sink") {
                vector<byte> buf(65536);
                auto in = s.input();
                for (;;) {
                    auto n = co_await in.async_read(buf.as_slice());
                    if (!n || *n == 0) {
                        break;
                    }
                }
            }
        });
        return srv;
    }

    // An SFTP file through one of the four pairs of a client and a server
    int sftp_case(const std::string& what, const std::string& client, const std::string& server, long mb) {
        namespace fsys = std::filesystem;
        const bool up = what == "sftp_upload";
        const fsys::path dir = fsys::temp_directory_path() / ("sgcl_sftp_bench_" + std::to_string(::getpid()));
        fsys::create_directories(dir / "root");
        const fsys::path local = dir / "local.bin", remote = dir / "root" / "remote.bin";
        {
            std::ofstream out(up ? local : remote, std::ios::binary);
            std::string block(1 << 20, 0);
            for (size_t i = 0; i < block.size(); ++i) {
                block[i] = char(i * 2654435761u >> 24);
            }
            for (long i = 0; i < mb; ++i) {
                out.write(block.data(), std::streamsize(block.size()));
            }
        }
        auto user = net::ssh::private_key::generate();
        auto host = net::ssh::private_key::generate();
        (void)user.save(string((dir / "user").string()));
        (void)host.save(string((dir / "host").string()));
        std::ofstream(dir / "authorized_keys") << std::string(user.public_key().to_string().view()) << "\n";
        uint16_t port = 0;
        net::ssh::server srv;
        long sshd = 0;
        if (server == "sgcl") {
            srv.host_keys = {host};
            srv.check_public_key = [](const string&, const net::ssh::public_key&) { return true; };
            string root((dir / "root").string());
            srv.handle([root](net::ssh::server_session s) -> async::task<> {
                if (s.subsystem() == "sftp") {
                    (void)co_await net::sftp::async_serve(s, root);
                }
            });
            auto l = net::tcp::listen("127.0.0.1:0");
            if (!l) {
                return 1;
            }
            port = l->local_endpoint().port();
            async::go(serve(srv, *l));
        } else {
            {
                auto l = net::tcp::listen("127.0.0.1:0");   // a free port, given back for sshd
                port = l ? l->local_endpoint().port() : 0;
                if (l) {
                    (void)l->close();
                }
            }
            std::ofstream(dir / "sshd_config") << "Port " << port << "\nListenAddress 127.0.0.1\nHostKey " << (dir / "host").string()
                                                << "\nAuthorizedKeysFile " << (dir / "authorized_keys").string() << "\nPidFile " << (dir / "sshd.pid").string()
                                                << "\nUsePAM no\nStrictModes no\nPasswordAuthentication no\nKbdInteractiveAuthentication no\n"
                                                   "Subsystem sftp /usr/libexec/sftp-server\n";
            if (std::system(("/usr/sbin/sshd -f " + (dir / "sshd_config").string() + " -E " + (dir / "sshd.log").string()).c_str()) != 0) {
                return 1;
            }
            for (int i = 0; i < 500 && !net::tcp::connect(net::endpoint(*net::ip_address::parse(string("127.0.0.1")), port)); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            std::ifstream(dir / "sshd.pid") >> sshd;
        }
        const std::string address = "127.0.0.1:" + std::to_string(port);
        const char* login = std::getenv("USER");   // sshd refuses a user it does not know
        const std::string name = login ? login : "bench";
        // sftp-server sees the whole file system, the module's server the root only
        const std::string path = server == "sgcl" ? "/remote.bin" : remote.string();
        bool ok = true;
        auto t0 = bench::Clock::now();
        if (client == "sgcl") {
            net::ssh::client::options o;
            o.user = string(std::string_view(name));
            o.keys = {user};
            o.agent = false;
            o.insecure_ignore_host_key = true;
            o.kex = {string("curve25519-sha256")};
            o.ciphers = {string("aes128-gcm@openssh.com")};
            auto fs = net::sftp::client::connect(string(std::string_view(address)), o);
            ok = fs.has_value();
            if (!ok) {
                std::fprintf(stderr, "%s\n", std::string(fs.error().message().view()).c_str());
            } else {
                auto n = up ? fs->upload(string(local.string()), string(std::string_view(path))) : fs->download(string(std::string_view(path)), string(local.string()));
                ok = n && *n == uint64_t(mb) << 20;
                (void)fs->close();
            }
        } else {
            std::ofstream(dir / "batch") << (up ? "put " + local.string() + " " + path : "get " + path + " " + local.string()) << "\n";
            const std::string command = "/usr/bin/sftp -F /dev/null -q -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=error "
                                        "-o KexAlgorithms=curve25519-sha256 -c aes128-gcm@openssh.com -i " + (dir / "user").string() + " -P " + std::to_string(port) +
                                        " -b " + (dir / "batch").string() + " " + name + "@127.0.0.1 > /dev/null";
            ok = std::system(command.c_str()) == 0;
        }
        const double wall = bench::seconds_since(t0);
        ok = ok && fsys::file_size(up ? remote : local) == uint64_t(mb) << 20;
        report((what + "/" + client + "-" + server).c_str(), wall, 1, double(mb));
        if (sshd > 0) {
            ::kill(pid_t(sshd), SIGTERM);
        }
        srv.close();
        if (!ok && sshd > 0) {
            std::ifstream log(dir / "sshd.log");
            std::fprintf(stderr, "%s\n", std::string(std::istreambuf_iterator<char>(log), {}).c_str());
        }
        fsys::remove_all(dir);
        return ok ? 0 : 1;
    }
}

int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[2]) != "sgcl") {
        std::fprintf(stderr, "usage: ssh <handshake|handshake_mlkem|exec|throughput_gcm|throughput_chacha> sgcl [n]\n"
                             "       ssh <sftp_upload|sftp_download> sgcl <sgcl|openssh> <sgcl|openssh> [MB]\n");
        return 2;
    }
    const std::string what = argv[1];
    if (what == "sftp_upload" || what == "sftp_download") {
        if (argc < 5) {
            return 2;
        }
        return sftp_case(what, argv[3], argv[4], argc > 5 ? std::atol(argv[5]) : 512);
    }
    long n = argc > 3 ? std::atol(argv[3]) : 0;
    net::ssh::server srv = make_server();
    auto l = net::tcp::listen("127.0.0.1:0");
    if (!l) {
        return 1;
    }
    string address(std::string_view("127.0.0.1:" + std::to_string(l->local_endpoint().port())));
    async::go(serve(srv, *l));
    net::ssh::client::options o;
    o.user = "bench";
    o.keys = {net::ssh::private_key::generate()};
    o.agent = false;
    o.insecure_ignore_host_key = true;
    o.kex = {string("curve25519-sha256")};
    bool ok = true;
    if (what == "handshake" || what == "handshake_mlkem") {
        n = n ? n : 2000;
        if (what == "handshake_mlkem") {
            o.kex = {};
        }
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            auto c = net::ssh::client::connect(address, o);
            if (!c) {
                std::fprintf(stderr, "%s\n", std::string(c.error().message().view()).c_str());
                return 1;
            }
            (void)c->close();
        }
        report(what.c_str(), bench::seconds_since(t0), double(n));
    } else if (what == "exec") {
        n = n ? n : 5000;
        auto c = net::ssh::client::connect(address, o);
        if (!c) {
            return 1;
        }
        auto t0 = bench::Clock::now();
        for (long i = 0; i < n; ++i) {
            auto r = c->run(string("true"));
            ok &= r && r->status.code == 0;
        }
        report(what.c_str(), bench::seconds_since(t0), double(n));
        (void)c->close();
    } else if (what == "throughput_gcm" || what == "throughput_chacha") {
        n = n ? n : 1024;
        o.ciphers = {string(what == "throughput_gcm" ? "aes128-gcm@openssh.com" : "chacha20-poly1305@openssh.com")};
        auto c = net::ssh::client::connect(address, o);
        if (!c) {
            return 1;
        }
        auto s = c->open_session();
        ok &= s && s->exec(string("sink")).has_value();
        vector<byte> block(32768);
        const long writes = n * 32;
        auto in = s->input();
        auto t0 = bench::Clock::now();
        for (long i = 0; i < writes; ++i) {
            ok &= in.write(block.as_slice()).has_value();
        }
        ok &= s->close_input().has_value();
        auto st = s->wait();
        ok &= st && st->code == 0;
        report(what.c_str(), bench::seconds_since(t0), double(writes), double(n));
        (void)c->close();
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    srv.close();
    return ok ? 0 : 1;
}
