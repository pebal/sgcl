//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// codec: the JPEG decoder (jpeg.h, detail/jpeg_*.h). Bit for bit against
// libjpeg-turbo as `djpeg -dct int` decodes (tools/codec_oracle.c, the same
// library, the integer IDCT and fancy upsampling set): libjpeg-turbo's test
// images, Go's image/testdata, and files cjpeg makes here of every sampling,
// size, quality, restart interval, color space and scan layout. Go's
// image/jpeg as a second opinion (its own IDCT, no fancy upsampling): the
// differences measured, not required to be none. Negatives each of the
// right code; a stream in pieces as memory; EXIF and ICC.
#include "oracle.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

using namespace sgcl;
using codec::pixel_format;
using namespace codec_test;

namespace {
    const std::string TestImages = "libjpeg-turbo/libjpeg-turbo-3.2.0/testimages/";

    std::filesystem::path scratch_dir() {
        return scratch_path("sgcl_codec_jpeg_tests");
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

    std::string go_testdata() {
        FILE* p = popen("go env GOROOT 2>/dev/null", "r");
        if (!p) {
            return "";
        }
        char buf[512] = {};
        std::string root = fgets(buf, sizeof(buf), p) ? std::string(buf) : "";
        pclose(p);
        while (!root.empty() && (root.back() == '\n' || root.back() == '\r')) {
            root.pop_back();
        }
        return root.empty() ? "" : root + "/src/image/testdata/";
    }

    std::string djpeg_tool(const char* name) {
        for (const std::string dir : {"/opt/homebrew/opt/jpeg-turbo/bin/", "/opt/homebrew/bin/", "/usr/local/bin/"}) {
            if (std::filesystem::exists(dir + name)) {
                return dir + name;
            }
        }
        return "";
    }

    // An Adobe APP14 segment in the file: CMYK values inverted
    bool has_adobe(const std::string& data) {
        for (size_t i = 0; i + 9 < data.size(); ++i) {
            if (uint8_t(data[i]) == 0xFF && uint8_t(data[i + 1]) == 0xEE && data.compare(i + 4, 5, "Adobe") == 0) {
                return true;
            }
        }
        return false;
    }

    // The module's image in the C oracle's form: the samples as libjpeg
    // gives them (CMYK with Adobe's inversion)
    std::string as_libjpeg(const codec::image& im, bool adobe) {
        auto px = im.pixels();
        std::string out(reinterpret_cast<const char*>(px.data()), px.size());
        if (im.format() == pixel_format::cmyk8 && adobe) {
            for (auto& c : out) {
                c = char(~uint8_t(c));
            }
        }
        return out;
    }

    // Against libjpeg: "" when the same, else what differs
    std::string against_libjpeg(const std::string& path, const std::string& data, const char* mode = "jpeg") {
        auto ours = codec::jpeg::decode(bytes(data));
        auto theirs = run_oracle(c_oracle(), mode, path);
        if (!ours || !theirs) {
            return std::string(ours ? "" : "ours: " + std::string(ours.error().message().view())) + (theirs ? "" : " libjpeg refused");
        }
        if (ours->width() != theirs->width || ours->height() != theirs->height ||
            codec::detail::bytes_per_pixel(ours->format()) != unsigned(theirs->depth)) {
            return "size or channels";
        }
        const std::string mine = as_libjpeg(*ours, has_adobe(data));
        if (mine == theirs->pixels) {
            return "";
        }
        size_t first = 0;
        size_t differ = 0;
        for (size_t i = 0; i < mine.size(); ++i) {
            if (mine[i] != theirs->pixels[i]) {
                if (!differ) {
                    first = i;
                }
                ++differ;
            }
        }
        const size_t c = theirs->depth, row = size_t(theirs->width) * c;
        return std::to_string(differ) + " samples differ, first at x " + std::to_string((first % row) / c) + " y " + std::to_string(first / row) +
               " ours " + std::to_string(uint8_t(mine[first])) + " libjpeg " + std::to_string(uint8_t(theirs->pixels[first]));
    }

    // A PPM (or PGM) of w × h cut from testorig.ppm, repeated where larger
    std::optional<std::string> source_image(uint32_t w, uint32_t h, bool gray) {
        auto ppm = read_oracle(TestImages + "testorig.ppm");
        if (!ppm) {
            return std::nullopt;
        }
        unsigned sw, sh, maxval;
        int consumed = 0;
        if (std::sscanf(ppm->c_str(), "P6 %u %u %u%n", &sw, &sh, &maxval, &consumed) != 3) {
            return std::nullopt;
        }
        const char* px = ppm->data() + consumed + 1;
        std::string out = (gray ? "P5\n" : "P6\n") + std::to_string(w) + " " + std::to_string(h) + "\n255\n";
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                const char* p = px + (size_t(y % sh) * sw + x % sw) * 3;
                if (gray) {
                    out += p[1];
                } else {
                    out.append(p, 3);
                }
            }
        }
        return out;
    }

    // cjpeg's file of the source with the arguments given, "" when it fails
    std::string cjpeg(const std::string& source_path, const std::string& args, const std::string& name) {
        auto out = (scratch_dir() / name).string();
        std::string cmd = "'" + djpeg_tool("cjpeg") + "' " + args + " -outfile '" + out + "' '" + source_path + "' 2>/dev/null";
        if (std::system(cmd.c_str()) != 0) {
            return "";
        }
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

    codec::errc code_of(const expected<codec::image, codec::error>& r) {
        return r ? codec::errc{} : r.error().code();
    }

    std::vector<std::string> corpus_files() {
        std::vector<std::string> out;
        for (const char* name : {"testorig.jpg", "testimgint.jpg"}) {
            if (std::filesystem::exists(oracle_path(TestImages + name))) {
                out.push_back(oracle_path(TestImages + name));
            }
        }
        const std::string go = go_testdata();
        if (!go.empty() && std::filesystem::is_directory(go)) {
            std::vector<std::string> names;
            for (auto& e : std::filesystem::directory_iterator(go)) {
                const auto n = e.path().filename().string();
                if (e.path().extension() == ".jpeg") {
                    names.push_back(e.path().string());
                }
            }
            std::sort(names.begin(), names.end());
            out.insert(out.end(), names.begin(), names.end());
        }
        return out;
    }
}

