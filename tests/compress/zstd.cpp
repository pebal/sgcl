//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// zstd: frames both ways at every kind of level (negative, fast, the lazy
// rows, the tree, the optimal parsers) and with every option, against
// libzstd (when the build found it) and on their own; frames written by
// hand from RFC 8878 (raw, RLE and compressed blocks, the single segment,
// skippable frames, frames one after another); dictionaries of zstd's
// format (trained by libzstd) and of raw content; every kind of damage; the
// limits; the streams in pieces, flushed, in a task.
#include "common.h"

#include <array>

#if SGCL_TEST_ZSTD
#define ZDICT_STATIC_LINKING_ONLY
#include <zdict.h>
#include <zstd.h>
#endif

using namespace compress_test;

namespace {
    using sgcl::byte;
    namespace io = sgcl::io;
    using zstd = compress::zstd;

    std::string contents(const io::buffer& b) {
        auto d = b.data();
        return std::string(reinterpret_cast<const char*>(d.data()), d.size());
    }

    // Text with repeats at every distance, some of them far
    std::string prose(size_t n, unsigned seed = 1) {
        std::mt19937 rng(seed);
        static const char* words[] = {"the ", "frame ", "block ", "of ", "zstd ", "and ", "a ", "sequence\n", "literal ", "offset, ", "FSE ", "Huffman "};
        std::string s;
        while (s.size() < n) {
            s += words[rng() % 12];
            if (rng() % 40 == 0 && s.size() > 1000) {
                s += s.substr(rng() % (s.size() - 500), 200 + rng() % 300);   // a long repeat
            }
            if (rng() % 300 == 0) {
                s += std::to_string(rng());
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

    void le24(std::string& o, uint32_t v) {
        for (int i = 0; i < 3; ++i) {
            o += char(uint8_t(v >> (8 * i)));
        }
    }

    // The low 32 bits of XXH64, little-endian: a frame's content checksum
    std::string checksum_of(const std::string& s) {
        std::string o;
        le32(o, uint32_t(sgcl::hash::xxh64::of(bytes(s))));
        return o;
    }

    compress::errc code_of(const std::string& s, const compress::limits& l = {}) {
        auto r = zstd::decompress(bytes(s), l);
        return r ? compress::errc{} : r.error().code();
    }

    std::string tag(int level) {
        return "level " + std::to_string(level);
    }
}

// Frames by hand: the magic, the frame header descriptor (single segment
// with a 1-byte content size, a window descriptor, the checksum flag), a
// raw block, an RLE block, a compressed block of raw literals and no
// sequences, the checksum
TEST(Zstd_Tests, FramesByHand) {
    // single segment (0x20), checksum (0x04): FCS of 1 byte; a raw last block
    std::string f;
    le32(f, 0xFD2FB528);
    f += char(0x24);
    f += char(5);
    le24(f, (5u << 3) | 1u);
    f += "hello";
    f += checksum_of("hello");
    auto r = zstd::decompress(bytes(f));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), "hello");
    EXPECT_EQ(zstd::content_size(bytes(f)), 5u);
    EXPECT_EQ(zstd::dictionary_id(bytes(f)), 0u);
    // a window descriptor (no single segment, no size): exponent 0 = 1 KB;
    // an RLE block of 7 'z', then a compressed block: raw literals "ab" and
    // no sequences
    std::string g;
    le32(g, 0xFD2FB528);
    g += char(0x00);
    g += char(0x00);
    le24(g, (7u << 3) | (1u << 1));
    g += 'z';
    std::string body;
    body += char(2 << 3);   // raw literals, 1-byte header, size 2
    body += "ab";
    body += char(0);        // no sequences
    le24(g, uint32_t(body.size() << 3) | (2u << 1) | 1u);
    g += body;
    r = zstd::decompress(bytes(g));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), "zzzzzzzab");
    EXPECT_FALSE(zstd::content_size(bytes(g)));
    // the same frames one after another, a skippable frame between and after
    std::string skip;
    le32(skip, 0x184D2A53);
    le32(skip, 4);
    skip += "skip";
    r = zstd::decompress(bytes(f + skip + g + skip));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), "hellozzzzzzzab");
    zstd::reader rd(dribble{f + skip + g + skip, 3});
    auto all = rd.read_all();
    ASSERT_TRUE(all) << all.error().message();
    EXPECT_EQ(text(*all), "hellozzzzzzzab");
    // a skippable frame alone is no data
    r = zstd::decompress(bytes(skip));
    ASSERT_TRUE(r);
    EXPECT_EQ(r->size(), 0u);
    // the empty frame: single segment, size 0, an empty raw last block
    std::string e;
    le32(e, 0xFD2FB528);
    e += char(0x20);
    e += char(0);
    le24(e, 1u);
    r = zstd::decompress(bytes(e));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(r->size(), 0u);
    // ours of nothing reads back as nothing, and says its size
    auto ours = zstd::compress(bytes(std::string()));
    EXPECT_EQ(zstd::content_size(ours), 0u);
    EXPECT_EQ(zstd::decompress(ours)->size(), 0u);
}

