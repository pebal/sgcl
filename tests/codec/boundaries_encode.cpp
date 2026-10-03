//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec's encoders and files at their boundaries (DESIGN 408): one pixel of
// every format through every encoder, the options at their bounds and
// outside them, metadata at what a format holds, a stream that fails after
// every number of bytes or throws, and the files' paths at their edges. What
// the format's own tests hold is not repeated: every format and size through
// PNG (CodecPngEncode_Tests.RoundTripEveryFormatSizeAndLevel), every format
// through JPEG at 19 × 13 (CodecJpegEncode_Tests.WhatItWritesReadsBack), a
// JPEG side past 65 535 (ASidePast65535IsRefused), a quality of 0 and 101
// (WhatItWritesReadsBack, CodecHeif_Tests.QualityOrientationAndProfile), a
// moved-from image (CodecFiles_Tests.AMovedFromImageEncodesAsItsSource), the
// extensions not written and a rename that fails (CodecFiles_Tests.*).
#include "common.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <unistd.h>
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

    template<class T>
    codec::errc code_of(const expected<T, codec::error>& r) {
        return r ? codec::errc{} : r.error().code();
    }

    void fill_random(codec::image& im, uint32_t seed) {
        std::mt19937 rng(seed);
        for (auto& b : im.pixels()) {
            b = static_cast<std::byte>(rng());
        }
    }

    std::vector<std::byte> random_bytes(size_t n, uint32_t seed) {
        std::mt19937 rng(seed);
        std::vector<std::byte> out(n);
        for (auto& b : out) {
            b = std::byte(rng());
        }
        return out;
    }

    // A stream that takes `room` bytes, then fails every write
    struct failing_writer {
        size_t room;
        std::string took;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            if (took.size() + b.size() > room) {
                return unexpected(io::error(std::make_error_code(std::errc::no_space_on_device), "write", "test"));
            }
            took.append(reinterpret_cast<const char*>(b.data()), b.size());
            return b.size();
        }
    };

    // A stream whose write throws once it has taken `room` bytes
    struct throwing_writer {
        size_t room;
        size_t took = 0;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            if (took + b.size() > room) {
                throw std::runtime_error("the stream's own failure");
            }
            took += b.size();
            return b.size();
        }
    };

    struct Scratch {
        std::string dir;

        Scratch() {
            static std::atomic<int> n{0};
            dir = (std::filesystem::temp_directory_path() / ("sgcl_codec_bounds_" + std::to_string(::getpid()) + "_" + std::to_string(n++))).string();
            std::filesystem::create_directories(dir);
        }

        ~Scratch() {
            std::filesystem::remove_all(dir);
        }

        string operator/(const std::string& name) const {
            return string(dir + "/" + name);
        }
    };

    bool exists(const string& path) {
        return std::filesystem::exists(std::string(path.view()));
    }

    bool heic_written() {
        auto r = codec::heif::encode(codec::image(2, 2, pixel_format::rgb8));
        return bool(r);
    }
}

TEST(CodecEncodeBounds_Tests, OnePixelOfEveryFormatThroughEveryEncoder) {
    // PNG back as it was (CMYK as RGB); JPEG of every subsampling, plain and
    // optimized, at the qualities' bounds: one pixel back, gray as gray
    for (pixel_format f : AllFormats) {
        codec::image dot(1, 1, f);
        fill_random(dot, unsigned(f) + 3);
        auto png = codec::png::decode(*codec::png::encode(dot));
        ASSERT_TRUE(png) << int(f);
        EXPECT_EQ(png->width(), 1u);
        const codec::image expected = f == pixel_format::cmyk8 ? dot.convert(pixel_format::rgb8) : dot;
        EXPECT_EQ(std::memcmp(png->pixels().data(), expected.pixels().data(), expected.pixels().size()), 0) << int(f);
        for (auto s : {codec::jpeg::subsampling::s444, codec::jpeg::subsampling::s422, codec::jpeg::subsampling::s420}) {
            for (bool optimize : {false, true}) {
                for (int q : {1, 100}) {
                    auto file = codec::jpeg::encode(dot, {.quality = q, .subsampling = s, .optimize = optimize});
                    ASSERT_TRUE(file);
                    auto back = codec::jpeg::decode(*file);
                    ASSERT_TRUE(back) << int(f) << " " << int(s) << " " << q;
                    EXPECT_EQ(back->width(), 1u);
                    EXPECT_EQ(back->height(), 1u);
                    EXPECT_EQ(back->format(), codec::detail::gray(f) ? pixel_format::gray8 : pixel_format::rgb8);
                }
            }
        }
    }
}

