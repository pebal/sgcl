//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: image::cropped, flipped and rotated (image.h, detail/orient.h).
// Every pixel format and odd sizes against a pixel-by-pixel reference; the
// turns composed (four quarter turns, two flips, a turn and its inverse);
// oriented() as the same transforms; the metadata copied; the contracts
// (a rectangle outside, a side of zero, an angle not a multiple of 90); a
// moved-from image; and the vector road against the plain one over every
// pixel size, transform and size around the blocks.
#include "common.h"

#include <random>
#include <string>
#include <vector>

using namespace sgcl;
using codec::pixel_format;

namespace {
    codec::image noise(uint32_t w, uint32_t h, pixel_format f, uint32_t seed = 1) {
        codec::image im(w, h, f);
        std::mt19937 rng(seed);
        for (auto& b : im.pixels()) {
            b = std::byte(rng());
        }
        return im;
    }

    std::string pixels_of(const codec::image& im) {
        auto px = im.pixels();
        return std::string(reinterpret_cast<const char*>(px.data()), px.size());
    }

    // The pixel at (x, y), its bytes
    std::string at(const codec::image& im, uint32_t x, uint32_t y) {
        const size_t b = im.stride() / im.width();
        auto row = im.row(y);
        return std::string(reinterpret_cast<const char*>(row.data()) + x * b, b);
    }
}

TEST(CodecImageTransforms_Tests, CroppedIsTheRectangle) {
    for (unsigned f = 0; f < 9; ++f) {
        codec::image im = noise(37, 23, pixel_format(f), f);
        for (auto [x, y, w, h] : {std::array<uint32_t, 4>{0, 0, 37, 23}, {5, 3, 10, 7}, {36, 22, 1, 1}, {0, 22, 37, 1}, {12, 0, 1, 23}}) {
            codec::image c = im.cropped(x, y, w, h);
            ASSERT_EQ(c.width(), w);
            ASSERT_EQ(c.height(), h);
            EXPECT_EQ(c.format(), im.format());
            for (uint32_t j = 0; j < h; ++j) {
                for (uint32_t i = 0; i < w; ++i) {
                    ASSERT_EQ(at(c, i, j), at(im, x + i, y + j)) << f;
                }
            }
        }
    }
}

TEST(CodecImageTransforms_Tests, CroppedRefusesWhatIsNotInside) {
    codec::image im = noise(10, 8, pixel_format::rgb8);
    EXPECT_THROW(im.cropped(0, 0, 0, 1), out_of_range);
    EXPECT_THROW(im.cropped(0, 0, 1, 0), out_of_range);
    EXPECT_THROW(im.cropped(10, 0, 1, 1), out_of_range);
    EXPECT_THROW(im.cropped(0, 8, 1, 1), out_of_range);
    EXPECT_THROW(im.cropped(5, 0, 6, 1), out_of_range);
    EXPECT_THROW(im.cropped(0, 4, 1, 5), out_of_range);
    EXPECT_THROW(im.cropped(UINT32_MAX, 0, 2, 1), out_of_range);   // no wrap of x + width
    EXPECT_THROW(im.cropped(0, UINT32_MAX, 1, 2), out_of_range);
    EXPECT_NO_THROW(im.cropped(9, 7, 1, 1));
}

TEST(CodecImageTransforms_Tests, FlippedMirrors) {
    for (unsigned f = 0; f < 9; ++f) {
        codec::image im = noise(29, 17, pixel_format(f), f + 9);
        codec::image h = im.flipped();
        codec::image v = im.flipped(codec::flip::vertical);
        EXPECT_EQ(pixels_of(im.flipped(codec::flip::horizontal)), pixels_of(h));
        for (uint32_t y = 0; y < 17; ++y) {
            for (uint32_t x = 0; x < 29; ++x) {
                ASSERT_EQ(at(h, x, y), at(im, 28 - x, y));
                ASSERT_EQ(at(v, x, y), at(im, x, 16 - y));
            }
        }
        EXPECT_EQ(pixels_of(h.flipped()), pixels_of(im));
        EXPECT_EQ(pixels_of(v.flipped(codec::flip::vertical)), pixels_of(im));
    }
}

