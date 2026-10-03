//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: the PNG decoder (png.h, detail/png_decoder.h). PngSuite
// (~/Programming/oracles/pngsuite) pixel for pixel against libpng
// (tools/codec_oracle.c) and Go's image/png (tools/codec_oracle.go), in
// the file's own format and converted to 8 bits (the nearest value, both
// oracles made to round the same way); its x*.png files each an error of
// the right code; memory and a stream in pieces alike; and files made here
// for what the suite has none of.
#include "oracle.h"
#include "png_builder.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;

namespace {
    // A stream handing out at most `step` bytes a read
    struct pieces {
        const std::string* data;
        size_t step;
        size_t at = 0;

        expected<size_t, io::error> read(const slice<std::byte>& b) {
            size_t n = std::min({step, b.size(), data->size() - at});
            std::memcpy(b.data(), data->data() + at, n);
            at += n;
            return n;
        }
    };

    // A stream that fails after `good` bytes
    struct failing {
        const std::string* data;
        size_t good;
        size_t at = 0;

        expected<size_t, io::error> read(const slice<std::byte>& b) {
            if (at >= good) {
                return unexpected(io::error(std::make_error_code(std::errc::io_error), "read", "test"));
            }
            size_t n = std::min({b.size(), good - at});
            std::memcpy(b.data(), data->data() + at, n);
            at += n;
            return n;
        }
    };