// Sequences by hand, every code of the block in RLE mode (no state bits):
// raw literals "abcd", then one sequence of 4 literals and a match of 3
// from a new offset of 2 (code 2, extra bits 01); and a sequence of no
// literals whose repeat (value 3, shifted by the length of 0) is the last
// offset less one: 1 - 1, an offset of 0, which no frame may have
TEST(Zstd_Tests, SequencesByHand) {
    auto frame = [](const std::string& body) {
        std::string f;
        le32(f, 0xFD2FB528);
        f += char(0x00);   // a window descriptor: 1 KB (a single segment's window would be smaller than the block)
        f += char(0x00);
        le24(f, uint32_t(body.size() << 3) | (2u << 1) | 1u);
        return f + body;
    };
    std::string body;
    body += char(4 << 3);   // raw literals, 1-byte header, size 4
    body += "abcd";
    body += char(1);        // one sequence
    body += char(0x54);     // literal lengths, offsets, match lengths: RLE
    body += char(4);        // literal length code 4: 4
    body += char(2);        // offset code 2: 4 + 2 extra bits
    body += char(0);        // match length code 0: 3
    body += char(0x05);     // the closing bit, then 01
    auto r = zstd::decompress(bytes(frame(body)));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), "abcdcdc");
    zstd::reader rd(dribble{frame(body), 2});
    auto all = rd.read_all();
    ASSERT_TRUE(all) << all.error().message();
    EXPECT_EQ(text(*all), "abcdcdc");
    std::string zero;
    zero += char(0);        // no literals
    zero += char(1);
    zero += char(0x54);
    zero += char(0);        // literal length 0
    zero += char(1);        // offset code 1: 2 + 1 extra bit
    zero += char(0);
    zero += char(0x03);     // the closing bit, then 1: the value 3
    r = zstd::decompress(bytes(frame(zero)));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::corrupt);
    EXPECT_EQ(r.error().message().find("a repeated offset of 0") == std::string::npos, false) << r.error().message();
}

// Every kind of level and the options, both ways through ourselves, over
// the corpus (text, random bytes that go raw, runs, lengths about a block)
TEST(Zstd_Tests, EveryLevelAndOptionRoundTrips) {
    auto corp = corpus();
    corp.push_back({"prose", prose(400000)});
    corp.push_back({"block and one", prose((128 << 10) + 1, 2)});
    corp.push_back({"runs", std::string(300000, 'a') + std::string(1000, 'b') + std::string(200000, 'a')});
    corp.push_back({"tiny", "abcabcabcabcabca"});
    for (auto& [name, plain] : corp) {
        for (int level : {1, 2, 3, 4, 5, 6, 8, 9, 12, 13, 15, 16, 18, 19, 22, -1, -7, -131072}) {
            if (level >= 13 && plain.size() > 150000) {
                continue;   // the slowest levels over the smaller inputs
            }
            SCOPED_TRACE(name + " " + tag(level));
            for (bool checksum : {false, true}) {
                zstd::options o {.level = level, .checksum = checksum, .content_size = !checksum};
                auto c = zstd::compress(bytes(plain), o);
                auto d = zstd::decompress(c);
                ASSERT_TRUE(d) << d.error().message();
                ASSERT_EQ(text(*d), plain);
                EXPECT_EQ(zstd::content_size(c).has_value(), !checksum);
            }
        }
        // the window's size given
        for (uint8_t w : {uint8_t(10), uint8_t(17), uint8_t(27)}) {
            SCOPED_TRACE(name + " window_log " + std::to_string(w));
            auto c = zstd::compress(bytes(plain), {.level = 5, .window_log = w});
            auto d = zstd::decompress(c);
            ASSERT_TRUE(d) << d.error().message();
            ASSERT_EQ(text(*d), plain);
        }
    }
}

