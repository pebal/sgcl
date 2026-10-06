//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: the GIF encoder (gif.h, detail/gif_encoder.h, detail/quantize.h).
// An image of no more colors than the palette comes back pixel for pixel
// (alpha below 128 transparent) through the module's decoder, Go's
// image/gif and giflib; a photo through median cut and dithering comes back
// close (PSNR) with no more colors than asked; animations of changing
// rectangles, pixels turning transparent, frames that do not change, every
// pixel format, delays and loop counts, every frame as the oracles compose
// it. The refusals (sides, colors, frames), a failing stream, the stream's
// bytes the vector's, save to .gif.
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
        auto path = (scratch_path("sgcl_codec_gif_encode_tests") / name).string();
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
        return path;
    }

    // An image as GIF shows it: alpha below 128 transparent (all zero),
    // anything else opaque
    std::string shown(const codec::image& im) {
        codec::image rgba = im.convert(pixel_format::rgba8);
        auto px = rgba.pixels();
        std::string out(reinterpret_cast<const char*>(px.data()), px.size());
        for (size_t i = 0; i < out.size(); i += 4) {
            if (uint8_t(out[i + 3]) < 128) {
                out[i] = out[i + 1] = out[i + 2] = out[i + 3] = 0;
            } else {
                out[i + 3] = char(255);
            }
        }
        return out;
    }

    std::string pixels_of(const codec::image& im) {
        auto px = im.pixels();
        return std::string(reinterpret_cast<const char*>(px.data()), px.size());
    }

    // An image of `colors` colors (and transparent pixels when asked) in a
    // pattern of runs and noise
    codec::image few_colors(uint32_t w, uint32_t h, unsigned colors, bool transparent, uint32_t seed = 1) {
        std::mt19937 rng(seed);
        std::vector<uint32_t> palette(colors);
        for (auto& c : palette) {
            c = rng();
        }
        codec::image im(w, h, pixel_format::rgba8);
        for (uint32_t y = 0; y < h; ++y) {
            auto row = im.row(y);
            for (uint32_t x = 0; x < w; ++x) {
                const uint32_t c = palette[((x / 3) * 7 + y * 13 + (rng() % 4 == 0 ? rng() : 0)) % colors];
                row[4 * x] = std::byte(c);
                row[4 * x + 1] = std::byte(c >> 8);
                row[4 * x + 2] = std::byte(c >> 16);
                row[4 * x + 3] = std::byte(transparent && (x + y) % 5 == 0 ? rng() % 128 : 128 + rng() % 128);
            }
        }
        return im;
    }

    // A smooth photo-like picture of many colors
    codec::image photo(uint32_t w, uint32_t h) {
        codec::image im(w, h, pixel_format::rgb8);
        for (uint32_t y = 0; y < h; ++y) {
            auto row = im.row(y);
            for (uint32_t x = 0; x < w; ++x) {
                const double fx = double(x) / w, fy = double(y) / h;
                row[3 * x] = std::byte(uint8_t(255 * fx));
                row[3 * x + 1] = std::byte(uint8_t(255 * fy));
                row[3 * x + 2] = std::byte(uint8_t(127 + 127 * std::sin(10 * fx * fy)));
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

    size_t distinct_colors(const codec::image& im) {
        codec::image rgba = im.convert(pixel_format::rgba8);
        auto px = rgba.pixels();
        std::set<uint32_t> s;
        for (size_t i = 0; i < px.size(); i += 4) {
            uint32_t v;
            std::memcpy(&v, px.data() + i, 4);
            s.insert(v);
        }
        return s.size();
    }

    // Every frame of a GIF through the module's decoder, RGBA one after
    // another, and the delays in milliseconds
    struct Frames {
        uint32_t plays = 0;
        std::string pixels;
        std::vector<int64_t> delays_ms;
        size_t count = 0;
    };

    Frames read_frames(const vector<byte>& file) {
        Frames r;
        auto f = codec::gif::frames(file);
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
            r.pixels += pixels_of((*n)->picture);
            r.delays_ms.push_back((*n)->delay.milliseconds());
            ++r.count;
        }
        r.plays = f->loop_count();
        return r;
    }

    // A stream that fails after `room` bytes
    struct failing {
        size_t room;
        size_t taken = 0;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            if (taken + b.size() > room) {
                return unexpected(io::error(std::make_error_code(std::errc::no_space_on_device), "write", "test"));
            }
            taken += b.size();
            return b.size();
        }
    };

    struct collect {
        vector<byte>* out;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            out->insert(out->end(), b.begin(), b.end());
            return b.size();
        }
    };
}

