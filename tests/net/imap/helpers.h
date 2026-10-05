//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the IMAP tests share: a server of the module on the loopback over a
// memory_backend (or a backend given) with users alice and bob, a raw
// connection to it that sends lines and reads responses as text, the
// test certificate of the tree, a scratch directory, a process runner.
#pragma once

#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/imap.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace sgcl_test::imap {
    using namespace sgcl;
    using namespace std::chrono_literals;

    inline std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    inline std::string msg_text(const std::string& subject, const std::string& body = "Hello\r\n", const std::string& from = "Bob <bob@example.com>") {
        return "From: " + from + "\r\nTo: alice@example.com\r\nSubject: " + subject + "\r\nDate: Mon, 5 Oct 2026 10:00:00 +0200\r\nMessage-ID: <" + subject +
               "@example.com>\r\n\r\n" + body;
    }

    inline sgcl::string message(const std::string& subject, const std::string& body = "Hello\r\n", const std::string& from = "Bob <bob@example.com>") {
        return sgcl::string(msg_text(subject, body, from));
    }

    inline std::string slurp(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    inline std::string testdata(const std::string& name) {
        return (source_root() / "tests/net/tls_testdata" / name).string();
    }

    inline net::tls::config server_tls() {
        net::tls::config tls;
        tls.identities = {net::tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
        return tls;
    }

    inline net::tls::config client_tls() {
        net::tls::config tls;
        tls.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
        tls.server_name = sgcl::string("localhost");
        return tls;
    }

    // A server on 127.0.0.1 over the backend given
    struct Server {
        net::imap::server srv;
        net::listener listener;
        async::task<expected<void, io::error>> serving;
        uint16_t port = 0;

        explicit Server(const net::imap::backend& b, bool tls_listener = false, void (*setup)(net::imap::server&) = nullptr) {
            srv.backend = b;
            srv.poll_interval = 200ms;
            if (setup) {
                setup(srv);
            }
            listener = tls_listener ? net::tls::listen("127.0.0.1:0", server_tls()).value() : net::tcp::listen("127.0.0.1:0").value();
            port = listener.local_endpoint().port();
            serving = async::spawn(srv.async_serve(listener));
        }

        ~Server() {
            srv.close();
            (void)serving.wait();
        }

        std::string address() const {
            return "127.0.0.1:" + std::to_string(port);
        }

        sgcl::string url(const std::string& userinfo = "alice:secret") const {
            return sgcl::string("imap://" + userinfo + "@127.0.0.1:" + std::to_string(port));
        }
    };

    inline net::imap::memory_backend standard_mail() {
        net::imap::memory_backend mail;
        mail.add_user("alice", "secret");
        mail.add_user("bob", "hunter2");
        return mail;
    }

    // A connection that speaks IMAP as text
    struct Raw {
        net::connection c;
        std::string buf;

        explicit Raw(uint16_t port) {
            c = net::tcp::connect(sgcl::string("127.0.0.1:" + std::to_string(port))).value();
        }

        explicit Raw(const net::connection& conn)
        : c(conn) {
        }

        void send(const std::string& s) {
            (void)c.write(sgcl::slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size()));
        }

        // The next line without CRLF; "<eof>" or "<timeout>"
        std::string line(std::chrono::milliseconds wait = 5000ms) {
            c.set_read_deadline(sgcl::clock::now() + wait);
            for (;;) {
                const size_t lf = buf.find('\n');
                if (lf != std::string::npos) {
                    std::string l = buf.substr(0, lf);
                    buf.erase(0, lf + 1);
                    if (!l.empty() && l.back() == '\r') {
                        l.pop_back();
                    }
                    return l;
                }
                char tmp[8192];
                auto r = c.read(sgcl::slice<byte>(reinterpret_cast<byte*>(tmp), sizeof(tmp)));
                if (!r) {
                    return r.error().is_timeout() ? "<timeout>" : "<eof>";
                }
                if (*r == 0) {
                    return "<eof>";
                }
                buf.append(tmp, *r);
            }
        }

        // The lines to the tagged one (included), joined by "\n"
        std::string until(const std::string& tag, std::chrono::milliseconds wait = 5000ms) {
            std::string out;
            for (;;) {
                std::string l = line(wait);
                out += l;
                out += '\n';
                if (l.rfind(tag + " ", 0) == 0 || l == "<eof>" || l == "<timeout>") {
                    return out;
                }
            }
        }

        std::string cmd(const std::string& tag, const std::string& text) {
            send(tag + " " + text + "\r\n");
            return until(tag);
        }

        // Greeting read, logged in as alice
        void login(const std::string& user = "alice", const std::string& pass = "secret") {
            std::string g = line();
            EXPECT_EQ(g.rfind("* OK", 0), 0u) << g;
            std::string r = cmd("L1", "LOGIN " + user + " " + pass);
            EXPECT_NE(r.find("L1 OK"), std::string::npos) << r;
        }
    };

    inline bool contains(const std::string& haystack, const std::string& needle) {
        return haystack.find(needle) != std::string::npos;
    }

    // A fresh directory under the system's temporary one
    inline std::string scratch_dir(const std::string& name) {
        auto p = std::filesystem::temp_directory_path() / ("sgcl-imap-" + name + "-" + std::to_string(::getpid()));
        std::filesystem::remove_all(p);
        std::filesystem::create_directories(p);
        return p.string();
    }

    inline std::string run_command(const std::string& cmd, int* status = nullptr) {
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
        const int st = pclose(p);
        if (status) {
            *status = st;
        }
        return out;
    }

    inline bool have(const char* tool) {
        return std::system((std::string("command -v ") + tool + " > /dev/null 2>&1").c_str()) == 0;
    }
}
