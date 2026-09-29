//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: WebP (webp.h, decode.h, detail/webp_container.h, detail/vp8l_*.h).
// Every lossless file of libwebp-test-data pixel for pixel against libwebp
// and Go's x/image/webp, and against the MD5 of what dwebp writes; the lossy
// ones errc::unsupported until VP8. Files made here (webp_builder.h): the
// predictor modes 14 and 15, which RFC 9649 leaves out, decoding as 0 in both
// decoders; animations of every blending and disposal against libwebp's
// WebPAnimDecoder (blended pixels within one of it, the rest exact); the
// container's order, which the module takes or refuses as libwebp's demuxer
// does; metadata, limits, negatives of the right code; a stream in pieces as
// memory; decode_options for every format.
#include "md5.h"
#include "oracle.h"
#include "webp_builder.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <vector>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;
namespace wb = codec_test::webp;

namespace {
    const std::string TestData = "libwebp-test-data";

    std::filesystem::path scratch_dir() {
        return scratch_path("sgcl_codec_webp_tests");
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

    std::vector<std::string> test_files() {
        std::vector<std::string> out;
        const auto dir = oracle_path(TestData);
        if (std::filesystem::is_directory(dir)) {
            for (auto& e : std::filesystem::directory_iterator(dir)) {
                if (e.path().extension() == ".webp") {
                    out.push_back(e.path().string());
                }
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    // The MD5 libwebp-test-data lists for each output of dwebp
    std::map<std::string, std::string> md5_list() {
        std::map<std::string, std::string> m;
        std::istringstream in(read_file(oracle_path(TestData + "/libwebp_tests.md5")));
        std::string sum, name;
        while (in >> sum >> name) {
            m[name] = sum;
        }
        return m;
    }

    // A lossy file's planes as the module decodes them: the rows of Y, of U
    // and of V, cropped to the picture ("" when it does not decode)
    std::string yuv_of(const std::string& data) {
        codec::detail::MemoryInput in(bytes(data));
        codec::decode_options o;
        codec::detail::WebpReader<codec::detail::MemoryInput> r(in, o);
        codec::detail::WebpFrame f;
        if (!r.start() || r.next(f) != codec::detail::WebpReader<codec::detail::MemoryInput>::Step::frame || !f.lossy || !r.decode(f)) {
            return "";
        }
        const auto& d = r.lossy_decoder();
        EXPECT_FALSE(d.beyond_encoders());
        const uint32_t w = f.width, h = f.height, cw = (w + 1) / 2, ch = (h + 1) / 2;
        std::string out;
        for (uint32_t y = 0; y < h; ++y) {
            out.append(reinterpret_cast<const char*>(d.y() + y * d.y_stride()), w);
        }
        for (const uint8_t* plane : {d.u(), d.v()}) {
            for (uint32_t y = 0; y < ch; ++y) {
                out.append(reinterpret_cast<const char*>(plane + y * d.uv_stride()), cw);
            }
        }
        return out;
    }

    // dwebp -pgm's form of the planes: rows of 2·⌈w/2⌉ bytes, Y (zeros past
    // w), then each row of U followed by its row of V, then the alpha when
    // there is one (w bytes a row, zeros past w)
    std::string pgm(const std::string& planes, uint32_t w, uint32_t h, const std::string& alpha) {
        const uint32_t cw = (w + 1) / 2, ch = (h + 1) / 2, W = 2 * cw;
        std::string out = "P5\n" + std::to_string(W) + " " + std::to_string(h + ch + (alpha.empty() ? 0 : h)) + "\n255\n";
        for (uint32_t y = 0; y < h; ++y) {
            out += planes.substr(size_t(y) * w, w) + std::string(W - w, '\0');
        }
        const size_t u = size_t(w) * h, v = u + size_t(cw) * ch;
        for (uint32_t y = 0; y < ch; ++y) {
            out += planes.substr(u + size_t(y) * cw, cw) + planes.substr(v + size_t(y) * cw, cw);
        }
        for (uint32_t y = 0; !alpha.empty() && y < h; ++y) {
            out += alpha.substr(size_t(y) * w, w) + std::string(W - w, '\0');
        }
        return out;
    }

    // dwebp -pam's form of an image
    std::string pam(const codec::image& rgba) {
        auto px = rgba.pixels();
        return "P7\nWIDTH " + std::to_string(rgba.width()) + "\nHEIGHT " + std::to_string(rgba.height()) +
               "\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n" + std::string(reinterpret_cast<const char*>(px.data()), px.size());
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

    struct Read {
        uint32_t width = 0, height = 0, plays = 0;
        std::vector<std::string> canvases;
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
            r.canvases.emplace_back(reinterpret_cast<const char*>(px.data()), px.size());
            r.delays_ms.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::nanoseconds((*n)->delay)).count());
        }
        r.plays = f->loop_count();
        return r;
    }

    codec::errc code_of(const expected<codec::image, codec::error>& r) {
        return r ? codec::errc{} : r.error().code();
    }

    std::string rgba_of(const codec::image& im) {
        auto px = im.pixels();
        return std::string(reinterpret_cast<const char*>(px.data()), px.size());
    }

    bool has_vp8(const std::string& data) {
        return data.find("VP8 ") != std::string::npos || data.find("ALPH") != std::string::npos;
    }

    // A still VP8L file of the simple format
    std::string simple(uint32_t w, uint32_t h, const std::vector<uint32_t>& argb, const std::vector<wb::Transform>& t = {}, bool alpha = true) {
        return wb::riff(wb::chunk("VP8L", wb::vp8l(w, h, argb, t, alpha)));
    }
}

TEST(CodecWebp_Tests, TheTestDataAgainstLibwebpGoAndMd5) {
    auto files = test_files();
    if (files.empty()) {
        GTEST_SKIP() << "no " << oracle_path(TestData);
    }
    const auto sums = md5_list();
    size_t lossless = 0, lossy = 0, md5_checked = 0, pgm_checked = 0;
    for (const auto& path : files) {
        const std::string name = std::filesystem::path(path).filename().string();
        const std::string data = read_file(path);
        auto ours = codec::decode(bytes(data), {.want = pixel_format::rgba8});
        ASSERT_TRUE(ours) << name << ": " << ours.error().message().view();
        const bool is_lossy = has_vp8(data);
        ++(is_lossy ? lossy : lossless);
        // RGBA against libwebp (WebPDecodeRGBA) byte for byte, lossy too
        if (!c_oracle().empty()) {
            auto theirs = run_oracle(c_oracle(), "webp", path);
            ASSERT_TRUE(theirs) << name;
            EXPECT_EQ(theirs->width, ours->width()) << name;
            EXPECT_EQ(theirs->height, ours->height()) << name;
            EXPECT_TRUE(theirs->pixels == rgba_of(*ours)) << name << " against libwebp";
        }
        // Go converts YUV its own way: against Go the lossless pixels, the lossy planes below
        if (!go_webp_oracle().empty() && !is_lossy) {
            auto theirs = run_oracle(go_webp_oracle(), "webp", path);
            ASSERT_TRUE(theirs) << name;
            EXPECT_TRUE(theirs->pixels == rgba_of(*ours)) << name << " against Go";
        }
        auto sum = sums.find(name + ".pam");
        if (sum != sums.end()) {
            EXPECT_EQ(md5(pam(*ours).data(), pam(*ours).size()), sum->second) << name;
            ++md5_checked;
        }
        if (is_lossy) {
            // the planes of Y, U and V against libwebp's, Go's and dwebp -pgm's MD5
            const std::string planes = yuv_of(data);
            ASSERT_FALSE(planes.empty()) << name;
            if (!c_oracle().empty()) {
                auto theirs = run_oracle(c_oracle(), "webpyuv", path);
                ASSERT_TRUE(theirs) << name;
                EXPECT_TRUE(theirs->pixels == planes) << name << " YUV against libwebp";
            }
            if (!go_webp_oracle().empty()) {
                auto theirs = run_oracle(go_webp_oracle(), "webpyuv", path);
                ASSERT_TRUE(theirs) << name;
                EXPECT_TRUE(theirs->pixels == planes) << name << " YUV against Go";
            }
            auto pgm_sum = sums.find(name + ".pgm");
            if (pgm_sum != sums.end()) {
                std::string alpha;
                if (data.find("ALPH") != std::string::npos) {
                    const std::string rgba = rgba_of(*ours);
                    for (size_t i = 3; i < rgba.size(); i += 4) {
                        alpha += rgba[i];
                    }
                }
                const std::string p = pgm(planes, ours->width(), ours->height(), alpha);
                EXPECT_EQ(md5(p.data(), p.size()), pgm_sum->second) << name;
                ++pgm_checked;
            }
        }
        // the same through codec::decode and webp::decode, in the file's own format
        codec::image native = *codec::webp::decode(bytes(data));
        codec::image any = *codec::decode(bytes(data));
        EXPECT_TRUE(rgba_of(native.convert(pixel_format::rgba8)) == rgba_of(*ours) || native.format() == pixel_format::rgb8) << name;
        EXPECT_TRUE(rgba_of(any) == rgba_of(native)) << name;
    }
    EXPECT_GE(lossless, 40u);
    EXPECT_GE(lossy, 80u);
    if (!sums.empty()) {
        EXPECT_GE(md5_checked, 80u);
        EXPECT_GE(pgm_checked, 60u);
    }
}

TEST(CodecWebp_Tests, TheLosslessVectorsAreTheirReferenceImages) {
    // test_lossless.sh of libwebp-test-data: lossless_vec_1_* are grid.pam,
    // lossless_vec_2_* peak.pam, lossless_color_transform its own .pam
    CODEC_ORACLE(grid, TestData + "/grid.pam");
    CODEC_ORACLE(peak, TestData + "/peak.pam");
    CODEC_ORACLE(transform, TestData + "/lossless_color_transform.pam");
    size_t checked = 0;
    for (int set : {1, 2}) {
        for (int i = 0; i < 16; ++i) {
            const std::string name = "lossless_vec_" + std::to_string(set) + "_" + std::to_string(i) + ".webp";
            CODEC_ORACLE(data, TestData + "/" + name);
            auto ours = codec::decode(bytes(data), {.want = pixel_format::rgba8});
            ASSERT_TRUE(ours) << name;
            EXPECT_TRUE(pam(*ours) == (set == 1 ? grid : peak)) << name;
            ++checked;
        }
    }
    CODEC_ORACLE(data, TestData + "/lossless_color_transform.webp");
    auto ours = codec::decode(bytes(data), {.want = pixel_format::rgba8});
    ASSERT_TRUE(ours);
    EXPECT_TRUE(pam(*ours) == transform);
    EXPECT_EQ(checked, 32u);
}

TEST(CodecWebp_Tests, PredictorModes14And15AreMode0) {
    // RFC 9649 names modes 0 to 13; a block of 14 or 15 predicts as 0
    // (black) in libwebp and here, shown on a file that uses them
    const uint32_t w = 12, h = 9;
    const auto residuals = wb::pattern(w, h, 3, true);
    auto file_of = [&](unsigned a, unsigned b) {
        // blocks of 4: 3 × 3 of them, the modes in green
        std::vector<uint32_t> modes;
        const unsigned m[9] = {a, b, 0, 13, a, 11, b, 5, a};
        for (unsigned k : m) {
            modes.push_back(0xff000000u | k << 8);
        }
        return simple(w, h, residuals, {{0, 2, modes}});
    };
    const std::string with = file_of(14, 15);
    const std::string as_zero = file_of(0, 0);
    auto ours = codec::decode(bytes(with), {.want = pixel_format::rgba8});
    ASSERT_TRUE(ours) << ours.error().message().view();
    auto zero = codec::decode(bytes(as_zero), {.want = pixel_format::rgba8});
    ASSERT_TRUE(zero);
    EXPECT_TRUE(rgba_of(*ours) == rgba_of(*zero));
    if (c_oracle().empty()) {
        GTEST_SKIP() << "no libwebp";
    }
    auto theirs = run_oracle(c_oracle(), "webp", write_file("modes14_15.webp", with));
    ASSERT_TRUE(theirs) << "libwebp refused it";
    EXPECT_TRUE(theirs->pixels == rgba_of(*ours));
    auto theirs_zero = run_oracle(c_oracle(), "webp", write_file("modes0.webp", as_zero));
    ASSERT_TRUE(theirs_zero);
    EXPECT_TRUE(theirs_zero->pixels == theirs->pixels);
}

TEST(CodecWebp_Tests, EveryTransformMadeHere) {
    if (c_oracle().empty()) {
        GTEST_SKIP() << "no libwebp";
    }
    // each transform and their orders, every predictor mode, palettes of
    // every bundling, widths that do not divide the blocks
    int n = 0;
    for (uint32_t w : {1u, 7u, 16u, 33u}) {
        for (uint32_t h : {1u, 5u, 18u}) {
            std::vector<uint32_t> modes;
            const uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4;
            for (uint32_t i = 0; i < bw * bh; ++i) {
                modes.push_back(0xff000000u | ((i * 5 + w) % 16) << 8);
            }
            std::vector<uint32_t> elements;
            for (uint32_t i = 0; i < bw * bh; ++i) {
                elements.push_back(0xff000000u | ((i * 77 + 3) & 0xff) << 16 | ((i * 131 + 200) & 0xff) << 8 | ((i * 29 + 90) & 0xff));
            }
            const auto px = wb::pattern(w, h, n, true);
            std::vector<std::vector<wb::Transform>> sets = {
                {},
                {{2, 2, {}}},
                {{0, 2, modes}},
                {{1, 2, elements}},
                {{2, 2, {}}, {1, 2, elements}, {0, 2, modes}},
                {{0, 2, modes}, {2, 2, {}}},
            };
            for (unsigned colors : {2u, 3u, 11u, 200u}) {
                std::vector<uint32_t> palette;
                for (unsigned i = 0; i < colors; ++i) {
                    palette.push_back(wb::pattern(colors, 1, i, true)[i]);
                }
                const unsigned bits = colors > 16 ? 0 : colors > 4 ? 1 : colors > 2 ? 2 : 3;
                const uint32_t packed = (w + (1u << bits) - 1) >> bits;
                std::vector<uint32_t> indices(size_t(packed) * h);
                for (size_t i = 0; i < indices.size(); ++i) {
                    indices[i] = 0xff000000u | uint32_t((i * 13 + 7) & (bits ? 0xff : 0xff)) << 8;
                }
                const std::string file = simple(w, h, indices, {{3, 0, palette}});
                auto ours = codec::decode(bytes(file), {.want = pixel_format::rgba8});
                ASSERT_TRUE(ours) << ours.error().message().view();
                auto theirs = run_oracle(c_oracle(), "webp", write_file("palette.webp", file));
                ASSERT_TRUE(theirs) << "colors " << colors;
                EXPECT_TRUE(theirs->pixels == rgba_of(*ours)) << w << "x" << h << " colors " << colors;
            }
            for (size_t k = 0; k < sets.size(); ++k) {
                const std::string file = simple(w, h, px, sets[k]);
                auto ours = codec::decode(bytes(file), {.want = pixel_format::rgba8});
                ASSERT_TRUE(ours) << ours.error().message().view();
                auto theirs = run_oracle(c_oracle(), "webp", write_file("transforms.webp", file));
                ASSERT_TRUE(theirs) << w << "x" << h << " set " << k;
                EXPECT_TRUE(theirs->pixels == rgba_of(*ours)) << w << "x" << h << " set " << k;
            }
            ++n;
        }
    }
}

namespace {
    // One frame of an animation made here
    struct Frame {
        uint32_t x, y, w, h;
        bool blend, dispose;
        bool alpha;
        unsigned seed;
        uint32_t duration;
    };

    std::string animation(uint32_t cw, uint32_t ch, const std::vector<Frame>& frames, uint32_t loops) {
        std::string body = wb::vp8x(0x02 | 0x10, cw, ch) + wb::anim(loops, 0xff336699u);
        for (const auto& f : frames) {
            auto px = wb::pattern(f.w, f.h, f.seed, f.alpha);
            if (f.alpha) {
                // some pixels opaque and some fully transparent, the rest between
                for (size_t i = 0; i < px.size(); i += 5) {
                    px[i] |= 0xff000000u;
                }
                for (size_t i = 2; i < px.size(); i += 7) {
                    px[i] &= 0x00ffffffu;
                }
            }
            body += wb::anmf(f.x, f.y, f.w, f.h, f.duration, f.blend, f.dispose, wb::chunk("VP8L", wb::vp8l(f.w, f.h, px, {}, f.alpha)));
        }
        return wb::riff(body);
    }

    // The canvases against WebPAnimDecoder's: exact, but a pixel of a
    // blended frame's rectangle within one in alpha and, in each color,
    // within what libwebp's fixed-point blend may be off the exact formula
    // (about 255 / A: its error in the weight of the pixel below, divided
    // by the alpha of the result), and a pixel transparent on both sides by
    // its alpha alone
    std::string against_libwebp(const std::string& data, const std::vector<Frame>& frames, uint32_t cw) {
        const Read ours = read_all(codec::webp::frames(bytes(data)));
        if (!ours.failure.empty()) {
            return "ours: " + ours.failure;
        }
        const auto path = write_file("anim.webp", data);
        auto theirs = run_oracle(c_oracle(), "webpanim", path);
        if (!theirs) {
            return "libwebp refused it";
        }
        const size_t canvas = size_t(theirs->width) * theirs->height * 4;
        if (ours.canvases.size() * canvas != theirs->pixels.size()) {
            return "frame count";
        }
        for (size_t k = 0; k < ours.canvases.size(); ++k) {
            const std::string& a = ours.canvases[k];
            const char* b = theirs->pixels.data() + k * canvas;
            for (size_t p = 0; p < canvas; p += 4) {
                const uint8_t* x = reinterpret_cast<const uint8_t*>(a.data() + p);
                const uint8_t* y = reinterpret_cast<const uint8_t*>(b + p);
                if (std::memcmp(x, y, 4) == 0 || (x[3] == 0 && y[3] == 0)) {
                    continue;
                }
                const uint32_t px = uint32_t(p / 4 % cw), py = uint32_t(p / 4 / cw);
                bool blended = false;
                for (size_t j = 0; j <= k; ++j) {
                    const Frame& f = frames[j];
                    blended |= f.blend && px >= f.x && px < f.x + f.w && py >= f.y && py < f.y + f.h;
                }
                bool near = blended && std::abs(int(x[3]) - int(y[3])) <= 1;
                const int a = std::max(1, std::min(int(x[3]), int(y[3])));
                for (int c = 0; c < 3; ++c) {
                    near &= std::abs(int(x[c]) - int(y[c])) <= 1 + 255 / a;
                }
                if (!near) {
                    return "frame " + std::to_string(k) + " pixel (" + std::to_string(px) + ", " + std::to_string(py) + "): ours " +
                           std::to_string(x[0]) + "," + std::to_string(x[1]) + "," + std::to_string(x[2]) + "," + std::to_string(x[3]) +
                           " libwebp " + std::to_string(y[0]) + "," + std::to_string(y[1]) + "," + std::to_string(y[2]) + "," +
                           std::to_string(y[3]);
                }
            }
        }
        FILE* p = popen(("'" + c_oracle() + "' webpmeta '" + path + "' 2>/dev/null").c_str(), "r");
        uint32_t plays = 0;
        std::vector<int64_t> delays;
        char line[128];
        while (p && fgets(line, sizeof(line), p)) {
            unsigned v;
            int d;
            if (std::sscanf(line, "plays %u", &v) == 1) {
                plays = v;
            } else if (std::sscanf(line, "delay %d", &d) == 1) {
                delays.push_back(d);
            }
        }
        if (p) {
            pclose(p);
        }
        if (plays != ours.plays || delays != ours.delays_ms) {
            return "loop count or delays";
        }
        return "";
    }
}

TEST(CodecWebp_Tests, AnimationsAgainstWebPAnimDecoder) {
    if (c_oracle().empty()) {
        GTEST_SKIP() << "no libwebp";
    }
    const uint32_t cw = 24, ch = 17;
    int n = 0;
    for (bool blend1 : {false, true}) {
        for (bool dispose1 : {false, true}) {
            for (bool blend2 : {false, true}) {
                for (bool dispose2 : {false, true}) {
                    for (bool alpha : {false, true}) {
                        std::vector<Frame> frames = {
                            {0, 0, cw, ch, blend1, dispose1, alpha, 1, 70},
                            {4, 2, 9, 7, blend2, dispose2, true, 2, 0},
                            {10, 6, 14, 11, true, false, alpha, 3, 1000},
                            {2, 0, 5, 3, blend2, false, true, 4, 40},
                        };
                        const uint32_t loops = uint32_t(n % 3 == 0 ? 0 : n % 3 == 1 ? 1 : 7);
                        EXPECT_EQ(against_libwebp(animation(cw, ch, frames, loops), frames, cw), "")
                            << "blend " << blend1 << blend2 << " dispose " << dispose1 << dispose2 << " alpha " << alpha;
                        ++n;
                    }
                }
            }
        }
    }
}

TEST(CodecWebp_Tests, TheBlendFormula) {
    using codec::detail::WebpCanvas;
    // opaque over anything, transparent over anything
    EXPECT_EQ(WebpCanvas::blend(0xff102030u, 0x80405060u), 0xff102030u);
    EXPECT_EQ(WebpCanvas::blend(0x00102030u, 0x80405060u), 0x80405060u);
    EXPECT_EQ(WebpCanvas::blend(0x00102030u, 0x00405060u), 0u);
    // onto transparent: the source itself
    EXPECT_EQ(WebpCanvas::blend(0x80102030u, 0u), 0x80102030u);
    // half over opaque white: A = 255, each color (c·128 + 255·127) / 255
    const uint32_t v = WebpCanvas::blend(0x80000000u, 0xffffffffu);
    EXPECT_EQ(v >> 24, 255u);
    EXPECT_EQ(v & 0xff, uint32_t((255 * 127 * 255 + (255 * 255) / 2) / (255 * 255)));
}

namespace {
    // The container as the demuxer takes it: whether libwebp decodes every
    // frame of the file (WebPAnimDecoder) as frames() does, and the first
    // (the demuxer's checks of the whole file, then one frame) as decode()
    std::string acceptance(const std::string& name, const std::string& data) {
        const Read ours = read_all(codec::webp::frames(bytes(data)));
        const bool ours_ok = ours.failure.empty();
        auto first = codec::webp::decode(bytes(data));
        const auto path = write_file(name + ".webp", data);
        const bool theirs_ok = bool(run_oracle(c_oracle(), "webpanim", path));
        const bool theirs_first = bool(run_oracle(c_oracle(), "webpfirst", path));
        if (ours_ok != theirs_ok || bool(first) != theirs_first) {
            return name + ": libwebp " + (theirs_ok ? "takes" : "refuses") + " it (the first frame: " + (theirs_first ? "takes" : "refuses") +
                   "), frames " + (ours_ok ? "takes" : "refuse (" + ours.failure + ")") + ", decode " + (first ? "takes" : "refuses");
        }
        return "";
    }
}

TEST(CodecWebp_Tests, TheContainerAsLibwebpTakesIt) {
    if (c_oracle().empty()) {
        GTEST_SKIP() << "no libwebp";
    }
    const uint32_t w = 6, h = 4;
    const std::string img = wb::chunk("VP8L", wb::vp8l(w, h, wb::pattern(w, h, 1, true)));
    const std::string small = wb::chunk("VP8L", wb::vp8l(2, 2, wb::pattern(2, 2, 2, true)));
    const std::string other = wb::chunk("VP8L", wb::vp8l(w + 2, h, wb::pattern(w + 2, h, 1, true)));
    const std::string x = wb::vp8x(0x10, w, h), xa = wb::vp8x(0x12, w, h);
    const std::string exif = wb::chunk("EXIF", "II*\0\x08\0\0\0\0\0"), iccp = wb::chunk("ICCP", std::string(9, 'c'));
    const std::string xmp = wb::chunk("XMP ", "<x/>"), unknown = wb::chunk("ABCD", "12345");
    const std::string an = wb::anim(0);
    auto frame = [&](uint32_t fx, uint32_t fy, const std::string& inside) {
        return wb::anmf(fx, fy, 2, 2, 50, true, false, inside);
    };
    const std::pair<const char*, std::string> cases[] = {
        {"simple", wb::riff(img)},
        {"simple_then_unknown", wb::riff(img + unknown)},
        {"simple_then_exif", wb::riff(img + exif)},
        {"unknown_first", wb::riff(unknown + img)},
        {"extended", wb::riff(x + img)},
        {"extended_all", wb::riff(x + iccp + img + exif + xmp)},
        {"extended_unknown_before", wb::riff(x + unknown + img)},
        {"extended_unknown_after", wb::riff(x + img + unknown)},
        {"extended_exif_first", wb::riff(x + exif + iccp + img)},
        {"extended_iccp_after", wb::riff(x + img + iccp)},
        {"extended_other_size", wb::riff(x + other)},
        {"extended_two_images", wb::riff(x + img + img)},
        {"extended_no_image", wb::riff(x)},
        {"two_vp8x", wb::riff(x + x + img)},
        {"vp8x_longer", wb::riff(wb::chunk("VP8X", std::string{0x10, 0, 0, 0} + wb::le24(w - 1) + wb::le24(h - 1) + "ab") + img)},
        {"vp8x_shorter", wb::riff(wb::chunk("VP8X", std::string{0x10, 0, 0, 0} + wb::le24(w - 1) + wb::le24(h - 1)).substr(0, 16) + img)},
        {"alph_then_vp8l", wb::riff(x + wb::chunk("ALPH", "\0abc") + img)},
        {"anim_in_still", wb::riff(x + an + img)},
        {"animation", wb::riff(xa + an + frame(0, 0, small) + frame(4, 2, small))},
        {"animation_unknown_between", wb::riff(xa + an + frame(0, 0, small) + unknown + frame(4, 2, small))},
        {"animation_no_anim", wb::riff(xa + frame(0, 0, small))},
        {"animation_anmf_first", wb::riff(xa + frame(0, 0, small) + an)},
        {"animation_image_outside", wb::riff(xa + an + img)},
        {"animation_no_frames", wb::riff(xa + an)},
        {"animation_out_of_canvas", wb::riff(xa + an + frame(6, 0, small))},
        {"animation_frame_size", wb::riff(xa + an + wb::anmf(0, 0, 4, 2, 50, true, false, small))},
        {"animation_unknown_in_frame", wb::riff(xa + an + frame(0, 0, unknown + small))},
        {"animation_unknown_after_image", wb::riff(xa + an + frame(0, 0, small + unknown))},
        {"animation_alph_in_frame", wb::riff(xa + an + frame(0, 0, wb::chunk("ALPH", "\0abc") + small))},
        {"animation_two_images_in_frame", wb::riff(xa + an + frame(0, 0, small + small))},
        {"anmf_without_flag", wb::riff(x + an + frame(0, 0, small) + img)},
        {"anmf_without_flag_only", wb::riff(x + an + frame(0, 0, small))},
        {"exif_before_anmf", wb::riff(xa + an + exif + frame(0, 0, small))},
        {"simple_two_images", wb::riff(img + img)},
        {"simple_then_vp8x", wb::riff(img + x)},
        {"simple_then_alph", wb::riff(img + wb::chunk("ALPH", "\0abc"))},
        // after the simple format's image: chunks up to the first not ALPH
        {"simple_unknown_then_short_header", wb::riff(img + unknown + "ab")},
        {"simple_short_header", wb::riff(img + "ab")},
        {"simple_alph_then_short_header", wb::riff(img + wb::chunk("ALPH", "\0abc") + "ab")},
        {"simple_alph_then_vp8l", wb::riff(img + wb::chunk("ALPH", "\0abc") + img)},
        {"simple_alph_then_unknown_past_riff", wb::riff(img + wb::chunk("ALPH", "\0abc") + unknown.substr(0, 8))},
        {"simple_two_alph_then_short_header", wb::riff(img + wb::chunk("ALPH", "\0abc") + wb::chunk("ALPH", "\0abc") + "ab")},
        {"simple_unknown_past_riff", wb::riff(img + unknown.substr(0, 8))},
        {"simple_image_then_short_header", wb::riff(img + img + "ab")},
        {"animation_anmf_size_past_canvas", wb::riff(xa + an + wb::anmf(4, 2, 100, 100, 50, true, false, small))},
        {"animation_image_past_canvas", wb::riff(xa + an + wb::anmf(4, 2, 2, 2, 50, true, false, other))},
        {"animation_alph_after_image", wb::riff(xa + an + frame(0, 0, small + wb::chunk("ALPH", "\0abc")))},
        {"animation_short_anmf", wb::riff(xa + an + wb::chunk("ANMF", std::string(15, '\0')))},
        {"animation_anim_short", wb::riff(xa + wb::chunk("ANIM", "abcde") + frame(0, 0, small))},
        {"animation_anim_shorter", wb::riff(xa + wb::chunk("ANIM", "abcd") + frame(0, 0, small))},
        {"animation_anim_longer", wb::riff(xa + wb::chunk("ANIM", "abcdefgh") + frame(0, 0, small))},
        {"animation_anmf_17", wb::riff(xa + an + wb::chunk("ANMF", std::string(16, '\0') + "x"))},
        {"animation_second_anim", wb::riff(xa + an + frame(0, 0, small) + an + frame(2, 0, small))},
        {"animation_vp8x_later", wb::riff(xa + an + frame(0, 0, small) + x)},
        {"animation_iccp_between", wb::riff(xa + an + frame(0, 0, small) + iccp + frame(2, 0, small))},
        // a frame's chunks as the demuxer takes them: ALPH, the image, and
        // at the first other chunk the frame ends, the rest read at the top
        {"frame_then_unknown_first_frame", wb::riff(xa + an + frame(0, 0, small) + frame(0, 0, unknown + small))},
        {"frame_then_unknown_only_frame", wb::riff(xa + an + frame(0, 0, small) + frame(0, 0, unknown))},
        {"unknown_first_frame_then_frame", wb::riff(xa + an + frame(0, 0, unknown + small) + frame(0, 0, small))},
        {"frame_then_alph_first_frame", wb::riff(xa + an + frame(0, 0, small) + frame(0, 0, wb::chunk("ALPH", "\0abc") + small))},
        {"frame_then_alph_only_frame", wb::riff(xa + an + frame(0, 0, small) + frame(0, 0, wb::chunk("ALPH", "\0abc")))},
        {"frame_then_empty_frame", wb::riff(xa + an + frame(0, 0, small) + wb::chunk("ANMF", std::string(16, '\0')))},
        {"empty_frame_then_frame", wb::riff(xa + an + wb::chunk("ANMF", std::string(16, '\0')) + frame(0, 0, small))},
        {"frame_then_image_then_unknown", wb::riff(xa + an + frame(0, 0, small) + frame(0, 0, small + unknown))},
        {"frame_then_image_then_iccp", wb::riff(xa + an + frame(0, 0, small) + frame(0, 0, small + iccp))},
        {"still_alph_unknown_image", wb::riff(x + wb::chunk("ALPH", "\0abc") + unknown + img)},
        {"frame_image_short", wb::riff(xa + an + frame(0, 0, small) + frame(0, 0, wb::chunk("VP8L", wb::vp8l(2, 2, wb::pattern(2, 2, 2, true)).substr(0, 5))))},
        {"frame_image_bad_header", wb::riff(xa + an + frame(0, 0, small) + frame(0, 0, wb::chunk("VP8L", std::string("\x2e\x01\x40\x00\x00zzzz", 9))))},
        {"frame_image_of_3", wb::riff(xa + an + frame(0, 0, small) + frame(0, 0, wb::chunk("VP8L", "\x2f\x01\x40")))},
        {"anmf_not_animated_unknown_first", wb::riff(x + an + frame(0, 0, unknown + small) + img)},
    };
    for (const auto& [name, data] : cases) {
        EXPECT_EQ(acceptance(name, data), "");
        // the RIFF size larger than the data, smaller (bytes after it), odd padding cut
        std::string longer = data;
        longer[4] = char(uint8_t(longer[4]) + 2);
        EXPECT_EQ(acceptance(std::string(name) + "_riff_longer", longer), "");
        EXPECT_EQ(acceptance(std::string(name) + "_trailing", data + "xyz"), "");
    }
    // a last chunk of odd size: its padding byte counted in the RIFF size and there, or not there
    const std::string odd = wb::chunk("XMP ", "abc");
    const std::string with_pad = wb::riff(x + img + odd);
    EXPECT_EQ(acceptance("odd_padded", with_pad), "");
    std::string no_pad = with_pad.substr(0, with_pad.size() - 1);
    no_pad[4] = char(uint8_t(no_pad[4]) - 1);
    EXPECT_EQ(acceptance("odd_unpadded", no_pad), "");
}

TEST(CodecWebp_Tests, TheNativeFormatAndTheFormatAskedFor) {
    const auto px = wb::pattern(5, 3, 1, true);
    const std::string with_alpha = simple(5, 3, px, {}, true);
    const std::string without = simple(5, 3, px, {}, false);
    EXPECT_EQ(codec::webp::decode(bytes(with_alpha))->format(), pixel_format::rgba8);
    EXPECT_EQ(codec::webp::decode(bytes(without))->format(), pixel_format::rgb8);
    // VP8X's flag rules over VP8L's bit
    EXPECT_EQ(codec::webp::decode(bytes(wb::riff(wb::vp8x(0, 5, 3) + wb::chunk("VP8L", wb::vp8l(5, 3, px)))))->format(), pixel_format::rgb8);
    EXPECT_EQ(codec::webp::decode(bytes(wb::riff(wb::vp8x(0x10, 5, 3) + wb::chunk("VP8L", wb::vp8l(5, 3, px, {}, false)))))->format(),
              pixel_format::rgba8);
    codec::image native = *codec::webp::decode(bytes(with_alpha));
    for (int k = 0; k < 9; ++k) {
        const auto want = static_cast<pixel_format>(k);
        codec::image asked = *codec::decode(bytes(with_alpha), {.want = want});
        codec::image converted = native.convert(want);
        ASSERT_EQ(asked.format(), want);
        EXPECT_TRUE(rgba_of(asked) == rgba_of(converted)) << k;
    }
    EXPECT_EQ(code_of(codec::decode(bytes(with_alpha), {.want = static_cast<pixel_format>(40)})), codec::errc::invalid_argument);
}

TEST(CodecWebp_Tests, TheOptionsOfEveryDecoding) {
    const std::string file = simple(4, 2, wb::pattern(4, 2, 5, true));
    codec::decode_options o{.limits = {.max_pixels = 1000}};
    EXPECT_EQ(codec::decode(bytes(file), o)->format(), pixel_format::rgba8);
    EXPECT_EQ(codec::webp::decode(bytes(file), {.want = pixel_format::gray8})->format(), pixel_format::gray8);
    o.limits.max_pixels = 7;
    EXPECT_EQ(code_of(codec::decode(bytes(file), o)), codec::errc::too_large);
    EXPECT_EQ(code_of(codec::webp::decode(bytes(file), o)), codec::errc::too_large);
}

TEST(CodecWebp_Tests, Metadata) {
    // TIFF, little-endian, IFD0 of one entry: orientation 6
    const std::string tiff = std::string("II*\0\x08\0\0\0\x01\0\x12\x01\x03\0\x01\0\0\0\x06\0\0\0\0\0\0\0", 26);
    const std::string icc(300, '\x42');
    const auto img = wb::chunk("VP8L", wb::vp8l(3, 3, wb::pattern(3, 3, 1, false), {}, false));
    const std::string file = wb::riff(wb::vp8x(0x28, 3, 3) + wb::chunk("ICCP", icc) + img + wb::chunk("EXIF", std::string("Exif\0\0", 6) + tiff));
    codec::image im = *codec::webp::decode(bytes(file));
    EXPECT_EQ(im.orientation(), 6);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(im.exif().data()), im.exif().size()), tiff);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(im.icc().data()), im.icc().size()), icc);
    codec::image bare = *codec::decode(bytes(file), {.metadata = false});
    EXPECT_TRUE(bare.exif().empty());
    EXPECT_TRUE(bare.icc().empty());
    EXPECT_EQ(bare.orientation(), 1);
    EXPECT_EQ(code_of(codec::decode(bytes(file), {.limits = {.max_metadata = 100}})), codec::errc::too_large);
    // from a stream too
    pieces p{&file, 7};
    codec::image streamed = *codec::webp::decode(io::reader(p));
    EXPECT_EQ(streamed.orientation(), 6);
    EXPECT_EQ(streamed.icc().size(), icc.size());
}

