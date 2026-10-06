//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: BMP, TIFF, ICO/CUR, QOI and Netpbm (bmp.h, tiff.h, ico.h, qoi.h,
// pnm.h). Every pixel format through each encoder and back through the
// module's decoder, and through ffmpeg's (Homebrew's, the tests' oracle of
// these formats; ImageIO, through the HEIF oracle, for what ffmpeg reads
// wrong: tiled TIFF); files that ffmpeg writes (every TIFF compression, BMP,
// QOI, PAM, PGM, PBM, ICO) decoded as ffmpeg decodes them; BMP's palettes
// of 1 to 8 bits, RLE4 and RLE8, bit fields, the core header, top-down rows
// and alpha made here; TIFF's tiles, planes, 16 bits, palette, bilevel,
// WhiteIsZero, associated alpha and JPEG made here; sniffing, load and
// save; streams; truncated and malformed files and the limits.
#include "oracle.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;

namespace {
    std::string scratch(const std::string& name) {
        return (scratch_path("sgcl_codec_formats_tests") / name).string();
    }

    std::string save_bytes(const std::string& name, const vector<byte>& data) {
        const std::string path = scratch(name);
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
        return path;
    }

    std::string save_text(const std::string& name, const std::string& data) {
        const std::string path = scratch(name);
        std::ofstream(path, std::ios::binary).write(data.data(), std::streamsize(data.size()));
        return path;
    }

    vector<byte> bytes_of(const std::string& s) {
        vector<byte> v(s.size());
        std::memcpy(v.data(), s.data(), s.size());
        return v;
    }

    std::string rgba8(const codec::image& im) {
        codec::image c = im.convert(pixel_format::rgba8);
        return std::string(reinterpret_cast<const char*>(c.pixels().data()), c.pixels().size());
    }

    std::string pixels_of(const codec::image& im) {
        return std::string(reinterpret_cast<const char*>(im.pixels().data()), im.pixels().size());
    }

    bool have_ffmpeg() {
        static const bool yes = std::system("command -v ffmpeg > /dev/null 2>&1") == 0;
        return yes;
    }

    // ffmpeg's pixels of a file's first image, RGBA of 8 bits (of 16,
    // little-endian, as the module's 16-bit channels on this machine, when
    // asked); "" when it refuses the file
    std::string ffmpeg_rgba(const std::string& path, bool wide = false) {
        const std::string cmd = "ffmpeg -v error -i '" + path + "' -frames:v 1 -f rawvideo -pix_fmt " + (wide ? "rgba64le" : "rgba") + " - 2>/dev/null";
        FILE* p = popen(cmd.c_str(), "r");
        std::string out;
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        return pclose(p) == 0 ? out : std::string();
    }

    // A file ffmpeg writes from a PNG of the image, with its arguments
    std::string ffmpeg_make(const codec::image& im, const std::string& name, const std::string& args) {
        const std::string in = save_bytes(name + ".png", *codec::png::encode(im));
        const std::string out = scratch(name);
        const std::string cmd = "ffmpeg -v error -y -i '" + in + "' " + args + " '" + out + "' 2>&1";
        if (std::system(cmd.c_str()) != 0) {
            return "";
        }
        std::ifstream f(out, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), {});
    }

    codec::image noise(uint32_t w, uint32_t h, pixel_format f, uint32_t seed = 1) {
        codec::image im(w, h, f);
        std::mt19937 rng(seed);
        for (auto& b : im.pixels()) {
            b = std::byte(rng());
        }
        return im;
    }

    // Smooth content in rgb8: compressors find something to do
    codec::image picture(uint32_t w, uint32_t h) {
        codec::image im(w, h, pixel_format::rgb8);
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                im.row(y)[3 * x] = std::byte(x * 255 / w);
                im.row(y)[3 * x + 1] = std::byte(y * 255 / h);
                im.row(y)[3 * x + 2] = std::byte((x ^ y) & 0xF0);
            }
        }
        return im;
    }

    // What a format keeps of an image (8 bits a channel, gray stays gray)
    codec::image as_held(const codec::image& im, pixel_format held) {
        return im.convert(held);
    }

    struct pieces {
        const vector<byte>* data;
        size_t step;
        size_t at = 0;

        expected<size_t, io::error> read(const slice<std::byte>& b) {
            const size_t n = std::min({step, b.size(), data->size() - at});
            std::memcpy(b.data(), data->data() + at, n);
            at += n;
            return n;
        }
    };

    struct collect {
        vector<byte>* out;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            out->insert(out->end(), b.begin(), b.end());
            return b.size();
        }
    };

    // A little-endian writer of the test's own BMP and TIFF files
    struct Bytes {
        std::string s;
        void u8(unsigned v) { s += char(v); }
        void u16(unsigned v) { u8(v & 0xff); u8(v >> 8); }
        void u32(uint32_t v) { u16(v & 0xffff); u16(v >> 16); }
    };
}

// ---------------------------------------------------------------- QOI

