//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// LZW: Go's compress/lzw as the oracle. The files in
// ~/Programming/oracles/go-compress/lzw are inputs of each literal width
// and what Go's writer makes of them in both bit orders (made by the
// program in ~/Programming/oracles/go-compress/lzw-oracle): we decompress
// them and make the same bytes, so Go decompresses what we make.
#include "common.h"

#include <stdexcept>

using namespace compress_test;
using compress::lzw;

namespace {
    const char* const Names[] = {"empty", "one", "e", "gettysburg", "random", "run", "sawtooth", "long", "small"};

    std::string order_name(lzw::order o) {
        return o == lzw::order::lsb ? "lsb" : "msb";
    }

    // Codes packed as a stream of the format: the widths as a decoder
    // reads them, for streams no compressor makes
    struct packer {
        lzw::order o;
        int literal_width;
        uint32_t width = 0, next = 0, bits = 0, count = 0;
        bool previous = false;
        std::string out;

        packer(lzw::order o, int lw) : o(o), literal_width(lw) {
            start_over();
        }

        void start_over() {
            width = uint32_t(literal_width) + 1;
            next = (1u << literal_width) + 2;
            previous = false;
        }

        void put(uint32_t code) {
            if (o == lzw::order::msb) {
                bits = bits << width | code;
                count += width;
                while (count >= 8) {
                    count -= 8;
                    out += char(bits >> count);
                }
            } else {
                bits |= code << count;
                count += width;
                while (count >= 8) {
                    out += char(bits);
                    bits >>= 8;
                    count -= 8;
                }
            }
        }

        // a code, and what the decoder does after it
        void code(uint32_t c) {
            put(c);
            if (c == (1u << literal_width)) {
                start_over();
                return;
            }
            if (previous) {
                ++next;
                if (next == (1u << width) && width < 12) {
                    ++width;
                }
            }
            previous = true;
        }

        std::string end() {
            put((1u << literal_width) + 1);
            if (count) {
                out += o == lzw::order::msb ? char(bits << (8 - count)) : char(bits);
            }
            return out;
        }
    };
}

// Go's streams decompress to the input, and ours are Go's, byte for byte
TEST(Lzw_Tests, GoBothWaysAtEveryWidthAndOrder) {
    size_t files = 0;
    for (int w = 2; w <= 8; ++w) {
        for (auto name : Names) {
            std::string base = "lzw/lw" + std::to_string(w) + "-" + name;
            std::string in = read_oracle(base + ".in");
            for (auto o : {lzw::order::lsb, lzw::order::msb}) {
                std::string go = read_oracle(base + "." + order_name(o) + ".lzw");
                ASSERT_FALSE(go.empty()) << base;
                auto got = lzw::decompress(bytes(go), o, w);
                ASSERT_TRUE(got) << base << " " << order_name(o) << ": " << got.error().message().view();
                ASSERT_EQ(text(*got), in) << base << " " << order_name(o);
                auto ours = lzw::compress(bytes(in), o, w);
                ASSERT_EQ(text(ours), go) << base << " " << order_name(o);
                ++files;
            }
        }
    }
    EXPECT_EQ(files, 7u * 9u * 2u);
}

// The writer in pieces of any size, the reader fed a byte, two, three and
// seven at a time
TEST(Lzw_Tests, StreamsInPiecesOfAnySize) {
    for (int w : {2, 5, 8}) {
        for (auto name : {"gettysburg", "small", "long", "empty"}) {
            std::string base = "lzw/lw" + std::to_string(w) + "-" + name;
            std::string in = read_oracle(base + ".in");
            if (in.size() > 100000) {
                in.resize(100000);
            }
            for (auto o : {lzw::order::lsb, lzw::order::msb}) {
                std::string whole = text(lzw::compress(bytes(in), o, w));
                for (size_t step : {size_t(1), size_t(7), size_t(1000), in.size() + 1}) {
                    sgcl::io::buffer sink;
                    lzw::writer wr(sink, o, w);
                    for (size_t i = 0; i < in.size(); i += step) {
                        ASSERT_TRUE(wr.write(bytes(in.substr(i, step))));
                    }
                    ASSERT_TRUE(wr.close());
                    ASSERT_TRUE(wr.close());
                    std::string c(reinterpret_cast<const char*>(sink.data().data()), sink.size());
                    ASSERT_EQ(c, whole) << base << " step " << step;
                }
                for (size_t feed : {size_t(1), size_t(2), size_t(3), size_t(7), size_t(1) << 20}) {
                    lzw::reader r(dribble{whole, feed}, o, w);
                    auto all = r.read_all();
                    ASSERT_TRUE(all) << base << " feed " << feed;
                    ASSERT_EQ(text(*all), in) << base << " feed " << feed;
                }
                // a reader's buffer of one byte, with strings far longer
                lzw::reader r(dribble{whole, 5}, o, w);
                std::string got;
                std::byte one[1];
                for (;;) {
                    auto n = r.read(one);
                    ASSERT_TRUE(n);
                    if (*n == 0) {
                        break;
                    }
                    got += char(one[0]);
                }
                ASSERT_EQ(got, in) << base;
            }
        }
    }
}

