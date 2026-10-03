//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec::heif: HEIF, HEIC and AVIF through the system's codec (ImageIO on
// macOS). The files are made while the test runs, by sips and by ImageIO
// (tools/codec_oracle_heif.c), from PngSuite's images; the oracle is
// ImageIO's own reading of each file drawn by CoreGraphics, so what is held
// is the module's wrapping: size, depth, alpha, orientation, the profile,
// the conversions, the limits, the streams and the errors.
#include <gtest/gtest.h>

#include "common.h"
#include "oracle.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;

namespace {
    std::string read_file(const std::string& path) {
        std::ifstream is(path, std::ios::binary);
        std::stringstream ss;
        ss << is.rdbuf();
        return ss.str();
    }

    std::string write_file(const std::string& name, const std::string& data) {
        const std::string path = (scratch_path("sgcl_codec_heif") / name).string();
        std::ofstream(path, std::ios::binary).write(data.data(), std::streamsize(data.size()));
        return path;
    }

    std::string temp(const std::string& name) {
        return (scratch_path("sgcl_codec_heif") / name).string();
    }

    // A PngSuite image as HEIC made by sips; "" when sips cannot
    std::string sips_heic(const std::string& png) {
        const std::string src = oracle_path("pngsuite/" + png);
        if (!std::filesystem::exists(src) || std::system("command -v sips > /dev/null 2>&1") != 0) {
            return "";
        }
        const std::string out = temp(png + ".heic");
        const std::string cmd = "sips -s format heic '" + src + "' --out '" + out + "' > /dev/null 2>&1";
        if (std::system(cmd.c_str()) != 0) {
            return "";
        }
        return read_file(out);
    }

    std::string text_of(const std::string& cmd) {
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return "";
        }
        std::string out;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    }

    // A value of the oracle's meta output ("orientation 6" …)
    long meta_of(const std::string& meta, const std::string& key) {
        const auto at = meta.find(key + " ");
        return at == std::string::npos ? -1 : std::atol(meta.c_str() + at + key.size() + 1);
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

    struct collect {
        std::string bytes;

        expected<size_t, io::error> write(const slice<const std::byte>& b) {
            bytes.append(reinterpret_cast<const char*>(b.data()), b.size());
            return b.size();
        }
    };

    bool has_alpha(pixel_format f) {
        return f == pixel_format::gray_alpha8 || f == pixel_format::gray_alpha16 || f == pixel_format::rgba8 || f == pixel_format::rgba16;
    }

    bool is_deep(pixel_format f) {
        return f == pixel_format::gray16 || f == pixel_format::gray_alpha16 || f == pixel_format::rgb16 || f == pixel_format::rgba16;
    }

    // The largest difference of a channel between the module's image in the
    // oracle's form (premultiplied as the oracle's bitmap holds it) and the
    // oracle's pixels
    long largest_difference(const codec::image& im, const oracle_image& theirs) {
        const int depth = theirs.depth;
        std::string ours = oracle_form(im, depth);
        const size_t bpc = depth == 16 ? 2 : 1, max = depth == 16 ? 65535 : 255;
        auto at = [&](const std::string& s, size_t i) -> long {
            return bpc == 2 ? long(uint8_t(s[2 * i])) << 8 | uint8_t(s[2 * i + 1]) : long(uint8_t(s[i]));
        };
        if (ours.size() != theirs.pixels.size()) {
            return -1;
        }
        long worst = 0;
        const size_t pixels = ours.size() / (4 * bpc);
        for (size_t p = 0; p < pixels; ++p) {
            const long a = at(ours, 4 * p + 3);
            for (size_t c = 0; c < 4; ++c) {
                long v = at(ours, 4 * p + c);
                if (c < 3) {
                    v = (v * a + long(max) / 2) / long(max);
                }
                worst = std::max(worst, std::labs(v - at(theirs.pixels, 4 * p + c)));
            }
        }
        return worst;
    }

    // An image of smooth gradients (what a lossy codec keeps), in format f
    codec::image gradient(uint32_t w, uint32_t h, pixel_format f) {
        codec::image rgba(w, h, pixel_format::rgba16);
        auto px = rgba.pixels();
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                const uint16_t v[4] = {uint16_t(x * 65535 / (w - 1)), uint16_t(y * 65535 / (h - 1)), uint16_t((x + y) * 65535 / (w + h - 2)),
                                       uint16_t(40000 + (x * 20000) / w)};
                std::memcpy(px.data() + (size_t(y) * w + x) * 8, v, 8);
            }
        }
        return rgba.convert(f);
    }

    // PSNR of two images of one format and size, over every channel
    double psnr(const codec::image& a, const codec::image& b) {
        const codec::image x = a.convert(pixel_format::rgba16), y = b.convert(pixel_format::rgba16);
        auto p = x.pixels(), q = y.pixels();
        double sum = 0;
        const size_t n = p.size() / 2;
        for (size_t i = 0; i < n; ++i) {
            uint16_t u, v;
            std::memcpy(&u, p.data() + 2 * i, 2);
            std::memcpy(&v, q.data() + 2 * i, 2);
            const double d = (double(u) - double(v)) / 65535.0;
            sum += d * d;
        }
        const double mse = sum / double(n);
        return mse == 0 ? 99.0 : 10.0 * std::log10(1.0 / mse);
    }

    codec::errc code_of(const expected<codec::image, codec::error>& r) {
        return r ? codec::errc{} : r.error().code();
    }
}

