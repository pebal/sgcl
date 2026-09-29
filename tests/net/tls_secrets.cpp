//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// No secret of a TLS server in managed memory (the rule that secrets never
// lie there): the identity made from the key's PEM, held in plain memory
// by the test, then an exchange of our client and our server over the
// loopback; the managed pages in use searched (tests/managed_scan.h) for
// the private key's scalar and its DER, and for every traffic secret, key
// and IV the connections took (the install hook of the zeroing probe,
// record.h). A control first: a secret put in managed memory on purpose is
// found.
#include "tests/managed_scan.h"

#include "sgcl/crypto/p256.h"
#include "sgcl/net/tls.h"

#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace tls = sgcl::net::tls;
using managed_scan::bytes_t;

namespace {
    std::mutex installed_lock;
    std::vector<bytes_t> installed;   // the traffic secrets, their keys and IVs

    void on_install(tls::detail::Cipher c, const tls::detail::Secret& s) {
        std::lock_guard<std::mutex> g(installed_lock);
        installed.emplace_back(s.bytes, s.bytes + s.size);
        tls::detail::TrafficKeys keys;
        tls::detail::traffic_keys(tls::detail::hash_of(c), keys, s, tls::detail::key_size(c));
        installed.emplace_back(keys.key, keys.key + keys.key_size);
        installed.emplace_back(keys.iv, keys.iv + 12);
    }

    std::string testdata(const std::string& name) {
        std::string f = __FILE__;
        return f.substr(0, f.rfind('/')) + "/tls_testdata/" + name;
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    // The key's PEM where the test holds it: plain memory
    sgcl::slice<const std::byte> key_pem(const std::string& text) {
        return sgcl::slice<const std::byte>(std::string_view(text));
    }

    SGCL_NOINLINE void exchange(const std::string& key) {
        auto l = net::tcp::listen("127.0.0.1:0");
        ASSERT_TRUE(l.has_value());
        tls::config scfg;
        auto id = tls::identity::from_pem(sgcl::string(slurp(testdata("ecdsa.pem"))), key_pem(key));
        ASSERT_TRUE(id.has_value());
        scfg.identities = {*id};
        std::thread server([&] {
            auto a = l->accept();
            if (!a) {
                return;
            }
            auto s = tls::server(*a, scfg);
            if (!s) {
                return;
            }
            auto line = s->read_line();
            (void)line;
            (void)s->write(sgcl::string("reply\n"));
            byte buf[4096];
            size_t total = 0;
            while (total < 200000) {
                auto n = s->read(buf);
                if (!n || *n == 0) {
                    break;
                }
                total += *n;
            }
            (void)s->read(buf);
            (void)s->close();
        });
        tls::config ccfg;
        ccfg.roots = crypto::x509::certificate_pool::from_pem(sgcl::string(slurp(testdata("ca.pem"))));
        ccfg.server_name = sgcl::string("localhost");
        auto t = net::tcp::connect(l->local_endpoint());
        ASSERT_TRUE(t.has_value());
        auto c = tls::client(*t, ccfg);
        ASSERT_TRUE(c.has_value());
        ASSERT_TRUE(c->write(sgcl::string("hello\n")).has_value());
        auto reply = c->read_line();
        ASSERT_TRUE(reply && *reply);
        ASSERT_TRUE(c->write(sgcl::string(std::string(200000, 'k'))).has_value());
        (void)c->close();
        server.join();
        (void)l->close();
    }
}

TEST(TlsSecrets, TheScanFindsWhatIsThere) {
    std::vector<bytes_t> patterns = {{0x5e, 0xc2, 0xe7, 0x01, 0x9a, 0x44, 0x71, 0x0d, 0xbe, 0x33, 0x86, 0x27, 0xf0, 0x12, 0xd9, 0x6c,
                                      0x48, 0xa1, 0x3f, 0x77, 0xc5, 0x0b, 0xe2, 0x94, 0x6d, 0x19, 0xab, 0x52, 0x80, 0x3e, 0xf6, 0x2d}};
    sgcl::vector<std::byte> held;
    const size_t found = managed_scan::found_after(patterns, [&] {
        held.assign(reinterpret_cast<const std::byte*>(patterns[0].data()), reinterpret_cast<const std::byte*>(patterns[0].data() + 32));
    });
    EXPECT_EQ(found, 1u);
    std::vector<bytes_t> absent = {bytes_t(patterns[0].rbegin(), patterns[0].rend())};
    EXPECT_EQ(managed_scan::found_after(absent, [] {}), 0u);
}

TEST(TlsSecrets, NoKeyAndNoTrafficSecretInManagedMemory) {
    const std::string key = slurp(testdata("ecdsa.key"));
    // the scalar and the DER of the key, the patterns
    std::vector<bytes_t> patterns;
    {
        auto k = crypto::p256::private_key::from_pem(key_pem(key));
        ASSERT_TRUE(k.has_value());
        auto scalar = k->bytes();
        patterns.emplace_back(reinterpret_cast<const uint8_t*>(scalar.bytes().data()), reinterpret_cast<const uint8_t*>(scalar.bytes().data()) + 32);
        auto der = k->to_pkcs8_der();
        auto d = der.as_slice();
        patterns.emplace_back(reinterpret_cast<const uint8_t*>(d.data()), reinterpret_cast<const uint8_t*>(d.data()) + d.size());
    }
    tls::detail::ZeroingProbe::installed.store(&on_install);
    std::vector<size_t> which;
    const size_t found = managed_scan::found_after(
        patterns,
        [&] {
            std::thread([&] { exchange(key); }).join();
            std::lock_guard<std::mutex> g(installed_lock);
            patterns.insert(patterns.end(), installed.begin(), installed.end());
        },
        &which);
    tls::detail::ZeroingProbe::installed.store(nullptr);
    EXPECT_GE(patterns.size(), 2u + 8 * 3);   // the key, and both sides' traffic secrets with their keys and IVs
    std::string names;
    for (size_t i : which) {
        names += i == 0 ? " the key's scalar" : i == 1 ? " the key's DER" : " traffic secret/key/IV " + std::to_string(i - 2);
    }
    EXPECT_EQ(found, 0u) << "in managed memory:" << names;
}
