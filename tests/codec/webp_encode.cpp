//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: the WebP encoder (webp.h, detail/webp_encoder.h, vp8l_encoder.h,
// vp8_encoder.h). Lossless files come back pixel for pixel through the
// module's decoder, libwebp (WebPDecodeRGBA) and Go's x/image/webp, the RGB
// of transparent pixels too, at every effort, for photos, palettes of every
// bundling, alpha and every pixel format; lossy files decode in libwebp to
// the module's pixels bit for bit, at a PSNR that grows with the quality,
// their alpha exact; animations frame by frame through the module and
// libwebp's WebPAnimDecoder, delays and loop counts; metadata; the
// refusals, a failing stream, the stream's bytes the vector's, save; and
// the vector kernels of the VP8 encoder against their plain twins.
#include "oracle.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;

namespace {
    std::string write_scratch(const std::string& name, const vector<byte>& data) {
        auto path = (scratch_path("sgcl_codec_webp_encode_tests") / name).string();
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
        return path;
    }

    std::string rgba_of(const codec::image& im) {
        codec::image rgba = im.convert(pixel_format::rgba8);
        auto px = rgba.pixels();
        return std::string(reinterpret_cast<const char*>(px.data()), px.size());
    }

    // A smooth photo-like picture, rgb8
    codec::image photo(uint32_t w, uint32_t h, uint32_t seed = 0) {
        codec::image im(w, h, pixel_format::rgb8);
        std::mt19937 rng(seed);
        for (uint32_t y = 0; y < h; ++y) {
            auto row = im.row(y);
            for (uint32_t x = 0; x < w; ++x) {
                const double fx = double(x) / w, fy = double(y) / h;
                row[3 * x] = std::byte(uint8_t(255 * fx * (0.8 + 0.2 * std::sin(7 * fy)) + rng() % 3));
                row[3 * x + 1] = std::byte(uint8_t(255 * fy));
                row[3 * x + 2] = std::byte(uint8_t(127 + 120 * std::sin(10 * fx * fy + seed)));
            }
        }
        return im;
    }

    // An image of `colors` colors in runs and noise, rgba8, alpha from the
    // colors (some transparent with RGB not zero)
    codec::image palette(uint32_t w, uint32_t h, unsigned colors, uint32_t seed = 1) {
        std::mt19937 rng(seed);
        std::vector<uint32_t> table(colors);
        for (auto& c : table) {
            c = rng();
        }
        codec::image im(w, h, pixel_format::rgba8);
        for (uint32_t y = 0; y < h; ++y) {
            auto row = im.row(y);
            for (uint32_t x = 0; x < w; ++x) {
                const uint32_t c = table[((x / 4) * 3 + y * 5 + (rng() % 5 == 0 ? rng() : 0)) % colors];
                for (int k = 0; k < 4; ++k) {
                    row[4 * x + k] = std::byte(c >> (8 * k));
                }
            }
        }
        return im;
    }