// The levels order the sizes as their scale says, on text
TEST(Zstd_Tests, LevelsTradeSpeedForSize) {
    const std::string plain = prose(1 << 20, 3);
    auto size = [&](int level) { return zstd::compress(bytes(plain), {.level = level}).size(); };
    const size_t fastest = size(-50), one = size(1), three = size(3), nine = size(9), sixteen = size(16), nineteen = size(19);
    EXPECT_GT(fastest, one);
    EXPECT_GT(one, three);
    EXPECT_GT(three, nine);
    EXPECT_GT(nine, sixteen);
    EXPECT_GE(sixteen, nineteen);
    EXPECT_LT(one, plain.size() / 2);
}

#if SGCL_TEST_ZSTD
namespace {
    std::string theirs_compress(const std::string& plain, int level, bool checksum, bool size, const std::string& dict = "") {
        ZSTD_CCtx* c = ZSTD_createCCtx();
        ZSTD_CCtx_setParameter(c, ZSTD_c_compressionLevel, level);
        ZSTD_CCtx_setParameter(c, ZSTD_c_checksumFlag, checksum);
        ZSTD_CCtx_setParameter(c, ZSTD_c_contentSizeFlag, size);
        if (!dict.empty()) {
            ZSTD_CCtx_loadDictionary(c, dict.data(), dict.size());
        }
        std::string out(ZSTD_compressBound(plain.size()), 0);
        if (size) {
            out.resize(ZSTD_compress2(c, out.data(), out.size(), plain.data(), plain.size()));
        } else {
            // a stream: the size not known to the header
            ZSTD_inBuffer in {plain.data(), plain.size(), 0};
            ZSTD_outBuffer o {out.data(), out.size(), 0};
            size_t left;
            do {
                left = ZSTD_compressStream2(c, &o, &in, ZSTD_e_end);
            } while (left != 0 && !ZSTD_isError(left));
            out.resize(o.pos);
        }
        ZSTD_freeCCtx(c);
        return out;
    }

    bool theirs_decompress(const std::string& frame, const std::string& dict, std::string& out) {
        ZSTD_DCtx* d = ZSTD_createDCtx();
        if (!dict.empty()) {
            ZSTD_DCtx_loadDictionary(d, dict.data(), dict.size());
        }
        out.clear();
        std::string buf(1 << 17, 0);
        ZSTD_inBuffer in {frame.data(), frame.size(), 0};
        bool ok = true;
        size_t r = 0;
        for (;;) {
            ZSTD_outBuffer o {buf.data(), buf.size(), 0};
            r = ZSTD_decompressStream(d, &o, &in);
            if (ZSTD_isError(r)) {
                ok = false;
                break;
            }
            out.append(buf.data(), o.pos);
            if (r == 0 ? in.pos == in.size : (in.pos == in.size && o.pos == 0)) {
                break;   // the last frame done, or no input and no progress
            }
        }
        ZSTD_freeDCtx(d);
        return ok && r == 0;
    }

    // A dictionary trained by libzstd on pieces of text like the data
    std::string trained(unsigned seed, size_t capacity = 16384) {
        std::string samples;
        std::vector<size_t> sizes;
        std::mt19937 rng(seed);
        for (int i = 0; i < 400; ++i) {
            std::string s = prose(300 + rng() % 700, seed * 1000 + unsigned(i));
            samples += s;
            sizes.push_back(s.size());
        }
        std::string dict(capacity, 0);
        size_t n = ZDICT_trainFromBuffer(dict.data(), dict.size(), samples.data(), sizes.data(), unsigned(sizes.size()));
        if (ZDICT_isError(n)) {
            return std::string();
        }
        dict.resize(n);
        return dict;
    }
}

