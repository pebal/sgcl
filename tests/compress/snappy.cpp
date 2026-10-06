//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Snappy: blocks and framed streams written by hand from the specification
// (every kind of element and chunk), both ways through ourselves over the
// corpus, every kind of damage, the limits, the streams in pieces and in a
// task. No reference library is on this machine (no Homebrew snappy, none
// in Go's standard library): the vectors are the format's own rules.
#include "common.h"

using namespace compress_test;

namespace {
    using sgcl::byte;
    namespace io = sgcl::io;
    using snappy = compress::snappy;

    std::string contents(const io::buffer& b) {
        auto d = b.data();
        return std::string(reinterpret_cast<const char*>(d.data()), d.size());
    }

    std::string prose(size_t n, unsigned seed = 1) {
        std::mt19937 rng(seed);
        static const char* words[] = {"snappy ", "chunk ", "copy ", "of ", "a ", "literal ", "offset\n", "tag "};
        std::string s;
        while (s.size() < n) {
            s += words[rng() % 8];
        }
        s.resize(n);
        return s;
    }

    // The masked CRC-32C as framing_format.txt defines it
    uint32_t masked(const std::string& data) {
        uint32_t c = sgcl::hash::crc32c::of(bytes(data));
        return ((c >> 15) | (c << 17)) + 0xa282ead8u;
    }

    std::string le(uint32_t v, int n) {
        std::string s;
        for (int i = 0; i < n; ++i) {
            s += char(uint8_t(v >> (8 * i)));
        }
        return s;
    }

    const std::string identifier("\xff\x06\x00\x00sNaPpY", 10);

    std::string chunk(uint8_t type, const std::string& body) {
        return std::string(1, char(type)) + le(uint32_t(body.size()), 3) + body;
    }

    std::string block_text(const std::string& block, size_t limit = SIZE_MAX) {
        auto r = snappy::decompress_block(bytes(block), compress::limits{.max_size = limit});
        return r ? text(*r) : "error: " + std::string(r.error().message().view());
    }
}

// Every kind of element written by hand: literals of every length form,
// copies with offsets of one, two and four bytes, overlapping copies
TEST(Snappy_Tests, BlocksByHand) {
    EXPECT_EQ(block_text(std::string("\x00", 1)), "");
    EXPECT_EQ(block_text(std::string("\x04\x0c" "abcd", 6)), "abcd");
    // a 1-byte offset copy of 8 from 4 back: tag 1 | (8-4)<<2, offset 4
    EXPECT_EQ(block_text(std::string("\x0c\x0c" "abcd\x11\x04", 8)), "abcdabcdabcd");
    // a 2-byte offset copy of 6 from 4 back: tag (6-1)<<2 | 2
    EXPECT_EQ(block_text(std::string("\x0a\x0c" "abcd\x16\x04\x00", 9)), "abcdabcdab");
    // a 4-byte offset copy of 2 from 3 back
    EXPECT_EQ(block_text(std::string("\x05\x08" "xyz\x07\x03\x00\x00\x00", 10)), "xyzxy");
    // an offset of 1: a run
    EXPECT_EQ(block_text(std::string("\x07\x00" "a\x16\x01\x00", 6)), "aaaaaaa");
    // literals of 60 (tag 59<<2), 61 (one length byte, tag 60<<2), 300 (two bytes, tag 61<<2)
    for (size_t n : {60, 61, 256, 257, 300, 70000}) {
        std::string lit(n, 'q');
        std::string b;
        uint64_t v = n;
        while (v >= 0x80) {
            b += char(uint8_t(v | 0x80));
            v >>= 7;
        }
        b += char(uint8_t(v));
        size_t m = n - 1;
        if (m < 60) {
            b += char(uint8_t(m << 2));
        } else {
            int k = m < 256 ? 1 : m < 65536 ? 2 : 3;
            b += char(uint8_t((59 + k) << 2));
            b += le(uint32_t(m), k);
        }
        EXPECT_EQ(block_text(b + lit), lit) << n;
    }
    // what we write for those, and the length read from the head
    auto b = snappy::compress_block("abcdabcdabcdabcdabcdabcd");
    EXPECT_EQ(*snappy::decompressed_size(b), 24u);
    EXPECT_EQ(text(*snappy::decompress_block(b)), "abcdabcdabcdabcdabcdabcd");
    EXPECT_LT(b.size(), 24u);
}

// A framed stream written by hand: the identifier, an uncompressed chunk,
// a compressed one, padding and a skippable chunk, the identifier again
TEST(Snappy_Tests, FramesByHand) {
    const std::string hello = "hello";
    std::string s = identifier + chunk(0x01, le(masked(hello), 4) + hello);
    auto r = snappy::decompress(bytes(s));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), "hello");
    const std::string body = text(snappy::compress_block(", world"));
    s += chunk(0x00, le(masked(", world"), 4) + body);
    s += chunk(0xfe, std::string(7, '\0'));
    s += chunk(0x80, "anything");
    s += identifier + chunk(0x01, le(masked("!"), 4) + "!");
    r = snappy::decompress(bytes(s));
    ASSERT_TRUE(r) << r.error().message();
    EXPECT_EQ(text(*r), "hello, world!");
    snappy::reader rd(dribble{s, 3});
    auto all = rd.read_all();
    ASSERT_TRUE(all) << all.error().message();
    EXPECT_EQ(text(*all), "hello, world!");
    // ours begins with the identifier and checks the same way
    auto ours = text(snappy::compress("hello"));
    EXPECT_EQ(ours.substr(0, 10), identifier);
    EXPECT_EQ(ours, identifier + chunk(0x01, le(masked(hello), 4) + hello));   // five bytes go uncompressed
    // just the identifier: a stream of no data
    EXPECT_EQ(snappy::decompress(bytes(identifier))->size(), 0u);
}

