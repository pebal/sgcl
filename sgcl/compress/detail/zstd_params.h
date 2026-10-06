//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/os.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>

// The strategies and the parameters of Zstandard's levels (zstd_match.h
// says what each strategy does)
namespace sgcl::compress::detail {
    enum class ZstdStrategy : uint8_t {
        fast,
        dfast,
        greedy,
        lazy,
        lazy2,
        btlazy2,
        btopt,
        btultra,
        btultra2
    };

    struct ZstdParams {
        unsigned window_log;
        unsigned chain_log;
        unsigned hash_log;
        unsigned search_log;
        unsigned min_match;
        unsigned target_length;
        ZstdStrategy strategy;
        unsigned window_trim = 0;   // bytes the window lacks of 2^window_log (brotli's 16)
    };

    // The levels as zstd's command numbers them, for inputs past 256 KB;
    // smaller inputs shrink the window and the tables to the input
    inline ZstdParams zstd_params(int level, uint64_t size, unsigned window_log) noexcept {
        static constexpr ZstdParams table[23] = {
            {19, 12, 13, 1, 6, 1, ZstdStrategy::fast},       // 0: as 1 (negative levels use it with acceleration)
            {19, 12, 13, 1, 6, 1, ZstdStrategy::fast},       // 1
            {20, 15, 16, 1, 6, 0, ZstdStrategy::fast},       // 2
            {21, 16, 17, 1, 5, 0, ZstdStrategy::dfast},      // 3
            {21, 18, 18, 1, 5, 0, ZstdStrategy::dfast},      // 4
            {21, 18, 19, 3, 5, 2, ZstdStrategy::greedy},     // 5
            {21, 18, 19, 3, 5, 4, ZstdStrategy::lazy},       // 6
            {21, 19, 20, 4, 5, 8, ZstdStrategy::lazy},       // 7
            {21, 19, 20, 4, 5, 16, ZstdStrategy::lazy2},     // 8
            {22, 20, 21, 4, 5, 16, ZstdStrategy::lazy2},     // 9
            {22, 21, 22, 5, 5, 16, ZstdStrategy::lazy2},     // 10
            {22, 21, 22, 6, 5, 16, ZstdStrategy::lazy2},     // 11
            {22, 22, 23, 6, 5, 32, ZstdStrategy::lazy2},     // 12
            {22, 22, 22, 4, 5, 32, ZstdStrategy::btlazy2},   // 13
            {22, 22, 23, 5, 5, 32, ZstdStrategy::btlazy2},   // 14
            {22, 23, 23, 6, 5, 32, ZstdStrategy::btlazy2},   // 15
            {22, 22, 22, 5, 5, 48, ZstdStrategy::btopt},     // 16
            {23, 23, 22, 5, 4, 64, ZstdStrategy::btopt},     // 17
            {23, 23, 22, 6, 3, 64, ZstdStrategy::btultra},   // 18
            {23, 24, 22, 7, 3, 256, ZstdStrategy::btultra2}, // 19
            {25, 25, 23, 7, 3, 256, ZstdStrategy::btultra2}, // 20
            {26, 26, 24, 7, 3, 512, ZstdStrategy::btultra2}, // 21
            {27, 27, 25, 9, 3, 999, ZstdStrategy::btultra2}, // 22
        };
        ZstdParams p = table[level < 0 ? 0 : std::min(level, 22)];
        if (level < 0) {
            p.target_length = unsigned(-level);   // the acceleration
        }
        if (window_log) {
            p.window_log = window_log;
        }
        if (size != UINT64_MAX) {
            // a window no larger than the input, and tables no larger than the window
            unsigned need = 10;
            while (need < p.window_log && (uint64_t(1) << need) < size) {
                ++need;
            }
            p.window_log = std::min(p.window_log, need);
        }
        const bool tree = p.strategy >= ZstdStrategy::btlazy2;
        p.hash_log = std::min(p.hash_log, p.window_log + 1);
        p.chain_log = std::min(p.chain_log, p.window_log + (tree ? 0u : 1u));
        return p;
    }

    SGCL_INLINE_HOT uint64_t zstd_read64(const uint8_t* p) noexcept {
        uint64_t v;
        std::memcpy(&v, p, 8);
        if constexpr (std::endian::native == std::endian::big) {
            v = __builtin_bswap64(v);
        }
        return v;
    }

}
