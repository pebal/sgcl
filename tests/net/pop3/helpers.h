//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of net::pop3 share: a server on the loopback over a
// memory_backend, a client of raw lines, the tree's test certificates.
#pragma once

#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace pop3_test {
    using namespace sgcl;
    using namespace std::chrono_literals;
    namespace pop3 = sgcl::net::pop3;

    inline std::string str(const sgcl::string& s) {
        return std::string(s.view());
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

    // A server on 127.0.0.1 for as long as the object lives (on the test's
    // stack): alice (secret) with two messages, bob (hunter2) with none
    struct Server {
        net::imap::memory_backend mail;
        pop3::server srv;
        net::listener listener;
        async::task<expected<void, io::error>> serving;

        explicit Server(void (*setup)(pop3::server&) = nullptr, bool tls_listener = false) {
            mail.add_user("alice", "secret");
            mail.add_user("bob", "hunter2");
            (void)mail.append("alice", "INBOX", "From: Bob <bob@example.com>\r\nSubject: Lunch\r\n\r\nNoon?\r\n.hidden dot\r\n");
            (void)mail.append("alice", "INBOX", "From: Carol <carol@example.com>\r\nSubject: Report\r\n\r\nLine 1\r\nLine 2\r\nLine 3\r\n");
            srv.backend = mail;
            if (setup) {
                setup(srv);
            }
            listener = tls_listener ? net::tls::listen("127.0.0.1:0", server_tls()).value() : net::tcp::listen("127.0.0.1:0").value();
            serving = async::spawn(srv.async_serve(listener));
        }

        ~Server() {
            srv.close();
            serving.wait();
        }

        sgcl::string address() const {
            return listener.local_endpoint().to_string();
        }
    };

    // A client of raw lines
    struct Talk {
        net::connection c;
        std::string buf;

        explicit Talk(const sgcl::string& address) {
            auto conn = net::tcp::connect(address);
            EXPECT_TRUE(conn);
            if (conn) {
                c = *conn;
                c.set_deadline(sgcl::clock::now() + 10s);
            }
        }

        void send(const std::string& text) {
            ASSERT_TRUE(c.write(sgcl::string(text)));
        }

        // One line without its CRLF; "EOF" at the end
        std::string line() {
            for (;;) {
                size_t nl = buf.find("\r\n");
                if (nl != std::string::npos) {
                    std::string l = buf.substr(0, nl);
                    buf.erase(0, nl + 2);
                    return l;
                }
                char tmp[4096];
                auto r = c.read(slice<byte>(reinterpret_cast<byte*>(tmp), sizeof tmp));
                if (!r || *r == 0) {
                    return "EOF";
                }
                buf.append(tmp, *r);
            }
        }

        // The lines of a multi-line response up to the dot, raw (stuffed)
        std::string lines() {
            std::string out;
            for (;;) {
                std::string l = line();
                if (l == "." || l == "EOF") {
                    return out;
                }
                out += l + "\n";
            }
        }

        std::string ask(const std::string& command) {
            send(command + "\r\n");
            return line();
        }
    };

    inline pop3::client::options plain_options(const char* user = "alice", const char* password = "secret") {
        pop3::client::options o;
        o.user = user;
        o.password = password;
        o.security = pop3::security::none;
        return o;
    }
}
