//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: the allocation probe. The program's operator new counts the
// allocations of the thread that asks (a count of its own per thread, so
// that the collector's threads do not add to it), and a decoder's count
// must not grow with the rows: an image of 16 rows and one of 1024 cost
// the same, in plain memory and in managed objects (the image's two).
#include "png_builder.h"
#include "webp_builder.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <new>

namespace {
    thread_local size_t news = 0;
    thread_local size_t largest = 0;   // the largest allocation of the thread since reset
}

void* operator new(size_t n) {
    ++news;
    largest = n > largest ? n : largest;
    if (void* p = std::malloc(n ? n : 1)) {
        return p;
    }
    throw std::bad_alloc();
}

void* operator new[](size_t n) {
    ++news;
    largest = n > largest ? n : largest;
    if (void* p = std::malloc(n ? n : 1)) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete[](void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, size_t) noexcept {
    std::free(p);
}

void operator delete[](void* p, size_t) noexcept {
    std::free(p);
}

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;

namespace {
    struct Cost {
        size_t news;
        size_t objects;
    };

    // What one decode of the file allocates: plain allocations of this
    // thread, managed objects alive after it with the image held
    // Each decode in a frame of its own, so that nothing it leaves on the
    // stack keeps an object alive for the collector's conservative scan
    template<class Decode>
    Cost cost(Decode decode) {
        off_frame([&] { (void)decode(); });   // what a first call makes once (a thread's registration)
        collector::force_collect(true);
        const size_t objects = collector::get_live_object_count();
        size_t before = 0, after = 0;
        sgcl::vector<codec::image> held;
        held.reserve(1);
        off_frame([&] {
            before = news;
            auto r = decode();
            after = news;
            EXPECT_TRUE(r);
            if (r) {
                held.push_back(*r);
            }
        });
        collector::force_collect(true);
        const size_t alive = collector::get_live_object_count() - objects - 1;   // the vector's buffer: one
        return {after - before, alive};
    }

    struct Pieces {
        const std::string* data;
        size_t at = 0;

        expected<size_t, io::error> read(const slice<std::byte>& b) {
            size_t n = std::min({size_t(1000), b.size(), data->size() - at});
            std::memcpy(b.data(), data->data() + at, n);
            at += n;
            return n;
        }
    };
}

TEST(CodecAllocProbe_Tests, PngAllocatesNothingPerRow) {
    struct Case { const char* what; int depth; int color; unsigned channels; int interlace; optional<pixel_format> want; };
    const Case cases[] = {
        {"rgb8, in place", 8, 2, 3, 0, nullopt},
        {"rgba16", 16, 6, 4, 0, nullopt},
        {"gray 4 bits", 4, 0, 1, 0, nullopt},
        {"palette 8 bits", 8, 3, 1, 0, nullopt},
        {"rgb8 as gray8", 8, 2, 3, 0, pixel_format::gray8},
        {"interlaced rgb8", 8, 2, 3, 1, nullopt},
        {"interlaced gray16 as rgba8", 16, 0, 1, 1, pixel_format::rgba8},
    };
    const uint32_t w = 300;
    for (const auto& c : cases) {
        Cost costs[2];
        int i = 0;
        for (uint32_t h : {16u, 1024u}) {
            const unsigned bits = c.channels * c.depth;
            const std::string rows = c.interlace ? random_interlaced_rows(w, h, bits, h) : random_rows(h, (size_t(w) * bits + 7) / 8, h);
            const std::string plte = c.color == 3 ? png_chunk("PLTE", std::string(768, '\x55')) : "";
            const std::string file = png_file(w, h, c.depth, c.color, rows, plte, c.interlace);
            codec::decode_options o{.want = c.want};
            costs[i] = cost([&] { return codec::png::decode(bytes(file), o); });
            ++i;
        }
        EXPECT_EQ(costs[0].news, costs[1].news) << c.what;
        EXPECT_EQ(costs[0].objects, costs[1].objects) << c.what;
        EXPECT_EQ(costs[1].objects, 2u) << c.what;   // the image: its state and its pixels
        EXPECT_LE(costs[1].news, 2u) << c.what;      // the block of the decoding, and nothing else
    }
}

TEST(CodecAllocProbe_Tests, PngEncodingAllocatesNothingPerRow) {
    // the encoder's plain memory: the rows, the Deflater, its output
    // buffer; the file written to a stream that drops it, so that what is
    // counted is the encoder's (a vector of the file grows as appending
    // grows it, managed, and the heap's bookkeeping of large objects with it)
    struct Drop {
        size_t bytes = 0;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            bytes += b.size();
            return b.size();
        }
    };
    const uint32_t w = 300;
    for (pixel_format f : {pixel_format::rgb8, pixel_format::rgba16, pixel_format::gray8, pixel_format::cmyk8}) {
        for (int level : {0, 6}) {
            size_t news_of[2];
            int i = 0;
            for (uint32_t h : {16u, 1024u}) {
                codec::image picture(w, h, f);
                std::mt19937 rng(h);
                for (auto& b : picture.pixels()) {
                    b = std::byte(rng() & 0x3F);   // compressible, written explicitly
                }
                Drop sink;
                off_frame([&] { (void)codec::png::encode(picture, io::writer(sink), {.level = level}); });
                size_t before = 0, after = 0;
                off_frame([&] {
                    before = news;
                    auto r = codec::png::encode(picture, io::writer(sink), {.level = level});
                    after = news;
                    EXPECT_TRUE(r);
                });
                news_of[i++] = after - before;
            }
            EXPECT_EQ(news_of[0], news_of[1]) << int(f) << " level " << level;
            EXPECT_LE(news_of[1], 8u) << int(f) << " level " << level;
        }
    }
}