TEST(CodecJpeg_Tests, TheCorporaBitForBitWithLibjpeg) {
    if (c_oracle().empty()) {
        GTEST_SKIP() << "no C oracle (cc with libpng and libjpeg)";
    }
    auto files = corpus_files();
    if (files.empty()) {
        GTEST_SKIP() << "no " << oracle_path(TestImages) << " and no Go image/testdata";
    }
    size_t compared = 0;
    for (const auto& path : files) {
        const std::string data = read_file(path);
        // Go's progressive.truncated is whole (EOI and all) but its
        // progression stops early: libjpeg smooths it, the module does not
        const bool incomplete = path.find("truncated") != std::string::npos;
        EXPECT_EQ(against_libjpeg(path, data, incomplete ? "jpegns" : "jpeg"), "") << path;
        ++compared;
    }
    EXPECT_GE(compared, 20u);
}

TEST(CodecJpeg_Tests, CjpegFilesOfEveryKind) {
    if (c_oracle().empty() || djpeg_tool("cjpeg").empty()) {
        GTEST_SKIP() << "no C oracle or no cjpeg";
    }
    struct Kind { const char* name; const char* args; bool gray; };
    const Kind kinds[] = {
        {"444", "-sample 1x1", false},
        {"422", "-sample 2x1", false},
        {"440", "-sample 1x2", false},
        {"420", "-sample 2x2", false},
        {"411", "-sample 4x1", false},
        {"410", "-sample 4x2", false},
        {"gray", "-grayscale", true},
        {"rgb", "-rgb", false},
        {"420 restart 1 row", "-sample 2x2 -restart 1", false},
        {"422 restart 3 MCUs", "-sample 2x1 -restart 3B", false},
        {"gray restart 1 MCU", "-restart 1B", true},
        {"420 optimized", "-sample 2x2 -optimize", false},
        {"444 quality 100", "-sample 1x1 -quality 100", false},
        {"420 quality 5", "-sample 2x2 -quality 5", false},
    };
    const std::pair<uint32_t, uint32_t> sizes[] = {{1, 1}, {2, 3}, {3, 2}, {9, 17}, {17, 9}, {33, 31}, {64, 64}, {227, 149}};
    size_t made = 0;
    for (const auto& k : kinds) {
        for (auto [w, h] : sizes) {
            auto src = source_image(w, h, k.gray);
            if (!src) {
                GTEST_SKIP() << "no " << oracle_path(TestImages + "testorig.ppm");
            }
            const auto src_path = write_file(k.gray ? "src.pgm" : "src.ppm", *src);
            const auto path = cjpeg(src_path, k.args, "made.jpg");
            ASSERT_FALSE(path.empty()) << k.name;
            EXPECT_EQ(against_libjpeg(path, read_file(path)), "") << k.name << " " << w << "x" << h;
            ++made;
        }
    }
    EXPECT_EQ(made, std::size(kinds) * std::size(sizes));
}

