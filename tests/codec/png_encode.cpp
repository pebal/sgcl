//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: the PNG encoder (png.h, detail/png_encoder.h). What it writes
// decodes to the same pixels through the module's decoder, libpng and Go
// (tools/codec_oracle.c and .go), for every pixel format, size and level;
// PngSuite decoded and written again reads back the same; the size of its
// files within 5 % of libpng's at the same zlib level (the measure of its
// filters); EXIF and the ICC profile carried; a stream alike and a stream
// that fails. The pixels of every image are written explicitly, none left
// to the constructor's zeros.
#include "oracle.h"
#include "png_builder.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;

namespace {
    constexpr pixel_format AllFormats[] = {
        pixel_format::gray8, pixel_format::gray_alpha8, pixel_format::rgb8, pixel_format::rgba8,
        pixel_format::gray16, pixel_format::gray_alpha16, pixel_format::rgb16, pixel_format::rgba16,
        pixel_format::cmyk8
    };

    // Pixels of some structure (gradients, a band of noise, flat runs), so
    // that every filter wins somewhere and the stream has matches to find
    void fill(codec::image& im, uint32_t seed) {
        std::mt19937 rng(seed);
        const size_t stride = im.stride();
        for (uint32_t y = 0; y < im.height(); ++y) {
            auto row = im.row(y);
            for (size_t i = 0; i < stride; ++i) {
                uint8_t v;
                switch ((y / 3 + seed) % 4) {
                    case 0: v = uint8_t(i * 3 + y); break;
                    case 1: v = uint8_t(rng()); break;
                    case 2: v = uint8_t((i / 7) * 40); break;
                    default: v = uint8_t(y * 5 + (rng() & 3)); break;
                }
                row[i] = std::byte{v};
            }
        }
    }

    bool same(const codec::image& a, const codec::image& b) {
        return a.width() == b.width() && a.height() == b.height() && a.format() == b.format() &&
               std::memcmp(a.pixels().data(), b.pixels().data(), a.pixels().size()) == 0;
    }

    std::string text(const vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    // What the decoder makes of a file written from an image of format f
    pixel_format read_back(pixel_format f) {
        return f == pixel_format::cmyk8 ? pixel_format::rgb8 : f;
    }

    std::filesystem::path scratch_dir() {
        return scratch_path("sgcl_codec_png_encode_tests");
    }

    std::string write_file(const std::string& name, const std::string& data) {
        auto path = (scratch_dir() / name).string();
        std::ofstream out(path, std::ios::binary);
        out << data;
        return path;
    }

    // The rows of an image as PNG has them (16-bit samples big-endian), for
    // libpng's encoder
    std::string png_layout(const codec::image& im) {
        auto px = im.pixels();
        std::string out(reinterpret_cast<const char*>(px.data()), px.size());
        if (codec::detail::wide(im.format())) {
            for (size_t i = 0; i + 1 < out.size(); i += 2) {
                std::swap(out[i], out[i + 1]);   // this machine is little-endian (arm64, x86-64)
            }
        }
        return out;
    }

    int color_type(pixel_format f) {
        switch (f) {
            case pixel_format::gray8: case pixel_format::gray16: return 0;
            case pixel_format::gray_alpha8: case pixel_format::gray_alpha16: return 4;
            case pixel_format::rgb8: case pixel_format::rgb16: return 2;
            default: return 6;
        }
    }

    // libpng's file of the image at the level: its size
    std::optional<size_t> libpng_size(const codec::image& im, int level, const std::string& out_name) {
        auto raw = write_file("raw.bin", png_layout(im));
        auto out = (scratch_dir() / out_name).string();
        std::string cmd = "'" + c_oracle() + "' pngenc " + std::to_string(level) + " " + std::to_string(im.width()) + " " +
                          std::to_string(im.height()) + " " + std::to_string(color_type(im.format())) + " " +
                          std::to_string(codec::detail::wide(im.format()) ? 16 : 8) + " '" + raw + "' '" + out + "' 2>/dev/null";
        if (std::system(cmd.c_str()) != 0) {
            return std::nullopt;
        }
        return size_t(std::filesystem::file_size(out));
    }

    // A PPM (P6, 255) as an rgb8 image
    std::optional<codec::image> read_ppm(const std::string& data) {
        unsigned w, h, maxval;
        int consumed = 0;
        if (std::sscanf(data.c_str(), "P6 %u %u %u%n", &w, &h, &maxval, &consumed) != 3 || maxval != 255) {
            return std::nullopt;
        }
        const size_t start = size_t(consumed) + 1;
        if (data.size() < start + size_t(w) * h * 3) {
            return std::nullopt;
        }
        codec::image im(w, h, pixel_format::rgb8);
        std::memcpy(im.pixels().data(), data.data() + start, size_t(w) * h * 3);
        return im;
    }

    struct failing_writer {
        size_t room;
        size_t took = 0;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            if (took + b.size() > room) {
                return unexpected(io::error(std::make_error_code(std::errc::no_space_on_device), "write", "test"));
            }
            took += b.size();
            return b.size();
        }
    };
}