// Ours read by libzstd and libzstd's by ours: every kind of level, with and
// without the checksum and the size, frames and streams
TEST(Zstd_Tests, BothWaysWithLibzstd) {
    auto corp = corpus();
    corp.push_back({"prose", prose(1 << 20, 4)});
    for (auto& [name, plain] : corp) {
        for (int level : {-5, 1, 3, 5, 7, 9, 12, 13, 16, 19}) {
            if (level >= 13 && plain.size() > 300000) {
                continue;
            }
            SCOPED_TRACE(name + " " + tag(level));
            for (bool checksum : {false, true}) {
                auto ours = text(zstd::compress(bytes(plain), {.level = level, .checksum = checksum, .content_size = checksum}));
                std::string back;
                EXPECT_TRUE(theirs_decompress(ours, "", back));
                EXPECT_EQ(back, plain);
                auto theirs = theirs_compress(plain, level, checksum, !checksum);
                auto d = zstd::decompress(bytes(theirs));
                ASSERT_TRUE(d) << d.error().message();
                EXPECT_EQ(text(*d), plain);
                zstd::reader r(dribble{theirs, 4099});
                auto all = r.read_all();
                ASSERT_TRUE(all) << all.error().message();
                EXPECT_EQ(text(*all), plain);
            }
        }
    }
}

// Every shape of copy the decoder takes apart: runs of literals of 0 to
// 100 bytes (about the 16 and 32 a copy moves at once) and matches of 3 to
// 300 bytes from 1 to 80 back (the patterns of 1, 2, 4 bytes, the other
// near ones, 16 to 31, 32 and more), made by libzstd at fast and optimal
// levels and read back whole and in pieces
TEST(Zstd_Tests, EveryCopyShapeFromLibzstd) {
    std::mt19937 rng(17);
    std::string plain;
    auto noise = [&](size_t n) {
        for (size_t i = 0; i < n; ++i) {
            plain += char('A' + rng() % 52);
        }
    };
    noise(100);
    for (size_t back = 1; back <= 80; ++back) {
        for (size_t length : {3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 47, 64, 65, 100, 300}) {
            noise(std::array<size_t, 10> {0, 1, 5, 15, 16, 17, 31, 32, 33, 100}[rng() % 10]);
            for (size_t i = 0; i < length; ++i) {
                plain += plain[plain.size() - back];
            }
        }
    }
    for (int level : {1, 3, 19}) {
        SCOPED_TRACE(tag(level));
        auto theirs = theirs_compress(plain, level, true, true);
        auto d = zstd::decompress(bytes(theirs));
        ASSERT_TRUE(d) << d.error().message();
        EXPECT_EQ(text(*d), plain);
        zstd::reader r(dribble{theirs, 777});
        auto all = r.read_all();
        ASSERT_TRUE(all) << all.error().message();
        EXPECT_EQ(text(*all), plain);
    }
}

// Our sizes beside libzstd's at the same levels, on text (the project's
// design notes): no more than a few per cent larger
TEST(Zstd_Tests, SizesBesideLibzstd) {
    std::string plain;
    {
        std::ifstream is(std::string(SGCL_TEST_SOURCE_ROOT) + "/DESIGN.md", std::ios::binary);
        std::stringstream ss;
        ss << is.rdbuf();
        plain = ss.str();
    }
    ASSERT_GT(plain.size(), 100000u);
    for (int level : {1, 3, 5, 9, 12, 16, 19}) {
        const size_t ours = zstd::compress(bytes(plain), {.level = level}).size();
        const size_t theirs = theirs_compress(plain, level, true, true).size();
        EXPECT_LE(ours, theirs + theirs * 3 / 100) << level;
    }
}

// Records of one shape (a JSON array of 64 KB whose items differ in a few
// digits) at the greedy level: the last offset again a byte on taken
// before a search, as libzstd does; without it the parse took longer
// matches farther back and twice libzstd's size
TEST(Zstd_Tests, GreedyKeepsTheRepeatOnRecords) {
    std::string json = "[";
    for (size_t i = 0; json.size() < 65536; ++i) {
        json += "{\"id\":" + std::to_string(i) + ",\"name\":\"item " + std::to_string(i) + "\",\"ok\":true},";
    }
    json.resize(65536);
    const auto ours = zstd::compress(bytes(json), {.level = 5});
    const size_t theirs = theirs_compress(json, 5, true, true).size();
    EXPECT_LE(ours.size(), theirs + theirs * 5 / 100);
    auto back = zstd::decompress(ours);
    ASSERT_TRUE(back) << back.error().message();
    EXPECT_EQ(text(*back), json);
    EXPECT_LE(zstd::compress(bytes(json), {.level = 5}).size(), zstd::compress(bytes(json), {.level = 3}).size());
}

