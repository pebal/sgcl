//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// bzip2: Go's test vectors and test data, and the system's libbz2 as the
// oracle (it compresses what we decompress, at every block size).
#include "common.h"

#include <bzlib.h>

using namespace compress_test;
using compress::bzip2;

namespace {
    std::string from_hex(const std::string& h) {
        std::string s;
        for (size_t i = 0; i + 1 < h.size(); i += 2) {
            s += char(std::stoi(h.substr(i, 2), nullptr, 16));
        }
        return s;
    }

    std::string from_base64(const std::string& t) {
        static const std::string digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string s;
        uint32_t acc = 0;
        int n = 0;
        for (char c : t) {
            auto k = digits.find(c);
            if (k == std::string::npos) {
                continue;
            }
            acc = acc << 6 | uint32_t(k);
            n += 6;
            if (n >= 8) {
                n -= 8;
                s += char(acc >> n);
            }
        }
        return s;
    }

    std::string bz_compress(const std::string& in, int block = 9, int work = 30) {
        unsigned size = unsigned(in.size() + in.size() / 100 + 600);
        std::string out(size, 0);
        int r = BZ2_bzBuffToBuffCompress(out.data(), &size, const_cast<char*>(in.data()), unsigned(in.size()), block, 0, work);
        EXPECT_EQ(r, BZ_OK);
        out.resize(size);
        return out;
    }

    // libbz2's decompression of one stream
    std::string bz_decompress(const std::string& in) {
        bz_stream s{};
        BZ2_bzDecompressInit(&s, 0, 0);
        std::string out, buf(1 << 16, 0);
        s.next_in = const_cast<char*>(in.data());
        s.avail_in = unsigned(in.size());
        int r;
        do {
            s.next_out = buf.data();
            s.avail_out = unsigned(buf.size());
            r = BZ2_bzDecompress(&s);
            out.append(buf.data(), buf.size() - s.avail_out);
        } while (r == BZ_OK);
        EXPECT_EQ(r, BZ_STREAM_END);
        BZ2_bzDecompressEnd(&s);
        return out;
    }

    // The runs RLE1 shortens, of every length around its four and its 255
    std::string runs() {
        std::string s;
        for (int n = 1; n < 300; ++n) {
            s += std::string(size_t(n), char('a' + n % 7));
            s += char(n);
        }
        for (int n : {3, 4, 5, 258, 259, 260, 1000}) {
            s += std::string(size_t(n), 'z');
            s += std::string(size_t(n), 'z' - 1);
        }
        return s;
    }

    std::string all_bytes() {
        std::string s;
        for (int k = 0; k < 40; ++k) {
            for (int b = 0; b < 256; ++b) {
                s += char((b * 31 + k) & 255);
            }
        }
        return s;
    }

    std::string read_all(bzip2::reader& r) {
        std::string got;
        std::byte buf[5000];
        for (;;) {
            auto n = r.read(buf);
            if (!n || *n == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf), *n);
        }
        return got;
    }
}

// Go's own vectors (compress/bzip2/bzip2_test.go): what they decode to,
// and the ones that must fail
TEST(Bzip2_Tests, GoVectors) {
    std::string hello = from_hex("425a68393141592653594eece83600000251800010400006449080200031064c4101a7a9a580bb9431f8bb9229c28482776741b0");
    struct Case {
        std::string name, input, output;
        bool fail;
    };
    std::vector<Case> cases = {
        {"hello world", hello, "hello world\n", false},
        {"concatenated", hello + hello, "hello world\nhello world\n", false},
        {"32B zeros", from_hex("425a6839314159265359b5aa5098000000600040000004200021008283177245385090b5aa5098"), std::string(32, '\0'), false},
        {"1MiB zeros", from_hex("425a683931415926535938571ce50008084000c0040008200030cc0529a60806c4201e2ee48a70a12070ae39ca"), std::string(1 << 20, '\0'), false},
        {"RLE1 stage", from_hex("425a6839314159265359d992d0f60000137dfe84020310091c1e280e100e042801099210094806c0110002e70806402000546034000034000000f2830000032000d3403264049270eb7a9280d308ca06ad28f6981bee1bf8160727c7364510d73a1e123083421b63f031f63993a0f40051fbf177245385090d992d0f60"),
            from_hex("92d5652616ac444a4a04af1a8a3964aca0450d43d6cf233bd03233f4ba92f8719e6c2a2bd4f5f88db07ecd0da3a33b263483db9b2c158786ad6363be35d17335ba"), false},
        {"out-of-range selector (issue 8363)", from_hex("425a68393141592653594eece83600000251800010400006449080200031064c4101a7a9a580bb943117724538509000000000"), "", true},
        {"bad block size (issue 13941)", from_hex("425a683131415926535936dc55330063ffc0006000200020a40830008b0008b8bb9229c28481b6e2a998"), "", true},
        {"bad huffman delta", from_hex("425a6836314159265359b1f7404b000000400040002000217d184682ee48a70a12163ee80960"), "", true},
    };
    for (auto& c : cases) {
        auto got = bzip2::decompress(bytes(c.input));
        EXPECT_EQ(!got, c.fail) << c.name << (got ? "" : ": " + std::string(got.error().message().view()));
        if (got && !c.fail) {
            EXPECT_EQ(text(*got), c.output) << c.name;
        }
        bzip2::reader r(dribble{c.input, 3});
        auto all = r.read_all();
        EXPECT_EQ(!all, c.fail) << c.name;
        if (all && !c.fail) {
            EXPECT_EQ(text(*all), c.output) << c.name;
        }
    }
    // Go's TestZeroRead: an empty read is 0 and no error
    bzip2::reader r(dribble{cases[2].input, 100});
    std::byte none[1];
    auto zero = r.read(sgcl::slice<std::byte>(none, size_t(0)));
    ASSERT_TRUE(zero);
    EXPECT_EQ(*zero, 0u);
}

