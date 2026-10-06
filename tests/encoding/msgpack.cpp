//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// msgpack: every format of msgpack/spec.md written at its edges (the
// shortest format for each length and value) and read back, every format
// read where it is not the shortest, the timestamp extension in its three
// forms, the errors, the limits, and what MessagePack has no format for.
#include "common.h"

#include <cmath>
#include <string>

using namespace sgcl::encoding;
using namespace enc_test;

namespace {
    std::string hexs(const sgcl::vector<byte>& v) {
        static constexpr char d[] = "0123456789abcdef";
        std::string s;
        for (auto b : v) {
            s += d[uint8_t(b) >> 4];
            s += d[uint8_t(b) & 15];
        }
        return s;
    }

    sgcl::vector<byte> bytes(std::string_view h) {
        auto v = from_hex(h);
        return sgcl::vector<byte>(v.begin(), v.end());
    }

    std::string diag(const cbor& c) {
        return std::string(c.to_string().view());
    }
}

TEST(Msgpack_Tests, TheFormatsAtTheirEdges) {
    struct {
        cbor value;
        const char* hex;
    } cases[] = {
        {cbor(), "c0"}, {cbor(false), "c2"}, {cbor(true), "c3"},
        {0, "00"}, {127, "7f"}, {128, "cc80"}, {255, "ccff"}, {256, "cd0100"}, {65535, "cdffff"}, {65536, "ce00010000"},
        {4294967295u, "ceffffffff"}, {4294967296ll, "cf0000000100000000"}, {UINT64_MAX, "cfffffffffffffffff"},
        {-1, "ff"}, {-32, "e0"}, {-33, "d0df"}, {-128, "d080"}, {-129, "d1ff7f"}, {-32768, "d18000"}, {-32769, "d2ffff7fff"},
        {INT32_MIN, "d280000000"}, {int64_t(INT32_MIN) - 1, "d3ffffffff7fffffff"}, {INT64_MIN, "d38000000000000000"},
        {1.5, "ca3fc00000"}, {0.1, "cb3fb999999999999a"}, {-0.0, "ca80000000"},
        {cbor(""), "a0"}, {cbor("a"), "a161"},
    };
    for (auto& c : cases) {
        auto out = msgpack::encode(c.value);
        EXPECT_EQ(hexs(out), c.hex) << diag(c.value);
        auto back = msgpack::parse(out);
        ASSERT_TRUE(back) << c.hex;
        EXPECT_EQ(*back, c.value) << c.hex;
    }
    // the lengths of str, bin, array, map and ext at each format's edge
    for (size_t n : {0, 1, 15, 16, 31, 32, 255, 256, 65535, 65536}) {
        std::string s(n, 'x');
        auto t = msgpack::encode(cbor(sgcl::string(s)));
        std::string head = hexs(t).substr(0, n <= 31 ? 2 : n <= 255 ? 4 : n <= 65535 ? 6 : 10);
        std::string want = n <= 31 ? std::string(1, "0123456789abcdef"[(0xA0 | n) >> 4]) + "0123456789abcdef"[(0xA0 | n) & 15]
                         : n <= 255 ? "d9" : n <= 65535 ? "da" : "db";
        EXPECT_EQ(head.substr(0, 2), want.substr(0, 2)) << n;
        EXPECT_EQ(*msgpack::parse(t)->as_string(), sgcl::string(s));
        auto b = msgpack::encode(cbor::bytes(sgcl::vector<byte>(n)));
        EXPECT_EQ(hexs(b).substr(0, 2), n <= 255 ? "c4" : n <= 65535 ? "c5" : "c6") << n;
        EXPECT_EQ(msgpack::parse(b)->as_bytes()->size(), n);
        sgcl::vector<cbor> items(n, cbor(1));
        auto a = msgpack::encode(cbor::array(items));
        EXPECT_EQ(hexs(a).substr(0, 2), n <= 15 ? hexs(sgcl::vector<byte>{byte(0x90 | n)}) : n <= 65535 ? "dc" : "dd") << n;
        EXPECT_EQ(msgpack::parse(a)->size(), n);
        sgcl::vector<cbor::member> ms;
        for (size_t i = 0; i < n; ++i) {
            ms.push_back(cbor::member{cbor(uint64_t(i)), cbor()});
        }
        auto m = msgpack::encode(cbor::map(ms));
        EXPECT_EQ(hexs(m).substr(0, 2), n <= 15 ? hexs(sgcl::vector<byte>{byte(0x80 | n)}) : n <= 65535 ? "de" : "df") << n;
        EXPECT_EQ(msgpack::parse(m)->size(), n);
        auto e = msgpack::encode(cbor::extension(9, sgcl::vector<byte>(n)));
        std::string ehead = n == 1 ? "d4" : n == 2 ? "d5" : n == 4 ? "d6" : n == 8 ? "d7" : n == 16 ? "d8" : n <= 255 ? "c7" : n <= 65535 ? "c8" : "c9";
        EXPECT_EQ(hexs(e).substr(0, 2), ehead) << n;
        auto eb = msgpack::parse(e);
        ASSERT_TRUE(eb);
        EXPECT_EQ(eb->tag(), 9u);
        EXPECT_EQ(eb->as_bytes()->size(), n);
    }
    for (size_t n : {2, 4, 8, 16}) {
        auto e = msgpack::encode(cbor::extension(-2, sgcl::vector<byte>(n)));
        EXPECT_EQ(hexs(e).substr(2, 2), "fe") << n;
    }
}

