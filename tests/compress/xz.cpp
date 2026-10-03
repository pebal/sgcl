//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// xz, LZMA2, the branch converters, Delta and BCJ2. liblzma (xz's library)
// is the oracle both ways: its streams of every preset, check and filter
// chain decoded here, ours decoded there; its converters' output (a raw
// stream of [filter, LZMA2], its LZMA2 undone by us) compared byte for byte
// with ours. BCJ2 has no oracle in this build (libarchive reads it only
// inside 7z, which comes later): our encoder against our decoder, and a
// vector worked by hand from the description.
#include "common.h"

#include "sgcl/compress/detail/bcj2.h"

#include <filesystem>

#ifndef SGCL_TEST_LIBLZMA
#define SGCL_TEST_LIBLZMA 0
#endif
#if SGCL_TEST_LIBLZMA
#include <lzma.h>
#endif

using namespace compress_test;
using compress::xz;
namespace cd = sgcl::compress::detail;

namespace {
    std::string repo_text(size_t limit) {
        auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "sgcl";
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

    // A program of the system: machine code of x86-64 and ARM64 (macOS's
    // are universal), the first `limit` bytes
    std::string program(size_t limit) {
        for (auto f : {"/bin/zsh", "/bin/bash", "/usr/bin/xz", "/bin/ls"}) {
            std::ifstream is(f, std::ios::binary);
            if (is) {
                std::stringstream ss;
                ss << is.rdbuf();
                auto s = ss.str();
                if (s.size() > 100000) {
                    s.resize(std::min(s.size(), limit));
                    return s;
                }
            }
        }
        return {};
    }

    // Random bytes with the instruction patterns of every converter planted
    // thick: E8/E9 with near operands, ARM's EB, Thumb's F0/F8, ARM64's bl
    // and adrp, PowerPC's bl, SPARC's call, RISC-V's jal and auipc pairs
    // (and auipc x0/x2 that the encoder must swap), IA-64 branch bundles
    std::string branches(size_t n, uint32_t seed) {
        std::mt19937 rng(seed);
        std::string s(n, 0);
        for (auto& c : s) {
            c = char(rng());
        }
        auto put32le = [&](size_t at, uint32_t v) {
            for (int i = 0; i < 4; ++i) s[at + i] = char(v >> (8 * i));
        };
        for (size_t at = 0; at + 16 <= n; at += 2 + rng() % 13) {
            switch (rng() % 12) {
                case 0: s[at] = char(0xE8); s[at + 4] = char(rng() % 2 ? 0 : 0xFF); break;
                case 1: s[at] = char(0xE9); s[at + 4] = 0; break;
                case 2: s[at + 3] = char(0xEB); break;
                case 3: s[at + 1] = char(0xF0 | (rng() & 7)); s[at + 3] = char(0xF8 | (rng() & 7)); break;
                case 4: put32le(at, 0x94000000 | (rng() & 0x03FFFFFF)); break;
                case 5: put32le(at, 0x90000000 | (rng() & 0x60FFFFFF) | ((rng() & 1) ? 0 : 0x00E00000)); break;
                case 6: s[at] = char(0x48 | (rng() & 3)); s[at + 3] = char((rng() & 0xFC) | 1); break;
                case 7: s[at] = 0x40; s[at + 1] = char(rng() & 0x3F); break;
                case 8: s[at] = char(0xEF); s[at + 1] = char((rng() & 0xF0) | (rng() % 2 ? 0 : 2)); break;
                case 9: {
                    uint32_t rd = 1 + rng() % 31;
                    put32le(at, 0x17 | (rd << 7) | (rng() & 0xFFFFF000));
                    put32le(at + 4, 0x13 | (rd << 15) | (rng() & 0xFFF07F80));
                    break;
                }
                case 10: {
                    uint32_t rd = rng() % 2 ? 0 : 2;
                    put32le(at, 0x3017 | (rd << 7) | (rng() & 0xFFFFC000));
                    break;
                }
                case 11: s[at & ~size_t(15)] = char(0x10 + rng() % 14); break;
            }
        }
        return s;
    }

    std::string buffer_text(const sgcl::io::buffer& b) {
        return std::string(reinterpret_cast<const char*>(b.data().data()), b.size());
    }

    template<class R>
    bool read_everything(R& r, size_t step, std::string& out) {
        std::vector<std::byte> buf(step);
        out.clear();
        for (;;) {
            auto n = r.read(sgcl::slice<std::byte>(buf.data(), buf.size()));
            if (!n) {
                return false;
            }
            if (*n == 0) {
                return true;
            }
            out.append(reinterpret_cast<const char*>(buf.data()), *n);
        }
    }

    // LZMA2 data decoded by our decoder in memory (the dictionary given)
    bool lzma2_decode(const std::string& in, uint32_t dictionary, std::string& out) {
        cd::Lzma2Decoder d;
        d.reset(dictionary);
        static std::vector<uint8_t> w(64 << 20);   // taken once: the cut-short cases call this thousands of times
        size_t pos = 0;
        auto p = reinterpret_cast<const uint8_t*>(in.data());
        for (;;) {
            auto st = d.decode<true>(w.data(), w.size(), pos, w.size() - cd::LzmaDecoder::CopySlack, p, reinterpret_cast<const uint8_t*>(in.data()) + in.size(), true);
            if (st == cd::LzmaStatus::done) {
                out.assign(reinterpret_cast<const char*>(w.data()), pos);
                return true;
            }
            if (st != cd::LzmaStatus::need_room) {
                return false;
            }
            return false;
        }
    }

    std::string lzma2_encode(const std::string& in, int level) {
        auto s = cd::LzmaEncoderSettings::of(level, false);
        cd::Lzma2Encoder e(s);
        e.attach(reinterpret_cast<const uint8_t*>(in.data()), in.size());
        std::vector<uint8_t> out;
        (void)e.run(true, out);
        e.finish(out);
        return std::string(out.begin(), out.end());
    }

    struct Kind {
        const char* name;
        cd::SimpleKind kind;
        std::optional<xz::filter> filter;
        uint64_t id;
    };

    const std::vector<Kind>& kinds() {
        static const std::vector<Kind> k = {
            {"x86", cd::SimpleKind::x86, xz::filter::x86, 0x04},
            {"powerpc", cd::SimpleKind::powerpc, xz::filter::powerpc, 0x05},
            {"ia64", cd::SimpleKind::ia64, xz::filter::ia64, 0x06},
            {"arm", cd::SimpleKind::arm, xz::filter::arm, 0x07},
            {"armt", cd::SimpleKind::armt, xz::filter::armt, 0x08},
            {"sparc", cd::SimpleKind::sparc, xz::filter::sparc, 0x09},
            {"arm64", cd::SimpleKind::arm64, xz::filter::arm64, 0x0A},
            {"riscv", cd::SimpleKind::riscv, xz::filter::riscv, 0x0B},
        };
        return k;
    }

    std::string apply(cd::SimpleKind k, bool encoder, std::string s, uint32_t start = 0, uint32_t distance = 1) {
        cd::SimpleFilter f;
        f.init(k, encoder, start, distance);
        f.run(reinterpret_cast<uint8_t*>(s.data()), s.size());
        return s;
    }

    // The same through the chain buffer, fed `step` bytes at a time
    std::string apply_in_pieces(cd::SimpleKind k, bool encoder, const std::string& s, size_t step, uint32_t distance = 1) {
        cd::FilterChain chain;
        cd::SimpleFilter f;
        f.init(k, encoder, 0, distance);
        chain.add(f);
        std::string out;
        for (size_t i = 0; i < s.size(); i += step) {
            size_t k2 = std::min(step, s.size() - i);
            (void)chain.room();
            chain.push(reinterpret_cast<const uint8_t*>(s.data()) + i, k2);
            chain.run(false);
            out.append(reinterpret_cast<const char*>(chain.ready()), chain.ready_size());
            chain.take(chain.ready_size());
        }
        chain.run(true);
        out.append(reinterpret_cast<const char*>(chain.ready()), chain.ready_size());
        return out;
    }

#if SGCL_TEST_LIBLZMA
    std::string xz_run(lzma_stream& s, const std::string& in, bool& ok) {
        std::string out;
        std::string buf(1 << 16, 0);
        s.next_in = reinterpret_cast<const uint8_t*>(in.data());
        s.avail_in = in.size();
        lzma_ret r;
        do {
            s.next_out = reinterpret_cast<uint8_t*>(buf.data());
            s.avail_out = buf.size();
            r = lzma_code(&s, LZMA_FINISH);
            out.append(buf.data(), buf.size() - s.avail_out);
        } while (r == LZMA_OK);
        ok = r == LZMA_STREAM_END;
        lzma_end(&s);
        return out;
    }

    bool xz_decode(const std::string& in, std::string& out) {
        lzma_stream s = LZMA_STREAM_INIT;
        if (lzma_stream_decoder(&s, UINT64_MAX, LZMA_CONCATENATED) != LZMA_OK) {
            return false;
        }
        bool ok;
        out = xz_run(s, in, ok);
        return ok;
    }

    std::string xz_easy(const std::string& in, uint32_t preset, lzma_check check = LZMA_CHECK_CRC64) {
        lzma_stream s = LZMA_STREAM_INIT;
        EXPECT_EQ(lzma_easy_encoder(&s, preset, check), LZMA_OK);
        bool ok;
        auto out = xz_run(s, in, ok);
        EXPECT_TRUE(ok);
        return out;
    }

    // A chain of [filter (start offset), delta (distance, 0: none), LZMA2 at the preset]
    std::string xz_filtered(const std::string& in, uint64_t filter, uint32_t delta, uint32_t preset = 6, bool raw = false, uint32_t start = 0) {
        lzma_options_lzma lz;
        lzma_lzma_preset(&lz, preset);
        lzma_options_bcj bcj{};
        bcj.start_offset = start;
        lzma_options_delta dl{};
        dl.type = LZMA_DELTA_TYPE_BYTE;
        dl.dist = delta;
        lzma_filter chain[4];
        int n = 0;
        if (filter) {
            chain[n++] = {filter, start ? &bcj : nullptr};
        }
        if (delta) {
            chain[n++] = {LZMA_FILTER_DELTA, &dl};
        }
        chain[n++] = {LZMA_FILTER_LZMA2, &lz};
        chain[n] = {LZMA_VLI_UNKNOWN, nullptr};
        lzma_stream s = LZMA_STREAM_INIT;
        EXPECT_EQ(raw ? lzma_raw_encoder(&s, chain) : lzma_stream_encoder(&s, chain, LZMA_CHECK_CRC32), LZMA_OK);
        bool ok;
        auto out = xz_run(s, in, ok);
        EXPECT_TRUE(ok);
        return out;
    }

    std::string xz_raw_lzma2_decode(const std::string& in, bool& ok) {
        lzma_options_lzma lz;
        lzma_lzma_preset(&lz, 6);
        lz.dict_size = uint32_t(64) << 20;
        lzma_filter chain[2] = {{LZMA_FILTER_LZMA2, &lz}, {LZMA_VLI_UNKNOWN, nullptr}};
        lzma_stream s = LZMA_STREAM_INIT;
        EXPECT_EQ(lzma_raw_decoder(&s, chain), LZMA_OK);
        return xz_run(s, in, ok);
    }
#endif

#define SKIP_WITHOUT_LIBLZMA() \
    if (!SGCL_TEST_LIBLZMA) GTEST_SKIP() << "liblzma not found (SGCL_XZ_ROOT)"
}

// ---------------------------------------------------------------- LZMA2

// Our chunks decoded by liblzma, liblzma's by us: text (LZMA chunks of 2
// MiB), random bytes (stored chunks), and the two mixed (a stored chunk
// between LZMA ones: the state reset after it)
TEST(Lzma2_Tests, RawBothWaysWithLiblzma) {
    SKIP_WITHOUT_LIBLZMA();
#if SGCL_TEST_LIBLZMA
    std::mt19937 rng(1);
    std::string noise(300000, 0);
    for (auto& c : noise) c = char(rng());
    auto text = repo_text(size_t(5) << 20);
    std::string mixed = text.substr(0, 200000) + noise + text.substr(200000, 200000) + noise.substr(0, 70000) + text.substr(0, 3000);
    for (auto* t : {&text, &noise, &mixed}) {
        for (int level : {0, 3, 6}) {
            auto ours = lzma2_encode(*t, level);
            bool ok;
            auto back = xz_raw_lzma2_decode(ours, ok);
            ASSERT_TRUE(ok) << "level " << level << " size " << t->size();
            ASSERT_EQ(back, *t);
            std::string mine;
            ASSERT_TRUE(lzma2_decode(ours, uint32_t(64) << 20, mine));
            ASSERT_EQ(mine, *t);
        }
        auto theirs = xz_filtered(*t, 0, 0, 6, true);
        std::string mine;
        ASSERT_TRUE(lzma2_decode(theirs, uint32_t(8) << 20, mine));
        ASSERT_EQ(mine, *t);
    }
#endif
}

TEST(Lzma2_Tests, InvalidChunksFail) {
    std::string out;
    // a control byte of 3..0x7F
    EXPECT_FALSE(lzma2_decode(std::string("\x03", 1), 4096, out));
    // the first chunk not resetting the dictionary (stored 2, LZMA 0x80..0xDF)
    EXPECT_FALSE(lzma2_decode(std::string("\x02\x00\x00x\x00", 5), 4096, out));
    // an LZMA chunk before any properties (a stored chunk resets the dictionary only)
    EXPECT_FALSE(lzma2_decode(std::string("\x01\x00\x00x\xA0\x00\x00\x00\x05\x00\x00\x00\x00\x00\x00", 15), 4096, out));
    // after a dictionary reset in a stored chunk, an LZMA chunk that keeps
    // the state (and so its repeated distances, which would reach past the
    // reset): new properties are required, as liblzma requires them (the
    // fuzzer's first finding: a copy from before the window's start)
    auto first = lzma2_encode("abcabcabcabcabcabc", 6);
    first.pop_back();   // the end byte
    std::string again = first.substr(0, 1) == "\xE0" ? "\x80" + first.substr(1, 4) : "";
    ASSERT_FALSE(again.empty());
    EXPECT_FALSE(lzma2_decode(first + std::string("\x01\x00\x00x", 4) + again + first.substr(6) + std::string(1, '\0'), 4096, out));
    EXPECT_TRUE(lzma2_decode(first + std::string("\x02\x00\x00x", 4) + std::string(1, '\0'), 4096, out));
    EXPECT_EQ(out, "abcabcabcabcabcabcx");
    // properties with lc + lp past 4
    EXPECT_FALSE(lzma2_decode(std::string("\xE0\x00\x00\x00\x05\x2D\x00\x00\x00\x00\x00\x00", 12), 4096, out));
    // a stored chunk and the end
    ASSERT_TRUE(lzma2_decode(std::string("\x01\x00\x02" "abc" "\x02\x00\x00" "d" "\x00", 11), 4096, out));
    EXPECT_EQ(out, "abcd");
    // cut short, anywhere
    auto c = lzma2_encode(read_oracle("compress/e.txt"), 6);
    for (size_t n = 0; n < c.size(); n += 7) {
        EXPECT_FALSE(lzma2_decode(c.substr(0, n), 1 << 20, out)) << n;
    }
}

// ---------------------------------------------------------------- BCJ

// Our encoders against liblzma's byte for byte (its raw [filter, LZMA2]
// stream, the LZMA2 undone by us), and our decoders taking it back; on a
// real program (x86-64 and ARM64 code) and on random data thick with every
// processor's patterns, with a start offset too
TEST(Bcj_Tests, EveryConverterMatchesLiblzma) {
    SKIP_WITHOUT_LIBLZMA();
#if SGCL_TEST_LIBLZMA
    auto prog = program(size_t(3) << 20);
    for (auto& k : kinds()) {
        for (uint32_t seed : {1u, 2u, 3u}) {
            auto data = seed == 3 && !prog.empty() ? prog : branches(200003 + seed, seed);
            for (uint32_t start : {0u, 64u}) {
                auto raw = xz_filtered(data, k.id, 0, 1, true, start);
                std::string filtered;
                ASSERT_TRUE(lzma2_decode(raw, uint32_t(64) << 20, filtered)) << k.name;
                auto ours = apply(k.kind, true, data, start);
                ASSERT_EQ(ours.size(), filtered.size());
                size_t diff = std::mismatch(ours.begin(), ours.end(), filtered.begin()).first - ours.begin();
                ASSERT_EQ(ours, filtered) << k.name << " seed " << seed << " start " << start << ": first difference at " << diff;
                ASSERT_EQ(apply(k.kind, false, filtered, start), data) << k.name << " seed " << seed;
            }
        }
    }
#endif
}

// A converter fed in pieces of 1..7 bytes (and larger) makes what it makes
// over the whole: x86's state (the mask of the opcodes seen, the position
// of the last) goes over every boundary; the others keep their last bytes
TEST(Bcj_Tests, PiecesOfAnySizeMakeTheWhole) {
    auto data = branches(40000, 9);
    for (auto& k : kinds()) {
        auto enc = apply(k.kind, true, data);
        auto dec = apply(k.kind, false, enc);
        ASSERT_EQ(dec, data) << k.name;
        for (size_t step : {1, 2, 3, 4, 5, 6, 7, 13, 4096}) {
            ASSERT_EQ(apply_in_pieces(k.kind, true, data, step), enc) << k.name << " step " << step;
            ASSERT_EQ(apply_in_pieces(k.kind, false, enc, step), data) << k.name << " step " << step;
        }
    }
}

// Throughput of the converters is measured in bench_lzma; here, that a
// run of bytes with no instruction goes through unchanged
TEST(Bcj_Tests, DataWithoutBranchesIsUnchanged) {
    std::string zeros(100000, '\0');
    for (auto& k : kinds()) {
        EXPECT_EQ(apply(k.kind, true, zeros), zeros) << k.name;
    }
}

// ---------------------------------------------------------------- Delta

TEST(Delta_Tests, BothWaysAndWithLiblzma) {
    std::string wave;
    for (int i = 0; i < 100000; ++i) {
        wave += char(int(100 * std::sin(i * 0.01)) + (i % 4) * 7);
    }
    for (uint32_t d : {1u, 2u, 4u, 7u, 256u}) {
        auto enc = apply(cd::SimpleKind::delta, true, wave, 0, d);
        EXPECT_EQ(apply(cd::SimpleKind::delta, false, enc, 0, d), wave);
        for (size_t step : {1, 3, 7, 1000}) {
            EXPECT_EQ(apply_in_pieces(cd::SimpleKind::delta, true, wave, step, d), enc);
        }
#if SGCL_TEST_LIBLZMA
        auto theirs = xz_filtered(wave, 0, d);
        auto back = xz::decompress(bytes(theirs));
        ASSERT_TRUE(back) << back.error().message();
        EXPECT_EQ(text(*back), wave);
        auto ours = xz::compress(bytes(wave), {.delta = uint16_t(d)});
        std::string x;
        ASSERT_TRUE(xz_decode(text(ours), x)) << d;
        EXPECT_EQ(x, wave);
#endif
    }
}

// ---------------------------------------------------------------- BCJ2

// A vector worked by hand: 90 E8 10000000 90 — the call's operand 0x10 at
// position 2, its target 0x10 + 6 = 0x16 into the call stream, big-endian;
// the range coder's one bit 1 at probability 1024: low 0x7FFFFC00, range
// 0x800003FF, then the five bytes of the flush: 00 7F FF FC 00
TEST(Bcj2_Tests, AVectorWorkedByHand) {
    const uint8_t data[] = {0x90, 0xE8, 0x10, 0x00, 0x00, 0x00, 0x90};
    auto s = cd::bcj2_encode(data, sizeof(data));
    EXPECT_EQ(s.main, (std::vector<uint8_t>{0x90, 0xE8, 0x90}));
    EXPECT_EQ(s.call, (std::vector<uint8_t>{0x00, 0x00, 0x00, 0x16}));
    EXPECT_TRUE(s.jump.empty());
    EXPECT_EQ(s.rc, (std::vector<uint8_t>{0x00, 0x7F, 0xFF, 0xFC, 0x00}));
    cd::Bcj2Decoder d;
    d.init(s.main.data(), s.main.size(), s.call.data(), s.call.size(), s.jump.data(), s.jump.size(), s.rc.data(), s.rc.size(), sizeof(data));
    uint8_t out[16];
    ASSERT_EQ(d.decode(out, sizeof(out)), sizeof(data));
    EXPECT_TRUE(d.done());
    EXPECT_EQ(std::memcmp(out, data, sizeof(data)), 0);
}

// Our encoder and our decoder: a program, random data thick with E8/E9 and
// 0F 8x, an opcode as the last byte and with fewer than four after it;
// the decoder giving its output in pieces of every size
TEST(Bcj2_Tests, BothWaysInPieces) {
    std::vector<std::string> inputs = {program(size_t(1) << 20), branches(100000, 4), std::string("\xE8", 1), std::string("ab\xE8\x01\x02", 5), std::string("\x0F\x85\x00\x00\x00\x00\xE9", 7), std::string()};
    std::string jcc;
    std::mt19937 rng(8);
    for (int i = 0; i < 20000; ++i) {
        jcc += char(rng() % 3 ? 0x0F : rng());
        jcc += char(0x80 + rng() % 16);
    }
    inputs.push_back(jcc);
    for (auto& t : inputs) {
        auto p = reinterpret_cast<const uint8_t*>(t.data());
        auto s = cd::bcj2_encode(p, t.size());
        EXPECT_LE(s.main.size(), t.size());
        for (size_t piece : {size_t(1), size_t(3), size_t(4), size_t(5), size_t(7), size_t(1) << 20}) {
            cd::Bcj2Decoder d;
            d.init(s.main.data(), s.main.size(), s.call.data(), s.call.size(), s.jump.data(), s.jump.size(), s.rc.data(), s.rc.size(), t.size());
            std::string out;
            std::vector<uint8_t> buf(piece);
            for (;;) {
                size_t k = d.decode(buf.data(), piece);
                if (!k) {
                    break;
                }
                out.append(reinterpret_cast<const char*>(buf.data()), k);
            }
            ASSERT_FALSE(d.failed()) << d.error_text;
            ASSERT_TRUE(d.done());
            ASSERT_EQ(out, t) << "piece " << piece;
        }
    }
}

TEST(Bcj2_Tests, DamagedStreamsFailOrDiffer) {
    auto t = branches(20000, 5);
    auto s = cd::bcj2_encode(reinterpret_cast<const uint8_t*>(t.data()), t.size());
    auto run = [&](const cd::Bcj2Streams& x, uint64_t size) {
        cd::Bcj2Decoder d;
        d.init(x.main.data(), x.main.size(), x.call.data(), x.call.size(), x.jump.data(), x.jump.size(), x.rc.data(), x.rc.size(), size);
        std::vector<uint8_t> out(size + 8);
        size_t n = 0;
        while (size_t k = d.decode(out.data() + n, size - n)) {
            n += k;
        }
        return d.done();
    };
    auto cut = s;
    cut.call.resize(cut.call.size() / 2);
    EXPECT_FALSE(run(cut, t.size()));
    cut = s;
    cut.main.resize(cut.main.size() - 1);
    EXPECT_FALSE(run(cut, t.size()));
    cut = s;
    cut.rc.resize(3);
    EXPECT_FALSE(run(cut, t.size()));
    cut = s;
    cut.rc[0] = 1;
    EXPECT_FALSE(run(cut, t.size()));
}

// ---------------------------------------------------------------- xz

// Text compresses as it stands, as lzma's does: a literal, a character
// array, a std::string_view and a string make the same bytes
TEST(Xz_Tests, TextAsItStands) {
    static_assert(noexcept(xz::compress("text")));
    static_assert(!noexcept(xz::compress("text", xz::options{})));
    auto want = xz::compress(sgcl::string("hello, hello, hello"));
    const char array[] = "hello, hello, hello";
    EXPECT_TRUE(xz::compress("hello, hello, hello") == want);
    EXPECT_TRUE(xz::compress(array) == want);
    EXPECT_TRUE(xz::compress(std::string_view("hello, hello, hello")) == want);
    EXPECT_TRUE(xz::compress("hello, hello, hello", {.check = xz::check::sha256}) ==
                xz::compress(sgcl::string("hello, hello, hello"), {.check = xz::check::sha256}));
    EXPECT_EQ(text(value_of(xz::decompress(xz::compress("hello")))), "hello");
    EXPECT_THROW(xz::compress("hello", {.delta = 300}), std::invalid_argument);
}

// What we make at every level and check, decoded by liblzma and by us;
// what liblzma makes at every preset and check, decoded by us
TEST(Xz_Tests, BothWaysWithLiblzma) {
    SKIP_WITHOUT_LIBLZMA();
#if SGCL_TEST_LIBLZMA
    const xz::check checks[] = {xz::check::none, xz::check::crc32, xz::check::crc64, xz::check::sha256};
    const lzma_check theirs[] = {LZMA_CHECK_NONE, LZMA_CHECK_CRC32, LZMA_CHECK_CRC64, LZMA_CHECK_SHA256};
    for (auto& [name, t] : corpus()) {
        for (int level = 0; level <= 9; ++level) {
            auto c = xz::compress(bytes(t), {.level = level, .extreme = level == 9, .check = checks[level % 4]});
            std::string back;
            ASSERT_TRUE(xz_decode(text(c), back)) << name << " level " << level;
            ASSERT_EQ(back, t) << name << " level " << level;
            auto ours = xz::decompress(c);
            ASSERT_TRUE(ours) << name << " level " << level << ": " << ours.error().message();
            ASSERT_EQ(text(*ours), t);
            auto x = xz_easy(t, uint32_t(level) | (level == 5 ? LZMA_PRESET_EXTREME : 0), theirs[(level + 1) % 4]);
            auto mine = xz::decompress(bytes(x));
            ASSERT_TRUE(mine) << name << " preset " << level << ": " << mine.error().message();
            ASSERT_EQ(text(*mine), t);
        }
    }
    // the empty stream: no block, 32 bytes, as xz makes it
    auto empty = xz::compress(bytes(std::string()));
    EXPECT_EQ(empty.size(), 32u);
    EXPECT_EQ(text(empty), xz_easy("", 6));
#endif
}

// Every converter and Delta in a stream, both ways, and a chain of two
TEST(Xz_Tests, FiltersBothWays) {
    SKIP_WITHOUT_LIBLZMA();
#if SGCL_TEST_LIBLZMA
    auto prog = program(size_t(2) << 20);
    for (auto& k : kinds()) {
        auto data = k.kind == cd::SimpleKind::x86 || k.kind == cd::SimpleKind::arm64 ? prog : branches(150000, 6);
        auto theirs = xz_filtered(data, k.id, 0);
        auto back = xz::decompress(bytes(theirs));
        ASSERT_TRUE(back) << k.name << ": " << back.error().message();
        ASSERT_EQ(text(*back), data) << k.name;
        auto ours = xz::compress(bytes(data), {.level = 2, .bcj = k.filter});
        std::string x;
        ASSERT_TRUE(xz_decode(text(ours), x)) << k.name;
        ASSERT_EQ(x, data) << k.name;
        auto both = xz_filtered(data, k.id, 4);
        back = xz::decompress(bytes(both));
        ASSERT_TRUE(back) << k.name << ": " << back.error().message();
        ASSERT_EQ(text(*back), data);
        ours = xz::compress(bytes(data), {.level = 1, .bcj = k.filter, .delta = 3});
        ASSERT_TRUE(xz_decode(text(ours), x)) << k.name;
        ASSERT_EQ(x, data);
    }
    // a start offset (xz --x86=start=...)
    auto shifted = xz_filtered(prog, 0x04, 0, 1, false, 4096);
    auto back = xz::decompress(bytes(shifted));
    ASSERT_TRUE(back) << back.error().message();
    EXPECT_EQ(text(*back), prog);
#endif
}

// A filtered stream through the reader fed 1..7 bytes at a time, and
// written in pieces of 1..7: x86's state over every boundary of the input
// and of the output (the review's point)
TEST(Xz_Tests, FiltersThroughStreamsInPieces) {
    auto prog = program(size_t(300) << 10);
    if (prog.empty()) {
        prog = branches(300000, 7);
    }
    for (auto f : {xz::filter::x86, xz::filter::arm64, xz::filter::riscv}) {
        auto c = text(xz::compress(bytes(prog), {.level = 0, .bcj = f, .delta = f == xz::filter::riscv ? uint16_t(2) : uint16_t(0)}));
        for (size_t feed = 1; feed <= 7; ++feed) {
            xz::reader r(dribble{c, feed});
            std::string got;
            ASSERT_TRUE(read_everything(r, 1 + feed * 3, got)) << "feed " << feed << ": " << (r.last_error() ? r.last_error()->message() : sgcl::string(""));
            ASSERT_EQ(got, prog) << "feed " << feed;
        }
        for (size_t step = 1; step <= 7; ++step) {
            sgcl::io::buffer sink;
            xz::writer w(sink, {.level = 0, .bcj = f});
            size_t i = 0;
            for (size_t k = 0; i < prog.size(); k = (k + 1) % 7) {
                size_t n = std::min(step + k, prog.size() - i);
                ASSERT_TRUE(w.write(bytes(prog.substr(i, n))));
                i += n;
                if (i > 60000 && step > 1) {   // the rest in one piece, to keep the test short
                    ASSERT_TRUE(w.write(bytes(prog.substr(i))));
                    i = prog.size();
                }
            }
            ASSERT_TRUE(w.close());
            auto back = xz::decompress(bytes(buffer_text(sink)));
            ASSERT_TRUE(back) << back.error().message();
            ASSERT_EQ(text(*back), prog) << "step " << step;
        }
    }
}

// Streams one after another with padding, several blocks (liblzma's
// threaded encoder, the sizes in the block headers), in memory and as a stream
TEST(Xz_Tests, StreamsBlocksAndPadding) {
    SKIP_WITHOUT_LIBLZMA();
#if SGCL_TEST_LIBLZMA
    auto a = repo_text(size_t(1) << 20);
    auto b = read_oracle("compress/e.txt");
    lzma_mt mt{};
    mt.threads = 2;
    mt.block_size = 100000;
    mt.preset = 1;
    mt.check = LZMA_CHECK_SHA256;
    lzma_stream s = LZMA_STREAM_INIT;
    ASSERT_EQ(lzma_stream_encoder_mt(&s, &mt), LZMA_OK);
    bool ok;
    auto blocks = xz_run(s, a, ok);
    ASSERT_TRUE(ok);
    auto two = blocks + std::string(8, '\0') + xz_easy(b, 0, LZMA_CHECK_NONE) + std::string(4, '\0');
    auto back = xz::decompress(bytes(two));
    ASSERT_TRUE(back) << back.error().message();
    EXPECT_EQ(text(*back), a + b);
    for (size_t feed : {size_t(1) << 20, size_t(4099), size_t(13)}) {
        if (feed == 13) {
            two = blocks.substr(0, blocks.size()) + std::string(4, '\0') + xz_easy(b.substr(0, 3000), 0);
        }
        xz::reader r(dribble{two, feed});
        std::string got;
        ASSERT_TRUE(read_everything(r, 7000, got)) << feed << ": " << (r.last_error() ? r.last_error()->message() : sgcl::string(""));
        EXPECT_EQ(got, feed == 13 ? a + b.substr(0, 3000) : a + b);
    }
    // padding not in fours, and data after the streams
    auto bad = xz_easy(b, 0) + std::string(3, '\0');
    auto r = xz::decompress(bytes(bad));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::corrupt);
    bad = xz_easy(b, 0) + "junk";
    r = xz::decompress(bytes(bad));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::invalid_header);
    xz::reader rd(dribble{bad, 100});
    std::string got;
    EXPECT_FALSE(read_everything(rd, 100, got));
