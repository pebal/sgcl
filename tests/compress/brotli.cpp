//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// brotli: streams both ways at every quality and window, against libbrotli
// (when the build found it) and on their own; streams written by hand from
// RFC 7932 (the empty stream, stored and metadata meta-blocks, the window
// descriptors); the static dictionary's words and transforms through
// libbrotli's streams; damage, every prefix, the limits; the streams in
// pieces, flushed, in a task.
#include "common.h"

#if SGCL_TEST_BROTLI
#include <brotli/decode.h>
#include <brotli/encode.h>
#endif

using namespace compress_test;

namespace {
    using sgcl::byte;
    namespace io = sgcl::io;
    using brotli = compress::brotli;

    std::string contents(const io::buffer& b) {
        auto d = b.data();
        return std::string(reinterpret_cast<const char*>(d.data()), d.size());
    }

    std::string prose(size_t n, unsigned seed = 1) {
        std::mt19937 rng(seed);
        static const char* words[] = {"the ", "window ", "block ", "of ", "brotli ", "and ", "a ", "context\n", "literal ", "distance, ", "Huffman ",
                                      "dictionary "};
        std::string s;
        while (s.size() < n) {
            s += words[rng() % 12];
            if (rng() % 40 == 0 && s.size() > 1000) {
                s += s.substr(rng() % (s.size() - 500), 200 + rng() % 300);
            }
            if (rng() % 300 == 0) {
                s += std::to_string(rng());
            }
        }
        s.resize(n);
        return s;
    }

    // Bits least significant first, as a stream is written
    struct bits {
        std::string out;
        uint64_t acc = 0;
        unsigned n = 0;

        void put(uint64_t v, unsigned k) {
            acc |= v << n;
            n += k;
            while (n >= 8) {
                out += char(uint8_t(acc));
                acc >>= 8;
                n -= 8;
            }
        }

        void align() {
            put(0, (8 - n % 8) % 8);
        }
    };

    compress::errc code_of(const std::string& s, const compress::limits& l = {}) {
        auto r = brotli::decompress(bytes(s), l);
        return r ? compress::errc{} : r.error().code();
    }

    std::string tag(int q) {
        return "quality " + std::to_string(q);
    }
}

// Streams by hand (RFC 7932, 9): the empty stream; a stored meta-block and
// a metadata meta-block before the empty last one; every window descriptor
TEST(Brotli_Tests, StreamsByHand) {
    // WBITS 16 (a 0 bit), ISLAST, ISLASTEMPTY
    auto r = brotli::decompress(bytes(std::string("\x06", 1)));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(r->size(), 0u);
    // a metadata meta-block of 3 bytes, a stored one of "hello", the end
    bits b;
    b.put(0, 1);        // WBITS 16
    b.put(0, 1);        // ISLAST 0
    b.put(3, 2);        // MNIBBLES 0: metadata
    b.put(0, 1);        // reserved
    b.put(1, 2);        // MSKIPBYTES 1
    b.put(2, 8);        // MSKIPLEN - 1
    b.align();
    b.out += "xyz";
    b.put(0, 1);        // ISLAST 0
    b.put(0, 2);        // MNIBBLES 4
    b.put(4, 16);       // MLEN - 1
    b.put(1, 1);        // ISUNCOMPRESSED
    b.align();
    b.out += "hello";
    b.put(1, 1);        // ISLAST
    b.put(1, 1);        // ISLASTEMPTY
    b.align();
    r = brotli::decompress(bytes(b.out));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), "hello");
    brotli::reader rd(dribble{b.out, 1});
    auto all = rd.read_all();
    ASSERT_TRUE(all) << all.error().message();
    EXPECT_EQ(text(*all), "hello");
    // every window: 16, 18..24 (1 and three bits), 10..15 and 17 (1, 000 and three bits)
    for (unsigned wbits : {10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 24u}) {
        bits e;
        if (wbits == 16) {
            e.put(0, 1);
        } else if (wbits >= 18) {
            e.put(1, 1);
            e.put(wbits - 17, 3);
        } else {
            e.put(1, 1);
            e.put(0, 3);
            e.put(wbits == 17 ? 0 : wbits - 8, 3);
        }
        e.put(3, 2);   // ISLAST, ISLASTEMPTY
        e.align();
        EXPECT_TRUE(brotli::decompress(bytes(e.out))) << wbits;
    }
    // the large-window escape (1, 000, 001) is not RFC 7932
    bits large;
    large.put(1, 1);
    large.put(0, 3);
    large.put(1, 3);
    large.put(3, 2);
    large.align();
    EXPECT_EQ(code_of(large.out), compress::errc::invalid_header);
    // ours of nothing reads back as nothing
    EXPECT_EQ(brotli::decompress(brotli::compress(bytes(std::string())))->size(), 0u);
}