TEST(Msgpack_Tests, LongerFormatsRead) {
    struct {
        const char* hex;
        const char* diag;
    } cases[] = {
        {"cc05", "5"}, {"cd0005", "5"}, {"ce00000005", "5"}, {"cf0000000000000005", "5"},
        {"d0ff", "-1"}, {"d1ffff", "-1"}, {"d2ffffffff", "-1"}, {"d3ffffffffffffffff", "-1"}, {"d005", "5"},
        {"d90161", "\"a\""}, {"da000161", "\"a\""}, {"db0000000161", "\"a\""},
        {"c40101", "h'01'"}, {"c5000101", "h'01'"}, {"c60000000101", "h'01'"},
        {"dc000101", "[1]"}, {"dd0000000101", "[1]"}, {"de00010102", "{1: 2}"}, {"df000000010102", "{1: 2}"},
        {"ca3fc00000", "1.5"}, {"cb3ff8000000000000", "1.5"},
        {"c7010501", "extension(5, h'01')"}, {"c800010501", "extension(5, h'01')"}, {"c90000000105 01", "extension(5, h'01')"},
    };
    for (auto& c : cases) {
        std::string h;
        for (const char* p = c.hex; *p; ++p) {
            if (*p != ' ') {
                h += *p;
            }
        }
        auto v = msgpack::parse(bytes(h));
        ASSERT_TRUE(v) << c.hex << ": " << v.error().message();
        EXPECT_EQ(diag(*v), c.diag) << c.hex;
    }
}

TEST(Msgpack_Tests, Timestamps) {
    using sgcl::time::datetime;
    using sgcl::time::zone;
    struct {
        int64_t seconds;
        uint32_t ns;
        const char* hex;
    } cases[] = {
        {0, 0, "d6ff00000000"},
        {1700000000, 0, "d6ff6553f100"},
        {4294967295, 0, "d6ffffffffff"},
        {4294967296, 0, "d7ff0000000100000000"},
        {1700000000, 500000000, "d7ff773594006553f100"},
        {9000000000, 999999999, "d7ffee6b27fe18711a00"},
        {-1, 0, "c70cff00000000ffffffffffffffff"},
        {-1, 999999999, "c70cff3b9ac9ffffffffffffffffff"},
    };
    for (auto& c : cases) {
        auto t = datetime::from_unix_nano(c.seconds * 1000000000 + c.ns, zone::utc());
        cbor ts = msgpack::timestamp(t);
        auto out = msgpack::encode(ts);
        EXPECT_EQ(hexs(out), c.hex) << c.seconds;
        auto back = msgpack::parse(out);
        ASSERT_TRUE(back);
        ASSERT_TRUE(back->as_time()) << c.seconds;
        EXPECT_EQ(back->as_time()->unix_nano(), t.unix_nano()) << c.seconds;
        // a CBOR instant (tag 1) is written as the timestamp
        EXPECT_EQ(msgpack::parse(msgpack::encode(cbor::epoch_time(datetime::from_unix(c.seconds, zone::utc()))))->as_time()->unix(), c.seconds);
    }
    EXPECT_EQ(hexs(msgpack::encode(msgpack::timestamp(datetime::from_unix_nano(1700000000500000000, zone::utc())))), "d7ff" "77359400" "6553f100");
    // the instants past a datetime's years: the end of them
    EXPECT_EQ(msgpack::parse(bytes("d7ffee6b27ffffffffff"))->as_time()->unix(), datetime::from_unix(INT64_MAX).unix());
    EXPECT_EQ(msgpack::parse(bytes("c70cff000000000000000400000000"))->as_time()->unix(), datetime::from_unix(INT64_MAX).unix());
    // a timestamp of a wrong length or past 999999999 ns
    EXPECT_EQ(msgpack::parse(bytes("d5ff0000")).error().code(), errc::syntax);
    EXPECT_EQ(msgpack::parse(bytes("d7ffee6b2800ffffffff")).error().code(), errc::out_of_range);
    EXPECT_EQ(msgpack::parse(bytes("c70cff3b9aca00ffffffffffffffff")).error().code(), errc::out_of_range);
}

