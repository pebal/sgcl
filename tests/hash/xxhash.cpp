//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// XXH32 and XXH64: the vectors of libxxhash (tools/xxhash_oracle.c), in one
// call and in pieces of every size about a stripe; the seed at its limits
// (XXH32 takes 32 bits, XXH64 64); the thresholds of the algorithm (a
// stripe, a word, a byte after them); and, when the build found libxxhash,
// the library asked directly over random lengths, seeds and splits.
#include "common.h"
#include "xxhash_vectors.h"

#if SGCL_TEST_XXHASH
#include <xxhash.h>
#endif

#include <limits>
#include <random>

namespace {
    using namespace hash_test;

    template<class H, class Seed>
    auto in_pieces(Seed seed, const unsigned char* p, size_t n, size_t piece) {
        H h(seed);
        for (size_t at = 0; at < n; at += piece) {
            h.update(bytes(p + at, std::min(piece, n - at)));
        }
        return h.value();
    }
}

TEST(Hash_Xxhash, ReferenceVectors) {
    size_t rows = 0;
    for (const auto& r : xxhash_vectors::rows) {
        auto data = pattern(r.pattern, r.length);
        const uint64_t seed = xxhash_vectors::seeds[r.seed];
        const auto* p = data.data();
        const size_t n = data.size();
        EXPECT_EQ(text(hash::xxh32::of(bytes(p, n), uint32_t(seed))), text(r.xxh32)) << r.pattern << " " << n << " " << seed;
        EXPECT_EQ(text(hash::xxh64::of(bytes(p, n), seed)), text(r.xxh64)) << r.pattern << " " << n << " " << seed;
        for (size_t piece : {1, 3, 15, 16, 17, 31, 32, 33, 4096}) {
            if (piece > n && piece != 1) {
                continue;
            }
            EXPECT_EQ(text(in_pieces<hash::xxh32>(uint32_t(seed), p, n, piece)), text(r.xxh32)) << n << " pieces of " << piece;
            EXPECT_EQ(text(in_pieces<hash::xxh64>(seed, p, n, piece)), text(r.xxh64)) << n << " pieces of " << piece;
        }
        if (seed == 0) {
            EXPECT_EQ(hash::xxh32::of(bytes(p, n)), r.xxh32);
            EXPECT_EQ(hash::xxh64::of(bytes(p, n)), r.xxh64);
            hash::xxh32 a;
            hash::xxh64 b;
            a.update(bytes(p, n));
            b.update(bytes(p, n));
            EXPECT_EQ(a.value(), r.xxh32);
            EXPECT_EQ(b.value(), r.xxh64);
        }
        ++rows;
    }
    EXPECT_GT(rows, 600u);
}

// The values every implementation prints for the empty input and a word,
// as xxhsum -H0 and -H1 write them
TEST(Hash_Xxhash, KnownValues) {
    EXPECT_EQ(hash::xxh32::of(""), 0x02cc5d05u);
    EXPECT_EQ(hash::xxh64::of(""), 0xef46db3751d8e999ull);
    EXPECT_EQ(hash::xxh32::of("abc"), 0x32d153ffu);
    EXPECT_EQ(hash::xxh64::of("abc"), 0x44bc2cf5ad770999ull);
}

TEST(Hash_Xxhash, DigestIsBigEndian) {
    hash::xxh32 a;
    hash::xxh64 b;
    a.update("abc");
    b.update("abc");
    EXPECT_EQ(text(a.digest()), "32d153ff");
    EXPECT_EQ(text(b.digest()), "44bc2cf5ad770999");
    static_assert(hash::xxh32::digest_size == 4 && hash::xxh32::block_size == 16);
    static_assert(hash::xxh64::digest_size == 8 && hash::xxh64::block_size == 32);
    static_assert(sizeof(hash::xxh32) == 48 && sizeof(hash::xxh64) == 88);
}

