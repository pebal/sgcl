//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: the JPEG encoder (jpeg.h, detail/jpeg_encoder.h). Byte for byte
// the file libjpeg-turbo's cjpeg writes (`-dct int -baseline`, the same
// quality, sampling and -optimize) from the same pixels: cuts of
// testorig.ppm of every size around the blocks and MCUs, every quality
// band, and PngSuite decoded; so its size is cjpeg's. What it writes
// decodes through the module and through libjpeg; EXIF and ICC carried; a
// stream alike and a stream that fails.
#include "oracle.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <vector>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;

namespace {
    const std::string TestImages = "libjpeg-turbo/libjpeg-turbo-3.2.0/testimages/";

    std::filesystem::path scratch_dir() {
        return scratch_path("sgcl_codec_jpeg_encode_tests");
    }

    std::string write_file(const std::string& name, const std::string& data) {
        auto path = (scratch_dir() / name).string();
        std::ofstream out(path, std::ios::binary);
        out << data;
        return path;
    }

    std::string read_file(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    std::string tool(const char* name) {
        for (const std::string dir : {"/opt/homebrew/opt/jpeg-turbo/bin/", "/opt/homebrew/bin/", "/usr/local/bin/"}) {
            if (std::filesystem::exists(dir + name)) {
                return dir + name;
            }
        }
        return "";
    }

    std::string text(const vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    bool is_gray(pixel_format f) {
        return f == pixel_format::gray8 || f == pixel_format::gray16 || f == pixel_format::gray_alpha8 || f == pixel_format::gray_alpha16;
    }

    // The image as cjpeg reads it: a PGM of its gray8 conversion, or a PPM
    // of its rgb8 one (what the encoder encodes)
    std::string as_pnm(const codec::image& im) {
        const bool gray = is_gray(im.format());
        codec::image p = im.convert(gray ? pixel_format::gray8 : pixel_format::rgb8);
        std::string out = std::string(gray ? "P5\n" : "P6\n") + std::to_string(p.width()) + " " + std::to_string(p.height()) + "\n255\n";
        auto px = p.pixels();
        out.append(reinterpret_cast<const char*>(px.data()), px.size());
        return out;
    }

    const char* sample_arg(codec::jpeg::subsampling s) {
        switch (s) {
            case codec::jpeg::subsampling::s444: return "1x1";
            case codec::jpeg::subsampling::s422: return "2x1";
            default: return "2x2";
        }
    }

    // cjpeg's file of the image with the options, "" when it fails
    std::string cjpeg_of(const codec::image& im, const codec::jpeg::options& o, const std::string& more = "") {
        const auto src = write_file("src.pnm", as_pnm(im));
        const auto out = (scratch_dir() / "cjpeg.jpg").string();
        std::string cmd = "'" + tool("cjpeg") + "' -dct int -baseline -quality " + std::to_string(o.quality);
        if (!is_gray(im.format())) {
            cmd += std::string(" -sample ") + sample_arg(o.subsampling);
        }
        if (o.optimize) {
            cmd += " -optimize";
        }
        cmd += " " + more + " -outfile '" + out + "' '" + src + "' 2>/dev/null";
        if (std::system(cmd.c_str()) != 0) {
            return "";
        }
        return read_file(out);
    }

    // "" when the same, else where the files part
    std::string compare(const std::string& ours, const std::string& theirs) {
        if (ours == theirs) {
            return "";
        }
        size_t i = 0;
        while (i < ours.size() && i < theirs.size() && ours[i] == theirs[i]) {
            ++i;
        }
        return "sizes " + std::to_string(ours.size()) + " and " + std::to_string(theirs.size()) + ", first difference at byte " + std::to_string(i);
    }

    // A cut of testorig.ppm, w × h, repeated where larger
    std::optional<codec::image> source(uint32_t w, uint32_t h, bool gray) {
        auto ppm = read_oracle(TestImages + "testorig.ppm");
        if (!ppm) {
            return std::nullopt;
        }
        unsigned sw, sh, maxval;
        int consumed = 0;
        if (std::sscanf(ppm->c_str(), "P6 %u %u %u%n", &sw, &sh, &maxval, &consumed) != 3) {
            return std::nullopt;
        }
        const char* px = ppm->data() + consumed + 1;
        codec::image im(w, h, gray ? pixel_format::gray8 : pixel_format::rgb8);
        auto out = im.pixels();
        size_t k = 0;
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                const char* p = px + (size_t(y % sh) * sw + x % sw) * 3;
                if (gray) {
                    out[k++] = byte(uint8_t(p[1]));
                } else {
                    for (int c = 0; c < 3; ++c) {
                        out[k++] = byte(uint8_t(p[c]));
                    }
                }
            }
        }
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

TEST(CodecJpegEncode_Tests, ByteForByteWithCjpeg) {
    if (tool("cjpeg").empty()) {
        GTEST_SKIP() << "no cjpeg";
    }
    const std::pair<uint32_t, uint32_t> sizes[] = {{1, 1}, {2, 3}, {7, 9}, {8, 8}, {15, 17}, {16, 16}, {17, 15}, {33, 31}, {64, 48}, {227, 149}};
    size_t compared = 0;
    for (bool gray : {false, true}) {
        for (auto [w, h] : sizes) {
            auto im = source(w, h, gray);
            if (!im) {
                GTEST_SKIP() << "no " << oracle_path(TestImages + "testorig.ppm");
            }
            for (int quality : {1, 5, 25, 50, 75, 85, 95, 100}) {
                for (auto s : {codec::jpeg::subsampling::s444, codec::jpeg::subsampling::s422, codec::jpeg::subsampling::s420}) {
                    if (gray && s != codec::jpeg::subsampling::s420) {
                        continue;   // sampling means nothing to one component
                    }
                    for (bool optimize : {false, true}) {
                        codec::jpeg::options o{.quality = quality, .subsampling = s, .optimize = optimize};
                        const std::string theirs = cjpeg_of(*im, o);
                        ASSERT_FALSE(theirs.empty());
                        EXPECT_EQ(compare(text(codec::jpeg::encode(*im, o)), theirs), "")
                            << (gray ? "gray " : "") << w << "x" << h << " q" << quality << " " << sample_arg(s) << (optimize ? " optimize" : "");
                        ++compared;
                    }
                }
            }
        }
    }
    EXPECT_GT(compared, 500u);
}

TEST(CodecJpegEncode_Tests, PngSuiteByteForByte) {
    if (tool("cjpeg").empty()) {
        GTEST_SKIP() << "no cjpeg";
    }
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
        codec::image im = *codec::png::decode(bytes(data), {.metadata = false});
        for (bool optimize : {false, true}) {
            codec::jpeg::options o{.optimize = optimize};
            EXPECT_EQ(compare(text(codec::jpeg::encode(im, o)), cjpeg_of(im, o)), "") << name << (optimize ? " optimize" : "");
        }
        ++files;
    }
    EXPECT_GT(files, 150u);
}

TEST(CodecJpegEncode_Tests, WhatItWritesReadsBack) {
    // through the module and through libjpeg, the same pixels; near the
    // source (PSNR over 30 dB at the default quality)
    auto im = source(227, 149, false);
    if (!im) {
        GTEST_SKIP();
    }
    for (auto s : {codec::jpeg::subsampling::s444, codec::jpeg::subsampling::s422, codec::jpeg::subsampling::s420}) {
        const std::string file = text(codec::jpeg::encode(*im, {.subsampling = s}));
        codec::image back = *codec::jpeg::decode(bytes(file));
        ASSERT_EQ(back.width(), 227u);
        ASSERT_EQ(back.format(), pixel_format::rgb8);
        double se = 0;
        auto a = back.pixels(), b = im->pixels();
        for (size_t i = 0; i < a.size(); ++i) {
            const double d = double(uint8_t(a[i])) - double(uint8_t(b[i]));
            se += d * d;
        }
        const double psnr = 10 * std::log10(255.0 * 255.0 / (se / double(a.size())));
        EXPECT_GT(psnr, 30.0) << sample_arg(s);
        if (!c_oracle().empty()) {
            const auto path = write_file("ours.jpg", file);
            auto theirs = run_oracle(c_oracle(), "jpeg", path);
            ASSERT_TRUE(theirs);
            EXPECT_EQ(std::memcmp(theirs->pixels.data(), a.data(), a.size()), 0) << sample_arg(s);
        }
    }
    // every pixel format encodes: gray ones as gray, the rest as YCbCr
    uint32_t seed = 1;
    for (int f = 0; f < 9; ++f) {
        codec::image picture(19, 13, static_cast<pixel_format>(f));
        std::mt19937 rng(seed++);
        for (auto& v : picture.pixels()) {
            v = byte(uint8_t(rng()));
        }
        codec::image back = *codec::jpeg::decode(codec::jpeg::encode(picture));
        EXPECT_EQ(back.format(), is_gray(picture.format()) ? pixel_format::gray8 : pixel_format::rgb8) << f;
        EXPECT_EQ(back.width(), 19u);
        EXPECT_EQ(back.height(), 13u);
    }
    EXPECT_THROW((void)codec::jpeg::encode(*im, {.quality = 0}), std::invalid_argument);
    EXPECT_THROW((void)codec::jpeg::encode(*im, {.quality = 101}), std::invalid_argument);
}

TEST(CodecJpegEncode_Tests, ExifAndIcc) {
    auto im = source(40, 30, false);
    if (!im) {
        GTEST_SKIP();
    }
    auto& s = codec::detail::ImageAccess::state(*im);
    const std::string exif = std::string("MM\0*\0\0\0\x08\0\x01", 10) + std::string("\x01\x12\0\x03\0\0\0\x01\0\x08\0\0", 12) + std::string(4, '\0');
    std::mt19937 rng(3);
    std::string icc(140000, '\0');
    for (auto& c : icc) {
        c = char(rng());
    }
    for (char c : icc) {
        s.icc.push_back(byte(c));
    }
    // ICC alone: cjpeg -icc writes the same chunks
    if (!tool("cjpeg").empty()) {
        const auto icc_path = write_file("profile.icc", icc);
        EXPECT_EQ(compare(text(codec::jpeg::encode(*im)), cjpeg_of(*im, {}, "-icc '" + icc_path + "'")), "");
    }
    for (char c : exif) {
        s.exif.push_back(byte(c));
    }
    codec::image back = *codec::jpeg::decode(codec::jpeg::encode(*im));
    EXPECT_EQ(back.orientation(), 8u);
    ASSERT_EQ(back.exif().size(), exif.size());
    EXPECT_EQ(std::memcmp(back.exif().data(), exif.data(), exif.size()), 0);
    ASSERT_EQ(back.icc().size(), icc.size());
    EXPECT_EQ(std::memcmp(back.icc().data(), icc.data(), icc.size()), 0);
    if (!c_oracle().empty()) {
        EXPECT_TRUE(run_oracle(c_oracle(), "jpeg", write_file("meta.jpg", text(codec::jpeg::encode(*im)))));
    }
}

TEST(CodecJpegEncode_Tests, AStreamAndAStreamThatFails) {
    auto im = source(227, 149, false);
    if (!im) {
        GTEST_SKIP();
    }
    const std::string whole = text(codec::jpeg::encode(*im, {.optimize = true}));
    io::buffer out;
    ASSERT_TRUE(codec::jpeg::encode(*im, out, {.optimize = true}));
    auto held = out.data();
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(held.data()), held.size()), whole);
    failing_writer f{1000};
    auto r = codec::jpeg::encode(*im, io::writer(f));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), codec::errc::io);
    ASSERT_TRUE(r.error().io_error());
}