TEST(CodecHeif_Tests, SniffHeifAndAvif) {
    auto ftyp = [](const std::string& major, const std::vector<std::string>& compatible) {
        std::string box = std::string(4, '\0') + "ftyp" + major + std::string(4, '\0');
        for (const auto& c : compatible) {
            box += c;
        }
        const size_t n = box.size();
        box[0] = char(n >> 24);
        box[1] = char(n >> 16);
        box[2] = char(n >> 8);
        box[3] = char(n);
        return box + std::string(16, '\0');
    };
    EXPECT_EQ(codec::sniff(bytes(ftyp("heic", {"mif1", "heic"}))), codec::format::heif);
    EXPECT_EQ(codec::sniff(bytes(ftyp("heix", {}))), codec::format::heif);
    EXPECT_EQ(codec::sniff(bytes(ftyp("hevc", {}))), codec::format::heif);
    EXPECT_EQ(codec::sniff(bytes(ftyp("avif", {"mif1"}))), codec::format::avif);
    EXPECT_EQ(codec::sniff(bytes(ftyp("avis", {}))), codec::format::avif);
    EXPECT_EQ(codec::sniff(bytes(ftyp("mif1", {"mif1", "heic"}))), codec::format::heif);
    EXPECT_EQ(codec::sniff(bytes(ftyp("mif1", {"miaf", "avif"}))), codec::format::avif);
    EXPECT_EQ(codec::sniff(bytes(ftyp("mif1", {"heic", "avif"}))), codec::format::heif);   // the first of them
    EXPECT_EQ(codec::sniff(bytes(ftyp("msf1", {"hevc"}))), codec::format::heif);
    EXPECT_FALSE(codec::sniff(bytes(ftyp("mif1", {"miaf"}))));
    EXPECT_FALSE(codec::sniff(bytes(ftyp("isom", {"mp41"}))));       // an MP4
    EXPECT_FALSE(codec::sniff(bytes(ftyp("qt  ", {"qt  "}))));       // QuickTime
    // the compatible brands past the box's size are not the file's
    std::string cut = ftyp("mif1", {"heic"});
    cut[3] = char(16);
    EXPECT_FALSE(codec::sniff(bytes(cut)));
    EXPECT_FALSE(codec::sniff(bytes(std::string("\0\0\0\x18" "ftyp", 8))));
}

