//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// PPMd var.H as 7z codes it (detail/ppmd7.h; the public face comes with
// sevenzip). The oracle is libarchive through bsdtar: it makes a .7z of one
// file with compression=ppmd, and the test takes the packed stream out by
// hand — it starts right after the 32-byte signature header; its size, the
// coder's properties (order and memory) and the size unpacked are read
// from the archive's header, which the signature header points to and
// bsdtar writes unencoded — decodes it and compares with the file, and
// codes the file itself and compares with libarchive's stream byte for
// byte. The rest is our encoder against our decoder.
#include "common.h"
#include "tests/source_root.h"

#include "sgcl/compress/detail/ppmd7.h"

#include <cstdlib>
#include <filesystem>

using namespace compress_test;
namespace cd = sgcl::compress::detail;

namespace {
    std::string repo_text(size_t limit) {
        auto root = source_root() / "sgcl";
        std::vector<std::filesystem::path> files;
        for (auto& e : std::filesystem::recursive_directory_iterator(root)) {
            if (e.path().extension() == ".h") {
                files.push_back(e.path());
            }
        }
        std::sort(files.begin(), files.end());
        std::string all;
        for (auto& f : files) {
            std::ifstream is(f, std::ios::binary);
            std::stringstream ss;
            ss << is.rdbuf();
            all += ss.str();
            if (all.size() >= limit) {
                break;
            }
        }
        all.resize(std::min(all.size(), limit));
        return all;
    }

    std::string random_bytes(size_t n, uint32_t seed) {
        std::mt19937 rng(seed);
        std::string s(n, 0);
        for (auto& c : s) {
            c = char(rng());
        }
        return s;
    }

    std::string encode(const std::string& t, uint32_t order, uint32_t memory, bool marker = false, uint32_t* restarts = nullptr) {
        auto e = std::make_unique<cd::Ppmd7Encoder>(order, memory);
        std::vector<uint8_t> c;
        e->encode(reinterpret_cast<const uint8_t*>(t.data()), t.size(), c);
        e->finish(c, marker);
        if (restarts) {
            *restarts = e->model().restarts;
        }
        return std::string(c.begin(), c.end());
    }

    // The data decoded to `size` bytes (UINT64_MAX: to the end marker)
    // in one call; the status and the error's code
    std::string decode(const std::string& c, uint32_t order, uint32_t memory, uint64_t size, cd::LzmaStatus& st, sgcl::compress::errc* code = nullptr) {
        auto d = std::make_unique<cd::Ppmd7Decoder>();
        d->reset(order, memory, size);
        std::string out(size == UINT64_MAX ? c.size() * 20 + 64 : size_t(size), 0);
        size_t pos = 0;
        auto in = reinterpret_cast<const uint8_t*>(c.data());
        st = d->decode(reinterpret_cast<uint8_t*>(out.data()), pos, out.size(), in, in + c.size(), true);
        if (code) {
            *code = d->error;
        }
        out.resize(pos);
        return out;
    }

    // The same fed `feed` bytes at a time, the output in pieces of `piece`
    std::string decode_in_pieces(const std::string& c, uint32_t order, uint32_t memory, uint64_t size, size_t feed, size_t piece, bool& ok) {
        auto d = std::make_unique<cd::Ppmd7Decoder>();
        d->reset(order, memory, size);
        std::string out(size, 0);
        std::vector<uint8_t> buf;
        size_t given = 0, pos = 0;
        for (;;) {
            const uint8_t* in = buf.data();
            auto st = d->decode(reinterpret_cast<uint8_t*>(out.data()), pos, std::min<size_t>(size, pos + piece), in, buf.data() + buf.size(), given == c.size());
            buf.erase(buf.begin(), buf.begin() + (in - buf.data()));
            if (st == cd::LzmaStatus::done || st == cd::LzmaStatus::failed) {
                ok = st == cd::LzmaStatus::done;
                out.resize(pos);
                return out;
            }
            if (st == cd::LzmaStatus::need_input) {
                size_t k = std::min(feed, c.size() - given);
                buf.insert(buf.end(), c.begin() + given, c.begin() + given + k);
                given += k;
            }
        }
    }

    // A number of 7z's header: the leading 1 bits of the first byte count
    // the bytes after it (little-endian), the rest of it is the top
    uint64_t number(const std::string& h, size_t& at) {
        uint8_t first = uint8_t(h.at(at++));
        uint64_t v = 0;
        uint8_t mask = 0x80;
        for (int i = 0; i < 8; ++i, mask >>= 1) {
            if (!(first & mask)) {
                return v | (uint64_t(first & (mask - 1)) << (8 * i));
            }
            v |= uint64_t(uint8_t(h.at(at++))) << (8 * i);
        }
        return v;
    }

