//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The JPEG decoder's shortcut for a block of its DC alone (idct_dc_only in
// sgcl/codec/detail/jpeg_idct.h) held against the full integer IDCT
// (idct_islow_plain) for every pair of a 16-bit DC and a 16-bit quantizer:
// 2^32 blocks, every one of their 64 samples. Run once for the change that
// brought the shortcut, and again whenever either function changes:
//
//   c++ -std=c++20 -O2 -I. tools/jpeg_dc_only_proof.cpp -o /tmp/proof && /tmp/proof
//
// Prints the pairs held and the first difference, if any; exit status 1 on one.
#include "sgcl/codec/detail/jpeg_idct.h"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

using namespace sgcl::codec::detail;

int main() {
    const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    std::atomic<int> next_quant{0};
    std::atomic<bool> failed{false};
    std::atomic<uint64_t> held{0};
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < threads; ++t) {
        pool.emplace_back([&] {
            int16_t coef[64] = {};
            uint16_t quant[64];
            uint8_t out[64];
            for (int q; (q = next_quant.fetch_add(1)) <= 65535 && !failed;) {
                for (auto& x : quant) {
                    x = uint16_t(q);
                }
                uint64_t n = 0;
                for (int dc = -32768; dc <= 32767; ++dc) {
                    coef[0] = int16_t(dc);
                    idct_islow_plain(coef, quant, out, 8);
                    const uint8_t v = idct_dc_only(int16_t(dc), uint16_t(q));
                    for (int i = 0; i < 64; ++i) {
                        if (out[i] != v) {
                            if (!failed.exchange(true)) {
                                std::printf("differs: dc %d quant %d sample %d: plain %d, shortcut %d\n", dc, q, i, out[i], v);
                            }
                            return;
                        }
                    }
                    ++n;
                }
                held += n;
            }
        });
    }
    for (auto& t : pool) {
        t.join();
    }
    std::printf("%llu of %llu pairs held\n", (unsigned long long)held.load(), 65536ull * 65536ull);
    return failed ? 1 : 0;
}
