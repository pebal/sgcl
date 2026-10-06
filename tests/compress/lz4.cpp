//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// LZ4: frames and raw blocks both ways, at every level and with every frame
// option, against liblz4 (when the build found it) and on their own; the
// frames written by hand from the specification (the empty frame, stored
// blocks, skippable and legacy frames); every kind of damage the format can
// show; the limits; the streams in pieces, in a task, with a dictionary.
#include "common.h"

#if SGCL_TEST_LZ4
#include <lz4.h>
#include <lz4frame.h>
#include <lz4hc.h>
#endif

using namespace compress_test;

namespace {
    using sgcl::byte;
    namespace io = sgcl::io;
    using lz4 = compress::lz4;

    std::string contents(const io::buffer& b) {
        auto d = b.data();
        return std::string(reinterpret_cast<const char*>(d.data()), d.size());
    }

    // Text with repeats at every distance up to the window and past it
    std::string prose(size_t n, unsigned seed = 1) {
        std::mt19937 rng(seed);
        static const char* words[] = {"the ", "frame ", "block ", "of ", "LZ4 ", "and ", "a ", "match\n", "literal ", "offset, "};
        std::string s;
        while (s.size() < n) {
            s += words[rng() % 10];
            if (rng() % 50 == 0 && s.size() > 1000) {
                s += s.substr(rng() % (s.size() - 500), 300);   // a long repeat
            }
        }
        s.resize(n);
        return s;
    }

    void le32(std::string& o, uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            o += char(uint8_t(v >> (8 * i)));
        }
    }

    // A frame header written by hand: FLG, BD and the fields, its checksum
    std::string header(uint8_t flg, uint8_t bd, const std::string& fields = "") {
        std::string h;
        le32(h, 0x184D2204);
        std::string d;
        d += char(flg);
        d += char(bd);
        d += fields;
        h += d;
        h += char(uint8_t(sgcl::hash::xxh32::of(bytes(d)) >> 8));
        return h;
    }

    // The block format for a run of literals alone: the token, the length
    std::string literals_block(const std::string& s) {
        std::string b;
        size_t n = s.size();
        b += char(uint8_t(std::min<size_t>(n, 15) << 4));
        if (n >= 15) {
            n -= 15;
            while (n >= 255) {
                b += char(255);
                n -= 255;
            }
            b += char(uint8_t(n));
        }
        return b + s;
    }

    std::string all_levels_tag(int level) {
        return "level " + std::to_string(level);
    }
}

// The empty frame as the specification lays it out, byte for byte: the
// magic, FLG 0x64 (version 01, independent, content checksum), BD 0x40
// (64 KB), the header's checksum, the end mark, XXH32 of nothing
TEST(Lz4_Tests, TheEmptyFrameByHand) {
    const std::string frame("\x04\x22\x4d\x18\x64\x40\xa7\x00\x00\x00\x00\x05\x5d\xcc\x02", 15);
    auto r = lz4::decompress(bytes(frame));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(r->size(), 0u);
    auto ours = lz4::compress(bytes(std::string()), {.block_size = lz4::block_size::kb64});
    // ours carries the content size (0): FLG 0x6C and 8 more bytes
    EXPECT_EQ(uint8_t(ours[4]), 0x6Cu);
    EXPECT_EQ(ours.size(), 15u + 8u);
    EXPECT_EQ(lz4::decompress(ours)->size(), 0u);
}