TEST(CodecAllocProbe_Tests, PngFromAStreamAllocatesNothingPerRow) {
    const uint32_t w = 300;
    Cost costs[2];
    int i = 0;
    for (uint32_t h : {16u, 1024u}) {
        const std::string file = png_file(w, h, 8, 6, random_rows(h, size_t(w) * 4, h));
        costs[i++] = cost([&] {
            Pieces p{&file};
            return codec::png::decode(io::reader(p));
        });
    }
    EXPECT_EQ(costs[0].news, costs[1].news);
    EXPECT_EQ(costs[0].objects, costs[1].objects);
}

TEST(CodecAllocProbe_Tests, JpegAllocatesNothingPerMcu) {
    // files cjpeg makes of one image 16 and 1024 rows high: the same
    // allocations for both (the image, one block of rows and buffers, the
    // segments' bodies), a stream of pieces too; a file of a scan per
    // component keeps its components whole, still in one block, and a
    // progressive one the coefficients of the image, in one block more
    std::string cjpeg;
    for (const char* dir : {"/opt/homebrew/opt/jpeg-turbo/bin/", "/opt/homebrew/bin/", "/usr/local/bin/"}) {
        if (std::filesystem::exists(std::string(dir) + "cjpeg")) {
            cjpeg = std::string(dir) + "cjpeg";
            break;
        }
    }
    if (cjpeg.empty()) {
        GTEST_SKIP() << "no cjpeg";
    }
    const auto dir = codec_test::scratch_path("sgcl_codec_alloc_probe");
    const std::string scans = (dir / "scans.txt").string();
    {
        std::ofstream out(scans);
        out << "0;\n1;\n2;\n";
    }
    struct Case { const char* what; std::string args; bool gray; };
    const Case cases[] = {
        {"4:2:0", "-sample 2x2", false},
        {"4:4:4", "-sample 1x1", false},
        {"gray", "-grayscale", true},
        {"4:2:2, restart every MCU", "-sample 2x1 -restart 1B", false},
        {"a scan per component", "-sample 2x2 -scans '" + scans + "'", false},
        {"progressive 4:2:0", "-progressive -sample 2x2", false},
        {"progressive gray, restart", "-progressive -grayscale -restart 1B", true},
    };
    const uint32_t w = 96;
    for (const auto& c : cases) {
        size_t news_of[2], stream_news_of[2], objects_of[2];
        int i = 0;
        for (uint32_t h : {16u, 1024u}) {
            std::string pnm = (c.gray ? "P5\n" : "P6\n") + std::to_string(w) + " " + std::to_string(h) + "\n255\n";
            std::mt19937 rng(h);
            for (size_t k = 0; k < size_t(w) * h * (c.gray ? 1 : 3); ++k) {
                pnm += char((k / 3 + (rng() & 15)) & 0xFF);
            }
            const std::string src = (dir / "src.pnm").string(), out = (dir / "probe.jpg").string();
            {
                std::ofstream f(src, std::ios::binary);
                f << pnm;
            }
            ASSERT_EQ(std::system(("'" + cjpeg + "' " + c.args + " -outfile '" + out + "' '" + src + "'").c_str()), 0) << c.what;
            std::ifstream f(out, std::ios::binary);
            std::stringstream ss;
            ss << f.rdbuf();
            const std::string file = ss.str();
            const Cost memory = cost([&] { return codec::jpeg::decode(bytes(file)); });
            const Cost streamed = cost([&] {
                Pieces p{&file};
                return codec::jpeg::decode(io::reader(p));
            });
            news_of[i] = memory.news;
            stream_news_of[i] = streamed.news;
            objects_of[i] = memory.objects;
            ++i;
        }
        EXPECT_EQ(news_of[0], news_of[1]) << c.what;
        EXPECT_EQ(stream_news_of[0], stream_news_of[1]) << c.what;
        EXPECT_EQ(objects_of[0], objects_of[1]) << c.what;
        EXPECT_EQ(objects_of[1], 2u) << c.what;
    }
}