TEST(CodecJpeg_Tests, AScanPerComponent) {
    // sequential files of several scans (cjpeg's scan scripts): the
    // components kept whole until the last scan
    if (c_oracle().empty() || djpeg_tool("cjpeg").empty()) {
        GTEST_SKIP() << "no C oracle or no cjpeg";
    }
    const char* scripts[] = {"0;\n1;\n2;\n", "0;\n1 2;\n", "2;\n0;\n1;\n", "1 2;\n0;\n"};
    for (const char* script : scripts) {
        for (const char* sampling : {"1x1", "2x2", "2x1"}) {
            for (auto [w, h] : {std::pair{17u, 9u}, std::pair{64u, 48u}, std::pair{227u, 149u}}) {
                auto src = source_image(w, h, false);
                if (!src) {
                    GTEST_SKIP();
                }
                const auto src_path = write_file("src.ppm", *src);
                const auto script_path = write_file("scans.txt", script);
                const auto path = cjpeg(src_path, std::string("-sample ") + sampling + " -scans '" + script_path + "'", "scans.jpg");
                ASSERT_FALSE(path.empty());
                EXPECT_EQ(against_libjpeg(path, read_file(path)), "") << "scans " << script << " " << sampling << " " << w << "x" << h;
            }
        }
    }
}

TEST(CodecJpeg_Tests, DjpegItselfAgrees) {
    // the C oracle is the library djpeg is: its command, on the test
    // images, gives the same pixels
    const auto djpeg = djpeg_tool("djpeg");
    if (djpeg.empty()) {
        GTEST_SKIP() << "no djpeg";
    }
    for (const char* name : {"testorig.jpg", "testimgint.jpg"}) {
        CODEC_ORACLE(data, TestImages + name);
        auto out = (scratch_dir() / "djpeg.ppm").string();
        std::string cmd = "'" + djpeg + "' -dct int -pnm -outfile '" + out + "' '" + oracle_path(TestImages + name) + "'";
        ASSERT_EQ(std::system(cmd.c_str()), 0);
        const std::string ppm = read_file(out);
        unsigned w, h, maxval;
        int consumed = 0;
        ASSERT_EQ(std::sscanf(ppm.c_str(), "P6 %u %u %u%n", &w, &h, &maxval, &consumed), 3);
        codec::image ours = *codec::jpeg::decode(bytes(data));
        auto px = ours.pixels();
        ASSERT_EQ(ppm.size() - size_t(consumed) - 1, px.size());
        EXPECT_EQ(std::memcmp(ppm.data() + consumed + 1, px.data(), px.size()), 0) << name;
    }
}

TEST(CodecJpeg_Tests, GoAsASecondOpinion) {
    // Go's IDCT is its own and it upsamples by repeating: the pixels are
    // near libjpeg's, not equal. Measured here: the largest difference and
    // the mean on each file. Where no chroma is upsampled (gray, 4:4:4,
    // CMYK) the IDCTs alone part, by at most 3 levels (gray by 1); where
    // it is, repetition and fancy upsampling part at the edges of color,
    // up to some 90 levels on a sharp one, a mean of a few
    if (go_oracle().empty()) {
        GTEST_SKIP() << "no Go oracle";
    }
    for (const auto& path : corpus_files()) {
        const std::string data = read_file(path);
        auto ours = codec::jpeg::decode(bytes(data));
        auto go = run_oracle(go_oracle(), "jpeg", path);
        if (!ours || !go) {
            continue;
        }
        const std::string mine = oracle_form(*ours, 8);
        ASSERT_EQ(mine.size(), go->pixels.size()) << path;
        int worst = 0;
        double sum = 0;
        for (size_t i = 0; i < mine.size(); ++i) {
            const int d = std::abs(int(uint8_t(mine[i])) - int(uint8_t(go->pixels[i])));
            worst = std::max(worst, d);
            sum += d;
        }
        const double mean = sum / double(mine.size() / 4 * 3);
        const std::string name = std::filesystem::path(path).filename().string();
        std::printf("  %-44s largest %3d, mean %.3f\n", name.c_str(), worst, mean);
        const bool no_upsampling = name.find("gray") != std::string::npos || name.find(".444.") != std::string::npos ||
                                   name.find("cmyk") != std::string::npos || name == "video-001.jpeg";
        if (no_upsampling) {
            EXPECT_LE(worst, 3) << path;
        }
        EXPECT_LT(mean, 5.0) << path;
    }
}