TEST(CodecFormats_Tests, QoiRoundTripsAndFfmpegAgrees) {
    for (unsigned f = 0; f < 9; ++f) {
        for (auto im : {noise(37, 23, pixel_format(f), f), picture(64, 48).convert(pixel_format(f))}) {
            auto file = codec::qoi::encode(im);
            ASSERT_TRUE(file.has_value());
            auto back = codec::qoi::decode(*file);
            ASSERT_TRUE(back.has_value()) << back.error().message().view();
            const bool alpha = codec::detail::alpha(pixel_format(f));
            EXPECT_EQ(back->format(), alpha ? pixel_format::rgba8 : pixel_format::rgb8);
            EXPECT_EQ(rgba8(*back), rgba8(im)) << f;
            if (have_ffmpeg() && f == 3) {
                EXPECT_EQ(ffmpeg_rgba(save_bytes("ours.qoi", *file)), rgba8(im));
            }
        }
    }
    if (have_ffmpeg()) {
        for (auto im : {picture(80, 60), noise(33, 21, pixel_format::rgba8, 4)}) {
            const std::string theirs = ffmpeg_make(im, "theirs.qoi", "");
            ASSERT_FALSE(theirs.empty());
            auto back = codec::qoi::decode(bytes(theirs));
            ASSERT_TRUE(back.has_value());
            EXPECT_EQ(rgba8(*back), rgba8(im));
        }
    }
}

TEST(CodecFormats_Tests, QoiMalformed) {
    auto file = *codec::qoi::encode(picture(20, 10));
    for (size_t cut : {size_t(0), size_t(5), size_t(13), size_t(14), file.size() / 2}) {
        auto r = codec::qoi::decode(slice<const byte>(file.data(), cut));
        ASSERT_FALSE(r.has_value()) << cut;
        EXPECT_EQ(r.error().code(), cut < 4 ? codec::errc::unexpected_end : codec::errc::unexpected_end);
    }
    vector<byte> bad = file;
    bad[12] = std::byte(5);   // channels
    EXPECT_EQ(codec::qoi::decode(bad).error().code(), codec::errc::corrupt);
    bad = file;
    bad[0] = std::byte('x');
    EXPECT_EQ(codec::qoi::decode(bad).error().code(), codec::errc::corrupt);
    // the end marker is not required
    EXPECT_TRUE(codec::qoi::decode(slice<const byte>(file.data(), file.size() - 8)).has_value());
    // a huge size claimed by a small file
    bad = file;
    bad[4] = bad[8] = std::byte(0x7f);
    EXPECT_EQ(codec::qoi::decode(bad).error().code(), codec::errc::too_large);
}

// ---------------------------------------------------------------- PNM

TEST(CodecFormats_Tests, PnmEveryKindRoundTrips) {
    using kind = codec::pnm::kind;
    for (unsigned f = 0; f < 9; ++f) {
        codec::image im = noise(31, 17, pixel_format(f), f + 40);
        for (kind k : {kind::automatic, kind::pgm, kind::ppm, kind::pam, kind::pbm}) {
            for (bool plain : {false, true}) {
                const bool pam = k == kind::pam || (k == kind::automatic && codec::detail::alpha(pixel_format(f)));
                if (pam && plain) {
                    EXPECT_EQ(codec::pnm::encode(im, {.kind = k, .plain = true}).error().code(), codec::errc::invalid_argument);
                    continue;
                }
                auto file = codec::pnm::encode(im, {.kind = k, .plain = plain});
                ASSERT_TRUE(file.has_value());
                auto back = codec::pnm::decode(*file);
                ASSERT_TRUE(back.has_value()) << back.error().message().view();
                const bool w16 = codec::detail::wide(pixel_format(f));
                kind actual = k;
                if (k == kind::automatic) {
                    actual = codec::detail::alpha(pixel_format(f)) ? kind::pam : codec::detail::gray(pixel_format(f)) ? kind::pgm : kind::ppm;
                }
                pixel_format want;
                if (actual == kind::pbm) {
                    // black where the gray is below half
                    codec::image g = im.convert(pixel_format::gray8);
                    for (auto& b : g.pixels()) {
                        b = std::byte(uint8_t(b) < 128 ? 0 : 255);
                    }
                    EXPECT_EQ(pixels_of(*back), pixels_of(g)) << f;
                    continue;
                } else if (actual == kind::pgm) {
                    want = w16 ? pixel_format::gray16 : pixel_format::gray8;
                } else if (actual == kind::ppm) {
                    want = w16 ? pixel_format::rgb16 : pixel_format::rgb8;
                } else {
                    want = pixel_format(f) == pixel_format::cmyk8 ? pixel_format::rgb8 : pixel_format(f);
                }
                EXPECT_EQ(back->format(), want) << f << " " << int(actual);
                EXPECT_EQ(pixels_of(*back), pixels_of(im.convert(want))) << f << " " << int(actual) << " plain " << plain;
            }
        }
    }
}