TEST(CodecAllocProbe_Tests, JpegEncodingAllocatesNothingPerMcu) {
    // the encoder's plain memory: one row of MCUs, the Huffman tables, a
    // block of output; the file written to a stream that drops it
    struct Drop {
        size_t bytes = 0;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            bytes += b.size();
            return b.size();
        }
    };
    const uint32_t w = 200;
    for (pixel_format f : {pixel_format::rgb8, pixel_format::gray8, pixel_format::rgba16}) {
        for (auto s : {codec::jpeg::subsampling::s420, codec::jpeg::subsampling::s444}) {
            for (bool optimize : {false, true}) {
                size_t news_of[2];
                int i = 0;
                for (uint32_t h : {16u, 1024u}) {
                    codec::image picture(w, h, f);
                    std::mt19937 rng(h);
                    for (auto& b : picture.pixels()) {
                        b = std::byte(rng() & 0x3F);
                    }
                    const codec::jpeg::options o{.subsampling = s, .optimize = optimize};
                    Drop sink;
                    off_frame([&] { (void)codec::jpeg::encode(picture, io::writer(sink), o); });
                    size_t before = 0, after = 0;
                    off_frame([&] {
                        before = news;
                        auto r = codec::jpeg::encode(picture, io::writer(sink), o);
                        after = news;
                        EXPECT_TRUE(r);
                    });
                    news_of[i++] = after - before;
                }
                EXPECT_EQ(news_of[0], news_of[1]) << int(f) << " " << int(s) << (optimize ? " optimize" : "");
            }
        }
    }
}