TEST(CodecJpeg_Tests, AStreamInPiecesIsMemory) {
    size_t step = 1;
    for (const auto& path : corpus_files()) {
        const std::string data = read_file(path);
        auto whole = codec::jpeg::decode(bytes(data));
        pieces p{&data, step};
        auto streamed = codec::jpeg::decode(io::reader(p));
        step = step % 97 + 5;
        ASSERT_EQ(bool(whole), bool(streamed)) << path;
        if (whole) {
            ASSERT_EQ(whole->pixels().size(), streamed->pixels().size());
            EXPECT_EQ(std::memcmp(whole->pixels().data(), streamed->pixels().data(), whole->pixels().size()), 0) << path;
            auto any = codec::decode(bytes(data));
            ASSERT_TRUE(any);
            EXPECT_EQ(std::memcmp(any->pixels().data(), whole->pixels().data(), whole->pixels().size()), 0) << path;
            pieces q{&data, 777};
            auto any_streamed = codec::decode(io::reader(q));
            ASSERT_TRUE(any_streamed) << path;
        }
    }
}

TEST(CodecJpeg_Tests, WhatIsNotRead) {
    // arithmetic coding, 12-bit samples: unsupported
    for (const char* name : {"testimgari.jpg", "monkey12.jpg"}) {
        CODEC_ORACLE(data, TestImages + name);
        EXPECT_EQ(code_of(codec::jpeg::decode(bytes(data))), codec::errc::unsupported) << name;
    }
    // a progressive file cut after some of its scans: unexpected_end, no
    // partial image (ProgressiveNegatives cuts one everywhere)
    const std::string go = go_testdata();
    if (!go.empty() && std::filesystem::exists(go + "video-001.progressive.jpeg")) {
        const std::string whole = read_file(go + "video-001.progressive.jpeg");
        EXPECT_EQ(code_of(codec::jpeg::decode(bytes(whole.substr(0, whole.size() / 2)))), codec::errc::unexpected_end);
    }
}

TEST(CodecJpeg_Tests, EveryCutIsUnexpectedEnd) {
    CODEC_ORACLE(data, TestImages + "testorig.jpg");
    for (size_t n = 0; n + 1 < data.size(); ++n) {
        const std::string cut = data.substr(0, n);
        auto r = codec::jpeg::decode(bytes(cut));
        ASSERT_FALSE(r) << n;
        EXPECT_EQ(r.error().code(), codec::errc::unexpected_end) << n << ": " << r.error().message();
        if (n % 97 == 0) {
            pieces p{&cut, 7};
            EXPECT_EQ(code_of(codec::jpeg::decode(io::reader(p))), codec::errc::unexpected_end) << n;
        }
    }
}

TEST(CodecJpeg_Tests, BadMarkersAndTables) {
    CODEC_ORACLE(data, TestImages + "testorig.jpg");
    auto find = [&](const std::string& d, uint8_t code) {
        for (size_t i = 0; i + 1 < d.size(); ++i) {
            if (uint8_t(d[i]) == 0xFF && uint8_t(d[i + 1]) == code) {
                return i;
            }
        }
        return std::string::npos;
    };
    auto decode = [](const std::string& d) { return code_of(codec::jpeg::decode(bytes(d))); };
    EXPECT_EQ(decode(data), codec::errc{});
    // no SOI
    EXPECT_EQ(decode("\xFF\xD9" + data.substr(2)), codec::errc::corrupt);
    // a DHT of class 2, of table 4, and of more codes than its bits hold
    {
        const size_t at = find(data, 0xC4);
        ASSERT_NE(at, std::string::npos);
        auto bad = data;
        bad[at + 4] = char(0x20);
        EXPECT_EQ(decode(bad), codec::errc::corrupt);
        bad = data;
        bad[at + 4] = char(0x04);
        EXPECT_EQ(decode(bad), codec::errc::corrupt);
        bad = data;
        bad[at + 5] = char(3);   // three codes of length 1
        EXPECT_EQ(decode(bad), codec::errc::corrupt);
    }
    // a DQT of table 5
    {
        const size_t at = find(data, 0xDB);
        auto bad = data;
        bad[at + 4] = char(0x05);
        EXPECT_EQ(decode(bad), codec::errc::corrupt);
    }
    // SOF: 12 bits, no components, height 0 (DNL), lossless, arithmetic
    {
        const size_t at = find(data, 0xC0);
        ASSERT_NE(at, std::string::npos);
        auto bad = data;
        bad[at + 4] = char(12);
        EXPECT_EQ(decode(bad), codec::errc::unsupported);
        bad = data;
        bad[at + 9] = 0;
        EXPECT_EQ(decode(bad), codec::errc::corrupt);
        bad = data;
        bad[at + 5] = bad[at + 6] = 0;
        EXPECT_EQ(decode(bad), codec::errc::unsupported);
        bad = data;
        bad[at + 1] = char(0xC3);
        EXPECT_EQ(decode(bad), codec::errc::unsupported);
        bad = data;
        bad[at + 1] = char(0xC9);
        EXPECT_EQ(decode(bad), codec::errc::unsupported);
    }
    // a DNL segment
    {
        const size_t at = find(data, 0xDA);
        const std::string dnl("\xFF\xDC\x00\x04\x00\x10", 6);
        EXPECT_EQ(decode(data.substr(0, at) + dnl + data.substr(at)), codec::errc::unsupported);
    }
    // SOS naming a Huffman table never defined, and a scan of Ss 1
    {
        const size_t at = find(data, 0xDA);
        auto bad = data;
        bad[at + 6] = char(0x33);
        EXPECT_EQ(decode(bad), codec::errc::corrupt);
        bad = data;
        const size_t ns = uint8_t(data[at + 4]);
        bad[at + 5 + 2 * ns] = 1;
        EXPECT_EQ(decode(bad), codec::errc::corrupt);
    }
    // the scan's data ended by EOI halfway: corrupt; a segment length of 1
    {
        const size_t at = find(data, 0xDA);
        EXPECT_EQ(decode(data.substr(0, at + 400) + "\xFF\xD9"), codec::errc::corrupt);
        auto bad = data;
        const size_t dqt = find(data, 0xDB);
        bad[dqt + 2] = 0;
        bad[dqt + 3] = 1;
        EXPECT_EQ(decode(bad), codec::errc::corrupt);
    }
}