namespace {
    // A normal prefix code of the lengths, written through a code of code
    // lengths giving 0..15 four bits each; and the canonical codes of the
    // lengths, to write symbols with
    void put_lengths(wb::BitWriter& w, const std::vector<uint8_t>& lengths) {
        static const unsigned order[19] = {17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        w.put(0, 1);
        w.put(19 - 4, 4);
        for (unsigned i = 0; i < 19; ++i) {
            w.put(order[i] <= 15 ? 4 : 0, 3);
        }
        w.put(0, 1);
        for (uint8_t l : lengths) {
            w.put_code(l, 4);
        }
    }

    std::vector<uint32_t> canonical(const std::vector<uint8_t>& lengths) {
        unsigned count[16] = {};
        for (uint8_t l : lengths) {
            ++count[l];
        }
        count[0] = 0;
        uint32_t next[16] = {}, code = 0;
        for (unsigned l = 1; l < 16; ++l) {
            code = (code + count[l - 1]) << 1;
            next[l] = code;
        }
        std::vector<uint32_t> codes(lengths.size());
        for (size_t s = 0; s < lengths.size(); ++s) {
            if (lengths[s]) {
                codes[s] = next[lengths[s]]++;
            }
        }
        return codes;
    }

    std::string header(uint32_t w, uint32_t h) {
        wb::BitWriter b;
        b.put(0x2F, 8);
        b.put(w - 1, 14);
        b.put(h - 1, 14);
        b.put(1, 1);
        b.put(0, 3);
        return b.bytes();
    }
}

TEST(CodecWebp_Tests, Negatives) {
    const auto px = wb::pattern(5, 4, 1, true);
    const std::string good = simple(5, 4, px);
    ASSERT_TRUE(codec::webp::decode(bytes(good)));
    // cut anywhere: an error, never a crash; inside the RIFF size: the data ends
    for (size_t n = 0; n < good.size(); ++n) {
        auto r = codec::webp::decode(bytes(good.substr(0, n)));
        ASSERT_FALSE(r) << n;
        if (n >= 12) {
            EXPECT_EQ(r.error().code(), codec::errc::unexpected_end) << n;
        }
    }
    // not RIFF WEBP
    std::string bad = good;
    bad[3] = 'X';
    EXPECT_EQ(code_of(codec::webp::decode(bytes(bad))), codec::errc::corrupt);
    // a VP8L signature or version that is not
    bad = good;
    bad[20] = 0x2E;
    EXPECT_EQ(code_of(codec::webp::decode(bytes(bad))), codec::errc::corrupt);
    bad = good;
    bad[24] = char(uint8_t(bad[24]) | 0x20);
    EXPECT_EQ(code_of(codec::webp::decode(bytes(bad))), codec::errc::corrupt);
    // a transform twice
    EXPECT_EQ(code_of(codec::webp::decode(bytes(simple(5, 4, px, {{2, 2, {}}, {2, 2, {}}})))), codec::errc::corrupt);
    // color cache bits of 0 and 12
    for (unsigned bits : {0u, 12u}) {
        wb::BitWriter w;
        w.put(0, 1);       // no transform
        w.put(1, 1);       // a color cache
        w.put(bits, 4);
        const std::string file = wb::riff(wb::chunk("VP8L", header(5, 4) + w.bytes() + std::string(8, '\0')));
        EXPECT_EQ(code_of(codec::webp::decode(bytes(file))), codec::errc::corrupt) << bits;
    }
    // a code that is not a whole tree: lengths 1 and 2 alone
    {
        wb::BitWriter w;
        w.put(0, 1);
        w.put(0, 1);
        w.put(0, 1);
        std::vector<uint8_t> lengths(280, 0);
        lengths[0] = 1;
        lengths[1] = 2;
        put_lengths(w, lengths);
        const std::string file = wb::riff(wb::chunk("VP8L", header(5, 4) + w.bytes() + std::string(8, '\0')));
        EXPECT_EQ(code_of(codec::webp::decode(bytes(file))), codec::errc::corrupt);
    }
    // a back reference before the first pixel: green's code of literal 0
    // and length 1 (symbol 256), the first symbol the reference
    {
        wb::BitWriter w;
        w.put(0, 1);
        w.put(0, 1);
        w.put(0, 1);
        std::vector<uint8_t> lengths(280, 0);
        lengths[0] = 1;
        lengths[256] = 1;
        put_lengths(w, lengths);
        wb::put_single(w, 0);
        wb::put_single(w, 0);
        wb::put_single(w, 0);
        wb::put_single(w, 0);
        const auto codes = canonical(lengths);
        w.put_code(codes[256], 1);
        const std::string file = wb::riff(wb::chunk("VP8L", header(2, 1) + w.bytes() + std::string(8, '\0')));
        EXPECT_EQ(code_of(codec::webp::decode(bytes(file))), codec::errc::corrupt);
        if (!c_oracle().empty()) {
            EXPECT_FALSE(run_oracle(c_oracle(), "webp", write_file("backref.webp", file)));
        }
    }
    // the limits: pixels past max_pixels, a canvas of 2^32 pixels
    EXPECT_EQ(code_of(codec::decode(bytes(good), {.limits = {.max_pixels = 19}})), codec::errc::too_large);
    EXPECT_TRUE(codec::decode(bytes(good), {.limits = {.max_pixels = 20}}));
    const std::string huge = wb::riff(wb::vp8x(0x10, 1u << 24, 1u << 24) + wb::chunk("VP8L", wb::vp8l(5, 4, px)));
    EXPECT_EQ(code_of(codec::webp::decode(bytes(huge))), codec::errc::corrupt);
    const std::string big = wb::riff(wb::vp8x(0x10, 20000, 20000) + wb::chunk("VP8L", wb::vp8l(5, 4, px)));
    EXPECT_EQ(code_of(codec::webp::decode(bytes(big))), codec::errc::too_large);
    // not an animation format for frames
    EXPECT_FALSE(codec::decode_frames(bytes(std::string("\x89PNG\r\n\x1a\n", 8))));
}

TEST(CodecWebp_Tests, AStreamInPiecesIsMemory) {
    std::vector<std::string> files;
    for (const auto& path : test_files()) {
        const std::string data = read_file(path);
        if (!has_vp8(data)) {
            files.push_back(data);
        }
    }
    std::vector<Frame> frames = {{0, 0, 24, 17, false, true, true, 1, 70}, {4, 2, 9, 7, true, false, true, 2, 20}};
    files.push_back(animation(24, 17, frames, 3));
    size_t step = 1;
    for (const auto& data : files) {
        auto whole = codec::decode(bytes(data), {.want = pixel_format::rgba8});
        ASSERT_TRUE(whole);
        pieces p{&data, step};
        auto streamed = codec::decode(io::reader(p), {.want = pixel_format::rgba8});
        ASSERT_TRUE(streamed) << streamed.error().message().view();
        EXPECT_TRUE(rgba_of(*whole) == rgba_of(*streamed));
        pieces q{&data, step + 5};
        const Read a = read_all(codec::webp::frames(bytes(data)));
        const Read b = read_all(codec::webp::frames(io::reader(q)));
        EXPECT_EQ(a.failure, "");
        EXPECT_EQ(b.failure, "");
        EXPECT_TRUE(a.canvases == b.canvases);
        EXPECT_EQ(a.plays, b.plays);
        pieces r{&data, 999};
        EXPECT_TRUE(codec::decode_frames(io::reader(r)));
        step = step % 97 + 4;
    }
}

TEST(CodecWebp_Tests, FramesShareEndAndFail) {
    std::vector<Frame> frames = {{0, 0, 8, 6, false, false, false, 1, 10}, {2, 2, 4, 4, true, true, true, 2, 20}, {0, 0, 8, 6, true, false, true, 3, 30}};
    const std::string data = animation(8, 6, frames, 0);
    codec::frames clip = *codec::webp::frames(bytes(data));
    EXPECT_EQ(clip.width(), 8u);
    EXPECT_EQ(clip.height(), 6u);
    codec::frames copy = clip;
    ASSERT_TRUE(*clip.next());
    EXPECT_EQ(clip.loop_count(), 0u);
    ASSERT_TRUE(*copy.next());
    ASSERT_TRUE(*clip.next());
    for (int i = 0; i < 3; ++i) {
        auto n = clip.next();
        ASSERT_TRUE(n);
        EXPECT_FALSE(*n);
    }
    // a still image is one frame; decode of an animation its first frame
    const Read still = read_all(codec::webp::frames(bytes(simple(3, 2, wb::pattern(3, 2, 1, true)))));
    EXPECT_EQ(still.canvases.size(), 1u);
    EXPECT_EQ(still.plays, 1u);
    codec::image first = *codec::webp::decode(bytes(data));
    const Read all = read_all(codec::webp::frames(bytes(data)));
    EXPECT_TRUE(rgba_of(first) == all.canvases[0]);
    // a failure is the same on every call after it
    const std::string cut = data.substr(0, data.size() - 3);
    std::string fixed = cut;
    const uint32_t riff = uint32_t(fixed.size() - 8);
    std::memcpy(fixed.data() + 4, &riff, 4);
    codec::frames broken = *codec::webp::frames(bytes(fixed));
    ASSERT_TRUE(*broken.next());
    ASSERT_TRUE(*broken.next());
    auto e1 = broken.next();
    ASSERT_FALSE(e1);
    auto e2 = broken.next();
    ASSERT_FALSE(e2);
    EXPECT_EQ(e2.error(), e1.error());
}

TEST(CodecWebp_Tests, TheKernelsAgainstOnePixelAtATime) {
    // each kernel over random words and every length to 40, against its
    // formula one pixel at a time (in a SIMD build: the vector loop and its
    // tail; in tests_codec_portable: the plain loop)
    namespace k = codec::detail::vp8l;
    std::mt19937 rng(7);
    for (size_t n = 0; n <= 40; ++n) {
        std::vector<uint32_t> src(n + 2);
        for (auto& v : src) {
            v = rng();
        }
        std::vector<uint8_t> rgba(n * 4), rgb(n * 3);
        k::argb_to_rgba(src.data(), rgba.data(), n);
        k::argb_to_rgb(src.data(), rgb.data(), n);
        for (size_t i = 0; i < n; ++i) {
            const uint32_t v = src[i];
            const uint8_t want[4] = {uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v), uint8_t(v >> 24)};
            ASSERT_EQ(std::memcmp(rgba.data() + i * 4, want, 4), 0) << n;
            ASSERT_EQ(std::memcmp(rgb.data() + i * 3, want, 3), 0) << n;
        }
        std::vector<uint32_t> p = src;
        k::add_green(p.data(), n);
        for (size_t i = 0; i < n; ++i) {
            const uint32_t v = src[i], g = (v >> 8) & 0xff;
            const uint32_t want = (v & 0xff00ff00u) | (((v >> 16) + g) & 0xff) << 16 | ((v + g) & 0xff);
            ASSERT_EQ(p[i], want) << n;
        }
        const uint32_t element = rng();
        p = src;
        k::color_transform(p.data(), n, element);
        for (size_t i = 0; i < n; ++i) {
            const uint32_t v = src[i];
            const int8_t g2r = int8_t(element), g2b = int8_t(element >> 8), r2b = int8_t(element >> 16);
            const int8_t green = int8_t(v >> 8);
            const int red = int((v >> 16) & 0xff) + ((g2r * green) >> 5);
            int blue = int(v & 0xff) + ((g2b * green) >> 5);
            blue += (r2b * int8_t(red & 0xff)) >> 5;
            ASSERT_EQ(p[i], (v & 0xff00ff00u) | uint32_t(red & 0xff) << 16 | uint32_t(blue & 0xff)) << n;
        }
        for (unsigned mode : {0u, 2u, 3u, 4u, 8u, 9u}) {
            // the row above with a word on each side: TL of the first, TR of the last
            std::vector<uint32_t> top(n + 2);
            for (auto& v : top) {
                v = rng();
            }
            p = src;
            k::predict_top(mode, p.data(), top.data() + 1, n);
            for (size_t i = 0; i < n; ++i) {
                const uint32_t T = top[i + 1], TL = top[i], TR = top[i + 2];
                const uint32_t pred = mode == 2   ? T
                                      : mode == 3 ? TR
                                      : mode == 4 ? TL
                                      : mode == 8 ? k::average2(TL, T)
                                      : mode == 9 ? k::average2(T, TR)
                                                  : 0xff000000u;
                ASSERT_EQ(p[i], k::add_pixels(src[i], pred)) << "mode " << mode << " n " << n;
            }
        }
    }
    // the per-channel helpers themselves
    EXPECT_EQ(k::add_pixels(0xff80017fu, 0x0180ff81u), 0x00000000u);
    EXPECT_EQ(k::average2(0xff000102u, 0x01ff0304u), 0x807f0203u);
}