TEST(CodecEncodeBounds_Tests, JpegQualityAtItsBoundsAndSidesOfEveryRemainder) {
    // quality 1 and 100 hold the colors as far as they can; sides of every
    // remainder of the 16-pixel MCU of 4:2:0, near the source
    codec::image flat(17, 9, pixel_format::rgb8);
    for (size_t i = 0; i < flat.pixels().size(); i += 3) {
        flat.pixels()[i] = std::byte{200};
        flat.pixels()[i + 1] = std::byte{100};
        flat.pixels()[i + 2] = std::byte{50};
    }
    for (int q : {1, 100}) {
        codec::image back = *codec::jpeg::decode(*codec::jpeg::encode(flat, {.quality = q}));
        for (size_t i = 0; i < back.pixels().size(); ++i) {
            const int d = int(uint8_t(back.pixels()[i])) - int(uint8_t(flat.pixels()[i]));
            ASSERT_LE(std::abs(d), q == 100 ? 2 : 64) << q << " " << i;
        }
    }
    for (uint32_t w = 1; w <= 17; ++w) {
        codec::image im(w, 17 - w + 1, pixel_format::rgb8);
        fill_random(im, w);
        auto back = codec::jpeg::decode(*codec::jpeg::encode(im, {.quality = 100, .subsampling = codec::jpeg::subsampling::s444}));
        ASSERT_TRUE(back) << w;
        EXPECT_EQ(back->width(), w);
        EXPECT_EQ(back->height(), 18 - w);
    }
}

TEST(CodecEncodeBounds_Tests, JpegSubsamplingOutsideTheListIsAContract) {
    // the subsampling is a contract as the quality is: one outside the list
    // is invalid_argument, nothing written, never a guess of 4:2:2
    codec::image im(8, 8, pixel_format::rgb8);
    const auto odd = static_cast<codec::jpeg::subsampling>(3);
    EXPECT_THROW((void)codec::jpeg::encode(im, {.subsampling = odd}), std::invalid_argument);
    failing_writer out{1u << 20};
    EXPECT_THROW((void)codec::jpeg::encode(im, io::writer(out), {.subsampling = odd}), std::invalid_argument);
    EXPECT_TRUE(out.took.empty());
    Scratch s;
    EXPECT_THROW((void)im.save(s / "odd.jpg", {.subsampling = odd}), std::invalid_argument);
    EXPECT_FALSE(exists(s / "odd.jpg"));
    EXPECT_FALSE(exists(s / "odd.jpg.part"));
    // a PNG or HEIC leaves the field alone
    EXPECT_TRUE(im.save(s / "odd.png", {.subsampling = odd}));
}