TEST(CodecJpeg_Tests, RestartMarkersOutOfTurn) {
    if (djpeg_tool("cjpeg").empty()) {
        GTEST_SKIP() << "no cjpeg";
    }
    auto src = source_image(64, 64, false);
    if (!src) {
        GTEST_SKIP();
    }
    const auto path = cjpeg(write_file("src.ppm", *src), "-sample 2x2 -restart 1B", "restart.jpg");
    ASSERT_FALSE(path.empty());
    const std::string data = read_file(path);
    EXPECT_TRUE(codec::jpeg::decode(bytes(data)));
    // the first RST0 made RST1
    auto bad = data;
    for (size_t i = 0; i + 1 < bad.size(); ++i) {
        if (uint8_t(bad[i]) == 0xFF && uint8_t(bad[i + 1]) == 0xD0) {
            bad[i + 1] = char(0xD1);
            break;
        }
    }
    EXPECT_EQ(code_of(codec::jpeg::decode(bytes(bad))), codec::errc::corrupt);
}

TEST(CodecJpeg_Tests, ExifAndIcc) {
    if (djpeg_tool("cjpeg").empty()) {
        GTEST_SKIP() << "no cjpeg";
    }
    auto src = source_image(40, 30, false);
    if (!src) {
        GTEST_SKIP();
    }
    // a profile of three chunks (65 519 bytes each at most)
    std::mt19937 rng(1);
    std::string profile(150000, '\0');
    for (auto& c : profile) {
        c = char(rng());
    }
    const auto icc_path = write_file("profile.icc", profile);
    const auto path = cjpeg(write_file("src.ppm", *src), "-icc '" + icc_path + "'", "icc.jpg");
    ASSERT_FALSE(path.empty());
    std::string data = read_file(path);
    // EXIF with orientation 6 after SOI
    const std::string tiff = std::string("MM\0*\0\0\0\x08\0\x01", 10) + std::string("\x01\x12\0\x03\0\0\0\x01\0\x06\0\0", 12) + std::string(4, '\0');
    const std::string body = std::string("Exif\0\0", 6) + tiff;
    const std::string app1 = std::string("\xFF\xE1", 2) + char((body.size() + 2) >> 8) + char((body.size() + 2) & 0xFF) + body;
    data = data.substr(0, 2) + app1 + data.substr(2);
    codec::image photo = *codec::jpeg::decode(bytes(data));
    EXPECT_EQ(photo.orientation(), 6u);
    ASSERT_EQ(photo.exif().size(), tiff.size());
    EXPECT_EQ(std::memcmp(photo.exif().data(), tiff.data(), tiff.size()), 0);
    ASSERT_EQ(photo.icc().size(), profile.size());
    EXPECT_EQ(std::memcmp(photo.icc().data(), profile.data(), profile.size()), 0);
    // no metadata asked; a limit below the profile
    codec::image bare = *codec::jpeg::decode(bytes(data), {.metadata = false});
    EXPECT_TRUE(bare.exif().empty());
    EXPECT_TRUE(bare.icc().empty());
    EXPECT_EQ(code_of(codec::jpeg::decode(bytes(data), {.limits = {.max_metadata = 100000}})), codec::errc::too_large);
    // the chunks in reverse order are joined in theirs; one missing drops the profile
    std::vector<std::pair<size_t, size_t>> segments;
    for (size_t i = 0; i + 4 < data.size(); ++i) {
        if (uint8_t(data[i]) == 0xFF && uint8_t(data[i + 1]) == 0xE2) {
            const size_t len = size_t(uint8_t(data[i + 2])) << 8 | uint8_t(data[i + 3]);
            segments.push_back({i, len + 2});
            i += len + 1;
        }
    }
    ASSERT_EQ(segments.size(), 3u);
    const size_t first = segments.front().first, last = segments.back().first + segments.back().second;
    std::string reversed;
    for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
        reversed += data.substr(it->first, it->second);
    }
    const std::string swapped = data.substr(0, first) + reversed + data.substr(last);
    codec::image joined = *codec::jpeg::decode(bytes(swapped));
    ASSERT_EQ(joined.icc().size(), profile.size());
    EXPECT_EQ(std::memcmp(joined.icc().data(), profile.data(), profile.size()), 0);
    const std::string missing = data.substr(0, segments[1].first) + data.substr(segments[1].first + segments[1].second);
    codec::image dropped = *codec::jpeg::decode(bytes(missing));
    EXPECT_TRUE(dropped.icc().empty());
}