TEST(CodecGifEncode_Tests, FewColorsComeBackExactly) {
    for (unsigned colors : {1u, 2u, 3u, 16u, 255u, 256u}) {
        for (bool transparent : {false, true}) {
            if (transparent && colors == 256) {
                continue;   // 257 entries: quantized, below
            }
            codec::image im = few_colors(53, 41, colors, transparent, colors);
            auto file = codec::gif::encode(im);
            ASSERT_TRUE(file.has_value());
            auto back = codec::gif::decode(*file);
            ASSERT_TRUE(back.has_value()) << back.error().message().view();
            EXPECT_EQ(back->format(), pixel_format::rgba8);
            EXPECT_EQ(pixels_of(*back), shown(im)) << colors << " colors, transparent " << transparent;
        }
    }
}

TEST(CodecGifEncode_Tests, EveryPixelFormatAsShown) {
    for (unsigned f = 0; f < 9; ++f) {
        codec::image im(19, 7, pixel_format(f));
        auto px = im.pixels();
        for (size_t i = 0; i < px.size(); ++i) {
            px[i] = std::byte((i * 37 / 5) % 7 * 40);   // few values: few colors
        }
        ASSERT_LE(distinct_colors(im), 256u);
        auto file = codec::gif::encode(im);
        ASSERT_TRUE(file.has_value());
        auto back = codec::gif::decode(*file);
        ASSERT_TRUE(back.has_value());
        EXPECT_EQ(pixels_of(*back), shown(im)) << "format " << f;
    }
}

TEST(CodecGifEncode_Tests, OraclesReadTheSamePixels) {
    const std::string& go = go_oracle();
    const std::string& c = c_oracle();
    if (go.empty() && c.empty()) {
        GTEST_SKIP() << "no oracle (Go, giflib)";
    }
    int n = 0;
    for (auto im : {few_colors(64, 48, 200, true, 7), few_colors(1, 1, 1, false), photo(97, 61)}) {
        for (bool dither : {false, true}) {
            auto file = codec::gif::encode(im, {.dither = dither});
            ASSERT_TRUE(file.has_value());
            codec::image back = codec::gif::decode(*file);
            const std::string path = write_scratch("oracle_" + std::to_string(n++) + ".gif", *file);
            for (const std::string* exe : {&go, &c}) {
                if (exe->empty()) {
                    continue;
                }
                auto o = run_oracle(*exe, "gif", path);
                ASSERT_TRUE(o.has_value()) << *exe << " refused " << path;
                EXPECT_EQ(o->width, im.width());
                EXPECT_EQ(o->height, im.height());
                EXPECT_EQ(o->pixels, oracle_form(back, 8)) << *exe << " " << path;
            }
        }
    }
}

TEST(CodecGifEncode_Tests, PhotoQuantizedClose) {
    codec::image im = photo(320, 200);
    ASSERT_GT(distinct_colors(im), 256u);
    for (int colors : {256, 64, 16, 2}) {
        for (bool dither : {false, true}) {
            auto file = codec::gif::encode(im, {.colors = colors, .dither = dither});
            ASSERT_TRUE(file.has_value());
            codec::image back = codec::gif::decode(*file);
            EXPECT_LE(distinct_colors(back), size_t(colors));
            const double p = psnr(im, back);
            const double floor = colors == 256 ? (dither ? 28 : 31) : colors == 64 ? (dither ? 22 : 25) : colors == 16 ? (dither ? 17 : 19) : 11;
            EXPECT_GT(p, floor) << colors << " colors, dither " << dither;
        }
    }
    // dithering changes the pixels; without it a pixel is its nearest entry
    EXPECT_NE(*codec::gif::encode(im, {.dither = false}), *codec::gif::encode(im, {.dither = true}));
}

