//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// cbor: RFC 8949. Every example of its appendix A read, written back in the
// preferred serialization and shown in the diagnostic notation; the
// examples not in preferred form (indefinite lengths, wide floats) read to
// the value the preferred bytes write. The deterministic encoding (§4.2.1),
// the errors of appendix F (not well-formed), the limits, the tags, and the
// boundaries of every member.
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

TEST(Cbor_Tests, AppendixA) {
    // the preferred serialization: read, written back the same, shown
    struct {
        const char* hex;
        const char* diag;
    } cases[] = {
        {"00", "0"}, {"01", "1"}, {"0a", "10"}, {"17", "23"}, {"1818", "24"}, {"1819", "25"}, {"1864", "100"},
        {"1903e8", "1000"}, {"1a000f4240", "1000000"}, {"1b000000e8d4a51000", "1000000000000"},
        {"1bffffffffffffffff", "18446744073709551615"}, {"c249010000000000000000", "2(h'010000000000000000')"},
        {"3bffffffffffffffff", "-18446744073709551616"}, {"c349010000000000000000", "3(h'010000000000000000')"},
        {"20", "-1"}, {"29", "-10"}, {"3863", "-100"}, {"3903e7", "-1000"},
        {"f90000", "0.0"}, {"f98000", "-0.0"}, {"f93c00", "1.0"}, {"fb3ff199999999999a", "1.1"}, {"f93e00", "1.5"},
        {"f97bff", "65504.0"}, {"fa47c35000", "100000.0"}, {"fa7f7fffff", "3.4028234663852886e+38"},
        {"fb7e37e43c8800759c", "1.0e+300"}, {"f90001", "5.960464477539063e-8"}, {"f90400", "0.00006103515625"},
        {"f9c400", "-4.0"}, {"fbc010666666666666", "-4.1"}, {"f97c00", "Infinity"}, {"f97e00", "NaN"},
        {"f9fc00", "-Infinity"}, {"f4", "false"}, {"f5", "true"}, {"f6", "null"}, {"f7", "undefined"},
        {"f0", "simple(16)"}, {"f8ff", "simple(255)"},
        {"c074323031332d30332d32315432303a30343a30305a", "0(\"2013-03-21T20:04:00Z\")"},
        {"c11a514b67b0", "1(1363896240)"}, {"c1fb41d452d9ec200000", "1(1363896240.5)"},
        {"d74401020304", "23(h'01020304')"}, {"d818456449455446", "24(h'6449455446')"},
        {"d82076687474703a2f2f7777772e6578616d706c652e636f6d", "32(\"http://www.example.com\")"},
        {"40", "h''"}, {"4401020304", "h'01020304'"}, {"60", "\"\""}, {"6161", "\"a\""}, {"6449455446", "\"IETF\""},
        {"62225c", "\"\\\"\\\\\""}, {"62c3bc", "\"ü\""}, {"63e6b0b4", "\"水\""}, {"64f0908591", "\"𐅑\""},
        {"80", "[]"}, {"83010203", "[1, 2, 3]"}, {"8301820203820405", "[1, [2, 3], [4, 5]]"},
        {"98190102030405060708090a0b0c0d0e0f101112131415161718181819",
         "[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25]"},
        {"a0", "{}"}, {"a201020304", "{1: 2, 3: 4}"}, {"a26161016162820203", "{\"a\": 1, \"b\": [2, 3]}"},
        {"826161a161626163", "[\"a\", {\"b\": \"c\"}]"},
        {"a56161614161626142616361436164614461656145", "{\"a\": \"A\", \"b\": \"B\", \"c\": \"C\", \"d\": \"D\", \"e\": \"E\"}"},
    };
    for (auto& c : cases) {
        auto in = bytes(c.hex);
        auto v = cbor::parse(in);
        ASSERT_TRUE(v) << c.hex << ": " << v.error().message();
        EXPECT_EQ(hexs(v->to_bytes()), c.hex) << c.diag;
        EXPECT_EQ(diag(*v), c.diag) << c.hex;
    }
    // not preferred: read to the value the preferred bytes write
    struct {
        const char* hex;
        const char* preferred;
    } others[] = {
        {"fa7f800000", "f97c00"}, {"fa7fc00000", "f97e00"}, {"faff800000", "f9fc00"}, {"fb7ff0000000000000", "f97c00"},
        {"fb7ff8000000000000", "f97e00"}, {"fbfff0000000000000", "f9fc00"},
        {"5f42010243030405ff", "450102030405"}, {"7f657374726561646d696e67ff", "6973747265616d696e67"},
        {"9fff", "80"}, {"9f018202039f0405ffff", "8301820203820405"}, {"9f01820203820405ff", "8301820203820405"},
        {"83018202039f0405ff", "8301820203820405"}, {"83019f0203ff820405", "8301820203820405"},
        {"9f0102030405060708090a0b0c0d0e0f101112131415161718181819ff", "98190102030405060708090a0b0c0d0e0f101112131415161718181819"},
        {"bf61610161629f0203ffff", "a26161016162820203"}, {"826161bf61626163ff", "826161a161626163"},
        {"bf6346756ef563416d7421ff", "a26346756ef563416d7421"},
        {"1800", "00"}, {"190000", "00"}, {"1a00000000", "00"}, {"1b0000000000000017", "17"}, {"3800", "20"},
        {"5800", "40"}, {"7800", "60"}, {"9800", "80"}, {"b800", "a0"}, {"d80000", "c000"},
    };
    for (auto& c : others) {
        auto in = bytes(c.hex);
        auto v = cbor::parse(in);
        ASSERT_TRUE(v) << c.hex << ": " << v.error().message();
        EXPECT_EQ(hexs(v->to_bytes()), c.preferred) << c.hex;
    }
}