// Dictionaries of zstd's format (trained by libzstd) and of raw content,
// both ways, in memory and in streams; the id written or left out
TEST(Zstd_Tests, DictionariesWithLibzstd) {
    const std::string dict = trained(5);
    ASSERT_FALSE(dict.empty());
    auto parsed = zstd::dictionary::parse(bytes(dict));
    ASSERT_TRUE(parsed) << parsed.error().message();
    const zstd::dictionary d = *parsed;
    EXPECT_EQ(d.id(), ZDICT_getDictID(dict.data(), dict.size()));
    EXPECT_GT(d.size(), 0u);
    EXPECT_LT(d.size(), dict.size());
    const std::string plain = prose(3000, 77);
    for (int level : {1, 3, 9, 19, -2}) {
        SCOPED_TRACE(tag(level));
        auto ours = text(zstd::compress(bytes(plain), {.level = level, .dictionary = d}));
        auto without = zstd::compress(bytes(plain), {.level = level});
        EXPECT_LT(ours.size(), without.size());
        EXPECT_EQ(zstd::dictionary_id(bytes(ours)), d.id());
        std::string back;
        EXPECT_TRUE(theirs_decompress(ours, dict, back));
        EXPECT_EQ(back, plain);
        auto mine = zstd::decompress(bytes(ours), {.dictionary = d});
        ASSERT_TRUE(mine) << mine.error().message();
        EXPECT_EQ(text(*mine), plain);
        for (bool size : {false, true}) {
            auto theirs = theirs_compress(plain, level, true, size, dict);
            auto r = zstd::decompress(bytes(theirs), {.dictionary = d});
            ASSERT_TRUE(r) << r.error().message();
            EXPECT_EQ(text(*r), plain);
            zstd::reader rd(dribble{theirs, 100}, {.dictionary = d});
            auto all = rd.read_all();
            ASSERT_TRUE(all) << all.error().message();
            EXPECT_EQ(text(*all), plain);
        }
    }
    // the id left out: the frame names none, the dictionary still needed
    auto anonymous = text(zstd::compress(bytes(plain), {.dictionary = d, .write_dictionary_id = false}));
    EXPECT_EQ(zstd::dictionary_id(bytes(anonymous)), 0u);
    std::string back;
    EXPECT_TRUE(theirs_decompress(anonymous, dict, back));
    EXPECT_EQ(back, plain);
    EXPECT_EQ(text(*zstd::decompress(bytes(anonymous), {.dictionary = d})), plain);
    // raw content: any bytes not of the format, id 0
    const std::string content = prose(20000, 77);
    zstd::dictionary raw(bytes(content));
    EXPECT_EQ(raw.id(), 0u);
    EXPECT_EQ(raw.size(), content.size());
    auto ours = text(zstd::compress(bytes(plain), {.dictionary = raw}));
    EXPECT_LT(ours.size(), plain.size() / 4);
    EXPECT_TRUE(theirs_decompress(ours, content, back));
    EXPECT_EQ(back, plain);
    auto theirs = theirs_compress(plain, 3, true, true, content);
    EXPECT_EQ(text(*zstd::decompress(bytes(theirs), {.dictionary = raw})), plain);
    // a writer with a dictionary, in pieces
    io::buffer sink;
    zstd::writer w(sink, {.level = 6, .dictionary = d});
    for (size_t i = 0; i < plain.size(); i += 333) {
        ASSERT_TRUE(w.write(bytes(plain.substr(i, 333))));
    }
    ASSERT_TRUE(w.close());
    EXPECT_TRUE(theirs_decompress(contents(sink), dict, back));
    EXPECT_EQ(back, plain);
}

// zstd's own command reads what ours writes, with checksums and sizes
TEST(Zstd_Tests, LibzstdSimpleCalls) {
    const std::string plain = prose(500000, 12);
    auto ours = text(zstd::compress(bytes(plain), {.level = 19}));
    EXPECT_EQ(ZSTD_getFrameContentSize(ours.data(), ours.size()), plain.size());
    std::string back(plain.size(), 0);
    EXPECT_EQ(ZSTD_decompress(back.data(), back.size(), ours.data(), ours.size()), plain.size());
    EXPECT_EQ(back, plain);
    EXPECT_EQ(ZSTD_findFrameCompressedSize(ours.data(), ours.size()), ours.size());
}
#endif

