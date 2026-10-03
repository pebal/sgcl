//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of JSON (DESIGN 408): the default and the moved-from
// value, builder, token and the moved-from builder; numbers at the edges of
// int64, uint64 and double; empty keys; the depth at 0 and past any stack
// of calls; the bound of a token of a stream at its exact length; a stream
// failing half-way and one ending inside a token; the writer after a
// mistake; the text of a pretty value past what a string holds; a type's
// fields past the sixteen a list holds inline. What the suites beside it
// cover is not repeated: the depth of 1, 10 and 512 and a million brackets
// (JsonReader_Tests.DepthLimit), empty input and what trails a value
// (EmptyAndTrailing), the exact integers of a literal at the edges of the
// types (JsonNumbers_Tests.ExactIntegers), strings cut by the end
// (JsonStrings_Tests.InvalidStrings), the writer's mistakes
// (JsonWriter_Tests.MistakesInTheStructure), JSONTestSuite.
#include "json_common.h"

#include <cmath>
#include <limits>

using namespace json_test;
using sgcl::encoding::errc;
using sgcl::encoding::field_list;
using sgcl::string;

namespace {
    std::string s_of(const string& s) {
        return std::string(s.view());
    }

    // A value of n arrays inside one another, made by hand
    json nested(size_t n) {
        json v = json::array({});
        for (size_t i = 1; i < n; ++i) {
            v = json::array({v});
        }
        return v;
    }
}

// --- the value ---

TEST(JsonBoundaries_Tests, ADefaultAndAMovedFromValue) {
    json none;
    EXPECT_TRUE(none.is_null());
    EXPECT_EQ(none.type(), json::kind::null);
    EXPECT_EQ(none.size(), 0u);
    EXPECT_TRUE(none.empty());
    EXPECT_TRUE(none["a"].is_null());
    EXPECT_TRUE(none[size_t(0)].is_null());
    EXPECT_FALSE(none.contains(""));
    EXPECT_TRUE(none.elements().empty());
    EXPECT_TRUE(none.members().empty());
    EXPECT_FALSE(none.as_int());
    EXPECT_FALSE(none.number_text());
    EXPECT_EQ(none.to_string(), "null");
    EXPECT_EQ(none, json(nullptr));
    EXPECT_EQ(none.hash(), json(nullptr).hash());
    EXPECT_EQ(s_of(value_of(none.at_path("")).to_string()), "null");
    // moved from: the value it was (a tracked word's move is a copy)
    json v = value_of(json::parse("{\"a\":[1,2]}"));
    json w = std::move(v);
    EXPECT_EQ(v, w);
    EXPECT_EQ(v.to_string(), "{\"a\":[1,2]}");
    // assigned to itself, and given itself as an argument
    auto& same = w;
    w = same;
    EXPECT_EQ(w.to_string(), "{\"a\":[1,2]}");
    EXPECT_EQ(w.set("b", w).to_string(), "{\"a\":[1,2],\"b\":{\"a\":[1,2]}}");
    EXPECT_EQ(w["a"].push_back(w["a"]).to_string(), "[1,2,[1,2]]");
    EXPECT_EQ(w.set_path("/a/0", w).to_string(), "{\"a\":[{\"a\":[1,2]},2]}");
    EXPECT_EQ(w["a"].set(size_t(1), w["a"]).to_string(), "[1,[1,2]]");
}