TEST(CodecEncodeBounds_Tests, PngAtWhatAChunkHolds) {
    // PNG's four-byte numbers stop at 2^31 - 1: a side past it is
    // invalid_argument before anything is written; an eXIf or iCCP chunk
    // past it is left out, as JPEG leaves out what a segment does not hold.
    // The bound is the encoder's parameter (PngChunkMax by default): held
    // here at small values, the real one where nothing large is needed
    static_assert(codec::detail::PngChunkMax == 0x7FFFFFFFu);
    auto encode = [](const codec::image& im, size_t most) -> expected<std::string, codec::error> {
        vector<byte> out;
        codec::detail::VectorSink sink{out, nullopt};
        if (!codec::detail::PngEncoder<codec::detail::VectorSink>(im, 7, sink, most).run()) {
            return unexpected(*sink.failure);
        }
        return std::string(reinterpret_cast<const char*>(out.data()), out.size());
    };
    // sides at the bound and one past
    EXPECT_TRUE(encode(codec::image(20, 3, pixel_format::gray8), 20));
    EXPECT_TRUE(encode(codec::image(3, 20, pixel_format::gray8), 20));
    for (auto [w, h] : {std::pair{21u, 3u}, std::pair{3u, 21u}}) {
        auto r = encode(codec::image(w, h, pixel_format::gray8), 20);
        ASSERT_FALSE(r) << w << "x" << h;
        EXPECT_EQ(r.error().code(), codec::errc::invalid_argument);
    }
    // the EXIF block at the bound written, one byte past it left out, the
    // image written all the same
    codec::image im(4, 2, pixel_format::rgb8);
    auto block = random_bytes(300, 4);
    block[0] = block[1] = std::byte{'M'};
    im.set_exif(slice<const byte>(block.data(), block.size()));
    auto at = encode(im, 300);
    ASSERT_TRUE(at);
    EXPECT_EQ(codec::png::decode(bytes(*at))->exif().size(), 300u);
    auto past = encode(im, 299);
    ASSERT_TRUE(past);
    EXPECT_EQ(past->find("eXIf"), std::string::npos);
    auto read = codec::png::decode(bytes(*past));
    ASSERT_TRUE(read);
    EXPECT_TRUE(read->exif().empty());
    EXPECT_EQ(read->width(), 4u);
    // the iCCP chunk: its whole body (name, method, the zlib stream) at the
    // bound written, one byte past it left out
    im.set_exif({});
    auto profile = random_bytes(500, 5);
    im.set_icc(slice<const byte>(profile.data(), profile.size()));
    const std::string whole = *encode(im, codec::detail::PngChunkMax);
    const size_t at_iccp = whole.find("iCCP");
    ASSERT_NE(at_iccp, std::string::npos);
    const size_t body = size_t(uint8_t(whole[at_iccp - 4])) << 24 | size_t(uint8_t(whole[at_iccp - 3])) << 16 |
                        size_t(uint8_t(whole[at_iccp - 2])) << 8 | size_t(uint8_t(whole[at_iccp - 1]));
    EXPECT_EQ(codec::png::decode(bytes(*encode(im, body)))->icc().size(), 500u);
    auto no_profile = encode(im, body - 1);
    ASSERT_TRUE(no_profile);
    EXPECT_EQ(no_profile->find("iCCP"), std::string::npos);
    EXPECT_TRUE(codec::png::decode(bytes(*no_profile))->icc().empty());
    // the public path at the real bound, with no image of that size made:
    // the encoder reads the sides before any pixel. 2^31 is refused with
    // nothing written; 2^31 - 1 is taken, IHDR written with it (the stream
    // fails at the next chunk, before any row)
    codec::image claim(1, 1, pixel_format::gray8);
    claim.set_exif(slice<const byte>(block.data(), block.size()));
    auto& s = codec::detail::ImageAccess::state(claim);
    s.width = 0x80000000u;
    failing_writer none{1u << 20};
    auto refused = codec::png::encode(claim, io::writer(none));
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), codec::errc::invalid_argument);
    EXPECT_TRUE(none.took.empty());
    EXPECT_EQ(codec::png::encode(claim).error().code(), codec::errc::invalid_argument);
    s.width = 1;
    s.height = 0x80000000u;
    EXPECT_EQ(codec::png::encode(claim).error().code(), codec::errc::invalid_argument);
    s.height = 0x7FFFFFFFu;
    failing_writer head{8 + 25};
    auto taken = codec::png::encode(claim, io::writer(head));
    ASSERT_FALSE(taken);
    EXPECT_EQ(taken.error().code(), codec::errc::io);
    ASSERT_EQ(head.took.size(), 8u + 25u);
    EXPECT_EQ(head.took.substr(16, 8), std::string("\0\0\0\x01\x7F\xFF\xFF\xFF", 8));
    s.height = 1;
}