// Every quality, the windows at their ends, both ways through ourselves
TEST(Brotli_Tests, EveryQualityAndWindowRoundTrips) {
    auto corp = corpus();
    corp.push_back({"prose", prose(400000)});
    corp.push_back({"runs", std::string(300000, 'a') + std::string(1000, 'b') + std::string(200000, 'a')});
    corp.push_back({"tiny", "abcabcabcabcabca"});
    for (auto& [name, plain] : corp) {
        for (int q = 0; q <= 11; ++q) {
            if (q >= 10 && plain.size() > 150000) {
                continue;
            }
            for (uint8_t w : {uint8_t(10), uint8_t(16), uint8_t(24)}) {
                SCOPED_TRACE(name + " " + tag(q) + " window_log " + std::to_string(w));
                auto c = brotli::compress(bytes(plain), {.level = q, .window_log = w});
                auto d = brotli::decompress(c);
                ASSERT_TRUE(d) << d.error().message();
                ASSERT_EQ(text(*d), plain);
            }
        }
    }
}

// The qualities order the sizes, on text
TEST(Brotli_Tests, QualitiesTradeSpeedForSize) {
    const std::string plain = prose(1 << 20, 3);
    auto size = [&](int q) { return brotli::compress(bytes(plain), {.level = q}).size(); };
    const size_t one = size(1), three = size(3), five = size(5), nine = size(9), eleven = size(11);
    EXPECT_GT(one, three);
    EXPECT_GT(three, five);
    EXPECT_GT(five, nine);
    EXPECT_GE(nine, eleven);
    EXPECT_LT(one, plain.size() / 2);
}

// Records of one shape (a JSON array of 64 KB whose items differ in a few
// digits): quality 4 between 3 and 5 in size, not past 3 as when its
// greedy parse lost the last distance to a longer match farther back
TEST(Brotli_Tests, QualitiesInOrderOnRecords) {
    std::string json = "[";
    for (size_t i = 0; json.size() < 65536; ++i) {
        json += "{\"id\":" + std::to_string(i) + ",\"name\":\"item " + std::to_string(i) + "\",\"ok\":true},";
    }
    json.resize(65536);
    auto size = [&](int q) { return brotli::compress(bytes(json), {.level = q}).size(); };
    const size_t three = size(3), four = size(4), five = size(5);
    EXPECT_LE(four, three);
    EXPECT_LE(five, four);
    auto back = brotli::decompress(brotli::compress(bytes(json), {.level = 4}));
    ASSERT_TRUE(back) << back.error().message();
    EXPECT_EQ(text(*back), json);
}

#if SGCL_TEST_BROTLI
namespace {
    std::string theirs_compress(const std::string& plain, int q, int lgwin) {
        std::string out(BrotliEncoderMaxCompressedSize(plain.size()) + 64, 0);
        size_t n = out.size();
        BrotliEncoderCompress(q, lgwin, BROTLI_MODE_GENERIC, plain.size(), reinterpret_cast<const uint8_t*>(plain.data()), &n,
                              reinterpret_cast<uint8_t*>(out.data()));
        out.resize(n);
        return out;
    }

    bool theirs_decompress(const std::string& c, std::string& out) {
        BrotliDecoderState* s = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
        out.clear();
        std::string buf(1 << 16, 0);
        const uint8_t* in = reinterpret_cast<const uint8_t*>(c.data());
        size_t in_left = c.size();
        BrotliDecoderResult r;
        do {
            uint8_t* o = reinterpret_cast<uint8_t*>(buf.data());
            size_t o_left = buf.size();
            r = BrotliDecoderDecompressStream(s, &in_left, &in, &o_left, &o, nullptr);
            out.append(buf.data(), buf.size() - o_left);
        } while (r == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT);
        BrotliDecoderDestroyInstance(s);
        return r == BROTLI_DECODER_RESULT_SUCCESS && in_left == 0;
    }
}

