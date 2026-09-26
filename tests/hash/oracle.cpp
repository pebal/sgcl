//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Every algorithm against Go's hash packages and zlib, through the vectors
// tools/hash_oracle.go wrote (hash_vectors.h, whose header says which cases
// were asked): four patterns of bytes, every length 0…1100 of the random
// one and 0…300 of the others, powers of two ±1 up to a megabyte; Adler-32
// and the CRCs going on from a given value; zlib's combine.
#include "common.h"
#include "hash_vectors.h"

namespace {
    using namespace hash_test;

    // The four patterns at their longest; every row is a prefix of one
    const std::vector<unsigned char>& longest(int kind) {
        static const std::vector<unsigned char> p[4] = {
            pattern(0, (1 << 20) + 1), pattern(1, (1 << 20) + 1), pattern(2, (1 << 20) + 1), pattern(3, (1 << 20) + 1)};
        return p[kind];
    }
}

TEST(Hash_Oracle, EveryAlgorithmAgainstGo) {
    size_t rows = 0;
    for (const auto& r : hash_vectors::rows) {
        const unsigned char* p = longest(r.pattern).data();
        size_t n = r.length;
        SCOPED_TRACE("pattern " + std::to_string(r.pattern) + " length " + std::to_string(n));
        EXPECT_EQ(of<hash::crc32>(p, n), text(r.crc32));
        EXPECT_EQ(of<hash::crc32c>(p, n), text(r.crc32c));
        EXPECT_EQ(of<hash::crc64>(p, n), text(r.crc64));
        EXPECT_EQ(of<hash::crc64_iso>(p, n), text(r.crc64_iso));
        EXPECT_EQ(of<hash::adler32>(p, n), text(r.adler32));
        EXPECT_EQ(of<hash::fnv32>(p, n), text(r.fnv32));
        EXPECT_EQ(of<hash::fnv32a>(p, n), text(r.fnv32a));
        EXPECT_EQ(of<hash::fnv64>(p, n), text(r.fnv64));
        EXPECT_EQ(of<hash::fnv64a>(p, n), text(r.fnv64a));
        EXPECT_EQ(of<hash::fnv128>(p, n), text128(r.fnv128_high, r.fnv128_low));
        EXPECT_EQ(of<hash::fnv128a>(p, n), text128(r.fnv128a_high, r.fnv128a_low));
        ++rows;
        if (::testing::Test::HasFailure()) {
            break;   // one row's failures say enough
        }
    }
    EXPECT_GT(rows, 2000u);
}

// The portable path against the same vectors, whichever path the build
// takes: slicing by eight is what x86 runs today, and what arm64 runs for
// CRC-64 under 128 bytes
TEST(Hash_Oracle, PortableCrcAgainstGo) {
    using namespace sgcl::hash::detail;
    for (const auto& r : hash_vectors::rows) {
        const unsigned char* p = longest(r.pattern).data();
        size_t n = r.length;
        SCOPED_TRACE("pattern " + std::to_string(r.pattern) + " length " + std::to_string(n));
        ASSERT_EQ(text(uint32_t(~crc_update_portable<uint32_t, 0xEDB88320u>(~0u, p, n))), text(r.crc32));
        ASSERT_EQ(text(uint32_t(~crc_update_portable<uint32_t, 0x82F63B78u>(~0u, p, n))), text(r.crc32c));
        ASSERT_EQ(text(uint64_t(~crc_update_portable<uint64_t, 0xC96C5795D7870F42ull>(~0ull, p, n))), text(r.crc64));
        ASSERT_EQ(text(uint64_t(~crc_update_portable<uint64_t, 0xD800000000000000ull>(~0ull, p, n))), text(r.crc64_iso));
    }
}

TEST(Hash_Oracle, AdlerFromTheLargestSums) {
    const unsigned char* ff = longest(2).data();
    for (const auto& r : hash_vectors::adler_from_max) {
        SCOPED_TRACE("length " + std::to_string(r.length));
        hash::adler32 h = hash::adler32::resume(0xFFF0FFF0u);
        h.update(bytes(ff, r.length));
        EXPECT_EQ(text(h.value()), text(r.adler32));
    }
}

TEST(Hash_Oracle, CrcFromAValue) {
    const unsigned char* p = longest(0).data();
    for (const auto& r : hash_vectors::crc_from) {
        SCOPED_TRACE("length " + std::to_string(r.length));
        hash::crc32 a = hash::crc32::resume(r.from32);
        a.update(bytes(p, r.length));
        EXPECT_EQ(text(a.value()), text(r.crc32));
        hash::crc32c b = hash::crc32c::resume(r.from32);
        b.update(bytes(p, r.length));
        EXPECT_EQ(text(b.value()), text(r.crc32c));
        hash::crc64 c = hash::crc64::resume(r.from64);
        c.update(bytes(p, r.length));
        EXPECT_EQ(text(c.value()), text(r.crc64));
        hash::crc64_iso d = hash::crc64_iso::resume(r.from64);
        d.update(bytes(p, r.length));
        EXPECT_EQ(text(d.value()), text(r.crc64_iso));
    }
}

TEST(Hash_Oracle, CombineAgainstZlib) {
    for (const auto& r : hash_vectors::combine) {
        SCOPED_TRACE("length " + std::to_string(r.second_length));
        EXPECT_EQ(text(hash::crc32::combine(r.first, r.second, r.second_length)), text(r.crc32));
        EXPECT_EQ(text(hash::adler32::combine(r.adler_first, r.adler_second, r.second_length)), text(r.adler32));
    }
}

// combine of all four CRCs against the oracle's own method, matrices over
// GF(2), at lengths up to 2^64 − 1 (zlib's length is signed and stops at
// 2^63 − 1, and has no CRC-32C or CRC-64 at all)
TEST(Hash_Oracle, CombineAgainstMatricesOverGf2) {
    size_t rows = 0;
    for (const auto& r : hash_vectors::combine_all) {
        SCOPED_TRACE("length " + std::to_string(r.second_length));
        EXPECT_EQ(text(hash::crc32::combine(uint32_t(r.first), uint32_t(r.second), r.second_length)), text(r.crc32));
        EXPECT_EQ(text(hash::crc32c::combine(uint32_t(r.first), uint32_t(r.second), r.second_length)), text(r.crc32c));
        EXPECT_EQ(text(hash::crc64::combine(r.first, r.second, r.second_length)), text(r.crc64));
        EXPECT_EQ(text(hash::crc64_iso::combine(r.first, r.second, r.second_length)), text(r.crc64_iso));
        ++rows;
    }
    EXPECT_GT(rows, 200u);
}
