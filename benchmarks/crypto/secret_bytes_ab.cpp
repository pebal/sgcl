//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What giving secrets as crypto::secret_bytes costs (the secrets change,
// S1–S4), under every worker of the machine at once: each thread in a loop
// of one operation that returns a secret, its bytes summed so that nothing
// is cut away.
//
//   hkdf32   hkdf<sha256>::derive(salt, ikm, info, 32): 32 B, inline in a secret_bytes
//   hkdf1k   the same, 1024 B: a block of the wiping allocator (malloc, secure_zero at the end)
//   pkcs8    x25519::private_key::to_pkcs8_der(): 48 B, inline
//
// The source compiles against the tree before the change too (the
// functions then returned a managed vector<byte>), with `auto` and
// as_slice(): the A column is this program built against those headers,
// the B column against today's, both by the same command:
//
//   clang++ -std=c++20 -O3 -DNDEBUG -I<tree> benchmarks/crypto/secret_bytes_ab.cpp -o ab
//   ./ab <case> [threads] [seconds]
//
// It prints one line: "case T threads: X ns/op a thread, Y Mop/s in all, C
// cpu s" (the process's CPU time: the collector's work shows there).
#include "benchmarks/common.h"
#include "sgcl/crypto/hkdf.h"
#include "sgcl/crypto/sha256.h"
#include "sgcl/crypto/x25519.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl;

namespace {
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> sink{0};

    template<class R>
    uint64_t sum_of(const R& r) {
        auto s = r.as_slice();
        uint64_t v = 0;
        for (size_t i = 0; i < s.size(); i += 8) {
            v += uint64_t(s.data()[i]);
        }
        return v + s.size();
    }

    uint64_t run(const std::string& what, uint64_t& ops) {
        static const uint8_t salt_bytes[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        static const uint8_t ikm_bytes[32] = {7};
        static const uint8_t info_bytes[8] = {'s', 'g', 'c', 'l', ' ', 'a', 'b', 0};
        const slice<const byte> salt(reinterpret_cast<const byte*>(salt_bytes), sizeof salt_bytes);
        const slice<const byte> ikm(reinterpret_cast<const byte*>(ikm_bytes), sizeof ikm_bytes);
        const slice<const byte> info(reinterpret_cast<const byte*>(info_bytes), sizeof info_bytes);
        uint64_t v = 0;
        if (what == "pkcs8") {
            auto key = crypto::x25519::private_key::generate();
            while (!stop.load(std::memory_order_relaxed)) {
                for (int i = 0; i < 64; ++i) {
                    auto der = key.to_pkcs8_der();
                    v += sum_of(der);
                }
                ops += 64;
            }
        } else {
            const size_t n = what == "hkdf32" ? 32 : 1024;
            while (!stop.load(std::memory_order_relaxed)) {
                for (int i = 0; i < 16; ++i) {
                    auto okm = crypto::hkdf<crypto::sha256>::derive(salt, ikm, info, n);
                    v += sum_of(okm);
                }
                ops += 16;
            }
        }
        return v;
    }
}

int main(int argc, char** argv) {
    if (argc < 2 || !bench::has_variant(argv[1], {"hkdf32", "hkdf1k", "pkcs8"})) {
        std::fprintf(stderr, "usage: %s hkdf32|hkdf1k|pkcs8 [threads] [seconds]\n", argv[0]);
        return 2;
    }
    const std::string what = argv[1];
    const unsigned threads = argc > 2 ? unsigned(std::atoi(argv[2])) : bench::hardware_threads();
    const double seconds = argc > 3 ? std::atof(argv[3]) : 2.0;
    std::vector<uint64_t> ops(threads, 0);
    std::vector<std::thread> pool;
    const double cpu0 = bench::cpu_seconds();
    const auto t0 = bench::Clock::now();
    for (unsigned t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] { sink += run(what, ops[t]); });
    }
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    stop = true;
    for (auto& th : pool) {
        th.join();
    }
    const double wall = bench::seconds_since(t0);
    const double cpu = bench::cpu_seconds() - cpu0;
    uint64_t total = 0;
    for (auto o : ops) {
        total += o;
    }
    const double ns_per_op = wall * 1e9 * threads / double(total);
    std::printf("%s %u threads: %.1f ns/op a thread, %.2f Mop/s in all, %.2f cpu s (sink %llu)\n", what.c_str(), threads, ns_per_op,
                double(total) / wall / 1e6, cpu, (unsigned long long)(sink.load() & 0xff));
    return 0;
}