    struct Packed {
        std::string stream;
        uint32_t order = 0;
        uint32_t memory = 0;
        uint64_t size = 0;
    };

    // The one packed stream of a .7z bsdtar made of one file with PPMd
    bool unpack_7z(const std::string& a, Packed& p) {
        if (a.size() < 32 || a.compare(0, 6, "7z\xBC\xAF\x27\x1C", 6) != 0) {
            return false;
        }
        uint64_t offset = 0, size = 0;
        for (int i = 0; i < 8; ++i) {
            offset |= uint64_t(uint8_t(a[12 + i])) << (8 * i);
            size |= uint64_t(uint8_t(a[20 + i])) << (8 * i);
        }
        std::string h = a.substr(32 + offset, size);
        size_t at = 0;
        // kHeader, kMainStreamsInfo, kPackInfo: position, one stream, kSize
        if (h.at(at++) != 0x01 || h.at(at++) != 0x04 || h.at(at++) != 0x06) {
            return false;
        }
        uint64_t pos = number(h, at);
        if (number(h, at) != 1 || h.at(at++) != 0x09) {
            return false;
        }
        uint64_t packed = number(h, at);
        while (h.at(at) != 0x00) {
            ++at;   // the pack stream's CRC, when there is one
        }
        ++at;
        // kUnpackInfo, kFolder: one folder of one coder, 03 04 01 with 5 bytes of properties
        if (h.at(at++) != 0x07 || h.at(at++) != 0x0B || number(h, at) != 1 || h.at(at++) != 0 || number(h, at) != 1) {
            return false;
        }
        uint8_t flags = uint8_t(h.at(at++));
        if ((flags & 0x0F) != 3 || h.compare(at, 3, "\x03\x04\x01", 3) != 0 || !(flags & 0x20)) {
            return false;
        }
        at += 3;
        if (number(h, at) != 5) {
            return false;
        }
        p.order = uint8_t(h[at]);
        p.memory = uint32_t(uint8_t(h[at + 1])) | uint32_t(uint8_t(h[at + 2])) << 8 | uint32_t(uint8_t(h[at + 3])) << 16 | uint32_t(uint8_t(h[at + 4])) << 24;
        at += 5;
        if (h.at(at++) != 0x0C) {
            return false;
        }
        p.size = number(h, at);
        p.stream = a.substr(32 + pos, packed);
        return true;
    }
}

// libarchive's streams decoded here, and ours equal to them byte for
// byte: text, a program, zeros, one byte, and random data (which spends
// the 16 MiB model in a few MB: the allocator glues, the model restarts)
TEST(Ppmd_Tests, LibarchiveStreamsBothWays) {
    if (!have_bsdtar()) {
        GTEST_SKIP() << "bsdtar not found";
    }
    scratch_dir scratch("sgcl-ppmd");   // removed when the test ends
    const auto& dir = scratch.path();
    std::string prog;
    for (auto f : {"/bin/zsh", "/bin/bash", "/bin/ls"}) {
        prog = slurp(f);
        if (!prog.empty()) {
            break;
        }
    }
    std::vector<std::pair<std::string, std::string>> inputs = {
        {"text", repo_text(size_t(2) << 20)},
        {"program", prog},
        {"zeros", std::string(200000, '\0')},
        {"one", "x"},
        {"random", random_bytes(size_t(3) << 20, 5)},
    };
    for (auto& [name, t] : inputs) {
        auto file = dir / name;
        std::ofstream(file, std::ios::binary).write(t.data(), std::streamsize(t.size()));
        auto archive = dir / (name + ".7z");
        std::filesystem::remove(archive);
        std::string cmd = "cd '" + dir.string() + "' && bsdtar --format 7zip --options 7zip:compression=ppmd -cf '" + archive.string() + "' '" + name + "'";
        ASSERT_EQ(std::system(cmd.c_str()), 0) << cmd;
        Packed p;
        ASSERT_TRUE(unpack_7z(slurp(archive), p)) << name;
        EXPECT_EQ(p.order, 6u);
        EXPECT_EQ(p.memory, uint32_t(16) << 20);
        ASSERT_EQ(p.size, t.size());
        cd::LzmaStatus st;
        auto back = decode(p.stream, p.order, p.memory, p.size, st);
        ASSERT_EQ(st, cd::LzmaStatus::done) << name;
        ASSERT_EQ(back, t) << name;
        uint32_t restarts = 0;
        auto ours = encode(t, p.order, p.memory, false, &restarts);
        ASSERT_EQ(ours, p.stream) << name << ": our stream differs from libarchive's";
        if (name == "random") {
            EXPECT_GT(restarts, 1u);
        }
    }
}