// Numbers at the edges of int64, uint64 and double, read, kept and written
TEST(JsonBoundaries_Tests, NumbersAtTheEdges) {
    struct Case {
        const char* text;
        const char* written;
        bool is_int;
        bool is_uint;
    };
    const Case cases[] = {
        {"-9223372036854775808", "-9223372036854775808", true, false},
        {"9223372036854775807", "9223372036854775807", true, true},
        {"9223372036854775808", "9223372036854775808", false, true},
        {"18446744073709551615", "18446744073709551615", false, true},
        {"18446744073709551616", "18446744073709551616", false, false},    // kept as its text
        {"-9223372036854775809", "-9223372036854775809", false, false},
        {"-0", "-0", true, true},
        {"1.7976931348623157e308", "1.7976931348623157e+308", false, false},
        {"4.9e-324", "5e-324", false, false},
        {"2.4e-324", "0", true, true},                                      // under the least double: zero
        {"1e-400", "0", true, true},
        {"0e99999999999999999999", "0", true, true},
        {"1e-99999999999999999999", "0", true, true},
    };
    for (auto& c : cases) {
        auto v = json::parse(c.text);
        ASSERT_TRUE(v) << c.text << ": " << v.error().message();
        EXPECT_TRUE(v->is_number()) << c.text;
        EXPECT_EQ(s_of(v->to_string()), c.written) << c.text;
        EXPECT_EQ(bool(v->as_int()), c.is_int) << c.text;
        EXPECT_EQ(bool(v->as_uint()), c.is_uint) << c.text;
    }
    EXPECT_EQ(json::parse("18446744073709551616")->number_text(), "18446744073709551616");
    EXPECT_EQ(json::parse("-9223372036854775808")->as_int(), INT64_MIN);
    EXPECT_EQ(json::parse("18446744073709551615")->as_uint(), UINT64_MAX);
    // past the largest double: out_of_range, unless kept as text
    for (const char* past : {"1e309", "-1e309", "1.8e308", "1e99999999999999999999"}) {
        auto r = json::parse(past);
        ASSERT_FALSE(r) << past;
        EXPECT_EQ(r.error().code(), errc::out_of_range) << past;
        json::options keep;
        keep.keep_number_text = true;
        auto kept = json::parse(past, keep);
        ASSERT_TRUE(kept) << past;
        EXPECT_EQ(kept->number_text(), past);
        EXPECT_FALSE(kept->as_double()) << past;
    }
    // made of the limits of the C++ types, and written
    EXPECT_EQ(json(INT64_MIN).to_string(), "-9223372036854775808");
    EXPECT_EQ(json(UINT64_MAX).to_string(), "18446744073709551615");
    EXPECT_EQ(json(std::numeric_limits<double>::max()).to_string(), "1.7976931348623157e+308");
    EXPECT_EQ(json(std::numeric_limits<double>::denorm_min()).to_string(), "5e-324");
    EXPECT_EQ(json(std::numeric_limits<float>::max()).to_string(), "3.4028235e+38");
    EXPECT_EQ(json(-0.0).to_string(), "-0");
    // 2^63 as a double is the uint64 2^63 and no int64; 2^64 is neither
    EXPECT_FALSE(json(9223372036854775808.0).as_int());
    EXPECT_EQ(json(9223372036854775808.0).as_uint(), uint64_t(1) << 63);
    EXPECT_FALSE(json(18446744073709551616.0).as_uint());
    EXPECT_EQ(json(-9223372036854775808.0).as_int(), INT64_MIN);
    EXPECT_EQ(json(INT64_MAX), json(9223372036854775807.0));   // the integer and the double it rounds to
    EXPECT_EQ(json(0.0), json(-0.0));
    EXPECT_EQ(json(0.0).hash(), json(-0.0).hash());
}

// The empty key is a key: read, looked up, set, erased, reached by a pointer
TEST(JsonBoundaries_Tests, TheEmptyKey) {
    auto v = value_of(json::parse("{\"\":1,\"a\":{\"\":2}}"));
    EXPECT_EQ(v[""].as_int(), 1);
    EXPECT_TRUE(v.contains(""));
    EXPECT_EQ(value_of(v.at_path("/")).as_int(), 1);
    EXPECT_EQ(value_of(v.at_path("/a/")).as_int(), 2);
    EXPECT_EQ(v.set("", 3)[""].as_int(), 3);
    EXPECT_FALSE(v.erase("").contains(""));
    EXPECT_EQ(json().set_path("/", 4).to_string(), "{\"\":4}");
    EXPECT_EQ(error_of(json::parse("{\"\":1,\"\":2}")).code(), errc::duplicate_key);
    json::options dup;
    dup.allow_duplicate_keys = true;
    EXPECT_EQ(value_of(json::parse("{\"\":1,\"\":2}", dup))[""].as_int(), 2);
    // a pointer that is not one, and indexes that are not
    EXPECT_FALSE(v.at_path("a"));
    EXPECT_FALSE(value_of(json::parse("[1,2]")).at_path("/01"));
    EXPECT_FALSE(value_of(json::parse("[1,2]")).at_path("/2"));
    EXPECT_FALSE(value_of(json::parse("[1,2]")).at_path("/-"));
    EXPECT_FALSE(value_of(json::parse("[1,2]")).at_path("/18446744073709551616"));
    EXPECT_EQ(value_of(json::parse("[1,2]")).set_path("/3", 9).to_string(), "[1,2]");
    EXPECT_EQ(value_of(json::parse("[1,2]")).set_path("/2", 9).to_string(), "[1,2,9]");
}

