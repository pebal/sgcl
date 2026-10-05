//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of net::ssh share: the test data's paths, a server of the
// module's on the loopback, an echo server, shell commands.
#pragma once

#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/ssh.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace sgcl_ssh_test {
    using namespace sgcl;

    inline std::string data_path(const std::string& name) {
        return (source_root() / "tests/net/ssh/testdata" / name).string();
    }

    inline std::string read_file(const std::string& path) {
        std::ifstream f(path);
        std::stringstream s;
        s << f.rdbuf();
        return s.str();
    }

    inline std::string read_data(const std::string& name) {
        return read_file(data_path(name));
    }

    inline std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    inline std::string shell(const std::string& cmd) {
        std::string out;
        FILE* p = ::popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        ::pclose(p);
        return out;
    }

    inline bool have(const char* path) {
        return ::access(path, X_OK) == 0;
    }

    inline std::filesystem::path temp_dir(const std::string& name) {
        auto p = std::filesystem::temp_directory_path() / ("sgcl_ssh_" + name + "_" + std::to_string(::getpid()));
        std::filesystem::remove_all(p);
        std::filesystem::create_directories(p);
        return p;
    }

    inline net::ssh::private_key key(const std::string& name, const std::string& pass = "") {
        return *net::ssh::private_key::load(sgcl::string(data_path(name)), sgcl::string(pass));
    }

    inline net::ssh::public_key pub(const std::string& name) {
        return *net::ssh::public_key::parse(sgcl::string(read_data(name)));
    }

    inline async::task<> serve_task(net::ssh::server srv, net::listener l) {
        (void)co_await srv.async_serve(l);
    }

    // A server of the module's on 127.0.0.1, port of the system's choice,
    // closed with the object
    struct LocalServer {
        net::ssh::server srv;
        uint16_t port = 0;

        explicit LocalServer(net::ssh::server s)
        : srv(s) {
            auto l = net::tcp::listen("127.0.0.1:0");
            port = l->local_endpoint().port();
            async::go(serve_task(srv, *l));
        }

        LocalServer(const LocalServer&) = delete;

        ~LocalServer() {
            srv.close();
        }

        sgcl::string address() const {
            return sgcl::string("127.0.0.1:" + std::to_string(port));
        }
    };

    // A server whose sessions echo: the command's text, the input back,
    // a line on stderr, the exit code the command names ("exit 3")
    inline net::ssh::server echo_server() {
        net::ssh::server srv;
        srv.host_keys = {key("ed25519")};
        srv.check_password = [](const sgcl::string& user, const sgcl::string& pw) { return user == "user" && pw == "secret"; };
        srv.check_public_key = [](const sgcl::string&, const net::ssh::public_key&) { return true; };
        srv.handle([](net::ssh::server_session s) -> async::task<> {
            auto in = co_await s.input().async_read_all();
            std::string out = "cmd=" + text(s.command()) + " sub=" + text(s.subsystem()) + " in=" + (in ? std::string(reinterpret_cast<const char*>(in->data()), in->size()) : "?");
            co_await s.output().async_write(sgcl::string(out));
            co_await s.error_output().async_write(sgcl::string("err"));
            std::string cmd = text(s.command());
            int code = cmd.rfind("exit ", 0) == 0 ? std::atoi(cmd.c_str() + 5) : 0;
            co_await s.async_exit(code);
        });
        return srv;
    }

    inline net::ssh::client::options client_options() {
        net::ssh::client::options o;
        o.user = "user";
        o.insecure_ignore_host_key = true;
        o.agent = false;
        o.keys = {key("ed25519")};
        o.timeout = 20 * second;
        return o;
    }

    inline async::task<> echo_one(net::connection c) {
        (void)co_await c.async_copy_to(c);
        (void)c.close();
    }

    inline async::task<> echo_loop(net::listener l) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            async::go(echo_one(*c));
        }
    }

    // A TCP echo server on 127.0.0.1
    struct EchoServer {
        net::listener l;
        uint16_t port = 0;

        EchoServer() {
            l = *net::tcp::listen("127.0.0.1:0");
            port = l.local_endpoint().port();
            async::go(echo_loop(l));
        }

        ~EchoServer() {
            (void)l.close();
        }
    };

    inline std::string user_name() {
        const char* u = std::getenv("USER");
        return u ? u : "";
    }

    inline uint16_t free_port() {
        auto l = net::tcp::listen("127.0.0.1:0");
        uint16_t p = l->local_endpoint().port();
        (void)l->close();
        return p;
    }

    inline bool wait_port(uint16_t port) {
        for (int i = 0; i < 300; ++i) {
            if (net::tcp::connect(net::endpoint(*net::ip_address::parse(sgcl::string("127.0.0.1")), port))) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    inline void copy_key(const std::filesystem::path& dir, const std::string& name) {
        std::filesystem::copy_file(data_path(name), dir / name, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::permissions(dir / name, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    }

    // /usr/sbin/sshd for the duration of a test
    struct Sshd {
        std::filesystem::path dir;
        uint16_t port = 0;
        io::command cmd{sgcl::string("/usr/sbin/sshd")};
        bool running = false;

        explicit Sshd(const std::string& extra = "") {
            dir = temp_dir("sshd");
            for (const char* k : {"ed25519", "p256", "rsa", "host", "ca"}) {
                copy_key(dir, k);
            }
            std::filesystem::copy_file(data_path("host-cert.pub"), dir / "host-cert.pub");
            std::filesystem::copy_file(data_path("ca.pub"), dir / "ca.pub");
            std::ofstream(dir / "authorized_keys") << read_data("ed25519.pub") << read_data("p256.pub") << read_data("rsa.pub");
            port = free_port();
            std::ofstream cfg(dir / "sshd_config");
            cfg << "Port " << port << "\nListenAddress 127.0.0.1\n"
                << "HostKey " << (dir / "ed25519").string() << "\n"
                << "HostKey " << (dir / "p256").string() << "\n"
                << "HostKey " << (dir / "rsa").string() << "\n"
                << "HostKey " << (dir / "host").string() << "\n"
                << "HostCertificate " << (dir / "host-cert.pub").string() << "\n"
                << "AuthorizedKeysFile " << (dir / "authorized_keys").string() << "\n"
                << "TrustedUserCAKeys " << (dir / "ca.pub").string() << "\n"
                << "PidFile " << (dir / "sshd.pid").string() << "\n"
                << "UsePAM no\nStrictModes no\nPasswordAuthentication no\nKbdInteractiveAuthentication no\n"
                << "AcceptEnv SGCL_TEST\n"
                << "KexAlgorithms +diffie-hellman-group16-sha512,diffie-hellman-group14-sha256\n"
                << "Subsystem sftp /usr/libexec/sftp-server\n"
                << extra;
            cfg.close();
            cmd.args = {sgcl::string("-D"), sgcl::string("-e"), sgcl::string("-f"), sgcl::string((dir / "sshd_config").string())};
            running = cmd.start().has_value() && wait_port(port);
        }

        ~Sshd() {
            if (cmd.process) {
                (void)cmd.process.kill();
                (void)cmd.wait();
            }
            std::filesystem::remove_all(dir);
        }

        sgcl::string address() const {
            return sgcl::string("127.0.0.1:" + std::to_string(port));
        }
    };

    // A line through a connection and back
    inline std::string round_trip(const net::connection& c, const std::string& line) {
        auto w = c.write(sgcl::string(line));
        if (!w) {
            return "write: " + text(w.error().message());
        }
        vector<byte> buf(line.size());
        auto r = c.read_full(buf.as_slice());
        if (!r) {
            return "read: " + text(r.error().message());
        }
        return std::string(reinterpret_cast<const char*>(buf.data()), *r);
    }
}
