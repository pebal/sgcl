//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: JPEG XL through the system's codec (jxl.h; ImageIO on macOS,
// errc::unsupported elsewhere). Files made at run time by libjxl's cjxl
// (Homebrew's jpeg-xl; the tests are skipped without it): lossless ones
// from PngSuite (gray, RGB and RGBA of 8 and 16 bits) decoded to the
// source's pixels exactly, and to djxl's (libjxl's own decoder, the
// oracle) for lossy ones within the rounding of two decoders; the
// container as the bare codestream; a JPEG recompressed losslessly, its
// orientation and its EXIF (in a Brotli-compressed box, read by metadata);
// an animated GIF's first frame; decode_options.want and the limits; the
// stream, codec::decode, load and save; sniffing; truncated and broken
// files.
#include "oracle.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;

namespace {
    std::string scratch(const std::string& name) {
        return (scratch_path("sgcl_codec_jxl_tests") / name).string();
    }

    std::string brew_tool(const char* name) {
        for (const char* dir : {"/opt/homebrew/bin/", "/usr/local/bin/"}) {
            const std::string p = std::string(dir) + name;
            if (std::filesystem::exists(p)) {
                return p;
            }
        }
        return {};
    }

    std::string read_file(const std::string& path) {
        std::ifstream is(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(is), {});
    }

    void write_file(const std::string& path, const std::string& data) {
        std::ofstream(path, std::ios::binary).write(data.data(), std::streamsize(data.size()));
    }

    // cjxl of `in` into a scratch file with the arguments; "" when it fails
    std::string cjxl(const std::string& in, const std::string& name, const std::string& args) {
        const std::string tool = brew_tool("cjxl");
        if (tool.empty()) {
            return {};
        }
        const std::string out = scratch(name);
        const std::string cmd = "'" + tool + "' " + args + " '" + in + "' '" + out + "' > /dev/null 2>&1";
        if (std::system(cmd.c_str()) != 0) {
            return {};
        }
        return read_file(out);
    }

    // djxl of a file into PNG, decoded by the module's PNG decoder
    optional<codec::image> djxl(const std::string& file, const std::string& name) {
        const std::string tool = brew_tool("djxl");
        if (tool.empty()) {
            return nullopt;
        }
        const std::string in = scratch(name + ".jxl"), out = scratch(name + ".png");
        write_file(in, file);
        const std::string cmd = "'" + tool + "' '" + in + "' '" + out + "' > /dev/null 2>&1";
        if (std::system(cmd.c_str()) != 0) {
            return nullopt;
        }
        auto im = codec::png::decode(bytes(read_file(out)));
        if (!im) {
            return nullopt;
        }
        return *im;
    }

    // The largest difference of two images' samples, in their own depth
    int max_difference(const codec::image& a, const codec::image& b) {
        EXPECT_EQ(a.width(), b.width());
        EXPECT_EQ(a.height(), b.height());
        EXPECT_EQ(a.format(), b.format());
        if (a.width() != b.width() || a.height() != b.height() || a.format() != b.format()) {
            return 1 << 30;
        }
        const bool deep = a.format() == pixel_format::rgb16 || a.format() == pixel_format::rgba16 || a.format() == pixel_format::gray16 ||
                          a.format() == pixel_format::gray_alpha16;
        int worst = 0;
        for (uint32_t y = 0; y < a.height(); ++y) {
            auto ra = a.row(y), rb = b.row(y);
            if (deep) {
                for (size_t i = 0; i + 1 < ra.size(); i += 2) {
                    uint16_t va, vb;
                    std::memcpy(&va, ra.data() + i, 2);
                    std::memcpy(&vb, rb.data() + i, 2);
                    worst = std::max(worst, std::abs(int(va) - int(vb)));
                }
            } else {
                for (size_t i = 0; i < ra.size(); ++i) {
                    worst = std::max(worst, std::abs(int(ra[i]) - int(rb[i])));
                }
            }
        }
        return worst;
    }

#if defined(__APPLE__)
#define JXL_NEEDS_TOOLS()                                                  \
    if (brew_tool("cjxl").empty() || brew_tool("djxl").empty()) {         \
        GTEST_SKIP() << "no cjxl/djxl (Homebrew's jpeg-xl)";               \
    }
#else
#define JXL_NEEDS_TOOLS() GTEST_SKIP() << "JPEG XL needs the system's codec (macOS)"
#endif
}