TEST(CodecAllocProbe_Tests, GifAllocatesNothingPerFrame) {
    // an animation of 4 frames and one of 64, each frame the whole canvas:
    // the same plain allocations (the canvas, the indices, the decoder's
    // state), and per frame only its image (two managed objects), read from
    // memory and from a stream of pieces
    auto make = [](int count) {
        auto le16 = [](unsigned v) { return std::string{char(v & 0xFF), char(v >> 8)}; };
        const unsigned w = 64, h = 48;
        std::string out = "GIF89a" + le16(w) + le16(h) + std::string{char(0x81), 0, 0};
        out += std::string("\xFF\x00\x00\x00\xFF\x00\x00\x00\xFF\xFF\xFF\x00", 12);
        for (int f = 0; f < count; ++f) {
            std::vector<std::byte> idx(size_t(w) * h);
            for (size_t i = 0; i < idx.size(); ++i) {
                idx[i] = std::byte((i / 5 + f) & 3);
            }
            auto z = compress::lzw::compress(slice<const std::byte>(idx.data(), idx.size()), compress::lzw::order::lsb, 2);
            out += std::string("\x21\xF9\x04\x08\x02\x00\x00\x00", 8);   // disposal 2, 20 ms
            out += char(0x2C) + le16(0) + le16(0) + le16(w) + le16(h) + char(0) + char(2);
            for (size_t i = 0; i < z.size(); i += 255) {
                const size_t n = std::min<size_t>(255, z.size() - i);
                out += char(n);
                out.append(reinterpret_cast<const char*>(z.data()) + i, n);
            }
            out += char(0);
        }
        return out + ";";
    };
    for (bool stream : {false, true}) {
        size_t news_of[2], objects_of[2];
        int i = 0;
        for (int count : {4, 64}) {
            const std::string file = make(count);
            auto read = [&] {
                Pieces p{&file};
                auto f = stream ? codec::gif::frames(io::reader(p)) : codec::gif::frames(bytes(file));
                int n = 0;
                for (;;) {
                    auto next = f->next();
                    EXPECT_TRUE(next);
                    if (!next || !*next) {
                        break;
                    }
                    ++n;
                }
                EXPECT_EQ(n, count);
            };
            off_frame(read);   // what a first call makes once
            collector::force_collect(true);
            size_t before = 0, after = 0;
            off_frame([&] {
                before = news;
                read();
                after = news;
            });
            news_of[i] = after - before;
            ++i;
        }
        EXPECT_EQ(news_of[0], news_of[1]) << (stream ? "stream" : "memory");
        (void)objects_of;
    }
}

TEST(CodecAllocProbe_Tests, LossyWebpAllocatesNothingPerRow) {
    // lossy files cwebp makes of one picture 16 and 1024 rows high, with
    // alpha and without: the same plain allocations (the planes, the rows
    // of the conversion, the alpha's buffers, each once), the image's two
    // managed objects, from memory and from a stream of pieces
    std::string cwebp;
    for (const char* dir : {"/opt/homebrew/bin/", "/opt/homebrew/opt/webp/bin/", "/usr/local/bin/"}) {
        if (std::filesystem::exists(std::string(dir) + "cwebp")) {
            cwebp = std::string(dir) + "cwebp";
            break;
        }
    }
    if (cwebp.empty()) {
        GTEST_SKIP() << "no cwebp";
    }
    const auto dir = codec_test::scratch_path("sgcl_codec_alloc_probe");
    const uint32_t w = 300;
    for (bool alpha : {false, true}) {
        for (bool stream : {false, true}) {
            Cost costs[2];
            int i = 0;
            for (uint32_t h : {16u, 1024u}) {
                codec::image picture(w, h, alpha ? pixel_format::rgba8 : pixel_format::rgb8);
                std::mt19937 rng(h);
                auto px = picture.pixels();
                for (size_t k = 0; k < px.size(); ++k) {
                    px[k] = std::byte(((k / 7) + (rng() & 31)) & 0xFF);
                }
                const std::string src = (dir / "lossy.png").string(), out = (dir / "lossy.webp").string();
                {
                    auto png = codec::png::encode(picture);
                    std::ofstream f(src, std::ios::binary);
                    f.write(reinterpret_cast<const char*>(png.data()), std::streamsize(png.size()));
                }
                // the alpha raw and unfiltered: a stream whose shape does not change with the rows (the
                // encoder picks a lossless alpha's transforms and cache by its content; VP8L's own
                // probe is WebpAllocatesNothingPerRow)
                ASSERT_EQ(std::system(("'" + cwebp + "' -quiet -q 70 -alpha_method 0 -alpha_filter none '" + src + "' -o '" + out + "'").c_str()), 0);
                std::ifstream in(out, std::ios::binary);
                std::stringstream ss;
                ss << in.rdbuf();
                const std::string file = ss.str();
                auto decode = [&] {
                    Pieces p{&file};
                    return stream ? codec::webp::decode(io::reader(p)) : codec::webp::decode(bytes(file));
                };
                // measured the second time: the first may grow the heap's
                // own lists of free pages (plain vectors of the collector,
                // not the decoder's), which the image's pages come back to
                (void)cost(decode);
                costs[i++] = cost(decode);
            }
            EXPECT_EQ(costs[0].news, costs[1].news) << (alpha ? "alpha" : "opaque") << (stream ? ", stream" : "");
            EXPECT_EQ(costs[0].objects, costs[1].objects);
            EXPECT_EQ(costs[1].objects, 2u);   // the image: its state and its pixels
        }
    }
}