TEST(CodecJpeg_Tests, TheFormatAskedFor) {
    CODEC_ORACLE(data, TestImages + "testorig.jpg");
    codec::image native = *codec::jpeg::decode(bytes(data));
    EXPECT_EQ(native.format(), pixel_format::rgb8);
    for (int f = 0; f < 9; ++f) {
        auto want = static_cast<pixel_format>(f);
        codec::image asked = *codec::jpeg::decode(bytes(data), {.want = want});
        codec::image converted = native.convert(want);
        ASSERT_EQ(asked.pixels().size(), converted.pixels().size());
        EXPECT_EQ(std::memcmp(asked.pixels().data(), converted.pixels().data(), asked.pixels().size()), 0) << f;
    }
    // the example of the docs
    vector<byte> file(bytes(data).begin(), bytes(data).end());
    codec::image photo = codec::decode(file, {.want = codec::pixel_format::rgba8});
    EXPECT_EQ(photo.width(), 227u);
    EXPECT_EQ(photo.format(), codec::pixel_format::rgba8);
}

// ---- progressive (C5) ------------------------------------------------------

namespace {
    // libjpeg's own default progression for YCbCr (jcparam.c's script, as
    // T.81 G.1.1 allows it): DC with a bit held back, the bands of Y, Cb, Cr,
    // then the refinements
    const char* DefaultProgression =
        "0 1 2: 0 0 0 1;\n0: 1 5 0 2;\n2: 1 63 0 1;\n1: 1 63 0 1;\n0: 6 63 0 2;\n0: 1 63 2 1;\n"
        "0 1 2: 0 0 1 0;\n2: 1 63 1 0;\n1: 1 63 1 0;\n0: 1 63 1 0;\n";
    // bands without successive approximation, DC scans one per component
    const char* SpectralOnly = "0: 0 0 0 0;\n1: 0 0 0 0;\n2: 0 0 0 0;\n0: 1 9 0 0;\n0: 10 63 0 0;\n1: 1 63 0 0;\n2: 1 63 0 0;\n";
    // three bits held back in DC and in Y's AC, refined one by one
    const char* DeepApproximation =
        "0 1 2: 0 0 0 3;\n0 1 2: 0 0 3 2;\n0 1 2: 0 0 2 1;\n0 1 2: 0 0 1 0;\n"
        "0: 1 63 0 3;\n0: 1 63 3 2;\n0: 1 63 2 1;\n0: 1 63 1 0;\n1: 1 63 0 0;\n2: 1 63 0 0;\n";
    // a progression that never refines: DC and Y's AC a bit short
    const char* Incomplete = "0 1 2: 0 0 0 1;\n0: 1 63 0 1;\n1: 1 63 0 0;\n2: 1 63 0 0;\n";

    std::string with_script(const std::string& source, const char* script, const std::string& more) {
        const auto script_path = write_file("progression.txt", script);
        return cjpeg(source, "-scans '" + script_path + "' " + more, "progressive.jpg");
    }
}