TEST(CodecJxl_Tests, LosslessAsTheSource) {
    JXL_NEEDS_TOOLS();
    struct Case {
        const char* png;
        pixel_format format;
    };
    const Case cases[] = {
        {"pngsuite/basn0g08.png", pixel_format::gray8},   {"pngsuite/basn0g16.png", pixel_format::gray16},
        {"pngsuite/basn2c08.png", pixel_format::rgb8},    {"pngsuite/basn2c16.png", pixel_format::rgb16},
        {"pngsuite/basn6a08.png", pixel_format::rgba8},   {"pngsuite/basn6a16.png", pixel_format::rgba16},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.png);
        CODEC_ORACLE(src, c.png);
        const std::string path = scratch(std::filesystem::path(c.png).filename().string());
        write_file(path, src);
        const std::string file = cjxl(path, std::string(std::filesystem::path(c.png).stem().string()) + ".jxl", "-d 0 -e 3");
        ASSERT_FALSE(file.empty());
        EXPECT_EQ(codec::sniff(bytes(file)), codec::format::jxl);
        auto im = codec::jxl::decode(bytes(file));
        ASSERT_TRUE(im) << im.error().message();
        EXPECT_EQ(im->format(), c.format);
        const codec::image want = codec::png::decode(bytes(src)).value();
        EXPECT_EQ(max_difference(*im, want.convert(c.format)), 0);
        // libjxl's own decoder: the same pixels
        if (auto oracle = djxl(file, std::string("oracle_") + std::filesystem::path(c.png).stem().string())) {
            EXPECT_EQ(max_difference(*im, oracle->convert(c.format)), 0);
        }
    }
}

TEST(CodecJxl_Tests, GrayWithAlpha) {
    // gray with alpha: decoded as the source where the system's ImageIO
    // decodes it right, errc::unsupported where it does not (macOS 26: its
    // pixels come out 0 and 255 whatever the file holds)
    JXL_NEEDS_TOOLS();
    for (const char* png : {"pngsuite/basn4a08.png", "pngsuite/basn4a16.png"}) {
        SCOPED_TRACE(png);
        CODEC_ORACLE(src, png);
        const std::string stem = std::filesystem::path(png).stem().string();
        const std::string path = scratch(stem + ".png");
        write_file(path, src);
        const std::string file = cjxl(path, stem + ".jxl", "-d 0");
        ASSERT_FALSE(file.empty());
        auto im = codec::jxl::decode(bytes(file));
        if (!im) {
            EXPECT_EQ(im.error().code(), codec::errc::unsupported);
            EXPECT_EQ(im.error().message(), "offset 0: jxl: gray with alpha, which this system's ImageIO decodes wrong");
            continue;
        }
        const codec::image want = codec::png::decode(bytes(src)).value();
        EXPECT_EQ(max_difference(*im, want.convert(im->format())), 0);
    }
}

TEST(CodecJxl_Tests, LossyAsDjxl) {
    JXL_NEEDS_TOOLS();
    CODEC_ORACLE(src, "pngsuite/basn2c08.png");
    const std::string path = scratch("lossy_src.png");
    write_file(path, src);
    for (const char* distance : {"1", "3"}) {
        SCOPED_TRACE(distance);
        const std::string file = cjxl(path, std::string("lossy") + distance + ".jxl", std::string("-d ") + distance);
        ASSERT_FALSE(file.empty());
        auto im = codec::jxl::decode(bytes(file));
        ASSERT_TRUE(im) << im.error().message();
        auto oracle = djxl(file, std::string("lossy_oracle") + distance);
        ASSERT_TRUE(oracle);
        // two builds of libjxl, the system's and Homebrew's, round apart
        EXPECT_LE(max_difference(*im, oracle->convert(im->format())), 2);
    }
}