TEST(CodecEncodeBounds_Tests, JpegMetadataAtWhatASegmentHolds) {
    // EXIF of 65 527 bytes is one APP1 segment, one byte more is not
    // written; an ICC profile of 255 chunks of 65 519 is written, one byte
    // more is not; an empty block writes nothing
    codec::image im(4, 4, pixel_format::rgb8);
    for (size_t n : {size_t(1), size_t(65527), size_t(65528)}) {
        auto block = random_bytes(n, uint32_t(n));
        block[0] = std::byte{'M'};
        block[n - 1] = std::byte{'M'};
        im.set_exif(slice<const byte>(block.data(), block.size()));
        auto back = codec::jpeg::decode(*codec::jpeg::encode(im));
        ASSERT_TRUE(back) << n;
        EXPECT_EQ(back->exif().size(), n <= 65527 ? n : 0u) << n;
        if (n <= 65527) {
            EXPECT_EQ(std::memcmp(back->exif().data(), block.data(), n), 0);
        }
    }
    im.set_exif({});
    for (size_t n : {size_t(65519), size_t(65520), size_t(255) * 65519, size_t(255) * 65519 + 1}) {
        auto profile = random_bytes(n, uint32_t(n));
        im.set_icc(slice<const byte>(profile.data(), profile.size()));
        auto back = codec::jpeg::decode(*codec::jpeg::encode(im));
        ASSERT_TRUE(back) << n;
        EXPECT_EQ(back->icc().size(), n <= size_t(255) * 65519 ? n : 0u) << n;
        if (back->icc().size() == n) {
            EXPECT_EQ(std::memcmp(back->icc().data(), profile.data(), n), 0) << n;
        }
    }
    // PNG holds both whole
    auto profile = random_bytes(70000, 1);
    auto block = random_bytes(70000, 2);
    im.set_icc(slice<const byte>(profile.data(), profile.size()));
    im.set_exif(slice<const byte>(block.data(), block.size()));
    auto png = codec::png::decode(*codec::png::encode(im));
    ASSERT_TRUE(png);
    EXPECT_EQ(png->icc().size(), 70000u);
    EXPECT_EQ(png->exif().size(), 70000u);
}

TEST(CodecEncodeBounds_Tests, AStreamThatFailsAfterEveryNumberOfBytes) {
    // the stream takes n bytes, then fails: errc::io with the stream's error,
    // at the offset of the bytes it took, and nothing written after the failure
    codec::image im(40, 30, pixel_format::rgba8);
    fill_random(im, 5);
    auto exif = random_bytes(300, 3);
    exif[0] = exif[1] = std::byte{'I'};
    im.set_exif(slice<const byte>(exif.data(), exif.size()));
    const std::string png = [&] { auto v = *codec::png::encode(im); return std::string(reinterpret_cast<const char*>(v.data()), v.size()); }();
    const std::string jpg = [&] { auto v = *codec::jpeg::encode(im); return std::string(reinterpret_cast<const char*>(v.data()), v.size()); }();
    for (const std::string* whole : {&png, &jpg}) {
        for (size_t room = 0; room < whole->size(); room += whole->size() / 97 + 1) {
            failing_writer out{room};
            auto r = whole == &png ? codec::png::encode(im, io::writer(out)) : codec::jpeg::encode(im, io::writer(out));
            ASSERT_FALSE(r) << room;
            EXPECT_EQ(r.error().code(), codec::errc::io);
            ASSERT_TRUE(r.error().io_error());
            EXPECT_EQ(r.error().offset(), out.took.size()) << room;
            EXPECT_LE(out.took.size(), room);
            EXPECT_EQ(out.took, whole->substr(0, out.took.size())) << room;
        }
        failing_writer exact{whole->size()};
        EXPECT_TRUE(whole == &png ? codec::png::encode(im, io::writer(exact)) : codec::jpeg::encode(im, io::writer(exact)));
        EXPECT_EQ(exact.took, *whole);
    }
}