// Frames made by hand from the specification: a stored block, a block of
// literals, a match reaching back into the block before (linked), the
// block checksum, the content size, a skippable frame between two frames
TEST(Lz4_Tests, FramesByHand) {
    // a stored block in an independent frame with a block checksum
    std::string f = header(0x40 | 0x20 | 0x10, 0x40);
    le32(f, 5 | 0x80000000u);
    f += "hello";
    le32(f, sgcl::hash::xxh32::of("hello"));
    le32(f, 0);
    auto r = lz4::decompress(bytes(f));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), "hello");
    // linked: the second block's match copies the first block's bytes
    std::string g = header(0x40 | 0x08, 0x40, std::string("\x0a\0\0\0\0\0\0\0", 8));
    std::string b1 = literals_block("abcde");
    le32(g, uint32_t(b1.size()));
    g += b1;
    std::string b2;
    b2 += char(0x01);   // no literals, a match of 5
    b2 += "\x05";
    b2 += '\0';
    b2 += char(0x00);   // the last sequence: no literals
    le32(g, uint32_t(b2.size()));
    g += b2;
    le32(g, 0);
    r = lz4::decompress(bytes(g));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), "abcdeabcde");
    // the same blocks in an independent frame: the match reaches before its block
    std::string h = header(0x40 | 0x20, 0x40);
    le32(h, uint32_t(b1.size()));
    h += b1;
    le32(h, uint32_t(b2.size()));
    h += b2;
    le32(h, 0);
    r = lz4::decompress(bytes(h));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::corrupt);
    // a skippable frame between two frames, and one at the end
    std::string skip;
    le32(skip, 0x184D2A5F);
    le32(skip, 3);
    skip += "xyz";
    r = lz4::decompress(bytes(f + skip + f + skip));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), "hellohello");
    // a skippable frame alone is no data, and that is fine
    r = lz4::decompress(bytes(skip));
    ASSERT_TRUE(r);
    EXPECT_EQ(r->size(), 0u);
}

// The legacy frame (lz4 -l): blocks of 8 MB with their compressed size
// alone, until the input ends or another frame begins
TEST(Lz4_Tests, LegacyFrames) {
    const std::string plain = prose(100000);
    auto block = lz4::compress_block(bytes(plain));
    std::string legacy;
    le32(legacy, 0x184C2102);
    le32(legacy, uint32_t(block.size()));
    legacy += text(block);
    auto r = lz4::decompress(bytes(legacy));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), plain);
    // followed by a frame, and read by the stream in pieces
    std::string both = legacy + text(lz4::compress(bytes(std::string("tail"))));
    r = lz4::decompress(bytes(both));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), plain + "tail");
    lz4::reader rd(dribble{both, 333});
    auto all = rd.read_all();
    ASSERT_TRUE(all) << all.error().message();
    EXPECT_EQ(text(*all), plain + "tail");
    // a legacy block's size past the bound of 8 MB
    std::string bad;
    le32(bad, 0x184C2102);
    le32(bad, 0x7FFFFFFF);
    EXPECT_EQ(lz4::decompress(bytes(bad)).error().code(), compress::errc::corrupt);
}

// Every level and every frame option, both ways through ourselves, over
// the corpus (text, random bytes that go stored, runs, the lengths about
// the window and the block)
TEST(Lz4_Tests, EveryLevelAndOptionRoundTrips) {
    auto corp = corpus();
    corp.push_back({"prose", prose(300000)});
    corp.push_back({"twelve", "abcdefghijkl"});
    corp.push_back({"thirteen", "abcabcabcabca"});
    for (auto& [name, plain] : corp) {
        for (int level : {1, 2, 3, 6, 9, 10, 11, 12, -1, -7, -65537}) {
            if (level >= 10 && plain.size() > 100000) {
                continue;   // the slowest levels over the smaller inputs
            }
            for (auto bs : {lz4::block_size::kb64, lz4::block_size::mb4}) {
                for (bool linked : {false, true}) {
                    SCOPED_TRACE(name + " " + all_levels_tag(level) + (linked ? " linked" : ""));
                    lz4::options o {.level = level, .block_size = bs, .linked_blocks = linked, .block_checksum = linked};
                    auto c = lz4::compress(bytes(plain), o);
                    auto d = lz4::decompress(c);
                    ASSERT_TRUE(d) << d.error().message();
                    ASSERT_EQ(text(*d), plain);
                }
            }
            auto raw = lz4::compress_block(bytes(plain), {.level = level});
            auto back = lz4::decompress_block(raw, plain.size());
            ASSERT_TRUE(back) << back.error().message();
            ASSERT_EQ(text(*back), plain);
        }
    }
}