TEST(Bzip2_Tests, GoTestData) {
    auto check = [](const std::string& file, const std::string& want) {
        auto c = read_oracle("bzip2/" + file);
        ASSERT_FALSE(c.empty()) << file;
        auto got = bzip2::decompress(bytes(c));
        ASSERT_TRUE(got) << file << ": " << got.error().message().view();
        EXPECT_EQ(text(*got), want) << file;
    };
    check("pass-random1.bz2", read_oracle("bzip2/pass-random1.bin"));
    check("pass-random2.bz2", read_oracle("bzip2/pass-random2.bin"));
    std::string saw(1 << 20, 0);
    for (size_t i = 0; i < saw.size(); ++i) {
        saw[i] = char(i);
    }
    check("pass-sawtooth.bz2", saw);
    check("e.txt.bz2", read_oracle("compress/e.txt"));
    // no plain text of these two: libbz2 says what they hold
    for (auto f : {"Isaac.Newton-Opticks.txt.bz2", "random.data.bz2"}) {
        auto c = read_oracle(std::string("bzip2/") + f);
        check(f, bz_decompress(c));
    }
    // Go's issue 5747: a run of RLE2 past the block
    auto bad = from_base64(read_oracle("bzip2/fail-issue5747.bz2.base64"));
    ASSERT_GT(bad.size(), 1000u);
    auto got = bzip2::decompress(bytes(bad));
    ASSERT_FALSE(got);
    EXPECT_EQ(got.error().code(), compress::errc::corrupt) << got.error().message().view();
}

// libbz2 compresses at every block size (and its fallback sort, which
// makes other blocks), we decompress
TEST(Bzip2_Tests, WhatLibbz2MakesAtEveryBlockSize) {
    auto c = corpus();
    c.push_back({"runs", runs()});
    c.push_back({"all bytes", all_bytes()});
    std::string big;
    for (int i = 0; i < 12; ++i) {
        big += read_oracle("compress/e.txt");
    }
    c.push_back({"big", big});   // 1.2 MB: several blocks at every size
    for (auto& [name, t] : c) {
        for (int block = 1; block <= 9; ++block) {
            if (t.size() < 200000 && block != 1 && block != 9) {
                continue;
            }
            for (int work : {30, 1}) {
                auto z = bz_compress(t, block, work);
                auto got = bzip2::decompress(bytes(z));
                ASSERT_TRUE(got) << name << " block " << block << ": " << got.error().message().view();
                ASSERT_EQ(text(*got), t) << name << " block " << block;
            }
        }
    }
}