TEST(Cbor_Tests, NotWellFormed) {
    // RFC 8949 appendix F.1, a few of each kind, and where
    struct {
        const char* hex;
        errc code;
    } cases[] = {
        // the end of the input
        {"18", errc::unexpected_end}, {"19", errc::unexpected_end}, {"1a", errc::unexpected_end}, {"1b", errc::unexpected_end},
        {"1901", errc::unexpected_end}, {"1a0102", errc::unexpected_end}, {"1b01020304050607", errc::unexpected_end},
        {"38", errc::unexpected_end}, {"58", errc::unexpected_end}, {"78", errc::unexpected_end}, {"98", errc::unexpected_end},
        {"9a01ff00", errc::unexpected_end}, {"b8", errc::unexpected_end}, {"d8", errc::unexpected_end}, {"f8", errc::unexpected_end},
        {"f900", errc::unexpected_end}, {"fa0000", errc::unexpected_end}, {"fb000000", errc::unexpected_end},
        {"41", errc::unexpected_end}, {"61", errc::unexpected_end}, {"5affffffff00", errc::unexpected_end},
        {"5bffffffffffffffff010203", errc::unexpected_end}, {"7affffffff00", errc::unexpected_end},
        {"7b7fffffffffffffff010203", errc::unexpected_end}, {"81", errc::unexpected_end}, {"818181818181818181", errc::unexpected_end},
        {"8200", errc::unexpected_end}, {"a1", errc::unexpected_end}, {"a20102", errc::unexpected_end}, {"a100", errc::unexpected_end},
        {"a2000000", errc::unexpected_end}, {"c0", errc::unexpected_end}, {"5f4100", errc::unexpected_end},
        {"5f", errc::unexpected_end}, {"7f", errc::unexpected_end}, {"9f", errc::unexpected_end}, {"9f0102", errc::unexpected_end},
        {"bf", errc::unexpected_end}, {"bf01020102", errc::unexpected_end}, {"819f", errc::unexpected_end},
        {"9f8000", errc::unexpected_end}, {"9f9f9f9f9fffffffff", errc::unexpected_end}, {"9f819f819f9fffffff", errc::unexpected_end},
        // the reserved additional information
        {"1c", errc::syntax}, {"1d", errc::syntax}, {"1e", errc::syntax}, {"3c", errc::syntax}, {"5c", errc::syntax},
        {"7c", errc::syntax}, {"9c", errc::syntax}, {"bc", errc::syntax}, {"dc", errc::syntax}, {"fc", errc::syntax},
        {"fd", errc::syntax}, {"fe", errc::syntax},
        // an indefinite length where there is none
        {"1f", errc::syntax}, {"3f", errc::syntax}, {"df", errc::syntax},
        // a simple value under 32 in two bytes
        {"f800", errc::syntax}, {"f801", errc::syntax}, {"f818", errc::syntax}, {"f81f", errc::syntax},
        // a chunk of another type, of indefinite length, an integer
        {"5f00ff", errc::syntax}, {"5f21ff", errc::syntax}, {"5f6100ff", errc::syntax}, {"5f80ff", errc::syntax},
        {"5fa0ff", errc::syntax}, {"5fc000ff", errc::syntax}, {"5fe0ff", errc::syntax}, {"7f4100ff", errc::syntax},
        {"5f5f4100ffff", errc::syntax}, {"7f7f6100ffff", errc::syntax},
        // a break where nothing of indefinite length ends
        {"ff", errc::syntax}, {"81ff", errc::syntax}, {"8200ff", errc::syntax}, {"a1ff", errc::unexpected_end}, {"a1ff00", errc::syntax},
        {"a100ff", errc::syntax}, {"a20000ff", errc::unexpected_end}, {"9f81ff", errc::syntax}, {"9f829f819f9fffffffff", errc::syntax},
        // a map of indefinite length with a key and no value
        {"bf00ff", errc::syntax}, {"bf000000ff", errc::syntax},
        // bytes after the value
        {"0000", errc::syntax},
    };
    for (auto& c : cases) {
        auto in = bytes(c.hex);
        auto v = cbor::parse(in);
        ASSERT_FALSE(v) << c.hex;
        EXPECT_EQ(v.error().code(), c.code) << c.hex << ": " << v.error().message();
        EXPECT_LE(v.error().offset(), in.size());
    }
}

