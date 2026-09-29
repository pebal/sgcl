//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The codec's vector kernels (simd.h: NEON, SSE2) against their plain twins
// in one program, on random inputs over the whole range each takes: the
// same bytes, bit for bit. tests_codec_portable runs the files on the plain
// roads alone; this is where the two meet.
#include <gtest/gtest.h>

#include "sgcl/codec/detail/jpeg_color.h"
#include "sgcl/codec/detail/jpeg_fdct.h"
#include "sgcl/codec/detail/jpeg_idct.h"
#include "sgcl/codec/detail/png_filter.h"
#include "sgcl/codec/detail/vp8_decoder.h"
#include "sgcl/codec/detail/vp8_yuv.h"
#include "sgcl/codec/detail/vp8l_simd.h"

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

using namespace sgcl::codec::detail;

namespace {
    // A block of coefficients: `bound` their largest magnitude, `density`
    // the chance of a nonzero one after the DC
    void random_block(std::mt19937_64& rng, int16_t* coef, int bound, double density) {
        std::uniform_int_distribution<int> value(-bound, bound);
        std::bernoulli_distribution nonzero(density);
        for (int i = 0; i < 64; ++i) {
            coef[i] = int16_t(i == 0 || nonzero(rng) ? value(rng) : 0);
        }
    }

    void random_quant(std::mt19937_64& rng, uint16_t* quant, int bound) {
        std::uniform_int_distribution<int> value(1, bound);
        for (int i = 0; i < 64; ++i) {
            quant[i] = uint16_t(value(rng));
        }
    }
}

TEST(CodecSimd_Tests, IdctAgainstThePlainRoad) {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    std::mt19937_64 rng(41);
    // (coefficient bound, quantizer bound, density): the blocks of real
    // files, then larger and larger ones, up to every 16-bit coefficient
    // with 16-bit quantizers, where most leave the vector road
    const struct {
        int coef, quant;
        double density;
        int rounds;
    } sets[] = {
        {64, 64, 0.2, 20000},     {256, 16, 0.5, 20000},  {1023, 8, 0.3, 20000}, {2047, 1, 1.0, 20000},
        {1023, 255, 0.1, 20000},  {4000, 8, 0.8, 20000},  {32767, 1, 1.0, 20000}, {32767, 255, 0.5, 20000},
        {32767, 65535, 0.5, 20000}, {200, 30000, 0.3, 20000}, {0, 1, 0.0, 2000},
    };
    size_t vector_blocks = 0, all = 0;
    for (const auto& set : sets) {
        const size_t before = vector_blocks;
        for (int round = 0; round < set.rounds; ++round) {
            int16_t coef[64];
            uint16_t quant[64];
            random_block(rng, coef, set.coef, set.density);
            random_quant(rng, quant, set.quant);
            uint8_t a[8 * 11], b[8 * 11];
            std::memset(a, 0xAA, sizeof a);
            std::memset(b, 0xAA, sizeof b);
            idct_islow_plain(coef, quant, a, 11);
            const bool vector = idct_islow_vector(coef, quant, b, 11);
            ++all;
            if (!vector) {
                // nothing written: the plain road takes the block
                uint8_t untouched[8 * 11];
                std::memset(untouched, 0xAA, sizeof untouched);
                ASSERT_EQ(std::memcmp(b, untouched, sizeof b), 0);
                continue;
            }
            ++vector_blocks;
            ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << "coef bound " << set.coef << " quant bound " << set.quant << " round " << round;
        }
        std::printf("[ idct     ] coef %d quant %d density %.1f: %zu of %d on the vector road\n", set.coef, set.quant, set.density, vector_blocks - before, set.rounds);
    }
    // (random dense blocks of large coefficients leave 16 bits in the first
    // pass and go the plain way; the blocks of real files keep to 16 bits)
    EXPECT_GT(vector_blocks, 0u);
    std::printf("[ idct     ] %zu of %zu blocks on the vector road\n", vector_blocks, all);
    // the samples' wrapping past [-512, 511] on the vector road: DC terms
    // whose results pass 511 with 16-bit numbers throughout
    for (int dc = -32768; dc <= 32767; dc += 7) {
        int16_t coef[64] = {};
        uint16_t quant[64];
        for (auto& q : quant) {
            q = 1;
        }
        coef[0] = int16_t(dc);
        uint8_t a[64], b[64];
        idct_islow_plain(coef, quant, a, 8);
        if (idct_islow_vector(coef, quant, b, 8)) {
            ASSERT_EQ(std::memcmp(a, b, 64), 0) << dc;
        }
    }
#else
    GTEST_SKIP() << "the plain road only";
#endif
}

