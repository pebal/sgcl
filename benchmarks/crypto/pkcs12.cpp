//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// crypto::pkcs12 (RFC 7292): one case a run, the key and its certificate made
// before the clock starts. Prints one line with ns/op. Go's standard library
// has no PKCS #12; the reference is OpenSSL 3.6's PKCS12_create and
// PKCS12_parse with the same parameters (a program of its own outside the
// tree).
//
//   pkcs12 <case> sgcl
//
//   encode_p256, encode_rsa
//       a file of the key and its certificate as OpenSSL 3 writes one by
//       default: PBES2 (PBKDF2-HMAC-SHA-256, 2048 iterations, AES-256-CBC)
//       for the key and the certificate, a MAC of HMAC-SHA-256
//   parse_p256, parse_rsa
//       that file read: the MAC checked, both parts decrypted, the key and
//       the certificate read
//
// About two seconds a run after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/crypto/pkcs12.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace {
    using namespace sgcl;

    volatile uint64_t sink;

    template<class F>
    void measure(const char* what, F&& f) {
        auto run_for = [&](double seconds) {
            uint64_t calls = 0, acc = 0;
            auto t0 = bench::Clock::now();
            double wall = 0;
            do {
                acc += f();
                ++calls;
                wall = bench::seconds_since(t0);
            } while (wall < seconds);
            sink = acc;
            return std::pair<uint64_t, double>(calls, wall);
        };
        run_for(0.25);
        auto [calls, wall] = run_for(2.0);
        std::printf("pkcs12 %s ns/op=%.1f wall=%.2fs\n", what, wall * 1e9 / double(calls), wall);
    }

    template<class Key>
    int run(const std::string& what, const Key& key) {
        crypto::x509::certificate_template t;
        t.common_name = "bench";
        crypto::x509::chain chain;
        chain.push_back(crypto::x509::create_certificate(t, key));
        auto file = crypto::pkcs12::encode(key, chain, "password");
        if (what.rfind("encode_", 0) == 0) {
            measure(what.c_str(), [&] { return crypto::pkcs12::encode(key, chain, "password").size(); });
        } else {
            measure(what.c_str(), [&] { return crypto::pkcs12::parse(file, "password")->certificates().size(); });
        }
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: pkcs12 <case> sgcl\n");
        return 2;
    }
    const std::string what = argv[1];
    if (what == "encode_p256" || what == "parse_p256") {
        return run(what, crypto::p256::private_key::generate());
    }
    if (what == "encode_rsa" || what == "parse_rsa") {
        return run(what, crypto::rsa::private_key::generate(2048));
    }
    std::fprintf(stderr, "pkcs12: unknown case %s\n", what.c_str());
    return 2;
}
