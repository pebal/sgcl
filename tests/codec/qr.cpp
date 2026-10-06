//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: QR code generation (qr.h), ISO/IEC 18004:2015. The standard's
// conformance: the capacities of its table 7 at versions 1 and 40 for each
// mode and level, the format information of its annex C, the version
// information of annex D, the function patterns; the modes chosen (the
// versions they lead to); the options and the boundaries. Against the
// system's QR code (tools/codec_oracle_qr.m, CoreImage, macOS): every
// symbol CIQRCodeGenerator makes of a text, module for module (the data,
// the error correction, the placement, the mask it chooses), and
// CIDetector's reading of the module's symbols of every mode, Kanji and
// ECI included.
#include "oracle.h"

#include <cstdlib>
#include <fstream>
#include <random>
#include <string>

using namespace sgcl;
using namespace codec_test;
using level = codec::qr::level;

namespace {
    std::string scratch(const std::string& name) {
        return (scratch_path("sgcl_codec_qr_tests") / name).string();
    }

    codec::qr::options at(level l, int version = 0) {
        codec::qr::options o;
        o.level = l;
        o.boost_level = false;
        if (version) {
            o.min_version = version;
            o.max_version = version;
        }
        return o;
    }

    // The 15 format bits as the symbol holds them around the top left
    // finder, bit 14 first
    uint32_t format_bits(const codec::qr& q) {
        uint32_t v = 0;
        auto bit = [&](uint32_t x, uint32_t y) { v = v << 1 | (q.dark(x, y) ? 1 : 0); };
        // bits 14..9 at (14 - i, 8) for i 9..14, read from i = 14 down
        for (int i = 14; i >= 9; --i) {
            bit(uint32_t(14 - i), 8);
        }
        bit(7, 8);   // 8
        bit(8, 8);   // 7
        bit(8, 7);   // 6
        for (int i = 5; i >= 0; --i) {
            bit(8, uint32_t(i));
        }
        return v;
    }

    // The same 15 bits from the copy beside the other two finders
    uint32_t format_copy(const codec::qr& q) {
        const uint32_t n = q.size();
        uint32_t v = 0;
        for (int i = 14; i >= 8; --i) {
            v = v << 1 | (q.dark(8, n - 15 + uint32_t(i)) ? 1 : 0);
        }
        for (int i = 7; i >= 0; --i) {
            v = v << 1 | (q.dark(n - 1 - uint32_t(i), 8) ? 1 : 0);
        }
        return v;
    }

    std::string matrix(const codec::qr& q) {
        std::string s;
        for (uint32_t y = 0; y < q.size(); ++y) {
            for (uint32_t x = 0; x < q.size(); ++x) {
                s += q.dark(x, y) ? '1' : '0';
            }
            s += '\n';
        }
        return s;
    }
}

TEST(CodecQr_Tests, CapacitiesOfTheStandard) {
    // ISO/IEC 18004 table 7: characters at versions 1 and 40, by level:
    // numeric, alphanumeric, byte, Kanji
    struct Row {
        int version;
        level l;
        int numeric, alnum, bytes, kanji;
    };
    const Row rows[] = {
        {1, level::low, 41, 25, 17, 10},          {1, level::medium, 34, 20, 14, 8},
        {1, level::quartile, 27, 16, 11, 7},      {1, level::high, 17, 10, 7, 4},
        {40, level::low, 7089, 4296, 2953, 1817}, {40, level::medium, 5596, 3391, 2331, 1435},
        {40, level::quartile, 3993, 2420, 1663, 1024}, {40, level::high, 3057, 1852, 1273, 784},
    };
    for (const auto& r : rows) {
        SCOPED_TRACE(r.version * 10 + int(r.l));
        const auto o = at(r.l, r.version);
        auto fits = [&](const std::string& s) { return codec::qr::encode(string(s.c_str()), o).has_value(); };
        EXPECT_TRUE(fits(std::string(size_t(r.numeric), '7')));
        EXPECT_FALSE(fits(std::string(size_t(r.numeric) + 1, '7')));
        EXPECT_TRUE(fits(std::string(size_t(r.alnum), 'Q')));
        EXPECT_FALSE(fits(std::string(size_t(r.alnum) + 1, 'Q')));
        EXPECT_TRUE(fits(std::string(size_t(r.bytes), 'q')));
        EXPECT_FALSE(fits(std::string(size_t(r.bytes) + 1, 'q')));
        std::string kanji;
        for (int i = 0; i < r.kanji; ++i) {
            kanji += "漢";
        }
        EXPECT_TRUE(fits(kanji));
        EXPECT_FALSE(fits(kanji + "漢"));
        const vector<byte> data(size_t(r.bytes), byte(0xA5));
        EXPECT_TRUE(codec::qr::encode(data, o).has_value());
        const vector<byte> more(size_t(r.bytes) + 1, byte(0xA5));
        auto refused = codec::qr::encode(more, o);
        ASSERT_FALSE(refused);
        EXPECT_EQ(refused.error().code(), codec::errc::too_large);
    }
}