    std::vector<std::string> suite_files() {
        std::vector<std::string> out;
        auto dir = oracle_path("pngsuite");
        if (!std::filesystem::is_directory(dir)) {
            return out;
        }
        for (auto& e : std::filesystem::directory_iterator(dir)) {
            if (e.path().extension() == ".png") {
                out.push_back(e.path().filename().string());
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    bool same(const codec::image& a, const codec::image& b) {
        return a.width() == b.width() && a.height() == b.height() && a.format() == b.format() &&
               a.pixels().size() == b.pixels().size() && std::memcmp(a.pixels().data(), b.pixels().data(), a.pixels().size()) == 0;
    }

    codec::errc code_of(const expected<codec::image, codec::error>& r) {
        return r ? codec::errc{} : r.error().code();
    }

    std::string one_row_rgb(uint8_t r, uint8_t g, uint8_t b) {
        return std::string{0, char(r), char(g), char(b)};
    }
}

TEST(CodecPng_Tests, PngSuiteAgainstBothOracles) {
    auto files = suite_files();
    if (files.empty()) {
        GTEST_SKIP() << "no " << oracle_path("pngsuite");
    }
    struct Oracle { const char* name; std::string exe; };
    std::vector<Oracle> oracles;
    if (!c_oracle().empty()) {
        oracles.push_back({"libpng", c_oracle()});
    }
    if (!go_oracle().empty()) {
        oracles.push_back({"Go", go_oracle()});
    }
    if (oracles.empty()) {
        GTEST_SKIP() << "neither oracle builds (cc with libpng, go)";
    }
    size_t compared = 0;
    for (const auto& name : files) {
        if (name[0] == 'x') {
            continue;   // the negatives: CodecPng_Tests.PngSuiteNegatives
        }
        const std::string path = oracle_path("pngsuite/" + name);
        CODEC_ORACLE(data, "pngsuite/" + name);
        auto ours = codec::png::decode(bytes(data));
        ASSERT_TRUE(ours) << name << ": " << ours.error().message();
        auto ours8 = codec::png::decode(bytes(data), {.want = pixel_format::rgba8});
        ASSERT_TRUE(ours8) << name;
        for (const auto& o : oracles) {
            auto native = run_oracle(o.exe, "png", path);
            ASSERT_TRUE(native) << o.name << " refused " << name;
            EXPECT_EQ(ours->width(), native->width) << name;
            EXPECT_EQ(ours->height(), native->height) << name;
            EXPECT_TRUE(oracle_form(*ours, native->depth) == native->pixels) << name << " against " << o.name;
            auto eight = run_oracle(o.exe, "png8", path);
            ASSERT_TRUE(eight) << o.name << " refused " << name;
            EXPECT_EQ(ours8->format(), pixel_format::rgba8);
            EXPECT_TRUE(oracle_form(*ours8, 8) == eight->pixels) << name << " (want rgba8) against " << o.name;
            ++compared;
        }
    }
    EXPECT_GT(compared, 150u);
}

TEST(CodecPng_Tests, AStreamInPiecesIsMemory) {
    auto files = suite_files();
    if (files.empty()) {
        GTEST_SKIP() << "no " << oracle_path("pngsuite");
    }
    size_t step = 1;
    for (const auto& name : files) {
        CODEC_ORACLE(data, "pngsuite/" + name);
        auto whole = codec::png::decode(bytes(data));
        pieces p{&data, step};
        auto streamed = codec::png::decode(io::reader(p));
        step = step % 97 + 7;
        ASSERT_EQ(bool(whole), bool(streamed)) << name;
        if (whole) {
            EXPECT_TRUE(same(*whole, *streamed)) << name;
            EXPECT_EQ(whole->exif().size(), streamed->exif().size()) << name;
        } else {
            EXPECT_EQ(whole.error().code(), streamed.error().code()) << name;
        }
        // through codec::decode, both ways
        auto any = codec::decode(bytes(data));
        pieces q{&data, 4096};
        auto any_streamed = codec::decode(io::reader(q));
        ASSERT_EQ(bool(any), bool(whole)) << name;
        ASSERT_EQ(bool(any_streamed), bool(whole)) << name;
        if (whole) {
            EXPECT_TRUE(same(*any, *whole)) << name;
            EXPECT_TRUE(same(*any_streamed, *whole)) << name;
        }
    }
}

TEST(CodecPng_Tests, PngSuiteNegatives) {
    struct Case { const char* file; codec::errc code; };
    const Case cases[] = {
        {"xs1n0g01.png", codec::errc::corrupt},          // signature byte 1 wrong
        {"xs2n0g01.png", codec::errc::corrupt},          // signature byte 2 wrong
        {"xs4n0g01.png", codec::errc::corrupt},          // signature byte 4 wrong
        {"xs7n0g01.png", codec::errc::corrupt},          // 7-bit conversion of the signature
        {"xcrn0g04.png", codec::errc::corrupt},          // CR added (a transfer's conversion)
        {"xlfn0g04.png", codec::errc::corrupt},          // LF added
        {"xhdn0g08.png", codec::errc::checksum},         // IHDR's CRC wrong
        {"xc1n0g08.png", codec::errc::corrupt},          // color type 1
        {"xc9n2c08.png", codec::errc::corrupt},          // color type 9
        {"xd0n2c08.png", codec::errc::corrupt},          // bit depth 0
        {"xd3n2c08.png", codec::errc::corrupt},          // bit depth 3
        {"xd9n2c08.png", codec::errc::corrupt},          // bit depth 99
        {"xdtn0g01.png", codec::errc::corrupt},          // no IDAT
        {"xcsn0g01.png", codec::errc::checksum},         // IDAT's CRC wrong
    };
    for (const auto& c : cases) {
        CODEC_ORACLE(data, std::string("pngsuite/") + c.file);
        auto r = codec::png::decode(bytes(data));
        ASSERT_FALSE(r) << c.file;
        EXPECT_EQ(r.error().code(), c.code) << c.file << ": " << r.error().message();
        pieces p{&data, 3};
        auto s = codec::png::decode(io::reader(p));
        ASSERT_FALSE(s) << c.file;
        EXPECT_EQ(s.error().code(), c.code) << c.file;
        if (!c_oracle().empty()) {
            EXPECT_FALSE(run_oracle(c_oracle(), "png", oracle_path(std::string("pngsuite/") + c.file))) << "libpng takes " << c.file;
        }
    }
    // with a signature that is not PNG's, codec::decode knows no format
    CODEC_ORACLE(bad, "pngsuite/xs1n0g01.png");
    EXPECT_EQ(code_of(codec::decode(bytes(bad))), codec::errc::unsupported);
}

TEST(CodecPng_Tests, TheNativeFormats) {
    struct Case { const char* file; pixel_format f; };
    const Case cases[] = {
        {"basn0g01.png", pixel_format::gray8}, {"basn0g04.png", pixel_format::gray8},
        {"basn0g16.png", pixel_format::gray16}, {"basn2c08.png", pixel_format::rgb8},
        {"basn2c16.png", pixel_format::rgb16}, {"basn3p02.png", pixel_format::rgba8},
        {"basn4a08.png", pixel_format::gray_alpha8}, {"basn4a16.png", pixel_format::gray_alpha16},
        {"basn6a08.png", pixel_format::rgba8}, {"basn6a16.png", pixel_format::rgba16},
        {"tbbn0g04.png", pixel_format::gray_alpha8}, {"tbwn0g16.png", pixel_format::gray_alpha16},
        {"tbrn2c08.png", pixel_format::rgba8}, {"tbbn2c16.png", pixel_format::rgba16},
        {"basi0g16.png", pixel_format::gray16}, {"basi3p08.png", pixel_format::rgba8},
    };
    for (const auto& c : cases) {
        CODEC_ORACLE(data, std::string("pngsuite/") + c.file);
        auto r = codec::png::decode(bytes(data));
        ASSERT_TRUE(r) << c.file;
        EXPECT_EQ(r->format(), c.f) << c.file;
        EXPECT_EQ(r->width(), 32u) << c.file;
        EXPECT_EQ(r->height(), 32u) << c.file;
    }
    // every format asked for: the native image converted
    CODEC_ORACLE(data, "pngsuite/basi6a16.png");
    codec::image native = *codec::png::decode(bytes(data));
    for (int f = 0; f < 9; ++f) {
        auto want = static_cast<pixel_format>(f);
        auto r = codec::png::decode(bytes(data), {.want = want});
        ASSERT_TRUE(r);
        EXPECT_TRUE(same(*r, native.convert(want))) << f;
    }
}

TEST(CodecPng_Tests, Metadata) {
    CODEC_ORACLE(data, "pngsuite/exif2c08.png");
    codec::image photo = *codec::png::decode(bytes(data));
    EXPECT_FALSE(photo.exif().empty());
    EXPECT_TRUE(photo.exif()[0] == byte{'M'} || photo.exif()[0] == byte{'I'});
    codec::image bare = *codec::png::decode(bytes(data), {.metadata = false});
    EXPECT_TRUE(bare.exif().empty());
    auto small = codec::png::decode(bytes(data), {.limits = {.max_metadata = 16}});
    EXPECT_EQ(code_of(small), codec::errc::too_large);

    // an orientation, big- and little-endian
    const std::string be = std::string("MM\0*\0\0\0\x08\0\x01", 10) + std::string("\x01\x12\0\x03\0\0\0\x01\0\x06\0\0", 12) + std::string(4, '\0');
    const std::string le = std::string("II*\0\x08\0\0\0\x01\0", 10) + std::string("\x12\x01\x03\0\x01\0\0\0\x08\0\0\0", 12) + std::string(4, '\0');
    for (auto [block, o] : {std::pair{be, 6u}, std::pair{le, 8u}}) {
        auto file = png_file(1, 1, 8, 2, one_row_rgb(1, 2, 3), png_chunk("eXIf", block));
        codec::image turned = *codec::png::decode(bytes(file));
        EXPECT_EQ(turned.orientation(), o);
        EXPECT_EQ(turned.exif().size(), block.size());
    }

    // an ICC profile, compressed
    const std::string profile = "a profile of no color at all, " + std::string(300, 'x');
    auto file = png_file(1, 1, 8, 2, one_row_rgb(1, 2, 3), png_chunk("iCCP", std::string("test", 4) + std::string(2, '\0') + zlib_of(profile)));
    codec::image with_icc = *codec::png::decode(bytes(file));
    ASSERT_EQ(with_icc.icc().size(), profile.size());
    EXPECT_EQ(std::memcmp(with_icc.icc().data(), profile.data(), profile.size()), 0);
    EXPECT_EQ(code_of(codec::png::decode(bytes(file), {.limits = {.max_metadata = 100}})), codec::errc::too_large);
    // a profile that does not decompress is dropped, the image kept
    auto broken = png_file(1, 1, 8, 2, one_row_rgb(1, 2, 3), png_chunk("iCCP", std::string("test", 4) + std::string(2, '\0') + "garbage"));
    codec::image without = *codec::png::decode(bytes(broken));
    EXPECT_TRUE(without.icc().empty());
}

TEST(CodecPng_Tests, Limits) {
    auto file = png_signature() + png_ihdr(20000, 20000, 8, 6) + png_chunk("IEND", "");
    auto r = codec::png::decode(bytes(file));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), codec::errc::too_large);
    EXPECT_EQ(r.error().offset(), 8u);
    auto small = png_file(4, 4, 8, 0, random_rows(4, 4, 1));
    EXPECT_TRUE(codec::png::decode(bytes(small), {.limits = {.max_pixels = 16}}));
    EXPECT_EQ(code_of(codec::png::decode(bytes(small), {.limits = {.max_pixels = 15}})), codec::errc::too_large);
    EXPECT_EQ(code_of(codec::png::decode(bytes(small), {.want = static_cast<pixel_format>(20)})), codec::errc::invalid_argument);
}

TEST(CodecPng_Tests, ASizePastAnAddressIsTooLarge) {
    // sides of 2^31 - 1, which PNG allows, with the limit lifted: too_large at
    // IHDR, not the image's length_error, from memory, a stream and decode
    static_assert(noexcept(codec::png::decode(slice<const byte>())));
    static_assert(noexcept(codec::png::decode(slice<const byte>(), codec::decode_options())));
    const auto file = png_signature() + png_ihdr(0x7FFFFFFF, 0x7FFFFFFF, 8, 6) + png_chunk("IDAT", zlib_of(std::string(1, '\0'))) + png_chunk("IEND", "");
    const codec::decode_options o{.limits = {.max_pixels = UINT64_MAX}};
    auto r = codec::png::decode(bytes(file), o);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), codec::errc::too_large);
    EXPECT_EQ(r.error().offset(), 8u);
    pieces p{&file, 7};
    EXPECT_EQ(code_of(codec::png::decode(io::reader(p), o)), codec::errc::too_large);
    EXPECT_EQ(code_of(codec::decode(bytes(file), o)), codec::errc::too_large);
}