TEST(CodecFormats_Tests, PnmFromFfmpegAndOddMaxvals) {
    if (have_ffmpeg()) {
        codec::image im = picture(50, 30);
        for (auto [name, args] : {std::pair{"a.pam", ""}, std::pair{"a.pgm", "-pix_fmt gray"}, std::pair{"a.pbm", "-pix_fmt monob"},
                                  std::pair{"a.ppm", ""}, std::pair{"b.pgm", "-pix_fmt gray16be"}}) {
            const std::string theirs = ffmpeg_make(im, name, args);
            ASSERT_FALSE(theirs.empty()) << name;
            auto back = codec::pnm::decode(bytes(theirs));
            ASSERT_TRUE(back.has_value()) << name << back.error().message().view();
            const bool w = codec::detail::wide(back->format());
            EXPECT_EQ(w ? pixels_of(back->convert(pixel_format::rgba16)) : rgba8(*back), ffmpeg_rgba(save_text(name, theirs), w)) << name;
        }
    }
    // maxval 15 scaled to the full range; comments; samples past maxval taken as maxval
    auto a = codec::pnm::decode(bytes(std::string("P2\n# a comment\n3 1 # more\n15\n0 15 16\n")));
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(pixels_of(*a), std::string("\x00\xff\xff", 3));
    auto b = codec::pnm::decode(bytes(std::string("P5 2 1 1000\n\x01\xf4\x03\xe8", 16)));
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(b->format(), pixel_format::gray16);
    uint16_t v[2];
    std::memcpy(v, b->pixels().data(), 4);
    EXPECT_EQ(v[0], uint16_t((500u * 65535 + 500) / 1000));
    EXPECT_EQ(v[1], 65535);
    auto c = codec::pnm::decode(bytes(std::string("P1\n4 2\n0101\n1 0 1 0\n")));
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(pixels_of(*c), std::string("\xff\x00\xff\x00\x00\xff\x00\xff", 8));
    auto d = codec::pnm::decode(bytes(std::string("P7\nWIDTH 1\nHEIGHT 1\nDEPTH 2\nMAXVAL 255\nTUPLTYPE GRAYSCALE_ALPHA\nENDHDR\n\x10\x20")));
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(d->format(), pixel_format::gray_alpha8);
    // malformed
    for (std::string bad : {std::string("P8\n1 1\n255\n"), std::string("P5\n1 1\n0\n\x00", 9), std::string("P5\n1 1\n70000\n"), std::string("P5\nx 1\n"),
                            std::string("P7\nWIDTH 1\nENDHDR\n"), std::string("P7\nWIDTH 1\nHEIGHT 1\nDEPTH 5\nMAXVAL 255\nENDHDR\n\x00", 41)}) {
        auto r = codec::pnm::decode(bytes(bad));
        EXPECT_FALSE(r.has_value()) << bad;
    }
    EXPECT_EQ(codec::pnm::decode(bytes(std::string("P6\n2 2\n255\n\x01\x02", 13))).error().code(), codec::errc::unexpected_end);
}

// ---------------------------------------------------------------- BMP

TEST(CodecFormats_Tests, BmpRoundTripsAndFfmpegAgrees) {
    for (unsigned f = 0; f < 9; ++f) {
        codec::image im = noise(29, 13, pixel_format(f), f + 60);
        auto file = codec::bmp::encode(im);
        ASSERT_TRUE(file.has_value());
        auto back = codec::bmp::decode(*file);
        ASSERT_TRUE(back.has_value()) << back.error().message().view();
        const bool alpha = codec::detail::alpha(pixel_format(f));
        const pixel_format held = alpha ? pixel_format::rgba8 : pixel_format(f) == pixel_format::gray8 ? pixel_format::gray8 : pixel_format::rgb8;
        EXPECT_EQ(back->format(), held) << f;
        EXPECT_EQ(pixels_of(*back), pixels_of(im.convert(held))) << f;
        if (have_ffmpeg()) {
            EXPECT_EQ(ffmpeg_rgba(save_bytes("ours.bmp", *file)), rgba8(*back)) << f;
        }
    }
    if (have_ffmpeg()) {
        for (auto [args, f] : {std::pair{"", pixel_format::rgb8}, std::pair{"-pix_fmt bgra", pixel_format::rgba8}, std::pair{"-pix_fmt rgb565le", pixel_format::rgb8},
                               std::pair{"-pix_fmt rgb555le", pixel_format::rgb8}, std::pair{"-pix_fmt pal8", pixel_format::rgb8},
                               std::pair{"-pix_fmt gray", pixel_format::gray8}, std::pair{"-pix_fmt monob", pixel_format::gray8}}) {
            const std::string theirs = ffmpeg_make(picture(45, 27).convert(f), "theirs.bmp", args);
            ASSERT_FALSE(theirs.empty()) << args;
            auto back = codec::bmp::decode(bytes(theirs));
            ASSERT_TRUE(back.has_value()) << args << " " << back.error().message().view();
            EXPECT_EQ(rgba8(*back), ffmpeg_rgba(save_text("theirs.bmp", theirs))) << args;
        }
    }
}

namespace {
    // A BMP made here: a header of `hsize`, the palette, the rows as given
    std::string make_bmp(int32_t w, int32_t h, unsigned bpp, unsigned compression, const std::vector<uint32_t>& palette, const std::string& rows,
                         unsigned hsize = 40, const std::vector<uint32_t>& masks = {}) {
        Bytes b;
        const unsigned extra = (compression == 3 || compression == 6) && hsize == 40 ? unsigned(masks.size()) * 4 : 0;
        const unsigned pal = unsigned(palette.size()) * (hsize == 12 ? 3 : 4);
        b.s = "BM";
        b.u32(uint32_t(14 + hsize + extra + pal + rows.size()));
        b.u32(0);
        b.u32(14 + hsize + extra + pal);
        b.u32(hsize);
        if (hsize == 12) {
            b.u16(unsigned(w));
            b.u16(unsigned(h));
            b.u16(1);
            b.u16(bpp);
        } else {
            b.u32(uint32_t(w));
            b.u32(uint32_t(h));
            b.u16(1);
            b.u16(bpp);
            b.u32(compression);
            b.u32(uint32_t(rows.size()));
            b.u32(0);
            b.u32(0);
            b.u32(uint32_t(palette.size()));
            b.u32(0);
            for (unsigned i = 40; i < hsize; i += 4) {
                const size_t k = (i - 40) / 4;
                b.u32(k < masks.size() && hsize > 40 ? masks[k] : 0);
            }
            if (extra) {
                for (uint32_t m : masks) {
                    b.u32(m);
                }
            }
        }
        for (uint32_t c : palette) {
            b.u8(c & 0xff);
            b.u8((c >> 8) & 0xff);
            b.u8((c >> 16) & 0xff);
            if (hsize != 12) {
                b.u8(0);
            }
        }
        return b.s + rows;
    }
}