// Ours read by libbrotli and libbrotli's by ours: every quality, several
// windows, in memory and through the reader in pieces
TEST(Brotli_Tests, BothWaysWithLibbrotli) {
    auto corp = corpus();
    corp.push_back({"prose", prose(1 << 20, 4)});
    for (auto& [name, plain] : corp) {
        for (int q : {0, 1, 2, 4, 5, 7, 9, 10, 11}) {
            if (q >= 10 && plain.size() > 200000) {
                continue;
            }
            for (int w : {10, 16, 22}) {
                SCOPED_TRACE(name + " " + tag(q) + " lgwin " + std::to_string(w));
                auto ours = text(brotli::compress(bytes(plain), {.level = q, .window_log = uint8_t(w)}));
                std::string back;
                EXPECT_TRUE(theirs_decompress(ours, back));
                EXPECT_EQ(back, plain);
                auto theirs = theirs_compress(plain, q, w);
                auto d = brotli::decompress(bytes(theirs));
                ASSERT_TRUE(d) << d.error().message();
                EXPECT_EQ(text(*d), plain);
                brotli::reader r(dribble{theirs, 4099});
                auto all = r.read_all();
                ASSERT_TRUE(all) << all.error().message();
                EXPECT_EQ(text(*all), plain);
            }
        }
    }
}

// libbrotli's streams of English, which use the static dictionary's words
// and their transforms (upper case, cut, with spaces and punctuation), read
// back; a text made of the dictionary's own words
TEST(Brotli_Tests, TheDictionaryThroughLibbrotli) {
    std::string english;
    std::mt19937 rng(21);
    static const char* words[] = {"Information", "the ", "Government", "international ", "however,", "available.", "COMMUNITY ", "management ",
                                  "experience ", "Development", "the following ", "University ", "<div class=\"", "\" />", "http://"};
    while (english.size() < 200000) {
        english += words[rng() % 15];
        if (rng() % 7 == 0) {
            english += ". ";
        }
    }
    for (int q : {2, 5, 9, 11}) {
        SCOPED_TRACE(tag(q));
        auto theirs = theirs_compress(q >= 10 ? english.substr(0, 50000) : english, q, 22);
        auto d = brotli::decompress(bytes(theirs));
        ASSERT_TRUE(d) << d.error().message();
        EXPECT_EQ(text(*d), q >= 10 ? english.substr(0, 50000) : english);
    }
}

// Our sizes beside libbrotli's at the same quality, on text (the design
// notes): 2..9 within a few per cent; 0 and 1, 10 and 11 within their known
// gaps (libbrotli's own fast compressors, its optimal parser with block
// splitting)
TEST(Brotli_Tests, SizesBesideLibbrotli) {
    std::string plain;
    {
        std::ifstream is(std::string(SGCL_TEST_SOURCE_ROOT) + "/DESIGN.md", std::ios::binary);
        std::stringstream ss;
        ss << is.rdbuf();
        plain = ss.str().substr(0, 1 << 20);
    }
    ASSERT_GT(plain.size(), 100000u);
    for (int q : {1, 2, 3, 4, 5, 7, 9, 10, 11}) {
        const size_t ours = brotli::compress(bytes(plain), {.level = q}).size();
        const size_t theirs = theirs_compress(plain, q, 22).size();
        const size_t slack = q <= 1 ? theirs * 8 / 100 : q >= 10 ? theirs * 6 / 100 : theirs * 4 / 100;
        EXPECT_LE(ours, theirs + slack) << q;
    }
}
#endif

// Damage: trailing bytes, padding that is not zero, a cut stream; every
// prefix ends early, cleanly, in memory and through the reader
TEST(Brotli_Tests, DamageIsFound) {
    const std::string plain = prose(300000, 5);
    const std::string c = text(brotli::compress(bytes(plain), {.level = 5}));
    EXPECT_EQ(code_of(c + "x"), compress::errc::corrupt);
    // the empty stream with a padding bit set: 0x06 | 0x80
    EXPECT_EQ(code_of(std::string("\x86", 1)), compress::errc::corrupt);
    // every prefix fails as an early end, never anything else
    for (size_t n = 0; n < c.size(); n += (n < 100 ? 1 : 997)) {
        EXPECT_EQ(code_of(c.substr(0, n)), compress::errc::unexpected_end) << n;
        brotli::reader rd(dribble{c.substr(0, n), 4096});
        auto all = rd.read_all();
        ASSERT_FALSE(all) << n;
        ASSERT_TRUE(rd.last_error());
        EXPECT_EQ(rd.last_error()->code(), compress::errc::unexpected_end) << n;
    }
    // bits flipped: a failure or some output, never a crash
    std::mt19937 rng(11);
    for (int i = 0; i < 300; ++i) {
        std::string bad = c;
        const size_t at = rng() % bad.size();
        bad[at] = char(bad[at] ^ (1 << (rng() % 8)));
        (void)brotli::decompress(bytes(bad));
    }
}