// A dictionary on our own: the frame names one, and the one given must be it
TEST(Zstd_Tests, DictionaryRequired) {
    // a dictionary of zstd's format written by hand: the magic, an id, then
    // no valid tables: damaged
    std::string bad;
    le32(bad, 0xEC30A437);
    le32(bad, 1234);
    bad += std::string(40, '\x07');
    auto p = zstd::dictionary::parse(bytes(bad));
    ASSERT_FALSE(p);
    EXPECT_EQ(p.error().code(), compress::errc::corrupt);
    EXPECT_THROW(zstd::dictionary{bytes(bad)}, sgcl::bad_expected_access<compress::error>);
    // a raw dictionary: frames with no id, read with it and not without
    const std::string content = prose(30000, 8);
    const std::string plain = prose(60000, 8);
    zstd::dictionary raw(bytes(content));
    auto c = zstd::compress(bytes(plain), {.dictionary = raw});
    EXPECT_EQ(zstd::dictionary_id(c), 0u);
    auto r = zstd::decompress(c, {.dictionary = raw});
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), plain);
    // without it, the matches reach before the data
    auto none = zstd::decompress(c);
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), compress::errc::corrupt);
    // the empty handle is none; a copy shares the dictionary
    zstd::dictionary empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.id(), 0u);
    EXPECT_EQ(empty.size(), 0u);
    EXPECT_TRUE(zstd::dictionary::parse(bytes(std::string()))->empty());
    zstd::dictionary copy = raw;
    EXPECT_EQ(copy.size(), raw.size());
    zstd::dictionary moved = std::move(copy);
    EXPECT_EQ(moved.size(), raw.size());
    EXPECT_EQ(text(*zstd::decompress(c, {.dictionary = moved})), plain);
#if SGCL_TEST_ZSTD
    // a frame naming an id: read without a dictionary, or with another one
    const std::string dict = trained(6);
    zstd::dictionary d(bytes(dict));
    auto named = zstd::compress(bytes(plain), {.dictionary = d});
    EXPECT_EQ(zstd::dictionary_id(named), d.id());
    auto without = zstd::decompress(named);
    ASSERT_FALSE(without);
    EXPECT_EQ(without.error().code(), compress::errc::dictionary_required);
    zstd::dictionary other(bytes(trained(7)));
    ASSERT_NE(other.id(), d.id());
    auto wrong = zstd::decompress(named, {.dictionary = other});
    ASSERT_FALSE(wrong);
    EXPECT_EQ(wrong.error().code(), compress::errc::dictionary_required);
    zstd::reader rd(dribble{text(named), 1000});
    EXPECT_FALSE(rd.read_all());
    ASSERT_TRUE(rd.last_error());
    EXPECT_EQ(rd.last_error()->code(), compress::errc::dictionary_required);
#endif
}