TEST(CodecFormats_Tests, BmpVariantsMadeHere) {
    // 1 bit, 2 x 2, bottom-up: palette black and white
    {
        const std::string rows = std::string("\x40\0\0\0\x80\0\0\0", 8);   // bottom row 0 1, top row 1 0
        auto im = codec::bmp::decode(bytes(make_bmp(2, 2, 1, 0, {0x000000, 0xffffff}, rows)));
        ASSERT_TRUE(im.has_value()) << im.error().message().view();
        EXPECT_EQ(im->format(), pixel_format::gray8);
        EXPECT_EQ(pixels_of(*im), std::string("\xff\x00\x00\xff", 4));
    }
    // 4 bits, top-down, a colored palette
    {
        const std::string rows = std::string("\x12\0\0\0\x21\0\0\0", 8);
        auto im = codec::bmp::decode(bytes(make_bmp(2, -2, 4, 0, {0x000000, 0x0000ff, 0x00ff00}, rows)));
        ASSERT_TRUE(im.has_value()) << im.error().message().view();
        EXPECT_EQ(pixels_of(*im), std::string("\x00\x00\xff\x00\xff\x00\x00\xff\x00\x00\x00\xff", 12));
    }
    // the core header (12 bytes, a palette of 3 bytes an entry), 8 bits, bottom-up
    {
        const std::string rows = std::string("\x01\x00\0\0\x00\x01\0\0", 8);
        auto im = codec::bmp::decode(bytes(make_bmp(2, 2, 8, 0, {0x000000, 0x0000ff}, rows, 12)));
        ASSERT_TRUE(im.has_value()) << im.error().message().view();
        // the palette's 0x0000ff is blue (BGR); the top row is the second written
        EXPECT_EQ(pixels_of(*im), std::string("\x00\x00\x00\x00\x00\xff\x00\x00\xff\x00\x00\x00", 12));
    }
    // RLE8: a run, an absolute run, end of line, delta, end of bitmap; the skipped pixels transparent
    {
        const std::string rle = std::string("\x03\x01" "\x00\x03\x02\x01\x02\x00" "\x00\x00" "\x00\x02\x01\x00" "\x01\x02" "\x00\x01", 18);
        auto im = codec::bmp::decode(bytes(make_bmp(6, 2, 8, 1, {0x000000, 0xff0000, 0x00ff00}, rle)));
        ASSERT_TRUE(im.has_value()) << im.error().message().view();
        EXPECT_EQ(im->format(), pixel_format::rgba8);
        // bottom row (the image's last): 1 1 1 2 1 2; top row: (skip 1) 2 then nothing
        const std::string px = pixels_of(*im);
        const std::string red("\xff\x00\x00\xff", 4), green("\x00\xff\x00\xff", 4), clear(4, '\0');
        EXPECT_EQ(px.substr(24, 24), red + red + red + green + red + green);
        EXPECT_EQ(px.substr(0, 24), clear + green + clear + clear + clear + clear);
    }
    // RLE4: nibbles alternating in a run and in an absolute run
    {
        const std::string rle = std::string("\x04\x12" "\x00\x03\x21\x20" "\x00\x01", 8);
        auto im = codec::bmp::decode(bytes(make_bmp(7, 1, 4, 2, {0x000000, 0x0000ff, 0xff0000}, rle)));
        ASSERT_TRUE(im.has_value()) << im.error().message().view();
        const std::string blue("\x00\x00\xff\xff", 4), red("\xff\x00\x00\xff", 4);
        EXPECT_EQ(pixels_of(*im), blue + red + blue + red + red + blue + red);
    }
    // 16 bits 5-6-5 by bit fields under an INFO header; 32 bits with an alpha mask under V5
    {
        const std::string rows = std::string("\x1f\x00\xe0\x07", 4);   // blue, green
        auto im = codec::bmp::decode(bytes(make_bmp(2, 1, 16, 3, {}, rows, 40, {0xF800, 0x07E0, 0x001F})));
        ASSERT_TRUE(im.has_value()) << im.error().message().view();
        EXPECT_EQ(pixels_of(*im), std::string("\x00\x00\xff\x00\xff\x00", 6));
        const std::string px32 = std::string("\x10\x20\x30\x80", 4);
        auto a = codec::bmp::decode(bytes(make_bmp(1, 1, 32, 3, {}, px32, 124, {0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000})));
        ASSERT_TRUE(a.has_value()) << a.error().message().view();
        EXPECT_EQ(pixels_of(*a), std::string("\x30\x20\x10\x80", 4));
        // 32 bits without masks under INFO: opaque, as Go and Chromium read it
        auto o = codec::bmp::decode(bytes(make_bmp(1, 1, 32, 0, {}, px32)));
        ASSERT_TRUE(o.has_value());
        EXPECT_EQ(pixels_of(*o), std::string("\x30\x20\x10", 3));
    }
    // malformed
    EXPECT_EQ(codec::bmp::decode(bytes(std::string("BM"))).error().code(), codec::errc::unexpected_end);
    EXPECT_EQ(codec::bmp::decode(bytes(make_bmp(2, 2, 7, 0, {}, std::string(16, '\0')))).error().code(), codec::errc::corrupt);
    EXPECT_EQ(codec::bmp::decode(bytes(make_bmp(0, 2, 24, 0, {}, ""))).error().code(), codec::errc::corrupt);
    EXPECT_EQ(codec::bmp::decode(bytes(make_bmp(4, 4, 24, 0, {}, std::string(20, '\0')))).error().code(), codec::errc::unexpected_end);
    EXPECT_EQ(codec::bmp::decode(bytes(make_bmp(100000, 100000, 24, 0, {}, ""))).error().code(), codec::errc::too_large);
    EXPECT_EQ(codec::bmp::decode(bytes(make_bmp(2, 2, 24, 4, {}, ""))).error().code(), codec::errc::unsupported);
}