TEST(CodecPngEncode_Tests, RoundTripEveryFormatSizeAndLevel) {
    uint32_t seed = 1;
    for (pixel_format f : AllFormats) {
        for (auto [w, h] : {std::pair{1u, 1u}, std::pair{7u, 5u}, std::pair{64u, 33u}, std::pair{300u, 2u}}) {
            for (int level : {0, 1, 6, 9, compress::level::huffman_only}) {
                codec::image picture(w, h, f);
                fill(picture, seed++);
                vector<byte> file = codec::png::encode(picture, {.level = level});
                auto back = codec::png::decode(file);
                ASSERT_TRUE(back) << int(f) << " " << w << "x" << h << " level " << level << ": " << back.error().message();
                EXPECT_TRUE(same(*back, picture.convert(read_back(f)))) << int(f) << " " << w << "x" << h << " level " << level;
            }
        }
    }
}

TEST(CodecPngEncode_Tests, TheOraclesReadWhatItWrites) {
    std::vector<std::pair<const char*, std::string>> oracles;
    if (!c_oracle().empty()) {
        oracles.push_back({"libpng", c_oracle()});
    }
    if (!go_oracle().empty()) {
        oracles.push_back({"Go", go_oracle()});
    }
    if (oracles.empty()) {
        GTEST_SKIP() << "neither oracle builds (cc with libpng, go)";
    }
    uint32_t seed = 100;
    for (pixel_format f : AllFormats) {
        for (auto [w, h] : {std::pair{1u, 1u}, std::pair{13u, 9u}, std::pair{64u, 40u}}) {
            codec::image picture(w, h, f);
            fill(picture, seed++);
            auto path = write_file("ours.png", text(codec::png::encode(picture)));
            codec::image expected_pixels = picture.convert(read_back(f));
            for (const auto& [name, exe] : oracles) {
                auto o = run_oracle(exe, "png", path);
                ASSERT_TRUE(o) << name << " refused format " << int(f);
                EXPECT_TRUE(oracle_form(expected_pixels, o->depth) == o->pixels) << name << " format " << int(f) << " " << w << "x" << h;
            }
        }
    }
}

TEST(CodecPngEncode_Tests, PngSuiteWrittenAgain) {
    auto dir = oracle_path("pngsuite");
    if (!std::filesystem::is_directory(dir)) {
        GTEST_SKIP() << "no " << dir;
    }
    size_t files = 0;
    for (auto& e : std::filesystem::directory_iterator(dir)) {
        const auto name = e.path().filename().string();
        if (e.path().extension() != ".png" || name[0] == 'x') {
            continue;
        }
        CODEC_ORACLE(data, "pngsuite/" + name);
        codec::image original = *codec::png::decode(bytes(data));
        const std::string written = text(codec::png::encode(original));
        codec::image back = *codec::png::decode(bytes(written));
        EXPECT_TRUE(same(back, original)) << name;
        EXPECT_EQ(back.exif().size(), original.exif().size()) << name;
        if (!c_oracle().empty()) {
            auto path = write_file("suite.png", written);
            auto o = run_oracle(c_oracle(), "png", path);
            ASSERT_TRUE(o) << name;
            EXPECT_TRUE(oracle_form(original, o->depth) == o->pixels) << name;
        }
        ++files;
    }
    EXPECT_GT(files, 150u);
}