// YCbCr to RGB: every Y, Cb and Cr, the vector road's pixels the tables'
TEST(CodecSimd_Tests, YccToRgbAgainstThePlainRoad) {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    uint8_t y[256], cb[256], cr[256], a[3 * 256 + 4], b[3 * 256 + 4];
    for (int i = 0; i < 256; ++i) {
        y[i] = uint8_t(i);
    }
    for (int u = 0; u < 256; ++u) {
        for (int v = 0; v < 256; ++v) {
            std::memset(cb, u, sizeof cb);
            std::memset(cr, v, sizeof cr);
            ycc_to_rgb_plain(y, cb, cr, a, 256);
            const size_t done = ycc_to_rgb_vector(y, cb, cr, b, 256);
            ASSERT_GE(done, 240u);
            ASSERT_EQ(std::memcmp(a, b, 3 * done), 0) << u << " " << v;
        }
    }
    // every length, random samples, through the whole function
    std::mt19937_64 rng(42);
    for (size_t n = 0; n <= 70; ++n) {
        for (int round = 0; round < 50; ++round) {
            for (size_t i = 0; i < n; ++i) {
                y[i] = uint8_t(rng());
                cb[i] = uint8_t(rng());
                cr[i] = uint8_t(rng());
            }
            std::memset(a, 0x5A, sizeof a);
            std::memset(b, 0x5A, sizeof b);
            ycc_to_rgb_plain(y, cb, cr, a, n);
            ycc_to_rgb(y, cb, cr, b, n);
            ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << n;   // nothing written past 3n either
        }
    }
#else
    GTEST_SKIP() << "the plain road only";
#endif
}

// PNG's filters undone: every filter, every pixel size, random rows (and
// rows of extremes, where Paeth's ties fall), the vector road's rows the
// plain one's
TEST(CodecSimd_Tests, UnfilterAgainstThePlainRoad) {
    std::mt19937_64 rng(43);
    for (unsigned bpp : {1u, 2u, 3u, 4u, 6u, 8u}) {
        for (uint8_t type = 0; type <= 4; ++type) {
            for (size_t pixels : {size_t(1), size_t(2), size_t(3), size_t(7), size_t(64), size_t(333)}) {
                const size_t n = pixels * bpp;
                for (int round = 0; round < 40; ++round) {
                    std::vector<uint8_t> raw(n), prior(n), a(n + 8, 0x5A), b(n + 8, 0x5A);
                    for (size_t i = 0; i < n; ++i) {
                        // a third of the rounds from {0, 1, 127, 128, 254, 255}: ties and edges
                        static const uint8_t edges[] = {0, 1, 127, 128, 254, 255};
                        raw[i] = round % 3 == 0 ? edges[rng() % 6] : uint8_t(rng());
                        prior[i] = round % 3 == 0 ? edges[rng() % 6] : uint8_t(rng());
                    }
                    unfilter_plain(type, raw.data(), prior.data(), a.data(), n, bpp);
                    unfilter(type, raw.data(), prior.data(), b.data(), n, bpp);
                    ASSERT_EQ(a, b) << "filter " << int(type) << " bpp " << bpp << " pixels " << pixels;
                }
            }
        }
    }
}

// The forward DCT: random blocks, blocks of extremes (0 and 255, where
// the sums are largest) and flat ones, two strides; the vector road's
// coefficients the plain one's
TEST(CodecSimd_Tests, FdctAgainstThePlainRoad) {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    std::mt19937_64 rng(44);
    for (size_t stride : {size_t(8), size_t(13)}) {
        for (int round = 0; round < 100000; ++round) {
            uint8_t in[8 * 13];
            const int kind = round % 5;
            const uint8_t flat = uint8_t(rng());
            for (auto& s : in) {
                s = kind == 0 ? uint8_t(rng() & 1 ? 255 : 0) : kind == 1 ? flat : kind == 2 ? uint8_t(128 + int(rng() % 9) - 4) : uint8_t(rng());
            }
            int32_t a[64], b[64];
            fdct_islow_plain(in, stride, a);
            fdct_islow_vector(in, stride, b);
            ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << "stride " << stride << " round " << round;
        }
    }
    // the corners: every block of 0 and 255 whose rows are one pattern
    // (the first pass's extremes) or whose columns are
    for (int pattern = 0; pattern < 256; ++pattern) {
        for (int transposed = 0; transposed < 2; ++transposed) {
            uint8_t in[64];
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    in[8 * y + x] = (pattern >> (transposed ? y : x)) & 1 ? 255 : 0;
                }
            }
            int32_t a[64], b[64];
            fdct_islow_plain(in, 8, a);
            fdct_islow_vector(in, 8, b);
            ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << pattern;
        }
    }
