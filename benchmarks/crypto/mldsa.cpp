//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// crypto::mldsa against Go's crypto/mldsa (benchmarks/go/mldsa): one case a
// run, the key made before the clock starts. Prints one line with ns/op.
//
//   mldsa <case> sgcl
//
//   keygen_44, keygen_65, keygen_87
//       a private key of a fixed seed (KeyGen_internal) and its public key's bytes
//   sign_44, sign_65, sign_87
//       a hedged signature of 64 bytes, no context (Go's Sign)
//   verify_44, verify_65, verify_87
//       its verification under a public key made once
//
// About two seconds a run after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/crypto/mldsa.h"

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
        std::printf("mldsa %s ns/op=%.1f wall=%.2fs\n", what, wall * 1e9 / double(calls), wall);
    }

    template<class Private>
    int run(const std::string& what, const std::string& kind) {
        uint8_t seed_bytes[32];
        for (int i = 0; i < 32; ++i) {
            seed_bytes[i] = uint8_t(i * 7 + 1);
        }
        const slice<const byte> seed(reinterpret_cast<const byte*>(seed_bytes), 32);
        const std::string data(64, 'm');
        const slice<const byte> msg(reinterpret_cast<const byte*>(data.data()), data.size());
        auto key = Private::from_seed(seed).value();
        if (kind == "keygen") {
            measure(what.c_str(), [&] { return Private::from_seed(seed)->public_key().bytes().size(); });
        } else if (kind == "sign") {
            measure(what.c_str(), [&] { return key.sign(msg).size(); });
        } else if (kind == "verify") {
            auto pub = key.public_key();
            auto sig = key.sign(msg);
            measure(what.c_str(), [&] { return uint64_t(pub.verify(msg, sig)); });
        } else {
            std::fprintf(stderr, "mldsa: unknown case %s\n", what.c_str());
            return 2;
        }
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: mldsa <case> sgcl\n");
        return 2;
    }
    const std::string what = argv[1];
    const auto bar = what.find('_');
    if (bar == std::string::npos) {
        std::fprintf(stderr, "mldsa: unknown case %s\n", what.c_str());
        return 2;
    }
    const std::string kind = what.substr(0, bar), set = what.substr(bar + 1);
    if (set == "44") {
        return run<crypto::mldsa44::private_key>(what, kind);
    }
    if (set == "65") {
        return run<crypto::mldsa65::private_key>(what, kind);
    }
    if (set == "87") {
        return run<crypto::mldsa87::private_key>(what, kind);
    }
    std::fprintf(stderr, "mldsa: unknown case %s\n", what.c_str());
    return 2;
}
