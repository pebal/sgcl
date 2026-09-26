//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The CRCs: the catalogue's check and residue, the two paths against each
// other, combine() and going on from a value.
#include "common.h"

#include <random>

namespace {
    using namespace hash_test;

    // The check (the CRC of "123456789") and the residue (the register after
    // a message followed by its own CRC, little-endian, before the final XOR)
    // of the CRC catalogue (reveng): CRC-32/ISO-HDLC, CRC-32/ISCSI,
    // CRC-64/XZ, CRC-64/GO-ISO
    template<class H, class V>
    void catalogue(V check, V residue) {
        EXPECT_EQ(text(H::of("123456789")), text(check));
        H h;
        h.update("123456789");
        V v = h.value();
        unsigned char le[sizeof(V)];
        for (size_t i = 0; i < sizeof(V); ++i) {
            le[i] = (unsigned char)(v >> (8 * i));
        }
        h.update(bytes(le, sizeof le));
        EXPECT_EQ(text(V(~h.value())), text(residue));
    }

    template<class H>
    class Hash_Crcs : public ::testing::Test {};

    using Crcs = ::testing::Types<hash::crc32, hash::crc32c, hash::crc64, hash::crc64_iso>;
    TYPED_TEST_SUITE(Hash_Crcs, Crcs);
}

TEST(Hash_Crc, Catalogue) {
    catalogue<hash::crc32>(uint32_t(0xCBF43926), uint32_t(0xDEBB20E3));
    catalogue<hash::crc32c>(uint32_t(0xE3069283), uint32_t(0xB798B438));
    catalogue<hash::crc64>(uint64_t(0x995DC9BBDF1939FA), uint64_t(0x49958C9ABD7D353F));
    catalogue<hash::crc64_iso>(uint64_t(0xB90956C775A41001), uint64_t(0x5300000000000000));
    EXPECT_EQ(hash::crc32::of(""), 0u);
    EXPECT_EQ(hash::crc64::of(""), 0u);
}

// The arm64 path (folding, the CRC-32 instructions) against slicing by
// eight, called side by side in one program: every length 0…2100 at every
// offset 0…15, random lengths to 64 KB, and a start register that is not
// the initial one. The build with SGCL_HASH_PORTABLE runs the whole program
// on the portable path as well; this test is where the two meet.
TEST(Hash_Crc, TheTwoPathsAgree) {
#if defined(SGCL_HASH_ARM64)
    using namespace sgcl::hash::detail;
    auto data = pattern(0, 70000);
    std::mt19937_64 rng(11);
    auto both = [&](const unsigned char* p, size_t n, uint64_t start) {
        uint32_t s32 = uint32_t(start);
        ASSERT_EQ(text(crc32_update_arm64<0xEDB88320u>(s32, p, n)), text(crc_update_portable<uint32_t, 0xEDB88320u>(s32, p, n))) << n;
        ASSERT_EQ(text(crc32_update_arm64<0x82F63B78u>(s32, p, n)), text(crc_update_portable<uint32_t, 0x82F63B78u>(s32, p, n))) << n;
        ASSERT_EQ(text(crc64_update_arm64<0xC96C5795D7870F42ull>(start, p, n)), text(crc_update_portable<uint64_t, 0xC96C5795D7870F42ull>(start, p, n))) << n;
        ASSERT_EQ(text(crc64_update_arm64<0xD800000000000000ull>(start, p, n)), text(crc_update_portable<uint64_t, 0xD800000000000000ull>(start, p, n))) << n;
    };
    for (size_t offset = 0; offset < 16; ++offset) {
        for (size_t n = 0; n <= 2100; ++n) {
            both(data.data() + offset, n, rng());
            if (::testing::Test::HasFatalFailure()) {
                return;
            }
        }
    }
    for (int round = 0; round < 500; ++round) {
        size_t n = rng() % 65536;
        size_t offset = rng() % 16;
        both(data.data() + offset, n, rng());
        if (::testing::Test::HasFatalFailure()) {
            return;
        }
    }
#else
    GTEST_SKIP() << "the portable path only";
#endif
}

// combine(of(A), of(B), |B|) is of(A‖B): random pieces of random data, an
// empty A, an empty B
TYPED_TEST(Hash_Crcs, CombineJoinsTwoPieces) {
    using H = TypeParam;
    auto data = pattern(0, 20000);
    std::mt19937 rng(3);
    for (int round = 0; round < 3000; ++round) {
        size_t n = rng() % 20001;
        size_t cut = round % 3 == 0 ? (round % 2 ? 0 : n) : rng() % (n + 1);
        auto a = H::of(bytes(data.data(), cut));
        auto b = H::of(bytes(data.data() + cut, n - cut));
        ASSERT_EQ(text(H::combine(a, b, n - cut)), of<H>(data.data(), n)) << "n " << n << " cut " << cut;
    }
}

// Past what a test can hash: moving a CRC n bytes and then m is moving it
// n + m (combine with a zero second CRC is the move alone), up to lengths
// near 2^64
TYPED_TEST(Hash_Crcs, CombineOverHugeLengths) {
    using H = TypeParam;
    std::mt19937_64 rng(17);
    for (int round = 0; round < 500; ++round) {
        auto v = decltype(H().value())(rng());
        uint64_t n = rng() >> (rng() % 64);
        uint64_t m = rng() >> (rng() % 64);
        if (n + m < n) {
            m = ~n;   // n + m at the very top
        }
        ASSERT_EQ(text(H::combine(H::combine(v, 0, n), 0, m)), text(H::combine(v, 0, n + m))) << n << " + " << m;
    }
    // combine is constexpr
    constexpr auto c = H::combine(1, 2, 3);
    (void)c;
}

// A CRC saved and a new hasher made from it goes on where the first left off
TYPED_TEST(Hash_Crcs, GoesOnFromAValue) {
    using H = TypeParam;
    auto data = pattern(0, 3000);
    for (size_t cut : {size_t(0), size_t(1), size_t(127), size_t(128), size_t(1500), size_t(3000)}) {
        auto first = H::of(bytes(data.data(), cut));
        H h = H::resume(first);
        h.update(bytes(data.data() + cut, data.size() - cut));
        EXPECT_EQ(text(h.value()), of<H>(data)) << cut;
    }
}