#else
    GTEST_SKIP() << "the plain road only";
#endif
}

// The quantizer's reciprocals: for every q of a baseline table (1..255)
// and every v = |x| + 4q under 2^15, ((v·m) >> 16)·scale >> 16 is v / 8q.
// The arithmetic both roads' instructions do, tried one value at a time,
// on any build
TEST(CodecSimd_Tests, QuantStepsEqualDivision) {
    for (uint16_t q = 1; q <= 255; ++q) {
        uint16_t table[64];
        for (auto& t : table) {
            t = q;
        }
        const QuantSteps steps(table);
        ASSERT_TRUE(steps.reciprocal) << q;
        ASSERT_EQ(steps.half[0], 4 * q);
        for (uint32_t v = 0; v < 32768; ++v) {
            const uint32_t p = (v * steps.m[0]) >> 16;
            ASSERT_EQ((p * steps.scale[0]) >> 16, v / (8u * q)) << "q " << q << " v " << v;
        }
    }
    // a table with a step outside 1..255 keeps the division
    uint16_t table[64];
    for (auto& t : table) {
        t = 7;
    }
    table[63] = 256;
    EXPECT_FALSE(QuantSteps(table).reciprocal);
    table[63] = 0;
    EXPECT_FALSE(QuantSteps(table).reciprocal);
}

// Quantization on the vector road against the division: every q, every
// x the reciprocals hold for (|x| + 4q under 2^15, far past the FDCT's
// ±8192), and random mixed tables
TEST(CodecSimd_Tests, QuantizeAgainstThePlainRoad) {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    for (uint16_t q = 1; q <= 255; ++q) {
        uint16_t table[64];
        for (auto& t : table) {
            t = q;
        }
        const QuantSteps steps(table);
        const int32_t top = 32767 - 4 * q;
        for (int32_t from = -top; from <= top; from += 64) {
            int32_t dct[64];
            for (int i = 0; i < 64; ++i) {
                dct[i] = std::min(from + i, top);
            }
            int16_t a[64], b[64];
            quantize_plain(dct, table, a);
            quantize_vector(dct, steps, b);
            ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << "q " << q << " from " << from;
        }
    }
    std::mt19937_64 rng(45);
    for (int round = 0; round < 20000; ++round) {
        uint16_t table[64];
        int32_t dct[64];
        for (int i = 0; i < 64; ++i) {
            table[i] = uint16_t(1 + rng() % 255);
            dct[i] = int32_t(rng() % 16385) - 8192;
        }
        int16_t a[64], b[64];
        quantize_plain(dct, table, a);
        quantize(dct, QuantSteps(table), b);
        ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << round;
    }
#else
    GTEST_SKIP() << "the plain road only";
#endif
}

// RGB to YCbCr: every R, G and B, the vector road's samples the plain
// one's; then every length through the whole function
TEST(CodecSimd_Tests, RgbToYccAgainstThePlainRoad) {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    uint8_t rgb[3 * 256];
    uint8_t y[2][256 + 8], cb[2][256 + 8], cr[2][256 + 8];
    for (int g = 0; g < 256; ++g) {
        for (int bl = 0; bl < 256; ++bl) {
            for (int r = 0; r < 256; ++r) {
                rgb[3 * r] = uint8_t(r);
                rgb[3 * r + 1] = uint8_t(g);
                rgb[3 * r + 2] = uint8_t(bl);
            }
            rgb_to_ycc_plain(rgb, y[0], cb[0], cr[0], 256);
            const size_t done = rgb_to_ycc_vector(rgb, y[1], cb[1], cr[1], 256);
            ASSERT_GE(done, 240u);
            ASSERT_EQ(std::memcmp(y[0], y[1], done), 0) << g << " " << bl;
            ASSERT_EQ(std::memcmp(cb[0], cb[1], done), 0) << g << " " << bl;
            ASSERT_EQ(std::memcmp(cr[0], cr[1], done), 0) << g << " " << bl;
        }
    }
    std::mt19937_64 rng(46);
    for (size_t n = 0; n <= 70; ++n) {
        for (int round = 0; round < 50; ++round) {
            for (size_t i = 0; i < 3 * n; ++i) {
                rgb[i] = uint8_t(rng());
            }
            for (int k = 0; k < 2; ++k) {
                std::memset(y[k], 0x5A, sizeof y[k]);
                std::memset(cb[k], 0x5A, sizeof cb[k]);
                std::memset(cr[k], 0x5A, sizeof cr[k]);
            }
            rgb_to_ycc_plain(rgb, y[0], cb[0], cr[0], n);
            rgb_to_ycc(rgb, y[1], cb[1], cr[1], n);
            ASSERT_EQ(std::memcmp(y[0], y[1], sizeof y[0]), 0) << n;   // nothing written past n either
            ASSERT_EQ(std::memcmp(cb[0], cb[1], sizeof cb[0]), 0) << n;
            ASSERT_EQ(std::memcmp(cr[0], cr[1], sizeof cr[0]), 0) << n;
        }
    }
#else
    GTEST_SKIP() << "the plain road only";
#endif
}

