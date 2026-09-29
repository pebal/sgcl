//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: the image in memory (image.h), its pixel formats and their
// conversions (detail/pixels.h), the eight orientations of EXIF, the
// signatures (format.h), the error and the limits.
#include "common.h"

#include <cmath>
#include <cstring>
#include <random>
#include <vector>

using namespace sgcl;
using codec::pixel_format;

namespace {
    constexpr pixel_format AllFormats[] = {
        pixel_format::gray8, pixel_format::gray_alpha8, pixel_format::rgb8, pixel_format::rgba8,
        pixel_format::gray16, pixel_format::gray_alpha16, pixel_format::rgb16, pixel_format::rgba16,
        pixel_format::cmyk8
    };

    // One pixel of any format, from the channel values as numbers (16-bit
    // ones in the byte order of the machine)
    std::vector<std::byte> pixel(pixel_format f, std::initializer_list<unsigned> channels) {
        std::vector<std::byte> out;
        const bool wide = codec::detail::wide(f);
        for (unsigned v : channels) {
            if (wide) {
                uint16_t w = static_cast<uint16_t>(v);
                std::byte b[2];
                std::memcpy(b, &w, 2);
                out.push_back(b[0]);
                out.push_back(b[1]);
            } else {
                out.push_back(static_cast<std::byte>(v));
            }
        }
        EXPECT_EQ(out.size(), codec::detail::bytes_per_pixel(f));
        return out;
    }

    // A 1×1 image of the pixel converted to `to`, its bytes
    std::vector<std::byte> convert_one(pixel_format from, std::initializer_list<unsigned> channels, pixel_format to) {
        codec::image one(1, 1, from);
        auto p = pixel(from, channels);
        std::memcpy(one.pixels().data(), p.data(), p.size());
        codec::image converted = one.convert(to);
        auto px = converted.pixels();
        return std::vector<std::byte>(px.begin(), px.end());
    }

    void fill_random(codec::image& im, uint32_t seed) {
        std::mt19937 rng(seed);
        for (auto& b : im.pixels()) {
            b = static_cast<std::byte>(rng());
        }
    }

    bool same_pixels(const codec::image& a, const codec::image& b) {
        auto x = a.pixels();
        auto y = b.pixels();
        return x.size() == y.size() && std::memcmp(x.data(), y.data(), x.size()) == 0;
    }

    // live objects per image make() makes, 1000 of them
    template<class F>
    double objects_per(F make) {
        const size_t n = 1000;
        collector::force_collect(true);
        const size_t before = collector::get_live_object_count();
        sgcl::vector<decltype(make())> held;
        off_frame([&] {
            held.reserve(n);
            for (size_t i = 0; i < n; ++i) {
                held.push_back(make());
            }
        });
        const size_t after = collector::get_live_object_count();
        const double per = double(after - before - 1) / double(n);   // the vector's buffer: one
        held = {};
        return per;
    }
}

TEST(CodecImage_Tests, SizeFormatAndRows) {
    for (pixel_format f : AllFormats) {
        codec::image picture(5, 3, f);
        const size_t b = codec::detail::bytes_per_pixel(f);
        EXPECT_EQ(picture.width(), 5u);
        EXPECT_EQ(picture.height(), 3u);
        EXPECT_EQ(picture.format(), f);
        EXPECT_EQ(picture.stride(), 5 * b);
        EXPECT_EQ(picture.pixels().size(), 15 * b);
        for (auto v : picture.pixels()) {
            ASSERT_EQ(v, std::byte{0});
        }
        EXPECT_EQ(picture.row(2).data(), picture.pixels().data() + 10 * b);
        EXPECT_EQ(picture.row(2).size(), 5 * b);
        EXPECT_THROW((void)picture.row(3), std::out_of_range);
        EXPECT_EQ(picture.orientation(), 1u);
        EXPECT_TRUE(picture.exif().empty());
        EXPECT_TRUE(picture.icc().empty());
    }
    EXPECT_EQ(codec::detail::bytes_per_pixel(pixel_format::gray_alpha16), 4u);
    EXPECT_EQ(codec::detail::bytes_per_pixel(pixel_format::rgb16), 6u);
    EXPECT_EQ(codec::detail::bytes_per_pixel(pixel_format::rgba16), 8u);
    EXPECT_EQ(codec::detail::bytes_per_pixel(pixel_format::cmyk8), 4u);
}