    // A photo with an alpha gradient, fully transparent pixels at the left
    // keeping their colors
    codec::image with_alpha(uint32_t w, uint32_t h) {
        codec::image im = photo(w, h, 3).convert(pixel_format::rgba8);
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                const int a = int(255.0 * x / w);
                im.row(y)[4 * x + 3] = std::byte(a < 30 ? 0 : a);
            }
        }
        return im;
    }

    double psnr(const codec::image& a, const codec::image& b) {
        codec::image x = a.convert(pixel_format::rgb8), y = b.convert(pixel_format::rgb8);
        auto p = x.pixels(), q = y.pixels();
        double sum = 0;
        for (size_t i = 0; i < p.size(); ++i) {
            const double d = double(uint8_t(p[i])) - double(uint8_t(q[i]));
            sum += d * d;
        }
        const double mse = sum / double(p.size());
        return mse == 0 ? 99 : 10 * std::log10(255.0 * 255.0 / mse);
    }

    // The file's pixels by libwebp and Go, against the module's decoding
    void oracles_agree(const vector<byte>& file, const std::string& name, const std::string& expected_rgba) {
        const std::string path = write_scratch(name, file);
        if (!c_oracle().empty()) {
            auto theirs = run_oracle(c_oracle(), "webp", path);
            ASSERT_TRUE(theirs.has_value()) << "libwebp refused " << name;
            EXPECT_EQ(theirs->pixels, expected_rgba) << "libwebp " << name;
        }
        if (!go_webp_oracle().empty()) {
            auto theirs = run_oracle(go_webp_oracle(), "webp", path);
            ASSERT_TRUE(theirs.has_value()) << "Go refused " << name;
            EXPECT_EQ(theirs->pixels, expected_rgba) << "Go " << name;
        }
    }

    struct collect {
        vector<byte>* out;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            out->insert(out->end(), b.begin(), b.end());
            return b.size();
        }
    };

    struct failing {
        size_t room;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            if (b.size() > room) {
                return unexpected(io::error(std::make_error_code(std::errc::no_space_on_device), "write", "test"));
            }
            room -= b.size();
            return b.size();
        }
    };

    struct Frames {
        std::string pixels;
        std::vector<int64_t> delays_ms;
        uint32_t plays = 0;
        size_t count = 0;
    };

    Frames read_frames(const vector<byte>& file) {
        Frames r;
        auto f = codec::webp::frames(file);
        EXPECT_TRUE(f.has_value());
        if (!f) {
            return r;
        }
        for (;;) {
            auto n = f->next();
            EXPECT_TRUE(n.has_value()) << n.error().message().view();
            if (!n || !*n) {
                break;
            }
            r.pixels += rgba_of((*n)->picture);
            r.delays_ms.push_back((*n)->delay.milliseconds());
            ++r.count;
        }
        r.plays = f->loop_count();
        return r;
    }
}

TEST(CodecWebpEncode_Tests, LosslessExactThroughEveryDecoder) {
    int k = 0;
    for (auto im : {photo(97, 61), palette(64, 40, 2), palette(63, 41, 3), palette(65, 33, 11), palette(50, 50, 200),
                    with_alpha(120, 77), photo(1, 1), photo(1, 37), photo(53, 1), palette(16, 16, 1)}) {
        for (int effort : {1, 25, 75, 100}) {
            auto file = codec::webp::encode(im, {.lossless = true, .quality = effort});
            ASSERT_TRUE(file.has_value());
            auto back = codec::webp::decode(*file);
            ASSERT_TRUE(back.has_value()) << back.error().message().view();
            const std::string want = rgba_of(im);
            EXPECT_EQ(rgba_of(*back), want) << "image " << k << " effort " << effort;
            if (effort == 75) {
                oracles_agree(*file, "lossless_" + std::to_string(k) + ".webp", want);
            }
        }
        ++k;
    }
}

TEST(CodecWebpEncode_Tests, LosslessEveryPixelFormat) {
    for (unsigned f = 0; f < 9; ++f) {
        codec::image im(23, 17, pixel_format(f));
        auto px = im.pixels();
        for (size_t i = 0; i < px.size(); ++i) {
            px[i] = std::byte((i * 131) ^ (i >> 3));
        }
        auto file = codec::webp::encode(im, {.lossless = true});
        ASSERT_TRUE(file.has_value());
        // what WebP holds: 8 bits RGBA (16-bit channels and CMYK as
        // image::convert gives them)
        EXPECT_EQ(rgba_of(*codec::webp::decode(*file)), rgba_of(im)) << "format " << f;
    }
}

TEST(CodecWebpEncode_Tests, LossyDecodesInLibwebpAsInTheModule) {
    int k = 0;
    for (auto im : {photo(160, 120), photo(33, 17, 5), photo(1, 1), photo(16, 16), photo(17, 31)}) {
        for (int q : {1, 30, 75, 100}) {
            auto file = codec::webp::encode(im, {.quality = q});
            ASSERT_TRUE(file.has_value());
            auto back = codec::webp::decode(*file);
            ASSERT_TRUE(back.has_value()) << back.error().message().view();
            EXPECT_EQ(back->format(), pixel_format::rgb8);
            if (!c_oracle().empty()) {
                auto theirs = run_oracle(c_oracle(), "webp", write_scratch("lossy_" + std::to_string(k) + ".webp", *file));
                ASSERT_TRUE(theirs.has_value());
                EXPECT_EQ(theirs->pixels, rgba_of(*back)) << "image " << k << " q " << q;
            }
            ++k;
        }
    }
}

