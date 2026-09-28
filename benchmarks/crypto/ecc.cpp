//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The NIST curves against OpenSSL: one operation, one side a run. Prints
// one line: microseconds per operation and operations per second.
//
//   ecc <case> <sgcl|openssl>
//
//   p256-sign p384-sign       an ECDSA signature (DER) of a 32/48-byte digest:
//                             sign_digest against EVP_PKEY_sign
//   p256-verify p384-verify   its verification: verify_digest against
//                             EVP_PKEY_verify
//   p256-ecdh p384-ecdh       a shared secret with a fixed peer:
//                             shared_secret against EVP_PKEY_derive (the
//                             context made once, the peer set once)
//   p256-keygen p384-keygen   a new key and its public point: generate
//                             against EVP_PKEY_generate
//
// bench_ecc links libcrypto only when benchmarks/CMakeLists.txt finds
// OpenSSL (Homebrew's openssl@3). About two seconds a run after a quarter
// of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/crypto/crypto.h"

#if defined(SGCL_BENCH_OPENSSL)
#include <openssl/core_names.h>
#include <openssl/evp.h>
#endif

#include <cstdint>
#include <cstdio>
#include <string>

namespace {
    volatile uint64_t sink;

    template<class F>
    std::pair<uint64_t, double> run_for(F&& f, double seconds) {
        uint64_t calls = 0;
        uint64_t acc = 0;
        auto t0 = bench::Clock::now();
        double wall = 0;
        do {
            for (int i = 0; i < 16; ++i) {
                acc += f();
            }
            calls += 16;
            wall = bench::seconds_since(t0);
        } while (wall < seconds);
        sink = acc;
        return {calls, wall};
    }

    sgcl::slice<const sgcl::byte> as_slice(const unsigned char* p, size_t n) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(p), n);
    }

    const unsigned char digest[48] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                                      25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48};

    template<class Curve>
    int sgcl_case(const std::string& op, size_t dlen, auto measure) {
        using SK = typename Curve::private_key;
        using EK = typename Curve::ecdh_key;
        if (op == "sign") {
            auto key = SK::generate();
            measure([&] { return uint64_t(key.sign_digest(as_slice(digest, dlen)).size()); });
        } else if (op == "verify") {
            auto key = SK::generate();
            auto pub = key.public_key();
            auto sig = key.sign_digest(as_slice(digest, dlen));
            measure([&] { return uint64_t(pub.verify_digest(as_slice(digest, dlen), sig.as_slice())); });
        } else if (op == "ecdh") {
            auto key = EK::generate();
            auto peer = EK::generate().public_key();
            measure([&] { return uint64_t(key.shared_secret(peer)->bytes()[0]); });
        } else if (op == "keygen") {
            measure([&] { return uint64_t(SK::generate().public_key().bytes()[1]); });
        } else {
            return 2;
        }
        return 0;
    }

#if defined(SGCL_BENCH_OPENSSL)
    EVP_PKEY* ossl_generate(const char* group) {
        return EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", group);
    }

    int openssl_case(const char* group, const std::string& op, size_t dlen, auto measure) {
        EVP_PKEY* key = ossl_generate(group);
        if (op == "sign") {
            EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, nullptr);
            EVP_PKEY_sign_init(ctx);
            measure([&] {
                unsigned char sig[128];
                size_t n = sizeof sig;
                EVP_PKEY_sign(ctx, sig, &n, digest, dlen);
                return uint64_t(n);
            });
            EVP_PKEY_CTX_free(ctx);
        } else if (op == "verify") {
            EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, nullptr);
            EVP_PKEY_sign_init(ctx);
            unsigned char sig[128];
            size_t n = sizeof sig;
            EVP_PKEY_sign(ctx, sig, &n, digest, dlen);
            EVP_PKEY_CTX_free(ctx);
            ctx = EVP_PKEY_CTX_new(key, nullptr);
            EVP_PKEY_verify_init(ctx);
            measure([&] { return uint64_t(EVP_PKEY_verify(ctx, sig, n, digest, dlen)); });
            EVP_PKEY_CTX_free(ctx);
        } else if (op == "ecdh") {
            EVP_PKEY* peer = ossl_generate(group);
            EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, nullptr);
            EVP_PKEY_derive_init(ctx);
            EVP_PKEY_derive_set_peer(ctx, peer);
            measure([&] {
                unsigned char out[64];
                size_t n = sizeof out;
                EVP_PKEY_derive(ctx, out, &n);
                return uint64_t(out[0]);
            });
            EVP_PKEY_CTX_free(ctx);
            EVP_PKEY_free(peer);
        } else if (op == "keygen") {
            measure([&] {
                EVP_PKEY* k = ossl_generate(group);
                EVP_PKEY_free(k);
                return uint64_t(1);
            });
        } else {
            return 2;
        }
        EVP_PKEY_free(key);
        return 0;
    }
#endif
}

struct P256Names {
    using private_key = sgcl::crypto::p256::private_key;
    using ecdh_key = sgcl::crypto::p256::ecdh_key;
};

struct P384Names {
    using private_key = sgcl::crypto::p384::private_key;
    using ecdh_key = sgcl::crypto::p384::ecdh_key;
};

int main(int argc, char** argv) {
    std::string side = argc > 2 ? argv[2] : "";
    std::string what = argc > 1 ? argv[1] : "";
    size_t dash = what.find('-');
    if (argc < 3 || (side != "sgcl" && side != "openssl") || dash == std::string::npos) {
        std::fprintf(stderr, "usage: ecc <p256|p384>-<sign|verify|ecdh|keygen> <sgcl|openssl>\n");
        return 2;
    }
    std::string curve = what.substr(0, dash);
    std::string op = what.substr(dash + 1);
    size_t dlen = curve == "p256" ? 32 : 48;
    auto measure = [&](auto f) {
        run_for(f, 0.25);   // thrown away
        auto [calls, wall] = run_for(f, 2.0);
        double us = wall * 1e6 / double(calls);
        std::printf("ecc %s %s us/op=%.2f op/s=%.0f wall=%.2fs\n", what.c_str(), side.c_str(), us, double(calls) / wall, wall);
    };
    int rc = 2;
    if (side == "sgcl") {
        if (curve == "p256") {
            rc = sgcl_case<P256Names>(op, dlen, measure);
        } else if (curve == "p384") {
            rc = sgcl_case<P384Names>(op, dlen, measure);
        }
    } else {
#if defined(SGCL_BENCH_OPENSSL)
        rc = openssl_case(curve == "p256" ? "P-256" : "P-384", op, dlen, measure);
#else
        std::fprintf(stderr, "built without OpenSSL\n");
#endif
    }
    if (rc != 0) {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
    }
    return rc;
}