TEST(CodecJpegEncode_Tests, ASidePast65535IsRefused) {
    // SOF keeps a side in 16 bits: past 65 535, errc::invalid_argument and
    // nothing written, where the file was broken; 65 535 itself is written.
    // The forms of bytes return it too, and stay noexcept
    static_assert(noexcept(codec::jpeg::encode(std::declval<const codec::image&>())));
    static_assert(noexcept(codec::png::encode(std::declval<const codec::image&>())));
    for (auto [w, h] : {std::pair{65536u, 2u}, std::pair{2u, 65536u}}) {
        const codec::image big(w, h, pixel_format::gray8);
        io::buffer out;
        auto r = codec::jpeg::encode(big, out);
        ASSERT_FALSE(r) << w << "x" << h;
        EXPECT_EQ(r.error().code(), codec::errc::invalid_argument);
        EXPECT_EQ(r.error().message(), "offset 0: jpeg: a side past 65535 pixels, more than SOF holds");
        EXPECT_EQ(out.data().size(), 0u);
        auto bytes = codec::jpeg::encode(big);
        ASSERT_FALSE(bytes);
        EXPECT_EQ(bytes.error(), r.error());
    }
    const codec::image edge(65535, 2, pixel_format::rgb8);
    io::buffer out;
    ASSERT_TRUE(codec::jpeg::encode(edge, out));
    auto back = codec::jpeg::decode(out.data());
    ASSERT_TRUE(back);
    EXPECT_EQ(back->width(), 65535u);
}

TEST(CodecJpegEncode_Tests, TheFdctAgainstTheIdct) {
    // a block through the FDCT, quantized by ones, back through the IDCT:
    // within one level of the samples
    std::mt19937 rng(7);
    uint16_t ones[64];
    std::fill(std::begin(ones), std::end(ones), uint16_t(1));
    for (int round = 0; round < 200; ++round) {
        uint8_t in[64], out[64];
        for (auto& v : in) {
            v = uint8_t(rng());
        }
        int32_t dct[64];
        codec::detail::fdct_islow(in, 8, dct);
        int16_t coef[64];
        for (int i = 0; i < 64; ++i) {
            const int32_t x = dct[i];
            coef[i] = int16_t(x < 0 ? -((-x + 4) / 8) : (x + 4) / 8);
        }
        codec::detail::idct_islow(coef, ones, out, 8);
        for (int i = 0; i < 64; ++i) {
            ASSERT_LE(std::abs(int(in[i]) - int(out[i])), 1) << round << " " << i;
        }
    }
}