TEST(Cbor_Tests, Validity) {
    // invalid UTF-8, and allowed
    auto bad = bytes("62c328");
    EXPECT_EQ(cbor::parse(bad).error().code(), errc::invalid_utf8);
    cbor::options o;
    o.allow_invalid_utf8 = true;
    EXPECT_EQ(cbor::parse(bad, o)->as_string()->size(), 2u);
    // a key given twice, and the last one winning in the first's place
    auto dup = bytes("a30161610202016163");   // {1: "a", 2: 2, 1: "c"}
    auto r = cbor::parse(dup);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), errc::duplicate_key);
    EXPECT_EQ(r.error().message(), "offset 0: a key given twice in a map: 1");
    o.allow_duplicate_keys = true;
    auto last = cbor::parse(dup, o);
    ASSERT_TRUE(last);
    EXPECT_EQ(last->to_string(), "{1: \"c\", 2: 2}");
    // keys equal by value whatever their encoding: 1 and 1 in three bytes
    auto dup2 = bytes("a201616119000161" "62");
    EXPECT_EQ(cbor::parse(dup2).error().code(), errc::duplicate_key);
    // a float key and an integer key are different keys
    auto mixed = bytes("a201f5f93c00f4");
    auto m = cbor::parse(mixed);
    ASSERT_TRUE(m);
    EXPECT_EQ(*m->operator[](1).as_bool(), true);
    EXPECT_EQ(*m->operator[](cbor(1.0)).as_bool(), false);
}

TEST(Cbor_Tests, Deterministic) {
    // §4.2.1: keys sorted by their encodings, bytewise
    cbor m = cbor::map({{"b", 1}, {10, 2}, {cbor::array({}), 3}, {-1, 4}, {"a", 5}, {100, 6}, {false, 7}, {"aa", 8}});
    EXPECT_EQ(hexs(m.to_bytes()), "a8616201" "0a02" "8003" "2004" "616105" "186406" "f407" "62616108");
    EXPECT_EQ(hexs(m.to_bytes(cbor::deterministic)), "a8" "0a02" "186406" "2004" "616105" "616201" "62616108" "8003" "f407");
    // nested maps sorted too
    cbor n = cbor::array({cbor::map({{2, 0}, {1, 0}}), cbor::tagged(5, cbor::map({{"z", 0}, {"y", 0}}))});
    EXPECT_EQ(hexs(n.to_bytes(cbor::deterministic)), "82" "a201000200" "c5" "a2617900617a00");
    // the same value however its map was ordered
    cbor a = cbor::map({{1, 1}, {2, 2}});
    cbor b = cbor::map({{2, 2}, {1, 1}});
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.hash(), b.hash());
    EXPECT_EQ(hexs(a.to_bytes(cbor::deterministic)), hexs(b.to_bytes(cbor::deterministic)));
    EXPECT_NE(hexs(a.to_bytes()), hexs(b.to_bytes()));
}