namespace {
    // A chunk's payload out of a file ("" when it has none), the first of the tag
    std::string chunk_of(const std::string& file, const char* tag) {
        for (size_t at = 12; at + 8 <= file.size();) {
            uint32_t n;
            std::memcpy(&n, file.data() + at + 4, 4);
            if (std::memcmp(file.data() + at, tag, 4) == 0) {
                return file.substr(at + 8, n);
            }
            at += 8 + size_t(n) + (n & 1);
        }
        return "";
    }

    uint32_t term(int x, int c) {
        return uint32_t((x * c) >> 8);
    }
}

TEST(CodecWebp_Tests, TheLossyKernelsAgainstTheirFormulas) {
    // the upsampling and the colors over random rows and every width to 70,
    // against their definitions written out here (in a SIMD build: the
    // vector loops and their tails; in tests_codec_portable: the plain loops)
    namespace k = codec::detail::vp8;
    std::mt19937 rng(11);
    for (uint32_t w = 1; w <= 70; ++w) {
        const uint32_t cw = (w + 1) / 2;
        for (int rep = 0; rep < 20; ++rep) {
            std::vector<uint8_t> near(cw), far(cw), out(w);
            for (uint32_t i = 0; i < cw; ++i) {
                near[i] = uint8_t(rng());
                far[i] = uint8_t(rng());
            }
            std::vector<uint16_t> blend(cw + 2);
            k::upsample_row(near.data(), far.data(), out.data(), w, blend.data());
            for (uint32_t x = 0; x < w; ++x) {
                const int i = int(x / 2);
                const int j = x % 2 ? std::min(i + 1, int(cw) - 1) : std::max(i - 1, 0);
                const int want = (9 * near[i] + 3 * near[j] + 3 * far[i] + far[j] + 8) >> 4;
                ASSERT_EQ(out[x], want) << "w " << w << " x " << x;
            }
            std::vector<uint8_t> y(w), u(w), v(w), a(w);
            for (uint32_t i = 0; i < w; ++i) {
                y[i] = uint8_t(rng());
                u[i] = uint8_t(rng());
                v[i] = uint8_t(rng());
                a[i] = uint8_t(rng());
            }
            std::vector<uint32_t> argb(w);
            k::yuv_to_argb(y.data(), u.data(), v.data(), rep % 2 ? a.data() : nullptr, argb.data(), w);
            auto channel = [](int s) { return uint32_t(s < 0 ? 0 : std::min(s >> 6, 255)); };
            for (uint32_t i = 0; i < w; ++i) {
                const int ty = int(term(y[i], k::KY));
                const uint32_t r = channel(ty + int(term(v[i], k::KRV)) + k::OR);
                const uint32_t g = channel(ty - int(term(u[i], k::KGU)) - int(term(v[i], k::KGV)) + k::OG);
                const uint32_t b = channel(ty + int(term(u[i], k::KBU)) + k::OB);
                const uint32_t alpha = rep % 2 ? a[i] : 255;
                ASSERT_EQ(argb[i], alpha << 24 | r << 16 | g << 8 | b) << "w " << w << " i " << i;
            }
        }
    }
}