TEST(CodecPng_Tests, EveryCutIsUnexpectedEnd) {
    CODEC_ORACLE(data, "pngsuite/basi2c08.png");
    for (size_t n = 0; n < data.size(); ++n) {
        const std::string cut = data.substr(0, n);
        auto r = codec::png::decode(bytes(cut));
        ASSERT_FALSE(r) << n;
        EXPECT_EQ(r.error().code(), codec::errc::unexpected_end) << n << ": " << r.error().message();
        pieces p{&cut, 5};
        auto s = codec::png::decode(io::reader(p));
        ASSERT_FALSE(s) << n;
        EXPECT_EQ(s.error().code(), codec::errc::unexpected_end) << n;
    }
}

TEST(CodecPng_Tests, AStreamThatFails) {
    CODEC_ORACLE(data, "pngsuite/basn2c08.png");
    failing f{&data, 100};
    auto r = codec::png::decode(io::reader(f));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), codec::errc::io);
    ASSERT_TRUE(r.error().io_error());
    EXPECT_EQ(r.error().offset(), 100u);
    failing g{&data, 5};
    EXPECT_EQ(code_of(codec::decode(io::reader(g))), codec::errc::io);
}

TEST(CodecPng_Tests, ChunksOutOfPlaceAndChecksums) {
    const std::string rows = random_rows(3, 9, 7);
    const std::string idat = png_chunk("IDAT", zlib_of(rows));
    const std::string head = png_signature() + png_ihdr(3, 3, 8, 2);
    const std::string end = png_chunk("IEND", "");
    EXPECT_TRUE(codec::png::decode(bytes(head + idat + end)));
    // an unknown ancillary chunk passed over, an unknown critical one refused
    EXPECT_TRUE(codec::png::decode(bytes(head + png_chunk("zzZz", "anything") + idat + png_chunk("abCD", "") + end)));
    EXPECT_EQ(code_of(codec::png::decode(bytes(head + png_chunk("ABCD", "x") + idat + end))), codec::errc::unsupported);
    // IHDR first, IDATs together, PLTE before them, one IEND at the end
    EXPECT_EQ(code_of(codec::png::decode(bytes(png_signature() + idat + png_ihdr(3, 3, 8, 2) + end))), codec::errc::corrupt);
    auto z = zlib_of(rows);
    auto split = png_chunk("IDAT", z.substr(0, 10)) + png_chunk("tEXt", "a\0b") + png_chunk("IDAT", z.substr(10));
    EXPECT_EQ(code_of(codec::png::decode(bytes(head + split + end))), codec::errc::corrupt);
    auto together = png_chunk("IDAT", z.substr(0, 10)) + png_chunk("IDAT", "") + png_chunk("IDAT", z.substr(10));
    EXPECT_TRUE(codec::png::decode(bytes(head + together + end)));
    EXPECT_EQ(code_of(codec::png::decode(bytes(head + idat + png_chunk("PLTE", "abc") + end))), codec::errc::corrupt);
    EXPECT_EQ(code_of(codec::png::decode(bytes(head + idat))), codec::errc::unexpected_end);
    EXPECT_EQ(code_of(codec::png::decode(bytes(head + png_chunk("1DAT", "") + idat + end))), codec::errc::corrupt);
    // the Adler-32 of the image data, and a chunk's CRC
    auto bad_adler = z;
    bad_adler.back() ^= 1;
    EXPECT_EQ(code_of(codec::png::decode(bytes(head + png_chunk("IDAT", bad_adler) + end))), codec::errc::checksum);
    auto bad_crc = head + idat + end;
    bad_crc[bad_crc.size() - 13] ^= 1;   // the last byte of IDAT's CRC
    EXPECT_EQ(code_of(codec::png::decode(bytes(bad_crc))), codec::errc::checksum);
    // a filter type past 4
    auto bad_filter = rows;
    bad_filter[10] = 5;
    EXPECT_EQ(code_of(codec::png::decode(bytes(png_file(3, 3, 8, 2, bad_filter)))), codec::errc::corrupt);
    // too few rows, and rows past the image (ignored, as libpng does)
    EXPECT_EQ(code_of(codec::png::decode(bytes(png_file(3, 3, 8, 2, rows.substr(0, 20))))), codec::errc::corrupt);
    codec::image extra = *codec::png::decode(bytes(png_file(3, 3, 8, 2, rows + random_rows(2, 9, 8))));
    EXPECT_TRUE(same(extra, *codec::png::decode(bytes(head + idat + end))));
    // bytes after the zlib stream, inside IDAT
    EXPECT_TRUE(codec::png::decode(bytes(head + png_chunk("IDAT", z + "tail") + end)));
    // a zlib stream with a preset dictionary
    auto dict = z;
    dict[1] = char(0x20 | ((31 - ((0x78 * 256 + 0x20) % 31)) % 31));
    dict[0] = 0x78;
    EXPECT_EQ(code_of(codec::png::decode(bytes(head + png_chunk("IDAT", dict) + end))), codec::errc::corrupt);
}