TEST(CodecWebpEncode_Tests, LossyQualityRaisesPsnrAndSize) {
    codec::image im = photo(256, 192);
    double last_psnr = 0;
    size_t last_size = 0;
    for (int q : {1, 20, 40, 60, 75, 90, 100}) {
        auto file = codec::webp::encode(im, {.quality = q});
        ASSERT_TRUE(file.has_value());
        const double p = psnr(im, *codec::webp::decode(*file));
        EXPECT_GT(p, last_psnr) << q;
        EXPECT_GT(file->size(), last_size) << q;
        last_psnr = p;
        last_size = file->size();
    }
    EXPECT_GT(last_psnr, 40.0);
    // the default quality, 85 (as save's and jpeg's): the file of quality 85
    EXPECT_EQ(*codec::webp::encode(im), *codec::webp::encode(im, {.quality = 85}));
    EXPECT_EQ(codec::webp::options{}.quality, codec::save_options{}.quality);
    EXPECT_GT(psnr(im, *codec::webp::decode(*codec::webp::encode(im))), 35.0);
}

TEST(CodecWebpEncode_Tests, LossyAlphaIsExact) {
    codec::image im = with_alpha(90, 70);
    auto file = codec::webp::encode(im, {.quality = 60});
    ASSERT_TRUE(file.has_value());
    codec::image back = codec::webp::decode(*file);
    ASSERT_EQ(back.format(), pixel_format::rgba8);
    auto a = im.pixels(), b = back.pixels();
    for (size_t i = 3; i < a.size(); i += 4) {
        ASSERT_EQ(a[i], b[i]) << "pixel " << i / 4;
    }
    if (!c_oracle().empty()) {
        auto theirs = run_oracle(c_oracle(), "webp", write_scratch("lossy_alpha.webp", *file));
        ASSERT_TRUE(theirs.has_value());
        EXPECT_EQ(theirs->pixels, rgba_of(back));
    }
    // every alpha filter's plane: one opaque column, steps, noise
    for (int pattern = 0; pattern < 3; ++pattern) {
        codec::image p = photo(41, 29).convert(pixel_format::rgba8);
        std::mt19937 rng(9);
        for (uint32_t y = 0; y < 29; ++y) {
            for (uint32_t x = 0; x < 41; ++x) {
                const int v = pattern == 0 ? (x == 5 ? 255 : 0) : pattern == 1 ? int((x / 7 + y / 5) * 40 % 256) : int(rng() % 256);
                p.row(y)[4 * x + 3] = std::byte(v);
            }
        }
        codec::image q = codec::webp::decode(*codec::webp::encode(p, {.quality = 80}));
        for (size_t i = 3; i < p.pixels().size(); i += 4) {
            ASSERT_EQ(p.pixels()[i], q.pixels()[i]) << "pattern " << pattern;
        }
    }
}

TEST(CodecWebpEncode_Tests, MetadataWritten) {
    for (bool lossless : {false, true}) {
        codec::image im = photo(30, 20);
        const std::string icc = "an ICC profile's bytes";
        im.set_icc(bytes(icc));
        // an EXIF block of orientation 6 (little-endian TIFF, one entry)
        const unsigned char exif[] = {'I', 'I', 42, 0, 8, 0, 0, 0, 1, 0, 0x12, 0x01, 3, 0, 1, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0};
        im.set_exif(slice<const std::byte>(reinterpret_cast<const std::byte*>(exif), sizeof(exif)));
        auto file = codec::webp::encode(im, {.lossless = lossless});
        ASSERT_TRUE(file.has_value());
        codec::image back = codec::webp::decode(*file);
        EXPECT_EQ(std::string(reinterpret_cast<const char*>(back.icc().data()), back.icc().size()), icc);
        EXPECT_EQ(back.exif().size(), sizeof(exif));
        EXPECT_EQ(back.orientation(), 6);
        if (!c_oracle().empty()) {
            EXPECT_TRUE(run_oracle(c_oracle(), "webp", write_scratch("meta.webp", *file)).has_value());
        }
    }
}