TEST(CodecPngEncode_Tests, SizeWithinFivePercentOfLibpng) {
    if (c_oracle().empty()) {
        GTEST_SKIP() << "no C oracle (cc with libpng)";
    }
    // the images in a managed vector (a handle lives on a stack or in a
    // managed object), their names beside them
    sgcl::vector<codec::image> images;
    std::vector<std::string> names;
    const std::string testimages = "libjpeg-turbo/libjpeg-turbo-3.2.0/testimages/";
    if (auto ppm = read_oracle(testimages + "testorig.ppm")) {
        if (auto im = read_ppm(*ppm)) {
            images.push_back(*im);
            names.push_back("testorig.ppm (rgb8)");
        }
    }
    for (const char* name : {"monkey16.png", "testorig.png", "vgl_5674_0098.png"}) {
        if (auto data = read_oracle(testimages + name)) {
            // the pixels only: libpng is given no metadata, so neither is ours
            // (these files carry an ICC profile of some 2.6 KB)
            images.push_back(*codec::png::decode(bytes(*data), {.metadata = false}));
            names.push_back(name);
        }
    }
    if (images.empty()) {
        GTEST_SKIP() << "no " << oracle_path(testimages);
    }
    for (int level : {1, 6, 7, 8, 9}) {
        for (size_t i = 0; i < images.size(); ++i) {
            const codec::image& im = images[i];
            const std::string& name = names[i];
            // both files kept in the scratch directory, for a look at them
            const std::string tag = std::to_string(i) + "_" + std::to_string(level) + ".png";
            const std::string written = text(codec::png::encode(im, {.level = level}));
            write_file("ours_" + tag, written);
            const size_t ours = written.size();
            auto theirs = libpng_size(im, level, "libpng_" + tag);
            ASSERT_TRUE(theirs) << name;
            const double ratio = double(ours) / double(*theirs);
            std::printf("  %-22s level %d: ours %7zu, libpng %7zu, %+.2f %%\n", name.c_str(), level, ours, *theirs, (ratio - 1) * 100);
            EXPECT_LE(std::fabs(ratio - 1), 0.05) << name << " level " << level;
        }
    }
}

TEST(CodecPngEncode_Tests, MetadataCarried) {
    codec::image photo(5, 4, pixel_format::rgb8);
    fill(photo, 7);
    auto& s = codec::detail::ImageAccess::state(photo);
    const std::string exif = std::string("MM\0*\0\0\0\x08\0\x01", 10) + std::string("\x01\x12\0\x03\0\0\0\x01\0\x06\0\0", 12) + std::string(4, '\0');
    const std::string icc = "a profile, " + std::string(500, 'p');
    for (char c : exif) {
        s.exif.push_back(byte(c));
    }
    for (char c : icc) {
        s.icc.push_back(byte(c));
    }
    codec::image back = *codec::png::decode(codec::png::encode(photo));
    EXPECT_EQ(back.orientation(), 6u);
    ASSERT_EQ(back.exif().size(), exif.size());
    EXPECT_EQ(std::memcmp(back.exif().data(), exif.data(), exif.size()), 0);
    ASSERT_EQ(back.icc().size(), icc.size());
    EXPECT_EQ(std::memcmp(back.icc().data(), icc.data(), icc.size()), 0);
    if (!c_oracle().empty()) {
        EXPECT_TRUE(run_oracle(c_oracle(), "png", write_file("meta.png", text(codec::png::encode(photo)))));
    }
}

TEST(CodecPngEncode_Tests, AStreamAndAStreamThatFails) {
    codec::image picture(120, 80, pixel_format::rgba8);
    fill(picture, 3);
    const std::string whole = text(codec::png::encode(picture, {.level = 9}));
    io::buffer out;
    ASSERT_TRUE(codec::png::encode(picture, out, {.level = 9}));
    auto held = out.data();
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(held.data()), held.size()), whole);
    failing_writer f{100};
    auto r = codec::png::encode(picture, io::writer(f));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), codec::errc::io);
    ASSERT_TRUE(r.error().io_error());
    EXPECT_LE(r.error().offset(), 100u);
    // more than one IDAT chunk: an image past 64 KB compressed
    codec::image noise(300, 300, pixel_format::rgb8);
    std::mt19937 rng(5);
    for (auto& b : noise.pixels()) {
        b = byte(rng());
    }
    auto big = codec::png::encode(noise);
    EXPECT_GT(big.size(), 3u * 65536);
    EXPECT_TRUE(same(*codec::png::decode(big), noise));
}

TEST(CodecPngEncode_Tests, TheFiltersOneByOne) {
    // each filter undone by the decoder's unfilter, on rows of every bpp
    std::mt19937 rng(11);
    for (unsigned bpp : {1u, 2u, 3u, 4u, 6u, 8u}) {
        for (size_t n : {size_t(1), size_t(bpp), size_t(bpp) * 9 + 1}) {
            std::vector<uint8_t> prior(n), x(n), filtered(n), back(n);
            for (auto& v : prior) {
                v = uint8_t(rng());
            }
            for (auto& v : x) {
                v = uint8_t(rng());
            }
            for (uint8_t f = 0; f <= 4; ++f) {
                codec::detail::filter(f, x.data(), prior.data(), filtered.data(), n, bpp);
                codec::detail::unfilter(f, filtered.data(), prior.data(), back.data(), n, bpp);
                ASSERT_EQ(back, x) << "filter " << int(f) << " bpp " << bpp << " n " << n;
            }
        }
    }
}