// Damage: the header's bits, a reserved block type, a block past the
// window, the checksum, the size, bytes after the frame; every prefix of a
// frame ends early, cleanly, in memory and in a stream
TEST(Zstd_Tests, DamageIsFound) {
    const std::string plain = prose(300000, 5);
    const std::string c = text(zstd::compress(bytes(plain), {.level = 3}));
    std::string bad = c;
    bad[0] = 'X';
    EXPECT_EQ(code_of(bad), compress::errc::invalid_header);
    bad = c;
    bad[4] |= 0x08;   // the reserved bit of the descriptor
    EXPECT_EQ(code_of(bad), compress::errc::invalid_header);
    bad = c;
    bad[bad.size() - 1] ^= 0x01;   // the checksum
    EXPECT_EQ(code_of(bad), compress::errc::checksum);
    // damage inside the blocks: found as corrupt or as a wrong checksum,
    // never a crash and never data passed as good
    std::mt19937 rng(11);
    for (int i = 0; i < 300; ++i) {
        bad = c;
        const size_t at = 10 + rng() % (bad.size() - 14);
        bad[at] = char(bad[at] ^ (1 << (rng() % 8)));
        EXPECT_FALSE(zstd::decompress(bytes(bad))) << at;
    }
    // a block of the reserved type, and a block larger than the window
    std::string f;
    le32(f, 0xFD2FB528);
    f += char(0x00);
    f += char(0x00);   // window 1 KB
    le24(f, (3u << 1) | 1u);
    EXPECT_EQ(code_of(f), compress::errc::corrupt);
    std::string big;
    le32(big, 0xFD2FB528);
    big += char(0x00);
    big += char(0x00);
    le24(big, (2000u << 3) | 1u);
    big += std::string(2000, 'x');
    EXPECT_EQ(code_of(big), compress::errc::corrupt);
    // the content size in the header differs from the content
    std::string sized;
    le32(sized, 0xFD2FB528);
    sized += char(0x20);
    sized += char(6);
    le24(sized, (5u << 3) | 1u);
    sized += "hello";
    EXPECT_EQ(code_of(sized), compress::errc::corrupt);
    // after the frame, bytes that are no frame
    EXPECT_EQ(code_of(c + "garbage"), compress::errc::invalid_header);
    // every prefix fails as an early end, never anything else
    for (size_t n = 0; n < c.size(); n += (n < 100 ? 1 : 997)) {
        EXPECT_EQ(code_of(c.substr(0, n)), compress::errc::unexpected_end) << n;
        zstd::reader rd(dribble{c.substr(0, n), 4096});
        auto all = rd.read_all();
        ASSERT_FALSE(all) << n;
        ASSERT_TRUE(rd.last_error());
        EXPECT_EQ(rd.last_error()->code(), compress::errc::unexpected_end) << n;
    }
}

// The limits: the size, and the memory a frame's window asks for
TEST(Zstd_Tests, TheLimits) {
    const std::string plain = prose(600000, 6);
    auto c = zstd::compress(bytes(plain), {.level = 19});
    EXPECT_EQ(zstd::decompress(c, compress::limits{.max_size = plain.size() - 1}).error().code(), compress::errc::too_large);
    EXPECT_TRUE(zstd::decompress(c, compress::limits{.max_size = plain.size()}));
    // a window of 8 MB (a writer's at level 19) past a memory limit of 1 MB
    io::buffer sink;
    zstd::writer w(sink, {.level = 19});
    ASSERT_TRUE(w.write(bytes(plain)));
    ASSERT_TRUE(w.close());
    const std::string unsized = contents(sink);
    EXPECT_FALSE(zstd::content_size(bytes(unsized)));
    EXPECT_EQ(text(*zstd::decompress(bytes(unsized))), plain);
    auto r = zstd::decompress(bytes(unsized), compress::limits{.max_memory = 1 << 20});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    zstd::reader rd(dribble{unsized, 1000}, compress::limits{.max_memory = 1 << 20});
    EXPECT_FALSE(rd.read_all());
    EXPECT_EQ(rd.last_error()->code(), compress::errc::too_large);
    // a smaller window passes the same limit
    io::buffer small;
    zstd::writer ws(small, {.level = 3, .window_log = 18});
    ASSERT_TRUE(ws.write(bytes(plain)));
    ASSERT_TRUE(ws.close());
    EXPECT_EQ(text(*zstd::decompress(bytes(contents(small)), compress::limits{.max_memory = 1 << 20})), plain);
    // the largest window descriptor (2^41 and more) is no frame
    std::string huge;
    le32(huge, 0xFD2FB528);
    huge += char(0x00);
    huge += char(0xFF);
    le24(huge, 1u);
    EXPECT_NE(code_of(huge), compress::errc{});
}