// Decoding bytes and encoding into bytes cannot throw: a system object
// ImageIO's call needs and the system does not make ends the program, as
// running out of managed memory does (DESIGN 391). They threw bad_alloc.
// Into and from a stream they throw what the stream throws.
TEST(CodecHeif_Tests, BytesInAndOutCannotThrow) {
    codec::image picture(4, 4, pixel_format::rgb8);
    static_assert(noexcept(codec::heif::decode(slice<const byte>())));
    static_assert(noexcept(codec::heif::encode(picture)));
    static_assert(noexcept(codec::heif::encode(picture, codec::heif::options{})));
    static_assert(noexcept(codec::decode(slice<const byte>())));
    static_assert(noexcept(codec::decode(slice<const byte>(), codec::decode_options())));
    static_assert(!noexcept(codec::heif::encode(picture, std::declval<const io::writer&>())));
    static_assert(!noexcept(codec::heif::decode(std::declval<const io::reader&>())));
    // the boundaries: no file, a file of one byte, a cut file, one pixel
    EXPECT_FALSE(codec::heif::decode(slice<const byte>()));   // an error, not the end of the program
    const byte one{0};
    EXPECT_FALSE(codec::heif::decode(slice<const byte>(&one, 1)));
    EXPECT_FALSE(codec::decode(slice<const byte>(&one, 1)));
    codec::image dot(1, 1, pixel_format::rgb8);
    auto file = codec::heif::encode(dot);
#if defined(__APPLE__)
    if (!file) {
        GTEST_SKIP() << "no HEIC encoder on this system";
    }
    auto back = codec::heif::decode(file->as_slice());
    ASSERT_TRUE(back);
    EXPECT_EQ(back->width(), 1u);
    EXPECT_EQ(back->height(), 1u);
    EXPECT_FALSE(codec::heif::decode(file->as_slice(0, file->size() / 2)));
#else
    EXPECT_FALSE(file);
#endif
}

#if !defined(__APPLE__)
TEST(CodecHeif_Tests, UnsupportedWithoutTheSystemsCodec) {
    codec::image picture(4, 4, pixel_format::rgb8);
    EXPECT_EQ(codec::heif::encode(picture).error().code(), codec::errc::unsupported);
    const std::string file = std::string("\0\0\0\x18" "ftypheic\0\0\0\0mif1heic", 24);
    EXPECT_EQ(code_of(codec::heif::decode(bytes(file))), codec::errc::unsupported);
    EXPECT_EQ(code_of(codec::decode(bytes(file))), codec::errc::unsupported);
}
#else

TEST(CodecHeif_Tests, SipsFilesAgainstImageIO) {
    const std::string oracle = heif_oracle();
    if (oracle.empty()) {
        GTEST_SKIP() << "no HEIF oracle";
    }
    // (PngSuite image, the module's native format)
    const std::pair<const char*, pixel_format> cases[] = {
        {"basn2c08.png", pixel_format::rgb8},  {"basn6a08.png", pixel_format::rgba8}, {"basn0g08.png", pixel_format::gray8},
        {"basn4a08.png", pixel_format::gray_alpha8}, {"basn2c16.png", pixel_format::rgb16}, {"basn6a16.png", pixel_format::rgba16},
        {"tbrn2c08.png", pixel_format::rgb8},
    };
    for (const auto& [png, expected_format] : cases) {
        const std::string file = sips_heic(png);
        if (file.empty()) {
            GTEST_SKIP() << "no sips or no " << png;
        }
        const std::string path = write_file(std::string(png) + ".heic", file);
        EXPECT_EQ(codec::sniff(bytes(file)), codec::format::heif) << png;
        auto ours = codec::heif::decode(bytes(file));
        ASSERT_TRUE(ours) << png << ": " << ours.error().message().view();
        const std::string meta = text_of("'" + oracle + "' meta '" + path + "'");
        auto theirs = run_oracle(oracle, "heif", path);
        ASSERT_TRUE(theirs) << png;
        EXPECT_EQ(ours->width(), theirs->width) << png;
        EXPECT_EQ(ours->height(), theirs->height) << png;
        // what ImageIO says the file is, and what the module made of it
        EXPECT_EQ(is_deep(ours->format()), meta_of(meta, "depth") > 8) << png;
        EXPECT_EQ(has_alpha(ours->format()), meta_of(meta, "alpha") == 1) << png;
        EXPECT_EQ(long(ours->icc().size()), meta_of(meta, "icc")) << png;
        EXPECT_EQ(long(ours->orientation()), meta_of(meta, "orientation")) << png;
        (void)expected_format;
        // the pixels, RGB ones: ImageIO's drawn by CoreGraphics in the same
        // space; opaque ones the same, premultiplied ones within rounding
        const bool gray = ours->format() == pixel_format::gray8 || ours->format() == pixel_format::gray16 ||
                          ours->format() == pixel_format::gray_alpha8 || ours->format() == pixel_format::gray_alpha16;
        if (!gray) {
            const long worst = largest_difference(*ours, *theirs);
            EXPECT_LE(worst, has_alpha(ours->format()) ? (theirs->depth == 16 ? 257 : 1) : 0) << png;
            EXPECT_GE(worst, 0) << png;
        }
        // the stream the memory, in pieces of any size
        for (size_t step : {size_t(1), size_t(7), size_t(4096)}) {
            pieces p{&file, step};
            auto streamed = codec::heif::decode(io::reader(p));
            ASSERT_TRUE(streamed) << png;
            EXPECT_TRUE(std::equal(streamed->pixels().begin(), streamed->pixels().end(), ours->pixels().begin(), ours->pixels().end())) << png;
        }
        // codec::decode by the signature, and every format asked for
        auto by_sniff = codec::decode(bytes(file));
        ASSERT_TRUE(by_sniff);
        EXPECT_EQ(by_sniff->format(), ours->format());
        for (auto f : {pixel_format::gray8, pixel_format::gray_alpha8, pixel_format::rgb8, pixel_format::rgba8, pixel_format::gray16,
                       pixel_format::gray_alpha16, pixel_format::rgb16, pixel_format::rgba16, pixel_format::cmyk8}) {
            auto asked = codec::decode(bytes(file), {.want = f});
            ASSERT_TRUE(asked) << png;
            EXPECT_EQ(asked->format(), f);
            const codec::image expected_image = ours->convert(f);
            EXPECT_TRUE(std::equal(asked->pixels().begin(), asked->pixels().end(), expected_image.pixels().begin(), expected_image.pixels().end()))
                << png << " as " << int(f);
        }
        auto bare = codec::decode(bytes(file), {.metadata = false});
        ASSERT_TRUE(bare);
        EXPECT_TRUE(bare->icc().empty());
        EXPECT_EQ(bare->orientation(), 1);
    }
}