TEST(CodecQr_Tests, FormatVersionAndFunctionPatterns) {
    // annex C: the format words of mask 0 at each level, and of level M at
    // every mask (C.1's example: M, mask 5 is 100000011001110)
    const uint32_t level_mask0[4] = {0x77C4, 0x5412, 0x355F, 0x1689};
    for (int l = 0; l < 4; ++l) {
        auto o = at(level(l), 1);
        o.mask = 0;
        const codec::qr q = codec::qr::encode("SGCL", o).value();
        EXPECT_EQ(format_bits(q), level_mask0[l]) << l;
        EXPECT_EQ(format_copy(q), level_mask0[l]) << l;
    }
    auto m5 = at(level::medium, 1);
    m5.mask = 5;
    EXPECT_EQ(format_bits(codec::qr::encode("SGCL", m5).value()), 0x40CEu);
    // annex D: version 7's information 000111110010010100, both copies
    const codec::qr v7 = codec::qr::encode("SGCL", at(level::low, 7)).value();
    EXPECT_EQ(v7.size(), 45u);
    uint32_t below = 0, beside = 0;
    for (int i = 17; i >= 0; --i) {
        below = below << 1 | (v7.dark(v7.size() - 11 + uint32_t(i % 3), uint32_t(i / 3)) ? 1 : 0);
        beside = beside << 1 | (v7.dark(uint32_t(i / 3), v7.size() - 11 + uint32_t(i % 3)) ? 1 : 0);
    }
    EXPECT_EQ(below, 0x07C94u);
    EXPECT_EQ(beside, 0x07C94u);
    // version 40: 0x28C69
    const codec::qr v40 = codec::qr::encode("SGCL", at(level::low, 40)).value();
    uint32_t v40bits = 0;
    for (int i = 17; i >= 0; --i) {
        v40bits = v40bits << 1 | (v40.dark(v40.size() - 11 + uint32_t(i % 3), uint32_t(i / 3)) ? 1 : 0);
    }
    EXPECT_EQ(v40bits, 0x28C69u);
    // the finders, their separators, the timing patterns, the dark module
    for (int v : {1, 2, 7, 14, 40}) {
        const codec::qr q = codec::qr::encode("SGCL", at(level::high, v)).value();
        const uint32_t n = q.size();
        EXPECT_EQ(n, uint32_t(17 + 4 * v));
        for (auto [ox, oy] : {std::pair<uint32_t, uint32_t>{0, 0}, {n - 7, 0}, {0, n - 7}}) {
            for (uint32_t y = 0; y < 7; ++y) {
                for (uint32_t x = 0; x < 7; ++x) {
                    const uint32_t ring = std::max(x > 3 ? x - 3 : 3 - x, y > 3 ? y - 3 : 3 - y);
                    EXPECT_EQ(q.dark(ox + x, oy + y), ring != 2) << v;
                }
            }
        }
        for (uint32_t i = 0; i < 8; ++i) {
            EXPECT_FALSE(q.dark(7, i));
            EXPECT_FALSE(q.dark(i, 7));
            EXPECT_FALSE(q.dark(n - 8, i));
            EXPECT_FALSE(q.dark(i, n - 8));
        }
        for (uint32_t i = 8; i < n - 8; ++i) {
            EXPECT_EQ(q.dark(i, 6), i % 2 == 0);
            EXPECT_EQ(q.dark(6, i), i % 2 == 0);
        }
        EXPECT_TRUE(q.dark(8, n - 8));
    }
}