// The encoder's subsampling: every output count up to 70 and a long row,
// random samples and extremes, the vector road's rows the plain one's
TEST(CodecSimd_Tests, DownsampleAgainstThePlainRoad) {
    std::mt19937_64 rng(47);
    for (size_t n : {size_t(0), size_t(1), size_t(7), size_t(8), size_t(9), size_t(15), size_t(16), size_t(17), size_t(70), size_t(1000)}) {
        for (int round = 0; round < 200; ++round) {
            std::vector<uint8_t> a(2 * n), b(2 * n);
            for (size_t i = 0; i < 2 * n; ++i) {
                a[i] = round % 2 ? uint8_t(rng() & 1 ? 255 : 0) : uint8_t(rng());
                b[i] = round % 2 ? uint8_t(rng() & 1 ? 255 : 0) : uint8_t(rng());
            }
            std::vector<uint8_t> p(n + 8, 0x5A), v(n + 8, 0x5A);
            downsample::h2v1_plain(a.data(), p.data(), n);
            downsample::h2v1(a.data(), v.data(), n);
            ASSERT_EQ(p, v) << "h2v1 " << n;
            std::fill(p.begin(), p.end(), 0x5A);
            std::fill(v.begin(), v.end(), 0x5A);
            downsample::h2v2_plain(a.data(), b.data(), p.data(), n);
            downsample::h2v2(a.data(), b.data(), v.data(), n);
            ASSERT_EQ(p, v) << "h2v2 " << n;
        }
    }
}

// The encoder's mask of nonzero coefficients: every single coefficient set
// (each value class: small, large, the ends of 16 bits), then random blocks
// of every density; the vector road's mask the plain one's
TEST(CodecSimd_Tests, NonzeroMaskAgainstThePlainRoad) {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    for (int i = 0; i < 64; ++i) {
        for (int v : {1, -1, 127, -128, 128, -129, 255, 256, 32767, -32768}) {
            int16_t z[64] = {};
            z[i] = int16_t(v);
            ASSERT_EQ(nonzero_mask_vector(z), uint64_t(1) << i) << i << " " << v;
        }
    }
    std::mt19937_64 rng(48);
    for (int round = 0; round < 200000; ++round) {
        int16_t z[64];
        const unsigned density = unsigned(rng() % 65);
        for (auto& x : z) {
            x = rng() % 64 < density ? int16_t(rng()) : int16_t(0);
        }
        ASSERT_EQ(nonzero_mask_vector(z), nonzero_mask_plain(z)) << round;
    }
#else
    GTEST_SKIP() << "the plain road only";
#endif
}