TEST(CodecEncodeBounds_Tests, AStreamThatThrows) {
    // the stream's exception passes, at the first write, a later one and
    // the last, from PNG, JPEG and HEIC alike; HEIC's, written from inside
    // the system's encoder, is carried out of it and thrown after it
    codec::image im(64, 48, pixel_format::rgb8);
    fill_random(im, 6);
    const size_t png = codec::png::encode(im)->size(), jpg = codec::jpeg::encode(im)->size();
    for (size_t room : {size_t(0), size_t(100), png / 2, png - 1}) {
        throwing_writer a{room};
        EXPECT_THROW((void)codec::png::encode(im, io::writer(a)), std::runtime_error) << room;
    }
    for (size_t room : {size_t(0), size_t(100), jpg / 2, jpg - 1}) {
        throwing_writer b{room};
        EXPECT_THROW((void)codec::jpeg::encode(im, io::writer(b)), std::runtime_error) << room;
    }
#if defined(__APPLE__)
    if (!heic_written()) {
        GTEST_SKIP() << "no HEIC encoder";
    }
    const size_t heic = codec::heif::encode(im)->size();
    failing_writer whole{heic};
    EXPECT_TRUE(codec::heif::encode(im, io::writer(whole)));
    for (size_t room : {size_t(0), size_t(100), heic / 2, heic - 1}) {
        throwing_writer h{room};
        EXPECT_THROW((void)codec::heif::encode(im, io::writer(h)), std::runtime_error) << room;
        // and a writer that fails with a value there: errc::io
        failing_writer f{room};
        auto r = codec::heif::encode(im, io::writer(f));
        ASSERT_FALSE(r) << room;
        EXPECT_EQ(r.error().code(), codec::errc::io);
        EXPECT_EQ(r.error().offset(), f.took.size());
    }
    // the encoder whole after either
    EXPECT_TRUE(codec::heif::encode(im));
#endif
}

TEST(CodecEncodeBounds_Tests, HeicOfEveryFormatAndOnePixel) {
#if defined(__APPLE__)
    if (!heic_written()) {
        GTEST_SKIP() << "no HEIC encoder";
    }
    for (pixel_format f : AllFormats) {
        for (auto [w, h] : {std::pair{1u, 1u}, std::pair{33u, 17u}}) {
            codec::image im(w, h, f);
            fill_random(im, unsigned(f) * 7 + w);
            for (int q : {1, 100}) {
                auto file = codec::heif::encode(im, {.quality = q});
                ASSERT_TRUE(file) << int(f) << " " << w << " q " << q << ": " << file.error().message().view();
                auto back = codec::heif::decode(*file);
                ASSERT_TRUE(back) << int(f) << " " << w << ": " << back.error().message().view();
                EXPECT_EQ(back->width(), w);
                EXPECT_EQ(back->height(), h);
                EXPECT_EQ(codec::detail::alpha(back->format()), codec::detail::alpha(f)) << int(f);
            }
        }
    }
#else
    EXPECT_EQ(code_of(codec::heif::encode(codec::image(1, 1, pixel_format::gray8))), codec::errc::unsupported);
#endif
}