// ---------------------------------------------------------------- TIFF

TEST(CodecFormats_Tests, TiffRoundTripsEveryCompression) {
    using c = codec::tiff::compression;
    for (unsigned f = 0; f < 9; ++f) {
        for (auto im : {noise(41, 29, pixel_format(f), f + 80), picture(130, 70).convert(pixel_format(f))}) {
            for (c comp : {c::none, c::lzw, c::deflate}) {
                auto file = codec::tiff::encode(im, {.compression = comp});
                ASSERT_TRUE(file.has_value());
                auto back = codec::tiff::decode(*file);
                ASSERT_TRUE(back.has_value()) << back.error().message().view();
                EXPECT_EQ(back->format(), im.format()) << f;
                EXPECT_EQ(pixels_of(*back), pixels_of(im)) << f << " " << int(comp);
                if (have_ffmpeg() && (f == 0 || f == 2 || f == 3)) {
                    EXPECT_EQ(ffmpeg_rgba(save_bytes("ours.tif", *file)), rgba8(im)) << f << " " << int(comp);
                }
            }
        }
    }
}

TEST(CodecFormats_Tests, TiffFromFfmpegAndImageIo) {
    if (have_ffmpeg()) {
        for (const char* algo : {"raw", "packbits", "lzw", "deflate"}) {
            for (auto [fmt, f] : {std::pair{"rgb24", pixel_format::rgb8}, std::pair{"rgba", pixel_format::rgba8}, std::pair{"gray", pixel_format::gray8},
                                  std::pair{"rgb48le", pixel_format::rgb16}, std::pair{"gray16le", pixel_format::gray16},
                                  std::pair{"monob", pixel_format::gray8}, std::pair{"pal8", pixel_format::rgb8}}) {
                codec::image im = picture(77, 33).convert(f);
                const std::string theirs = ffmpeg_make(im, "theirs.tif", std::string("-pix_fmt ") + fmt + " -compression_algo " + algo);
                ASSERT_FALSE(theirs.empty()) << algo << " " << fmt;
                auto back = codec::tiff::decode(bytes(theirs));
                ASSERT_TRUE(back.has_value()) << algo << " " << fmt << " " << back.error().message().view();
                const bool w = codec::detail::wide(back->format());
                EXPECT_EQ(w ? pixels_of(back->convert(pixel_format::rgba16)) : rgba8(*back), ffmpeg_rgba(save_text("theirs.tif", theirs), w)) << algo << " " << fmt;
            }
        }
    }
    // ImageIO writes tiles of 256 (sips): the module reads them as ImageIO does
    if (!heif_oracle().empty()) {
        codec::image im = picture(600, 300);
        const std::string src = save_bytes("src.png", *codec::png::encode(im));
        const std::string out = scratch("imageio.tif");
        if (std::system(("sips -s format tiff '" + src + "' --out '" + out + "' > /dev/null 2>&1").c_str()) == 0) {
            std::ifstream f(out, std::ios::binary);
            const std::string data((std::istreambuf_iterator<char>(f)), {});
            auto back = codec::tiff::decode(bytes(data));
            ASSERT_TRUE(back.has_value()) << back.error().message().view();
            EXPECT_EQ(rgba8(*back), rgba8(im));
        }
    }
}

namespace {
    // A TIFF made here: one page of the given fields (tag, type, values)
    // and the chunks of data placed after the header
    struct Field {
        unsigned tag, type;
        std::vector<uint32_t> values;
    };