TEST(CodecImage_Tests, TheContractThrows) {
    EXPECT_THROW(codec::image(0, 5, pixel_format::rgb8), std::invalid_argument);
    EXPECT_THROW(codec::image(5, 0, pixel_format::rgb8), std::invalid_argument);
    EXPECT_THROW(codec::image(5, 5, static_cast<pixel_format>(9)), std::invalid_argument);
    EXPECT_THROW(codec::image(0xFFFFFFFFu, 0xFFFFFFFFu, pixel_format::rgba16), std::length_error);
    codec::image picture(2, 2, pixel_format::rgb8);
    EXPECT_THROW((void)picture.convert(static_cast<pixel_format>(200)), std::invalid_argument);
}

TEST(CodecImage_Tests, CopiesShareClonesDoNot) {
    codec::image picture(4, 4, pixel_format::rgba8);
    codec::image copy = picture;
    codec::image cloned = picture.clone();
    picture.row(1)[2] = std::byte{77};
    EXPECT_EQ(copy.row(1)[2], std::byte{77});
    EXPECT_EQ(cloned.row(1)[2], std::byte{0});
    fill_random(picture, 1);
    codec::image again = picture.clone();
    EXPECT_TRUE(same_pixels(again, picture));
    EXPECT_NE(again.pixels().data(), picture.pixels().data());
}

TEST(CodecImage_Tests, ASliceKeepsThePixels) {
    slice<const byte> kept;
    off_frame([&] {
        codec::image picture(64, 64, pixel_format::rgb8);
        fill_random(picture, 2);
        kept = std::as_const(picture).row(10);
    });
    collector::force_collect(true);
    std::mt19937 rng(2);
    for (size_t i = 0; i < 64 * 3 * 10; ++i) {
        (void)rng();
    }
    for (auto v : kept) {
        ASSERT_EQ(v, static_cast<std::byte>(rng()));
    }
}

TEST(CodecImage_Tests, DepthScalesToTheNearest) {
    for (unsigned v = 0; v < 65536; ++v) {
        ASSERT_EQ(codec::detail::narrow(static_cast<uint16_t>(v)), static_cast<unsigned>(std::lround(v / 257.0))) << v;
    }
    for (unsigned v = 0; v < 256; ++v) {
        ASSERT_EQ(codec::detail::widen(static_cast<uint8_t>(v)), v * 257);
        ASSERT_EQ(codec::detail::narrow(codec::detail::widen(static_cast<uint8_t>(v))), v);
    }
}