// The levels order the sizes as their names say, on text
TEST(Lz4_Tests, LevelsTradeSpeedForSize) {
    const std::string plain = prose(200000);
    size_t fast = lz4::compress(bytes(plain), {.level = 1}).size();
    size_t accelerated = lz4::compress(bytes(plain), {.level = -50}).size();
    size_t high = lz4::compress(bytes(plain), {.level = 9}).size();
    size_t smallest = lz4::compress(bytes(plain), {.level = 12}).size();
    EXPECT_LT(high, fast);
    EXPECT_LE(smallest, high);
    EXPECT_GE(accelerated, fast);
    EXPECT_LT(fast, plain.size() / 2);
}

#if SGCL_TEST_LZ4
namespace {
    std::string lz4f_decompress(const std::string& frame, const std::string& dictionary, bool& ok) {
        LZ4F_dctx* d = nullptr;
        LZ4F_createDecompressionContext(&d, LZ4F_VERSION);
        std::string out;
        std::string buf(1 << 16, 0);
        size_t at = 0;
        ok = true;
        size_t r = 1;
        while (at < frame.size()) {
            size_t dst = buf.size(), src = frame.size() - at;
            r = dictionary.empty() ? LZ4F_decompress(d, buf.data(), &dst, frame.data() + at, &src, nullptr)
                                   : LZ4F_decompress_usingDict(d, buf.data(), &dst, frame.data() + at, &src, dictionary.data(), dictionary.size(), nullptr);
            if (LZ4F_isError(r)) {
                ok = false;
                break;
            }
            out.append(buf.data(), dst);
            at += src;
            if (src == 0 && dst == 0) {
                break;
            }
        }
        ok = ok && r == 0;
        LZ4F_freeDecompressionContext(d);
        return out;
    }

    std::string lz4f_compress(const std::string& plain, int level, int block_id, bool linked, bool block_checksum, bool content_checksum, bool size) {
        LZ4F_preferences_t p {};
        p.frameInfo.blockMode = linked ? LZ4F_blockLinked : LZ4F_blockIndependent;
        p.frameInfo.blockSizeID = LZ4F_blockSizeID_t(block_id);
        p.frameInfo.contentChecksumFlag = LZ4F_contentChecksum_t(content_checksum);
        p.frameInfo.blockChecksumFlag = LZ4F_blockChecksum_t(block_checksum);
        p.frameInfo.contentSize = size ? plain.size() : 0;
        p.compressionLevel = level;
        std::string out(LZ4F_compressFrameBound(plain.size(), &p), 0);
        out.resize(LZ4F_compressFrame(out.data(), out.size(), plain.data(), plain.size(), &p));
        return out;
    }
}

// Ours read by liblz4 and liblz4's by ours, every option of the frame,
// frames and raw blocks, with the levels both sides have
TEST(Lz4_Tests, BothWaysWithLiblz4) {
    auto corp = corpus();
    corp.push_back({"prose", prose(1 << 20)});
    for (auto& [name, plain] : corp) {
        SCOPED_TRACE(name);
        for (int level : {1, 3, 9, 12, -3}) {
            if (level == 12 && plain.size() > 200000) {
                continue;
            }
            for (int bid : {4, 5, 7}) {
                for (bool linked : {false, true}) {
                    for (bool bc : {false, true}) {
                        lz4::options o {.level = level, .block_size = lz4::block_size(bid), .linked_blocks = linked, .block_checksum = bc,
                                        .content_checksum = !bc};
                        bool ok = false;
                        auto ours = text(lz4::compress(bytes(plain), o));
                        EXPECT_EQ(lz4f_decompress(ours, "", ok), plain);
                        EXPECT_TRUE(ok);
                        auto theirs = lz4f_compress(plain, level, bid, linked, bc, !bc, bc);
                        auto d = lz4::decompress(bytes(theirs));
                        ASSERT_TRUE(d) << d.error().message();
                        EXPECT_EQ(text(*d), plain);
                    }
                }
            }
            // raw blocks: LZ4_compress_default and LZ4_compress_HC, LZ4_decompress_safe
            auto raw = text(lz4::compress_block(bytes(plain), {.level = level}));
            std::string back(plain.size() + 1, 0);
            int n = LZ4_decompress_safe(raw.data(), back.data(), int(raw.size()), int(plain.size()));
            ASSERT_EQ(n, int(plain.size()));
            EXPECT_EQ(back.substr(0, plain.size()), plain);
            std::string theirs(LZ4_compressBound(int(plain.size())), 0);
            int size = level == 1 ? LZ4_compress_default(plain.data(), theirs.data(), int(plain.size()), int(theirs.size()))
                     : level < 0  ? LZ4_compress_fast(plain.data(), theirs.data(), int(plain.size()), int(theirs.size()), -level)
                                  : LZ4_compress_HC(plain.data(), theirs.data(), int(plain.size()), int(theirs.size()), level);
            theirs.resize(size_t(size));
            auto d = lz4::decompress_block(bytes(theirs), plain.size());
            ASSERT_TRUE(d) << d.error().message();
            EXPECT_EQ(text(*d), plain);
        }
    }
}