TEST(CodecGifEncode_Tests, PhotoAtLeastAsCloseAsFfmpeg) {
    // libjpeg-turbo's testorig (the seed): without dithering, at least the
    // PSNR of ffmpeg's palettegen and paletteuse (median cut, dither=none):
    // 35.70, 31.39 and 26.16 dB at 256, 64 and 16 colors
    std::ifstream in((repository_root() / "tests/codec/fuzz/seeds/jpeg_decode/testorig.jpg").string(), std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(in)), {});
    ASSERT_FALSE(bytes.empty());
    codec::image im = codec::decode(codec_test::bytes(bytes));
    for (auto [colors, floor] : {std::pair{256, 35.70}, std::pair{64, 31.39}, std::pair{16, 25.9}}) {
        codec::image back = codec::gif::decode(*codec::gif::encode(im, {.colors = colors, .dither = false}));
        EXPECT_GE(psnr(im, back), floor) << colors;
    }
}

TEST(CodecGifEncode_Tests, TransparencyTakesAnEntry) {
    // 256 colors and transparent pixels: 257 entries, quantized to 255 and
    // the transparent one
    codec::image im = few_colors(64, 64, 256, true, 3);
    auto file = codec::gif::encode(im, {.dither = false});
    ASSERT_TRUE(file.has_value());
    codec::image back = codec::gif::decode(*file);
    EXPECT_LE(distinct_colors(back), 256u);
    const std::string want = shown(im), got = pixels_of(back);
    for (size_t i = 0; i < want.size(); i += 4) {
        ASSERT_EQ(want[i + 3] == 0, got[i + 3] == 0) << "pixel " << i / 4;
    }
    // an image all transparent: one entry, the transparent
    codec::image clear(5, 3, pixel_format::rgba8);
    codec::image none = codec::gif::decode(*codec::gif::encode(clear));
    EXPECT_EQ(pixels_of(none), pixels_of(clear));
}

TEST(CodecGifEncode_Tests, AnimationFrameByFrame) {
    // frames that move a square, turn pixels transparent, repeat a frame,
    // come back opaque; in rgb8, rgba8 and gray8
    vector<codec::image> pics;
    for (int i = 0; i < 6; ++i) {
        codec::image im(40, 30, pixel_format::rgba8);
        for (uint32_t y = 0; y < 30; ++y) {
            auto row = im.row(y);
            for (uint32_t x = 0; x < 40; ++x) {
                const bool square = x >= uint32_t(5 * i) && x < uint32_t(5 * i + 8) && y >= 10 && y < 18;
                const bool hole = i == 3 && x < 10;   // frame 3: a transparent band where 2 had color
                row[4 * x] = std::byte(square ? 255 : 20);
                row[4 * x + 1] = std::byte(square ? 0 : 120);
                row[4 * x + 2] = std::byte(x * 6);
                row[4 * x + 3] = std::byte(hole ? 0 : 255);
            }
        }
        pics.push_back(im);
    }
    pics[4] = pics[3];   // a frame the same as the one before
    vector<codec::frame> frames;
    for (size_t i = 0; i < pics.size(); ++i) {
        frames.push_back({pics[i], std::chrono::milliseconds(10 * (int(i) + 1))});
    }
    frames.push_back({pics[0].convert(pixel_format::rgb8), std::chrono::milliseconds(250)});
    for (uint32_t loops : {0u, 1u, 3u}) {
        auto file = codec::gif::encode(frames, {.loop_count = loops});
        ASSERT_TRUE(file.has_value());
        Frames r = read_frames(*file);
        ASSERT_EQ(r.count, frames.size());
        std::string want;
        for (const auto& f : frames) {
            want += shown(f.picture);
        }
        EXPECT_EQ(r.pixels, want);
        EXPECT_EQ(r.plays, loops);
        EXPECT_EQ(r.delays_ms, (std::vector<int64_t>{10, 20, 30, 40, 50, 60, 250}));
        // the oracles compose the same frames
        const std::string path = write_scratch("anim_" + std::to_string(loops) + ".gif", *file);
        for (const std::string* exe : {&go_oracle(), &c_oracle()}) {
            if (exe->empty()) {
                continue;
            }
            auto o = run_oracle(*exe, "gif", path);
            ASSERT_TRUE(o.has_value()) << *exe;
            std::string ours;
            for (size_t i = 0; i < r.pixels.size(); ++i) {
                ours += r.pixels[i];
            }
            EXPECT_EQ(o->pixels, ours) << *exe;
        }
    }
    // the frames after the first are smaller than the first
    auto one = codec::gif::encode(slice<const codec::frame>(frames.data(), 1));
    auto all = codec::gif::encode(frames);
    EXPECT_LT(all->size(), one->size() * 4);
}