TEST(CodecJpeg_Tests, ProgressiveCjpegFilesOfEveryKind) {
    if (c_oracle().empty() || djpeg_tool("cjpeg").empty()) {
        GTEST_SKIP() << "no C oracle or no cjpeg";
    }
    struct Kind { const char* name; const char* args; bool gray; };
    const Kind kinds[] = {
        {"444", "-progressive -sample 1x1", false},
        {"422", "-progressive -sample 2x1", false},
        {"440", "-progressive -sample 1x2", false},
        {"420", "-progressive -sample 2x2", false},
        {"411", "-progressive -sample 4x1", false},
        {"gray", "-progressive -grayscale", true},
        {"rgb", "-progressive -rgb", false},
        {"420 optimized", "-progressive -optimize -sample 2x2", false},
        {"420 restart 1 row", "-progressive -sample 2x2 -restart 1", false},
        {"422 restart 2 MCUs", "-progressive -sample 2x1 -restart 2B", false},
        {"gray restart 1 MCU", "-progressive -grayscale -restart 1B", true},
        {"q100", "-progressive -quality 100", false},
    };
    const std::pair<uint32_t, uint32_t> sizes[] = {{1, 1}, {2, 3}, {9, 17}, {17, 9}, {33, 31}, {64, 64}, {227, 149}};
    for (const auto& k : kinds) {
        for (auto [w, h] : sizes) {
            auto src = source_image(w, h, k.gray);
            if (!src) {
                GTEST_SKIP();
            }
            const auto path = cjpeg(write_file(k.gray ? "src.pgm" : "src.ppm", *src), k.args, "progressive.jpg");
            ASSERT_FALSE(path.empty()) << k.name;
            EXPECT_EQ(against_libjpeg(path, read_file(path)), "") << k.name << " " << w << "x" << h;
        }
    }
}

TEST(CodecJpeg_Tests, ProgressionScripts) {
    if (c_oracle().empty() || djpeg_tool("cjpeg").empty()) {
        GTEST_SKIP() << "no C oracle or no cjpeg";
    }
    struct Script { const char* name; const char* script; };
    const Script scripts[] = {{"default", DefaultProgression}, {"spectral only", SpectralOnly}, {"deep approximation", DeepApproximation}};
    for (const auto& sc : scripts) {
        for (const char* more : {"-sample 2x2", "-sample 1x1", "-sample 2x1 -restart 3B", "-sample 2x2 -optimize"}) {
            for (auto [w, h] : {std::pair{9u, 7u}, std::pair{64u, 48u}, std::pair{227u, 149u}}) {
                auto src = source_image(w, h, false);
                if (!src) {
                    GTEST_SKIP();
                }
                const auto path = with_script(write_file("src.ppm", *src), sc.script, more);
                ASSERT_FALSE(path.empty()) << sc.name;
                EXPECT_EQ(against_libjpeg(path, read_file(path)), "") << sc.name << " " << more << " " << w << "x" << h;
            }
        }
    }
}

TEST(CodecJpeg_Tests, AnIncompleteProgressionWithoutSmoothing) {
    // libjpeg-turbo smooths the blocks of a progression whose low AC
    // coefficients never become whole; the module does not: its pixels are
    // libjpeg's with do_block_smoothing off, and differ from its default
    if (c_oracle().empty() || djpeg_tool("cjpeg").empty()) {
        GTEST_SKIP() << "no C oracle or no cjpeg";
    }
    auto src = source_image(64, 48, false);
    if (!src) {
        GTEST_SKIP();
    }
    const auto path = with_script(write_file("src.ppm", *src), Incomplete, "-sample 2x2");
    ASSERT_FALSE(path.empty());
    const std::string data = read_file(path);
    EXPECT_EQ(against_libjpeg(path, data, "jpegns"), "");
    EXPECT_NE(against_libjpeg(path, data, "jpeg"), "");
}

TEST(CodecJpeg_Tests, TestimgpAndJpegtran) {
    // testimgp.jpg as libjpeg-turbo's own tests make it (it ships
    // testorig.ppm, not the file): cjpeg -dct int -progressive -opt; and
    // testorig.jpg made progressive by jpegtran, its coefficients as they are
    if (c_oracle().empty() || djpeg_tool("cjpeg").empty() || djpeg_tool("jpegtran").empty()) {
        GTEST_SKIP() << "no C oracle, cjpeg or jpegtran";
    }
    CODEC_ORACLE(ppm, TestImages + "testorig.ppm");
    const auto testimgp = cjpeg(oracle_path(TestImages + "testorig.ppm"), "-dct int -progressive -opt", "testimgp.jpg");
    ASSERT_FALSE(testimgp.empty());
    EXPECT_EQ(against_libjpeg(testimgp, read_file(testimgp)), "");
    const auto out = (scratch_dir() / "transcoded.jpg").string();
    const std::string cmd = "'" + djpeg_tool("jpegtran") + "' -progressive -outfile '" + out + "' '" + oracle_path(TestImages + "testorig.jpg") + "'";
    ASSERT_EQ(std::system(cmd.c_str()), 0);
    const std::string transcoded = read_file(out);
    EXPECT_EQ(against_libjpeg(out, transcoded), "");
    // the same coefficients: the same pixels as the sequential original
    CODEC_ORACLE(original, TestImages + "testorig.jpg");
    codec::image a = *codec::jpeg::decode(bytes(original));
    codec::image b = *codec::jpeg::decode(bytes(transcoded));
    ASSERT_EQ(a.pixels().size(), b.pixels().size());
    EXPECT_EQ(std::memcmp(a.pixels().data(), b.pixels().data(), a.pixels().size()), 0);
}