// What follows the end code is not read; a stream with no end code, cut
// short, or with a code past the table fails
TEST(Lzw_Tests, TheEndAndCorruptData) {
    std::string in = read_oracle("lzw/lw8-gettysburg.in");
    std::string c = text(lzw::compress(bytes(in), lzw::order::lsb, 8));
    auto after = lzw::decompress(bytes(c + "garbage after the end"), lzw::order::lsb, 8);
    ASSERT_TRUE(after);
    EXPECT_EQ(text(*after), in);
    for (size_t n = 0; n + 1 < c.size(); ++n) {
        auto cut = lzw::decompress(bytes(c.substr(0, n)), lzw::order::lsb, 8);
        ASSERT_FALSE(cut) << n;
        EXPECT_EQ(cut.error().code(), compress::errc::unexpected_end) << n;
    }
    // a code past the table: after a clear, the first code must be a literal
    packer p(lzw::order::lsb, 8);
    p.code(256);
    p.code(258);
    auto past = lzw::decompress(bytes(p.end()), lzw::order::lsb, 8);
    ASSERT_FALSE(past);
    EXPECT_EQ(past.error().code(), compress::errc::corrupt);
    // KwKwK: the code being made, used at once, is the one code past the table allowed
    packer k(lzw::order::msb, 8);
    k.code(256);
    k.code('a');
    k.code(258);   // "aa"
    k.code(260);   // past the table: 259 is next
    auto kwk = lzw::decompress(bytes(k.end()), lzw::order::msb, 8);
    ASSERT_FALSE(kwk);
    packer ok(lzw::order::msb, 8);
    ok.code(256);
    ok.code('a');
    ok.code(258);
    auto good = lzw::decompress(bytes(ok.end()), lzw::order::msb, 8);
    ASSERT_TRUE(good) << good.error().message().view();
    EXPECT_EQ(text(*good), "aaa");
    // every bit flipped: an error or other bytes, never a crash (ASan)
    std::string small = text(lzw::compress(bytes(std::string("abracadabra abracadabra abracadabra")), lzw::order::lsb, 8));
    for (size_t bit = 0; bit < small.size() * 8; ++bit) {
        auto flipped = small;
        flipped[bit / 8] = char(flipped[bit / 8] ^ (1 << (bit % 8)));
        (void)lzw::decompress(bytes(flipped), lzw::order::lsb, 8);
        lzw::reader r(dribble{flipped, 3}, lzw::order::lsb, 8);
        (void)r.read_all();
    }
    lzw::reader r(dribble{c.substr(0, c.size() / 2), 3}, lzw::order::lsb, 8);
    ASSERT_FALSE(r.read_all());
    ASSERT_TRUE(r.last_error());
    EXPECT_EQ(r.last_error()->code(), compress::errc::unexpected_end);
}

// A table filled to its 4096 codes is read, and codes after it with no
// clear are read against the full table, which gains nothing (the
// deferred clear of GIF encoders, as Go reads it); a clear starts over
TEST(Lzw_Tests, AFullTableStaysFullUntilAClear) {
    for (auto o : {lzw::order::lsb, lzw::order::msb}) {
        for (int w : {2, 8}) {
            uint32_t clear = 1u << w;
            packer full(o, w);
            full.code(clear);
            std::string want;
            // literals only: each code after the first makes one, up to 4095
            uint32_t codes = 4096 - (clear + 2) + 1;
            for (uint32_t i = 0; i < codes; ++i) {
                full.code(i % clear);
                want += char(i % clear);
            }
            ASSERT_EQ(full.next, 4096u);
            packer more = full;
            packer cleared = full;
            auto filled = lzw::decompress(bytes(full.end()), o, w);
            ASSERT_TRUE(filled) << filled.error().message().view();
            EXPECT_EQ(text(*filled), want);
            more.code(1);
            more.code(clear + 2);   // an entry of the full table: the first string made after the clear
            auto over = lzw::decompress(bytes(more.end()), o, w);
            ASSERT_TRUE(over) << over.error().message().view();
            EXPECT_EQ(text(*over), want + char(1) + want.substr(0, 2));
            // with a clear between, it goes on
            cleared.code(clear);
            cleared.code(1);
            auto again = lzw::decompress(bytes(cleared.end()), o, w);
            ASSERT_TRUE(again) << again.error().message().view();
            EXPECT_EQ(text(*again), want + char(1));
        }
    }
}