TEST(CodecHeif_Tests, AvifAsImageIOReadsIt) {
    const std::string oracle = heif_oracle();
    if (oracle.empty()) {
        GTEST_SKIP() << "no HEIF oracle";
    }
    for (const char* png : {"basn2c08.png", "basn6a08.png"}) {
        const std::string out = temp(std::string(png) + ".avif");
        const std::string cmd = "'" + oracle + "' make '" + oracle_path(std::string("pngsuite/") + png) + "' '" + out + "' public.avif 80 2>/dev/null";
        if (std::system(cmd.c_str()) != 0) {
            GTEST_SKIP() << "no AVIF encoder in this system's ImageIO";
        }
        const std::string file = read_file(out);
        EXPECT_EQ(codec::sniff(bytes(file)), codec::format::avif) << png;
        auto ours = codec::decode(bytes(file));
        ASSERT_TRUE(ours) << png << ": " << ours.error().message().view();
        auto theirs = run_oracle(oracle, "heif", out);
        ASSERT_TRUE(theirs);
        const long worst = largest_difference(*ours, *theirs);
        EXPECT_GE(worst, 0);
        EXPECT_LE(worst, has_alpha(ours->format()) ? 1 : 0) << png;
    }
}

TEST(CodecHeif_Tests, TheModulesOwnFilesRoundTrip) {
    const std::string oracle = heif_oracle();
    for (auto f : {pixel_format::rgb8, pixel_format::rgba8, pixel_format::gray8, pixel_format::rgb16, pixel_format::rgba16, pixel_format::cmyk8}) {
        const codec::image picture = gradient(96, 64, f);
        auto file = codec::heif::encode(picture);
        if (!file && file.error().code() == codec::errc::unsupported) {
            GTEST_SKIP() << "no HEIC encoder: " << file.error().message().view();
        }
        ASSERT_TRUE(file) << int(f);
        const std::string data(reinterpret_cast<const char*>(file->data()), file->size());
        EXPECT_EQ(codec::sniff(bytes(data)), codec::format::heif);
        auto back = codec::heif::decode(bytes(data));
        ASSERT_TRUE(back) << int(f) << ": " << back.error().message().view();
        EXPECT_EQ(back->width(), 96u);
        EXPECT_EQ(back->height(), 64u);
        EXPECT_EQ(has_alpha(back->format()), has_alpha(f)) << int(f);
        EXPECT_GT(psnr(picture.format() == pixel_format::cmyk8 ? picture.convert(pixel_format::rgb8) : picture, back->convert(picture.format() == pixel_format::cmyk8 ? pixel_format::rgb8 : picture.format())), 35.0)
            << int(f);
        if (!oracle.empty()) {
            const std::string path = write_file("own.heic", data);
            auto theirs = run_oracle(oracle, "heif", path);
            ASSERT_TRUE(theirs) << int(f);
        }
        // into a writer: the same file
        collect sink;
        auto written = codec::heif::encode(picture, io::writer(sink));
        ASSERT_TRUE(written);
        auto again = codec::heif::decode(bytes(sink.bytes));
        ASSERT_TRUE(again);
        EXPECT_TRUE(std::equal(again->pixels().begin(), again->pixels().end(), back->pixels().begin(), back->pixels().end())) << int(f);
    }
}

