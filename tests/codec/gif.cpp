//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: the GIF decoder and frames (gif.h, frames.h, detail/gif_decoder.h).
// Every frame on its canvas, pixel for pixel against giflib and Go's
// image/gif, each composed by its oracle as the module composes (the
// oracles agree with each other on every file): giflib's pictures, Go's
// testdata, and animations made here of every disposal, transparency,
// local tables, interlacing and loop count. The loop count and delays; a
// stream in pieces as memory; negatives of the right code; frames sharing
// their reading, ending and failing for good.
#include "oracle.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;

namespace {
    std::filesystem::path scratch_dir() {
        return scratch_path("sgcl_codec_gif_tests");
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

    std::vector<std::string> corpus() {
        std::vector<std::string> out;
        const auto pic = oracle_path("giflib/giflib-6.1.3/pic");
        if (std::filesystem::is_directory(pic)) {
            for (auto& e : std::filesystem::directory_iterator(pic)) {
                if (e.path().extension() == ".gif") {
                    out.push_back(e.path().string());
                }
            }
        }
        FILE* p = popen("go env GOROOT 2>/dev/null", "r");
        if (p) {
            char buf[512] = {};
            std::string root = fgets(buf, sizeof(buf), p) ? std::string(buf) : "";
            pclose(p);
            while (!root.empty() && (root.back() == '\n' || root.back() == '\r')) {
                root.pop_back();
            }
            const std::string dir = root + "/src/image/testdata";
            if (!root.empty() && std::filesystem::is_directory(dir)) {
                for (auto& e : std::filesystem::directory_iterator(dir)) {
                    if (e.path().extension() == ".gif") {
                        out.push_back(e.path().string());
                    }
                }
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

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

    // Every frame, RGBA one after another, and the delays; "" and an error
    // text when reading fails
    struct Read {
        uint32_t width = 0, height = 0, plays = 0;
        std::string pixels;
        std::vector<int64_t> delays_ms;
        std::string failure;
    };

    Read read_all(expected<codec::frames, codec::error> f) {
        Read r;
        if (!f) {
            r.failure = std::string(f.error().message().view());
            return r;
        }
        r.width = f->width();
        r.height = f->height();
        for (;;) {
            auto n = f->next();
            if (!n) {
                r.failure = std::string(n.error().message().view());
                return r;
            }
            if (!*n) {
                break;
            }
            auto px = (*n)->picture.pixels();
            r.pixels.append(reinterpret_cast<const char*>(px.data()), px.size());
            r.delays_ms.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::nanoseconds((*n)->delay)).count());
        }
        r.plays = f->loop_count();
        return r;
    }

    // The oracle's "plays" and delays
    struct Meta {
        uint32_t plays = 0;
        std::vector<int64_t> delays_ms;
    };

    std::optional<Meta> meta_of(const std::string& exe, const std::string& path) {
        FILE* p = popen(("'" + exe + "' gifmeta '" + path + "' 2>/dev/null").c_str(), "r");
        if (!p) {
            return std::nullopt;
        }
        Meta m;
        char line[128];
        while (fgets(line, sizeof(line), p)) {
            unsigned v;
            if (std::sscanf(line, "plays %u", &v) == 1) {
                m.plays = v;
            } else if (std::sscanf(line, "delay %u", &v) == 1) {
                m.delays_ms.push_back(int64_t(v) * 10);
            }
        }
        if (pclose(p) != 0) {
            return std::nullopt;
        }
        return m;
    }

    // A GIF made here: the logical screen with a global table of 4 colors,
    // an optional loop count, and frames each with a GCE (disposal, delay,
    // transparent index) and its indices, optionally a local table of 8
    // colors and interlaced
    struct GifFrame {
        uint16_t left = 0, top = 0, w = 1, h = 1;
        int disposal = 0;
        int delay = 0;
        int transparent = -1;
        bool local = false;
        bool interlaced = false;
        std::vector<uint8_t> indices;   // w × h, rows top to bottom
    };

    std::string le16(unsigned v) {
        return std::string{char(v & 0xFF), char(v >> 8)};
    }

    std::string lzw_blocks(const std::vector<uint8_t>& indices, int width) {
        std::vector<std::byte> raw(indices.size());
        for (size_t i = 0; i < indices.size(); ++i) {
            raw[i] = std::byte(indices[i]);
        }
        auto z = compress::lzw::compress(slice<const std::byte>(raw.data(), raw.size()), compress::lzw::order::lsb, width);
        std::string out;
        for (size_t i = 0; i < z.size(); i += 255) {
            const size_t n = std::min<size_t>(255, z.size() - i);
            out += char(n);
            out.append(reinterpret_cast<const char*>(z.data()) + i, n);
        }
        out += char(0);
        return out;
    }

    std::string make_gif(uint16_t w, uint16_t h, const std::vector<GifFrame>& frames, int loop = -1) {
        std::string out = "GIF89a" + le16(w) + le16(h) + std::string{char(0x80 | 0x01), 0, 0};
        out += std::string("\xFF\x00\x00\x00\xFF\x00\x00\x00\xFF\xFF\xFF\x00", 12);   // red, green, blue, yellow
        if (loop >= 0) {
            out += std::string("\x21\xFF\x0BNETSCAPE2.0\x03\x01", 16) + le16(unsigned(loop)) + std::string(1, '\0');
        }
        for (const auto& f : frames) {
            out += std::string("\x21\xF9\x04", 3);
            out += char((f.disposal & 7) << 2 | (f.transparent >= 0 ? 1 : 0));
            out += le16(unsigned(f.delay));
            out += char(f.transparent >= 0 ? f.transparent : 0);
            out += char(0);
            out += char(0x2C) + le16(f.left) + le16(f.top) + le16(f.w) + le16(f.h);
            out += char((f.local ? 0x82 : 0) | (f.interlaced ? 0x40 : 0));
            if (f.local) {
                for (int i = 0; i < 8; ++i) {
                    out += std::string{char(i * 30), char(255 - i * 30), char(i * 17)};
                }
            }
            std::vector<uint8_t> stream = f.indices;
            if (f.interlaced) {
                stream.clear();
                const uint32_t start[4] = {0, 4, 2, 1}, step[4] = {8, 8, 4, 2};
                for (int p = 0; p < 4; ++p) {
                    for (uint32_t y = start[p]; y < f.h; y += step[p]) {
                        stream.insert(stream.end(), f.indices.begin() + y * f.w, f.indices.begin() + (y + 1) * f.w);
                    }
                }
            }
            const int width = f.local ? 3 : 2;
            out += char(width);
            out += lzw_blocks(stream, width);
        }
        out += char(0x3B);
        return out;
    }

    std::vector<uint8_t> pattern(uint16_t w, uint16_t h, int colors, int seed) {
        std::vector<uint8_t> v(size_t(w) * h);
        for (size_t i = 0; i < v.size(); ++i) {
            v[i] = uint8_t((i * 7 + seed * 3 + i / w) % colors);
        }
        return v;
    }

    codec::errc code_of(const expected<codec::image, codec::error>& r) {
        return r ? codec::errc{} : r.error().code();
    }

    // Against both oracles: "" when the module's frames are theirs
    std::string against_oracles(const std::string& path, const std::string& data) {
        const Read ours = read_all(codec::gif::frames(bytes(data)));
        if (!ours.failure.empty()) {
            return "ours: " + ours.failure;
        }
        for (const std::string& exe : {c_oracle(), go_oracle()}) {
            if (exe.empty()) {
                continue;
            }
            auto theirs = run_oracle(exe, "gif", path);
            if (!theirs) {
                return exe + " refused it";
            }
            if (theirs->width != ours.width || theirs->height != ours.height) {
                return "size against " + exe;
            }
            if (theirs->pixels != ours.pixels) {
                return "pixels against " + exe + " (" + std::to_string(ours.pixels.size()) + " and " + std::to_string(theirs->pixels.size()) + " bytes)";
            }
            auto m = meta_of(exe, path);
            if (!m || m->plays != ours.plays || m->delays_ms != ours.delays_ms) {
                return "loop count or delays against " + exe;
            }
        }
        return "";
    }
}

TEST(CodecGif_Tests, TheCorporaAgainstGiflibAndGo) {
    if (c_oracle().empty() && go_oracle().empty()) {
        GTEST_SKIP() << "no oracle";
    }
    auto files = corpus();
    if (files.empty()) {
        GTEST_SKIP() << "no giflib pictures and no Go testdata";
    }
    for (const auto& path : files) {
        const std::string data = read_file(path);
        EXPECT_EQ(against_oracles(path, data), "") << path;
        // decode: the first frame; through codec::decode too
        const Read all = read_all(codec::gif::frames(bytes(data)));
        codec::image first = *codec::gif::decode(bytes(data));
        ASSERT_EQ(first.format(), pixel_format::rgba8);
        EXPECT_EQ(std::memcmp(first.pixels().data(), all.pixels.data(), first.pixels().size()), 0) << path;
        codec::image any = *codec::decode(bytes(data));
        EXPECT_EQ(std::memcmp(any.pixels().data(), first.pixels().data(), first.pixels().size()), 0) << path;
    }
    EXPECT_GE(files.size(), 13u);
}

TEST(CodecGif_Tests, AnimationsMadeHere) {
    if (c_oracle().empty() && go_oracle().empty()) {
        GTEST_SKIP() << "no oracle";
    }
    // every disposal after a frame, with and without transparency, frames
    // at offsets, local tables, interlacing, loop counts
    int case_number = 0;
    for (int disposal : {0, 1, 2, 3, 5}) {
        for (bool transparent : {false, true}) {
            for (bool interlaced : {false, true}) {
                for (bool local : {false, true}) {
                    std::vector<GifFrame> frames;
                    GifFrame base;
                    base.w = 20;
                    base.h = 11;
                    base.indices = pattern(20, 11, 4, 0);
                    base.disposal = 1;   // kept: what a later disposal 3 puts back
                    base.delay = 7;
                    frames.push_back(base);
                    GifFrame over;
                    over.left = 3;
                    over.top = 2;
                    over.w = 9;
                    over.h = 7;
                    over.local = local;
                    over.interlaced = interlaced;
                    over.indices = pattern(9, 7, local ? 8 : 4, 1);
                    over.transparent = transparent ? 1 : -1;
                    over.disposal = disposal;
                    over.delay = 13;
                    frames.push_back(over);
                    GifFrame last = over;
                    last.left = 10;
                    last.top = 4;
                    last.indices = pattern(9, 7, local ? 8 : 4, 2);
                    last.disposal = 0;
                    frames.push_back(last);
                    const int loop = case_number % 3 == 0 ? -1 : case_number % 3 == 1 ? 0 : 4;
                    const std::string data = make_gif(20, 11, frames, loop);
                    const auto path = write_file("made.gif", data);
                    EXPECT_EQ(against_oracles(path, data), "")
                        << "disposal " << disposal << " transparent " << transparent << " interlaced " << interlaced << " local " << local;
                    const Read ours = read_all(codec::gif::frames(bytes(data)));
                    EXPECT_EQ(ours.plays, loop < 0 ? 1u : loop == 0 ? 0u : uint32_t(loop + 1));
                    EXPECT_EQ(ours.delays_ms, (std::vector<int64_t>{70, 130, 130}));
                    ++case_number;
                }
            }
        }
    }
}

TEST(CodecGif_Tests, AStreamInPiecesIsMemory) {
    size_t step = 1;
    for (const auto& path : corpus()) {
        const std::string data = read_file(path);
        const Read whole = read_all(codec::gif::frames(bytes(data)));
        pieces p{&data, step};
        const Read streamed = read_all(codec::gif::frames(io::reader(p)));
        step = step % 61 + 3;
        EXPECT_EQ(whole.failure, "") << path;
        EXPECT_EQ(streamed.failure, "") << path;
        EXPECT_TRUE(whole.pixels == streamed.pixels) << path;
        EXPECT_EQ(whole.plays, streamed.plays) << path;
        pieces q{&data, 100};
        auto first = codec::gif::decode(io::reader(q));
        ASSERT_TRUE(first) << path;
        EXPECT_EQ(std::memcmp(first->pixels().data(), whole.pixels.data(), first->pixels().size()), 0) << path;
        pieces r{&data, 777};
        EXPECT_TRUE(codec::decode(io::reader(r))) << path;
    }
}

TEST(CodecGif_Tests, TheFormatAskedFor) {
    GifFrame f;
    f.w = 5;
    f.h = 3;
    f.indices = pattern(5, 3, 4, 0);
    const std::string data = make_gif(5, 3, {f, f});
    codec::image native = *codec::gif::decode(bytes(data));
    for (int k = 0; k < 9; ++k) {
        const auto want = static_cast<pixel_format>(k);
        codec::image asked = *codec::gif::decode(bytes(data), {.want = want});
        codec::image converted = native.convert(want);
        ASSERT_EQ(asked.pixels().size(), converted.pixels().size());
        EXPECT_EQ(std::memcmp(asked.pixels().data(), converted.pixels().data(), converted.pixels().size()), 0) << k;
        auto frames = codec::gif::frames(bytes(data), {.want = want});
        ASSERT_TRUE(frames);
        auto one = frames->next();
        ASSERT_TRUE(one && *one);
        EXPECT_EQ((*one)->picture.format(), want);
    }
    EXPECT_FALSE(codec::gif::frames(bytes(data), {.want = static_cast<pixel_format>(40)}));
}

TEST(CodecGif_Tests, FramesShareEndAndFail) {
    GifFrame f;
    f.w = 4;
    f.h = 4;
    f.indices = pattern(4, 4, 4, 0);
    const std::string data = make_gif(4, 4, {f, f, f}, 0);
    codec::frames clip = *codec::gif::frames(bytes(data));
    EXPECT_EQ(clip.width(), 4u);
    EXPECT_EQ(clip.height(), 4u);
    EXPECT_EQ(clip.loop_count(), 0u);   // known before the first frame
    codec::frames copy = clip;
    ASSERT_TRUE(*clip.next());
    ASSERT_TRUE(*copy.next());          // the second frame: the reading is shared
    ASSERT_TRUE(*clip.next());
    for (int i = 0; i < 3; ++i) {
        auto n = clip.next();
        ASSERT_TRUE(n);
        EXPECT_FALSE(*n);
    }
    // a failure is the same on every call after it
    const std::string cut = data.substr(0, data.size() - 10);
    codec::frames broken = *codec::gif::frames(bytes(cut));
    ASSERT_TRUE(*broken.next());
    ASSERT_TRUE(*broken.next());
    auto e1 = broken.next();
    ASSERT_FALSE(e1);
    EXPECT_EQ(e1.error().code(), codec::errc::unexpected_end);
    auto e2 = broken.next();
    ASSERT_FALSE(e2);
    EXPECT_EQ(e2.error(), e1.error());
}

TEST(CodecGif_Tests, Negatives) {
    GifFrame f;
    f.w = 6;
    f.h = 5;
    f.indices = pattern(6, 5, 4, 0);
    const std::string good = make_gif(6, 5, {f, f});
    ASSERT_TRUE(codec::gif::decode(bytes(good)));
    auto decode = [](const std::string& d) { return code_of(codec::gif::decode(bytes(d))); };
    // every cut: the first frame's image or the frames' reading ends early
    for (size_t n = 0; n + 1 < good.size(); ++n) {
        const std::string cut = good.substr(0, n);
        const Read r = read_all(codec::gif::frames(bytes(cut)));
        ASSERT_NE(r.failure, "") << n;
        auto first = codec::gif::decode(bytes(cut));
        if (!first) {
            EXPECT_EQ(first.error().code(), codec::errc::unexpected_end) << n << ": " << first.error().message();
        }
    }
    // a signature that is not GIF's
    EXPECT_EQ(decode("GIF88a" + good.substr(6)), codec::errc::corrupt);
    // a canvas of zero pixels; one past the limit
    EXPECT_EQ(decode(good.substr(0, 6) + le16(0) + good.substr(8)), codec::errc::corrupt);
    EXPECT_EQ(code_of(codec::gif::decode(bytes(good), {.limits = {.max_pixels = 29}})), codec::errc::too_large);
    // the positions of the first image's parts
    const size_t image = good.find('\x2C');
    ASSERT_NE(image, std::string::npos);
    const size_t code_size = image + 10;
    // an LZW code size of 1 and of 9
    std::string bad = good;
    bad[code_size] = 1;
    EXPECT_EQ(decode(bad), codec::errc::corrupt);
    bad[code_size] = 9;
    EXPECT_EQ(decode(bad), codec::errc::corrupt);
    // an unknown block where an image or an extension is due
    bad = good;
    bad[image] = char(0x2B);
    EXPECT_EQ(decode(bad), codec::errc::corrupt);
    // no color table
    bad = good;
    bad[10] = 0;   // the global table's flag off (its bytes then read as blocks)
    EXPECT_NE(decode(bad), codec::errc{});
    std::string no_table = good.substr(0, 10) + std::string(1, '\0') + good.substr(11, 2) + good.substr(13 + 12);
    EXPECT_EQ(decode(no_table), codec::errc::corrupt);
    // too few pixels: an image of 6 × 5 whose data holds 6 × 4
    GifFrame short_frame = f;
    short_frame.indices.resize(24);
    std::string few = make_gif(6, 5, {short_frame});
    EXPECT_EQ(decode(few), codec::errc::corrupt);
    // a code past the table: the first code after the clear, 7 with a width of 3 bits
    std::string past = good.substr(0, code_size + 1) + std::string("\x01\x3C\x00\x3B", 4);
    EXPECT_EQ(decode(past), codec::errc::corrupt);
    // an image past limits.max_pixels (the canvas within them)
    GifFrame wide = f;
    wide.w = 200;
    wide.h = 1;
    wide.indices = pattern(200, 1, 4, 0);
    EXPECT_EQ(code_of(codec::gif::decode(bytes(make_gif(6, 5, {wide})), {.limits = {.max_pixels = 100}})), codec::errc::too_large);
    // pixels past the image are dropped; a frame past the canvas is clipped
    GifFrame more = f;
    more.indices = pattern(6, 6, 4, 0);
    EXPECT_TRUE(codec::gif::decode(bytes(make_gif(6, 5, {more}))));
    GifFrame off = f;
    off.left = 4;
    off.top = 3;
    EXPECT_TRUE(codec::gif::decode(bytes(make_gif(6, 5, {off}))));
    // no image at all: the trailer after the screen
    EXPECT_EQ(decode(good.substr(0, 13 + 12) + ";"), codec::errc::corrupt);
}

TEST(CodecGif_Tests, AStringAcrossTheLastPixel) {
    // Regression (the review of C7): the LZW decoder writes a string whole,
    // and one that crosses the image's last pixel must fill it. 40 pixels of
    // one color for an image of 6 × 5 are the strings 1, 2, …, 8 long; the
    // one of pixels 28..35 crosses pixel 29. Before the fix: "an image with
    // fewer pixels than its size"
    GifFrame f;
    f.w = 6;
    f.h = 5;
    f.indices.assign(40, 0);
    const std::string data = make_gif(6, 5, {f});
    auto first = codec::gif::decode(bytes(data));
    ASSERT_TRUE(first) << first.error().message();
    auto px = first->pixels();
    for (size_t i = 0; i < 30; ++i) {
        ASSERT_EQ(px[4 * i], std::byte(0xFF)) << i;       // red, the first color
        ASSERT_EQ(px[4 * i + 1], std::byte(0)) << i;
        ASSERT_EQ(px[4 * i + 3], std::byte(0xFF)) << i;
    }
    // the same length exactly: the string of pixels 28..35 against an image of 36
    GifFrame g = f;
    g.h = 6;
    g.indices.assign(36, 0);
    EXPECT_TRUE(codec::gif::decode(bytes(make_gif(6, 6, {g}))));
    // giflib reads it the same (Go refuses it: "too much image data", its
    // one strictness the module does not share)
    if (!c_oracle().empty()) {
        const auto path = write_file("crossing.gif", data);
        auto theirs = run_oracle(c_oracle(), "gif", path);
        ASSERT_TRUE(theirs);
        EXPECT_EQ(theirs->pixels, std::string(reinterpret_cast<const char*>(px.data()), px.size()));
    }
}