TEST(CodecJxl_Tests, ContainerJpegAndAnimation) {
    JXL_NEEDS_TOOLS();
    CODEC_ORACLE(src, "pngsuite/basn2c08.png");
    const std::string path = scratch("container_src.png");
    write_file(path, src);
    // the container: the same image as the codestream
    const std::string bare = cjxl(path, "bare.jxl", "-d 0 --container=0");
    const std::string boxed = cjxl(path, "boxed.jxl", "-d 0 --container=1");
    ASSERT_FALSE(bare.empty());
    ASSERT_FALSE(boxed.empty());
    EXPECT_EQ(uint8_t(bare[0]), 0xFF);
    EXPECT_EQ(boxed.substr(4, 4), "JXL ");
    EXPECT_EQ(codec::sniff(bytes(boxed)), codec::format::jxl);
    EXPECT_EQ(max_difference(codec::jxl::decode(bytes(bare)).value(), codec::jxl::decode(bytes(boxed)).value()), 0);

    // a JPEG recompressed without loss: its orientation and EXIF kept
    codec::image photo(48, 32, pixel_format::rgb8);
    for (uint32_t y = 0; y < 32; ++y) {
        for (uint32_t x = 0; x < 48; ++x) {
            photo.row(y)[3 * x] = byte(x * 5);
            photo.row(y)[3 * x + 1] = byte(y * 7);
        }
    }
    // an EXIF block of the orientation 6 and a Make
    const unsigned char exif[] = {'I', 'I', 42, 0, 8, 0, 0, 0, 2, 0,
                                  0x0F, 0x01, 2, 0, 6, 0, 0, 0, 38, 0, 0, 0,
                                  0x12, 0x01, 3, 0, 1, 0, 0, 0, 6, 0, 0, 0,
                                  0, 0, 0, 0, 'S', 'G', 'C', 'L', '!', 0};
    photo.set_exif(slice<const byte>(reinterpret_cast<const byte*>(exif), sizeof(exif)));
    const std::string jpeg_path = scratch("photo.jpg");
    const vector<byte> jpeg = codec::jpeg::encode(photo).value();
    write_file(jpeg_path, std::string(reinterpret_cast<const char*>(jpeg.data()), jpeg.size()));
    const std::string recompressed = cjxl(jpeg_path, "photo.jxl", "");
    ASSERT_FALSE(recompressed.empty());
    auto back = codec::jxl::decode(bytes(recompressed));
    ASSERT_TRUE(back) << back.error().message();
    EXPECT_EQ(back->width(), 48u);
    EXPECT_EQ(back->height(), 32u);
    EXPECT_EQ(back->orientation(), 6u);
    auto m = codec::metadata::read(bytes(recompressed));
    ASSERT_TRUE(m);
    EXPECT_EQ(m->make(), optional<string>("SGCL!"));
    EXPECT_EQ(m->orientation(), 6u);
    EXPECT_FALSE(m->exif().empty());
    // against the JPEG's own pixels: libjxl reconstructs the JPEG's
    // coefficients, two IDCTs apart
    EXPECT_LE(max_difference(*back, codec::jpeg::decode(jpeg).value()), 3);

    // an animated GIF: the first frame, without loss
    CODEC_ORACLE(gif, "giflib/giflib-6.1.3/pic/fire.gif");
    const std::string gif_path = scratch("fire.gif");
    write_file(gif_path, gif);
    const std::string anim = cjxl(gif_path, "fire.jxl", "-d 0");
    ASSERT_FALSE(anim.empty());
    auto first = codec::jxl::decode(bytes(anim));
    ASSERT_TRUE(first) << first.error().message();
    const codec::image want = codec::gif::decode(bytes(gif)).value();
    EXPECT_EQ(max_difference(first->convert(pixel_format::rgba8), want), 0);
}