TEST(CodecImage_Tests, ConvertKnownPixels) {
    using P = std::vector<std::byte>;
    auto px = [](pixel_format f, std::initializer_list<unsigned> c) { return pixel(f, c); };
    // color to gray: the luma of Rec. 601
    const unsigned y = (19595 * 10 + 38470 * 200 + 7471 * 30 + 32768) >> 16;
    EXPECT_EQ(convert_one(pixel_format::rgb8, {10, 200, 30}, pixel_format::gray8), P(px(pixel_format::gray8, {y})));
    EXPECT_EQ(convert_one(pixel_format::rgba8, {10, 200, 30, 40}, pixel_format::gray_alpha8), P(px(pixel_format::gray_alpha8, {y, 40})));
    // alpha dropped, alpha made opaque
    EXPECT_EQ(convert_one(pixel_format::rgba8, {1, 2, 3, 4}, pixel_format::rgb8), P(px(pixel_format::rgb8, {1, 2, 3})));
    EXPECT_EQ(convert_one(pixel_format::rgb8, {1, 2, 3}, pixel_format::rgba8), P(px(pixel_format::rgba8, {1, 2, 3, 255})));
    EXPECT_EQ(convert_one(pixel_format::rgb16, {1, 2, 3}, pixel_format::rgba16), P(px(pixel_format::rgba16, {1, 2, 3, 65535})));
    // gray to color: the value in each channel
    EXPECT_EQ(convert_one(pixel_format::gray_alpha8, {9, 99}, pixel_format::rgba8), P(px(pixel_format::rgba8, {9, 9, 9, 99})));
    EXPECT_EQ(convert_one(pixel_format::gray16, {1000}, pixel_format::rgb16), P(px(pixel_format::rgb16, {1000, 1000, 1000})));
    // depth
    EXPECT_EQ(convert_one(pixel_format::rgb8, {0, 128, 255}, pixel_format::rgb16), P(px(pixel_format::rgb16, {0, 128 * 257, 65535})));
    EXPECT_EQ(convert_one(pixel_format::rgb16, {128, 129, 65535}, pixel_format::rgb8), P(px(pixel_format::rgb8, {0, 1, 255})));
    EXPECT_EQ(convert_one(pixel_format::gray16, {0x1234}, pixel_format::gray8), P(px(pixel_format::gray8, {18})));   // 4660 / 257 = 18.13
    EXPECT_EQ(convert_one(pixel_format::gray8, {18}, pixel_format::gray_alpha16), P(px(pixel_format::gray_alpha16, {18 * 257, 65535})));
    // a gray value through color keeps its value (the weights sum to 65536)
    for (unsigned v : {0u, 1u, 127u, 128u, 254u, 255u}) {
        EXPECT_EQ(convert_one(pixel_format::rgb8, {v, v, v}, pixel_format::gray8), P(px(pixel_format::gray8, {v})));
    }
    // CMYK: no ink is white, full black is black, cyan alone takes red
    EXPECT_EQ(convert_one(pixel_format::cmyk8, {0, 0, 0, 0}, pixel_format::rgb8), P(px(pixel_format::rgb8, {255, 255, 255})));
    EXPECT_EQ(convert_one(pixel_format::cmyk8, {0, 0, 0, 255}, pixel_format::rgb8), P(px(pixel_format::rgb8, {0, 0, 0})));
    EXPECT_EQ(convert_one(pixel_format::cmyk8, {255, 0, 0, 0}, pixel_format::rgba8), P(px(pixel_format::rgba8, {0, 255, 255, 255})));
    // (255 - 100)(255 - 50) / 255 = 124.6
    EXPECT_EQ(convert_one(pixel_format::cmyk8, {100, 0, 255, 50}, pixel_format::rgb8), P(px(pixel_format::rgb8, {125, 205, 0})));
    EXPECT_EQ(convert_one(pixel_format::cmyk8, {0, 0, 0, 0}, pixel_format::rgb16), P(px(pixel_format::rgb16, {65535, 65535, 65535})));
    EXPECT_EQ(convert_one(pixel_format::rgb8, {255, 255, 255}, pixel_format::cmyk8), P(px(pixel_format::cmyk8, {0, 0, 0, 0})));
    EXPECT_EQ(convert_one(pixel_format::rgb8, {0, 0, 0}, pixel_format::cmyk8), P(px(pixel_format::cmyk8, {0, 0, 0, 255})));
    EXPECT_EQ(convert_one(pixel_format::rgb8, {255, 0, 0}, pixel_format::cmyk8), P(px(pixel_format::cmyk8, {0, 255, 255, 0})));
    EXPECT_EQ(convert_one(pixel_format::rgb8, {100, 50, 0}, pixel_format::cmyk8), P(px(pixel_format::cmyk8, {0, 128, 255, 155})));
}

TEST(CodecImage_Tests, ConvertRoundTrips) {
    // every format to the one that holds more of it and back: the same
    // pixels (all but CMYK, which has no place in the others)
    struct Pair { pixel_format narrow, wide; };
    const Pair pairs[] = {
        {pixel_format::gray8, pixel_format::gray16}, {pixel_format::gray8, pixel_format::rgb8},
        {pixel_format::gray8, pixel_format::rgba16}, {pixel_format::gray_alpha8, pixel_format::gray_alpha16},
        {pixel_format::gray_alpha8, pixel_format::rgba8}, {pixel_format::rgb8, pixel_format::rgb16},
        {pixel_format::rgb8, pixel_format::rgba8}, {pixel_format::rgb8, pixel_format::rgba16},
        {pixel_format::rgba8, pixel_format::rgba16}, {pixel_format::gray16, pixel_format::rgb16},
        {pixel_format::gray_alpha16, pixel_format::rgba16}, {pixel_format::rgb16, pixel_format::rgba16},
        {pixel_format::cmyk8, pixel_format::cmyk8},
    };
    uint32_t seed = 10;
    for (auto [a, b] : pairs) {
        codec::image picture(37, 11, a);
        fill_random(picture, seed++);
        codec::image back = picture.convert(b).convert(a);
        EXPECT_EQ(back.format(), a);
        EXPECT_TRUE(same_pixels(back, picture)) << int(a) << " -> " << int(b);
    }
    // any format into any other: the size holds, and a conversion is a copy
    for (pixel_format a : AllFormats) {
        codec::image picture(13, 7, a);
        fill_random(picture, seed++);
        for (pixel_format b : AllFormats) {
            codec::image converted = picture.convert(b);
            EXPECT_EQ(converted.width(), 13u);
            EXPECT_EQ(converted.height(), 7u);
            EXPECT_EQ(converted.format(), b);
            EXPECT_NE(converted.pixels().data(), picture.pixels().data());
        }
    }
}