TEST(CodecQr_Tests, ModesChosen) {
    // the versions the fewest bits lead to (numeric 10 bits a 3 digits,
    // alphanumeric 11 a 2, byte 8, Kanji 13, each segment's header)
    auto version_of = [](const std::string& s, codec::qr::options o = at(level::low)) {
        return codec::qr::encode(string(s.c_str()), o).value().version();
    };
    EXPECT_EQ(version_of(std::string(41, '1')), 1);
    EXPECT_EQ(version_of(std::string(42, '1')), 2);
    EXPECT_EQ(version_of(std::string(25, 'A')), 1);
    EXPECT_EQ(version_of(std::string(26, 'A')), 2);
    // digits inside an alphanumeric text: a numeric segment pays off past
    // a run of digits long enough
    EXPECT_EQ(version_of("A" + std::string(30, '5') + "B"), 1);
    // lower case is byte mode
    EXPECT_EQ(version_of(std::string(17, 'a')), 1);
    EXPECT_EQ(version_of(std::string(18, 'a')), 2);
    // Kanji mode against byte mode for Japanese text: 10 characters fit
    // version 1 L in Kanji mode, 3 bytes each would not
    std::string ten;
    for (int i = 0; i < 10; ++i) {
        ten += "日";
    }
    EXPECT_EQ(version_of(ten), 1);
    auto no_kanji = at(level::low);
    no_kanji.kanji = false;
    EXPECT_GT(version_of(ten, no_kanji), 1);
    // ECI in front of UTF-8 bytes: 12 bits more than without
    const std::string polish = "Zażółć";
    auto no_eci = at(level::low);
    no_eci.eci = false;
    EXPECT_EQ(version_of(polish), version_of(polish, no_eci));
    // the empty text: version 1
    EXPECT_EQ(version_of(""), 1);
}

TEST(CodecQr_Tests, AsCoreImageGenerates) {
#if defined(__APPLE__)
    if (qr_oracle().empty()) {
        GTEST_SKIP() << "no QR oracle";
    }
    std::mt19937 rng(7);
    // characters of byte mode only (none of alphanumeric mode's), so that
    // the segments are one, as CoreImage makes them
    const std::string chars = "abcdefghijklmnopqrstuvwxyz,!?";
    int compared = 0;
    for (char lv : std::string("LMQH")) {
        const level l = lv == 'L' ? level::low : lv == 'M' ? level::medium : lv == 'Q' ? level::quartile : level::high;
        for (int n : {1, 5, 14, 30, 60, 100, 150, 200, 300, 400, 500, 700, 900, 1100, 1300, 1500, 1700, 2000, 2300, 2600, 2900}) {
            std::string text;
            for (int i = 0; i < n; ++i) {
                text += chars[rng() % chars.size()];
            }
            auto ours = codec::qr::encode(string(text.c_str()), at(l));
            if (!ours) {
                continue;   // past version 40 at this level
            }
            auto theirs = run_text("'" + qr_oracle() + "' make '" + text + "' " + lv);
            ASSERT_TRUE(theirs) << n << lv;
            const size_t nl = theirs->find('\n');
            EXPECT_EQ(std::stoul(theirs->substr(0, nl)), ours->size()) << n << lv;
            EXPECT_EQ(theirs->substr(nl + 1), matrix(*ours)) << n << lv;
            ++compared;
        }
    }
    EXPECT_GT(compared, 60);
#else
    GTEST_SKIP() << "CoreImage is macOS's";
#endif
}

TEST(CodecQr_Tests, CoreImageReadsEveryMode) {
#if defined(__APPLE__)
    if (qr_oracle().empty()) {
        GTEST_SKIP() << "no QR oracle";
    }
    const char* texts[] = {
        "0123456789012345678901234567890123456789",
        "HELLO WORLD $%*+-./:",
        "ORDER 12345678901234567890 X",
        "https://example.com/path?q=1&r=two",
        "日本語のテキストです",
        "Zażółć gęślą jaźń",
        "Mixed 漢字 and ASCII 12345 ÄÖÜ",
        "",
    };
    int i = 0;
    for (const char* t : texts) {
        for (level l : {level::low, level::high}) {
            SCOPED_TRACE(t);
            const codec::qr q = codec::qr::encode(t, at(l)).value();
            const std::string path = scratch("read" + std::to_string(i++) + ".png");
            ASSERT_TRUE(q.to_image(4).save(string(path.c_str())));
            auto read = run_text("'" + qr_oracle() + "' read '" + path + "'");
            ASSERT_TRUE(read);
            const size_t nl = read->find('\n');
            EXPECT_EQ(read->substr(0, nl), t);
            const std::string tail = read->substr(nl + 1);
            const char letter = "LMQH"[int(q.correction())];
            EXPECT_EQ(tail, "version " + std::to_string(q.version()) + " level " + letter + " mask " + std::to_string(q.mask()) + "\n");
        }
    }
#else
    GTEST_SKIP() << "CoreImage is macOS's";
#endif
}