TEST(Cbor_Tests, MakingAndReading) {
    EXPECT_EQ(hexs(cbor(int8_t(-128)).to_bytes()), "387f");
    EXPECT_EQ(hexs(cbor(INT64_MIN).to_bytes()), "3b7fffffffffffffff");
    EXPECT_EQ(*cbor(INT64_MIN).as_int(), INT64_MIN);
    EXPECT_EQ(hexs(cbor(UINT64_MAX).to_bytes()), "1bffffffffffffffff");
    EXPECT_FALSE(cbor(UINT64_MAX).as_int());
    EXPECT_EQ(*cbor(UINT64_MAX).as_uint(), UINT64_MAX);
    EXPECT_FALSE(cbor(-1).as_uint());
    EXPECT_EQ(*cbor(-1).as_double(), -1.0);
    EXPECT_EQ(hexs(cbor(1.5f).to_bytes()), "f93e00");
    EXPECT_EQ(hexs(cbor(0.1f).to_bytes()), "fa3dcccccd");
    EXPECT_EQ(hexs(cbor(0.1).to_bytes()), "fb3fb999999999999a");
    EXPECT_EQ(hexs(cbor(std::nan("")).to_bytes()), "f97e00");
    EXPECT_EQ(hexs(cbor(1e-40).to_bytes()), "fb37a16c262777579c");          // below a float's subnormals
    EXPECT_EQ(hexs(cbor(double(std::ldexp(1.0f, -149))).to_bytes()), "fa00000001");   // a float subnormal
    EXPECT_EQ(hexs(cbor(std::ldexp(1.0, -24)).to_bytes()), "f90001");       // a half subnormal
    EXPECT_EQ(hexs(cbor(std::ldexp(3.0, -25)).to_bytes()), "fa33c00000");   // between half's steps
    EXPECT_EQ(hexs(cbor(65520.0).to_bytes()), "fa477ff000");                // past half's largest
    // big integers: within 64 bits an integer, past them a bignum
    using sgcl::math::big_integer;
    EXPECT_EQ(hexs(cbor(big_integer("18446744073709551615")).to_bytes()), "1bffffffffffffffff");
    EXPECT_EQ(hexs(cbor(big_integer("-18446744073709551616")).to_bytes()), "3bffffffffffffffff");
    EXPECT_EQ(hexs(cbor(big_integer("18446744073709551616")).to_bytes()), "c249010000000000000000");
    EXPECT_EQ(hexs(cbor(big_integer("-18446744073709551617")).to_bytes()), "c349010000000000000000");
    for (const char* t : {"0", "-1", "18446744073709551616", "-18446744073709551617", "123456789012345678901234567890", "-99999999999999999999999"}) {
        big_integer v{sgcl::string(t)};
        auto back = cbor::parse(cbor(v).to_bytes());
        ASSERT_TRUE(back);
        EXPECT_EQ(*back->as_big_integer(), v) << t;
    }
    // the tags of time and decimal fractions
    auto t = sgcl::time::datetime::from_unix_milli(1363896240500, sgcl::time::zone::utc());
    EXPECT_EQ(diag(cbor::epoch_time(t)), "1(1363896240.5)");
    EXPECT_EQ(diag(cbor::epoch_time(sgcl::time::datetime::from_unix(1363896240, sgcl::time::zone::utc()))), "1(1363896240)");
    EXPECT_EQ(diag(cbor::date_time(t)), "0(\"2013-03-21T20:04:00.5Z\")");
    EXPECT_EQ(cbor::date_time(t).as_time()->unix_milli(), 1363896240500);
    EXPECT_EQ(cbor::epoch_time(t).as_time()->unix_milli(), 1363896240500);
    auto zoned = cbor::tagged(0, "2013-03-21T22:04:00+02:00").as_time();
    ASSERT_TRUE(zoned);
    EXPECT_EQ(zoned->unix(), 1363896240);
    EXPECT_FALSE(cbor::tagged(0, "yesterday").as_time());
    EXPECT_FALSE(cbor::tagged(1, "1").as_time());
    EXPECT_FALSE(cbor::tagged(2, 1).as_time());
    EXPECT_EQ(diag(cbor::decimal(big_integer(27315), -2)), "4([-2, 27315])");   // RFC 8949 §3.4.4
    auto d = cbor::parse(bytes("c48221196ab3"))->as_decimal();
    ASSERT_TRUE(d);
    EXPECT_EQ(d->first, big_integer(27315));
    EXPECT_EQ(d->second, -2);
    EXPECT_FALSE(cbor::tagged(4, cbor::array({1})).as_decimal());
    EXPECT_FALSE(cbor::tagged(2, "x").as_big_integer());
    // simple values
    EXPECT_EQ(*cbor::simple(16).as_simple(), 16);
    EXPECT_THROW(cbor::simple(20), std::invalid_argument);
    EXPECT_THROW(cbor::simple(31), std::invalid_argument);
    EXPECT_EQ(hexs(cbor::simple(32).to_bytes()), "f820");
    EXPECT_FALSE(cbor(true).as_simple());
    // an extension is not CBOR
    EXPECT_THROW(cbor::extension(5, sgcl::vector<byte>(2)).to_bytes(), std::invalid_argument);
    EXPECT_EQ(diag(cbor::extension(-5, sgcl::vector<byte>(2))), "extension(-5, h'0000')");
    EXPECT_EQ(cbor::extension(-5, sgcl::vector<byte>(2)).tag(), 251u);
}