TEST(CodecImage_Tests, TheConverterOfADecoderRow) {
    // what a decoder calls for decode_options.want, on memory of its own
    const unsigned char rgb[] = {1, 2, 3, 4, 5, 6};
    unsigned char rgba[8] = {};
    codec::detail::converter(pixel_format::rgb8, pixel_format::rgba8)(
        reinterpret_cast<const std::byte*>(rgb), reinterpret_cast<std::byte*>(rgba), 2);
    const unsigned char expected[] = {1, 2, 3, 255, 4, 5, 6, 255};
    EXPECT_EQ(std::memcmp(rgba, expected, 8), 0);
}

TEST(CodecImage_Tests, MetadataTravelsWithCopies) {
    codec::image picture(3, 2, pixel_format::rgb8);
    auto& s = codec::detail::ImageAccess::state(picture);
    s.exif.push_back(byte{'M'});
    s.exif.push_back(byte{'M'});
    s.icc.push_back(byte{7});
    codec::detail::ImageAccess::set_orientation(picture, 6);
    for (codec::image other : {picture.clone(), picture.convert(pixel_format::gray16)}) {
        EXPECT_EQ(other.exif().size(), 2u);
        EXPECT_EQ(other.icc().size(), 1u);
        EXPECT_EQ(other.orientation(), 6u);
        EXPECT_NE(other.exif().data(), picture.exif().data());
    }
    codec::image upright = picture.oriented();
    EXPECT_EQ(upright.orientation(), 1u);
    EXPECT_EQ(upright.exif().size(), 2u);
    EXPECT_EQ(upright.icc().size(), 1u);
    // what EXIF may hold past 1 to 8 is 1, the image as stored
    codec::detail::ImageAccess::set_orientation(picture, 9);
    EXPECT_EQ(picture.orientation(), 1u);
    codec::detail::ImageAccess::set_orientation(picture, 0);
    EXPECT_EQ(picture.orientation(), 1u);
}

TEST(CodecImage_Tests, TheEightOrientations) {
    // a 3×2 image of pixels 0..5, as each orientation shows it:
    //   0 1 2
    //   3 4 5
    struct Case { unsigned orientation; uint32_t w, h; std::vector<unsigned> shown; };
    const Case cases[] = {
        {1, 3, 2, {0, 1, 2, 3, 4, 5}},
        {2, 3, 2, {2, 1, 0, 5, 4, 3}},   // mirrored left to right
        {3, 3, 2, {5, 4, 3, 2, 1, 0}},   // turned by 180 degrees
        {4, 3, 2, {3, 4, 5, 0, 1, 2}},   // mirrored top to bottom
        {5, 2, 3, {0, 3, 1, 4, 2, 5}},   // transposed
        {6, 2, 3, {3, 0, 4, 1, 5, 2}},   // turned clockwise
        {7, 2, 3, {5, 2, 4, 1, 3, 0}},   // transposed across the other diagonal
        {8, 2, 3, {2, 5, 1, 4, 0, 3}},   // turned counterclockwise
    };
    for (pixel_format f : AllFormats) {
        const size_t b = codec::detail::bytes_per_pixel(f);
        codec::image picture(3, 2, f);
        for (size_t i = 0; i < 6; ++i) {
            for (size_t k = 0; k < b; ++k) {
                picture.pixels()[i * b + k] = static_cast<std::byte>(i * 16 + k);
            }
        }
        for (const auto& c : cases) {
            codec::detail::ImageAccess::set_orientation(picture, c.orientation);
            codec::image shown = picture.oriented();
            ASSERT_EQ(shown.width(), c.w) << c.orientation;
            ASSERT_EQ(shown.height(), c.h) << c.orientation;
            ASSERT_EQ(shown.format(), f);
            for (size_t i = 0; i < 6; ++i) {
                for (size_t k = 0; k < b; ++k) {
                    ASSERT_EQ(shown.pixels()[i * b + k], static_cast<std::byte>(c.shown[i] * 16 + k))
                        << "orientation " << c.orientation << " format " << int(f) << " pixel " << i;
                }
            }
        }
    }
}