TEST(CodecAllocProbe_Tests, WebpAllocatesNothingPerRow) {
    // VP8L of 16 rows and of 1024, with each transform: the same plain
    // allocations (the decoder's buffers, each once), the image's two
    // managed objects, from memory and from a stream of pieces
    namespace wb = codec_test::webp;
    const uint32_t w = 300;
    struct Case { const char* what; int transforms; optional<pixel_format> want; };
    const Case cases[] = {
        {"plain", 0, nullopt},
        {"subtract green, color, predictor", 1, nullopt},
        {"palette of 4", 2, nullopt},
        {"as gray16", 1, pixel_format::gray16},
    };
    for (const auto& c : cases) {
        for (bool stream : {false, true}) {
            Cost costs[2];
            int i = 0;
            for (uint32_t h : {16u, 1024u}) {
                std::vector<wb::Transform> t;
                std::vector<uint32_t> px = wb::pattern(w, h, h, true);
                if (c.transforms == 1) {
                    const uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4;
                    t = {{2, 2, {}}, {1, 2, wb::pattern(bw, bh, 1, false)}, {0, 2, wb::pattern(bw, bh, 2, false)}};
                } else if (c.transforms == 2) {
                    t = {{3, 0, {0xff000000u, 0xff0000ffu, 0x80ff0000u, 0xffffffffu}}};
                    px.assign(size_t((w + 3) / 4) * h, 0xff001b00u);
                }
                const std::string file = wb::riff(wb::chunk("VP8L", wb::vp8l(w, h, px, t)));
                const codec::decode_options d{.want = c.want};
                costs[i++] = cost([&] {
                    Pieces p{&file};
                    return stream ? codec::decode(io::reader(p), d) : codec::decode(bytes(file), d);
                });
            }
            EXPECT_EQ(costs[0].news, costs[1].news) << c.what << (stream ? ", stream" : "");
            EXPECT_EQ(costs[0].objects, costs[1].objects) << c.what;
            EXPECT_EQ(costs[1].objects, 2u) << c.what;   // the image: its state and its pixels
        }
    }
}

TEST(CodecAllocProbe_Tests, WebpAllocatesNothingPerFrame) {
    // an animation of 4 frames and one of 64: the decoder's buffers once,
    // kept from frame to frame; per frame only its image
    namespace wb = codec_test::webp;
    auto make = [](int count) {
        std::string body = wb::vp8x(0x12, 40, 30) + wb::anim(0);
        for (int f = 0; f < count; ++f) {
            body += wb::anmf(uint32_t(f % 3) * 2, 0, 30, 30, 20, f % 2 == 0, f % 4 == 1,
                             wb::chunk("VP8L", wb::vp8l(30, 30, wb::pattern(30, 30, unsigned(f), true), {{2, 2, {}}})));
        }
        return wb::riff(body);
    };
    for (bool stream : {false, true}) {
        size_t news_of[2];
        int i = 0;
        for (int count : {4, 64}) {
            const std::string file = make(count);
            auto read = [&] {
                Pieces p{&file};
                auto f = stream ? codec::webp::frames(io::reader(p)) : codec::webp::frames(bytes(file));
                int n = 0;
                for (;;) {
                    auto next = f->next();
                    EXPECT_TRUE(next);
                    if (!next || !*next) {
                        break;
                    }
                    ++n;
                }
                EXPECT_EQ(n, count);
            };
            off_frame(read);
            collector::force_collect(true);
            size_t before = 0, after = 0;
            off_frame([&] {
                before = news;
                read();
                after = news;
            });
            news_of[i++] = after - before;
        }
        EXPECT_EQ(news_of[0], news_of[1]) << (stream ? "stream" : "memory");
    }
}