TEST(Cbor_Tests, Lookups) {
    cbor m = cbor::map({{1, "one"}, {-1, "minus"}, {"k", 2}, {cbor::bytes(sgcl::vector<byte>(1)), 3}});
    EXPECT_EQ(*m[1].as_string(), "one");
    EXPECT_EQ(*m[-1].as_string(), "minus");
    EXPECT_EQ(*m["k"].as_int(), 2);
    EXPECT_EQ(*m[sgcl::string("k")].as_int(), 2);
    EXPECT_EQ(*m[cbor::bytes(sgcl::vector<byte>(1))].as_int(), 3);
    EXPECT_EQ(m[7].type(), cbor::kind::null);
    EXPECT_TRUE(m.contains(-1));
    EXPECT_FALSE(m.contains(2));
    cbor a = cbor::array({10, 20, 30});
    EXPECT_EQ(*a[2].as_int(), 30);
    EXPECT_EQ(a[3].type(), cbor::kind::null);
    EXPECT_EQ(a[-1].type(), cbor::kind::null);
    EXPECT_EQ(a["x"].type(), cbor::kind::null);
    EXPECT_EQ(a.elements().size(), 3u);
    EXPECT_EQ(m.members().size(), 4u);
    EXPECT_TRUE(cbor(1).elements().empty());
    EXPECT_TRUE(cbor(1).members().empty());
    // new versions
    EXPECT_EQ(diag(m.set("k", 3).set(9, true)), "{1: \"one\", -1: \"minus\", \"k\": 3, h'00': 3, 9: true}");
    EXPECT_EQ(diag(m.erase(1).erase("nope")), "{-1: \"minus\", \"k\": 2, h'00': 3}");
    EXPECT_EQ(diag(a.set(1, "x").set(3, "end").set(9, "far")), "[10, \"x\", 30, \"end\"]");
    EXPECT_EQ(diag(a.erase(0).push_back(40)), "[20, 30, 40]");
    EXPECT_EQ(diag(cbor().push_back(1)), "[1]");
    EXPECT_EQ(diag(cbor(5).push_back(1)), "5");
    EXPECT_EQ(diag(a), "[10, 20, 30]");   // unchanged
    // tags
    cbor tg = cbor::tagged(42, a);
    EXPECT_EQ(tg.tag(), 42u);
    EXPECT_EQ(tg.content(), a);
    EXPECT_EQ(cbor(1).content().type(), cbor::kind::null);
    EXPECT_EQ(cbor(1).tag(), 0u);
}