TEST(CodecPng_Tests, PaletteAndTransparency) {
    // four entries, two of them with alpha; index 7 past the palette:
    // opaque black
    const std::string plte = png_chunk("PLTE", std::string("\x10\x20\x30\x40\x50\x60\x70\x80\x90\xA0\xB0\xC0", 12));
    const std::string trns = png_chunk("tRNS", std::string("\x00\x80", 2));
    const std::string rows = std::string{0, char(0x01), char(0x27)};   // 4 bits: 0, 1, 2, 7
    auto file = png_file(4, 1, 4, 3, rows, plte + trns);
    codec::image colors = *codec::png::decode(bytes(file));
    const unsigned char expected[] = {0x10, 0x20, 0x30, 0x00, 0x40, 0x50, 0x60, 0x80, 0x70, 0x80, 0x90, 0xFF, 0, 0, 0, 0xFF};
    ASSERT_EQ(colors.pixels().size(), sizeof(expected));
    EXPECT_EQ(std::memcmp(colors.pixels().data(), expected, sizeof(expected)), 0);
    // indexed with no palette; a tRNS longer than the palette ignored;
    // tRNS in an image with alpha ignored
    EXPECT_EQ(code_of(codec::png::decode(bytes(png_file(4, 1, 4, 3, rows)))), codec::errc::corrupt);
    auto long_trns = png_file(4, 1, 4, 3, rows, plte + png_chunk("tRNS", std::string(5, '\0')));
    codec::image opaque = *codec::png::decode(bytes(long_trns));
    EXPECT_EQ(opaque.pixels()[3], byte{0xFF});
    auto rgba = png_file(1, 1, 8, 6, std::string{0, 1, 2, 3, 4}, png_chunk("tRNS", std::string(6, '\0')));
    EXPECT_TRUE(codec::png::decode(bytes(rgba)));
    // a gray key of 16 bits
    auto gray = png_file(2, 1, 16, 0, std::string{0, 0x12, 0x34, 0x12, 0x35}, png_chunk("tRNS", std::string("\x12\x34", 2)));
    codec::image keyed = *codec::png::decode(bytes(gray));
    ASSERT_EQ(keyed.format(), pixel_format::gray_alpha16);
    uint16_t v[4];
    std::memcpy(v, keyed.pixels().data(), 8);
    EXPECT_EQ(v[0], 0x1234);
    EXPECT_EQ(v[1], 0);
    EXPECT_EQ(v[2], 0x1235);
    EXPECT_EQ(v[3], 0xFFFF);
}