TEST(CodecWebp_Tests, LossyInTheContainerAsLibwebpTakesIt) {
    if (c_oracle().empty()) {
        GTEST_SKIP() << "no libwebp";
    }
    CODEC_ORACLE(plain, TestData + "/test.webp");                 // VP8, 128x128
    CODEC_ORACLE(with_alpha, TestData + "/alpha_filter_1.webp");  // VP8X, ALPH, VP8
    const std::string vp8 = chunk_of(plain, "VP8 ");
    const std::string avp8 = chunk_of(with_alpha, "VP8 ");
    const std::string alph = chunk_of(with_alpha, "ALPH");
    ASSERT_FALSE(vp8.empty() || avp8.empty() || alph.empty());
    uint32_t aw = 0, ah = 0;
    ASSERT_TRUE(codec::detail::vp8::Decoder::size_of(reinterpret_cast<const uint8_t*>(avp8.data()), avp8.size(), aw, ah));
    const std::string unknown = wb::chunk("ABCD", "12345");
    auto tag_of = [&](const std::string& v, uint32_t set, uint32_t clear) {
        std::string out = v;
        uint32_t t = uint32_t(uint8_t(out[0])) | uint32_t(uint8_t(out[1])) << 8 | uint32_t(uint8_t(out[2])) << 16;
        t = (t & ~clear) | set;
        out[0] = char(t);
        out[1] = char(t >> 8);
        out[2] = char(t >> 16);
        return out;
    };
    std::string bad_start = vp8;
    bad_start[3] = char(0x9e);
    std::string zero_width = vp8;
    zero_width[6] = zero_width[7] = 0;
    const std::string small = wb::chunk("VP8L", wb::vp8l(2, 2, wb::pattern(2, 2, 2, true)));
    const std::string xa = wb::vp8x(0x12, 24, 20), an = wb::anim(0);
    const std::pair<const char*, std::string> cases[] = {
        {"lossy_simple", wb::riff(wb::chunk("VP8 ", vp8))},
        {"lossy_simple_alpha", wb::riff(wb::chunk("ALPH", alph) + wb::chunk("VP8 ", avp8))},
        {"lossy_simple_alpha_unknown", wb::riff(wb::chunk("ALPH", alph) + unknown + wb::chunk("VP8 ", avp8))},
        {"lossy_extended", wb::riff(wb::vp8x(0, 128, 128) + wb::chunk("VP8 ", vp8))},
        {"lossy_extended_alpha", wb::riff(wb::vp8x(0x10, aw, ah) + wb::chunk("ALPH", alph) + wb::chunk("VP8 ", avp8))},
        {"lossy_extended_alpha_unknown", wb::riff(wb::vp8x(0x10, aw, ah) + wb::chunk("ALPH", alph) + unknown + wb::chunk("VP8 ", avp8))},
        {"lossy_extended_two_alph", wb::riff(wb::vp8x(0x10, aw, ah) + wb::chunk("ALPH", alph) + wb::chunk("ALPH", alph) + wb::chunk("VP8 ", avp8))},
        {"lossy_extended_alph_after", wb::riff(wb::vp8x(0x10, aw, ah) + wb::chunk("VP8 ", avp8) + wb::chunk("ALPH", alph))},
        {"lossy_extended_no_flag", wb::riff(wb::vp8x(0, aw, ah) + wb::chunk("ALPH", alph) + wb::chunk("VP8 ", avp8))},
        {"lossy_animation", wb::riff(xa + an + wb::anmf(0, 0, aw, ah, 30, true, false, wb::chunk("ALPH", alph) + wb::chunk("VP8 ", avp8)) +
                                     wb::anmf(4, 2, 2, 2, 30, true, true, small) + wb::anmf(2, 2, aw, ah, 30, true, false, wb::chunk("VP8 ", avp8)))},
        {"lossy_animation_unknown_in_frame", wb::riff(xa + an + wb::anmf(0, 0, aw, ah, 30, true, false, wb::chunk("ALPH", alph) + unknown + wb::chunk("VP8 ", avp8)))},
        {"lossy_not_key", wb::riff(wb::chunk("VP8 ", tag_of(vp8, 1, 0)))},
        {"lossy_version_4", wb::riff(wb::chunk("VP8 ", tag_of(vp8, 4 << 1, 7 << 1)))},
        {"lossy_hidden", wb::riff(wb::chunk("VP8 ", tag_of(vp8, 0, 1 << 4)))},
        {"lossy_first_partition_past", wb::riff(wb::chunk("VP8 ", tag_of(vp8, 0x7ffffu << 5, 0x7ffffu << 5)))},
        {"lossy_bad_start", wb::riff(wb::chunk("VP8 ", bad_start))},
        {"lossy_zero_width", wb::riff(wb::chunk("VP8 ", zero_width))},
        {"lossy_short", wb::riff(wb::chunk("VP8 ", vp8.substr(0, 9)))},
        {"lossy_cut", wb::riff(wb::chunk("VP8 ", vp8.substr(0, vp8.size() / 2)))},
        {"lossy_cut_header", wb::riff(wb::chunk("VP8 ", vp8.substr(0, 40)))},
    };
    for (const auto& [name, data] : cases) {
        EXPECT_EQ(acceptance(name, data), "");
        std::string longer = data;
        longer[4] = char(uint8_t(longer[4]) + 2);
        EXPECT_EQ(acceptance(std::string(name) + "_riff_longer", longer), "");
        // a still image both take: WebPDecodeRGBA's pixels (it applies an
        // ALPH chunk whatever VP8X's alpha flag says; WebPAnimDecoder
        // leaves such an image opaque)
        auto ours = codec::decode(bytes(data), {.want = pixel_format::rgba8});
        auto theirs = run_oracle(c_oracle(), "webp", write_file(std::string(name) + ".webp", data));
        if (ours && theirs && std::string(name).find("animation") == std::string::npos) {
            EXPECT_TRUE(theirs->pixels == rgba_of(*ours)) << name;
        }
    }
}