TEST(Snappy_Tests, RoundTripsOverTheCorpus) {
    auto corp = corpus();
    corp.push_back({"prose", prose(500000)});
    for (auto& [name, plain] : corp) {
        SCOPED_TRACE(name);
        auto b = snappy::compress_block(bytes(plain));
        EXPECT_LE(b.size(), 32 + plain.size() + plain.size() / 6);
        EXPECT_EQ(*snappy::decompressed_size(b), plain.size());
        auto d = snappy::decompress_block(b);
        ASSERT_TRUE(d) << d.error().message();
        ASSERT_EQ(text(*d), plain);
        auto f = snappy::compress(bytes(plain));
        auto e = snappy::decompress(f);
        ASSERT_TRUE(e) << e.error().message();
        ASSERT_EQ(text(*e), plain);
        snappy::reader r(dribble{text(f), 1000});
        auto all = r.read_all();
        ASSERT_TRUE(all) << all.error().message();
        EXPECT_EQ(text(*all), plain);
    }
    // text compresses, random bytes go as they are (the framing stores them)
    auto t = prose(100000);
    EXPECT_LT(snappy::compress(bytes(t)).size(), t.size() / 2);
}

// Damage: elements cut short, copies before the start, lengths that differ,
// checksums, chunk types, the identifier; every prefix ends early, cleanly
TEST(Snappy_Tests, DamageIsFound) {
    auto code = [](const std::string& s) {
        auto r = snappy::decompress_block(bytes(s));
        return r ? compress::errc{} : r.error().code();
    };
    EXPECT_EQ(code(""), compress::errc::unexpected_end);
    EXPECT_EQ(code(std::string("\x80", 1)), compress::errc::unexpected_end);
    EXPECT_EQ(code(std::string("\xff\xff\xff\xff\xff\x01", 6)), compress::errc::corrupt);   // a varint past five bytes
    EXPECT_EQ(code(std::string("\xff\xff\xff\xff\x7f", 5)), compress::errc::corrupt);       // past 2^32
    EXPECT_EQ(code(std::string("\x04\x0c" "ab", 4)), compress::errc::unexpected_end);       // a literal cut
    EXPECT_EQ(code(std::string("\x05\x0c" "abcd", 6)), compress::errc::corrupt);            // less data than said
    EXPECT_EQ(code(std::string("\x03\x0c" "abcd", 6)), compress::errc::corrupt);            // more data than said
    EXPECT_EQ(code(std::string("\x08\x0c" "abcd\x01\x00", 8)), compress::errc::corrupt);    // offset 0
    EXPECT_EQ(code(std::string("\x08\x0c" "abcd\x01\x05", 8)), compress::errc::corrupt);    // before the start
    EXPECT_EQ(code(std::string("\x08\x0c" "abcd\x01", 7)), compress::errc::unexpected_end); // the offset cut
    EXPECT_EQ(code(std::string("\x08\x0c" "abcd\x0e\x04", 8)), compress::errc::unexpected_end);
    EXPECT_EQ(code(std::string("\x08\x0c" "abcd\x0f\x04\x00\x00", 10)), compress::errc::unexpected_end);
    auto fcode = [](const std::string& s) {
        auto r = snappy::decompress(bytes(s));
        return r ? compress::errc{} : r.error().code();
    };
    const std::string hello = "hello";
    const std::string good = identifier + chunk(0x01, le(masked(hello), 4) + hello);
    EXPECT_EQ(fcode(""), compress::errc::unexpected_end);
    EXPECT_EQ(fcode(chunk(0x01, le(masked(hello), 4) + hello)), compress::errc::invalid_header);   // no identifier
    EXPECT_EQ(fcode(std::string("\xff\x06\x00\x00sNaPpX", 10)), compress::errc::invalid_header);
    EXPECT_EQ(fcode(std::string("\xff\x05\x00\x00sNaPp", 9)), compress::errc::invalid_header);
    EXPECT_EQ(fcode(identifier + chunk(0x01, le(masked(hello) ^ 1, 4) + hello)), compress::errc::checksum);
    EXPECT_EQ(fcode(identifier + chunk(0x02, "x")), compress::errc::corrupt);    // reserved, not skippable
    EXPECT_EQ(fcode(identifier + chunk(0x7f, "x")), compress::errc::corrupt);
    EXPECT_EQ(fcode(identifier + chunk(0x01, "abc")), compress::errc::corrupt);   // no room for the checksum
    std::string big(65537, 'z');
    EXPECT_EQ(fcode(identifier + chunk(0x01, le(masked(big), 4) + big)), compress::errc::corrupt);
    auto big_block = text(snappy::compress_block(bytes(big)));
    EXPECT_EQ(fcode(identifier + chunk(0x00, le(masked(big), 4) + big_block)), compress::errc::corrupt);
    // a compressed chunk whose block is damaged
    auto body = text(snappy::compress_block(", world"));
    body[1] = char(0xfc);   // the first tag: a literal longer than the chunk
    EXPECT_EQ(fcode(identifier + chunk(0x00, le(masked(", world"), 4) + body)), compress::errc::corrupt);
    // after a good stream, bytes that are no chunk
    EXPECT_EQ(fcode(good + "\x01"), compress::errc::unexpected_end);
    // every prefix of a stream ends early, never anything else
    const std::string plain = prose(200000, 2);
    const std::string f = text(snappy::compress(bytes(plain)));
    for (size_t n = 1; n < f.size(); n += (n < 64 ? 1 : 1009)) {
        if (n == 10) {
            continue;   // the identifier alone is a whole stream
        }
        bool at_chunk = false;
        for (size_t at = 10; at < f.size(); at += 4 + (uint8_t(f[at + 1]) | uint8_t(f[at + 2]) << 8 | uint8_t(f[at + 3]) << 16)) {
            at_chunk = at_chunk || at == n;
        }
        auto r = snappy::decompress(bytes(f.substr(0, n)));
        if (at_chunk) {
            EXPECT_TRUE(r) << n;   // the stream ends between two chunks: what is there is whole
            continue;
        }
        ASSERT_FALSE(r) << n;
        EXPECT_EQ(r.error().code(), compress::errc::unexpected_end) << n;
        snappy::reader rd(dribble{f.substr(0, n), 4096});
        EXPECT_FALSE(rd.read_all()) << n;
        EXPECT_EQ(rd.last_error()->code(), compress::errc::unexpected_end) << n;
    }
}