TEST(CodecImageTransforms_Tests, RotatedTurnsClockwise) {
    for (unsigned f = 0; f < 9; ++f) {
        codec::image im = noise(19, 11, pixel_format(f), f + 20);
        codec::image r90 = im.rotated(90), r180 = im.rotated(180), r270 = im.rotated(270);
        ASSERT_EQ(r90.width(), 11u);
        ASSERT_EQ(r90.height(), 19u);
        for (uint32_t y = 0; y < 11; ++y) {
            for (uint32_t x = 0; x < 19; ++x) {
                // clockwise: the source's bottom-left corner comes to the top-left
                ASSERT_EQ(at(r90, 10 - y, x), at(im, x, y));
                ASSERT_EQ(at(r180, 18 - x, 10 - y), at(im, x, y));
                ASSERT_EQ(at(r270, y, 18 - x), at(im, x, y));
            }
        }
        EXPECT_EQ(pixels_of(im.rotated(-90)), pixels_of(r270));
        EXPECT_EQ(pixels_of(im.rotated(-270)), pixels_of(r90));
        EXPECT_EQ(pixels_of(im.rotated(450)), pixels_of(r90));
        EXPECT_EQ(pixels_of(im.rotated(0)), pixels_of(im));
        EXPECT_EQ(pixels_of(im.rotated(360)), pixels_of(im));
        EXPECT_EQ(pixels_of(r90.rotated(90).rotated(90).rotated(90)), pixels_of(im));
        EXPECT_EQ(pixels_of(r90.rotated(-90)), pixels_of(im));
        EXPECT_EQ(pixels_of(im.flipped().flipped(codec::flip::vertical)), pixels_of(r180));
    }
    codec::image im = noise(3, 3, pixel_format::gray8);
    for (int bad : {1, 45, 89, 91, -1, 100, 1000001}) {
        EXPECT_THROW(im.rotated(bad), invalid_argument) << bad;
    }
}

TEST(CodecImageTransforms_Tests, MetadataComesAlong) {
    codec::image im = noise(6, 4, pixel_format::rgba8);
    const std::string icc = "profile";
    im.set_icc(codec_test::bytes(icc));
    const unsigned char exif[] = {'I', 'I', 42, 0, 8, 0, 0, 0, 1, 0, 0x12, 0x01, 3, 0, 1, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0};
    im.set_exif(slice<const std::byte>(reinterpret_cast<const std::byte*>(exif), sizeof(exif)));
    for (const codec::image& t : {im.cropped(1, 1, 2, 2), im.flipped(), im.rotated(90)}) {
        EXPECT_EQ(t.orientation(), 6);
        EXPECT_EQ(t.exif().size(), sizeof(exif));
        EXPECT_EQ(t.icc().size(), icc.size());
    }
    // new pixels: the original untouched by a change of the copy
    codec::image c = im.cropped(0, 0, 6, 4);
    c.pixels()[0] = std::byte(uint8_t(im.pixels()[0]) ^ 1);
    EXPECT_NE(c.pixels()[0], im.pixels()[0]);
}

TEST(CodecImageTransforms_Tests, OrientedIsTheSameTransforms) {
    codec::image im = noise(13, 7, pixel_format::rgb8);
    const codec::image want[9] = {im, im, im.flipped(), im.rotated(180), im.flipped(codec::flip::vertical),
                                  im.rotated(90).flipped(), im.rotated(90), im.rotated(270).flipped(), im.rotated(270)};
    for (unsigned o = 1; o <= 8; ++o) {
        codec::image t = im.clone();
        t.set_orientation(o);
        EXPECT_EQ(pixels_of(t.oriented()), pixels_of(want[o])) << o;
    }
}

TEST(CodecImageTransforms_Tests, Boundaries) {
    // one pixel, one row, one column
    codec::image one = noise(1, 1, pixel_format::rgba16);
    EXPECT_EQ(pixels_of(one.rotated(90)), pixels_of(one));
    EXPECT_EQ(pixels_of(one.flipped()), pixels_of(one));
    codec::image row = noise(100, 1, pixel_format::gray8);
    codec::image col = row.rotated(90);
    ASSERT_EQ(col.width(), 1u);
    ASSERT_EQ(col.height(), 100u);
    for (uint32_t i = 0; i < 100; ++i) {
        ASSERT_EQ(at(col, 0, i), at(row, i, 0));
    }
    // a moved-from image is still the image
    codec::image a = noise(8, 8, pixel_format::rgb8);
    codec::image b = std::move(a);
    EXPECT_EQ(pixels_of(a.rotated(90)), pixels_of(b.rotated(90)));   // NOLINT(bugprone-use-after-move)
    static_assert(noexcept(b.flipped()));
}

TEST(CodecImageTransforms_Tests, VectorRoadAgainstThePlainOne) {
    std::mt19937 rng(5);
    for (unsigned bytes : {1u, 2u, 3u, 4u, 6u, 8u}) {
        for (auto [w, h] : {std::pair<size_t, size_t>{1, 1}, {7, 9}, {8, 8}, {9, 7}, {16, 16}, {17, 33}, {64, 64}, {65, 130}, {200, 3}, {3, 200}}) {
            std::vector<uint8_t> src(w * h * bytes), a(src.size()), b(src.size());
            for (auto& v : src) {
                v = uint8_t(rng());
            }
            for (unsigned o = 1; o <= 8; ++o) {
                codec::detail::orient::apply(src.data(), w, h, bytes, o, a.data());
                codec::detail::orient::apply_plain(src.data(), w, h, bytes, o, b.data());
                ASSERT_EQ(a, b) << "bytes " << bytes << " " << w << "x" << h << " o " << o;
            }
        }
    }
}