TEST(CodecImage_Tests, OrientationsCompose) {
    // on a larger image: 3 is 6 twice, 8 is 6 three times, 2 twice is 1,
    // 5 is 2 after 6, 7 is 2 after 8, 4 is 2 after 3
    auto turn = [](const codec::image& im, unsigned o) {
        codec::image copy = im.clone();
        codec::detail::ImageAccess::set_orientation(copy, o);
        return copy.oriented();
    };
    for (pixel_format f : {pixel_format::gray8, pixel_format::rgb8, pixel_format::rgba16}) {
        codec::image picture(29, 17, f);
        fill_random(picture, 99);
        EXPECT_TRUE(same_pixels(turn(picture, 3), turn(turn(picture, 6), 6)));
        EXPECT_TRUE(same_pixels(turn(picture, 8), turn(turn(turn(picture, 6), 6), 6)));
        EXPECT_TRUE(same_pixels(turn(turn(picture, 2), 2), picture));
        EXPECT_TRUE(same_pixels(turn(picture, 5), turn(turn(picture, 6), 2)));
        EXPECT_TRUE(same_pixels(turn(picture, 7), turn(turn(picture, 8), 2)));
        EXPECT_TRUE(same_pixels(turn(picture, 4), turn(turn(picture, 3), 2)));
        EXPECT_TRUE(same_pixels(turn(turn(picture, 6), 8), picture));
        EXPECT_TRUE(same_pixels(turn(turn(picture, 5), 5), picture));
        EXPECT_TRUE(same_pixels(turn(turn(picture, 7), 7), picture));
    }
    // a single row and a single column
    codec::image line(5, 1, pixel_format::gray8);
    fill_random(line, 3);
    EXPECT_EQ(turn(line, 6).width(), 1u);
    EXPECT_EQ(turn(line, 6).height(), 5u);
    EXPECT_TRUE(same_pixels(turn(turn(line, 6), 8), line));
}

TEST(CodecImage_Tests, AnImageIsTwoObjects) {
    // the state and the pixels; metadata adds a buffer each
    EXPECT_DOUBLE_EQ(objects_per([] { return codec::image(16, 16, pixel_format::rgba8); }), 2.0);
    EXPECT_DOUBLE_EQ(objects_per([] {
        codec::image picture(4, 4, pixel_format::rgb8);
        codec::detail::ImageAccess::state(picture).exif.push_back(byte{1});
        return picture;
    }), 3.0);
}

TEST(CodecFormat_Tests, SniffBySignature) {
    auto sniff = [](const std::string& s) { return codec::sniff(codec_test::bytes(s)); };
    EXPECT_EQ(sniff(std::string("\x89PNG\r\n\x1a\n", 8)), codec::format::png);
    EXPECT_EQ(sniff("\xFF\xD8\xFF\xE0"), codec::format::jpeg);
    EXPECT_EQ(sniff("GIF87a"), codec::format::gif);
    EXPECT_EQ(sniff("GIF89a..."), codec::format::gif);
    EXPECT_EQ(sniff("RIFF\x10\x20\x30\x40WEBPVP8 "), codec::format::webp);
    EXPECT_EQ(sniff(std::string("\x89PNG\r\n\x1a", 7)), nullopt);   // too short to tell
    EXPECT_EQ(sniff("\xFF\xD8"), nullopt);
    EXPECT_EQ(sniff("GIF88a"), nullopt);
    EXPECT_EQ(sniff(std::string("RIFF\0\0\0\0WAVE", 12)), nullopt);
    EXPECT_EQ(sniff(std::string("RIFF\0\0\0\0WEB", 11)), nullopt);   // one short
    EXPECT_EQ(sniff(std::string("RIFF\0\0\0\0WEBP", 12)), codec::format::webp);
    EXPECT_EQ(sniff(""), nullopt);
    EXPECT_EQ(codec::sniff(slice<const byte>()), nullopt);
}