TEST(CodecGifEncode_Tests, AFrameOfAFullPaletteStaysExact) {
    // the second frame has 16 colors and keeps some pixels of the first:
    // its unchanged pixels as transparent would make 17 entries, so it is
    // written with its own pixels (the fuzzer's find)
    // a: 10 colors; b: a with its lower half changed, all but every
    // seventh pixel there, to the 16 colors of a's 10 and 6 new ones (so
    // that the changed pixels alone take the whole palette)
    codec::image a = few_colors(30, 20, 10, false, 1);
    for (size_t i = 3; i < a.pixels().size(); i += 4) {
        a.pixels()[i] = std::byte(255);
    }
    std::vector<uint32_t> sixteen;
    {
        std::set<uint32_t> own;
        auto px = a.pixels();
        for (size_t i = 0; i < px.size(); i += 4) {
            uint32_t v;
            std::memcpy(&v, px.data() + i, 4);
            own.insert(v);
        }
        sixteen.assign(own.begin(), own.end());
        ASSERT_EQ(sixteen.size(), 10u);
        for (uint32_t k = 0; k < 6; ++k) {
            sixteen.push_back(0xFF000000u | (200 + k) << 8 | (200 + k));
        }
    }
    codec::image b = a.clone();
    for (uint32_t y = 10; y < 20; ++y) {
        for (uint32_t x = 0; x < 30; ++x) {
            if ((x + 3 * y) % 7 == 0) {
                continue;   // unchanged pixels inside the changed rectangle
            }
            uint32_t old_value, v = sixteen[(x + y) % 16];
            std::memcpy(&old_value, a.row(y).data() + 4 * x, 4);
            if (v == old_value) {
                v = sixteen[(x + y + 1) % 16];
            }
            std::memcpy(b.row(y).data() + 4 * x, &v, 4);
        }
    }
    std::set<std::string> shown_colors;
    const std::string sb = shown(b);
    for (size_t i = 0; i < sb.size(); i += 4) {
        shown_colors.insert(sb.substr(i, 4));
    }
    ASSERT_EQ(shown_colors.size(), 16u);
    vector<codec::frame> frames;
    frames.push_back({a, std::chrono::milliseconds(10)});
    frames.push_back({b, std::chrono::milliseconds(10)});
    Frames r = read_frames(*codec::gif::encode(frames, {.colors = 16}));
    EXPECT_EQ(r.pixels, shown(a) + shown(b));
}

TEST(CodecGifEncode_Tests, AnimatedPhotosQuantizedPerFrame) {
    vector<codec::frame> frames;
    for (int i = 0; i < 4; ++i) {
        codec::image im = photo(80 + 0, 60);
        auto px = im.pixels();
        for (size_t k = 0; k < px.size(); k += 3) {
            px[k] = std::byte(uint8_t(px[k]) ^ uint8_t(i * 50));
        }
        frames.push_back({im, std::chrono::milliseconds(100)});
    }
    auto file = codec::gif::encode(frames);
    ASSERT_TRUE(file.has_value());
    auto f = codec::gif::frames(*file);
    ASSERT_TRUE(f.has_value());
    for (int i = 0; i < 4; ++i) {
        auto n = f->next();
        ASSERT_TRUE(n.has_value() && n->has_value());
        EXPECT_GT(psnr(frames[size_t(i)].picture, (*n)->picture), 28.0) << "frame " << i;
    }
}