TEST(CodecJpeg_Tests, MotionJpegWithoutDht) {
    // a frame with no DHT (Motion-JPEG): the typical tables of Annex K.3,
    // which cjpeg writes when it does not optimize; taken out of the file,
    // libjpeg-turbo and the module both put them back
    if (c_oracle().empty() || djpeg_tool("cjpeg").empty()) {
        GTEST_SKIP() << "no C oracle or no cjpeg";
    }
    // (a progressive file has no such frame: cjpeg optimizes its tables,
    // Annex K has no codes for EOB runs)
    for (const char* args : {"-sample 2x2", "-sample 2x1 -restart 1", "-grayscale", "-sample 1x1 -quality 95"}) {
        const bool gray = std::string(args).find("gray") != std::string::npos;
        auto src = source_image(64, 48, gray);
        if (!src) {
            GTEST_SKIP();
        }
        const auto path = cjpeg(write_file(gray ? "src.pgm" : "src.ppm", *src), args, "mjpeg.jpg");
        ASSERT_FALSE(path.empty());
        const std::string data = read_file(path);
        std::string stripped;
        size_t removed = 0;
        for (size_t i = 0; i < data.size();) {
            if (i + 4 <= data.size() && uint8_t(data[i]) == 0xFF && uint8_t(data[i + 1]) == 0xC4) {
                i += 2 + (size_t(uint8_t(data[i + 2])) << 8 | uint8_t(data[i + 3]));
                ++removed;
                continue;
            }
            if (i + 1 < data.size() && uint8_t(data[i]) == 0xFF && uint8_t(data[i + 1]) == 0xDA) {
                stripped += data.substr(i);   // the scan's data as it is
                break;
            }
            stripped += data[i];
            ++i;
        }
        ASSERT_GT(removed, 0u) << args;
        const auto mjpeg = write_file("stripped.jpg", stripped);
        EXPECT_EQ(against_libjpeg(mjpeg, stripped), "") << args;
        codec::image with = *codec::jpeg::decode(bytes(data));
        codec::image without = *codec::jpeg::decode(bytes(stripped));
        ASSERT_EQ(with.pixels().size(), without.pixels().size());
        EXPECT_EQ(std::memcmp(with.pixels().data(), without.pixels().data(), with.pixels().size()), 0) << args;
    }
}

TEST(CodecJpeg_Tests, ProgressiveNegatives) {
    if (djpeg_tool("cjpeg").empty()) {
        GTEST_SKIP() << "no cjpeg";
    }
    auto src = source_image(32, 32, false);
    if (!src) {
        GTEST_SKIP();
    }
    const auto path = cjpeg(write_file("src.ppm", *src), "-progressive -sample 2x2", "negative.jpg");
    ASSERT_FALSE(path.empty());
    const std::string data = read_file(path);
    ASSERT_TRUE(codec::jpeg::decode(bytes(data)));
    // the SOS segments: the first (DC, three components) and the first AC one
    std::vector<size_t> sos;
    for (size_t i = 0; i + 1 < data.size(); ++i) {
        if (uint8_t(data[i]) == 0xFF && uint8_t(data[i + 1]) == 0xDA) {
            sos.push_back(i);
        }
    }
    ASSERT_GE(sos.size(), 2u);
    auto tail = [&](size_t at) { return at + 5 + 2 * size_t(uint8_t(data[at + 4])); };   // Ss
    auto decode = [](const std::string& d) { return code_of(codec::jpeg::decode(bytes(d))); };
    std::string bad = data;
    bad[tail(sos[0]) + 1] = 5;   // a DC scan with Se 5
    EXPECT_EQ(decode(bad), codec::errc::corrupt);
    bad = data;
    bad[tail(sos[1])] = 9;       // Ss 9 past Se
    bad[tail(sos[1]) + 1] = 3;
    EXPECT_EQ(decode(bad), codec::errc::corrupt);
    bad = data;
    bad[tail(sos[1]) + 2] = char(0x0E);   // Al 14
    EXPECT_EQ(decode(bad), codec::errc::corrupt);
    bad = data;
    bad[tail(sos[1]) + 1] = char(64);     // Se 64
    EXPECT_EQ(decode(bad), codec::errc::corrupt);
    // an AC band over the three components of the DC scan
    bad = data;
    bad[tail(sos[0])] = 1;
    bad[tail(sos[0]) + 1] = 63;
    EXPECT_EQ(decode(bad), codec::errc::corrupt);
    // every cut: unexpected_end, no partial image
    for (size_t n = 0; n + 1 < data.size(); ++n) {
        EXPECT_EQ(decode(data.substr(0, n)), codec::errc::unexpected_end) << n;
    }
}