// The differential fuzzer's finds. Taken or refused and decoded as libwebp
// does: chunks after the simple format's image up to the first not ALPH;
// the transforms' rows past 16 bits; a block of zero tokens to its end
// non-zero for the contexts and the filter
TEST(CodecWebp_Tests, TheFuzzersFindsAsLibwebpTakesThem) {
    if (c_oracle().empty()) {
        GTEST_SKIP() << "no libwebp";
    }
    const std::string here = std::string(__FILE__).substr(0, std::string(__FILE__).rfind('/'));
    for (const char* name : {"regress_simple_short_header_after_stop", "regress_lossy_transform_rows_past_16_bits",
                             "regress_lossy_zero_run_is_non_zero", "regress_anmf_area"}) {
        const std::string data = read_file(here + "/fuzz/seeds/webp_decode/" + name);
        ASSERT_FALSE(data.empty()) << name;
        EXPECT_EQ(acceptance(name, data), "");
        auto ours = codec::decode(bytes(data), {.want = pixel_format::rgba8});
        auto theirs = run_oracle(c_oracle(), "webp", write_file(std::string(name) + ".webp", data));
        EXPECT_EQ(bool(ours), bool(theirs)) << name;
        if (ours && theirs) {
            EXPECT_TRUE(theirs->pixels == rgba_of(*ours)) << name;
        }
    }
    // coefficients past what an encoder makes: taken as libwebp takes them,
    // their pixels the module's own (libwebp's paths and Go disagree
    // there), told by beyond_encoders, which the test data never trips
    // (yuv_of)
    {
        const std::string data = read_file(here + "/fuzz/seeds/webp_decode/regress_lossy_beyond_encoders");
        ASSERT_FALSE(data.empty());
        EXPECT_EQ(acceptance("regress_lossy_beyond_encoders", data), "");
        codec::detail::MemoryInput in(bytes(data));
        codec::decode_options o;
        codec::detail::WebpReader<codec::detail::MemoryInput> r(in, o);
        codec::detail::WebpFrame f;
        ASSERT_TRUE(r.start() && r.next(f) == codec::detail::WebpReader<codec::detail::MemoryInput>::Step::frame && f.lossy && r.decode(f));
        EXPECT_TRUE(r.lossy_decoder().beyond_encoders());
    }
}