#endif
}

// A damaged check, index, header or footer is found; data cut short fails;
// flipped bits fail or differ, never crash, the reader agreeing with memory
TEST(Xz_Tests, DamageIsFound) {
    auto t = read_oracle("compress/gettysburg.txt");
    for (auto check : {xz::check::crc32, xz::check::crc64, xz::check::sha256}) {
        auto c = text(xz::compress(bytes(t), {.check = check}));
        for (size_t n = 0; n < c.size(); ++n) {
            auto r = xz::decompress(bytes(c.substr(0, n)));
            ASSERT_FALSE(r) << n;
            xz::reader rd(dribble{c.substr(0, n), 7});
            std::string got;
            EXPECT_FALSE(read_everything(rd, 100, got)) << n;
        }
        std::mt19937 rng{uint32_t(check)};
        for (int i = 0; i < 2000; ++i) {
            auto d = c;
            size_t at = rng() % d.size();
            d[at] = char(d[at] ^ (1 << (rng() % 8)));
            auto r = xz::decompress(bytes(d));
            EXPECT_FALSE(r) << "flip at " << at;   // every byte is covered by a CRC or a check
            xz::reader rd(dribble{d, 64});
            std::string got;
            EXPECT_FALSE(read_everything(rd, 512, got));
        }
    }
    // the data's check: errc::checksum
    auto c = text(xz::compress(bytes(t), {.check = xz::check::crc32}));
    size_t check_at = c.size() - 12 - 12 - 4;   // footer, index (of one record: 12 bytes here), the CRC-32
    c[check_at] = char(c[check_at] ^ 1);
    auto r = xz::decompress(bytes(c));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::checksum) << r.error().message();
    // check none: nothing to compare, read as it is
    auto none = xz::compress(bytes(t), {.check = xz::check::none});
    auto back = xz::decompress(none);
    ASSERT_TRUE(back);
    EXPECT_EQ(text(*back), t);
}

