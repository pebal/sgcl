//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Images on files: codec::load, image::save and codec::save, and their
// tasks. A file saved holds what the format's encode writes (PNG and JPEG
// byte for byte) and loads back; the extension names the format in either
// case; .gif, .webp, .avif and any other extension are errc::unsupported
// with nothing written; a failure (a stream's, an encoder's exception)
// leaves no .part and the path as it was; a file that does not open is
// errc::io with io's error inside.
#include "common.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace {
    using namespace sgcl;
    using codec::pixel_format;

    struct Scratch {
        std::string dir;

        Scratch() {
            static std::atomic<int> n{0};
            dir = (std::filesystem::temp_directory_path() / ("sgcl_codec_files_" + std::to_string(::getpid()) + "_" + std::to_string(n++))).string();
            std::filesystem::create_directories(dir);
        }

        ~Scratch() {
            std::filesystem::remove_all(dir);
        }

        string operator/(const std::string& name) const {
            return string(dir + "/" + name);
        }
    };

    std::string bytes_of(const string& path) {
        std::ifstream in(std::string(path.view()), std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    bool exists(const string& path) {
        return std::filesystem::exists(std::string(path.view()));
    }

    // A picture with gradients and edges, of the format asked for
    codec::image picture(pixel_format f, uint32_t w = 37, uint32_t h = 23) {
        codec::image rgba(w, h, pixel_format::rgba8);
        auto* p = reinterpret_cast<uint8_t*>(rgba.pixels().data());
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                uint8_t* q = p + (size_t(y) * w + x) * 4;
                q[0] = uint8_t(x * 7 + y);
                q[1] = uint8_t(y * 11);
                q[2] = uint8_t((x ^ y) * 5);
                q[3] = uint8_t(255 - x);
            }
        }
        return rgba.convert(f);
    }

    std::string pixels_of(const codec::image& im) {
        auto px = im.pixels();
        return std::string(reinterpret_cast<const char*>(px.data()), px.size());
    }

    std::string encoded(const vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }
}

TEST(CodecFiles_Tests, PngRoundTrip) {
    Scratch s;
    for (auto f : {pixel_format::rgb8, pixel_format::rgba8, pixel_format::gray8, pixel_format::rgba16}) {
        const codec::image im = picture(f);
        ASSERT_TRUE(im.save(s / "a.png"));
        // what png::encode writes, byte for byte
        EXPECT_EQ(bytes_of(s / "a.png"), encoded(codec::png::encode(im)));
        EXPECT_FALSE(exists(s / "a.png.part"));
        codec::image back = codec::load(s / "a.png");
        EXPECT_EQ(back.format(), f);
        EXPECT_EQ(pixels_of(back), pixels_of(im));
    }
    // the options' level reaches the encoder
    const codec::image im = picture(pixel_format::rgb8, 200, 100);
    ASSERT_TRUE(codec::save(im, s / "stored.png", {.level = 0}));
    EXPECT_EQ(bytes_of(s / "stored.png"), encoded(codec::png::encode(im, {.level = 0})));
}

TEST(CodecFiles_Tests, JpegRoundTrip) {
    Scratch s;
    const codec::image im = picture(pixel_format::rgb8);
    ASSERT_TRUE(im.save(s / "a.jpg"));
    EXPECT_EQ(bytes_of(s / "a.jpg"), encoded(codec::jpeg::encode(im)));
    ASSERT_TRUE(im.save(s / "b.JPEG", {.quality = 95, .subsampling = codec::jpeg::subsampling::s444}));
    EXPECT_EQ(bytes_of(s / "b.JPEG"), encoded(codec::jpeg::encode(im, {.quality = 95, .subsampling = codec::jpeg::subsampling::s444})));
    for (const char* name : {"a.jpg", "b.JPEG"}) {
        auto back = codec::load(s / name);
        ASSERT_TRUE(back) << name;
        EXPECT_EQ(back->width(), im.width());
        EXPECT_EQ(back->height(), im.height());
        EXPECT_EQ(back->format(), pixel_format::rgb8);
        // what the file decodes to: decode's pixels from the same bytes
        vector<byte> file = io::read_file(s / name);
        codec::image direct = codec::decode(file);
        EXPECT_EQ(pixels_of(*back), pixels_of(direct));
    }
}

TEST(CodecFiles_Tests, HeicRoundTrip) {
#if defined(__APPLE__)
    Scratch s;
    const codec::image im = picture(pixel_format::rgb8, 64, 48);
    auto saved = im.save(s / "a.heic");
    if (!saved && saved.error().code() == codec::errc::unsupported) {
        GTEST_SKIP() << "the system writes no HEIC here: " << saved.error().message().view();
    }
    ASSERT_TRUE(saved) << saved.error().message().view();
    EXPECT_FALSE(exists(s / "a.heic.part"));
    auto back = codec::load(s / "a.heic");
    ASSERT_TRUE(back) << back.error().message().view();
    EXPECT_EQ(back->width(), im.width());
    EXPECT_EQ(back->height(), im.height());
#else
    Scratch s;
    auto saved = picture(pixel_format::rgb8).save(s / "a.heic");
    ASSERT_FALSE(saved);
    EXPECT_EQ(saved.error().code(), codec::errc::unsupported);
    EXPECT_FALSE(exists(s / "a.heic"));
    EXPECT_FALSE(exists(s / "a.heic.part"));
#endif
}