// An ANMF chunk's own width and height bound nothing of its frame (the
// image's are the frame's) but their product: libwebp's demuxer refuses a
// frame that claims 2^32 pixels or more, and so do frames and decode here
// (the fuzzer's find, regress_anmf_area: 16711696 x 1640704 over a 16 x 16
// image). Each side of the bound, both ways it is reached; and a claim
// past the canvas that stays under it, which both take
TEST(CodecWebp_Tests, AnmfClaimedAreaAsLibwebpBoundsIt) {
    const std::vector<uint32_t> px = wb::pattern(16, 16, 3, false);
    const std::string image = wb::chunk("VP8L", wb::vp8l(16, 16, px));
    // the ANMF chunk of a frame at (0, 0) claiming w x h over the 16 x 16 image
    auto file = [&](uint32_t w, uint32_t h) {
        const std::string frame = wb::le24(0) + wb::le24(0) + wb::le24(w - 1) + wb::le24(h - 1) + wb::le24(100) + std::string(1, '\0') + image;
        return wb::riff(wb::vp8x(0x02, 24, 20) + wb::anim(0) + wb::chunk("ANMF", frame));
    };
    const struct {
        uint32_t w, h;
        bool taken;
    } cases[] = {
        {65536, 65535, true},  {65536, 65536, false},  {65537, 65536, false},
        {16777216, 255, true}, {16777216, 256, false}, {40, 30, true},
    };
    for (const auto& c : cases) {
        const std::string data = file(c.w, c.h);
        const Read ours = read_all(codec::webp::frames(bytes(data)));
        EXPECT_EQ(ours.failure.empty(), c.taken) << c.w << " x " << c.h << ": " << ours.failure;
        EXPECT_EQ(bool(codec::webp::decode(bytes(data))), c.taken) << c.w << " x " << c.h;
        if (!c_oracle().empty()) {
            EXPECT_EQ(acceptance("anmf_area_" + std::to_string(c.w) + "x" + std::to_string(c.h), data), "");
        }
    }
}