    std::string make_tiff(std::vector<Field> fields, const std::vector<std::string>& chunks, bool chunk_fields_are_tiles = false) {
        Bytes b;
        b.s = "II";
        b.u16(42);
        b.u32(0);
        std::vector<uint32_t> offsets, counts;
        for (const auto& c : chunks) {
            offsets.push_back(uint32_t(b.s.size()));
            counts.push_back(uint32_t(c.size()));
            b.s += c;
            if (b.s.size() & 1) {
                b.u8(0);
            }
        }
        fields.push_back({chunk_fields_are_tiles ? 324u : 273u, 4, offsets});
        fields.push_back({chunk_fields_are_tiles ? 325u : 279u, 4, counts});
        std::sort(fields.begin(), fields.end(), [](const Field& x, const Field& y) { return x.tag < y.tag; });
        // values that do not fit an entry
        std::vector<uint32_t> at(fields.size(), 0);
        for (size_t i = 0; i < fields.size(); ++i) {
            const unsigned size = fields[i].type == 3 ? 2 : fields[i].type == 1 || fields[i].type == 7 ? 1 : 4;
            if (fields[i].values.size() * size > 4) {
                at[i] = uint32_t(b.s.size());
                for (uint32_t v : fields[i].values) {
                    size == 2 ? b.u16(v) : size == 1 ? b.u8(v) : b.u32(v);
                }
                if (b.s.size() & 1) {
                    b.u8(0);
                }
            }
        }
        const uint32_t ifd = uint32_t(b.s.size());
        b.s[4] = char(ifd);
        b.s[5] = char(ifd >> 8);
        b.s[6] = char(ifd >> 16);
        b.s[7] = char(ifd >> 24);
        b.u16(unsigned(fields.size()));
        for (size_t i = 0; i < fields.size(); ++i) {
            const auto& fl = fields[i];
            const unsigned size = fl.type == 3 ? 2 : fl.type == 1 || fl.type == 7 ? 1 : 4;
            b.u16(fl.tag);
            b.u16(fl.type);
            b.u32(uint32_t(fl.values.size()));
            if (at[i]) {
                b.u32(at[i]);
            } else {
                Bytes v;
                for (uint32_t x : fl.values) {
                    size == 2 ? v.u16(x) : size == 1 ? v.u8(x) : v.u32(x);
                }
                v.s.resize(4, '\0');
                b.s += v.s;
            }
        }
        b.u32(0);
        return b.s;
    }
}