TEST(Xz_Tests, UnsupportedFiltersAndChecks) {
    auto c = text(xz::compress(bytes(std::string("some data to compress"))));
    // the check type 2 (a CRC-32 size, not defined): the header's CRC fixed up
    auto odd = c;
    odd[7] = 2;
    uint32_t crc = cd::xz_format::crc32(reinterpret_cast<const uint8_t*>(odd.data()) + 6, 2);
    for (int i = 0; i < 4; ++i) odd[8 + i] = char(crc >> (8 * i));
    auto r = xz::decompress(bytes(odd));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::unsupported);
    // a filter ID of 0x4F in the block header, its CRC fixed up
    auto f = c;
    size_t h = 12;
    size_t size = (size_t(uint8_t(f[h])) + 1) * 4;
    size_t at = h + 2;
    while (uint8_t(f[at]) & 0x80) ++at;   // compressed size
    ++at;
    while (uint8_t(f[at]) & 0x80) ++at;   // uncompressed size
    ++at;
    f[at] = 0x4F;
    crc = cd::xz_format::crc32(reinterpret_cast<const uint8_t*>(f.data()) + h, size - 4);
    for (int i = 0; i < 4; ++i) f[h + size - 4 + i] = char(crc >> (8 * i));
    r = xz::decompress(bytes(f));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::unsupported) << r.error().message();
    EXPECT_THROW(xz::compress(bytes(c), {.delta = 257}), std::invalid_argument);
    EXPECT_THROW(xz::compress(bytes(c), {.check = xz::check(3)}), std::invalid_argument);
    EXPECT_THROW(xz::compress(bytes(c), {.dictionary = 100}), std::invalid_argument);
}