// The loop filter's level clamped once, after the deltas, as libwebp and
// Go clamp it: files made from test.webp with a frame header whose
// segment level is past 0..63 (60 + 30 with a reference delta of -40; an
// absolute -20 with +40; 10 - 30 with a B_PRED delta of +40). Clamping the
// segment's level first, as RFC 6386's reference decoder does, differs
// from libwebp in 3385 to 3539 bytes of the planes
TEST(CodecWebp_Tests, TheFilterLevelClampedOnceAsLibwebp) {
    const std::string here = std::string(__FILE__).substr(0, std::string(__FILE__).rfind('/'));
    for (const char* name : {"filter_level_past_63.webp", "filter_level_absolute_below_0.webp", "filter_level_below_0_b_pred.webp"}) {
        const std::string path = here + "/fuzz/seeds/webp_decode/" + name;
        const std::string data = read_file(path);
        ASSERT_FALSE(data.empty()) << name;
        const std::string planes = yuv_of(data);
        ASSERT_FALSE(planes.empty()) << name;
        if (!c_oracle().empty()) {
            auto theirs = run_oracle(c_oracle(), "webpyuv", path);
            ASSERT_TRUE(theirs) << name;
            EXPECT_TRUE(theirs->pixels == planes) << name << " against libwebp";
        }
        if (!go_webp_oracle().empty()) {
            auto theirs = run_oracle(go_webp_oracle(), "webpyuv", path);
            ASSERT_TRUE(theirs) << name;
            EXPECT_TRUE(theirs->pixels == planes) << name << " against Go";
        }
    }
}