// A literal width outside 2..8 is the program's mistake; a byte the width
// cannot hold is thrown by compress and the writer's error
TEST(Lzw_Tests, WidthsAndBytesPastThem) {
    for (int w : {-1, 0, 1, 9, 12}) {
        EXPECT_THROW(lzw::compress(bytes(std::string("a")), lzw::order::lsb, w), std::invalid_argument);
        EXPECT_THROW((void)lzw::decompress(bytes(std::string("a")), lzw::order::lsb, w), std::invalid_argument);
        EXPECT_THROW(lzw::reader(dribble{"", 1}, lzw::order::msb, w), std::invalid_argument);
        sgcl::io::buffer sink;
        EXPECT_THROW(lzw::writer(sink, lzw::order::msb, w), std::invalid_argument);
    }
    EXPECT_THROW(lzw::compress(bytes(std::string("\x04", 1)), lzw::order::lsb, 2), std::invalid_argument);
    EXPECT_NO_THROW(lzw::compress(bytes(std::string("\x03", 1)), lzw::order::lsb, 2));
    sgcl::io::buffer sink;
    lzw::writer wr(sink, lzw::order::lsb, 7);
    ASSERT_TRUE(wr.write(bytes(std::string("ascii"))));
    auto bad = wr.write(bytes(std::string("\xC3\xA9")));
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error().code(), compress::make_error_code(compress::errc::invalid_argument));
    EXPECT_FALSE(wr.write(bytes(std::string("more"))));
    EXPECT_FALSE(wr.close());
}

TEST(Lzw_Tests, TheLimitStopsABomb) {
    std::string zeros(10 << 20, '\0');
    auto c = lzw::compress(bytes(zeros), lzw::order::lsb, 8);
    EXPECT_LT(c.size(), 400000u);
    auto limited = lzw::decompress(bytes(text(c)), lzw::order::lsb, 8, compress::limits{1 << 20});
    ASSERT_FALSE(limited);
    EXPECT_EQ(limited.error().code(), compress::errc::too_large);
    auto whole = lzw::decompress(bytes(text(c)), lzw::order::lsb, 8, compress::limits{UINT64_MAX});
    ASSERT_TRUE(whole);
    EXPECT_EQ(whole->size(), zeros.size());
    std::string in = read_oracle("lzw/lw8-gettysburg.in");
    auto g = text(lzw::compress(bytes(in), lzw::order::msb, 8));
    EXPECT_TRUE(lzw::decompress(bytes(g), lzw::order::msb, 8, compress::limits{in.size()}));
    auto shy = lzw::decompress(bytes(g), lzw::order::msb, 8, compress::limits{in.size() - 1});
    ASSERT_FALSE(shy);
    EXPECT_EQ(shy.error().code(), compress::errc::too_large);
}

TEST(Lzw_Tests, TheAsyncForms) {
    std::string in = read_oracle("lzw/lw8-long.in");
    auto task = sgcl::async::spawn([](std::string in) -> sgcl::async::task<std::string> {
        sgcl::io::buffer sink;
        lzw::writer w(sink, lzw::order::msb, 8);
        (void)co_await w.async_write(bytes(in));
        (void)co_await w.async_close();
        std::string c(reinterpret_cast<const char*>(sink.data().data()), sink.size());
        if (c != text(lzw::compress(bytes(in), lzw::order::msb, 8))) {
            co_return "the writer's bytes differ";
        }
        lzw::reader r(dribble{c, 1000}, lzw::order::msb, 8);
        auto all = co_await r.async_read_all();
        co_return all ? text(*all) : std::string("read failed");
    }(in));
    EXPECT_EQ(task.wait(), in);
    sgcl::async::scheduler::stop();
}

namespace {
    struct closable {
        std::string data;
        std::shared_ptr<bool> closed;
        size_t at = 0;

        sgcl::expected<size_t, sgcl::io::error> read(sgcl::slice<std::byte> b) {
            size_t n = std::min(b.size(), data.size() - at);
            std::memcpy(b.data(), data.data() + at, n);
            at += n;
            return n;
        }

        sgcl::expected<void, sgcl::io::error> close() {
            *closed = true;
            return {};
        }
    };
}

TEST(Lzw_Tests, CloseClosesTheSource) {
    auto closed = std::make_shared<bool>(false);
    lzw::reader r(closable{text(lzw::compress(bytes(std::string("xyz")), lzw::order::lsb, 8)), closed}, lzw::order::lsb, 8);
    EXPECT_EQ(text(value_of(r.read_all())), "xyz");
    ASSERT_TRUE(r.close());
    EXPECT_TRUE(*closed);
}
