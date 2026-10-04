//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The zeroing probe of the TLS connection (ZeroingProbe, record.h): every
// traffic secret the connections take (install and KeyUpdate) is recorded
// with the key and IV made of it, and every block the record layer lets go
// of — a record's block given back to its list, a buffer, the connection's
// block of secrets after its destructor, before its memory is freed — is
// searched for them. Our client and our server over the loopback exchange
// lines and a record larger than the reader's buffer, close, and are
// collected: no secret, key or IV is left in any block.
#include "tests/types.h"
#include "tests/source_root.h"

#include "sgcl/net/tls.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace tls = sgcl::net::tls;

namespace {
    using bytes_t = std::vector<uint8_t>;

    struct Seen {
        std::mutex lock;
        std::vector<bytes_t> secrets;   // the traffic secrets, their keys and IVs
        size_t installs = 0, blocks = 0, connection_blocks = 0, found = 0;
    };

    Seen& seen() {
        static Seen s;
        return s;
    }

    void on_install(tls::detail::Cipher c, const tls::detail::Secret& s) {
        auto& z = seen();
        std::lock_guard<std::mutex> g(z.lock);
        ++z.installs;
        z.secrets.emplace_back(s.bytes, s.bytes + s.size);
        tls::detail::TrafficKeys keys;
        tls::detail::traffic_keys(tls::detail::hash_of(c), keys, s, tls::detail::key_size(c));
        z.secrets.emplace_back(keys.key, keys.key + keys.key_size);
        z.secrets.emplace_back(keys.iv, keys.iv + 12);
    }

    size_t occurrences(const uint8_t* p, size_t n, const std::vector<bytes_t>& patterns) {
        size_t hits = 0;
        for (auto& pat : patterns) {
            if (std::search(p, p + n, pat.begin(), pat.end()) != p + n) {
                ++hits;
            }
        }
        return hits;
    }

    void on_release(const void* p, size_t n, tls::detail::ZeroingProbe::Kind k) {
        auto& z = seen();
        std::lock_guard<std::mutex> g(z.lock);
        ++z.blocks;
        if (k == tls::detail::ZeroingProbe::connection_block) {
            ++z.connection_blocks;
        }
        z.found += occurrences(static_cast<const uint8_t*>(p), n, z.secrets);
    }

    std::string testdata(const std::string& name) {
        return (source_root() / "tests/net/tls_testdata" / name).string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    // One exchange over our client and server, everything dropped at its end
    SGCL_NOINLINE void exchange() {
        auto l = net::tcp::listen("127.0.0.1:0");
        ASSERT_TRUE(l.has_value());
        tls::config scfg;
        scfg.identities = {tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
        std::string served;
        std::thread server([&] {
            auto a = l->accept();
            if (!a) {
                return;
            }
            auto s = tls::server(*a, scfg);
            if (!s) {
                return;
            }
            // a line, then 40 000 bytes read 100 at a time (a record larger
            // than the reader's buffer: the plaintext block), then the end
            auto line = s->read_line();
            if (line && *line) {
                served = std::string((*line)->view());
            }
            (void)s->write(sgcl::string("reply\n"));
            byte buf[100];
            size_t total = 0;
            while (total < 40000) {
                auto n = s->read(buf);
                if (!n || *n == 0) {
                    break;
                }
                total += *n;
            }
            (void)s->read(buf);   // the close_notify
            (void)s->close();
        });
        tls::config ccfg;
        ccfg.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
        ccfg.server_name = sgcl::string("localhost");
        // (the blocking connect to an endpoint and the handshake on this
        // thread: no worker's stack keeps a word of the connection)
        auto t = net::tcp::connect(l->local_endpoint());
        ASSERT_TRUE(t.has_value());
        auto c = tls::client(*t, ccfg);
        ASSERT_TRUE(c.has_value());
        ASSERT_TRUE(c->write(sgcl::string("hello\n")).has_value());
        auto reply = c->read_line();
        ASSERT_TRUE(reply && *reply);
        ASSERT_TRUE(c->write(sgcl::string(std::string(40000, 'k'))).has_value());
        (void)c->close();
        server.join();
        (void)l->close();
        EXPECT_EQ(served, "hello");
    }
}

TEST(TlsZeroing, NoSecretLeftInTheBlocksAConnectionLetsGo) {
    // the probe finds what it looks for (a control)
    {
        bytes_t secret = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        bytes_t block(64, 0);
        std::copy(secret.begin(), secret.end(), block.begin() + 20);
        EXPECT_EQ(occurrences(block.data(), block.size(), {secret}), 1u);
        std::fill(block.begin(), block.end(), 0);
        EXPECT_EQ(occurrences(block.data(), block.size(), {secret}), 0u);
    }
    auto& z = seen();
    // the connections of the tests before collected first: counted apart
    // from this test's two (every block is searched all the same)
    tls::detail::ZeroingProbe::released.store(&on_release);
    size_t before = 0;
    for (int i = 0; i < 100; ++i) {
        collector::clear_stack();
        collector::force_collect(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> g(z.lock);
        if (i >= 5 && z.connection_blocks == before) {
            break;
        }
        before = z.connection_blocks;
    }
    {
        std::lock_guard<std::mutex> g(z.lock);
        z.connection_blocks = 0;
    }
    tls::detail::ZeroingProbe::installed.store(&on_install);
    tls::detail::ZeroingProbe::released.store(&on_release);
    std::thread([] { exchange(); }).join();   // its stack gone with it
    // the connections collected: their blocks of secrets released
    for (int i = 0; i < 200; ++i) {
        collector::clear_stack();
        collector::force_collect(true);
        {
            std::lock_guard<std::mutex> g(z.lock);
            if (z.connection_blocks >= 2) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tls::detail::ZeroingProbe::installed.store(nullptr);
    tls::detail::ZeroingProbe::released.store(nullptr);
    std::lock_guard<std::mutex> g(z.lock);
    // both sides: handshake read and write, application read and write
    EXPECT_GE(z.installs, 8u);
    EXPECT_GE(z.connection_blocks, 2u);   // the client's and the server's, after their collection
    EXPECT_GE(z.blocks, 8u);
    EXPECT_EQ(z.found, 0u) << "a secret, a key or an IV left in a block a connection let go of";
}