TEST(CodecHeif_Tests, QualityOrientationAndProfile) {
    const codec::image picture = gradient(128, 96, pixel_format::rgb8);
    auto low = codec::heif::encode(picture, {.quality = 10});
    if (!low && low.error().code() == codec::errc::unsupported) {
        GTEST_SKIP() << "no HEIC encoder";
    }
    ASSERT_TRUE(low);
    auto high = codec::heif::encode(picture, {.quality = 100});
    ASSERT_TRUE(high);
    EXPECT_LT(low->size(), high->size());
    for (int q : {0, -5, 101}) {
        auto bad = codec::heif::encode(picture, {.quality = q});
        ASSERT_FALSE(bad);
        EXPECT_EQ(bad.error().code(), codec::errc::invalid_argument);
        collect sink;
        auto bad_out = codec::heif::encode(picture, io::writer(sink), {.quality = q});
        ASSERT_FALSE(bad_out);
        EXPECT_EQ(bad_out.error().code(), codec::errc::invalid_argument);
        EXPECT_TRUE(sink.bytes.empty());
    }
    // the default is 85
    auto a = codec::heif::encode(picture, {.quality = 85}), b = codec::heif::encode(picture), c = codec::heif::encode(picture, {.quality = 20});
    ASSERT_TRUE(a && b && c);
    EXPECT_EQ(a->size(), b->size());
    EXPECT_NE(a->size(), c->size());
    // orientation written and read back, as ImageIO reads it
    codec::image turned = picture.clone();
    turned.set_orientation(6);
    auto file = codec::heif::encode(turned);
    ASSERT_TRUE(file);
    auto back = codec::heif::decode(slice<const std::byte>(file->data(), file->size()));
    ASSERT_TRUE(back);
    EXPECT_EQ(back->orientation(), 6);
    if (!heif_oracle().empty()) {
        const std::string path = write_file("turned.heic", std::string(reinterpret_cast<const char*>(file->data()), file->size()));
        EXPECT_EQ(meta_of(text_of("'" + heif_oracle() + "' meta '" + path + "'"), "orientation"), 6);
    }
    // a profile carried through: the file's profile on an image written and read
    const std::string sips = sips_heic("basn2c08.png");
    if (!sips.empty()) {
        auto with_profile = codec::heif::decode(bytes(sips));
        ASSERT_TRUE(with_profile);
        ASSERT_FALSE(with_profile->icc().empty());
        auto rewritten = codec::heif::encode(*with_profile);
        ASSERT_TRUE(rewritten);
        auto reread = codec::heif::decode(slice<const std::byte>(rewritten->data(), rewritten->size()));
        ASSERT_TRUE(reread);
        EXPECT_TRUE(std::equal(reread->icc().begin(), reread->icc().end(), with_profile->icc().begin(), with_profile->icc().end()));
    }
}

TEST(CodecHeif_Tests, LimitsAndDamage) {
    const std::string file = sips_heic("basn2c08.png");
    if (file.empty()) {
        GTEST_SKIP() << "no sips";
    }
    // the size from the file's properties, before any pixel
    auto small = codec::decode(bytes(file), {.limits = {.max_pixels = 32 * 32 - 1}});
    ASSERT_FALSE(small);
    EXPECT_EQ(small.error().code(), codec::errc::too_large);
    EXPECT_TRUE(codec::decode(bytes(file), {.limits = {.max_pixels = 32 * 32}}));
    auto small_profile = codec::decode(bytes(file), {.limits = {.max_metadata = 10}});
    ASSERT_FALSE(small_profile);
    EXPECT_EQ(small_profile.error().code(), codec::errc::too_large);
    // cut anywhere, or bytes changed: an error, never a crash
    for (size_t n = 0; n < file.size(); n += std::max<size_t>(1, file.size() / 97)) {
        auto cut = codec::heif::decode(bytes(file.substr(0, n)));
        if (!cut) {
            const auto c = cut.error().code();
            EXPECT_TRUE(c == codec::errc::corrupt || c == codec::errc::unexpected_end || c == codec::errc::unsupported) << n;
        }
    }
    std::string noisy = file;
    for (size_t i = 40; i < noisy.size(); i += 13) {
        noisy[i] = char(noisy[i] ^ 0x5A);
    }
    (void)codec::heif::decode(bytes(noisy));
    EXPECT_FALSE(codec::heif::decode(bytes(std::string("not an image at all"))));
    // ImageIO takes a JPEG; a JPEG handed to heif::decode is not HEIF
    auto jpeg = read_oracle("libjpeg-turbo/testimages/testorig.jpg");
    if (jpeg) {
        auto r = codec::heif::decode(bytes(*jpeg));
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), codec::errc::corrupt);
    }
}

