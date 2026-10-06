//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// crypto::slhdsa (FIPS 205): one case a run, the key made of fixed seeds
// before the clock starts. Prints one line with ns/op. Go has no SLH-DSA;
// the reference is OpenSSL 3.6's EVP (the same three operations, measured
// by a program of its own outside the tree).
//
//   slhdsa <case> sgcl
//
//   keygen_<set>   a private key of the fixed seeds (PK.root computed)
//   sign_<set>     a hedged signature of 64 bytes, no context
//   verify_<set>   its verification
//
// <set>: sha2_128s, sha2_128f, sha2_192f, sha2_256f, shake_128s, shake_128f.
// About two seconds a run after a quarter of a second thrown away (at least
// four calls: a signature of an s set takes a tenth of a second).
#include "benchmarks/common.h"
#include "sgcl/crypto/slhdsa.h"

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
            } while (wall < seconds || calls < 4);
            sink = acc;
            return std::pair<uint64_t, double>(calls, wall);
        };
        run_for(0.25);
        auto [calls, wall] = run_for(2.0);
        std::printf("slhdsa %s ns/op=%.1f wall=%.2fs\n", what, wall * 1e9 / double(calls), wall);
    }

    template<class Private>
    int run(const std::string& what, const std::string& kind) {
        constexpr size_t n = Private::params::n;
        uint8_t seeds[3 * n];
        for (size_t i = 0; i < sizeof seeds; ++i) {
            seeds[i] = uint8_t(i * 7 + 1);
        }
        const std::string data(64, 'm');
        const slice<const byte> msg(reinterpret_cast<const byte*>(data.data()), data.size());
        auto key = crypto::detail::slhdsa::Access::from_seeds<Private>(seeds);
        if (kind == "keygen") {
            measure(what.c_str(), [&] { return crypto::detail::slhdsa::Access::from_seeds<Private>(seeds).public_key().bytes().size(); });
        } else if (kind == "sign") {
            measure(what.c_str(), [&] { return key.sign(msg).size(); });
        } else if (kind == "verify") {
            auto pub = key.public_key();
            auto sig = key.sign(msg);
            measure(what.c_str(), [&] { return uint64_t(pub.verify(msg, sig)); });
        } else {
            std::fprintf(stderr, "slhdsa: unknown case %s\n", what.c_str());
            return 2;
        }
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: slhdsa <case> sgcl\n");
        return 2;
    }
    const std::string what = argv[1];
    const auto bar = what.find('_');
    const std::string kind = what.substr(0, bar), set = bar == std::string::npos ? std::string() : what.substr(bar + 1);
    if (set == "sha2_128s") {
        return run<crypto::slhdsa_sha2_128s::private_key>(what, kind);
    }
    if (set == "sha2_128f") {
        return run<crypto::slhdsa_sha2_128f::private_key>(what, kind);
    }
    if (set == "sha2_192f") {
        return run<crypto::slhdsa_sha2_192f::private_key>(what, kind);
    }
    if (set == "sha2_256f") {
        return run<crypto::slhdsa_sha2_256f::private_key>(what, kind);
    }
    if (set == "shake_128s") {
        return run<crypto::slhdsa_shake_128s::private_key>(what, kind);
    }
    if (set == "shake_128f") {
        return run<crypto::slhdsa_shake_128f::private_key>(what, kind);
    }
    std::fprintf(stderr, "slhdsa: unknown case %s\n", what.c_str());
    return 2;
}