// Huffman code lengths no prefix code has, taken as libbz2 takes them: a
// stream the fuzzer made whose tables oversubscribe the code, and whose
// data decodes with the CRCs right (1024 bytes, 0..255 four times, as
// libbz2 says)
TEST(Bzip2_Tests, AnOversubscribedCodeAsLibbz2ReadsIt) {
    std::string z = from_hex(
        "425a6831314159265359e5a3c1ec000001ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc0021c000126000980"
        "00980000000000000000008a00000000000000000000000000000000000000930004c0004c0000000000000000000000000000000000000000000000"
        "00000000004980026000260000000000000000000000000000000000000000000000000000000005555404c04c000000000000000000000000000000"
        "000000000000000000000000000000fe90120440c821053fe20c41c8410921442c861ff90d21c43c8811122444c8a11522c45c8c11923446c8e11d23"
        "c47c9012124448c9212524c49c941292544ac9612d25c4bc981312644cc9a13526c4dc9c1392744ec9e13d27c4fca014128450ca214528c51ca41492"
        "9452ca614d29c53ca81512a454caa1552ac55cac1592b456cae15d2bc57cb01612c458cb21652cc59cb41692d45acb616d2dc5bcb81712e45ccba175"
        "2ec5dcbc1792f45ecbe17d2fc5fcc018130460cc218530c61cc418931462cc618d31c63cc819132464cca19532c65ccc19933466cce19d33c67cd01a"
        "134468cd21a534c69cd41a93546acd61ad35c6bcd81b13646ccda1b536c6dcdc1b93746ecde1bd37c6fce01c138470ce21c538c71ce41c939472ce61"
        "cd39c73ce81d13a474cea1d53ac75cec1d93b476cee1dd3bc77cf01e13c478cf21e53cc79cf41e93d47acf61ed3dc7bcf81f13e47ccfa1f53ec7dcfc"
        "1f93f47ecfe1fd3fc2ee48a70a121cb4783d80");
    ASSERT_EQ(z.size(), 559u);
    auto got = bzip2::decompress(bytes(z));
    ASSERT_TRUE(got) << got.error().message().view();
    EXPECT_EQ(text(*got), bz_decompress(z));
    EXPECT_EQ(got->size(), 1024u);
}

// The reader fed a byte, two, three and seven at a time, several blocks
// and several streams, read into buffers of any size
TEST(Bzip2_Tests, TheReaderInPiecesOfAnySize) {
    std::string t = read_oracle("compress/e.txt") + runs() + all_bytes() + read_oracle("compress/gettysburg.txt");
    std::string z = bz_compress(t, 1) + bz_compress("", 3) + bz_compress("second stream", 9);
    std::string want = t + "second stream";
    for (size_t feed : {size_t(1), size_t(2), size_t(3), size_t(7), size_t(1) << 20}) {
        bzip2::reader r(dribble{z, feed});
        auto all = r.read_all();
        ASSERT_TRUE(all) << "feed " << feed;
        ASSERT_EQ(text(*all), want) << "feed " << feed;
    }
    for (size_t out : {size_t(1), size_t(7), size_t(4096)}) {
        bzip2::reader r(dribble{z, 1000});
        std::string got;
        std::vector<std::byte> buf(out);
        for (;;) {
            auto n = r.read(sgcl::slice<std::byte>(buf.data(), buf.size()));
            ASSERT_TRUE(n);
            if (*n == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf.data()), *n);
        }
        ASSERT_EQ(got, want) << "out " << out;
    }
}

TEST(Bzip2_Tests, StreamsOneAfterAnotherAndWhatFollowsThem) {
    std::string a = bz_compress("first ", 1), b = bz_compress("second", 9), empty = bz_compress("", 5);
    EXPECT_EQ(text(value_of(bzip2::decompress(bytes(a + empty + b + empty)))), "first second");
    EXPECT_EQ(text(value_of(bzip2::decompress(bytes(empty)))), "");
    // data after the end that is not a stream, and a stream cut short in its magic
    auto garbage = bzip2::decompress(bytes(a + "PK\x03\x04"));
    ASSERT_FALSE(garbage);
    EXPECT_EQ(garbage.error().code(), compress::errc::invalid_header);
    auto zeros = bzip2::decompress(bytes(a + std::string(8, '\0')));
    ASSERT_FALSE(zeros);
    EXPECT_EQ(zeros.error().code(), compress::errc::invalid_header);
    auto cut = bzip2::decompress(bytes(a + "B"));
    ASSERT_FALSE(cut);
    EXPECT_EQ(cut.error().code(), compress::errc::unexpected_end);
    bzip2::reader r(dribble{a + "PK\x03\x04", 2});
    EXPECT_EQ(read_all(r), "first ");
    ASSERT_TRUE(r.last_error());
    EXPECT_EQ(r.last_error()->code(), compress::errc::invalid_header);
    // the error again, as an io::error of the compress category
    std::byte buf[16];
    auto again = r.read(buf);
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().code(), compress::make_error_code(compress::errc::invalid_header));
    EXPECT_FALSE(bzip2::decompress(bytes(std::string())));
    auto header = bzip2::decompress(bytes(std::string("BZh0") + a.substr(4)));
    ASSERT_FALSE(header);
    EXPECT_EQ(header.error().code(), compress::errc::invalid_header);
    auto gz = bzip2::decompress(bytes(std::string("\x1F\x8B\x08\x00", 4)));
    ASSERT_FALSE(gz);
    EXPECT_EQ(gz.error().code(), compress::errc::invalid_header);
}