// A depth of 0 takes the scalars alone; a value deeper than any stack of
// calls, built by hand or read with the bound lifted, is written, compared
// and hashed with no recursion
TEST(JsonBoundaries_Tests, DepthAtItsEdges) {
    json::options zero;
    zero.max_depth = 0;
    EXPECT_TRUE(json::parse("1", zero));
    EXPECT_EQ(error_of(json::parse("[]", zero)).code(), errc::depth_limit);
    json::reader r(string("{}"), zero);
    EXPECT_FALSE(r.next());
    EXPECT_EQ(r.last_error()->code(), errc::depth_limit);
    const size_t deep = 200000;
    json::options lifted;
    lifted.max_depth = UINT32_MAX;
    auto read = json::parse(string(std::string(deep, '[') + std::string(deep, ']')), lifted);
    ASSERT_TRUE(read);
    json made = nested(deep);
    EXPECT_EQ(*read, made);
    EXPECT_EQ(read->hash(), made.hash());
    EXPECT_EQ(made.to_string().size(), 2 * deep);
    EXPECT_NE(made, nested(deep - 1));
}

// A pretty text past the 4 GiB a string holds is length_error, found as
// the text passes it: the writing stops there, not after a text of
// terabytes (a value 100000 deep indented by 255 would be 1.3e12
// characters)
TEST(JsonBoundaries_Tests, APrettyTextPastTheStringLimit) {
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
    GTEST_SKIP() << "4 GiB of text under a sanitizer";
#endif
#endif
    json v = nested(100000);
    json::style wide;
    wide.indent = 255;
    EXPECT_THROW((void)v.to_string(wide), std::length_error);
    EXPECT_THROW((void)json::stringify(v, wide), std::length_error);
    EXPECT_EQ(v.to_string().size(), 200000u);   // compact, as before
}

// The limit is weighed where the text's block grows, not at each line: a
// text of exactly the limit is kept, one character more stops the writing
// with the text let go, compact as well as indented (the block never grows
// past the limit, so no append inside it passes the limit unweighed)
TEST(JsonBoundaries_Tests, TheLimitWeighedWhereTheTextGrows) {
    using sgcl::encoding::detail::JsonOut;
    using sgcl::encoding::detail::write_json;
    json v = nested(1000);   // compact: 2000 characters
    {
        JsonOut out(0, false);
        out.limit(2000);
        write_json(out, v);
        EXPECT_FALSE(out.too_long());
        EXPECT_FALSE(out.failed());
        EXPECT_EQ(out.text().size(), 2000u);
    }
    {
        JsonOut out(0, false);
        out.limit(1999);
        write_json(out, v);
        EXPECT_TRUE(out.too_long());
        EXPECT_TRUE(out.failed());
        EXPECT_LE(out.text().size(), 1999u);
    }
    {
        JsonOut out(255, false);   // the lines of 1000 levels: 1.3e8 characters
        out.limit(100000);
        write_json(out, v);
        EXPECT_TRUE(out.too_long());
        EXPECT_LE(out.text().size(), 100000u);
    }
}

// --- the builder ---

// A builder moved from is empty, as one build() emptied: either kind may
// begin it again
TEST(JsonBoundaries_Tests, ABuilderMovedFromIsEmpty) {
    json::builder b;
    b.set("a", 1);
    json::builder c = std::move(b);
    EXPECT_EQ(b.size(), 0u);
    EXPECT_EQ(b.build().to_string(), "[]");
    EXPECT_NO_THROW(b.push_back(2));
    EXPECT_EQ(b.build().to_string(), "[2]");
    EXPECT_EQ(c.build().to_string(), "{\"a\":1}");
    json::builder d;
    d.push_back(1);
    json::builder e;
    e.set("x", 0);
    e = std::move(d);
    EXPECT_EQ(e.build().to_string(), "[1]");
    EXPECT_NO_THROW(d.set("k", 1));
    EXPECT_EQ(d.build().to_string(), "{\"k\":1}");
    // assigned to itself, moved into itself: the same builder
    json::builder f;
    f.set("x", 1);
    auto& same = f;
    f = same;
    f = std::move(same);
    EXPECT_EQ(f.size(), 1u);
    EXPECT_EQ(f.build().to_string(), "{\"x\":1}");
    // a copy is a builder of its own
    json::builder g;
    g.push_back(1);
    json::builder h = g;
    h.push_back(2);
    EXPECT_EQ(g.build().to_string(), "[1]");
    EXPECT_EQ(h.build().to_string(), "[1,2]");
    // a key set twice counts twice and keeps its last value
    json::builder k;
    k.set("k", 1).set("k", 2);
    EXPECT_EQ(k.size(), 2u);
    EXPECT_EQ(k.build().to_string(), "{\"k\":2}");
}

// --- the reader ---

