//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of net::mqtt share: a broker on the loopback for as long as
// the object lives, its URL, clients of it, the tree's test certificates.
#pragma once

#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/mqtt.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace mqtt_test {
    using namespace sgcl;
    using namespace std::chrono_literals;
    namespace mqtt = sgcl::net::mqtt;

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

    struct Broker {
        mqtt::broker b;
        net::listener listener;
        async::task<expected<void, io::error>> serving;

        explicit Broker(void (*setup)(mqtt::broker&) = nullptr, bool tls = false) {
            if (setup) {
                setup(b);
            }
            listener = tls ? net::tls::listen("127.0.0.1:0", server_tls()).value() : net::tcp::listen("127.0.0.1:0").value();
            serving = async::spawn(b.async_serve(listener));
        }

        ~Broker() {
            b.close();
            serving.wait();
        }

        uint16_t port() const {
            return listener.local_endpoint().port();
        }

        sgcl::string url(const char* scheme = "mqtt") const {
            return sgcl::string(std::string(scheme) + "://127.0.0.1:" + std::to_string(port()));
        }

        mqtt::client connect(mqtt::client::options o = {}) const {
            auto c = mqtt::client::connect(url(), o);
            EXPECT_TRUE(c) << (c ? std::string() : str(c.error().message()));
            return c ? *c : mqtt::client();
        }

        mqtt::client connect_v3(mqtt::client::options o = {}) const {
            o.version = mqtt::version::v3_1_1;
            return connect(o);
        }
    };

    // The next message within a time, or none
    inline optional<mqtt::message> next(const mqtt::client& c, std::chrono::milliseconds wait = 2000ms) {
        auto deadline = std::chrono::steady_clock::now() + wait;
        while (std::chrono::steady_clock::now() < deadline) {
            if (auto m = c.try_receive()) {
                return m;
            }
            std::this_thread::sleep_for(2ms);
        }
        return nullopt;
    }
}