TEST(Bzip2_Tests, CorruptAndTruncatedDataFail) {
    std::string t = "hello, bzip2 " + runs().substr(0, 3000);
    std::string z = bz_compress(t, 1);
    // every length short of the whole: an error, never bytes it did not check
    for (size_t n = 0; n < z.size(); ++n) {
        auto got = bzip2::decompress(bytes(z.substr(0, n)));
        ASSERT_FALSE(got) << "cut at " << n;
        EXPECT_EQ(got.error().code(), compress::errc::unexpected_end) << "cut at " << n << ": " << got.error().message().view();
    }
    // every bit flipped: an error, or (in the padding of the last byte) the same data
    size_t failed = 0;
    for (size_t bit = 0; bit < z.size() * 8; ++bit) {
        auto flipped = z;
        flipped[bit / 8] = char(flipped[bit / 8] ^ (1 << (bit % 8)));
        auto got = bzip2::decompress(bytes(flipped));
        if (!got) {
            ++failed;
        } else {
            EXPECT_EQ(text(*got), t) << "bit " << bit;
        }
        bzip2::reader r(dribble{flipped, 7});
        (void)r.read_all();
    }
    EXPECT_GE(failed + 8, z.size() * 8);
    // the block's CRC (bytes 10..13) and the stream's (the last 32 bits before the padding)
    auto block = z;
    block[12] = char(block[12] ^ 0x10);
    auto b = bzip2::decompress(bytes(block));
    ASSERT_FALSE(b);
    EXPECT_EQ(b.error().code(), compress::errc::checksum);
    auto stream = z;
    stream[stream.size() - 2] = char(stream[stream.size() - 2] ^ 0x01);
    auto s = bzip2::decompress(bytes(stream));
    ASSERT_FALSE(s);
    EXPECT_EQ(s.error().code(), compress::errc::checksum);
    EXPECT_NE(std::string(s.error().message().view()).find("stream CRC"), std::string::npos);
    // the randomised bit, just after the block's CRC
    auto randomised = z;
    randomised[14] = char(randomised[14] | 0x80);
    auto rnd = bzip2::decompress(bytes(randomised));
    ASSERT_FALSE(rnd);
    EXPECT_EQ(rnd.error().code(), compress::errc::unsupported);
}

namespace {
    // A block written by hand, for what no compressor writes: the bytes in
    // use 'a' (and 'b'), two tables whose codes are all two bits (so a
    // symbol is its own number), every selector the first table, and the
    // symbols given, the last the end of the block. Nothing after it: the
    // decoder must fail before it needs more.
    std::string crafted(int level, bool two_bytes, const std::vector<int>& symbols) {
        std::string out = "BZh" + std::string(1, char('0' + level));
        uint64_t acc = 0;
        int count = 0;
        auto put = [&](uint64_t v, int n) {
            for (int i = n - 1; i >= 0; --i) {
                acc = acc << 1 | ((v >> i) & 1);
                if (++count == 8) {
                    out += char(acc);
                    acc = 0;
                    count = 0;
                }
            }
        };
        put(0x314159265359, 48);
        put(0, 32);   // the block's CRC
        put(0, 1);
        put(0, 24);   // the original pointer
        put(0x8000 >> 6, 16);   // 0x60..0x6F
        put(two_bytes ? (0x8000 >> 1 | 0x8000 >> 2) : 0x8000 >> 1, 16);
        int alphabet = two_bytes ? 4 : 3;
        size_t selectors = (symbols.size() + 49) / 50;
        put(2, 3);
        put(selectors, 15);
        for (size_t i = 0; i < selectors; ++i) {
            put(0, 1);
        }
        for (int t = 0; t < 2; ++t) {
            put(2, 5);
            for (int i = 0; i < alphabet; ++i) {
                put(0, 1);
            }
        }
        for (int sym : symbols) {
            put(uint64_t(sym), 2);
        }
        put(0, 64);
        return out;
    }