TEST(CodecFormat_Tests, SniffTheCorpora) {
    struct File { const char* path; codec::format f; };
    const File files[] = {
        {"pngsuite/basn0g01.png", codec::format::png},
        {"pngsuite/basi6a16.png", codec::format::png},
        {"libjpeg-turbo/libjpeg-turbo-3.2.0/testimages/testorig.jpg", codec::format::jpeg},
        {"libjpeg-turbo/libjpeg-turbo-3.2.0/testimages/testimgari.jpg", codec::format::jpeg},
        {"giflib/giflib-6.1.3/pic/porsche.gif", codec::format::gif},
        {"giflib/giflib-6.1.3/pic/treescap-interlaced.gif", codec::format::gif},
        {"libwebp-test-data/alpha_filter_1.webp", codec::format::webp},
    };
    for (const auto& file : files) {
        CODEC_ORACLE(data, file.path);
        EXPECT_EQ(codec::sniff(codec_test::bytes(data)), file.f) << file.path;
    }
    // what is none of them
    CODEC_ORACLE(ppm, "libjpeg-turbo/libjpeg-turbo-3.2.0/testimages/testorig.ppm");
    EXPECT_EQ(codec::sniff(codec_test::bytes(ppm)), nullopt);
}

TEST(CodecError_Tests, CodeOffsetAndMessage) {
    codec::error plain(codec::errc::checksum, 1234);
    EXPECT_EQ(plain.code(), codec::errc::checksum);
    EXPECT_EQ(plain.offset(), 1234u);
    EXPECT_FALSE(plain.io_error());
    EXPECT_EQ(plain.message(), "offset 1234: checksum mismatch");
    codec::error detailed(codec::errc::corrupt, 16, "png: IHDR width 0");
    EXPECT_EQ(detailed.message(), "offset 16: png: IHDR width 0");
    EXPECT_NE(plain, detailed);
    EXPECT_EQ(plain, codec::error(codec::errc::checksum, 1234));
    error_code ec = codec::errc::too_large;
    EXPECT_STREQ(ec.category().name(), "codec");
    EXPECT_EQ(ec.message(), "size limit exceeded");
    EXPECT_EQ(ec.value(), 5);
    codec::error failed(io::error(std::make_error_code(std::errc::io_error), "read", "photo.png"), 512);
    EXPECT_EQ(failed.code(), codec::errc::io);
    ASSERT_TRUE(failed.io_error());
    EXPECT_TRUE(failed.message().starts_with("offset 512: input/output error: "));
    // as a stream reports it
    EXPECT_EQ(codec::detail::to_io_error(plain, "png").code(), make_error_code(codec::errc::checksum));
    EXPECT_EQ(codec::detail::to_io_error(failed, "png"), *failed.io_error());
}

TEST(CodecLimits_Tests, TheSizeAFileClaims) {
    codec::limits l;
    EXPECT_EQ(l.max_pixels, 100'000'000u);
    EXPECT_EQ(l.max_metadata, size_t(64) << 20);
    EXPECT_FALSE(codec::detail::check_size(10'000, 10'000, l, 16));
    auto big = codec::detail::check_size(10'000, 10'001, l, 16);
    ASSERT_TRUE(big);
    EXPECT_EQ(big->code(), codec::errc::too_large);
    EXPECT_EQ(big->offset(), 16u);
    auto huge = codec::detail::check_size(0xFFFFFFFFu, 0xFFFFFFFFu, l, 0);   // no overflow in the product
    ASSERT_TRUE(huge);
    EXPECT_EQ(huge->code(), codec::errc::too_large);
    auto zero = codec::detail::check_size(0, 5, l, 16);
    ASSERT_TRUE(zero);
    EXPECT_EQ(zero->code(), codec::errc::corrupt);
    codec::decode_options o{.want = pixel_format::rgba8};
    EXPECT_EQ(o.want, pixel_format::rgba8);
    EXPECT_TRUE(o.metadata);
    EXPECT_EQ(o.limits.max_pixels, 100'000'000u);
}