// The level's range and the options' checks
TEST(Zstd_Tests, OptionsAtTheirEnds) {
    for (int v : {1, 3, 19, 22, -1, -131072}) {
        EXPECT_EQ(zstd::level(v).value(), v);
    }
    for (int v : {0, 23, -131073, INT_MIN, INT_MAX}) {
        EXPECT_THROW(zstd::level{v}, std::invalid_argument);
    }
    EXPECT_EQ(zstd::level().value(), zstd::level::standard);
    EXPECT_EQ(zstd::level(zstd::level::fastest).value(), -131072);
    EXPECT_EQ(zstd::level(zstd::level::ultra).value(), 22);
    for (uint8_t w : {uint8_t(9), uint8_t(32), uint8_t(255)}) {
        EXPECT_THROW(zstd::compress(bytes(std::string("x")), {.window_log = w}), std::invalid_argument);
        io::buffer out;
        zstd::writer wr(out, {.window_log = w});
        auto r = wr.write(std::string("x"));
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), make_error_code(compress::errc::invalid_argument));
        EXPECT_EQ(out.size(), 0u);
    }
    for (uint8_t w : {uint8_t(10), uint8_t(31)}) {
        auto c = zstd::compress(bytes(std::string("abcabcabc")), {.window_log = w});
        EXPECT_EQ(text(*zstd::decompress(c)), "abcabcabc");
    }
    io::buffer out;
    zstd::writer wr(out, {.window_log = 10});
    ASSERT_TRUE(wr.write(bytes(prose(5000))));
    ASSERT_TRUE(wr.close());
    EXPECT_EQ(text(*zstd::decompress(bytes(contents(out)))), prose(5000));
    // the text overloads
    EXPECT_EQ(text(*zstd::decompress(zstd::compress("hello, hello, hello", {.level = 19}))), "hello, hello, hello");
    EXPECT_EQ(text(*zstd::decompress(zstd::compress(sgcl::string("twice twice")))), "twice twice");
}

// The writer in pieces of every size, flushed now and then: each flush
// makes everything so far decodable
TEST(Zstd_Tests, StreamsInPieces) {
    const std::string plain = prose(900000, 7);
    for (int level : {-3, 1, 3, 7, 12, 16}) {
        SCOPED_TRACE(tag(level));
        io::buffer sink;
        zstd::writer w(sink, {.level = level});
        std::mt19937 rng(unsigned(level + 100));
        size_t at = 0;
        while (at < plain.size()) {
            size_t k = std::min<size_t>(rng() % 150000, plain.size() - at);
            ASSERT_TRUE(w.write(bytes(plain.substr(at, k))));
            at += k;
            if (rng() % 4 == 0) {
                ASSERT_TRUE(w.flush());
                zstd::reader partial(dribble{contents(sink), 5000});
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
        zstd::reader r(dribble{contents(sink), 777});
        auto all = r.read_all();
        ASSERT_TRUE(all) << all.error().message();
        EXPECT_EQ(text(*all), plain);
        EXPECT_EQ(text(*zstd::decompress(bytes(contents(sink)))), plain);
    }
}

TEST(Zstd_Tests, TheAsyncForms) {
    const std::string plain = prose(500000, 8);
    auto task = sgcl::async::spawn([](std::string t) -> sgcl::async::task<std::string> {
        io::buffer sink;
        zstd::writer w(sink, {.level = 5});
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
        zstd::reader r(dribble{contents(sink), 1000});
        auto all = co_await r.async_read_all();
        co_return all ? text(*all) : std::string("read failed");
    }(plain));
    EXPECT_EQ(task.wait(), plain);
    sgcl::async::scheduler::stop();
}

// The engine's indexes near 2^31: the tables shifted down, the frame the same
TEST(Zstd_Tests, IndexesRebasedPastTwoToThe31) {
    const std::string plain = prose(700000, 10);
    for (int level : {1, 3, 6, 13, 17}) {
        SCOPED_TRACE(tag(level));
        namespace d = sgcl::compress::detail;
        d::ZstdSettings s;
        s.level = level;
        s.checksum = false;
        const d::ZstdParams params = d::zstd_params(level, UINT64_MAX, 17);
        std::vector<uint8_t> out;
        d::zstd_frame_header(out, s, params.window_log, nullptr, 0);
        auto e = std::make_unique<d::ZstdEngine>();
        e->matcher.configure(params);
        e->next = (uint32_t(1) << 31) - 300000;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(plain.data());
        const size_t window = size_t(1) << params.window_log;
        size_t at = 0;
        do {
            const size_t k = std::min(d::ZstdBlockMax, plain.size() - at);
            const size_t keep = std::min(at, window);
            const uint32_t start = e->room(k, keep);
            EXPECT_LT(uint64_t(start) + k, uint64_t(1) << 31);
            const uint8_t* src = p + at;
            e->compress(out, src - start, p, src, k, at + k == plain.size());
            at += k;
        } while (at < plain.size());
        auto back = zstd::decompress(sgcl::slice<const byte>(reinterpret_cast<const byte*>(out.data()), out.size()));
        ASSERT_TRUE(back) << back.error().message();
        EXPECT_EQ(text(*back), plain);
    }
}