TEST(Msgpack_Tests, Errors) {
    struct {
        const char* hex;
        errc code;
    } cases[] = {
        {"", errc::unexpected_end}, {"c1", errc::syntax}, {"cc", errc::unexpected_end}, {"cd00", errc::unexpected_end},
        {"cf00000000000000", errc::unexpected_end}, {"d3", errc::unexpected_end}, {"a1", errc::unexpected_end},
        {"d9", errc::unexpected_end}, {"d905", errc::unexpected_end}, {"dbffffffff", errc::unexpected_end},
        {"c6ffffffff00", errc::unexpected_end}, {"91", errc::unexpected_end}, {"81", errc::unexpected_end},
        {"8100", errc::unexpected_end}, {"ddffffffff", errc::unexpected_end}, {"dfffffffff", errc::unexpected_end},
        {"d4", errc::unexpected_end}, {"d405", errc::unexpected_end}, {"c701", errc::unexpected_end}, {"c70105", errc::unexpected_end},
        {"0000", errc::syntax}, {"a2c328", errc::invalid_utf8}, {"920101 0102", errc::syntax},
    };
    for (auto& c : cases) {
        std::string h;
        for (const char* p = c.hex; *p; ++p) {
            if (*p != ' ') {
                h += *p;
            }
        }
        auto in = bytes(h);
        auto v = msgpack::parse(in);
        ASSERT_FALSE(v) << c.hex;
        EXPECT_EQ(v.error().code(), c.code) << c.hex << ": " << v.error().message();
        EXPECT_LE(v.error().offset(), in.size());
    }
    // a key given twice, and allowed
    auto dup = bytes("82a161" "01" "a161" "02");
    EXPECT_EQ(msgpack::parse(dup).error().code(), errc::duplicate_key);
    cbor::options o;
    o.allow_duplicate_keys = true;
    EXPECT_EQ(diag(*msgpack::parse(dup, o)), "{\"a\": 2}");
    o.allow_invalid_utf8 = true;
    EXPECT_TRUE(msgpack::parse(bytes("a2c328"), o));
    // the depth
    o.max_depth = 2;
    EXPECT_TRUE(msgpack::parse(bytes("9191c0"), o));
    EXPECT_EQ(msgpack::parse(bytes("919191c0"), o).error().code(), errc::depth_limit);
    std::string deep(300000, '\x91');
    deep += '\xc0';
    auto d = msgpack::parse(sgcl::slice<const byte>(reinterpret_cast<const byte*>(deep.data()), deep.size()));
    EXPECT_EQ(d.error().code(), errc::depth_limit);
    o.max_depth = 400000;
    auto dd = msgpack::parse(sgcl::slice<const byte>(reinterpret_cast<const byte*>(deep.data()), deep.size()), o);
    ASSERT_TRUE(dd);
    EXPECT_EQ(msgpack::encode(*dd).size(), deep.size());
    // what MessagePack has no format for
    EXPECT_THROW(msgpack::encode(cbor::undefined()), std::invalid_argument);
    EXPECT_THROW(msgpack::encode(cbor::simple(5)), std::invalid_argument);
    EXPECT_THROW(msgpack::encode(cbor::tagged(32, "x")), std::invalid_argument);
    EXPECT_THROW(msgpack::encode(cbor(sgcl::math::big_integer("-9223372036854775809"))), std::invalid_argument);
    EXPECT_THROW(msgpack::encode(cbor(sgcl::math::big_integer("18446744073709551616"))), std::invalid_argument);
    EXPECT_THROW(msgpack::encode(cbor::array({1, cbor::undefined()})), std::invalid_argument);
    EXPECT_NO_THROW(msgpack::encode(cbor(sgcl::math::big_integer("-9223372036854775808"))));
}