// Data RFC 6386 or RFC 9649 make invalid and libwebp decodes anyway, what
// it reads past the end shown (the fuzzer's finds): refused, with the
// reason WebpDamage gives
TEST(CodecWebp_Tests, InvalidDataIsRefusedWithItsReason) {
    using Reader = codec::detail::WebpReader<codec::detail::MemoryInput>;
    using codec::detail::WebpDamage;
    const std::string here = std::string(__FILE__).substr(0, std::string(__FILE__).rfind('/'));
    auto damage_of = [](const std::string& data, bool& refused) {
        codec::detail::MemoryInput in(bytes(data));
        codec::decode_options o;
        Reader r(in, o);
        codec::detail::WebpFrame f;
        refused = !r.start();
        while (!refused && r.next(f) == Reader::Step::frame) {
            refused = !r.decode(f);
        }
        return r.damage();
    };
    const std::pair<const char*, WebpDamage> cases[] = {
        {"regress_alpha_bits_into_padding", WebpDamage::alpha_overrun},       // libwebp: into the padding byte
        {"regress_alpha_end_off_the_8bit_path", WebpDamage::alpha_overrun},   // libwebp refuses it too
        {"regress_lossy_value_past_the_range", WebpDamage::partition_start},  // a token partition starting 0xFF
    };
    for (const auto& [name, reason] : cases) {
        const std::string data = read_file(here + "/fuzz/seeds/webp_decode/" + name);
        ASSERT_FALSE(data.empty()) << name;
        EXPECT_FALSE(codec::webp::decode(bytes(data))) << name;
        bool refused = false;
        EXPECT_EQ(int(damage_of(data, refused)), int(reason)) << name;
        EXPECT_TRUE(refused) << name;
    }
    // a partition read past its end: a lossy chunk cut short in its last
    // partition; and the file whole, taken with no reason
    if (!c_oracle().empty()) {
        CODEC_ORACLE(plain, TestData + "/test.webp");
        const std::string vp8 = chunk_of(plain, "VP8 ");
        ASSERT_FALSE(vp8.empty());
        bool refused = false;
        EXPECT_EQ(int(damage_of(wb::riff(wb::chunk("VP8 ", vp8.substr(0, vp8.size() - 200))), refused)), int(WebpDamage::partition_overrun));
        EXPECT_TRUE(refused);
        EXPECT_EQ(int(damage_of(wb::riff(wb::chunk("VP8 ", vp8)), refused)), int(WebpDamage::none));
        EXPECT_FALSE(refused);
    }
}

TEST(CodecWebp_Tests, AlphaOfEveryFilterAndItsHeader) {
    if (c_oracle().empty()) {
        GTEST_SKIP() << "no libwebp";
    }
    CODEC_ORACLE(with_alpha, TestData + "/alpha_filter_1.webp");
    const std::string avp8 = chunk_of(with_alpha, "VP8 ");
    uint32_t w = 0, h = 0;
    ASSERT_TRUE(codec::detail::vp8::Decoder::size_of(reinterpret_cast<const uint8_t*>(avp8.data()), avp8.size(), w, h));
    std::mt19937 rng(5);
    std::string plane(size_t(w) * h, '\0');
    for (auto& c : plane) {
        c = char(rng() & 0x7f);
    }
    // raw alpha under each filter, and in a VP8L stream (headerless: its transforms first)
    for (unsigned filter = 0; filter < 4; ++filter) {
        for (unsigned method = 0; method < 2; ++method) {
            std::string payload(1, char(filter << 2 | method));
            if (method == 0) {
                payload += plane;
            } else {
                std::vector<uint32_t> green(plane.size());
                for (size_t i = 0; i < plane.size(); ++i) {
                    green[i] = uint32_t(uint8_t(plane[i])) << 8;
                }
                // a stream without the 5-byte header: the header builder's output from byte 5
                payload += wb::vp8l(w, h, green).substr(5);
            }
            const std::string file = wb::riff(wb::vp8x(0x10, w, h) + wb::chunk("ALPH", payload) + wb::chunk("VP8 ", avp8));
            auto ours = codec::decode(bytes(file), {.want = pixel_format::rgba8});
            auto theirs = run_oracle(c_oracle(), "webp", write_file("alpha.webp", file));
            ASSERT_EQ(bool(ours), bool(theirs)) << "filter " << filter << " method " << method;
            if (ours) {
                EXPECT_TRUE(theirs->pixels == rgba_of(*ours)) << "filter " << filter << " method " << method;
            }
        }
    }
    // the header's values: compression 2 and 3, preprocessing 2 and 3, the reserved bits; raw alpha too short
    for (unsigned head : {0x02u, 0x03u, 0x20u, 0x30u, 0x40u, 0x80u, 0x10u}) {
        const std::string file = wb::riff(wb::vp8x(0x10, w, h) + wb::chunk("ALPH", std::string(1, char(head)) + plane) + wb::chunk("VP8 ", avp8));
        EXPECT_EQ(acceptance("alpha_header_" + std::to_string(head), file), "") << head;
    }
    const std::string cut = wb::riff(wb::vp8x(0x10, w, h) + wb::chunk("ALPH", std::string(1, '\0') + plane.substr(0, plane.size() - 1)) + wb::chunk("VP8 ", avp8));
    EXPECT_EQ(acceptance("alpha_raw_short", cut), "");
}

TEST(CodecWebp_Tests, LossyNativeFormat) {
    CODEC_ORACLE(plain, TestData + "/test.webp");
    CODEC_ORACLE(with_alpha, TestData + "/alpha_filter_1.webp");
    EXPECT_EQ(codec::webp::decode(bytes(plain))->format(), pixel_format::rgb8);
    EXPECT_EQ(codec::webp::decode(bytes(with_alpha))->format(), pixel_format::rgba8);
    // rgb8 is rgba8 without its alpha
    codec::image rgb = *codec::webp::decode(bytes(plain));
    codec::image rgba = *codec::decode(bytes(plain), {.want = pixel_format::rgba8});
    EXPECT_TRUE(rgba_of(rgb.convert(pixel_format::rgba8)) == rgba_of(rgba));
    // from a stream in pieces, the same
    for (const std::string* data : {&plain, &with_alpha}) {
        pieces p{data, 13};
        auto streamed = codec::decode(io::reader(p), {.want = pixel_format::rgba8});
        ASSERT_TRUE(streamed) << streamed.error().message().view();
        EXPECT_TRUE(rgba_of(*streamed) == rgba_of(*codec::decode(bytes(*data), {.want = pixel_format::rgba8})));
    }
}