TEST(Cbor_Tests, Boundaries) {
    cbor none;
    EXPECT_EQ(none.type(), cbor::kind::null);
    EXPECT_EQ(none, cbor(nullptr));
    EXPECT_NE(none, cbor::undefined());
    EXPECT_EQ(diag(none), "null");
    EXPECT_EQ(hexs(none.to_bytes()), "f6");
    EXPECT_EQ(none.size(), 0u);
    EXPECT_TRUE(none.empty());
    EXPECT_FALSE(none.as_bool());
    EXPECT_FALSE(none.as_int());
    EXPECT_FALSE(none.as_string());
    EXPECT_FALSE(none.as_bytes());
    EXPECT_FALSE(none.as_time());
    EXPECT_FALSE(none.as_big_integer());
    EXPECT_FALSE(none.as_double());
    cbor moved = cbor::array({1});
    cbor to = std::move(moved);
    EXPECT_EQ(to.size(), 1u);
    // empty input, an empty array and map made
    EXPECT_EQ(cbor::parse(sgcl::slice<const byte>()).error().code(), errc::unexpected_end);
    EXPECT_EQ(hexs(cbor::array(sgcl::vector<cbor>()).to_bytes()), "80");
    EXPECT_EQ(hexs(cbor::map(sgcl::vector<cbor::member>()).to_bytes()), "a0");
    EXPECT_EQ(hexs(cbor::bytes(sgcl::slice<const byte>()).to_bytes()), "40");
    EXPECT_EQ(hexs(cbor(sgcl::string()).to_bytes()), "60");
    // a value inside itself
    cbor s = cbor::array({1});
    s = cbor::array({s, s});
    EXPECT_EQ(diag(s), "[[1], [1]]");
    // the depth, bounded and not recursed
    cbor::options o;
    o.max_depth = 3;
    EXPECT_TRUE(cbor::parse(bytes("81818100"), o));
    EXPECT_EQ(cbor::parse(bytes("8181818100"), o).error().code(), errc::depth_limit);
    EXPECT_EQ(cbor::parse(bytes("c1c1c1c100"), o).error().code(), errc::depth_limit);
    std::string deep(200000, '\x81');
    deep += '\x00';
    auto dr = cbor::parse(sgcl::slice<const byte>(reinterpret_cast<const byte*>(deep.data()), deep.size()));
    ASSERT_FALSE(dr);
    EXPECT_EQ(dr.error().code(), errc::depth_limit);
    o.max_depth = 300000;
    auto deep_ok = cbor::parse(sgcl::slice<const byte>(reinterpret_cast<const byte*>(deep.data()), deep.size()), o);
    ASSERT_TRUE(deep_ok);
    EXPECT_EQ(deep_ok->to_bytes().size(), deep.size());
    EXPECT_EQ(deep_ok->to_string().size(), 2 * 200000 + 1);
    EXPECT_EQ(*deep_ok, *deep_ok);
    (void)deep_ok->hash();
    // a count past the input refused before anything is held
    EXPECT_EQ(cbor::parse(bytes("9bffffffffffffffff")).error().code(), errc::unexpected_end);
    EXPECT_EQ(cbor::parse(bytes("bbffffffffffffffff")).error().code(), errc::unexpected_end);
    EXPECT_EQ(cbor::parse(bytes("5bffffffffffffffff")).error().code(), errc::unexpected_end);
    // every prefix of a value is refused
    cbor big = cbor::map({{"list", cbor::array({1, 2.5, "x", cbor::bytes(sgcl::vector<byte>(30))})}, {cbor::tagged(1, 5), cbor::simple(99)}});
    auto all = big.to_bytes();
    for (size_t n = 0; n < all.size(); ++n) {
        auto r = cbor::parse(sgcl::slice<const byte>(all.data(), n));
        ASSERT_FALSE(r) << n;
        EXPECT_EQ(r.error().code(), errc::unexpected_end) << n;
    }
    EXPECT_EQ(*cbor::parse(all), big);
    // equality across kinds and of floats
    EXPECT_NE(cbor(1), cbor(1.0));
    EXPECT_EQ(cbor(std::nan("")), cbor(-std::nan("1")));
    EXPECT_NE(cbor(0.0), cbor(-0.0));
    EXPECT_NE(cbor("a"), cbor::bytes(sgcl::vector<byte>{byte('a')}));
    EXPECT_NE(cbor::extension(1, sgcl::vector<byte>(1)), cbor::extension(2, sgcl::vector<byte>(1)));
    EXPECT_EQ(std::hash<cbor>()(big), big.hash());
}