// The limits: the size, and the memory a stream's window asks for
TEST(Brotli_Tests, TheLimits) {
    const std::string plain = prose(600000, 6);
    auto c = brotli::compress(bytes(plain), {.level = 5, .window_log = 24});
    EXPECT_EQ(brotli::decompress(c, compress::limits{.max_size = plain.size() - 1}).error().code(), compress::errc::too_large);
    EXPECT_TRUE(brotli::decompress(c, compress::limits{.max_size = plain.size()}));
    // compress() makes the window no larger than the data needs; a writer's is the options'
    io::buffer sink;
    brotli::writer w(sink, {.level = 2, .window_log = 24});
    ASSERT_TRUE(w.write(bytes(plain)));
    ASSERT_TRUE(w.close());
    auto r = brotli::decompress(bytes(contents(sink)), compress::limits{.max_memory = 1 << 20});
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::too_large);
    brotli::reader rd(dribble{contents(sink), 1000}, compress::limits{.max_memory = 1 << 20});
    EXPECT_FALSE(rd.read_all());
    EXPECT_EQ(rd.last_error()->code(), compress::errc::too_large);
    EXPECT_EQ(text(*brotli::decompress(bytes(contents(sink)))), plain);
}

// The level's range and the options' checks
TEST(Brotli_Tests, OptionsAtTheirEnds) {
    for (int v : {0, 1, 5, 11}) {
        EXPECT_EQ(brotli::level(v).value(), v);
    }
    for (int v : {-1, 12, INT_MIN, INT_MAX}) {
        EXPECT_THROW(brotli::level{v}, std::invalid_argument);
    }
    EXPECT_EQ(brotli::level().value(), brotli::level::standard);
    EXPECT_EQ(brotli::level(brotli::level::fastest).value(), 0);
    for (uint8_t w : {uint8_t(9), uint8_t(25), uint8_t(255)}) {
        EXPECT_THROW(brotli::compress(bytes(std::string("x")), {.window_log = w}), std::invalid_argument);
        io::buffer out;
        brotli::writer wr(out, {.window_log = w});
        auto r = wr.write(std::string("x"));
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), make_error_code(compress::errc::invalid_argument));
        EXPECT_EQ(out.size(), 0u);
    }
    for (uint8_t w : {uint8_t(10), uint8_t(24)}) {
        auto c = brotli::compress(bytes(prose(5000)), {.level = 4, .window_log = w});
        EXPECT_EQ(text(*brotli::decompress(c)), prose(5000));
    }
    EXPECT_EQ(text(*brotli::decompress(brotli::compress("hello, hello, hello", {.level = 5}))), "hello, hello, hello");
    EXPECT_EQ(text(*brotli::decompress(brotli::compress(sgcl::string("twice twice")))), "twice twice");
}

// The writer in pieces of every size, flushed now and then: each flush
// makes everything so far decodable
TEST(Brotli_Tests, StreamsInPieces) {
    const std::string plain = prose(700000, 7);
    for (int q : {0, 2, 5, 9}) {
        SCOPED_TRACE(tag(q));
        io::buffer sink;
        brotli::writer w(sink, {.level = q, .window_log = 20});
        std::mt19937 rng(unsigned(q + 100));
        size_t at = 0;
        while (at < plain.size()) {
            size_t k = std::min<size_t>(rng() % 150000, plain.size() - at);
            ASSERT_TRUE(w.write(bytes(plain.substr(at, k))));
            at += k;
            if (rng() % 4 == 0) {
                ASSERT_TRUE(w.flush());
                brotli::reader partial(dribble{contents(sink), 5000});
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
        brotli::reader r(dribble{contents(sink), 777});
        auto all = r.read_all();
        ASSERT_TRUE(all) << all.error().message();
        EXPECT_EQ(text(*all), plain);
        EXPECT_EQ(text(*brotli::decompress(bytes(contents(sink)))), plain);
    }
}

TEST(Brotli_Tests, TheAsyncForms) {
    const std::string plain = prose(500000, 8);
    auto task = sgcl::async::spawn([](std::string t) -> sgcl::async::task<std::string> {
        io::buffer sink;
        brotli::writer w(sink, {.level = 5});
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
        brotli::reader r(dribble{contents(sink), 1000});
        auto all = co_await r.async_read_all();
        co_return all ? text(*all) : std::string("read failed");
    }(plain));
    EXPECT_EQ(task.wait(), plain);
    sgcl::async::scheduler::stop();
}