TEST(CodecWebpEncode_Tests, AnimationFrameByFrame) {
    // a square moving, a band turning transparent, a frame repeated, a
    // change at an odd column
    vector<codec::image> pics;
    for (int i = 0; i < 6; ++i) {
        codec::image im(48, 32, pixel_format::rgba8);
        for (uint32_t y = 0; y < 32; ++y) {
            auto row = im.row(y);
            for (uint32_t x = 0; x < 48; ++x) {
                const bool square = x >= uint32_t(5 * i + 1) && x < uint32_t(5 * i + 9) && y >= 9 && y < 19;
                const bool hole = i == 3 && x < 10;
                row[4 * x] = std::byte(square ? 250 : 10 + x);
                row[4 * x + 1] = std::byte(square ? 0 : 100 + y);
                row[4 * x + 2] = std::byte(x * 5);
                row[4 * x + 3] = std::byte(hole ? 0 : square && i == 5 ? 128 : 255);
            }
        }
        pics.push_back(im);
    }
    pics[4] = pics[3];
    vector<codec::frame> frames;
    for (size_t i = 0; i < pics.size(); ++i) {
        frames.push_back({pics[i], std::chrono::milliseconds(40 + 10 * int(i))});
    }
    frames.push_back({photo(48, 32), std::chrono::milliseconds(1000)});
    for (bool lossless : {true, false}) {
        for (uint32_t loops : {0u, 3u}) {
            auto file = codec::webp::encode(frames, {.lossless = lossless, .quality = 90, .loop_count = loops});
            ASSERT_TRUE(file.has_value());
            Frames r = read_frames(*file);
            ASSERT_EQ(r.count, frames.size());
            EXPECT_EQ(r.plays, loops);
            EXPECT_EQ(r.delays_ms, (std::vector<int64_t>{40, 50, 60, 70, 80, 90, 1000}));
            std::string want;
            for (const auto& f : frames) {
                want += rgba_of(f.picture);
            }
            if (lossless) {
                EXPECT_EQ(r.pixels, want);
            } else {
                // each frame close, its alpha exact
                for (size_t i = 0; i < frames.size(); ++i) {
                    const size_t n = 48 * 32 * 4;
                    for (size_t k = 3; k < n; k += 4) {
                        ASSERT_EQ(r.pixels[i * n + k], want[i * n + k]) << "frame " << i;
                    }
                }
            }
            if (!c_oracle().empty()) {
                // libwebp's canvases the module's, a pixel transparent on
                // both sides by its alpha alone (libwebp gives such a pixel
                // of a lossy frame no color)
                auto theirs = run_oracle(c_oracle(), "webpanim", write_scratch("anim.webp", *file));
                ASSERT_TRUE(theirs.has_value());
                ASSERT_EQ(theirs->pixels.size(), r.pixels.size());
                size_t differ = 0;
                for (size_t p = 0; p < r.pixels.size(); p += 4) {
                    const bool clear = r.pixels[p + 3] == 0 && theirs->pixels[p + 3] == 0;
                    differ += !clear && std::memcmp(r.pixels.data() + p, theirs->pixels.data() + p, 4) != 0;
                }
                EXPECT_EQ(differ, 0u) << "lossless " << lossless;
            }
        }
    }
}