TEST(CodecFiles_Tests, ExtensionsNotWritten) {
    Scratch s;
    const codec::image im = picture(pixel_format::rgba8);
    for (const char* name : {"a.gif", "a.webp", "a.avif", "a.bmp", "a.tiff", "noextension", "dir.png/x"}) {
        auto r = im.save(s / name);
        ASSERT_FALSE(r) << name;
        EXPECT_EQ(r.error().code(), codec::errc::unsupported) << name;
        EXPECT_FALSE(exists(s / name)) << name;
        EXPECT_FALSE(exists(string::concat(s / name, ".part"))) << name;
    }
}

TEST(CodecFiles_Tests, FailureLeavesNoPart) {
    Scratch s;
    const codec::image im = picture(pixel_format::rgb8);
    // the encoder's contract broken (a quality outside 1..100): its
    // exception passes, and neither the part nor the file stays
    EXPECT_THROW((void)im.save(s / "q.jpg", {.quality = 0}), std::invalid_argument);
    EXPECT_FALSE(exists(s / "q.jpg"));
    EXPECT_FALSE(exists(s / "q.jpg.part"));
    // the rename fails (path is a directory with a file in it): errc::io,
    // the part removed, the directory as it was
    std::filesystem::create_directories(std::string((s / "taken.png").view()));
    std::ofstream(std::string((s / "taken.png/inside").view())) << "x";
    auto r = im.save(s / "taken.png");
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), codec::errc::io);
    ASSERT_TRUE(r.error().io_error().has_value());
    EXPECT_FALSE(exists(s / "taken.png.part"));
    EXPECT_TRUE(std::filesystem::is_directory(std::string((s / "taken.png").view())));
    EXPECT_EQ(bytes_of(s / "taken.png/inside"), "x");
    // the part cannot be made (no such directory): errc::io, nothing written
    auto m = im.save(s / "missing/a.png");
    ASSERT_FALSE(m);
    EXPECT_EQ(m.error().code(), codec::errc::io);
    EXPECT_FALSE(exists(s / "missing"));
    // an existing file is replaced whole
    ASSERT_TRUE(im.save(s / "over.png"));
    ASSERT_TRUE(picture(pixel_format::gray8).save(s / "over.png"));
    EXPECT_EQ(codec::load(s / "over.png")->format(), pixel_format::gray8);
}

TEST(CodecFiles_Tests, LoadErrors) {
    Scratch s;
    auto missing = codec::load(s / "none.png");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), codec::errc::io);
    ASSERT_TRUE(missing.error().io_error().has_value());
    EXPECT_TRUE(missing.error().io_error()->is_not_found());
    // a file of no image format: decode's error
    std::ofstream(std::string((s / "text.png").view())) << "not an image at all";
    auto text = codec::load(s / "text.png");
    ASSERT_FALSE(text);
    EXPECT_EQ(text.error().code(), codec::errc::unsupported);
    // the format by the signature, not the name
    ASSERT_TRUE(picture(pixel_format::rgb8).save(s / "real.png"));
    std::filesystem::rename(std::string((s / "real.png").view()), std::string((s / "named.jpg").view()));
    auto sniffed = codec::load(s / "named.jpg");
    ASSERT_TRUE(sniffed);
    EXPECT_EQ(pixels_of(*sniffed), pixels_of(picture(pixel_format::rgb8)));
    // the options pass to decode
    auto rgba = codec::load(s / "named.jpg", {.want = pixel_format::rgba8});
    ASSERT_TRUE(rgba);
    EXPECT_EQ(rgba->format(), pixel_format::rgba8);
}

TEST(CodecFiles_Tests, Tasks) {
    Scratch s;
    auto t = [](string dir) -> async::task<bool> {
        const codec::image im = picture(pixel_format::rgb8);
        bool ok = bool(co_await im.async_save(dir + "/a.png"));
        ok = ok && bool(co_await codec::async_save(im, dir + "/b.jpg", {.quality = 90}));
        ok = ok && bool(co_await im.async_save(dir + "/c.jpg", {.quality = 90}));
        auto a = co_await codec::async_load(dir + "/a.png");
        ok = ok && a && pixels_of(*a) == pixels_of(im);
        auto b = co_await codec::async_load(dir + "/b.jpg", {.want = pixel_format::rgba8});
        ok = ok && b && b->format() == pixel_format::rgba8;
        auto bad = co_await im.async_save(dir + "/d.webp");
        ok = ok && !bad && bad.error().code() == codec::errc::unsupported;
        co_return ok;
    }(string(s.dir));
    EXPECT_TRUE(t.wait());
    EXPECT_EQ(bytes_of(s / "b.jpg"), bytes_of(s / "c.jpg"));
}