    // n in bijective base 2: RUNA (0) is a digit 1, RUNB (1) a digit 2
    std::vector<int> run_of(uint64_t n) {
        std::vector<int> v;
        while (n) {
            if (n & 1) {
                v.push_back(0);
                n = (n - 1) / 2;
            } else {
                v.push_back(1);
                n = (n - 2) / 2;
            }
        }
        return v;
    }
}

// A block of more bytes than its header allows, as literals and as a run;
// a run of 32 digits whose value is 2^32 + 5 (5 in 32 bits) fails as it
// passes the block, not later at the checksum
TEST(Bzip2_Tests, ABlockPastItsSizeFails) {
    std::vector<int> literals(100001, 2);   // 'b', 'a', 'b'...: position 1 each time
    literals.push_back(3);
    auto big = bzip2::decompress(bytes(crafted(1, true, literals)));
    ASSERT_FALSE(big);
    EXPECT_EQ(big.error().code(), compress::errc::corrupt);
    EXPECT_NE(std::string(big.error().message().view()).find("larger than its header"), std::string::npos) << big.error().message().view();
    auto run = run_of(100001);
    run.push_back(2);
    auto long_run = bzip2::decompress(bytes(crafted(1, false, run)));
    ASSERT_FALSE(long_run);
    EXPECT_EQ(long_run.error().code(), compress::errc::corrupt);
    auto wrap = run_of((uint64_t(1) << 32) + 5);
    ASSERT_EQ(wrap.size(), 32u);   // its last digit a 2 of weight 2^31
    wrap.push_back(2);
    auto wrapped = bzip2::decompress(bytes(crafted(9, false, wrap)));
    ASSERT_FALSE(wrapped);
    EXPECT_EQ(wrapped.error().code(), compress::errc::corrupt) << wrapped.error().message().view();
    // the same blocks within their size are read up to the (wrong) checksum
    std::vector<int> fits(1000, 2);
    fits.push_back(3);
    auto ok = bzip2::decompress(bytes(crafted(1, true, fits)));
    ASSERT_FALSE(ok);
    EXPECT_EQ(ok.error().code(), compress::errc::checksum);
}

TEST(Bzip2_Tests, TheLimitStopsABomb) {
    std::string zeros(50 << 20, '\0');
    auto z = bz_compress(zeros, 9);
    EXPECT_LT(z.size(), 1000u);
    auto limited = bzip2::decompress(bytes(z), compress::limits{1 << 20});
    ASSERT_FALSE(limited);
    EXPECT_EQ(limited.error().code(), compress::errc::too_large);
    auto whole = bzip2::decompress(bytes(z), compress::limits{UINT64_MAX});
    ASSERT_TRUE(whole);
    EXPECT_EQ(whole->size(), zeros.size());
    // the limit exactly, and one byte short of it
    std::string t = runs();
    auto zt = bz_compress(t, 1);
    EXPECT_TRUE(bzip2::decompress(bytes(zt), compress::limits{t.size()}));
    auto shy = bzip2::decompress(bytes(zt), compress::limits{t.size() - 1});
    ASSERT_FALSE(shy);
    EXPECT_EQ(shy.error().code(), compress::errc::too_large);
}

TEST(Bzip2_Tests, TheAsyncForm) {
    std::string t = read_oracle("compress/e.txt") + runs();
    std::string z = bz_compress(t, 1) + bz_compress("tail", 2);
    auto task = sgcl::async::spawn([](std::string z) -> sgcl::async::task<std::string> {
        bzip2::reader r(dribble{z, 999});
        auto all = co_await r.async_read_all();
        if (!all) {
            co_return "read failed";
        }
        // a buffer larger than a portion: portions with yields between them
        bzip2::reader big(dribble{z, 1 << 20});
        std::vector<std::byte> buf(300000);
        std::string got;
        for (;;) {
            auto n = co_await big.async_read(sgcl::slice<std::byte>(buf.data(), buf.size()));
            if (!n || *n == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf.data()), *n);
        }
        co_return got == text(*all) ? got : std::string("portions differ");
    }(z));
    EXPECT_EQ(task.wait(), t + "tail");
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

TEST(Bzip2_Tests, CloseClosesTheSource) {
    auto closed = std::make_shared<bool>(false);
    bzip2::reader r(closable{bz_compress("x", 1), closed});
    EXPECT_EQ(read_all(r), "x");
    EXPECT_FALSE(r.last_error());
    ASSERT_TRUE(r.close());
    EXPECT_TRUE(*closed);
}
