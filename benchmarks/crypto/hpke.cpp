//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// crypto::hpke against Go's crypto/hpke (benchmarks/go/hpke): one case a
// run, the recipient's key made before the clock starts. Prints one line
// with ns/op.
//
//   hpke <case> sgcl
//
//   seal_x25519, seal_p256, seal_p384, seal_p521
//       a single-shot seal of 1 KB to the key (the KEM's encapsulation,
//       the key schedule, one AEAD seal): DHKEM of the curve, HKDF of its
//       hash, AES-128-GCM (P-384, P-521: AES-256-GCM)
//   open_x25519, open_p256, open_p384, open_p521
//       its single-shot open
//   context_seal
//       1 KB sealed under a context made once (X25519, AES-128-GCM)
//   export
//       32 bytes exported from a context made once
//
// About two seconds a run after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/crypto/crypto.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace {
    using namespace sgcl;
    namespace hpke = sgcl::crypto::hpke;

    volatile uint64_t sink;

    template<class F>
    void measure(const char* what, F&& f) {
        auto run_for = [&](double seconds) {
            uint64_t calls = 0, acc = 0;
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
            return std::pair<uint64_t, double>(calls, wall);
        };
        run_for(0.25);
        auto [calls, wall] = run_for(2.0);
        std::printf("hpke %s ns/op=%.1f wall=%.2fs\n", what, wall * 1e9 / double(calls), wall);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: hpke <case> sgcl\n");
        return 2;
    }
    const std::string what = argv[1];
    const std::string data(1024, 'x');
    const slice<const byte> pt(reinterpret_cast<const byte*>(data.data()), data.size());
    const slice<const byte> info(reinterpret_cast<const byte*>("info"), 4);
    const bool p256 = what.find("p256") != std::string::npos, p384 = what.find("p384") != std::string::npos,
               p521 = what.find("p521") != std::string::npos;
    const hpke::kem k = p256 ? hpke::kem::dhkem_p256 : p384 ? hpke::kem::dhkem_p384 : p521 ? hpke::kem::dhkem_p521 : hpke::kem::dhkem_x25519;
    const hpke::suite s = p384 ? hpke::suite{hpke::kdf::hkdf_sha384, hpke::aead::aes256_gcm}
                        : p521 ? hpke::suite{hpke::kdf::hkdf_sha512, hpke::aead::aes256_gcm}
                               : hpke::suite{};
    auto key = hpke::private_key::generate(k);
    auto pub = key.public_key();
    if (what.rfind("seal_", 0) == 0) {
        measure(what.c_str(), [&] { return hpke::seal(pub, pt, s, info)->size(); });
    } else if (what.rfind("open_", 0) == 0) {
        auto sealed = *hpke::seal(pub, pt, s, info);
        measure(what.c_str(), [&] { return hpke::open(key, sealed, s, info)->size(); });
    } else if (what == "context_seal") {
        auto c = std::move(*hpke::sender::setup(pub, s, info));
        measure(what.c_str(), [&] { return c.seal(pt).size(); });
    } else if (what == "export") {
        auto c = std::move(*hpke::sender::setup(pub, s, info));
        const slice<const byte> ctx(reinterpret_cast<const byte*>("context"), 7);
        measure(what.c_str(), [&] { return c.export_secret(ctx, 32).size(); });
    } else {
        std::fprintf(stderr, "hpke: unknown case %s\n", what.c_str());
        return 2;
    }
    return 0;
}