TEST(CodecGifEncode_Tests, DelaysRoundToHundredths) {
    codec::image a = few_colors(4, 4, 3, false, 1), b = few_colors(4, 4, 3, false, 2);
    using std::chrono::milliseconds;
    vector<codec::frame> frames;
    frames.push_back({a, milliseconds(4)});      // 0
    frames.push_back({b, milliseconds(5)});      // 1 (half rounds up)
    frames.push_back({a, milliseconds(14)});     // 1
    frames.push_back({b, milliseconds(-30)});    // 0
    frames.push_back({a, std::chrono::hours(1)});   // 65535
    Frames r = read_frames(*codec::gif::encode(frames));
    EXPECT_EQ(r.delays_ms, (std::vector<int64_t>{0, 10, 10, 0, 655350}));
}

TEST(CodecGifEncode_Tests, LoopCountsAsFramesReadThem) {
    codec::image a = few_colors(4, 4, 3, false, 1), b = few_colors(4, 4, 3, false, 2);
    vector<codec::frame> frames;
    frames.push_back({a, duration()});
    frames.push_back({b, duration()});
    for (uint32_t loops : {0u, 1u, 2u, 65536u}) {
        EXPECT_EQ(read_frames(*codec::gif::encode(frames, {.loop_count = loops})).plays, loops);
    }
    // past what NETSCAPE2.0 holds: 65536 plays at most
    EXPECT_EQ(read_frames(*codec::gif::encode(frames, {.loop_count = 1000000})).plays, 65536u);
    // one frame is an animation too
    vector<codec::frame> single;
    single.push_back({a, std::chrono::milliseconds(70)});
    Frames r = read_frames(*codec::gif::encode(single));
    EXPECT_EQ(r.count, 1u);
    EXPECT_EQ(r.delays_ms, std::vector<int64_t>{70});
}

TEST(CodecGifEncode_Tests, Refusals) {
    codec::image im(3, 3, pixel_format::rgb8);
    for (int colors : {-1, 0, 1, 257, 1000}) {
        auto e = codec::gif::encode(im, {.colors = colors});
        ASSERT_FALSE(e.has_value());
        EXPECT_EQ(e.error().code(), codec::errc::invalid_argument);
    }
    for (auto [w, h] : {std::pair{65536u, 1u}, std::pair{1u, 65536u}}) {
        codec::image wide(w, h, pixel_format::gray8);
        auto e = codec::gif::encode(wide);
        ASSERT_FALSE(e.has_value());
        EXPECT_EQ(e.error().code(), codec::errc::invalid_argument);
        vector<byte> out;
        collect c{&out};
        auto s = codec::gif::encode(wide, io::writer(c));
        ASSERT_FALSE(s.has_value());
        EXPECT_EQ(s.error().code(), codec::errc::invalid_argument);
        EXPECT_TRUE(out.empty());
    }
    codec::image edge(65535, 1, pixel_format::gray8);
    EXPECT_TRUE(codec::gif::encode(edge).has_value());

    vector<codec::frame> none;
    auto e = codec::gif::encode(none);
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code(), codec::errc::invalid_argument);

    vector<codec::frame> mixed;
    mixed.push_back({im, duration()});
    mixed.push_back({codec::image(3, 4, pixel_format::rgb8), duration()});
    e = codec::gif::encode(mixed);
    ASSERT_FALSE(e.has_value());
    EXPECT_EQ(e.error().code(), codec::errc::invalid_argument);
    vector<byte> out;
    collect c{&out};
    auto s = codec::gif::encode(mixed, io::writer(c));
    ASSERT_FALSE(s.has_value());
    EXPECT_TRUE(out.empty());
}

