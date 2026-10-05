//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The versions in tls::config and tls::state (sgcl/net/tls.h): the defaults
// (the client offers 1.3 and 1.2, its 1.2 suites in the list), the range at
// its edges (min above max, a value of no version, 1.2 alone, 1.3 alone),
// a list whose suites leave no version in the range, 1.2 alone with no
// curve of it, the server refusing a range without 1.3 and a list without
// a 1.3 suite (the 1.2 suites of its list passed over), and the state of a
// 1.3 connection on both sides.
#include "tests/types.h"
#include "tests/source_root.h"

#include "sgcl/net/tls.h"

#include <fstream>
#include <sstream>
#include <string>
#include <thread>

namespace tls = sgcl::net::tls;

namespace {
    std::string slurp(const std::string& name) {
        std::ifstream in((source_root() / "tests/net/tls_testdata" / name).string());
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    tls::config server_config() {
        tls::config c;
        c.identities = {tls::identity(sgcl::string(slurp("ecdsa.pem")), sgcl::string(slurp("ecdsa.key")))};
        return c;
    }

    // The error of a client's handshake over a connection in memory (EINVAL
    // comes before a byte is sent)
    std::string client_error(const tls::config& c) {
        auto [a, b] = net::connection::in_memory();
        auto r = tls::client(a, c);
        return r ? std::string() : std::string(r.error().message().view());
    }

    std::string server_error(const tls::config& c) {
        auto l = tls::listen("127.0.0.1:0", c);
        if (l) {
            (void)l->close();
            return std::string();
        }
        return std::string(l.error().message().view());
    }

    bool invalid(const std::string& e) {
        return e.find("Invalid argument") != std::string::npos;
    }
}

TEST(Tls12Config, TheDefaults) {
    tls::config c;
    EXPECT_EQ(c.min_version, tls::version::tls12);
    EXPECT_EQ(c.max_version, tls::version::tls13);
    ASSERT_EQ(c.ciphers.size(), 9u);
    EXPECT_EQ(c.ciphers[0], tls::cipher::aes_128_gcm_sha256);
    EXPECT_EQ(c.ciphers[3], tls::cipher::ecdhe_ecdsa_aes_128_gcm_sha256);
    EXPECT_EQ(uint16_t(tls::version::tls12), 0x0303);
    EXPECT_EQ(uint16_t(tls::version::tls13), 0x0304);
    tls::state s;
    EXPECT_EQ(s.version, tls::version::tls13);
}

TEST(Tls12Config, TheRangeAtItsEdges) {
    tls::config c;
    c.insecure_skip_verify = true;
    c.handshake_timeout = 100 * sgcl::millisecond;
    // min above max, values of no version
    c.min_version = tls::version::tls13;
    c.max_version = tls::version::tls12;
    EXPECT_TRUE(invalid(client_error(c)));
    c.min_version = tls::version(0x0301);
    c.max_version = tls::version::tls13;
    EXPECT_TRUE(invalid(client_error(c)));
    c.min_version = tls::version::tls12;
    c.max_version = tls::version(0x0305);
    EXPECT_TRUE(invalid(client_error(c)));
    // 1.2 alone, 1.3 alone: taken (the handshake then times out in memory)
    for (auto [lo, hi] : {std::pair{tls::version::tls12, tls::version::tls12}, std::pair{tls::version::tls13, tls::version::tls13}}) {
        c.min_version = lo;
        c.max_version = hi;
        std::string e = client_error(c);
        EXPECT_FALSE(invalid(e)) << e;
    }
    // suites that leave no version of the range: 1.3's alone with max 1.2, 1.2's alone with min 1.3
    c.min_version = tls::version::tls12;
    c.max_version = tls::version::tls12;
    c.ciphers = {tls::cipher::aes_128_gcm_sha256};
    EXPECT_TRUE(invalid(client_error(c)));
    c.min_version = tls::version::tls13;
    c.max_version = tls::version::tls13;
    c.ciphers = {tls::cipher::ecdhe_rsa_aes_128_gcm_sha256};
    EXPECT_TRUE(invalid(client_error(c)));
    // 1.2 alone with the hybrid alone: no curve of 1.2
    c.min_version = tls::version::tls12;
    c.max_version = tls::version::tls12;
    c.ciphers = {tls::cipher::ecdhe_rsa_aes_128_gcm_sha256};
    c.groups = {tls::group::x25519_mlkem768};
    EXPECT_TRUE(invalid(client_error(c)));
    // ... which 1.3 beside it makes whole
    c.max_version = tls::version::tls13;
    c.ciphers = {tls::cipher::aes_128_gcm_sha256, tls::cipher::ecdhe_rsa_aes_128_gcm_sha256};
    EXPECT_FALSE(invalid(client_error(c)));
}

TEST(Tls12Config, TheServerIsOf13Alone) {
    tls::config c = server_config();
    EXPECT_EQ(server_error(c), "");   // the default: 1.2's suites in its list, passed over
    c.max_version = tls::version::tls12;
    EXPECT_TRUE(invalid(server_error(c)));
    c.max_version = tls::version::tls13;
    c.min_version = tls::version::tls13;
    EXPECT_EQ(server_error(c), "");
    c.ciphers = {tls::cipher::ecdhe_ecdsa_aes_128_gcm_sha256};
    EXPECT_TRUE(invalid(server_error(c)));
    auto [a, b] = net::connection::in_memory();
    auto s = tls::server(a, c);
    ASSERT_FALSE(s.has_value());
    EXPECT_TRUE(invalid(std::string(s.error().message().view())));
}

TEST(Tls12Config, TheVersionOfA13Connection) {
    tls::config scfg = server_config();
    auto l = tls::listen("127.0.0.1:0", scfg);
    ASSERT_TRUE(l.has_value());
    std::string address = "localhost:" + std::to_string(l->local_endpoint().port());
    bool server13 = false;
    std::thread t([&] {
        auto c = l->accept();
        if (c) {
            auto st = tls::state_of(*c);
            server13 = st && st->version == tls::version::tls13;
            (void)c->close();
        }
    });
    tls::config cfg;
    cfg.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp("ca.pem")));
    auto c = tls::connect(sgcl::string(address), cfg);
    ASSERT_TRUE(c.has_value()) << std::string(c.error().message().view());
    EXPECT_EQ(tls::state_of(*c)->version, tls::version::tls13);
    (void)c->close();
    t.join();
    (void)l->close();
    EXPECT_TRUE(server13);
}
