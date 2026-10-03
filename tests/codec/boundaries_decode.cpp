//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec's decoders at their boundaries (DESIGN 408): the sides at each
// format's limit, decode_options.limits at exactly the bound and one past
// it, animations of no frame and of one, the frames when the stream throws
// half-way, a moved-from frames. What the format's own tests hold is not
// repeated: every cut of a file (CodecPng_Tests.EveryCutIsUnexpectedEnd,
// CodecJpeg_Tests.EveryCutIsUnexpectedEnd, CodecGif_Tests.Negatives,
// CodecWebp_Tests.Negatives, CodecHeif_Tests.LimitsAndDamage), a stream that
// fails (CodecPng_Tests.AStreamThatFails), 1 × 1 PNGs of every type
// (CodecPng_Tests.ImagesMadeHere), a canvas of zero pixels and one past
// max_pixels (CodecGif_Tests.Negatives, CodecWebp_Tests.Negatives).
#include "png_builder.h"
#include "webp_builder.h"

#include <cstring>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;
namespace wb = codec_test::webp;

namespace {
    template<class T>
    codec::errc code_of(const expected<T, codec::error>& r) {
        return r ? codec::errc{} : r.error().code();
    }

    std::string text(const vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

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

    // A stream whose read throws once when it reaches byte `at`, and reads
    // on after it
    struct throws_once {
        const std::string* data;
        size_t at;
        size_t pos = 0;
        bool thrown = false;

        expected<size_t, io::error> read(const slice<std::byte>& b) {
            if (!thrown && pos >= at) {
                thrown = true;
                throw std::runtime_error("the stream's own failure");
            }
            size_t n = std::min({size_t(7), b.size(), data->size() - pos});
            if (!thrown) {
                n = std::min(n, at - pos);
            }
            std::memcpy(b.data(), data->data() + pos, n);
            pos += n;
            return n;
        }
    };

    // GIF: the logical screen of w × h with a global table of 4 colors and
    // `frames` images of fw × fh at (0, 0), each of index 1
    std::string le16(unsigned v) {
        return std::string{char(v & 0xFF), char(v >> 8)};
    }

    std::string gif_image(uint16_t left, uint16_t top, uint16_t w, uint16_t h, int delay = 0) {
        std::vector<std::byte> raw(size_t(w) * h, std::byte{1});
        auto z = compress::lzw::compress(slice<const std::byte>(raw.data(), raw.size()), compress::lzw::order::lsb, 2);
        std::string out = std::string("\x21\xF9\x04\x00", 4) + le16(unsigned(delay)) + std::string("\x00\x00", 2);
        out += char(0x2C) + le16(left) + le16(top) + le16(w) + le16(h) + char(0) + char(2);
        for (size_t i = 0; i < z.size(); i += 255) {
            const size_t n = std::min<size_t>(255, z.size() - i);
            out += char(n);
            out.append(reinterpret_cast<const char*>(z.data()) + i, n);
        }
        out += char(0);
        return out;
    }

    std::string gif_head(uint16_t w, uint16_t h) {
        return "GIF89a" + le16(w) + le16(h) + std::string{char(0x81), 0, 0} + std::string("\xFF\x00\x00\x00\xFF\x00\x00\x00\xFF\xFF\xFF\x00", 12);
    }

    std::string gif(uint16_t w, uint16_t h, int frames, uint16_t fw = 0, uint16_t fh = 0) {
        std::string out = gif_head(w, h);
        for (int i = 0; i < frames; ++i) {
            out += gif_image(0, 0, fw ? fw : w, fh ? fh : h, 10 * (i + 1));
        }
        return out + ";";
    }

    // A still VP8L WebP, and an animation of `frames` frames of the whole canvas
    std::string webp_still(uint32_t w, uint32_t h) {
        return wb::riff(wb::chunk("VP8L", wb::vp8l(w, h, wb::pattern(w, h, 1, true))));
    }

    std::string webp_animation(uint32_t w, uint32_t h, int frames) {
        std::string body = wb::vp8x(0x02 | 0x10, w, h) + wb::anim(3);
        for (int i = 0; i < frames; ++i) {
            body += wb::anmf(0, 0, w, h, 40, false, false, wb::chunk("VP8L", wb::vp8l(w, h, wb::pattern(w, h, unsigned(i + 2), true))));
        }
        return wb::riff(body);
    }

    // The frames read to their end: how many, or the error
    expected<int, codec::error> count(expected<codec::frames, codec::error> f) {
        if (!f) {
            return unexpected(f.error());
        }
        int n = 0;
        for (;;) {
            auto next = f->next();
            if (!next) {
                return unexpected(next.error());
            }
            if (!*next) {
                return n;
            }
            ++n;
        }
    }

    // A JPEG of the image, its SOF's height and width set to h and w
    std::string jpeg_with_sides(uint32_t h, uint32_t w) {
        std::string file = text(*codec::jpeg::encode(codec::image(8, 8, pixel_format::gray8)));
        const size_t sof = file.find("\xFF\xC0");
        file[sof + 5] = char(h >> 8);
        file[sof + 6] = char(h);
        file[sof + 7] = char(w >> 8);
        file[sof + 8] = char(w);
        return file;
    }
}

TEST(CodecDecodeBounds_Tests, PngSidesAtTheFormatsLimit) {
    // IHDR allows 2^31 - 1: there too_large by the default limits (a size,
    // not damage); 2^31 is no PNG: corrupt, whatever the limits say
    const codec::decode_options none{.limits = {.max_pixels = UINT64_MAX}};
    for (auto [w, h] : {std::pair{0x7FFFFFFFu, 1u}, std::pair{1u, 0x7FFFFFFFu}}) {
        const auto file = png_signature() + png_ihdr(w, h, 8, 0) + png_chunk("IEND", "");
        EXPECT_EQ(code_of(codec::png::decode(bytes(file))), codec::errc::too_large) << w << "x" << h;
    }
    for (auto [w, h] : {std::pair{0x80000000u, 1u}, std::pair{1u, 0x80000000u}, std::pair{0xFFFFFFFFu, 0xFFFFFFFFu}}) {
        const auto file = png_signature() + png_ihdr(w, h, 8, 0) + png_chunk("IEND", "");
        EXPECT_EQ(code_of(codec::png::decode(bytes(file), none)), codec::errc::corrupt) << w << "x" << h;
        EXPECT_EQ(code_of(codec::decode(bytes(file), none)), codec::errc::corrupt) << w << "x" << h;
    }
    // a side of 1 the other way: rows of one pixel, every depth of gray
    for (int depth : {1, 2, 4, 8, 16}) {
        const auto file = png_file(1, 300, depth, 0, random_rows(300, depth == 16 ? 2 : 1, 4));
        auto r = codec::png::decode(bytes(file));
        ASSERT_TRUE(r) << depth << ": " << r.error().message().view();
        EXPECT_EQ(r->height(), 300u);
    }
}

TEST(CodecDecodeBounds_Tests, JpegSidesAtTheFormatsLimit) {
    // SOF's 16 bits: 65 535 each way within the limits; a height of 0 (DNL)
    // unsupported, a width of 0 corrupt; past max_pixels too_large before
    // any scan is read
    EXPECT_EQ(code_of(codec::jpeg::decode(bytes(jpeg_with_sides(0, 8)))), codec::errc::unsupported);
    EXPECT_EQ(code_of(codec::jpeg::decode(bytes(jpeg_with_sides(8, 0)))), codec::errc::corrupt);
    EXPECT_EQ(code_of(codec::jpeg::decode(bytes(jpeg_with_sides(65535, 65535)))), codec::errc::too_large);
    // 65 535 × 1 and 1 × 65 535: the scan has the data of 8 × 8 only, so
    // EOI comes in the middle of the scan, but the size is taken
    EXPECT_EQ(code_of(codec::jpeg::decode(bytes(jpeg_with_sides(1, 65535)))), codec::errc::corrupt);
    EXPECT_EQ(code_of(codec::jpeg::decode(bytes(jpeg_with_sides(65535, 1)))), codec::errc::corrupt);
    // whole files of a side of 65 535 and one of 1
    for (auto [w, h] : {std::pair{65535u, 1u}, std::pair{1u, 65535u}}) {
        const codec::image long_one(w, h, pixel_format::rgb8);
        auto back = codec::jpeg::decode(*codec::jpeg::encode(long_one));
        ASSERT_TRUE(back) << w << "x" << h;
        EXPECT_EQ(back->width(), w);
        EXPECT_EQ(back->height(), h);
    }
}

TEST(CodecDecodeBounds_Tests, GifSidesAtTheFormatsLimit) {
    // a canvas of 65 535 × 1 and 1 × 65 535; 65 535 × 65 535 past the limits
    for (auto [w, h] : {std::pair{65535, 1}, std::pair{1, 65535}}) {
        auto r = codec::gif::decode(bytes(gif(uint16_t(w), uint16_t(h), 1)));
        ASSERT_TRUE(r) << w << "x" << h << ": " << r.error().message().view();
        EXPECT_EQ(r->width(), uint32_t(w));
        EXPECT_EQ(r->height(), uint32_t(h));
        EXPECT_EQ(uint8_t(r->pixels()[r->pixels().size() - 3]), 0xFFu);   // the last pixel is green, index 1
    }
    EXPECT_EQ(code_of(codec::gif::decode(bytes(gif(65535, 65535, 1, 1, 1)))), codec::errc::too_large);
    // a frame of no pixels, a frame wholly off the canvas, a frame at the
    // largest offset: taken, nothing drawn
    std::string odd = gif_head(4, 4) + gif_image(0, 0, 4, 4) + gif_image(0, 0, 0, 0) + gif_image(4, 0, 2, 2) + gif_image(65535, 65535, 1, 1) + ";";
    auto all = count(codec::gif::frames(bytes(odd)));
    ASSERT_TRUE(all) << all.error().message().view();
    EXPECT_EQ(*all, 4);
    codec::frames clip = *codec::gif::frames(bytes(odd));
    codec::image first = (*clip.next())->picture;
    for (int i = 0; i < 3; ++i) {
        codec::image later = (*clip.next())->picture;
        EXPECT_EQ(std::memcmp(later.pixels().data(), first.pixels().data(), first.pixels().size()), 0) << i;
    }
}

TEST(CodecDecodeBounds_Tests, WebpSidesAtTheFormatsLimit) {
    // VP8L's 14 bits: 16 384 each way
    for (auto [w, h] : {std::pair{16384u, 1u}, std::pair{1u, 16384u}}) {
        auto r = codec::webp::decode(bytes(webp_still(w, h)));
        ASSERT_TRUE(r) << w << "x" << h << ": " << r.error().message().view();
        EXPECT_EQ(r->width(), w);
        EXPECT_EQ(r->height(), h);
    }
    // VP8X's canvas: under 2^32 pixels too_large by the limits, 2^32 itself corrupt
    const auto px = wb::pattern(2, 2, 1, true);
    auto canvas = [&](uint32_t w, uint32_t h) {
        return wb::riff(wb::vp8x(0x10, w, h) + wb::chunk("VP8L", wb::vp8l(2, 2, px)));
    };
    const codec::decode_options none{.limits = {.max_pixels = UINT64_MAX}};
    EXPECT_EQ(code_of(codec::webp::decode(bytes(canvas(65536, 65535)))), codec::errc::too_large);
    EXPECT_EQ(code_of(codec::webp::decode(bytes(canvas(65536, 65536)), none)), codec::errc::corrupt);
    EXPECT_EQ(code_of(codec::webp::decode(bytes(canvas(1u << 24, 256)), none)), codec::errc::corrupt);
    // 2^24 × 1, the widest canvas, of an image of another size: corrupt as libwebp's
    EXPECT_EQ(code_of(codec::webp::decode(bytes(canvas(1u << 24, 1)))), codec::errc::corrupt);
}

TEST(CodecDecodeBounds_Tests, MaxPixelsAtTheBoundAndOnePast) {
    // exactly the pixels: decoded; one fewer allowed: too_large, for each
    // format, from memory and from a stream; 0 refuses every image
    const codec::image picture(7, 5, pixel_format::rgb8);
    const std::string files[] = {
        text(*codec::png::encode(picture)),
        text(*codec::jpeg::encode(picture)),
        gif(7, 5, 2),
        webp_still(7, 5),
        webp_animation(7, 5, 2),
    };
    for (const auto& file : files) {
        EXPECT_TRUE(codec::decode(bytes(file), {.limits = {.max_pixels = 35}})) << file.substr(0, 4);
        EXPECT_EQ(code_of(codec::decode(bytes(file), {.limits = {.max_pixels = 34}})), codec::errc::too_large) << file.substr(0, 4);
        EXPECT_EQ(code_of(codec::decode(bytes(file), {.limits = {.max_pixels = 0}})), codec::errc::too_large) << file.substr(0, 4);
        pieces p{&file, 3};
        EXPECT_TRUE(codec::decode(io::reader(p), {.limits = {.max_pixels = 35}})) << file.substr(0, 4);
        pieces q{&file, 3};
        EXPECT_EQ(code_of(codec::decode(io::reader(q), {.limits = {.max_pixels = 34}})), codec::errc::too_large) << file.substr(0, 4);
    }
    for (const std::string* file : {&files[2], &files[4]}) {
        EXPECT_EQ(count(codec::decode_frames(bytes(*file), {.limits = {.max_pixels = 35}})), 2);
        EXPECT_EQ(code_of(codec::decode_frames(bytes(*file), {.limits = {.max_pixels = 34}})), codec::errc::too_large);
    }
    // a GIF frame larger than the canvas: its own pixels against the bound
    const std::string wide = gif_head(6, 5) + gif_image(0, 0, 200, 1) + ";";
    EXPECT_TRUE(codec::gif::decode(bytes(wide), {.limits = {.max_pixels = 200}}));
    EXPECT_EQ(code_of(codec::gif::decode(bytes(wide), {.limits = {.max_pixels = 199}})), codec::errc::too_large);
}

TEST(CodecDecodeBounds_Tests, MaxMetadataAtTheBoundAndOnePast) {
    // a block of exactly max_metadata bytes is read, one byte more is
    // too_large; with metadata false nothing is read, whatever the bound
    std::string exif = std::string("MM\0*\0\0\0\x08\0\x01", 10) + std::string("\x01\x12\0\x03\0\0\0\x01\0\x06\0\0", 12) + std::string(4, '\0');
    exif += std::string(1000 - exif.size(), 'e');
    std::string icc(3000, '\0');
    std::mt19937 rng(8);
    for (auto& b : icc) {
        b = char(rng());   // bytes that do not compress: PNG's iCCP holds more than the profile
    }
    auto check = [&](const std::string& file, size_t size, const char* what) {
        auto at = codec::decode(bytes(file), {.limits = {.max_metadata = size}});
        ASSERT_TRUE(at) << what << ": " << at.error().message().view();
        EXPECT_TRUE(at->exif().size() == size || at->icc().size() == size) << what;
        EXPECT_EQ(code_of(codec::decode(bytes(file), {.limits = {.max_metadata = size - 1}})), codec::errc::too_large) << what;
        auto bare = codec::decode(bytes(file), {.limits = {.max_metadata = 0}, .metadata = false});
        ASSERT_TRUE(bare) << what;
        EXPECT_TRUE(bare->exif().empty() && bare->icc().empty()) << what;
        EXPECT_EQ(bare->orientation(), 1u) << what;
        pieces p{&file, 5};
        EXPECT_TRUE(codec::decode(io::reader(p), {.limits = {.max_metadata = size}})) << what;
    };
    const std::string rows = random_rows(2, 6, 1);
    check(png_file(2, 2, 8, 2, rows, png_chunk("eXIf", exif)), exif.size(), "png eXIf");
    check(png_file(2, 2, 8, 2, rows, png_chunk("iCCP", std::string("icc\0\0", 5) + zlib_of(icc))), icc.size(), "png iCCP");
    codec::image picture(4, 4, pixel_format::rgb8);
    std::vector<std::byte> e(exif.size()), c(icc.size());
    std::memcpy(e.data(), exif.data(), exif.size());
    std::memcpy(c.data(), icc.data(), icc.size());
    picture.set_exif(slice<const byte>(e.data(), e.size()));
    check(text(*codec::jpeg::encode(picture)), exif.size(), "jpeg EXIF");
    picture.set_exif({});
    picture.set_icc(slice<const byte>(c.data(), c.size()));
    check(text(*codec::jpeg::encode(picture)), icc.size(), "jpeg ICC");
    const std::string image = wb::chunk("VP8L", wb::vp8l(2, 2, wb::pattern(2, 2, 1, true)));
    check(wb::riff(wb::vp8x(0x10 | 0x08, 2, 2) + image + wb::chunk("EXIF", exif)), exif.size(), "webp EXIF");
    check(wb::riff(wb::vp8x(0x10 | 0x20, 2, 2) + wb::chunk("ICCP", icc) + image), icc.size(), "webp ICCP");
}

TEST(CodecDecodeBounds_Tests, AnimationsOfNoFrameAndOfOne) {
    // one frame: one, then the end, and the end again; the loop count of a
    // file that says nothing
    for (const std::string& file : {gif(3, 2, 1), webp_animation(4, 2, 1), webp_still(3, 2)}) {
        codec::frames clip = *codec::decode_frames(bytes(file));
        auto one = clip.next();
        ASSERT_TRUE(one && *one) << file.substr(0, 4);
        EXPECT_EQ((*one)->picture.width(), clip.width());
        for (int i = 0; i < 3; ++i) {
            auto end = clip.next();
            ASSERT_TRUE(end);
            EXPECT_FALSE(*end);
        }
        EXPECT_TRUE(codec::decode(bytes(file)));
    }
    EXPECT_EQ(codec::gif::frames(bytes(gif(3, 2, 1)))->loop_count(), 1u);
    EXPECT_EQ(codec::webp::frames(bytes(webp_animation(4, 2, 1)))->loop_count(), 3u);
    // no frame: decode has no image to give; the frames end in the same
    // error at their first next(), GIF as WebP (and as Go's DecodeAll)
    for (const std::string& file : {gif(3, 2, 0), webp_animation(4, 2, 0)}) {
        EXPECT_EQ(code_of(codec::decode(bytes(file))), codec::errc::corrupt) << file.substr(0, 4);
        auto clip = codec::decode_frames(bytes(file));
        ASSERT_TRUE(clip) << file.substr(0, 4);
        auto first = clip->next();
        ASSERT_FALSE(first) << file.substr(0, 4);
        EXPECT_EQ(first.error().code(), codec::errc::corrupt) << file.substr(0, 4);
        auto again = clip->next();
        ASSERT_FALSE(again);
        EXPECT_EQ(again.error(), first.error());
        pieces p{&file, 2};
        auto streamed = count(codec::decode_frames(io::reader(p)));
        ASSERT_FALSE(streamed);
        EXPECT_EQ(streamed.error(), first.error());
    }
}

TEST(CodecDecodeBounds_Tests, EveryCutOfAnAnimationThroughItsFrames) {
    // an animation cut anywhere, read frame by frame from memory and from a
    // stream: an error where the data ends, never a frame made of it, the
    // two the same
    for (const std::string& file : {gif(5, 3, 3), webp_animation(6, 4, 3)}) {
        const int whole = *count(codec::decode_frames(bytes(file)));
        ASSERT_EQ(whole, 3);
        for (size_t n = 0; n < file.size(); ++n) {
            const std::string cut = file.substr(0, n);
            auto from_memory = count(codec::decode_frames(bytes(cut)));
            pieces p{&cut, 3};
            auto from_stream = count(codec::decode_frames(io::reader(p)));
            ASSERT_EQ(bool(from_memory), bool(from_stream)) << n;
            if (n + 1 < file.size()) {
                ASSERT_FALSE(from_memory) << file.substr(0, 4) << " " << n;
            }
            if (!from_memory) {
                EXPECT_EQ(from_memory.error().code(), from_stream.error().code()) << n;
            }
        }
    }
}

TEST(CodecDecodeBounds_Tests, FramesWhoseStreamThrowsHalfWay) {
    // the stream's exception passes through next(); the reading is then
    // where the stream left it, and stops: every next() after it is an
    // error of errc::io, never a frame of a reading cut in two
    for (const std::string& file : {gif(9, 7, 4), webp_animation(8, 6, 4)}) {
        for (size_t at : {file.size() / 3, file.size() / 2, file.size() - 5}) {
            throws_once s{&file, at};
            auto clip = codec::decode_frames(io::reader(s));
            ASSERT_TRUE(clip);
            bool threw = false;
            int frames = 0;
            for (int i = 0; i < 10; ++i) {
                try {
                    auto n = clip->next();
                    if (threw) {
                        ASSERT_FALSE(n) << file.substr(0, 4) << " at " << at;
                        EXPECT_EQ(n.error().code(), codec::errc::io);
                        continue;
                    }
                    if (!n || !*n) {
                        break;
                    }
                    ++frames;
                } catch (const std::runtime_error&) {
                    EXPECT_FALSE(threw);
                    threw = true;
                }
            }
            EXPECT_TRUE(threw) << file.substr(0, 4) << " at " << at;
            EXPECT_LT(frames, 4);
        }
        // decode of a stream that throws: the exception, nothing kept
        throws_once s{&file, 20};
        EXPECT_THROW((void)codec::decode(io::reader(s)), std::runtime_error);
    }
}

TEST(CodecDecodeBounds_Tests, MovedFromFramesAreTheReading) {
    // a move copies the word, as a tracked_ptr's does: the moved-from
    // frames are another handle of the same reading
    const std::string file = gif(3, 2, 3);
    codec::frames source = *codec::gif::frames(bytes(file));
    codec::frames target = std::move(source);
    EXPECT_EQ(source.width(), 3u);
    EXPECT_EQ(source.height(), 2u);
    EXPECT_EQ(source.loop_count(), 1u);
    ASSERT_TRUE(*source.next());
    ASSERT_TRUE(*target.next());
    ASSERT_TRUE(*source.next());
    auto end = target.next();
    ASSERT_TRUE(end);
    EXPECT_FALSE(*end);
    codec::frames& self = target;
    target = std::move(self);
    EXPECT_EQ(target.width(), 3u);
    EXPECT_FALSE(*target.next());
}

TEST(CodecDecodeBounds_Tests, EmptyAndTheSignatureAlone) {
    // nothing, and each format's signature and nothing after it: unsupported
    // for what tells no format, unexpected_end for a format told
    EXPECT_EQ(code_of(codec::decode(slice<const byte>())), codec::errc::unsupported);
    EXPECT_EQ(code_of(codec::decode_frames(slice<const byte>())), codec::errc::unsupported);
    for (auto [head, code] : {std::pair{std::string("\x89PNG\r\n\x1a\n", 8), codec::errc::unexpected_end},
                              std::pair{std::string("\xFF\xD8\xFF", 3), codec::errc::unexpected_end},
                              std::pair{std::string("GIF89a"), codec::errc::unexpected_end},
                              std::pair{std::string("RIFF\x04\0\0\0WEBP", 12), codec::errc::corrupt}}) {
        EXPECT_EQ(code_of(codec::decode(bytes(head))), code) << head.substr(0, 4);
        pieces p{&head, 1};
        EXPECT_EQ(code_of(codec::decode(io::reader(p))), code) << head.substr(0, 4);
    }
    // small files of each format through a stream of one byte a read; a GIF
    // shorter than the 64 bytes decode reads to tell the format
    const std::string tiny[] = {gif(1, 1, 1), webp_still(1, 1), text(*codec::png::encode(codec::image(1, 1, pixel_format::gray8))),
                                text(*codec::jpeg::encode(codec::image(1, 1, pixel_format::gray8)))};
    ASSERT_LT(tiny[0].size(), codec::detail::SniffBytes);
    for (const auto& file : tiny) {
        pieces p{&file, 1};
        auto r = codec::decode(io::reader(p));
        ASSERT_TRUE(r) << file.substr(0, 4);
        EXPECT_EQ(r->width(), 1u);
    }
}

TEST(CodecDecodeBounds_Tests, EachFormatsOwnDecodeOfNothingAndOfAStreamThatFails) {
    // nothing: the data ends (HEIF: an error of ImageIO's, never the end of
    // the program); a stream failing at its first read: errc::io with the
    // stream's error at offset 0; a want outside the list before any read
    struct broken {
        expected<size_t, io::error> read(const slice<std::byte>&) {
            return unexpected(io::error(std::make_error_code(std::errc::io_error), "read", "test"));
        }
    };
    const slice<const byte> none;
    EXPECT_EQ(code_of(codec::png::decode(none)), codec::errc::unexpected_end);
    EXPECT_EQ(code_of(codec::jpeg::decode(none)), codec::errc::unexpected_end);
    EXPECT_EQ(code_of(codec::gif::decode(none)), codec::errc::unexpected_end);
    EXPECT_EQ(code_of(codec::webp::decode(none)), codec::errc::unexpected_end);
    EXPECT_EQ(code_of(codec::gif::frames(none)), codec::errc::unexpected_end);
    EXPECT_EQ(code_of(codec::webp::frames(none)), codec::errc::unexpected_end);
    EXPECT_NE(code_of(codec::heif::decode(none)), codec::errc{});
    auto io_at_zero = [](const auto& r) { return !r && r.error().code() == codec::errc::io && r.error().io_error() && r.error().offset() == 0; };
    broken b;
    EXPECT_TRUE(io_at_zero(codec::png::decode(io::reader(b))));
    EXPECT_TRUE(io_at_zero(codec::jpeg::decode(io::reader(b))));
    EXPECT_TRUE(io_at_zero(codec::gif::decode(io::reader(b))));
    EXPECT_TRUE(io_at_zero(codec::webp::decode(io::reader(b))));
    EXPECT_TRUE(io_at_zero(codec::gif::frames(io::reader(b))));
    EXPECT_TRUE(io_at_zero(codec::webp::frames(io::reader(b))));
    EXPECT_TRUE(io_at_zero(codec::decode(io::reader(b))));
    EXPECT_TRUE(io_at_zero(codec::decode_frames(io::reader(b))));
#if defined(__APPLE__)
    EXPECT_TRUE(io_at_zero(codec::heif::decode(io::reader(b))));
#endif
    const codec::decode_options odd{.want = static_cast<pixel_format>(9)};
    const std::string png = text(*codec::png::encode(codec::image(1, 1, pixel_format::gray8)));
    const std::string jpg = text(*codec::jpeg::encode(codec::image(1, 1, pixel_format::gray8)));
    EXPECT_EQ(code_of(codec::png::decode(bytes(png), odd)), codec::errc::invalid_argument);
    EXPECT_EQ(code_of(codec::jpeg::decode(bytes(jpg), odd)), codec::errc::invalid_argument);
    EXPECT_EQ(code_of(codec::gif::decode(bytes(gif(1, 1, 1)), odd)), codec::errc::invalid_argument);
    EXPECT_EQ(code_of(codec::webp::decode(bytes(webp_still(1, 1)), odd)), codec::errc::invalid_argument);
    EXPECT_EQ(code_of(codec::webp::frames(bytes(webp_still(1, 1)), odd)), codec::errc::invalid_argument);
    EXPECT_EQ(code_of(codec::png::decode(io::reader(b), odd)), codec::errc::invalid_argument);
}