// The block made ready for the coder: every coefficient at every value
// class (the ends of 16 bits among them), random blocks of every density;
// the vector road's order, categories, bits and mask the plain one's
TEST(CodecSimd_Tests, CodedBlockAgainstThePlainRoad) {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    auto same = [](const CodedBlock& a, const CodedBlock& b) {
        return std::memcmp(a.z, b.z, sizeof a.z) == 0 && std::memcmp(a.bits, b.bits, sizeof a.bits) == 0 &&
               std::memcmp(a.size, b.size, sizeof a.size) == 0 && a.mask == b.mask;
    };
    for (int i = 0; i < 64; ++i) {
        for (int v : {1, -1, 2, -2, 1023, -1023, 1024, -1024, 32767, -32767, -32768}) {
            int16_t block[64] = {};
            block[i] = int16_t(v);
            CodedBlock a, b;
            coded_block_plain(block, a);
            coded_block_vector(block, b);
            ASSERT_TRUE(same(a, b)) << i << " " << v;
        }
    }
    std::mt19937_64 rng(50);
    for (int round = 0; round < 200000; ++round) {
        int16_t block[64];
        const unsigned density = unsigned(rng() % 65);
        const int bound = round % 4 == 0 ? 32768 : 2048;
        for (auto& x : block) {
            x = rng() % 64 < density ? int16_t(int(rng() % (2 * bound)) - bound) : int16_t(0);
        }
        CodedBlock a, b;
        coded_block_plain(block, a);
        coded_block_vector(block, b);
        ASSERT_TRUE(same(a, b)) << round;
    }
#else
    GTEST_SKIP() << "the plain road only";
#endif
}

// The decoder's fancy upsampling: every width up to 70 and a long row,
// random samples and extremes; the dispatching entries (the vector road
// inside, the plain one at the edges and the rest) against the plain ones
TEST(CodecSimd_Tests, UpsampleAgainstThePlainRoad) {
    std::mt19937_64 rng(49);
    for (size_t n = 3; n <= 70; ++n) {
        for (int round = 0; round < 300; ++round) {
            std::vector<uint8_t> row(n), near(n);
            for (size_t i = 0; i < n; ++i) {
                row[i] = round % 3 == 0 ? uint8_t(rng() & 1 ? 255 : 0) : uint8_t(rng());
                near[i] = round % 3 == 0 ? uint8_t(rng() & 1 ? 255 : 0) : uint8_t(rng());
            }
            std::vector<uint8_t> a(2 * n + 8, 0x5A), b(2 * n + 8, 0x5A);
            upsample::h2_plain(row.data(), a.data(), n);
            upsample::h2(row.data(), b.data(), n);
            ASSERT_EQ(a, b) << "h2 " << n;
            std::fill(a.begin(), a.end(), 0x5A);
            std::fill(b.begin(), b.end(), 0x5A);
            upsample::h2v2_plain(row.data(), near.data(), a.data(), n);
            upsample::h2v2(row.data(), near.data(), b.data(), n);
            ASSERT_EQ(a, b) << "h2v2 " << n;
            for (bool lower : {false, true}) {
                std::fill(a.begin(), a.end(), 0x5A);
                std::fill(b.begin(), b.end(), 0x5A);
                upsample::v2_plain(row.data(), near.data(), lower, a.data(), n);
                upsample::v2(row.data(), near.data(), lower, b.data(), n);
                ASSERT_EQ(a, b) << "v2 " << n << " " << lower;
            }
        }
    }
    const size_t n = 1200;
    std::vector<uint8_t> row(n), near(n);
    for (size_t i = 0; i < n; ++i) {
        row[i] = uint8_t(rng());
        near[i] = uint8_t(rng());
    }
    std::vector<uint8_t> a(2 * n), b(2 * n);
    upsample::h2v2_plain(row.data(), near.data(), a.data(), n);
    upsample::h2v2(row.data(), near.data(), b.data(), n);
    ASSERT_EQ(a, b);
}

// The decoder's shortcut for a block of its DC alone against the full
// integer IDCT: every DC under a set of quantizers (the whole 2^32 pairs
// are tools/jpeg_dc_only_proof.cpp's), and random pairs
TEST(CodecSimd_Tests, DcOnlyAgainstTheFullIdct) {
    auto check = [](int dc, int q) {
        int16_t coef[64] = {};
        uint16_t quant[64];
        for (auto& x : quant) {
            x = uint16_t(q);
        }
        coef[0] = int16_t(dc);
        uint8_t full[8 * 11], fill[8 * 11];
        std::memset(full, 0x5A, sizeof full);
        std::memset(fill, 0x5A, sizeof fill);
        idct_islow_plain(coef, quant, full, 11);
        idct_dc_only_fill(int16_t(dc), uint16_t(q), fill, 11);
        return std::memcmp(full, fill, sizeof full) == 0;
    };
    for (int q : {0, 1, 2, 3, 7, 16, 99, 255, 256, 1000, 32768, 65535}) {
        for (int dc = -32768; dc <= 32767; ++dc) {
            ASSERT_TRUE(check(dc, q)) << dc << " " << q;
        }
    }
    std::mt19937_64 rng(51);
    for (int round = 0; round < 1000000; ++round) {
        ASSERT_TRUE(check(int(int16_t(rng())), int(uint16_t(rng())))) << round;
    }
}