TEST(CodecJxl_Tests, OptionsStreamsAndEntryPoints) {
    JXL_NEEDS_TOOLS();
    CODEC_ORACLE(src, "pngsuite/basn6a16.png");
    const std::string path = scratch("entry_src.png");
    write_file(path, src);
    const std::string file = cjxl(path, "entry.jxl", "-d 0");
    ASSERT_FALSE(file.empty());
    const codec::image native = codec::jxl::decode(bytes(file)).value();
    EXPECT_EQ(native.format(), pixel_format::rgba16);
    // want: converted as asked
    auto small = codec::decode(bytes(file), {.want = pixel_format::rgb8});
    ASSERT_TRUE(small);
    EXPECT_EQ(small->format(), pixel_format::rgb8);
    EXPECT_EQ(max_difference(*small, native.convert(pixel_format::rgb8)), 0);
    EXPECT_EQ(codec::decode(bytes(file), {.want = pixel_format(200)}).error().code(), codec::errc::invalid_argument);
    // the limits
    auto limited = codec::decode(bytes(file), {.limits = {.max_pixels = 100}});
    ASSERT_FALSE(limited);
    EXPECT_EQ(limited.error().code(), codec::errc::too_large);
    // a stream, codec::decode of both, load; save is refused
    io::buffer in(bytes(file));
    EXPECT_EQ(max_difference(codec::jxl::decode(in).value(), native), 0);
    EXPECT_EQ(max_difference(codec::decode(bytes(file)).value(), native), 0);
    io::buffer in2(bytes(file));
    EXPECT_EQ(max_difference(codec::decode(in2).value(), native), 0);
    const std::string saved = scratch("entry_copy.jxl");
    write_file(saved, file);
    EXPECT_EQ(max_difference(codec::load(string(saved.c_str())).value(), native), 0);
    auto refused = native.save(string(scratch("out.jxl").c_str()));
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), codec::errc::unsupported);
    EXPECT_FALSE(std::filesystem::exists(scratch("out.jxl")));
}

TEST(CodecJxl_Tests, BrokenFiles) {
    // the signatures, and what is not one
    const std::string box = std::string("\0\0\0\x0cJXL \r\n\x87\n", 12);
    EXPECT_EQ(codec::sniff(bytes(std::string("\xff\x0a", 2))), codec::format::jxl);
    EXPECT_EQ(codec::sniff(bytes(box)), codec::format::jxl);
    EXPECT_EQ(codec::sniff(bytes(std::string("\xff\x0b", 2))), nullopt);
    EXPECT_EQ(codec::sniff(bytes(box.substr(0, 11))), nullopt);
#if defined(__APPLE__)
    // garbage behind a signature, and nothing behind it
    for (const std::string& file : {std::string("\xff\x0a", 2), std::string("\xff\x0a\x01\x02\x03\x04\x05\x06\x07\x08", 10), box}) {
        auto r = codec::jxl::decode(bytes(file));
        ASSERT_FALSE(r);
        EXPECT_TRUE(r.error().code() == codec::errc::corrupt || r.error().code() == codec::errc::unexpected_end ||
                    r.error().code() == codec::errc::unsupported)
            << r.error().message();
    }
    EXPECT_FALSE(codec::jxl::decode(slice<const byte>()));
    if (brew_tool("cjxl").empty()) {
        return;
    }
    CODEC_ORACLE(src, "pngsuite/basn2c08.png");
    const std::string path = scratch("broken_src.png");
    write_file(path, src);
    const std::string file = cjxl(path, "broken.jxl", "-d 1");
    ASSERT_FALSE(file.empty());
    // every cut of the file: an error or an image, never a crash
    for (size_t n = 1; n < file.size(); n += std::max<size_t>(1, file.size() / 40)) {
        auto r = codec::jxl::decode(bytes(file.substr(0, n)));
        if (r) {
            EXPECT_EQ(r->width(), 32u);
        }
    }
    // a PNG is not a JPEG XL to ImageIO
    auto wrong = codec::jxl::decode(bytes(src));
    ASSERT_FALSE(wrong);
    EXPECT_EQ(wrong.error().code(), codec::errc::corrupt);
#else
    auto r = codec::jxl::decode(bytes(std::string("\xff\x0a", 2)));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), codec::errc::unsupported);
#endif
}