// A chunk whose size claims more than the data holds (the fuzzer's find: an
// ALPH chunk in an ANMF claiming 4 GB, in 144 bytes) is refused before
// anything of that size is allocated (from a stream at most the 4 MB a
// chunk is read at once): from memory, from a stream in pieces, and
// through frames. The chunks: an ALPH past its ANMF; an ANMF and
// its ALPH both claiming 4 GB under a RIFF claiming 4 GB; a still image's
// ALPH; a VP8 chunk read from a stream; EXIF within max_metadata but past
// the data. The file of the find is a seed of the fuzzer too
// (seeds/webp_decode/regress_anmf_alph_size.webp)
TEST(CodecAllocProbe_Tests, WebpClaimedSizesAllocateWhatComes) {
    namespace wb = codec_test::webp;
    // a VP8 key frame's first ten bytes, 16x16
    const std::string vp8_head = std::string("\x70\x01\x00\x9d\x01\x2a\x10\x00\x10\x00", 10);
    auto chunk_claiming = [](const std::string& tag, uint32_t claimed, const std::string& payload) {
        return tag + wb::le32(claimed) + payload;
    };
    auto riff_claiming = [](uint32_t claimed, const std::string& chunks) {
        return "RIFF" + wb::le32(claimed) + "WEBP" + chunks;
    };
    const std::string filler(64, '\x5a');
    const std::string anmf_head = wb::le24(0) + wb::le24(0) + wb::le24(15) + wb::le24(15) + wb::le24(100) + std::string(1, '\0');
    struct Case {
        const char* what;
        std::string file;
    };
    const Case cases[] = {
        {"an ALPH past its ANMF",
         riff_claiming(0xF7FF013C, wb::vp8x(0x12, 16, 16) + wb::anim(0) +
                                       chunk_claiming("ANMF", 100, anmf_head + chunk_claiming("ALPH", 0xF0E04349, filler) + filler))},
        {"an ANMF and its ALPH claiming 4 GB",
         riff_claiming(0xFFFFFFF0, wb::vp8x(0x12, 16, 16) + wb::anim(0) +
                                       chunk_claiming("ANMF", 0xFFFFFF00, anmf_head + chunk_claiming("ALPH", 0xFFFFF000, filler)))},
        {"a still image's ALPH claiming 4 GB", riff_claiming(0xFFFFFFF0, wb::vp8x(0x10, 16, 16) + chunk_claiming("ALPH", 0xFFFFF000, filler))},
        {"a VP8 chunk claiming 4 GB", riff_claiming(0xFFFFFFF0, chunk_claiming("VP8 ", 0xFFFFF000, vp8_head + filler))},
        {"EXIF within max_metadata, past the data",
         riff_claiming(0xFFFFFFF0, wb::vp8x(0x08, 16, 16) + chunk_claiming("EXIF", 60u << 20, filler))},
    };
    for (const auto& c : cases) {
        slice<const std::byte> bytes(reinterpret_cast<const std::byte*>(c.file.data()), c.file.size());
        // from memory
        largest = 0;
        auto whole = codec::webp::decode(bytes);
        EXPECT_FALSE(whole) << c.what;
        EXPECT_LT(largest, size_t(1) << 23) << c.what << " (memory)";
        // from a stream, in pieces of 1000 bytes
        largest = 0;
        Pieces pieces{&c.file};
        auto streamed = codec::webp::decode(io::reader(pieces));
        EXPECT_FALSE(streamed) << c.what;
        EXPECT_LT(largest, size_t(1) << 23) << c.what << " (stream)";
        // through frames, from a stream
        largest = 0;
        Pieces again{&c.file};
        auto frames = codec::decode_frames(io::reader(again));
        if (frames) {
            auto f = frames->next();
            EXPECT_FALSE(f && *f) << c.what;
        }
        EXPECT_LT(largest, size_t(1) << 23) << c.what << " (frames)";
    }
    // the fuzzer's file itself
    std::ifstream in(std::filesystem::path(__FILE__).parent_path() / "fuzz/seeds/webp_decode/regress_anmf_alph_size.webp", std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string seed = ss.str();
    ASSERT_FALSE(seed.empty());
    largest = 0;
    Pieces pieces{&seed};
    auto frames = codec::decode_frames(io::reader(pieces));
    if (frames) {
        auto f = frames->next();
        EXPECT_FALSE(f && *f);
    }
    EXPECT_LT(largest, size_t(1) << 23);
}