// The seed at its limits: 0 is the hasher made without one; the largest
// hashes in pieces as in one call at every length about the thresholds;
// XXH32's seed is 32 bits, so a seed's high half is not part of it
TEST(Hash_Xxhash, SeedsAtTheirLimits) {
    auto data = pattern(0, 200);
    for (size_t n : {0, 1, 3, 4, 7, 8, 15, 16, 17, 19, 20, 31, 32, 33, 35, 36, 39, 40, 63, 64, 65, 200}) {
        auto in = bytes(data.data(), n);
        EXPECT_EQ(hash::xxh32::of(in, 0), hash::xxh32::of(in)) << n;
        EXPECT_EQ(hash::xxh64::of(in, 0), hash::xxh64::of(in)) << n;
        for (uint32_t seed : {1u, std::numeric_limits<uint32_t>::max()}) {
            EXPECT_EQ(in_pieces<hash::xxh32>(seed, data.data(), n, 5), hash::xxh32::of(in, seed)) << n;
            EXPECT_NE(hash::xxh32::of(in, seed), hash::xxh32::of(in)) << n;
        }
        for (uint64_t seed : {uint64_t(1), std::numeric_limits<uint64_t>::max()}) {
            EXPECT_EQ(in_pieces<hash::xxh64>(seed, data.data(), n, 5), hash::xxh64::of(in, seed)) << n;
            EXPECT_NE(hash::xxh64::of(in, seed), hash::xxh64::of(in)) << n;
        }
    }
}

// value() ends nothing; a copy is a branch; reset() keeps the seed; an
// empty update changes nothing, before and after a stripe
TEST(Hash_Xxhash, ValueCopyReset) {
    auto data = pattern(3, 100);
    hash::xxh64 h(7);
    hash::xxh32 g(7);
    h.update(bytes(data.data(), 40));
    g.update(bytes(data.data(), 40));
    auto mid64 = h.value();
    auto mid32 = g.value();
    EXPECT_EQ(mid64, hash::xxh64::of(bytes(data.data(), 40), 7));
    EXPECT_EQ(mid32, hash::xxh32::of(bytes(data.data(), 40), 7));
    hash::xxh64 branch = h;
    hash::xxh32 branch32 = g;
    h.update(bytes(data.data() + 40, 60));
    g.update(bytes(data.data() + 40, 60));
    h.update(bytes(data.data(), 0));
    g.update(bytes(data.data(), 0));
    EXPECT_EQ(h.value(), hash::xxh64::of(bytes(data), 7));
    EXPECT_EQ(g.value(), hash::xxh32::of(bytes(data), 7));
    EXPECT_EQ(branch.value(), mid64);
    EXPECT_EQ(branch32.value(), mid32);
    h.reset();
    g.reset();
    EXPECT_EQ(h.value(), hash::xxh64::of("", 7));
    EXPECT_EQ(g.value(), hash::xxh32::of("", 7));
    hash::xxh64& same = h;
    h = same;   // onto itself
    EXPECT_EQ(h.value(), hash::xxh64::of("", 7));
}

#if SGCL_TEST_XXHASH
// libxxhash asked directly: random lengths up to 70 KB, random seeds,
// random splits into up to five pieces, at random offsets of the buffer
TEST(Hash_Xxhash, AgainstLibxxhash) {
    std::mt19937_64 rng(20261005);
    auto data = pattern(0, 80000);
    for (int round = 0; round < 3000; ++round) {
        size_t n = round < 1000 ? size_t(rng() % 300) : size_t(rng() % 70000);
        size_t at = size_t(rng() % 64);
        uint64_t seed = rng();
        const unsigned char* p = data.data() + at;
        const uint32_t want32 = XXH32(p, n, uint32_t(seed));
        const uint64_t want64 = XXH64(p, n, seed);
        ASSERT_EQ(hash::xxh32::of(bytes(p, n), uint32_t(seed)), want32) << n;
        ASSERT_EQ(hash::xxh64::of(bytes(p, n), seed), want64) << n;
        hash::xxh32 a{uint32_t(seed)};
        hash::xxh64 b(seed);
        size_t i = 0;
        for (int k = 0; k < 5 && i < n; ++k) {
            size_t take = k == 4 ? n - i : std::min(n - i, size_t(rng() % (n + 1)));
            a.update(bytes(p + i, take));
            b.update(bytes(p + i, take));
            i += take;
        }
        a.update(bytes(p + i, n - i));
        b.update(bytes(p + i, n - i));
        ASSERT_EQ(a.value(), want32) << n;
        ASSERT_EQ(b.value(), want64) << n;
    }
}
#endif