// The fast level writes what liblz4's does, byte for byte (the same
// method), and the others no larger than a fraction more
TEST(Lz4_Tests, SizesBesideLiblz4) {
    const std::string plain = prose(1 << 20, 9);
    std::string theirs(LZ4_compressBound(int(plain.size())), 0);
    int fast = LZ4_compress_default(plain.data(), theirs.data(), int(plain.size()), int(theirs.size()));
    EXPECT_LE(lz4::compress_block(bytes(plain)).size(), size_t(fast) + size_t(fast) / 100);
    for (int level : {3, 9, 12}) {
        int hc = LZ4_compress_HC(plain.data(), theirs.data(), int(plain.size()), int(theirs.size()), level);
        EXPECT_LE(lz4::compress_block(bytes(plain), {.level = level}).size(), size_t(hc) + size_t(hc) / 50) << level;
    }
}

// A dictionary both ways: liblz4's frames made with it read by ours, ours
// read by liblz4, raw blocks the same
TEST(Lz4_Tests, DictionariesWithLiblz4) {
    const std::string dict = prose(100000, 3);   // its last 64 KB count
    const std::string plain = prose(300000, 3).substr(50000);
    for (bool linked : {false, true}) {
        for (int level : {1, 9}) {
            lz4::options o {.level = level, .block_size = lz4::block_size::kb64, .linked_blocks = linked, .dictionary = bytes(dict), .dictionary_id = 42};
            auto ours = text(lz4::compress(bytes(plain), o));
            auto without = lz4::compress(bytes(plain), {.level = level, .block_size = lz4::block_size::kb64, .linked_blocks = linked});
            EXPECT_LT(ours.size(), without.size());
            bool ok = false;
            EXPECT_EQ(lz4f_decompress(ours, dict, ok), plain);
            EXPECT_TRUE(ok);
            // liblz4's frame with the dictionary
            LZ4F_preferences_t p {};
            p.frameInfo.blockMode = linked ? LZ4F_blockLinked : LZ4F_blockIndependent;
            p.frameInfo.blockSizeID = LZ4F_max64KB;
            p.frameInfo.dictID = 42;
            p.compressionLevel = level;
            LZ4F_CDict* cd = LZ4F_createCDict(dict.data(), dict.size());
            LZ4F_cctx* cc = nullptr;
            LZ4F_createCompressionContext(&cc, LZ4F_VERSION);
            std::string theirs(LZ4F_compressFrameBound(plain.size(), &p), 0);
            theirs.resize(LZ4F_compressFrame_usingCDict(cc, theirs.data(), theirs.size(), plain.data(), plain.size(), cd, &p));
            LZ4F_freeCompressionContext(cc);
            LZ4F_freeCDict(cd);
            auto d = lz4::decompress(bytes(theirs), {.dictionary = bytes(dict)});
            ASSERT_TRUE(d) << d.error().message();
            EXPECT_EQ(text(*d), plain);
            lz4::reader r(dribble{theirs, 1000}, {.dictionary = bytes(dict), .dictionary_id = 42});
            auto all = r.read_all();
            ASSERT_TRUE(all) << all.error().message();
            EXPECT_EQ(text(*all), plain);
        }
    }
    // raw: a block against the dictionary
    const std::string small = plain.substr(0, 60000);
    auto raw = text(lz4::compress_block(bytes(small), {.dictionary = bytes(dict)}));
    std::string back(small.size(), 0);
    const std::string tail = dict.substr(dict.size() - 65536);
    int n = LZ4_decompress_safe_usingDict(raw.data(), back.data(), int(raw.size()), int(back.size()), tail.data(), int(tail.size()));
    ASSERT_EQ(n, int(small.size()));
    EXPECT_EQ(back, small);
    auto d = lz4::decompress_block(bytes(raw), small.size(), {.dictionary = bytes(dict)});
    ASSERT_TRUE(d) << d.error().message();
    EXPECT_EQ(text(*d), small);
}
#endif