TEST(CodecPng_Tests, ImagesMadeHere) {
    // every depth of each color type, plain and interlaced, sizes around
    // the passes of Adam7: the oracles read them as we do
    if (c_oracle().empty()) {
        GTEST_SKIP() << "no C oracle (cc with libpng)";
    }
    struct Type { int color; std::vector<int> depths; unsigned channels; };
    const Type types[] = {{0, {1, 2, 4, 8, 16}, 1}, {2, {8, 16}, 3}, {3, {1, 2, 4, 8}, 1}, {4, {8, 16}, 2}, {6, {8, 16}, 4}};
    const auto dir = scratch_path("sgcl_codec_png_tests");
    uint32_t seed = 1;
    for (const auto& t : types) {
        for (int depth : t.depths) {
            for (auto [w, h] : {std::pair{1u, 1u}, std::pair{9u, 3u}, std::pair{17u, 11u}, std::pair{33u, 2u}}) {
                for (int interlace : {0, 1}) {
                    const unsigned bits = t.channels * depth;
                    std::string plte;
                    if (t.color == 3) {
                        std::string entries;
                        for (int i = 0; i < (1 << depth); ++i) {
                            entries += std::string{char(i * 7), char(i * 13), char(255 - i)};
                        }
                        plte = png_chunk("PLTE", entries);
                    }
                    const std::string rows = interlace ? random_interlaced_rows(w, h, bits, seed) : random_rows(h, (size_t(w) * bits + 7) / 8, seed);
                    ++seed;
                    auto file = png_file(w, h, depth, t.color, rows, plte, interlace);
                    auto path = (dir / "made.png").string();
                    {
                        std::ofstream out(path, std::ios::binary);
                        out << file;
                    }
                    auto ours = codec::png::decode(bytes(file));
                    ASSERT_TRUE(ours) << t.color << "/" << depth << " " << w << "x" << h << " " << ours.error().message();
                    auto oracle = run_oracle(c_oracle(), "png", path);
                    ASSERT_TRUE(oracle);
                    EXPECT_TRUE(oracle_form(*ours, oracle->depth) == oracle->pixels)
                        << "color " << t.color << " depth " << depth << " " << w << "x" << h << " interlace " << interlace;
                }
            }
        }
    }
}

TEST(CodecPng_Tests, UnknownAndBroken) {
    // a WebP header of RIFF size 0: WebP is read (webp.h), and this is corrupt
    const std::string webp("RIFF\0\0\0\0WEBPVP8 ", 16);
    EXPECT_EQ(code_of(codec::decode(bytes(webp))), codec::errc::corrupt);
    EXPECT_EQ(code_of(codec::decode(bytes(std::string("hello, world")))), codec::errc::unsupported);
    EXPECT_EQ(code_of(codec::decode(bytes(std::string()))), codec::errc::unsupported);
    const std::string empty;
    pieces p{&empty, 1};
    EXPECT_EQ(code_of(codec::decode(io::reader(p))), codec::errc::unsupported);
}

TEST(CodecPng_Tests, TheExampleOfTheDocs) {
    CODEC_ORACLE(data, "pngsuite/basn2c08.png");
    vector<byte> file(bytes(data).begin(), bytes(data).end());
    codec::image photo = codec::decode(file, {.want = codec::pixel_format::rgba8});
    EXPECT_EQ(photo.width(), 32u);
    EXPECT_EQ(photo.format(), codec::pixel_format::rgba8);
    EXPECT_EQ(photo.row(0).size(), 32u * 4);
}