TEST(CodecWebpEncode_Tests, Refusals) {
    codec::image im(3, 3, pixel_format::rgb8);
    for (int q : {-1, 0, 101}) {
        auto e = codec::webp::encode(im, {.quality = q});
        ASSERT_FALSE(e.has_value());
        EXPECT_EQ(e.error().code(), codec::errc::invalid_argument);
    }
    for (auto [w, h] : {std::pair{16385u, 1u}, std::pair{1u, 16385u}}) {
        codec::image wide(w, h, pixel_format::gray8);
        for (bool lossless : {false, true}) {
            auto e = codec::webp::encode(wide, {.lossless = lossless});
            ASSERT_FALSE(e.has_value());
            EXPECT_EQ(e.error().code(), codec::errc::invalid_argument);
            vector<byte> out;
            collect c{&out};
            auto s = codec::webp::encode(wide, io::writer(c), {.lossless = lossless});
            ASSERT_FALSE(s.has_value());
            EXPECT_TRUE(out.empty());
        }
    }
    codec::image edge(16384, 1, pixel_format::gray8);
    EXPECT_TRUE(codec::webp::encode(edge, {.lossless = true}).has_value());
    EXPECT_TRUE(codec::webp::encode(edge).has_value());
    vector<codec::frame> none;
    EXPECT_EQ(codec::webp::encode(none).error().code(), codec::errc::invalid_argument);
    vector<codec::frame> mixed;
    mixed.push_back({im, duration()});
    mixed.push_back({codec::image(4, 3, pixel_format::rgb8), duration()});
    EXPECT_EQ(codec::webp::encode(mixed).error().code(), codec::errc::invalid_argument);
}

TEST(CodecWebpEncode_Tests, StreamIsTheVectorAndFails) {
    codec::image im = photo(70, 50);
    for (bool lossless : {false, true}) {
        auto file = codec::webp::encode(im, {.lossless = lossless});
        vector<byte> streamed;
        collect c{&streamed};
        ASSERT_TRUE(codec::webp::encode(im, io::writer(c), {.lossless = lossless}).has_value());
        EXPECT_EQ(streamed, *file);
        failing f{file->size() - 1};
        auto r = codec::webp::encode(im, io::writer(f), {.lossless = lossless});
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().code(), codec::errc::io);
        ASSERT_TRUE(r.error().io_error().has_value());
        failing enough{file->size()};
        EXPECT_TRUE(codec::webp::encode(im, io::writer(enough), {.lossless = lossless}).has_value());
    }
    vector<codec::frame> frames;
    frames.push_back({im, std::chrono::milliseconds(10)});
    frames.push_back({photo(70, 50, 2), std::chrono::milliseconds(10)});
    auto anim = codec::webp::encode(frames);
    vector<byte> streamed;
    collect c{&streamed};
    ASSERT_TRUE(codec::webp::encode(frames, io::writer(c)).has_value());
    EXPECT_EQ(streamed, *anim);
}

TEST(CodecWebpEncode_Tests, Boundaries) {
    // a moved-from image is still the image
    codec::image a = photo(9, 7);
    codec::image b = std::move(a);
    EXPECT_EQ(*codec::webp::encode(a), *codec::webp::encode(b));   // NOLINT(bugprone-use-after-move)
    // an image all transparent, lossy and lossless
    codec::image clear(13, 11, pixel_format::rgba8);
    EXPECT_EQ(rgba_of(*codec::webp::decode(*codec::webp::encode(clear, {.lossless = true}))), rgba_of(clear));
    codec::image back = codec::webp::decode(*codec::webp::encode(clear));
    for (size_t i = 3; i < back.pixels().size(); i += 4) {
        ASSERT_EQ(back.pixels()[i], std::byte(0));
    }
    // the same frame three times
    vector<codec::frame> same;
    for (int i = 0; i < 3; ++i) {
        same.push_back({b, std::chrono::milliseconds(20)});
    }
    Frames r = read_frames(*codec::webp::encode(same, {.lossless = true}));
    EXPECT_EQ(r.pixels, rgba_of(b) + rgba_of(b) + rgba_of(b));
    // the largest delay ANMF holds: 2^24 - 1 ms
    vector<codec::frame> slow;
    slow.push_back({b, std::chrono::hours(10)});
    slow.push_back({b, std::chrono::milliseconds(-5)});
    r = read_frames(*codec::webp::encode(slow, {.lossless = true}));
    EXPECT_EQ(r.delays_ms, (std::vector<int64_t>{16777215, 0}));
}