// An HEVC slice whose entry point lies past the slice's data: VideoToolbox
// (macOS 26.5) waits for ever on its decoding service on it, so the module
// refuses the file before ImageIO sees it. Each decoding runs on a thread
// of its own with a deadline: a hang fails the test instead of stopping
// the suite (the stuck thread is left behind)
TEST(CodecHeif_Tests, EntryPointPastTheSliceRefused) {
    auto within = [](const std::string& file) -> std::optional<codec::errc> {
        auto data = std::make_shared<const std::string>(file);
        auto done = std::make_shared<std::promise<codec::errc>>();
        auto answer = done->get_future();
        std::thread([data, done] {
            done->set_value(code_of(codec::heif::decode(bytes(*data))));
        }).detach();
        if (answer.wait_for(std::chrono::seconds(30)) != std::future_status::ready) {
            return std::nullopt;
        }
        return answer.get();
    };
    const std::string seeds = std::string(__FILE__).substr(0, std::string(__FILE__).rfind('/')) + "/fuzz/seeds/heif_decode/";
    // the fuzzer's two files: the entry point of the one slice past its end
    for (const char* name : {"regress_entry_point_past_slice.heic", "regress_entry_point_past_slice2.heic"}) {
        const std::string file = read_file(seeds + name);
        ASSERT_FALSE(file.empty()) << name;
        const auto code = within(file);
        ASSERT_TRUE(code) << name << ": the decoding did not return in 30 s";
        EXPECT_EQ(*code, codec::errc::corrupt) << name;
    }
    // sips's file of two rows of wavefronts, its second entry point moved:
    // within the slice's 168 bytes of data it decodes, at their end and
    // past it it is refused (the slice header's 4 bytes at 1015 written
    // again, the entry point in their last 12 bits but the stop bit)
    const std::string sips = read_file(seeds + "basn2c16.heic");
    ASSERT_EQ(sips.size(), 1187u);
    ASSERT_EQ(sips.substr(1015, 4), std::string("\xaf\xa1\x12\x70", 4));   // the entry point at 148, in 8 bits
    auto moved = [&](const char (&header)[5]) {
        std::string file = sips;
        file.replace(1015, 4, header, 4);
        return file;
    };
    const auto at148 = within(sips), at167 = within(moved("\xaf\xa1\x81\x4d")), at168 = within(moved("\xaf\xa1\x81\x4f")),
               at1000 = within(moved("\xaf\xa1\x87\xcf"));
    ASSERT_TRUE(at148 && at167 && at168 && at1000) << "a decoding did not return in 30 s";
    EXPECT_EQ(*at148, codec::errc{});
    EXPECT_EQ(*at167, codec::errc{});
    EXPECT_EQ(*at168, codec::errc::corrupt);
    EXPECT_EQ(*at1000, codec::errc::corrupt);
    // the error's offset: the slice's NAL unit; codec::decode and a stream the same
    auto direct = codec::heif::decode(bytes(moved("\xaf\xa1\x87\xcf")));
    ASSERT_FALSE(direct);
    EXPECT_EQ(direct.error().offset(), 1013u);
    EXPECT_EQ(code_of(codec::decode(bytes(moved("\xaf\xa1\x87\xcf")))), codec::errc::corrupt);
    const std::string past = moved("\xaf\xa1\x87\xcf");
    pieces p{&past, 7};
    EXPECT_EQ(code_of(codec::heif::decode(io::reader(p))), codec::errc::corrupt);
}
#endif
