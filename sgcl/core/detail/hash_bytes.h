//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <random>

namespace sgcl::detail {
    // The key of the hash of bytes: four words drawn once per process, so
    // that which texts collide is not known outside the process, and a
    // table keyed by what a client sends (the names of HTTP headers, the
    // keys of a JSON object) cannot be filled with keys of one bucket
    // (HashDoS: Go seeds its maps, Rust its HashMap, the same way). The
    // environment variable SGCL_HASH_SEED, a number, fixes it: the same
    // hashes, and the same order of a hash container, in every run.
    struct HashKey {
        uint64_t k[4];
    };

    inline uint64_t hash_key_step(uint64_t& x) noexcept {
        uint64_t z = (x += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    inline HashKey make_hash_key() noexcept {
        uint64_t seed = 0;
        if (const char* s = std::getenv("SGCL_HASH_SEED"); s && *s) {
            seed = std::strtoull(s, nullptr, 0);
        } else {
            try {
                std::random_device device;
                seed = (uint64_t(device()) << 32) ^ device();
            } catch (...) {   // no source of entropy: the address of the key at least differs between runs
                seed = uint64_t(reinterpret_cast<uintptr_t>(&make_hash_key)) ^ uint64_t(std::rand());
            }
        }
        HashKey key;
        for (auto& k : key.k) {
            k = hash_key_step(seed);
        }
        return key;
    }

    inline const HashKey& hash_key() noexcept {
        static const HashKey key = make_hash_key();
        return key;
    }

    // The product of two words, its halves xored: every bit of both in
    // every bit of the result
    inline uint64_t hash_mix(uint64_t a, uint64_t b) noexcept {
        const __uint128_t p = (__uint128_t)a * b;
        return uint64_t(p) ^ uint64_t(p >> 64);
    }

    inline uint64_t hash_load64(const unsigned char* p) noexcept {
        uint64_t v;
        std::memcpy(&v, p, 8);
        return v;
    }

    inline uint64_t hash_load32(const unsigned char* p) noexcept {
        uint32_t v;
        std::memcpy(&v, p, 4);
        return v;
    }

    // The keyed hash of n bytes: sixteen bytes at a time folded into the
    // state by one multiplication of the bytes, each word xored with a
    // word of the key, three lanes of it past 48 bytes; up to sixteen
    // bytes read as two words that overlap (every byte in one of them, the
    // length mixed in last, so "a" and "a\0" differ). About a nanosecond
    // for a short key, 10 GB/s over a long one.
    inline uint64_t hash_bytes(const void* data, size_t n) noexcept {
        const HashKey& key = hash_key();
        auto p = static_cast<const unsigned char*>(data);
        uint64_t state = key.k[0], a, b;
        if (n <= 16) {
            if (n >= 4) {
                const size_t q = (n >> 3) << 2;   // 0 below eight bytes, 4 from eight
                a = (hash_load32(p) << 32) | hash_load32(p + q);
                b = (hash_load32(p + n - 4) << 32) | hash_load32(p + n - 4 - q);
            } else if (n > 0) {
                a = (uint64_t(p[0]) << 16) | (uint64_t(p[n >> 1]) << 8) | p[n - 1];
                b = 0;
            } else {
                a = b = 0;
            }
        } else {
            size_t i = n;
            if (i > 48) {
                uint64_t s1 = state, s2 = state;
                do {
                    state = hash_mix(hash_load64(p) ^ key.k[1], hash_load64(p + 8) ^ state);
                    s1 = hash_mix(hash_load64(p + 16) ^ key.k[2], hash_load64(p + 24) ^ s1);
                    s2 = hash_mix(hash_load64(p + 32) ^ key.k[3], hash_load64(p + 40) ^ s2);
                    p += 48;
                    i -= 48;
                } while (i > 48);
                state ^= s1 ^ s2;
            }
            while (i > 16) {
                state = hash_mix(hash_load64(p) ^ key.k[1], hash_load64(p + 8) ^ state);
                p += 16;
                i -= 16;
            }
            a = hash_load64(p + i - 16);   // the last sixteen bytes, some of them hashed already
            b = hash_load64(p + i - 8);
        }
        return hash_mix(key.k[1] ^ n, hash_mix(a ^ key.k[1], b ^ state));
    }
}