TEST(CodecEncodeBounds_Tests, HeifProfileAtMaxMetadata) {
#if defined(__APPLE__)
    if (!heic_written()) {
        GTEST_SKIP() << "no HEIC encoder";
    }
    codec::image im(16, 16, pixel_format::rgb8);
    fill_random(im, 9);
    auto file = codec::heif::encode(im);
    ASSERT_TRUE(file);
    auto plain = codec::decode(*file);
    ASSERT_TRUE(plain);
    const size_t n = plain->icc().size();
    ASSERT_GT(n, 0u);
    auto at = codec::decode(*file, {.limits = {.max_metadata = n}});
    ASSERT_TRUE(at);
    EXPECT_EQ(at->icc().size(), n);
    EXPECT_EQ(code_of(codec::decode(*file, {.limits = {.max_metadata = n - 1}})), codec::errc::too_large);
    auto bare = codec::decode(*file, {.limits = {.max_metadata = 0}, .metadata = false});
    ASSERT_TRUE(bare);
    EXPECT_TRUE(bare->icc().empty());
    // max_pixels at the bound and one past
    EXPECT_TRUE(codec::decode(*file, {.limits = {.max_pixels = 256}}));
    EXPECT_EQ(code_of(codec::decode(*file, {.limits = {.max_pixels = 255}})), codec::errc::too_large);
    // a want outside the list, from bytes and from a stream
    EXPECT_EQ(code_of(codec::decode(*file, {.want = static_cast<pixel_format>(9)})), codec::errc::invalid_argument);
#endif
}

TEST(CodecFilesBounds_Tests, PathsAtTheirEdges) {
    Scratch s;
    const codec::image im(3, 2, pixel_format::rgb8);
    // no name, a name that is only an extension, a dot at the end
    auto empty = im.save(string());
    ASSERT_FALSE(empty);
    EXPECT_EQ(empty.error().code(), codec::errc::unsupported);
    auto dot = im.save(s / "a.");
    ASSERT_FALSE(dot);
    EXPECT_EQ(dot.error().code(), codec::errc::unsupported);
    EXPECT_FALSE(exists(s / "a."));
    ASSERT_TRUE(im.save(s / ".png"));
    EXPECT_TRUE(codec::load(s / ".png"));
    ASSERT_TRUE(im.save(s / "UPPER.PNG"));
    // the part's name taken by a directory: errc::io, the path not made
    std::filesystem::create_directories(std::string((s / "busy.png.part").view()));
    auto busy = im.save(s / "busy.png");
    ASSERT_FALSE(busy);
    EXPECT_EQ(busy.error().code(), codec::errc::io);
    EXPECT_FALSE(exists(s / "busy.png"));
    EXPECT_TRUE(std::filesystem::is_directory(std::string((s / "busy.png.part").view())));
    // load: an empty file, a directory, a file of one byte
    std::ofstream(std::string((s / "empty.png").view())).flush();
    EXPECT_EQ(code_of(codec::load(s / "empty.png")), codec::errc::unsupported);
    EXPECT_EQ(code_of(codec::load(string(s.dir))), codec::errc::io);
    EXPECT_EQ(code_of(codec::load(string())), codec::errc::io);
    std::ofstream(std::string((s / "one.png").view())) << '\x89';
    EXPECT_EQ(code_of(codec::load(s / "one.png")), codec::errc::unsupported);
}

TEST(CodecFilesBounds_Tests, TasksAtTheirEdges) {
    Scratch s;
    const codec::image im(3, 2, pixel_format::rgb8);
    // the contract broken in a task: wait() rethrows it, nothing left
    EXPECT_THROW((void)im.async_save(s / "q.jpg", {.quality = 0}).wait(), std::invalid_argument);
    EXPECT_THROW((void)codec::async_save(im, s / "q.jpg", {.quality = 101}).wait(), std::invalid_argument);
    EXPECT_FALSE(exists(s / "q.jpg"));
    EXPECT_FALSE(exists(s / "q.jpg.part"));
    // a file that is not there, and no extension: errors through the task
    EXPECT_EQ(code_of(codec::async_load(s / "none.png").wait()), codec::errc::io);
    EXPECT_EQ(code_of(im.async_save(s / "none").wait()), codec::errc::unsupported);
    // a task made and never run writes nothing
    {
        auto never = im.async_save(s / "never.png");
    }
    EXPECT_FALSE(exists(s / "never.png"));
}