TEST(CodecQr_Tests, OptionsAndBoundaries) {
    // boost: version 1 at L holds "SGCL" at H too
    codec::qr::options boosted;
    boosted.level = level::low;
    EXPECT_EQ(codec::qr::encode("SGCL", boosted).value().correction(), level::high);
    EXPECT_EQ(codec::qr::encode("SGCL", at(level::low)).value().correction(), level::low);
    // a mask forced, a version range
    for (int m = 0; m < 8; ++m) {
        auto o = at(level::medium);
        o.mask = m;
        EXPECT_EQ(codec::qr::encode("SGCL", o).value().mask(), m);
    }
    auto from5 = at(level::medium);
    from5.min_version = 5;
    EXPECT_EQ(codec::qr::encode("SGCL", from5).value().version(), 5);
    auto upto2 = at(level::high);
    upto2.max_version = 2;
    auto big = codec::qr::encode(string(std::string(100, 'x').c_str()), upto2);
    ASSERT_FALSE(big);
    EXPECT_EQ(big.error().code(), codec::errc::too_large);
    // options outside their ranges
    for (auto bad : {std::pair<int, int>{0, 40}, {1, 41}, {7, 6}}) {
        auto o = at(level::low);
        o.min_version = bad.first;
        o.max_version = bad.second;
        EXPECT_THROW((void)codec::qr::encode("x", o), invalid_argument);
    }
    auto mask9 = at(level::low);
    mask9.mask = 8;
    EXPECT_THROW((void)codec::qr::encode("x", mask9), invalid_argument);
    auto level9 = at(level::low);
    level9.level = level(9);
    EXPECT_THROW((void)codec::qr::encode("x", level9), invalid_argument);
    // a text that is not UTF-8: its bytes in byte mode
    const char raw[] = {'\xff', '\xfe', 'a', 0};
    EXPECT_TRUE(codec::qr::encode(string(raw)).has_value());
    // dark() past the symbol, the image and the SVG
    const codec::qr q = codec::qr::encode("SGCL").value();
    EXPECT_THROW((void)q.dark(q.size(), 0), out_of_range);
    EXPECT_THROW((void)q.dark(0, q.size()), out_of_range);
    const codec::image im = q.to_image(3, 2);
    EXPECT_EQ(im.width(), (q.size() + 4) * 3);
    EXPECT_EQ(im.format(), codec::pixel_format::gray8);
    for (uint32_t y = 0; y < q.size(); ++y) {
        for (uint32_t x = 0; x < q.size(); ++x) {
            EXPECT_EQ(uint8_t(im.row((y + 2) * 3 + 1)[(x + 2) * 3 + 2]), q.dark(x, y) ? 0 : 255);
        }
    }
    EXPECT_EQ(uint8_t(im.row(0)[0]), 255);
    EXPECT_THROW((void)q.to_image(0), invalid_argument);
    const codec::image tight = q.to_image(1, 0);
    EXPECT_EQ(tight.width(), q.size());
    const string svg = q.to_svg();
    const std::string s(svg.data(), svg.size());
    EXPECT_EQ(s.rfind("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 29 29\"", 0), 0u);
    EXPECT_NE(s.find("M4 4h7v1h-7z"), std::string::npos);   // the top row of the top left finder
    EXPECT_EQ(s.substr(s.size() - 7), "</svg>\n");
    // a copy shares; a moved-from value is still the value
    codec::qr a = q;
    codec::qr b = std::move(a);
    EXPECT_EQ(a.size(), b.size());
    EXPECT_EQ(matrix(a), matrix(q));
    // the same input, the same symbol
    EXPECT_EQ(matrix(codec::qr::encode("SGCL").value()), matrix(q));
}