TEST(Cbor_Tests, Json) {
    // the corpora of nativejson-benchmark when they are there, else a text
    // of every kind: JSON to CBOR and back is the same JSON
    std::vector<std::string> texts = {R"({"a":[1,-2,3.5,1e300,"x",true,false,null,{"b":{}}],"c":[],"18446744073709551615":18446744073709551615})"};
    for (const char* name : {"twitter.json", "citm_catalog.json", "canada.json"}) {
        auto path = std::string(std::getenv("HOME")) + "/Programming/oracles/nativejson/" + name;
        if (FILE* f = std::fopen(path.c_str(), "rb")) {
            std::string s;
            char buf[65536];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
                s.append(buf, n);
            }
            std::fclose(f);
            texts.push_back(s);
        }
    }
    for (auto& t : texts) {
        auto j = json::parse(sgcl::string(t));
        ASSERT_TRUE(j);
        cbor c = cbor::from_json(*j);
        auto back = cbor::parse(c.to_bytes());
        ASSERT_TRUE(back);
        EXPECT_EQ(*back, c);
        EXPECT_EQ(back->to_json(), *j);
    }
    // §6.1: what JSON has no form for
    cbor c = cbor::map({{1, cbor::bytes(sgcl::vector<byte>{byte(0xfb), byte(0xff)})},
                        {"b64", cbor::tagged(22, cbor::bytes(sgcl::vector<byte>{byte(0xfb), byte(0xff)}))},
                        {"hex", cbor::tagged(23, cbor::array({cbor::bytes(sgcl::vector<byte>{byte(0xab)})}))},
                        {"nan", std::nan("")}, {"u", cbor::undefined()}, {"s", cbor::simple(40)},
                        {"big", sgcl::math::big_integer("123456789012345678901234567890")},
                        {"neg", cbor(sgcl::math::big_integer("-18446744073709551616"))},
                        {"t", cbor::tagged(1, 5)}, {cbor::array({1}), "key"}});
    EXPECT_EQ(std::string(c.to_json().to_string().view()),
              R"({"1":"-_8","b64":"+/8=","hex":["ab"],"nan":null,"u":null,"s":null,"big":123456789012345678901234567890,"neg":-18446744073709551616,"t":5,"[1]":"key"})");
    EXPECT_EQ(cbor::from_json(json()).type(), cbor::kind::null);
    EXPECT_EQ(cbor(1).to_json(), json(1));
    EXPECT_EQ(std::string(cbor::map({}).to_json().to_string().view()), "{}");
    EXPECT_EQ(std::string(cbor::array({}).to_json().to_string().view()), "[]");
}

TEST(Cbor_Tests, PredicatesAndFallbacks) {
    cbor m = cbor::map({{"n", 5}, {"f", 1.5}, {"s", "x"}, {"b", true}, {"bytes", cbor::bytes(sgcl::vector<byte>(1))},
                        {"a", cbor::array({})}, {"t", cbor::tagged(1, 0)}, {"z", cbor()}});
    EXPECT_TRUE(m.is_map());
    EXPECT_TRUE(m["n"].is_integer() && m["n"].is_number());
    EXPECT_TRUE(m["f"].is_number() && !m["f"].is_integer());
    EXPECT_TRUE(m["s"].is_text() && !m["s"].is_bytes());
    EXPECT_TRUE(m["bytes"].is_bytes());
    EXPECT_TRUE(m["b"].is_bool());
    EXPECT_TRUE(m["a"].is_array());
    EXPECT_TRUE(m["t"].is_tag());
    EXPECT_TRUE(m["z"].is_null() && m["missing"].is_null());
    EXPECT_FALSE(cbor::undefined().is_null());
    EXPECT_EQ(m["n"].as_int(-1), 5);
    EXPECT_EQ(m["s"].as_int(-1), -1);
    EXPECT_EQ(m["n"].as_uint(7u), 5u);
    EXPECT_EQ(m["f"].as_double(0), 1.5);
    EXPECT_EQ(m["s"].as_double(2.5), 2.5);
    EXPECT_EQ(m["s"].as_string("?"), "x");
    EXPECT_EQ(m["n"].as_string("?"), "?");
    EXPECT_EQ(m["b"].as_bool(false), true);
    EXPECT_EQ(m["n"].as_bool(false), false);
}