// A dictionary on our own: the frame says it needs one, and the id given
// must be the frame's
TEST(Lz4_Tests, DictionaryRequired) {
    const std::string dict = prose(20000, 4);
    const std::string plain = prose(50000, 4);
    auto c = lz4::compress(bytes(plain), {.dictionary = bytes(dict), .dictionary_id = 9});
    auto none = lz4::decompress(c);
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), compress::errc::dictionary_required);
    auto other = lz4::decompress(c, {.dictionary = bytes(dict), .dictionary_id = 10});
    ASSERT_FALSE(other);
    EXPECT_EQ(other.error().code(), compress::errc::dictionary_required);
    auto right = lz4::decompress(c, {.dictionary = bytes(dict), .dictionary_id = 9});
    ASSERT_TRUE(right) << right.error().message();
    EXPECT_EQ(text(*right), plain);
    auto any_id = lz4::decompress(c, {.dictionary = bytes(dict)});   // no id given: the frame's is taken on trust
    ASSERT_TRUE(any_id);
    lz4::reader r(dribble{text(c), 100});
    auto all = r.read_all();
    ASSERT_FALSE(all);
    ASSERT_TRUE(r.last_error());
    EXPECT_EQ(r.last_error()->code(), compress::errc::dictionary_required);
    // a dictionary of no bytes is none
    auto empty = lz4::compress(bytes(plain), {.dictionary = bytes(std::string())});
    EXPECT_EQ(text(*lz4::decompress(empty)), plain);
}

// Damage: every byte of the header, a bad block, the checksums, the sizes;
// and every prefix of a frame ends early, cleanly
TEST(Lz4_Tests, DamageIsFound) {
    const std::string plain = prose(200000, 5);
    const std::string c = text(lz4::compress(bytes(plain), {.block_size = lz4::block_size::kb64, .linked_blocks = true, .block_checksum = true}));
    auto code = [](const std::string& s) {
        auto r = lz4::decompress(bytes(s));
        return r ? compress::errc{} : r.error().code();
    };
    std::string bad = c;
    bad[0] = 'X';
    EXPECT_EQ(code(bad), compress::errc::invalid_header);
    bad = c;
    bad[4] = char(0x80 | (bad[4] & 0x3F));   // version 10
    EXPECT_EQ(code(bad), compress::errc::invalid_header);
    bad = c;
    bad[4] ^= 0x02;                          // a reserved bit
    EXPECT_EQ(code(bad), compress::errc::invalid_header);
    bad = c;
    bad[5] = char(0x30);                     // a block size id of 3
    EXPECT_EQ(code(bad), compress::errc::invalid_header);
    bad = c;
    bad[5] |= 0x01;                          // a reserved bit of BD
    EXPECT_EQ(code(bad), compress::errc::invalid_header);
    bad = c;
    bad[14] ^= 0x55;                         // the header's checksum (FLG BD and 8 bytes of size before it)
    EXPECT_EQ(code(bad), compress::errc::checksum);
    bad = c;
    bad[30] ^= 0x01;                         // inside the first block: its checksum
    EXPECT_EQ(code(bad), compress::errc::checksum);
    bad = c;
    bad[bad.size() - 1] ^= 0x01;             // the content checksum
    EXPECT_EQ(code(bad), compress::errc::checksum);
    // a block larger than the frame's block size
    std::string big = header(0x60, 0x40);
    le32(big, 70000);
    big += std::string(70000, 'x');
    EXPECT_EQ(code(big), compress::errc::corrupt);
    // an offset of 0 and one before the data
    for (std::string seq : {std::string("\x10" "a\x00\x00\x00", 5), std::string("\x10" "a\x05\x00\x00", 5)}) {
        std::string f = header(0x60, 0x40);
        le32(f, uint32_t(seq.size()));
        f += seq;
        le32(f, 0);
        EXPECT_EQ(code(f), compress::errc::corrupt);
        auto raw = lz4::decompress_block(bytes(seq), 100);
        ASSERT_FALSE(raw);
        EXPECT_EQ(raw.error().code(), compress::errc::corrupt);
    }
    // the content size in the header differs from the content
    std::string sized = text(lz4::compress(bytes(std::string("abc"))));
    sized[6] = 4;   // 3 -> 4
    sized[4 + 2 + 8] = char(uint8_t(sgcl::hash::xxh32::of(bytes(sized.substr(4, 10))) >> 8));
    EXPECT_EQ(code(sized), compress::errc::corrupt);
    // after the frame, bytes that are no frame
    EXPECT_EQ(code(c + "garbage"), compress::errc::invalid_header);
    // every prefix fails as an early end, never anything else
    for (size_t n = 0; n < c.size(); n += (n < 100 ? 1 : 997)) {
        auto r = lz4::decompress(bytes(c.substr(0, n)));
        ASSERT_FALSE(r) << n;
        EXPECT_EQ(r.error().code(), compress::errc::unexpected_end) << n;
        lz4::reader rd(dribble{c.substr(0, n), 4096});
        auto all = rd.read_all();
        ASSERT_FALSE(all) << n;
        ASSERT_TRUE(rd.last_error());
        EXPECT_EQ(rd.last_error()->code(), compress::errc::unexpected_end) << n;
    }
    // a raw block cut short
    auto raw = text(lz4::compress_block(bytes(plain)));
    for (size_t n : {size_t(0), size_t(1), raw.size() / 2, raw.size() - 1}) {
        auto r = lz4::decompress_block(bytes(raw.substr(0, n)), plain.size());
        EXPECT_FALSE(r) << n;
    }
}