TEST(CodecWebpEncode_Tests, SaveWritesWebp) {
    auto dir = scratch_path("sgcl_codec_webp_encode_save");
    codec::image im = photo(40, 30);
    const std::string lossy = (dir / "photo.webp").string(), exact = (dir / "photo-exact.WEBP").string();
    ASSERT_TRUE(im.save(string(lossy.c_str()), {.quality = 90}).has_value());
    ASSERT_TRUE(codec::save(im, string(exact.c_str()), {.lossless = true}).has_value());
    codec::image a = codec::load(string(lossy.c_str()));
    codec::image b = codec::load(string(exact.c_str()));
    EXPECT_GT(psnr(im, a), 35.0);
    EXPECT_EQ(rgba_of(b), rgba_of(im));
    std::ifstream in(lossy, std::ios::binary);
    std::string bytes_written((std::istreambuf_iterator<char>(in)), {});
    auto file = codec::webp::encode(im, {.quality = 90});
    EXPECT_EQ(bytes_written, std::string(reinterpret_cast<const char*>(file->data()), file->size()));
    EXPECT_FALSE(std::filesystem::exists(lossy + ".part"));
}

#if defined(SGCL_CODEC_NEON)
TEST(CodecWebpEncode_Tests, Vp8KernelsAgainstThePlainRoad) {
    std::mt19937 rng(11);
    for (int t = 0; t < 20000; ++t) {
        uint8_t src[64], pred[64];
        for (int i = 0; i < 64; ++i) {
            src[i] = uint8_t(rng());
            pred[i] = t % 3 == 0 ? uint8_t(rng()) : uint8_t(src[i] + int(rng() % 9) - 4);
        }
        int16_t a[16], b[16];
        codec::detail::vp8::residue_dct(src, 8, pred, 8, a);
        codec::detail::vp8::residue_dct_plain(src, 8, pred, 8, b);
        ASSERT_EQ(std::memcmp(a, b, sizeof a), 0) << t;
        const int16_t qs[] = {4, 8, 37, 157, 314};
        const codec::detail::vp8::Quantizer4 k(qs[rng() % 5], qs[rng() % 5]);
        const int first = int(rng() % 2);
        int16_t la[16], lb[16], da[16], db[16];
        const int last_a = codec::detail::vp8::quantize(a, first, k, la, da);
        const int last_b = codec::detail::vp8::quantize_plain(a, first, k, lb, db);
        ASSERT_EQ(last_a, last_b) << t;
        ASSERT_EQ(std::memcmp(la, lb, sizeof la), 0) << t;
        ASSERT_EQ(std::memcmp(da, db, sizeof da), 0) << t;
        for (int w : {4, 8}) {
            ASSERT_EQ(codec::detail::vp8::sse(src, 8, pred, 8, w, 8), codec::detail::vp8::sse_plain(src, 8, pred, 8, w, 8));
        }
    }
    uint8_t s16[16 * 16], p16[16 * 16];
    for (int i = 0; i < 256; ++i) {
        s16[i] = uint8_t(rng());
        p16[i] = uint8_t(rng());
    }
    EXPECT_EQ(codec::detail::vp8::sse(s16, 16, p16, 16, 16, 16), codec::detail::vp8::sse_plain(s16, 16, p16, 16, 16, 16));
    // the extremes: a full-scale residue
    uint8_t white[16], black[16];
    std::memset(white, 255, 16);
    std::memset(black, 0, 16);
    int16_t a[16], b[16];
    codec::detail::vp8::residue_dct(white, 4, black, 4, a);
    codec::detail::vp8::residue_dct_plain(white, 4, black, 4, b);
    EXPECT_EQ(std::memcmp(a, b, sizeof a), 0);
}
#endif