TEST(Cbor_Tests, ItemsOfAStream) {
    // a CBOR sequence (RFC 8742): items one after another, each read alone
    std::string seq;
    for (auto h : {"a26161016162820203", "9f018202039f0405ffff", "7f657374726561646d696e67ff", "c11a514b67b0", "f6", "bf6346756ef563416d7421ff"}) {
        auto b = from_hex(h);
        seq.append(reinterpret_cast<const char*>(b.data()), b.size());
    }
    for (size_t piece : {1, 2, 3, 7, 4096}) {
        sgcl::io::reader in(make_tracked<dribble>(seq, piece));
        std::vector<std::string> got;
        for (;;) {
            auto v = cbor::parse(in);
            if (!v) {
                EXPECT_EQ(v.error().code(), errc::unexpected_end);
                EXPECT_EQ(v.error().offset(), 0u);
                break;
            }
            got.push_back(diag(*v));
        }
        ASSERT_EQ(got.size(), 6u) << piece;
        EXPECT_EQ(got[1], "[1, [2, 3], [4, 5]]");
        EXPECT_EQ(got[2], "\"streaming\"");
        EXPECT_EQ(got[5], "{\"Fun\": true, \"Amt\": -2}");
    }
    // cut inside an item, a stream that fails, past max_size, too deep
    sgcl::io::reader cut(make_tracked<dribble>(seq.substr(0, 5), 3));
    EXPECT_EQ(cbor::parse(cut).error().code(), errc::unexpected_end);
    sgcl::io::reader bad(make_tracked<failing>(std::string("\x82\x01", 2)));
    EXPECT_EQ(cbor::parse(bad).error().code(), errc::io);
    cbor::options o;
    o.max_size = 100;
    std::string big = "\x59\x01\x00" + std::string(256, 'x');
    sgcl::io::reader b(make_tracked<dribble>(big, 4096));
    EXPECT_EQ(cbor::parse(b, o).error().code(), errc::limit_exceeded);
    std::string huge("\x5b\xff\xff\xff\xff\xff\xff\xff\xff", 9);
    sgcl::io::reader h(make_tracked<dribble>(huge, 4096));
    EXPECT_EQ(cbor::parse(h).error().code(), errc::limit_exceeded);
    o.max_depth = 3;
    sgcl::io::reader d(make_tracked<dribble>(std::string(10, '\x81') + '\x00', 1));
    EXPECT_EQ(cbor::parse(d, o).error().code(), errc::depth_limit);
    // a malformed head: what was read is parsed, and says why
    sgcl::io::reader m(make_tracked<dribble>(std::string("\x1c\x00", 2), 1));
    EXPECT_EQ(cbor::parse(m).error().code(), errc::syntax);
    sgcl::io::reader m2(make_tracked<dribble>(std::string("\x82\x1c\x00", 3), 1));
    EXPECT_FALSE(cbor::parse(m2));
    // in a task
    auto t = sgcl::async::spawn([](std::string seq) -> sgcl::async::task<int> {
        sgcl::io::reader in(make_tracked<dribble>(seq, 2));
        auto x = co_await cbor::async_parse(in);
        if (!x || diag(*x) != "{\"a\": 1, \"b\": [2, 3]}") {
            co_return -1;
        }
        auto y = co_await cbor::async_parse(in, cbor::options());
        co_return y && y->size() == 3 ? 1 : -2;
    }(seq));
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}
