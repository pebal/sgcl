//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of net::smtp share: a server of the module on the loopback
// that keeps what it took, a raw TCP peer that answers with given bytes, the
// Go peer (go_smtp/main.go) built once and run as a child, the TLS settings
// of tests/net/tls_testdata.
#pragma once

#include "tests/source_root.h"
#include "tests/types.h"
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net/tls.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace smtp_test {
    using namespace sgcl;
    namespace smtp = sgcl::net::smtp;

    inline std::string str(const sgcl::string& s) {
        return std::string(s.view());
    }

    inline std::string str(const sgcl::vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    inline std::string slurp(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        std::stringstream s;
        s << f.rdbuf();
        return s.str();
    }

    inline std::string testdata() {
        return (source_root() / "tests/net/tls_testdata/").string();
    }

    // The client's side: the test CA trusted, the name the certificate has
    inline net::tls::config trusted() {
        net::tls::config c;
        c.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata() + "ca.pem")));
        c.server_name = sgcl::string("localhost");
        return c;
    }

    // The server's side: the ECDSA certificate of localhost and 127.0.0.1
    inline net::tls::config serving() {
        net::tls::config c;
        auto id = net::tls::identity::from_pem(sgcl::string(slurp(testdata() + "ecdsa.pem")), sgcl::string(slurp(testdata() + "ecdsa.key")));
        c.identities.push_back(*id);
        return c;
    }

    inline std::string run(const std::string& cmd) {
        std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    }

    inline bool have(const char* tool) {
        return std::system((std::string("command -v ") + tool + " > /dev/null 2>&1").c_str()) == 0;
    }

    inline std::string scratch() {
        static int n = 0;
        auto d = std::filesystem::temp_directory_path() / ("sgcl_smtp_" + std::to_string(::getpid()) + "_" + std::to_string(n++));
        std::filesystem::create_directories(d);
        return d.string();
    }

    // The Go peer built once; "" when there is no go
    inline const std::string& go_smtp() {
        static std::string path = [] {
            if (!have("go")) {
                return std::string();
            }
            auto out = std::filesystem::temp_directory_path() / ("sgcl_go_smtp_" + std::to_string(::getpid()));
            auto cmd = "go build -o '" + out.string() + "' '" + (source_root() / "tests/net/go_smtp/main.go").string() + "' 2>&1";
            return std::system(cmd.c_str()) == 0 ? out.string() : std::string();
        }();
        return path;
    }

    // A child whose output goes to a file, read as it grows; killed at the end
    struct Child {
        io::command cmd;
        std::string log;
        bool started = false;

        Child(const std::string& program, sgcl::vector<sgcl::string> args)
        : cmd(sgcl::string(program), std::move(args)) {
        }

        bool start() {
            log = scratch() + "/out.log";
            auto f = io::create(sgcl::string(log));
            if (!f) {
                return false;
            }
            cmd.out = *f;
            cmd.err = *f;
            started = (bool)cmd.start();
            (void)f->close();
            return started;
        }

        std::string output() const {
            return slurp(log);
        }

        // The output once it holds `what` ("" when it never did)
        std::string wait_for(const std::string& what, int ms = 10000) {
            for (int i = 0; i < ms / 10; ++i) {
                std::string s = slurp(log);
                if (s.find(what) != std::string::npos) {
                    return s;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return "";
        }

        void finish() {
            if (started) {
                (void)cmd.process.kill();
                (void)cmd.wait();
                started = false;
            }
        }

        ~Child() {
            finish();
        }
    };

    // The Go server of a mode, its port read from its first line
    struct GoServer {
        Child child;
        uint16_t port = 0;

        explicit GoServer(const std::string& mode)
        : child(go_smtp(), [&] {
            sgcl::vector<sgcl::string> a;
            a.push_back(sgcl::string("server"));
            a.push_back(sgcl::string(mode));
            a.push_back(sgcl::string(testdata() + "ecdsa.pem"));
            a.push_back(sgcl::string(testdata() + "ecdsa.key"));
            return a;
        }()) {
            if (go_smtp().empty() || !child.start()) {
                return;
            }
            auto s = child.wait_for("port ");
            if (!s.empty()) {
                port = uint16_t(std::atoi(s.c_str() + s.find("port ") + 5));
            }
        }

        sgcl::string url(const char* scheme = "smtp") const {
            return sgcl::string(std::string(scheme) + "://127.0.0.1:" + std::to_string(port));
        }

        // The JSON lines of the messages taken, once there are n of them
        std::vector<std::string> records(size_t n, int ms = 10000) {
            for (int i = 0; i < ms / 10; ++i) {
                std::vector<std::string> out;
                std::istringstream in(child.output());
                std::string line;
                while (std::getline(in, line)) {
                    if (!line.empty() && line.front() == '{') {
                        out.push_back(line);
                    }
                }
                if (out.size() >= n) {
                    return out;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return {};
        }
    };

    // What a server of the module took, kept where the tests read it
    struct Taken {
        std::mutex m;
        sgcl::vector<smtp::envelope> envelopes;
        sgcl::vector<sgcl::string> data;

        size_t size() {
            std::lock_guard g(m);
            return envelopes.size();
        }

        smtp::envelope envelope(size_t i) {
            std::lock_guard g(m);
            return envelopes[i];
        }

        std::string text(size_t i) {
            std::lock_guard g(m);
            return str(data[i]);
        }
    };

    // A server of the module on 127.0.0.1, serving until close()
    struct Running {
        smtp::server srv;
        sgcl::tracked_ptr<Taken> taken = sgcl::make_tracked<Taken>();
        net::listener l;
        async::task<expected<void, io::error>> serving;
        uint16_t port = 0;

        // srv's fields set by the test before start(); a handler that keeps
        // each message unless the test set its own
        void start(bool keep = true, bool implicit_tls = false) {
            if (keep) {
                sgcl::tracked_ptr t = taken;
                srv.handle([t](smtp::message m) {
                    std::lock_guard g(t->m);
                    t->envelopes.push_back(m.envelope());
                    t->data.push_back(sgcl::string(m.bytes()));
                });
            }
            if (implicit_tls) {
                l = *net::tls::listen("127.0.0.1:0", smtp_test::serving());
            } else {
                l = *net::tcp::listen("127.0.0.1:0");
            }
            port = l.local_endpoint().port();
            serving = async::spawn(srv.async_serve(l));
        }

        sgcl::string url(const char* scheme = "smtp", const char* userinfo = "") const {
            return sgcl::string(std::string(scheme) + "://" + userinfo + "127.0.0.1:" + std::to_string(port));
        }

        sgcl::string address() const {
            return sgcl::string("127.0.0.1:" + std::to_string(port));
        }

        bool stopped = false;

        void stop() {
            if (!stopped && port) {
                stopped = true;
                srv.close();
                (void)serving.wait();
            }
        }

        ~Running() {
            stop();
        }
    };

    // A peer that answers with what the test gives it: each script line
    // written when the client's next line (or the start, for the first)
    // has come; "<wait>" sleeps 3 s; the script's end closes the connection
    inline async::task<> raw_peer(net::listener l, std::vector<std::string> script, bool wait_lines) {
        auto c = co_await l.async_accept();
        if (!c) {
            co_return;
        }
        bool first = true;
        for (auto& s : script) {
            if (!first && wait_lines) {
                auto line = co_await c->async_read_line();
                if (!line || !*line) {
                    break;
                }
            }
            first = false;
            if (s == "<wait>") {
                co_await async::sleep(std::chrono::seconds(3));
                continue;
            }
            if (!co_await c->async_write(sgcl::string(s))) {
                break;
            }
        }
        (void)co_await c->async_close();
    }

    struct RawPeer {
        net::listener l;
        uint16_t port = 0;
        async::task<> serving;

        explicit RawPeer(std::vector<std::string> script, bool wait_lines = true) {
            l = *net::tcp::listen("127.0.0.1:0");
            port = l.local_endpoint().port();
            serving = async::spawn(raw_peer(l, std::move(script), wait_lines));
        }

        sgcl::string url() const {
            return sgcl::string("smtp://127.0.0.1:" + std::to_string(port));
        }

        ~RawPeer() {
            (void)l.close();
            serving.wait();
        }
    };

    inline encoding::email simple_message() {
        encoding::email m("Alice <alice@example.com>", "bob@example.org", "Hello", "Hi Bob.\n");
        return m;
    }
}