TEST(JsonBoundaries_Tests, ADefaultToken) {
    json::token t;
    EXPECT_EQ(t.type(), json::token::kind::null);
    EXPECT_TRUE(t.text().empty());
    EXPECT_FALSE(t.as_bool());
    EXPECT_FALSE(t.as_int());
    EXPECT_FALSE(t.as_double());
    EXPECT_EQ(t.as_int(7), 7);
}

// The bound of what a reader of a stream holds: a token of exactly
// max_token_size bytes is read however the stream hands the bytes out, a
// number one byte longer is out_of_range; a bound of 0 takes a stream
// with no token. (The bound is checked when the reader holds a token cut
// by the end of its data and asks for more, as XML's is: a token whole in
// one block is not measured.)
TEST(JsonBoundaries_Tests, ATokenAtTheExactBound) {
    for (size_t piece : {size_t(1), size_t(2), size_t(4096)}) {
        auto read = [&](const std::string& doc, size_t max) {
            json::options o;
            o.max_token_size = max;
            json::reader r(make_tracked<dribble>(doc, piece), o);
            size_t n = 0;
            while (r.next()) {
                ++n;
            }
            return std::pair{n, r.last_error() ? r.last_error()->code() : errc{}};
        };
        EXPECT_EQ(read("12345", 5), (std::pair{size_t(1), errc{}})) << piece;
        EXPECT_EQ(read("123456", 5), (std::pair{size_t(0), errc::out_of_range})) << piece;
        EXPECT_EQ(read("\"abc\"", 5), (std::pair{size_t(1), errc{}})) << piece;
        EXPECT_EQ(read("", 0), (std::pair{size_t(0), errc{}})) << piece;
        EXPECT_EQ(read("  \n ", 0), (std::pair{size_t(0), errc{}})) << piece;
        EXPECT_EQ(read("1", 0), (std::pair{size_t(0), errc::out_of_range})) << piece;
        // a value read whole: its text is what is held
        json::options o;
        o.max_token_size = 5;
        json::reader exact(make_tracked<dribble>(std::string("[1,2]"), piece), o);
        EXPECT_TRUE(exact.read()) << piece;
        json::reader past(make_tracked<dribble>(std::string("[1,223]"), piece), o);
        if (piece == 1) {   // more than the bound held, and more asked for
            EXPECT_FALSE(past.read());
            EXPECT_EQ(past.last_error()->code(), errc::out_of_range);
        }
    }
}

// A stream that ends inside a token, and one that fails half-way: the
// tokens before, then the error with its place; nothing after it
TEST(JsonBoundaries_Tests, AStreamEndingOrFailingInsideAToken) {
    for (auto [doc, at] : {std::pair{"[1,tr", 5}, std::pair{"[\"ab", 4}, std::pair{"[-", 2}, std::pair{"[1e", 3},
                           std::pair{"[1.", 3}, std::pair{"{\"a\"", 4}, std::pair{"{\"a\":", 5}}) {
        for (size_t piece : {size_t(1), size_t(4096)}) {
            json::reader r(make_tracked<dribble>(std::string(doc), piece));
            while (r.next()) {
            }
            ASSERT_TRUE(r.last_error()) << doc;
            EXPECT_EQ(r.last_error()->code(), errc::unexpected_end) << doc;
            EXPECT_EQ(r.last_error()->offset(), uint64_t(at)) << doc << " by " << piece;
            EXPECT_FALSE(r.next());
            EXPECT_FALSE(r.more());
            EXPECT_FALSE(r.read());
            EXPECT_FALSE(r.skip());
        }
    }
    json::reader failed(make_tracked<failing>("[1,\"ab"));
    auto first = failed.next();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->type(), json::token::kind::begin_array);
    EXPECT_TRUE(failed.next());
    EXPECT_FALSE(failed.next());
    ASSERT_TRUE(failed.last_error());
    EXPECT_EQ(failed.last_error()->code(), errc::io);
    EXPECT_EQ(failed.last_error()->offset(), 6u);
    EXPECT_EQ(error_of(json::parse(io::reader(make_tracked<failing>("{\"a\":1")))).code(), errc::io);
    // a stream of nothing: no value, no error, and parse's unexpected_end
    json::reader empty(make_tracked<dribble>(std::string(), 1));
    EXPECT_FALSE(empty.read());
    EXPECT_FALSE(empty.skip());
    EXPECT_FALSE(empty.last_error());
    EXPECT_EQ(error_of(json::parse(io::reader(make_tracked<dribble>(std::string(), 1)))).code(), errc::unexpected_end);
}

// --- the writer ---