TEST(CodecFormats_Tests, TiffVariantsMadeHere) {
    // planar RGB, 2 strips per plane
    {
        const std::string r = "\x01\x02\x03\x04", g = "\x11\x12\x13\x14", bl = "\x21\x22\x23\x24";
        auto im = codec::tiff::decode(bytes(make_tiff({{256, 3, {2}}, {257, 3, {2}}, {258, 3, {8, 8, 8}}, {262, 3, {2}}, {277, 3, {3}}, {278, 3, {1}}, {284, 3, {2}}},
                                                      {r.substr(0, 2), r.substr(2), g.substr(0, 2), g.substr(2), bl.substr(0, 2), bl.substr(2)})));
        ASSERT_TRUE(im.has_value()) << im.error().message().view();
        EXPECT_EQ(pixels_of(*im), std::string("\x01\x11\x21\x02\x12\x22\x03\x13\x23\x04\x14\x24", 12));
    }
    // tiles of 16 x 16 over a 20 x 18 gray image, PackBits
    {
        std::vector<std::string> tiles;
        std::string want(20 * 18, '\0');
        for (int t = 0; t < 4; ++t) {
            std::string raw(256, '\0');
            for (int y = 0; y < 16; ++y) {
                for (int x = 0; x < 16; ++x) {
                    const int gx = (t % 2) * 16 + x, gy = (t / 2) * 16 + y;
                    raw[size_t(y * 16 + x)] = char(gx * 7 + gy * 3);
                    if (gx < 20 && gy < 18) {
                        want[size_t(gy * 20 + gx)] = char(gx * 7 + gy * 3);
                    }
                }
            }
            std::vector<uint8_t> packed;
            codec::detail::tiff_codec::packbits(reinterpret_cast<const uint8_t*>(raw.data()), raw.size(), packed);
            tiles.emplace_back(reinterpret_cast<const char*>(packed.data()), packed.size());
        }
        auto im = codec::tiff::decode(bytes(make_tiff({{256, 3, {20}}, {257, 3, {18}}, {258, 3, {8}}, {259, 3, {32773}}, {262, 3, {1}}, {322, 3, {16}}, {323, 3, {16}}},
                                                      tiles, true)));
        ASSERT_TRUE(im.has_value()) << im.error().message().view();
        EXPECT_EQ(pixels_of(*im), want);
    }
    // a palette of 4 bits; bilevel WhiteIsZero; 4-bit gray
    {
        std::vector<uint32_t> map(48, 0);
        map[1] = 65535;           // red of 1
        map[16 + 2] = 65535;      // green of 2
        auto im = codec::tiff::decode(bytes(make_tiff({{256, 3, {3}}, {257, 3, {1}}, {258, 3, {4}}, {262, 3, {3}}, {320, 3, map}}, {std::string("\x12\x00", 2)})));
        ASSERT_TRUE(im.has_value()) << im.error().message().view();
        EXPECT_EQ(pixels_of(*im), std::string("\xff\x00\x00\x00\xff\x00\x00\x00\x00", 9));
        auto bw = codec::tiff::decode(bytes(make_tiff({{256, 3, {4}}, {257, 3, {1}}, {258, 3, {1}}, {262, 3, {0}}}, {std::string("\xa0", 1)})));
        ASSERT_TRUE(bw.has_value());
        EXPECT_EQ(pixels_of(*bw), std::string("\x00\xff\x00\xff", 4));
        auto g4 = codec::tiff::decode(bytes(make_tiff({{256, 3, {2}}, {257, 3, {1}}, {258, 3, {4}}, {262, 3, {1}}}, {std::string("\x0f", 1)})));
        ASSERT_TRUE(g4.has_value());
        EXPECT_EQ(pixels_of(*g4), std::string("\x00\xff", 2));
    }
    // associated alpha divided out; 16 bits big-endian
    {
        auto im = codec::tiff::decode(bytes(make_tiff({{256, 3, {1}}, {257, 3, {1}}, {258, 3, {8, 8, 8, 8}}, {262, 3, {2}}, {277, 3, {4}}, {338, 3, {1}}},
                                                      {std::string("\x40\x20\x00\x80", 4)})));
        ASSERT_TRUE(im.has_value());
        EXPECT_EQ(pixels_of(*im), std::string("\x80\x40\x00\x80", 4));
    }
    // JPEG in TIFF: the module's JPEG of a picture, with its tables apart
    {
        codec::image pic = picture(32, 16);
        auto jpg = *codec::jpeg::encode(pic, {.quality = 90});
        const std::string j(reinterpret_cast<const char*>(jpg.data()), jpg.size());
        auto im = codec::tiff::decode(bytes(make_tiff({{256, 3, {32}}, {257, 3, {16}}, {258, 3, {8, 8, 8}}, {259, 3, {7}}, {262, 3, {6}}, {277, 3, {3}}}, {j})));
        ASSERT_TRUE(im.has_value()) << im.error().message().view();
        EXPECT_EQ(pixels_of(*im), pixels_of(*codec::jpeg::decode(jpg)));
    }
    // multi-page: written and read back
    {
        codec::image a = picture(10, 5), b = noise(3, 7, pixel_format::gray16);
        vector<codec::image> pages;
        pages.push_back(a);
        pages.push_back(b);
        auto file = codec::tiff::encode(pages);
        ASSERT_TRUE(file.has_value());
        auto all = codec::tiff::decode_all(*file);
        ASSERT_TRUE(all.has_value());
        ASSERT_EQ(all->size(), 2u);
        EXPECT_EQ(pixels_of((*all)[0]), pixels_of(a));
        EXPECT_EQ(pixels_of((*all)[1]), pixels_of(b));
    }
    // malformed: a loop of IFDs, a strip outside the file, an unknown compression, cut data
    {
        std::string loop = make_tiff({{256, 3, {1}}, {257, 3, {1}}, {258, 3, {8}}, {262, 3, {1}}}, {std::string("\x05", 1)});
        const uint32_t ifd = uint32_t(uint8_t(loop[4])) | uint32_t(uint8_t(loop[5])) << 8;
        const size_t next = ifd + 2 + 6 * 12;   // after the six fields
        loop[next] = loop[4];
        loop[next + 1] = loop[5];
        loop[next + 2] = loop[6];
        loop[next + 3] = loop[7];
        EXPECT_EQ(codec::tiff::decode_all(bytes(loop)).error().code(), codec::errc::corrupt);
        EXPECT_EQ(codec::tiff::decode(bytes(make_tiff({{256, 3, {1}}, {257, 3, {1}}, {258, 3, {8}}, {259, 3, {2}}, {262, 3, {1}}}, {std::string("\x05", 1)}))).error().code(),
                  codec::errc::unsupported);
        EXPECT_EQ(codec::tiff::decode(bytes(make_tiff({{256, 3, {4}}, {257, 3, {4}}, {258, 3, {8}}, {262, 3, {1}}}, {std::string("\x05", 1)}))).error().code(),
                  codec::errc::unexpected_end);
        EXPECT_EQ(codec::tiff::decode(bytes(std::string("II\x2b\0\x08\0\0\0", 8))).error().code(), codec::errc::unsupported);
        EXPECT_EQ(codec::tiff::decode(bytes(std::string("IX\x2a\0", 4))).error().code(), codec::errc::unexpected_end);
        EXPECT_EQ(codec::tiff::decode(bytes(make_tiff({{256, 4, {100000}}, {257, 4, {100000}}, {258, 3, {8}}, {262, 3, {1}}}, {std::string("\x05", 1)}))).error().code(),
                  codec::errc::too_large);
    }
}

TEST(CodecFormats_Tests, TiffLzwAgainstItsOwnWriterAtEveryWidth) {
    // strings long enough to pass 512, 1024, 2048 and 4094 entries
    std::mt19937 rng(3);
    for (size_t n : {size_t(1), size_t(2), size_t(510), size_t(511), size_t(5000), size_t(100000)}) {
        for (int kind = 0; kind < 3; ++kind) {
            std::vector<uint8_t> in(n);
            for (auto& v : in) {
                v = kind == 0 ? uint8_t(rng()) : kind == 1 ? uint8_t(rng() % 4) : uint8_t(7);
            }
            std::vector<uint8_t> z;
            codec::detail::tiff_codec::LzwWriter w;
            w.write(in.data(), n, z);
            w.finish(z);
            std::vector<uint8_t> out(n);
            bool ok;
            ASSERT_EQ(codec::detail::tiff_codec::unlzw(z.data(), z.size(), out.data(), n, ok), n);
            ASSERT_TRUE(ok);
            ASSERT_EQ(out, in) << n << " " << kind;
        }
    }
}

// ---------------------------------------------------------------- ICO