TEST(Xz_Tests, TheLimits) {
    auto t = read_oracle("compress/e.txt");
    sgcl::io::buffer sink;
    xz::writer w(sink, {.dictionary = uint32_t(3) << 29});
    ASSERT_TRUE(w.write(bytes(t)));
    ASSERT_TRUE(w.close());
    auto c = buffer_text(sink);
    auto r = xz::decompress(bytes(c));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    xz::reader rd(dribble{c, 1000});
    EXPECT_FALSE(rd.read_all());
    ASSERT_TRUE(rd.last_error());
    EXPECT_EQ(rd.last_error()->code(), compress::errc::too_large);
    auto ok = xz::decompress(bytes(c), compress::limits{.max_memory = uint64_t(2) << 30});
    ASSERT_TRUE(ok) << ok.error().message();
    EXPECT_EQ(text(*ok), t);
    // the block's size in its header: the dictionary as large as the block
    auto known = xz::compress(bytes(t), {.dictionary = uint32_t(3) << 29});
    auto small = xz::decompress(known, compress::limits{.max_memory = uint64_t(1) << 20});
    ASSERT_TRUE(small) << small.error().message();
    // a bomb
    std::string zeros(size_t(8) << 20, '\0');
    auto bomb = xz::compress(bytes(zeros));
    r = xz::decompress(bomb, compress::limits{.max_size = 1 << 20});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    sgcl::io::buffer s2;
    xz::writer w2(s2);
    ASSERT_TRUE(w2.write(bytes(zeros)));
    ASSERT_TRUE(w2.close());
    r = xz::decompress(bytes(buffer_text(s2)), compress::limits{.max_size = 1 << 20});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
}