// After a mistake nothing reaches the stream, not even the text before it,
// and every flush gives the mistake; nothing written is no write
TEST(JsonBoundaries_Tests, AWriterAfterAMistake) {
    sgcl::tracked_ptr out = make_tracked<sink>();
    json::writer w(out);
    EXPECT_TRUE(w.flush());
    EXPECT_EQ(out->writes, 0u);
    w.begin_array().value(1).end_object().value(2).end_array();
    auto f = w.flush();
    ASSERT_FALSE(f);
    EXPECT_EQ(f.error().code(), sgcl::encoding::make_error_code(errc::syntax));
    w.value(3);
    EXPECT_FALSE(w.flush());
    EXPECT_EQ(out->text, "");
    // a value at the top level after another is a stream of values
    sgcl::tracked_ptr lines = make_tracked<sink>();
    json::writer nd(lines);
    nd.value(1).value(json()).value("");
    ASSERT_TRUE(nd.flush());
    EXPECT_EQ(lines->text, "1\nnull\n\"\"\n");
}

// --- typed values ---

namespace {
    struct limits {
        int8_t i8 = 0;
        uint8_t u8 = 0;
        int64_t i64 = 0;
        uint64_t u64 = 0;
        float f = 0;
        void describe(field_list& l) {
            l.add("i8", i8);
            l.add("u8", u8);
            l.add("i64", i64);
            l.add("u64", u64);
            l.add("f", f);
        }
    };

    // More fields than a field_list holds inline (sixteen)
    struct wide {
        int v[40] = {};
        void describe(field_list& l) {
            static const char* const names[40] = {"f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9",
                "f10", "f11", "f12", "f13", "f14", "f15", "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
                "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31", "f32", "f33", "f34", "f35", "f36", "f37",
                "f38", "f39"};
            for (int i = 0; i < 40; ++i) {
                auto f = l.add(names[i], v[i]);
                if (i == 39) {
                    f.required();
                }
            }
        }
    };

    struct empty_type {
        void describe(field_list&) {
        }
    };
}

TEST(JsonBoundaries_Tests, TypedValuesAtTheirLimits) {
    auto a = json::parse<limits>("{\"i8\":-128,\"u8\":255,\"i64\":-9223372036854775808,\"u64\":18446744073709551615,\"f\":3.4028235e38}");
    ASSERT_TRUE(a) << a.error().message();
    EXPECT_EQ(a->i8, INT8_MIN);
    EXPECT_EQ(a->u8, UINT8_MAX);
    EXPECT_EQ(a->i64, INT64_MIN);
    EXPECT_EQ(a->u64, UINT64_MAX);
    EXPECT_EQ(a->f, std::numeric_limits<float>::max());
    EXPECT_EQ(value_of(json::stringify(*a)), "{\"i8\":-128,\"u8\":255,\"i64\":-9223372036854775808,\"u64\":18446744073709551615,\"f\":3.4028235e+38}");
    for (auto [text, path] : {std::pair{"{\"i8\":128}", "/i8"}, std::pair{"{\"u8\":-1}", "/u8"},
                              std::pair{"{\"i64\":9223372036854775808}", "/i64"}, std::pair{"{\"u64\":18446744073709551616}", "/u64"},
                              std::pair{"{\"f\":3.5e38}", "/f"}}) {
        auto r = json::parse<limits>(text);
        ASSERT_FALSE(r) << text;
        EXPECT_EQ(r.error().code(), errc::out_of_range) << text;
        EXPECT_EQ(r.error().path(), path) << text;
    }
    // NaN is no JSON number
    limits nan;
    nan.f = NAN;
    EXPECT_EQ(error_of(json::stringify(nan)).code(), errc::unsupported_value);
    // a type of no fields, and of more than the list holds inline
    EXPECT_EQ(value_of(json::stringify(empty_type{})), "{}");
    EXPECT_TRUE(json::parse<empty_type>("{\"x\":1}"));
    wide w;
    for (int i = 0; i < 40; ++i) {
        w.v[i] = i * 3;
    }
    auto text = value_of(json::stringify(w));
    auto back = json::parse<wide>(text);
    ASSERT_TRUE(back) << back.error().message();
    for (int i = 0; i < 40; ++i) {
        EXPECT_EQ(back->v[i], i * 3) << i;
    }
    auto missing = json::parse<wide>("{\"f0\":1}");
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().code(), errc::missing_field);
    EXPECT_EQ(missing.error().path(), "/f39");
    // a value of a type: empty text, and the typed reader of nothing
    EXPECT_EQ(error_of(json::parse<limits>("")).code(), errc::unexpected_end);
    json::reader none(string(""));
    EXPECT_FALSE(none.read<limits>());
    EXPECT_FALSE(none.last_error());
}