// The loop filters' saturating form of RFC 6386's c8(s + 3 (q0 - p0)):
// saturating each of the three additions of d = c8(q0 - p0) gives the same
// for every s of 8 bits and every difference of two pixels
TEST(CodecSimd_Tests, SaturatingChainIsTheClamp) {
    auto c8 = [](int v) { return v < -128 ? -128 : v > 127 ? 127 : v; };
    for (int s = -128; s <= 127; ++s) {
        for (int x = -255; x <= 255; ++x) {
            const int d = c8(x);
            const int chain = c8(c8(c8(s + d) + d) + d);
            ASSERT_EQ(chain, c8(s + 3 * x)) << s << " " << x;
        }
    }
}

// The VP8 loop filters, vector road against the scalar one: every kind
// (simple, subblock, macroblock), across a column and across a row, luma
// (16 segments) and chroma (U and V, 8 each), random pixels and pixels of
// small differences (where the filters act), every limit
TEST(CodecSimd_Tests, Vp8FiltersAgainstThePlainRoad) {
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    using namespace sgcl::codec::detail::vp8;
    namespace fv = sgcl::codec::detail::vp8::filter_vector;
    std::mt19937_64 rng(52);
    constexpr ptrdiff_t Stride = 24;
    for (int round = 0; round < 60000; ++round) {
        // a 16x16 patch inside a 24-wide plane of 24 rows; the edge at
        // (8, 8) of the patch: columns or rows 4..11 around it
        uint8_t a[24 * 24], b[24 * 24], a2[24 * 24], b2[24 * 24];
        const int base = int(rng() % 256), spread = round % 3 == 0 ? 255 : int(1 + rng() % 24);
        for (int i = 0; i < 24 * 24; ++i) {
            a[i] = uint8_t(std::clamp(base + int(rng() % (2 * spread + 1)) - spread, 0, 255));
            a2[i] = uint8_t(std::clamp(base + int(rng() % (2 * spread + 1)) - spread, 0, 255));
        }
        std::memcpy(b, a, sizeof a);
        std::memcpy(b2, a2, sizeof a2);
        const int level = int(rng() % 64), interior = int(1 + rng() % 63), hev = int(rng() % 3);
        const int limit = round % 2 ? (level + 2) * 2 + interior : level * 2 + interior;
        const int kind = int(rng() % 3);
        const bool across_column = rng() & 1, chroma = rng() & 1;
        uint8_t* qa = a + 4 * Stride + 8;   // rows 4..19 (16) or 4..11 (8), the edge before column 8
        uint8_t* qb = b + 4 * Stride + 8;
        uint8_t* qa2 = a2 + 4 * Stride + 8;
        uint8_t* qb2 = b2 + 4 * Stride + 8;
        if (!across_column) {   // the edge before row 8, segments along columns 4..19 or 4..11
            qa = a + 8 * Stride + 4;
            qb = b + 8 * Stride + 4;
            qa2 = a2 + 8 * Stride + 4;
            qb2 = b2 + 8 * Stride + 4;
        }
        const ptrdiff_t step = across_column ? 1 : Stride, along = across_column ? Stride : 1;
        const int n = chroma ? 8 : 16;
        auto scalar = [&](uint8_t* q) {
            if (kind == 0) {
                filter::simple_edge(q, step, along, n, limit);
            } else if (kind == 1) {
                filter::subblock_edge(q, step, along, n, limit, interior, hev);
            } else {
                filter::macroblock_edge(q, step, along, n, limit, interior, hev);
            }
        };
        scalar(qa);
        if (chroma) {
            scalar(qa2);
        }
        uint8_t* second = chroma ? qb2 : nullptr;
        if (across_column) {
            if (kind == 0) {
                fv::vertical_edge<fv::Simple>(qb, second, Stride, limit, interior, hev);
            } else if (kind == 1) {
                fv::vertical_edge<fv::Subblock>(qb, second, Stride, limit, interior, hev);
            } else {
                fv::vertical_edge<fv::Macroblock>(qb, second, Stride, limit, interior, hev);
            }
        } else {
            if (kind == 0) {
                fv::horizontal_edge<fv::Simple>(qb, second, Stride, limit, interior, hev);
            } else if (kind == 1) {
                fv::horizontal_edge<fv::Subblock>(qb, second, Stride, limit, interior, hev);
            } else {
                fv::horizontal_edge<fv::Macroblock>(qb, second, Stride, limit, interior, hev);
            }
        }
        ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << "round " << round << " kind " << kind << " column " << across_column << " chroma " << chroma;
        ASSERT_EQ(std::memcmp(a2, b2, sizeof a2), 0) << "round " << round << " (second plane)";
    }
#else
    GTEST_SKIP() << "the plain road only";
#endif
}