// Every order and memory from the smallest (the model restarting every few
// hundred bytes) to 16 MiB, on text, a program, random bytes and runs
TEST(Ppmd_Tests, RoundTripsAtEveryOrderAndMemory) {
    auto text = repo_text(300000);
    auto mixed = text.substr(0, 100000) + random_bytes(50000, 2) + std::string(30000, 'a') + text.substr(100000, 50000);
    for (auto* t : {&text, &mixed}) {
        for (uint32_t order : {2u, 3u, 4u, 6u, 8u, 16u, 32u, 64u}) {
            for (uint32_t memory : {cd::ppmd::MinMemory, uint32_t(1) << 14, uint32_t(1) << 18, uint32_t(16) << 20}) {
                uint32_t restarts = 0;
                auto c = encode(*t, order, memory, false, &restarts);
                cd::LzmaStatus st;
                auto back = decode(c, order, memory, t->size(), st);
                ASSERT_EQ(st, cd::LzmaStatus::done) << "order " << order << " memory " << memory;
                ASSERT_EQ(back, *t) << "order " << order << " memory " << memory;
                if (memory == cd::ppmd::MinMemory) {
                    EXPECT_GT(restarts, 10u) << order;
                }
            }
        }
    }
    for (std::string t : {std::string(), std::string("a"), std::string(100000, '\0'), random_bytes(70000, 3)}) {
        auto c = encode(t, 6, 1 << 20);
        cd::LzmaStatus st;
        EXPECT_EQ(decode(c, 6, 1 << 20, t.size(), st), t);
        EXPECT_EQ(st, cd::LzmaStatus::done);
    }
}

// Fed a byte at a time (and 2..7), the output in pieces of 1..7 and more:
// what one call makes
TEST(Ppmd_Tests, PiecesOfAnySize) {
    auto t = repo_text(40000) + random_bytes(5000, 4);
    for (uint32_t memory : {cd::ppmd::MinMemory, uint32_t(1) << 20}) {
        auto c = encode(t, 8, memory);
        for (size_t feed = 1; feed <= 7; ++feed) {
            for (size_t piece : {feed, size_t(4096)}) {
                bool ok = false;
                auto back = decode_in_pieces(c, 8, memory, t.size(), feed, piece, ok);
                ASSERT_TRUE(ok) << feed << " " << piece;
                ASSERT_EQ(back, t) << feed << " " << piece;
            }
        }
    }
}

// The end marker: read to it when the size is not known; before the size
// known it is corrupt
TEST(Ppmd_Tests, TheEndMarker) {
    auto t = read_oracle("compress/e.txt");
    auto c = encode(t, 6, 1 << 20, true);
    cd::LzmaStatus st;
    EXPECT_EQ(decode(c, 6, 1 << 20, UINT64_MAX, st), t);
    EXPECT_EQ(st, cd::LzmaStatus::done);
    sgcl::compress::errc code;
    decode(c, 6, 1 << 20, t.size() + 10, st, &code);
    EXPECT_EQ(st, cd::LzmaStatus::failed);
    EXPECT_EQ(code, sgcl::compress::errc::corrupt);
}

// Cut short: unexpected_end or corrupt; flipped bits: an error or other
// bytes, never a crash, and the pieces agree with the whole
TEST(Ppmd_Tests, DamagedDataFails) {
    auto t = read_oracle("compress/gettysburg.txt");
    auto c = encode(t, 6, 1 << 16);
    for (size_t n = 0; n < c.size(); ++n) {
        cd::LzmaStatus st;
        sgcl::compress::errc code;
        decode(c.substr(0, n), 6, 1 << 16, t.size(), st, &code);
        ASSERT_EQ(st, cd::LzmaStatus::failed) << n;
        EXPECT_TRUE(code == sgcl::compress::errc::unexpected_end || code == sgcl::compress::errc::corrupt) << n;
    }
    std::mt19937 rng(6);
    for (int i = 0; i < 1000; ++i) {
        auto d = c;
        size_t at = rng() % d.size();
        d[at] = char(d[at] ^ (1 << (rng() % 8)));
        cd::LzmaStatus st;
        auto a = decode(d, 6, 1 << 16, t.size(), st);
        bool ok = false;
        auto b = decode_in_pieces(d, 6, 1 << 16, t.size(), 5, 77, ok);
        EXPECT_EQ(ok, st == cd::LzmaStatus::done);
        EXPECT_EQ(a, b);
    }
    auto bad = c;
    bad[0] = 1;
    cd::LzmaStatus st;
    sgcl::compress::errc code;
    decode(bad, 6, 1 << 16, t.size(), st, &code);
    EXPECT_EQ(st, cd::LzmaStatus::failed);
    EXPECT_EQ(code, sgcl::compress::errc::corrupt);
}