TEST(Msgpack_Tests, ValuesRoundTrip) {
    cbor v = cbor::map({{"compact", true}, {"schema", 0}, {1, cbor::array({1.5, -7, "x", cbor::bytes(sgcl::vector<byte>(3))})},
                        {"nested", cbor::map({{"k", cbor()}})}, {"ext", cbor::extension(4, sgcl::vector<byte>(5))}});
    auto out = msgpack::encode(v);
    EXPECT_EQ(hexs(out), "85a7636f6d70616374c3a6736368656d6100" "0194" "ca3fc00000" "f9" "a178" "c403000000" "a66e6573746564" "81a16bc0" "a3657874" "c70504" "0000000000");
    EXPECT_EQ(*msgpack::parse(out), v);
    // from CBOR's bytes and back to them, through MessagePack
    auto c = cbor::parse(bytes("a26161016162820203")).value();
    EXPECT_EQ(hexs(cbor::parse(msgpack::parse(msgpack::encode(c))->to_bytes())->to_bytes()), "a26161016162820203");
    // JSON's corpora: the same value through MessagePack
    for (const char* name : {"twitter.json", "citm_catalog.json"}) {
        auto path = std::string(std::getenv("HOME")) + "/Programming/oracles/nativejson/" + name;
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            continue;
        }
        std::string s;
        char buf[65536];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
            s.append(buf, n);
        }
        std::fclose(f);
        cbor value = cbor::from_json(json::parse(sgcl::string(s)).value());
        auto back = msgpack::parse(msgpack::encode(value));
        ASSERT_TRUE(back);
        EXPECT_EQ(*back, value);
    }
}

TEST(Msgpack_Tests, ValuesOfAStream) {
    std::string seq;
    for (auto h : {"82a161c3a16291c0", "dc0002ccffd0df", "c70cff00000000ffffffffffffffff", "d90161", "c0", "de00010102"}) {
        auto b = from_hex(h);
        seq.append(reinterpret_cast<const char*>(b.data()), b.size());
    }
    for (size_t piece : {1, 2, 5, 4096}) {
        sgcl::io::reader in(make_tracked<dribble>(seq, piece));
        std::vector<std::string> got;
        for (;;) {
            auto v = msgpack::parse(in);
            if (!v) {
                EXPECT_EQ(v.error().code(), errc::unexpected_end);
                EXPECT_EQ(v.error().offset(), 0u);
                break;
            }
            got.push_back(diag(*v));
        }
        ASSERT_EQ(got.size(), 6u) << piece;
        EXPECT_EQ(got[0], "{\"a\": true, \"b\": [null]}");
        EXPECT_EQ(got[1], "[255, -33]");
        EXPECT_EQ(got[5], "{1: 2}");
    }
    cbor::options o;
    o.max_size = 10;
    sgcl::io::reader b(make_tracked<dribble>(std::string("\xc4\x20") + std::string(32, 'x'), 64));
    EXPECT_EQ(msgpack::parse(b, o).error().code(), errc::limit_exceeded);
    sgcl::io::reader c1(make_tracked<dribble>(std::string("\xc1\x00", 2), 1));
    EXPECT_EQ(msgpack::parse(c1).error().code(), errc::syntax);
    sgcl::io::reader c2(make_tracked<dribble>(std::string("\x92\xc1", 2), 1));
    EXPECT_FALSE(msgpack::parse(c2));
    auto t = sgcl::async::spawn([](std::string seq) -> sgcl::async::task<int> {
        sgcl::io::reader in(make_tracked<dribble>(seq, 3));
        auto x = co_await msgpack::async_parse(in);
        auto y = co_await msgpack::async_parse(in, cbor::options());
        co_return x && y && y->size() == 2 ? 1 : -1;
    }(seq));
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}