TEST(Snappy_Tests, TheLimits) {
    const std::string plain = prose(300000, 3);
    auto b = snappy::compress_block(bytes(plain));
    EXPECT_TRUE(snappy::decompress_block(b, compress::limits{.max_size = plain.size()}));
    EXPECT_EQ(snappy::decompress_block(b, compress::limits{.max_size = plain.size() - 1}).error().code(), compress::errc::too_large);
    // a head that says 4 GB is refused before anything is made
    EXPECT_EQ(snappy::decompress_block(bytes(std::string("\xff\xff\xff\xff\x0f", 5))).error().code(), compress::errc::too_large);
    auto f = snappy::compress(bytes(plain));
    EXPECT_TRUE(snappy::decompress(f, compress::limits{.max_size = plain.size()}));
    EXPECT_EQ(snappy::decompress(f, compress::limits{.max_size = plain.size() - 1}).error().code(), compress::errc::too_large);
    EXPECT_TRUE(snappy::decompress(snappy::compress(""), compress::limits{.max_size = 0}));
    EXPECT_EQ(snappy::decompress(snappy::compress("x"), compress::limits{.max_size = 0}).error().code(), compress::errc::too_large);
    EXPECT_EQ(snappy::decompressed_size(bytes(std::string())).error().code(), compress::errc::unexpected_end);
}

TEST(Snappy_Tests, StreamsInPiecesAndFlushes) {
    const std::string plain = prose(400000, 4);
    io::buffer sink;
    snappy::writer w(sink);
    std::mt19937 rng(4);
    size_t at = 0;
    while (at < plain.size()) {
        size_t k = std::min<size_t>(rng() % 90000, plain.size() - at);
        ASSERT_TRUE(w.write(bytes(plain.substr(at, k))));
        at += k;
        if (rng() % 3 == 0) {
            ASSERT_TRUE(w.flush());
            auto now = snappy::decompress(bytes(contents(sink)));
            ASSERT_TRUE(now) << now.error().message();
            EXPECT_EQ(text(*now), plain.substr(0, at));
        }
    }
    ASSERT_TRUE(w.close());
    auto all = snappy::decompress(bytes(contents(sink)));
    ASSERT_TRUE(all);
    EXPECT_EQ(text(*all), plain);
}

TEST(Snappy_Tests, TheAsyncForms) {
    const std::string plain = prose(300000, 5);
    auto task = sgcl::async::spawn([](std::string t) -> sgcl::async::task<std::string> {
        io::buffer sink;
        snappy::writer w(sink);
        for (size_t i = 0; i < t.size(); i += 70000) {
            if (!co_await w.async_write(bytes(t.substr(i, 70000)))) {
                co_return "write failed";
            }
        }
        if (!co_await w.async_flush() || !co_await w.async_close()) {
            co_return "close failed";
        }
        snappy::reader r(dribble{contents(sink), 1000});
        auto all = co_await r.async_read_all();
        co_return all ? text(*all) : std::string("read failed");
    }(plain));
    EXPECT_EQ(task.wait(), plain);
    sgcl::async::scheduler::stop();
}