TEST(Xz_Tests, TheAsyncForms) {
    auto t = repo_text(size_t(300) << 10);
    auto task = sgcl::async::spawn([](std::string t) -> sgcl::async::task<std::string> {
        sgcl::io::buffer sink;
        xz::writer w(sink, {.level = 4, .check = xz::check::sha256, .bcj = xz::filter::x86});
        for (size_t i = 0; i < t.size(); i += 70000) {
            if (!co_await w.async_write(bytes(t.substr(i, 70000)))) {
                co_return "write failed";
            }
        }
        if (!co_await w.async_close()) {
            co_return "close failed";
        }
        auto c = buffer_text(sink);
        xz::reader r(dribble{c, 1000});
        auto all = co_await r.async_read_all();
        co_return all ? text(*all) : std::string("read failed");
    }(t));
    EXPECT_EQ(task.wait(), t);
    sgcl::async::scheduler::stop();
}

TEST(Xz_Tests, AWriterKeepsItsFirstErrorAndResets) {
    struct failing final : sgcl::io::mixin::writer<failing> {
        using sgcl::io::mixin::writer<failing>::write;
        size_t room = 500;
        int calls = 0;
        sgcl::expected<size_t, sgcl::io::error> write(const sgcl::slice<const std::byte>& d) {
            ++calls;
            if (d.size() > room) {
                return sgcl::unexpected<sgcl::io::error>(sgcl::io::error(sgcl::error_code(EIO, std::system_category()), "write", "disk"));
            }
            room -= d.size();
            return d.size();
        }
    } out;
    std::mt19937 rng(2);
    std::string noise(1 << 20, 0);
    for (auto& c : noise) c = char(rng());
    xz::writer w(out, {.level = 0});
    for (size_t i = 0; i < noise.size(); i += 65536) {
        (void)w.write(bytes(noise.substr(i, 65536)));
    }
    int calls = out.calls;
    auto c = w.close();
    ASSERT_FALSE(c);
    EXPECT_EQ(c.error().code(), sgcl::error_code(EIO, std::system_category()));
    EXPECT_EQ(out.calls, calls);
    ASSERT_TRUE(w.last_error());
    sgcl::io::buffer sink;
    w.reset(sink);
    ASSERT_TRUE(w.write(std::string("after the reset")));
    ASSERT_TRUE(w.close());
    auto back = xz::decompress(bytes(buffer_text(sink)));
    ASSERT_TRUE(back) << back.error().message();
    EXPECT_EQ(text(*back), "after the reset");
    sgcl::io::buffer empty;
    xz::writer e(empty, {.check = xz::check::none});
    ASSERT_TRUE(e.close());
    auto none = xz::decompress(bytes(buffer_text(empty)));
    ASSERT_TRUE(none);
    EXPECT_TRUE(none->empty());
#if SGCL_TEST_LIBLZMA
    std::string x;
    ASSERT_TRUE(xz_decode(buffer_text(empty), x));
    EXPECT_EQ(x, "");
#endif
}