TEST(CodecFormats_Tests, IcoRoundTripsAndOracles) {
    vector<codec::image> sizes;
    sizes.push_back(noise(16, 16, pixel_format::rgba8, 1));
    sizes.push_back(picture(32, 32));
    sizes.push_back(noise(256, 256, pixel_format::rgba8, 2));
    auto file = codec::ico::encode(sizes);
    ASSERT_TRUE(file.has_value());
    auto all = codec::ico::decode_all(*file);
    ASSERT_TRUE(all.has_value()) << all.error().message().view();
    ASSERT_EQ(all->size(), 3u);
    // BMP entries: alpha 0 pixels transparent black? the color is kept with its alpha
    EXPECT_EQ(rgba8((*all)[0]), rgba8(sizes[0]));
    EXPECT_EQ(rgba8((*all)[1]), rgba8(sizes[1]));
    EXPECT_EQ(rgba8((*all)[2]), rgba8(sizes[2]));
    auto largest = codec::ico::decode(*file);
    ASSERT_TRUE(largest.has_value());
    EXPECT_EQ(largest->width(), 256u);
    // CUR, and through load and save
    auto cur = codec::ico::encode(picture(24, 24), {.cursor = true, .hotspot_x = 3, .hotspot_y = 4});
    ASSERT_TRUE(cur.has_value());
    EXPECT_EQ(rgba8(*codec::ico::decode(*cur)), rgba8(picture(24, 24)));
    if (have_ffmpeg()) {
        const std::string theirs = ffmpeg_make(picture(48, 48).convert(pixel_format::rgba8), "theirs.ico", "-f ico");
        ASSERT_FALSE(theirs.empty());
        auto back = codec::ico::decode(bytes(theirs));
        ASSERT_TRUE(back.has_value()) << back.error().message().view();
        EXPECT_EQ(rgba8(*back), ffmpeg_rgba(save_text("theirs.ico", theirs)));
        EXPECT_EQ(ffmpeg_rgba(save_bytes("ours.ico", *codec::ico::encode(picture(48, 48)))), rgba8(picture(48, 48)));
    }
    // refusals and malformed
    EXPECT_EQ(codec::ico::encode(picture(257, 10)).error().code(), codec::errc::invalid_argument);
    vector<codec::image> none;
    EXPECT_EQ(codec::ico::encode(none).error().code(), codec::errc::invalid_argument);
    EXPECT_EQ(codec::ico::decode(bytes(std::string("\0\0\1\0\0\0", 6))).error().code(), codec::errc::corrupt);
    EXPECT_EQ(codec::ico::decode(bytes(std::string("\0\0\1\0\1\0", 6))).error().code(), codec::errc::unexpected_end);
    vector<byte> cut(file->begin(), file->begin() + 100);
    EXPECT_FALSE(codec::ico::decode(cut).has_value());
}

// ---------------------------------------------------------------- the module

TEST(CodecFormats_Tests, SniffDecodeLoadSave) {
    codec::image im = picture(20, 12);
    struct Case {
        const char* ext;
        codec::format f;
    };
    for (Case c : {Case{"bmp", codec::format::bmp}, Case{"tif", codec::format::tiff}, Case{"tiff", codec::format::tiff}, Case{"ico", codec::format::ico},
                   Case{"cur", codec::format::ico}, Case{"qoi", codec::format::qoi}, Case{"ppm", codec::format::pnm}, Case{"pgm", codec::format::pnm},
                   Case{"pbm", codec::format::pnm}, Case{"pam", codec::format::pnm}, Case{"pnm", codec::format::pnm}}) {
        const std::string path = scratch(std::string("saved.") + c.ext);
        auto r = im.save(string(path.c_str()));
        ASSERT_TRUE(r.has_value()) << c.ext << " " << r.error().message().view();
        std::ifstream f(path, std::ios::binary);
        const std::string data((std::istreambuf_iterator<char>(f)), {});
        EXPECT_EQ(codec::sniff(bytes(data)), c.f) << c.ext;
        auto back = codec::load(string(path.c_str()));
        ASSERT_TRUE(back.has_value()) << c.ext << " " << back.error().message().view();
        EXPECT_EQ(back->width(), 20u);
        // through a stream in pieces as from memory
        vector<byte> v = bytes_of(data);
        pieces p{&v, 7};
        auto streamed = codec::decode(io::reader(p));
        ASSERT_TRUE(streamed.has_value()) << c.ext;
        EXPECT_EQ(pixels_of(*streamed), pixels_of(*back)) << c.ext;
    }
    // the streams' bytes the vectors'
    {
        vector<byte> a, b2, c2, d;
        collect ca{&a}, cb{&b2}, cc{&c2}, cd{&d};
        ASSERT_TRUE(codec::bmp::encode(im, io::writer(ca)).has_value());
        ASSERT_TRUE(codec::tiff::encode(im, io::writer(cb)).has_value());
        ASSERT_TRUE(codec::qoi::encode(im, io::writer(cc)).has_value());
        ASSERT_TRUE(codec::pnm::encode(im, io::writer(cd)).has_value());
        EXPECT_EQ(a, *codec::bmp::encode(im));
        EXPECT_EQ(b2, *codec::tiff::encode(im));
        EXPECT_EQ(c2, *codec::qoi::encode(im));
        EXPECT_EQ(d, *codec::pnm::encode(im));
    }
    // decode_options: want and the limits
    vector<byte> q = *codec::qoi::encode(im);
    EXPECT_EQ(codec::decode(q, {.want = pixel_format::gray16})->format(), pixel_format::gray16);
    EXPECT_EQ(codec::decode(q, {.limits = {.max_pixels = 100}}).error().code(), codec::errc::too_large);
    EXPECT_EQ(codec::qoi::decode(q, {.want = pixel_format(99)}).error().code(), codec::errc::invalid_argument);
}
