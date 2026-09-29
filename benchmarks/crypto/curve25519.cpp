//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// X25519 and Ed25519 against OpenSSL: what one operation costs, one case
// and one side a run. Prints one line: ns per operation and operations per
// second.
//
//   curve25519 <case> <sgcl|openssl>
//
//   x25519-keygen     a private key from 32 bytes with its public key
//                     (private_key::from_bytes against
//                     EVP_PKEY_new_raw_private_key + get_raw_public_key)
//   x25519            a shared secret (shared_secret against
//                     EVP_PKEY_derive with a prepared context)
//   ed25519-keygen    a key from a seed (from_seed against
//                     EVP_PKEY_new_raw_private_key + get_raw_public_key)
//   ed25519-sign      a signature of 64 bytes (sign against EVP_DigestSign)
//   ed25519-verify    its verification (verify against EVP_DigestVerify,
//                     the public key decoded once on both sides)
//
// bench_curve25519 links libcrypto only when benchmarks/CMakeLists.txt
// finds OpenSSL; without it the openssl side is absent. Each run goes for
// about two seconds after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/crypto/ed25519.h"
#include "sgcl/crypto/x25519.h"

#if defined(SGCL_BENCH_OPENSSL)
#include <openssl/evp.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
            for (int i = 0; i < 64; ++i) {
                acc += f();
            }
            calls += 64;
            wall = bench::seconds_since(t0);
        } while (wall < seconds);
        sink = acc;
        return {calls, wall};
    }

    sgcl::slice<const sgcl::byte> as_slice(const unsigned char* p, size_t n) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(p), n);
    }

    unsigned char seed_a[32], seed_b[32], message[64];

    void fill() {
        for (int i = 0; i < 32; ++i) {
            seed_a[i] = (unsigned char)(i * 7 + 1);
            seed_b[i] = (unsigned char)(i * 13 + 5);
        }
        for (int i = 0; i < 64; ++i) {
            message[i] = (unsigned char)(i * 3);
        }
    }
}

int main(int argc, char** argv) {
    std::string side = argc > 2 ? argv[2] : "";
    if (argc < 3 || (side != "sgcl" && side != "openssl")) {
        std::fprintf(stderr, "usage: curve25519 <x25519-keygen|x25519|ed25519-keygen|ed25519-sign|ed25519-verify> <sgcl|openssl>\n");
        return 2;
    }
    std::string what = argv[1];
    fill();
    auto report = [&](auto&& f) {
        run_for(f, 0.25);
        auto [calls, wall] = run_for(f, 2.0);
        double ns = wall * 1e9 / double(calls);
        std::printf("curve25519 %s %s ns/op=%.0f op/s=%.0f wall=%.2fs\n", what.c_str(), side.c_str(), ns, 1e9 / ns, wall);
    };
    using namespace sgcl;
    if (side == "sgcl") {
        auto peer = crypto::x25519::private_key::from_bytes(as_slice(seed_b, 32))->public_key();
        auto xk = std::move(*crypto::x25519::private_key::from_bytes(as_slice(seed_a, 32)));
        auto ek = std::move(*crypto::ed25519::private_key::from_seed(as_slice(seed_a, 32)));
        auto sig = ek.sign(as_slice(message, 64));
        auto pub = ek.public_key();
        if (what == "x25519-keygen") {
            report([&] { return uint64_t(crypto::x25519::private_key::from_bytes(as_slice(seed_a, 32))->public_key().bytes()[0]); });
        } else if (what == "x25519") {
            report([&] { return uint64_t(xk.shared_secret(peer)->bytes()[0]); });
        } else if (what == "ed25519-keygen") {
            report([&] { return uint64_t(crypto::ed25519::private_key::from_seed(as_slice(seed_a, 32))->public_key().bytes()[0]); });
        } else if (what == "ed25519-sign") {
            report([&] { return uint64_t(ek.sign(as_slice(message, 64))[0]); });
        } else if (what == "ed25519-verify") {
            report([&] { return uint64_t(pub.verify(as_slice(message, 64), sig)); });
        } else {
            std::fprintf(stderr, "unknown case %s\n", what.c_str());
            return 2;
        }
        return 0;
    }
#if defined(SGCL_BENCH_OPENSSL)
    EVP_PKEY* xa = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, seed_a, 32);
    EVP_PKEY* xb = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, seed_b, 32);
    EVP_PKEY* ea = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed_a, 32);
    unsigned char pub[32], sig[64];
    size_t n = 32;
    EVP_PKEY_get_raw_public_key(ea, pub, &n);
    EVP_PKEY* ep = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, pub, 32);
    EVP_MD_CTX* mc = EVP_MD_CTX_new();
    n = 64;
    EVP_DigestSignInit(mc, nullptr, nullptr, nullptr, ea);
    EVP_DigestSign(mc, sig, &n, message, 64);
    if (what == "x25519-keygen" || what == "ed25519-keygen") {
        int type = what == "x25519-keygen" ? EVP_PKEY_X25519 : EVP_PKEY_ED25519;
        report([&] {
            EVP_PKEY* k = EVP_PKEY_new_raw_private_key(type, nullptr, seed_a, 32);
            unsigned char p[32];
            size_t m = 32;
            EVP_PKEY_get_raw_public_key(k, p, &m);
            EVP_PKEY_free(k);
            return uint64_t(p[0]);
        });
    } else if (what == "x25519") {
        EVP_PKEY_CTX* c = EVP_PKEY_CTX_new(xa, nullptr);
        EVP_PKEY_derive_init(c);
        EVP_PKEY_derive_set_peer(c, xb);
        report([&] {
            unsigned char out[32];
            size_t m = 32;
            EVP_PKEY_derive(c, out, &m);
            return uint64_t(out[0]);
        });
        EVP_PKEY_CTX_free(c);
    } else if (what == "ed25519-sign") {
        report([&] {
            unsigned char s[64];
            size_t m = 64;
            EVP_DigestSignInit(mc, nullptr, nullptr, nullptr, ea);
            EVP_DigestSign(mc, s, &m, message, 64);
            return uint64_t(s[0]);
        });
    } else if (what == "ed25519-verify") {
        report([&] {
            EVP_DigestVerifyInit(mc, nullptr, nullptr, nullptr, ep);
            return uint64_t(EVP_DigestVerify(mc, sig, 64, message, 64));
        });
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    EVP_MD_CTX_free(mc);
    EVP_PKEY_free(xa);
    EVP_PKEY_free(xb);
    EVP_PKEY_free(ea);
    EVP_PKEY_free(ep);
    return 0;
#else
    std::fprintf(stderr, "built without OpenSSL\n");
    return 2;
#endif
}