TEST(CodecGifEncode_Tests, StreamIsTheVector) {
    codec::image im = photo(150, 90);
    auto file = codec::gif::encode(im);
    vector<byte> streamed;
    collect c{&streamed};
    ASSERT_TRUE(codec::gif::encode(im, io::writer(c)).has_value());
    EXPECT_EQ(streamed, *file);

    vector<codec::frame> frames;
    frames.push_back({im, std::chrono::milliseconds(30)});
    frames.push_back({photo(150, 90).convert(pixel_format::gray8), std::chrono::milliseconds(30)});
    auto anim = codec::gif::encode(frames);
    vector<byte> streamed2;
    collect c2{&streamed2};
    ASSERT_TRUE(codec::gif::encode(frames, io::writer(c2)).has_value());
    EXPECT_EQ(streamed2, *anim);
}

TEST(CodecGifEncode_Tests, FailingStream) {
    codec::image im = photo(300, 200);
    const size_t total = codec::gif::encode(im)->size();
    for (size_t room : {size_t(0), size_t(5), size_t(13), size_t(800), total / 2, total - 1}) {
        failing f{room};
        auto r = codec::gif::encode(im, io::writer(f));
        ASSERT_FALSE(r.has_value()) << room;
        EXPECT_EQ(r.error().code(), codec::errc::io);
        EXPECT_LE(r.error().offset(), room);
        ASSERT_TRUE(r.error().io_error().has_value());
    }
    failing enough{total};
    EXPECT_TRUE(codec::gif::encode(im, io::writer(enough)).has_value());
}

TEST(CodecGifEncode_Tests, Boundaries) {
    // one pixel, opaque and transparent
    codec::image one(1, 1, pixel_format::rgba8);
    one.pixels()[3] = std::byte(255);
    one.pixels()[0] = std::byte(9);
    EXPECT_EQ(pixels_of(codec::gif::decode(*codec::gif::encode(one))), shown(one));
    one.pixels()[3] = std::byte(127);
    EXPECT_EQ(pixels_of(codec::gif::decode(*codec::gif::encode(one))), std::string(4, '\0'));
    // a moved-from image is still the image (a handle's move copies the word)
    codec::image a = few_colors(7, 5, 4, false);
    codec::image b = std::move(a);
    EXPECT_EQ(*codec::gif::encode(a), *codec::gif::encode(b));   // NOLINT(bugprone-use-after-move)
    // the same image as every frame: the frames after the first are one
    // transparent pixel
    vector<codec::frame> same;
    for (int i = 0; i < 3; ++i) {
        same.push_back({b, std::chrono::milliseconds(20)});
    }
    Frames r = read_frames(*codec::gif::encode(same));
    EXPECT_EQ(r.pixels, shown(b) + shown(b) + shown(b));
    // 1 x 65535 and 65535 x 1 of two colors
    codec::image tall(1, 65535, pixel_format::gray8);
    tall.pixels()[100] = std::byte(255);
    EXPECT_EQ(pixels_of(codec::gif::decode(*codec::gif::encode(tall))), shown(tall));
}

TEST(CodecGifEncode_Tests, SaveWritesGif) {
    auto dir = scratch_path("sgcl_codec_gif_encode_save");
    const std::string path = (dir / "picture.GIF").string();
    codec::image im = few_colors(20, 10, 12, true);
    auto r = im.save(string(path.c_str()));
    ASSERT_TRUE(r.has_value()) << r.error().message().view();
    auto back = codec::load(string(path.c_str()));
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(pixels_of(*back), shown(im));
    EXPECT_FALSE(std::filesystem::exists(path + ".part"));
    // what save writes is encode's
    std::ifstream in(path, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(in)), {});
    auto file = codec::gif::encode(im);
    EXPECT_EQ(bytes, std::string(reinterpret_cast<const char*>(file->data()), file->size()));
    // a refused image leaves no file
    codec::image wide(70000, 1, pixel_format::gray8);
    const std::string refused = (dir / "wide.gif").string();
    auto w = codec::save(wide, string(refused.c_str()));
    ASSERT_FALSE(w.has_value());
    EXPECT_EQ(w.error().code(), codec::errc::invalid_argument);
    EXPECT_FALSE(std::filesystem::exists(refused));
    EXPECT_FALSE(std::filesystem::exists(refused + ".part"));
}
