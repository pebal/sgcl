//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Adler-32 and the six FNVs: the published values, Adler-32's combine and
// its sums at their largest.
#include "common.h"

#include <random>

namespace {
    using namespace hash_test;

    // The sums of Adler-32 the long way, a byte at a time with the modulo
    // after each: the definition of RFC 1950, section 8.2, as it reads
    uint32_t adler_by_definition(uint32_t start, const unsigned char* p, size_t n) {
        uint64_t a = start & 0xffff;
        uint64_t b = start >> 16;
        for (size_t i = 0; i < n; ++i) {
            a = (a + p[i]) % 65521;
            b = (b + a) % 65521;
        }
        return uint32_t(b << 16 | a);
    }
}

// Values published with the algorithm: the empty string is 1 (a starts at
// one, b at zero); "Wikipedia" is the example of the Adler-32 article;
// "abc" and "message digest" are the values zlib's and Go's tests carry
TEST(Hash_Adler, KnownValues) {
    EXPECT_EQ(hash::adler32::of(""), 0x00000001u);
    EXPECT_EQ(hash::adler32::of("a"), 0x00620062u);
    EXPECT_EQ(hash::adler32::of("abc"), 0x024d0127u);
    EXPECT_EQ(hash::adler32::of("message digest"), 0x29750586u);
    EXPECT_EQ(hash::adler32::of("Wikipedia"), 0x11e60398u);
}

// The block loop against the definition, including from the largest sums
// and over the largest bytes, where a run of 5536 bytes is closest to
// overflowing 32 bits
TEST(Hash_Adler, BlocksAgainstTheDefinition) {
    std::mt19937 rng(9);
    for (int kind : {0, 2, 3}) {
        auto data = pattern(kind, 40000);
        for (int round = 0; round < 300; ++round) {
            size_t n = rng() % data.size();
            uint32_t start = round % 2 ? 0xFFF0FFF0u : ((rng() % 65521) << 16 | (rng() % 65521));
            hash::adler32 h = hash::adler32::resume(start);
            h.update(bytes(data.data(), n));
            ASSERT_EQ(text(h.value()), text(adler_by_definition(start, data.data(), n))) << "pattern " << kind << " n " << n;
        }
    }
}

// A value whose halves are 65521 or more is no checksum; the constructor
// takes it modulo, so that the loop's bound on the sums holds
TEST(Hash_Adler, ValueTakenModulo) {
    hash::adler32 h = hash::adler32::resume(0xFFFFFFFFu);
    EXPECT_EQ(h.value(), 0x000E000Eu);   // 65535 mod 65521 = 14, both halves
    auto ff = pattern(2, 100000);
    h.update(bytes(ff));
    EXPECT_EQ(text(h.value()), text(adler_by_definition(0x000E000Eu, ff.data(), ff.size())));
}

TEST(Hash_Adler, CombineJoinsTwoPieces) {
    auto data = pattern(0, 20000);
    std::mt19937 rng(4);
    for (int round = 0; round < 3000; ++round) {
        size_t n = rng() % 20001;
        size_t cut = round % 3 == 0 ? (round % 2 ? 0 : n) : rng() % (n + 1);
        auto a = hash::adler32::of(bytes(data.data(), cut));
        auto b = hash::adler32::of(bytes(data.data() + cut, n - cut));
        ASSERT_EQ(text(hash::adler32::combine(a, b, n - cut)), of<hash::adler32>(data.data(), n)) << "n " << n << " cut " << cut;
    }
    constexpr auto c = hash::adler32::combine(1, 1, 0);
    static_assert(c == 1);
}

TEST(Hash_Adler, GoesOnFromAValue) {
    auto data = pattern(0, 3000);
    for (size_t cut : {size_t(0), size_t(1), size_t(31), size_t(32), size_t(1500), size_t(3000)}) {
        hash::adler32 h = hash::adler32::resume(hash::adler32::of(bytes(data.data(), cut)));
        h.update(bytes(data.data() + cut, data.size() - cut));
        EXPECT_EQ(text(h.value()), of<hash::adler32>(data)) << cut;
    }
}

// The test vectors of draft-eastlake-fnv (also on Landon Curt Noll's FNV
// page): "", "a" and "foobar" for FNV-1 and FNV-1a in 32 and 64 bits. The
// empty input is the offset basis
TEST(Hash_Fnv, DraftVectors) {
    EXPECT_EQ(hash::fnv32::of(""), 0x811c9dc5u);
    EXPECT_EQ(hash::fnv32::of("a"), 0x050c5d7eu);
    EXPECT_EQ(hash::fnv32::of("foobar"), 0x31f0b262u);
    EXPECT_EQ(hash::fnv32a::of(""), 0x811c9dc5u);
    EXPECT_EQ(hash::fnv32a::of("a"), 0xe40c292cu);
    EXPECT_EQ(hash::fnv32a::of("foobar"), 0xbf9cf968u);
    EXPECT_EQ(hash::fnv64::of(""), 0xcbf29ce484222325ull);
    EXPECT_EQ(hash::fnv64::of("a"), 0xaf63bd4c8601b7beull);
    EXPECT_EQ(hash::fnv64::of("foobar"), 0x340d8765a4dda9c2ull);
    EXPECT_EQ(hash::fnv64a::of(""), 0xcbf29ce484222325ull);
    EXPECT_EQ(hash::fnv64a::of("a"), 0xaf63dc4c8601ec8cull);
    EXPECT_EQ(hash::fnv64a::of("foobar"), 0x85944171f73967e8ull);
}

// 128 bits: the empty input is the draft's offset basis; "a" is written out
// as a fixed point for a reader (the oracle checks the rest)
TEST(Hash_Fnv, Wide) {
    EXPECT_EQ(text(hash::fnv128::of("")), "6c62272e07bb014262b821756295c58d");
    EXPECT_EQ(text(hash::fnv128a::of("")), "6c62272e07bb014262b821756295c58d");
    EXPECT_EQ(text(hash::fnv128::of("a")), "d228cb69101a8caf78912b704e4a141e");
    EXPECT_EQ(text(hash::fnv128a::of("a")), "d228cb696f1a8caf78912b704e4a8964");
}

// The state of an FNV is its value: a hasher made from the value after A
// goes on as if it had seen A
namespace {
    template<class H>
    void fnv_goes_on() {
        auto data = pattern(0, 500);
        for (size_t cut : {size_t(0), size_t(1), size_t(250), size_t(500)}) {
            H h = H::resume(H::of(bytes(data.data(), cut)));
            h.update(bytes(data.data() + cut, data.size() - cut));
            EXPECT_EQ(text(h.value()), of<H>(data)) << cut;
        }
    }
}

TEST(Hash_Fnv, GoesOnFromAValue) {
    fnv_goes_on<hash::fnv32>();
    fnv_goes_on<hash::fnv32a>();
    fnv_goes_on<hash::fnv64>();
    fnv_goes_on<hash::fnv64a>();
    fnv_goes_on<hash::fnv128>();
    fnv_goes_on<hash::fnv128a>();
}