// The limits: the size, the memory a frame's block size asks for
TEST(Lz4_Tests, TheLimits) {
    const std::string plain = prose(300000, 6);
    auto c = lz4::compress(bytes(plain));
    EXPECT_EQ(lz4::decompress(c, compress::limits{.max_size = plain.size() - 1}).error().code(), compress::errc::too_large);
    EXPECT_TRUE(lz4::decompress(c, compress::limits{.max_size = plain.size()}));
    // a block size of 4 MB past a memory limit of 1 MB, in memory and in a stream
    auto r = lz4::decompress(c, compress::limits{.max_memory = 1 << 20});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    lz4::reader rd(dribble{text(c), 1000}, compress::limits{.max_memory = 1 << 20});
    EXPECT_FALSE(rd.read_all());
    EXPECT_EQ(rd.last_error()->code(), compress::errc::too_large);
    auto small = lz4::compress(bytes(plain), {.block_size = lz4::block_size::kb64});
    EXPECT_TRUE(lz4::decompress(small, compress::limits{.max_memory = 1 << 20}));
    // decompress_block: exactly the size, and one byte less
    auto raw = lz4::compress_block(bytes(plain));
    EXPECT_TRUE(lz4::decompress_block(raw, plain.size()));
    EXPECT_EQ(lz4::decompress_block(raw, plain.size() - 1).error().code(), compress::errc::too_large);
    EXPECT_EQ(lz4::decompress_block(raw, 0).error().code(), compress::errc::too_large);
    EXPECT_EQ(lz4::decompress_block(lz4::compress_block(bytes(std::string())), 0)->size(), 0u);
}

// The level's range and the options' checks
TEST(Lz4_Tests, OptionsAtTheirEnds) {
    for (int v : {1, 12, -1, -65537, 9, 2}) {
        EXPECT_EQ(lz4::level(v).value(), v);
    }
    for (int v : {0, 13, -65538, INT_MIN, INT_MAX}) {
        EXPECT_THROW(lz4::level{v}, std::invalid_argument);
    }
    EXPECT_EQ(lz4::level().value(), lz4::level::standard);
    EXPECT_EQ(lz4::level(lz4::level::fastest).value(), -65537);
    EXPECT_THROW(lz4::compress(bytes(std::string("x")), {.block_size = lz4::block_size(3)}), std::invalid_argument);
    io::buffer out;
    lz4::writer w(out, {.block_size = lz4::block_size(8)});
    auto r = w.write(std::string("x"));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), make_error_code(compress::errc::invalid_argument));
    EXPECT_EQ(out.size(), 0u);
}