// VP8's 4x4 inverse DCT with its add: blocks within the vector road's
// bound (every magnitude, sparse and dense) and past it (the plain road
// then), on random pixels; the dispatching entry against the plain one
TEST(CodecSimd_Tests, Vp8IdctAgainstThePlainRoad) {
    using namespace sgcl::codec::detail::vp8;
    std::mt19937_64 rng(53);
    size_t vector_blocks = 0;
    for (int round = 0; round < 400000; ++round) {
        int16_t in[16];
        const int bound = round % 4 == 0 ? 32767 : round % 4 == 1 ? 15000 : round % 4 == 2 ? 2048 : 64;
        const unsigned density = unsigned(1 + rng() % 16);
        for (auto& x : in) {
            x = rng() % 16 < density ? int16_t(int(rng() % (2 * uint64_t(bound) + 1)) - bound) : int16_t(0);
        }
        if (round % 1000 == 0) {
            in[rng() % 16] = int16_t(round % 2000 ? 15000 : -15000);
        }
        uint8_t a[4 * 7], b[4 * 7];
        for (auto& x : a) {
            x = uint8_t(rng());
        }
        std::memcpy(b, a, sizeof a);
        inverse_dct_add_plain(in, a, 7);
        inverse_dct_add(in, b, 7);
        ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << round;
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
        uint8_t c[4 * 7];
        std::memcpy(c, b, sizeof c);
        vector_blocks += inverse_dct_add_vector(in, c, 7);
#endif
    }
#if defined(SGCL_CODEC_NEON) || defined(SGCL_CODEC_SSE2)
    EXPECT_GT(vector_blocks, 200000u);
#endif
}

// Lossy WebP's colors written as the image's bytes: every Y, U and V, the
// vector road's rgb8 and rgba8 the plain one's and yuv_to_argb's channels
// (the path they replace for a still image); then every length
TEST(CodecSimd_Tests, Vp8YuvToRgbAgainstTheArgbRoad) {
    using namespace sgcl::codec::detail::vp8;
    uint8_t y[256], u[256], v[256], a[256];
    uint32_t argb[256];
    uint8_t rgb[3][3 * 256 + 4], rgba[3][4 * 256 + 4];
    for (int i = 0; i < 256; ++i) {
        y[i] = uint8_t(i);
        a[i] = uint8_t(255 - i);
    }
    for (int uu = 0; uu < 256; ++uu) {
        for (int vv = 0; vv < 256; ++vv) {
            std::memset(u, uu, sizeof u);
            std::memset(v, vv, sizeof v);
            yuv_to_argb(y, u, v, a, argb, 256);
            yuv_to_rgb(y, u, v, rgb[0], 256);
            yuv_to_rgb_plain(y, u, v, rgb[1], 256);
            yuv_to_rgba(y, u, v, a, rgba[0], 256);
            yuv_to_rgba_plain(y, u, v, a, rgba[1], 256);
            for (int i = 0; i < 256; ++i) {
                rgb[2][3 * i] = uint8_t(argb[i] >> 16);
                rgb[2][3 * i + 1] = uint8_t(argb[i] >> 8);
                rgb[2][3 * i + 2] = uint8_t(argb[i]);
                rgba[2][4 * i] = uint8_t(argb[i] >> 16);
                rgba[2][4 * i + 1] = uint8_t(argb[i] >> 8);
                rgba[2][4 * i + 2] = uint8_t(argb[i]);
                rgba[2][4 * i + 3] = uint8_t(argb[i] >> 24);
            }
            ASSERT_EQ(std::memcmp(rgb[0], rgb[2], 3 * 256), 0) << uu << " " << vv;
            ASSERT_EQ(std::memcmp(rgb[1], rgb[2], 3 * 256), 0) << uu << " " << vv;
            ASSERT_EQ(std::memcmp(rgba[0], rgba[2], 4 * 256), 0) << uu << " " << vv;
            ASSERT_EQ(std::memcmp(rgba[1], rgba[2], 4 * 256), 0) << uu << " " << vv;
        }
    }
    std::mt19937_64 rng(54);
    for (size_t n = 0; n <= 40; ++n) {
        for (int round = 0; round < 30; ++round) {
            for (size_t i = 0; i < n; ++i) {
                y[i] = uint8_t(rng());
                u[i] = uint8_t(rng());
                v[i] = uint8_t(rng());
                a[i] = uint8_t(rng());
            }
            std::memset(rgb, 0x5A, sizeof rgb);
            std::memset(rgba, 0x5A, sizeof rgba);
            yuv_to_rgb(y, u, v, rgb[0], n);
            yuv_to_rgb_plain(y, u, v, rgb[1], n);
            yuv_to_rgba(y, u, v, round & 1 ? a : nullptr, rgba[0], n);
            yuv_to_rgba_plain(y, u, v, round & 1 ? a : nullptr, rgba[1], n);
            ASSERT_EQ(std::memcmp(rgb[0], rgb[1], sizeof rgb[0]), 0) << n;   // nothing past 3n either
            ASSERT_EQ(std::memcmp(rgba[0], rgba[1], sizeof rgba[0]), 0) << n;
        }
    }
}