// The writer in pieces of every size, flushed now and then: each flush
// makes everything so far decodable; liblz4-free, through our reader
TEST(Lz4_Tests, StreamsInPieces) {
    const std::string plain = prose(700000, 7);
    for (int level : {1, 4, 10}) {
        for (bool linked : {false, true}) {
            SCOPED_TRACE(all_levels_tag(level) + (linked ? " linked" : ""));
            io::buffer sink;
            lz4::writer w(sink, {.level = level, .block_size = lz4::block_size::kb64, .linked_blocks = linked, .block_checksum = true});
            std::mt19937 rng(level);
            size_t at = 0;
            while (at < plain.size()) {
                size_t k = std::min<size_t>(rng() % 100000, plain.size() - at);
                ASSERT_TRUE(w.write(bytes(plain.substr(at, k))));
                at += k;
                if (rng() % 4 == 0) {
                    ASSERT_TRUE(w.flush());
                    // what is in the sink decodes to everything written so far
                    lz4::reader partial(dribble{contents(sink), 5000});
                    std::string got;
                    byte b[4096];
                    for (;;) {
                        auto n = partial.read(b);
                        if (!n || *n == 0) {
                            break;
                        }
                        got.append(reinterpret_cast<const char*>(b), *n);
                    }
                    EXPECT_EQ(got, plain.substr(0, at));
                }
            }
            ASSERT_TRUE(w.close());
            lz4::reader r(dribble{contents(sink), 777});
            auto all = r.read_all();
            ASSERT_TRUE(all) << all.error().message();
            EXPECT_EQ(text(*all), plain);
        }
    }
}

TEST(Lz4_Tests, TheAsyncForms) {
    const std::string plain = prose(500000, 8);
    auto task = sgcl::async::spawn([](std::string t) -> sgcl::async::task<std::string> {
        io::buffer sink;
        lz4::writer w(sink, {.level = 3, .block_size = lz4::block_size::kb256, .linked_blocks = true});
        for (size_t i = 0; i < t.size(); i += 70000) {
            if (!co_await w.async_write(bytes(t.substr(i, 70000)))) {
                co_return "write failed";
            }
            if (!co_await w.async_flush()) {
                co_return "flush failed";
            }
        }
        if (!co_await w.async_close()) {
            co_return "close failed";
        }
        lz4::reader r(dribble{contents(sink), 1000});
        auto all = co_await r.async_read_all();
        co_return all ? text(*all) : std::string("read failed");
    }(plain));
    EXPECT_EQ(task.wait(), plain);
    sgcl::async::scheduler::stop();
}

// The engine's indexes near 2^31: the tables shifted down, the output the same
TEST(Lz4_Tests, IndexesRebasedPastTwoToThe31) {
    const std::string plain = prose(300000, 10);
    for (int level : {1, 9}) {
        sgcl::compress::detail::Lz4Engine e;
        e.set_level(level);
        e.next = (uint32_t(1) << 31) - 100000;
        std::vector<uint8_t> out(sgcl::compress::detail::lz4_bound(plain.size()));
        const uint8_t* p = reinterpret_cast<const uint8_t*>(plain.data());
        size_t at = 0;
        std::string blocks;
        std::string back;
        while (at < plain.size()) {
            size_t k = std::min<size_t>(65536, plain.size() - at);
            size_t keep = std::min<size_t>(at, 65536);
            uint32_t start = e.room(k, keep);
            EXPECT_LT(uint64_t(start) + k, uint64_t(1) << 31);
            const uint8_t* src = p + at;
            uint8_t* end = e.compress(src - start, src - keep, src, k, out.data());
            std::string block(reinterpret_cast<char*>(out.data()), size_t(end - out.data()));
            // decoded against the plain history before it
            std::string history = plain.substr(at - keep, keep);
            auto d = lz4::decompress_block(bytes(block), k, {.dictionary = bytes(history)});
            ASSERT_TRUE(d) << d.error().message();
            back += text(*d);
            at += k;
        }
        EXPECT_EQ(back, plain);
    }
}