// TM_PRED of 16x16 and 8x8 and the DC-only residue add against their plain
// forms: random edges and extremes (where the clamps act); every DC
TEST(CodecSimd_Tests, Vp8TmAndDcOnlyAgainstThePlainRoad) {
    using namespace sgcl::codec::detail::vp8;
    std::mt19937_64 rng(55);
    for (int round = 0; round < 100000; ++round) {
        uint8_t above[17], left[16];
        for (auto& x : above) {
            x = round % 3 == 0 ? uint8_t(rng() & 1 ? 255 : 0) : uint8_t(rng());
        }
        for (auto& x : left) {
            x = round % 3 == 0 ? uint8_t(rng() & 1 ? 255 : 0) : uint8_t(rng());
        }
        uint8_t a[16 * 19], b[16 * 19];
        std::memset(a, 0x5A, sizeof a);
        std::memset(b, 0x5A, sizeof b);
        if (round & 1) {
            tm_predict_plain<16>(a, 19, above + 1, left);
            tm_predict<16>(b, 19, above + 1, left);
        } else {
            tm_predict_plain<8>(a, 19, above + 1, left);
            tm_predict<8>(b, 19, above + 1, left);
        }
        ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << round;
    }
    for (int dc = -32768; dc <= 32767; dc += 3) {
        int16_t in[16] = {};
        in[0] = int16_t(dc);
        uint8_t a[4 * 5], b[4 * 5];
        for (auto& x : a) {
            x = uint8_t(rng());
        }
        std::memcpy(b, a, sizeof a);
        inverse_dct_add_plain(in, a, 5);
        dc_only_add(int16_t(dc), b, 5);
        ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << dc;
    }
}

// VP8L's predictors that read the pixel to the left, as runs by mode,
// against the plain road: every such mode, every run length up to 40,
// random pixels and pixels of extremes (where Select's sums tie and the
// clamps act)
TEST(CodecSimd_Tests, Vp8lPredictLeftAgainstThePlainRoad) {
    using namespace sgcl::codec::detail::vp8l;
    std::mt19937_64 rng(57);
    for (unsigned mode : {1u, 5u, 6u, 7u, 10u, 11u, 12u, 13u}) {
        for (size_t n = 1; n <= 40; ++n) {
            for (int round = 0; round < 400; ++round) {
                // top: TL at [0], the n pixels above at [1..n], TR of the last at [n + 1]
                std::vector<uint32_t> top(n + 2), a(n + 1), b(n + 1);
                auto pixel = [&] {
                    if (round % 3 == 0) {
                        uint32_t v = 0;
                        for (int c = 0; c < 4; ++c) {
                            static const uint8_t edges[] = {0, 1, 127, 128, 254, 255};
                            v |= uint32_t(edges[rng() % 6]) << (8 * c);
                        }
                        return v;
                    }
                    return uint32_t(rng());
                };
                for (auto& x : top) {
                    x = pixel();
                }
                for (auto& x : a) {
                    x = pixel();
                }
                b = a;   // a[0] is L of the first pixel
                predict_left_run_plain(mode, a.data() + 1, top.data() + 1, n);
                predict_left_run(mode, b.data() + 1, top.data() + 1, n);
                ASSERT_EQ(a, b) << "mode " << mode << " n " << n << " round " << round;
            }
        }
    }
}
