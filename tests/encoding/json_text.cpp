//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The text of JSON (E2): numbers and strings both ways against Go, every
// file of JSONTestSuite against Go's v2, the reader in pieces, the errors
// and their places, the writer.
#include "json_common.h"

#include <bit>
#include <charconv>
#include <cmath>
#include <map>
#include <random>

using namespace json_test;
using sgcl::encoding::errc;

namespace {
    std::string written(double d) {
        return std::string(json(d).to_string().view());
    }

    std::string written_float(float f) {
        char buf[64];
        size_t n = sgcl::encoding::detail::number_text(buf, f);
        return std::string(buf, n);
    }
}

// --- numbers ---

TEST(JsonNumbers_Tests, DoublesWrittenAsGoWritesThem) {
    for (auto& w : json_oracle::doubles_written) {
        double d = std::bit_cast<double>(w.bits);
        EXPECT_EQ(written(d), w.text) << std::hex << w.bits;
    }
    rng r{12345};
    fnv_hash h;
    for (int i = 0; i < 1000000;) {
        double d = std::bit_cast<double>(r.next());
        if (!std::isfinite(d)) {
            continue;
        }
        char buf[64];
        size_t n = sgcl::encoding::detail::number_text(buf, d);
        h.add(std::string_view(buf, n));
        h.add("\n");
        ++i;
    }
    EXPECT_EQ(h.h, json_oracle::random_doubles_hash);
}

// A float is written with a float's shortest digits, not a double's
TEST(JsonNumbers_Tests, FloatsWrittenAsGoWritesThem) {
    for (auto& w : json_oracle::floats_written) {
        float f = std::bit_cast<float>(uint32_t(w.bits));
        EXPECT_EQ(written_float(f), w.text) << std::hex << w.bits;
    }
    EXPECT_EQ(written_float(0.1f), "0.1");
    EXPECT_EQ(written(double(0.1f)), "0.10000000149011612");
    rng r{54321};
    fnv_hash h;
    for (int i = 0; i < 1000000;) {
        float f = std::bit_cast<float>(uint32_t(r.next() >> 32));
        if (!std::isfinite(f)) {
            continue;
        }
        h.add(written_float(f));
        h.add("\n");
        ++i;
    }
    EXPECT_EQ(h.h, json_oracle::random_floats_hash);
}

// Each literal rounded once to a double and once to a float (never a
// double rounded again), against strconv.ParseFloat with 64 and 32 bits
TEST(JsonNumbers_Tests, LiteralsReadAsGoReadsThem) {
    using sgcl::encoding::detail::floating_of;
    auto bits64 = [](std::string_view lit) -> uint64_t {
        auto d = floating_of<double>(lit);
        return d ? std::bit_cast<uint64_t>(*d) : UINT64_MAX;
    };
    auto bits32 = [](std::string_view lit) -> uint64_t {
        auto f = floating_of<float>(lit);
        return f ? uint64_t(std::bit_cast<uint32_t>(*f)) : UINT64_MAX;
    };
    for (auto& r : json_oracle::literals_read) {
        EXPECT_EQ(bits64(r.literal), r.f64) << r.literal.substr(0, 60);
        EXPECT_EQ(bits32(r.literal), r.f32) << r.literal.substr(0, 60);
    }
    rng g{777};
    fnv_hash h;
    for (int i = 0; i < 500000; ++i) {
        auto lit = literal(g);
        for (uint64_t v : {bits64(lit), bits32(lit)}) {
            char b[8];
            for (int k = 0; k < 8; ++k) {
                b[k] = char(uint8_t(v >> (8 * k)));
            }
            h.add(std::string_view(b, 8));
        }
    }
    EXPECT_EQ(h.h, json_oracle::random_literals_hash);
}

// An integer from a literal, exactly or not at all: every form of 100 is
// 100, and nothing is rounded
// The fast path of floating_of (Clinger's: digits m <= 2^53, a power of
// ten within 10^±22) against std::from_chars, bit for bit, over literals
// of every shape it takes and of the shapes either side of its bounds: 16
// characters and 17, m at 2^53 and past it, 10^±22 and 10^±23, a sign,
// zeros before and after the point, an exponent with and without its sign
TEST(JsonNumbers_Tests, ShortLiteralsAsFromCharsReadsThem) {
    std::mt19937_64 rng(3);
    auto digits = [&](int n) {
        std::string d;
        for (int i = 0; i < n; ++i) {
            d += char('0' + rng() % 10);
        }
        return d;
    };
    size_t checked = 0;
    auto check = [&](const std::string& lit) {
        double want = 0;
        auto [end, ec] = std::from_chars(lit.data(), lit.data() + lit.size(), want, std::chars_format::general);
        if (ec != std::errc() || end != lit.data() + lit.size()) {
            return;
        }
        auto got = sgcl::encoding::detail::floating_of<double>(lit);
        ASSERT_TRUE(got.has_value()) << lit;
        ASSERT_EQ(std::bit_cast<uint64_t>(*got), std::bit_cast<uint64_t>(want)) << lit;
        ++checked;
    };
    for (const char* lit : {"0", "-0", "0.0", "-0.0", "9007199254740992", "9007199254740993", "900719925474099.3", "1e22", "1e23", "1e-22",
                            "1e-23", "123e-22", "-4.5e+3", "0.000001", "12.375", "1E5", "7e0", "0e30"}) {
        check(lit);
    }
    for (int i = 0; i < 1000000; ++i) {
        std::string lit = rng() % 4 == 0 ? "-" : "";
        lit += digits(int(rng() % 10) + 1);
        if (rng() % 2) {
            lit += "." + digits(int(rng() % 10) + 1);
        }
        if (rng() % 3 == 0) {
            lit += rng() % 2 ? "e" : "E";
            lit += rng() % 3 == 0 ? "-" : rng() % 2 ? "+" : "";
            lit += std::to_string(rng() % 30);
        }
        check(lit);
    }
    EXPECT_GT(checked, size_t(900000));
}

TEST(JsonNumbers_Tests, ExactIntegers) {
    using sgcl::encoding::detail::exact_integer;
    struct Case { std::string_view lit; std::optional<int64_t> i; std::optional<uint64_t> u; };
    const Case cases[] = {
        {"0", 0, 0}, {"-0", 0, 0}, {"100", 100, 100}, {"1e2", 100, 100}, {"100.0", 100, 100}, {"1.00e2", 100, 100}, {"10000e-2", 100, 100},
        {"0.001e5", 100, 100}, {"-100", -100, std::nullopt}, {"1.5", std::nullopt, std::nullopt}, {"1e-1", std::nullopt, std::nullopt}, {"15e-1", std::nullopt, std::nullopt},
        {"9223372036854775807", INT64_MAX, uint64_t(INT64_MAX)}, {"9223372036854775808", std::nullopt, uint64_t(INT64_MAX) + 1},
        {"-9223372036854775808", INT64_MIN, std::nullopt}, {"-9223372036854775809", std::nullopt, std::nullopt},
        {"18446744073709551615", std::nullopt, UINT64_MAX}, {"18446744073709551616", std::nullopt, std::nullopt}, {"1.8446744073709551615e19", std::nullopt, UINT64_MAX},
        {"1e19", std::nullopt, 10000000000000000000ull}, {"1e20", std::nullopt, std::nullopt}, {"0e999999999999", 0, 0}, {"1e999999999999", std::nullopt, std::nullopt},
        {"0.0000000000000000000000000000000000000000000000000001e52", 1, 1}, {"123.4500e2", 12345, 12345}, {"123.45001e2", std::nullopt, std::nullopt},
    };
    for (auto& c : cases) {
        EXPECT_EQ(exact_integer<int64_t>(c.lit), c.i) << c.lit;
        EXPECT_EQ(exact_integer<uint64_t>(c.lit), c.u) << c.lit;
    }
    EXPECT_EQ(exact_integer<int8_t>("127"), int8_t(127));
    EXPECT_EQ(exact_integer<int8_t>("128"), std::nullopt);
    EXPECT_EQ(exact_integer<int8_t>("-128"), int8_t(-128));
    EXPECT_EQ(exact_integer<int8_t>("-129"), std::nullopt);
    EXPECT_EQ(exact_integer<uint16_t>("65535"), uint16_t(65535));
    EXPECT_EQ(exact_integer<uint16_t>("65536"), std::nullopt);
    EXPECT_EQ(exact_integer<uint16_t>("-1"), std::nullopt);
}

// In a tree: an integer literal an int64 holds is one, else an uint64,
// else its text; -0 is the double -0; 1e400 is out of range
TEST(JsonNumbers_Tests, NumbersInATree) {
    auto v = json::parse(text("[9223372036854775807, -9223372036854775808, 18446744073709551615, 18446744073709551616, -9223372036854775809, -0, 0, 1.5, 1e-400, -1e-400]")).value();
    EXPECT_EQ(v[0].as_int(), INT64_MAX);
    EXPECT_EQ(v[1].as_int(), INT64_MIN);
    EXPECT_EQ(v[2].as_uint(), UINT64_MAX);
    EXPECT_EQ(v[2].as_int(), std::nullopt);
    EXPECT_EQ(v[3].number_text(), sgcl::string("18446744073709551616"));
    EXPECT_EQ(v[3].as_uint(), std::nullopt);
    EXPECT_EQ(v[3].as_double(), 18446744073709551616.0);
    EXPECT_EQ(v[4].number_text(), sgcl::string("-9223372036854775809"));
    EXPECT_TRUE(std::signbit(*v[5].as_double()));
    EXPECT_EQ(v[5].as_int(), 0);
    EXPECT_FALSE(std::signbit(*v[6].as_double()));
    EXPECT_EQ(v[7].as_double(), 1.5);
    EXPECT_EQ(v[7].as_int(), std::nullopt);
    EXPECT_EQ(v[8].as_double(), 0.0);
    EXPECT_TRUE(std::signbit(*v[9].as_double()));
    EXPECT_EQ(v.to_string(), "[9223372036854775807,-9223372036854775808,18446744073709551615,18446744073709551616,-9223372036854775809,-0,0,1.5,0,-0]");
    auto big = json::parse(text("[1e400]"));
    ASSERT_FALSE(big);
    EXPECT_EQ(big.error().code(), errc::out_of_range);
    EXPECT_EQ(big.error().offset(), 1u);
    // kept as text, a number past a double's range is not an error
    json::options keep;
    keep.keep_number_text = true;
    auto kept = json::parse(text("[1e400, 0.10000000000000000000001]"), keep).value();
    EXPECT_EQ(kept[0].number_text(), sgcl::string("1e400"));
    EXPECT_EQ(kept[0].as_double(), std::nullopt);
    EXPECT_EQ(kept[1].number_text(), sgcl::string("0.10000000000000000000001"));
    EXPECT_EQ(kept[1].as_double(), 0.1);
    EXPECT_EQ(kept.to_string(), "[1e400,0.10000000000000000000001]");
}

// --- strings ---

TEST(JsonStrings_Tests, WrittenAsGoWritesThem) {
    for (auto& s : json_oracle::strings_written) {
        std::string plain, html;
        sgcl::encoding::detail::write_string(plain, s.raw, false);
        sgcl::encoding::detail::write_string(html, s.raw, true);
        EXPECT_EQ(plain, s.plain) << s.raw;
        EXPECT_EQ(html, s.html) << s.raw;
        EXPECT_EQ(json(sgcl::string(s.raw)).to_string(), sgcl::string(s.plain));
        EXPECT_EQ(json(sgcl::string(s.raw)).to_string({0, true}), sgcl::string(s.html));
    }
}

// Strings of up to 64 bytes, which JsonOut quotes in one pass when none of
// their bytes is to be escaped, against write_string's loop: every byte
// value at the start, the middle and the end of strings of each length
// either side of the bound, plain and with escape_html
TEST(JsonStrings_Tests, ShortStringsAsTheLoopWritesThem) {
    for (size_t n : {1, 2, 7, 8, 9, 16, 31, 63, 64, 65, 66}) {
        for (int b = 0; b < 256; ++b) {
            for (size_t at : {size_t(0), n / 2, n - 1}) {
                std::string raw(n, 'a');
                raw[at] = char(b);
                for (bool html : {false, true}) {
                    std::string want;
                    sgcl::encoding::detail::write_string(want, raw, html);
                    auto got = json(sgcl::string(raw)).to_string({0, html});
                    ASSERT_EQ(std::string(got.view()), want) << "length " << n << " byte " << b << " at " << at << (html ? " html" : "");
                }
            }
        }
    }
}

TEST(JsonStrings_Tests, EscapesDecoded) {
    auto v = json::parse(text(R"(["\"\\\/\b\f\n\r\t", "\u0041\u00e9\u20AC\ud83d\ude00", "\u0000", "a\u002fb"])")).value();
    EXPECT_EQ(v[0].as_string(), sgcl::string("\"\\/\b\f\n\r\t"));
    EXPECT_EQ(v[1].as_string(), sgcl::string("A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80"));
    EXPECT_EQ(v[2].as_string()->size(), 1u);
    EXPECT_EQ(v[3].as_string(), sgcl::string("a/b"));
    // the raw DEL and every byte of UTF-8 are taken as they are
    EXPECT_EQ(json::parse(text("\"\x7f\xc5\xbc\"")).value().as_string(), sgcl::string("\x7f\xc5\xbc"));
}

// What a string may not hold, and where the error is; with
// allow_invalid_utf8, U+FFFD in its place
TEST(JsonStrings_Tests, InvalidStrings) {
    struct Case { std::string_view text; errc code; uint64_t offset; std::string_view lenient; };
    const Case cases[] = {
        {"\"a\nb\"", errc::syntax, 2, ""},                      // a raw control character
        {"\"a\x01\"", errc::syntax, 2, ""},
        {"\"\\x\"", errc::invalid_escape, 1, ""},
        {"\"\\u12G4\"", errc::invalid_escape, 1, ""},
        {"\"\\ud800\"", errc::invalid_escape, 1, "\xEF\xBF\xBD"},  // a lone high surrogate
        {"\"\\udc00\"", errc::invalid_escape, 1, "\xEF\xBF\xBD"},  // a lone low one
        {"\"\\ud800\\u0041\"", errc::invalid_escape, 1, "\xEF\xBF\xBD" "A"},
        {"\"\\ud800x\"", errc::invalid_escape, 1, "\xEF\xBF\xBD" "x"},
        {"\"\xff\"", errc::invalid_utf8, 1, "\xEF\xBF\xBD"},
        {"\"\xc0\xaf\"", errc::invalid_utf8, 1, "\xEF\xBF\xBD\xEF\xBF\xBD"},          // overlong
        {"\"\xed\xa0\x80\"", errc::invalid_utf8, 1, "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD"},  // an encoded surrogate
        {"\"\xf4\x90\x80\x80\"", errc::invalid_utf8, 1, "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD"},  // past U+10FFFF
        {"\"\xc3(\"", errc::invalid_utf8, 1, "\xEF\xBF\xBD("},     // a sequence cut short
        {"\"\xf5\x80\"", errc::invalid_utf8, 1, "\xEF\xBF\xBD\xEF\xBF\xBD"},
        {"\"abc", errc::unexpected_end, 4, ""},
        {"\"abc\\", errc::unexpected_end, 4, ""},
        {"\"\\u12", errc::unexpected_end, 1, ""},
    };
    for (auto& c : cases) {
        auto r = json::parse(text(c.text));
        ASSERT_FALSE(r) << c.text;
        EXPECT_EQ(r.error().code(), c.code) << c.text;
        EXPECT_EQ(r.error().offset(), c.offset) << c.text;
        json::options lenient;
        lenient.allow_invalid_utf8 = true;
        auto l = json::parse(text(c.text), lenient);
        if (c.lenient.empty()) {
            EXPECT_FALSE(l) << c.text;
        } else {
            ASSERT_TRUE(l) << c.text;
            EXPECT_EQ(l->as_string(), sgcl::string(c.lenient)) << c.text;
        }
    }
}

// --- JSONTestSuite ---

namespace {
    struct Verdict {
        bool reads;      // the reader takes it as exactly one value
        bool parses;     // json::parse takes it
    };

    Verdict judge(const std::string& data, std::string* tokens = nullptr) {
        Verdict v{};
        json::reader r(text(data));
        std::string out;
        size_t top = 0;
        bool failed = false;
        while (auto t = r.next()) {
            add_token(out, *t);
            if (r.depth() == 0 && t->type() != json::token::kind::key) {
                ++top;
            }
        }
        failed = r.last_error().has_value();
        v.reads = !failed && top == 1;
        v.parses = json::parse(text(data)).has_value();
        if (tokens) {
            *tokens = out;
        }
        return v;
    }
}

// Every file of test_parsing: y_ taken, n_ refused, and every one — the i_
// files too — decided as Go's v2 decides it (jsontext for the reader,
// Unmarshal into any for parse); the tokens and the value written back
// compact and indented are Go's
TEST(JsonSuite_Tests, EveryFileAsGoV2) {
    auto dir = suite_dir();
    if (!std::filesystem::exists(dir)) {
        GTEST_SKIP() << "JSONTestSuite is not in " << dir;
    }
    size_t files = 0;
    std::map<std::string, std::string> implementation_defined;
    for (auto& f : json_oracle::suite) {
        auto data = read_file(dir / std::string(f.name));
        std::string tokens;
        auto v = judge(data, &tokens);
        ++files;
        EXPECT_EQ(v.reads, f.valid) << f.name;
        EXPECT_EQ(v.parses, f.parses) << f.name;
        if (f.name.starts_with("y_")) {
            // a key given twice is valid RFC 8259, and refused by default
            // (as Go's v2 refuses it): taken with allow_duplicate_keys
            if (f.name.find("duplicated_key") != std::string_view::npos) {
                json::options o;
                o.allow_duplicate_keys = true;
                EXPECT_FALSE(v.parses) << f.name;
                EXPECT_TRUE(json::parse(text(data), o)) << f.name;
            } else {
                EXPECT_TRUE(v.parses) << f.name;
            }
        }
        if (f.name.starts_with("n_")) {
            EXPECT_FALSE(v.reads) << f.name;
            EXPECT_FALSE(v.parses) << f.name;
        }
        if (f.name.starts_with("i_")) {
            implementation_defined[std::string(f.name)] = v.parses ? "taken" : v.reads ? "read, not parsed" : "refused";
        }
        if (v.reads && f.valid) {
            EXPECT_EQ(hash_of(tokens), f.tokens) << f.name << "\n" << tokens;
        }
        if (v.parses && f.parses) {
            auto value = json::parse(text(data)).value();
            auto compact = value.to_string();
            EXPECT_EQ(hash_of(compact.view()), f.compact) << f.name << " " << compact;
            EXPECT_EQ(hash_of(value.to_string(json::pretty).view()), f.pretty) << f.name;
            // read back, the same value and the same text
            auto again = json::parse(compact).value();
            EXPECT_EQ(again, value) << f.name;
            EXPECT_EQ(again.to_string(), compact) << f.name;
        }
    }
    EXPECT_EQ(files, 318u);
    // the implementation-defined files, decided as Go's v2 decides them
    // (and so as the defaults of json.h say): invalid UTF-8 and lone
    // surrogates refused, a number past a double refused by parse (the
    // reader passes it on), deep nesting within 512 taken, a BOM refused
    const std::map<std::string, std::string> expected = {
        {"i_number_double_huge_neg_exp.json", "taken"},
        {"i_number_huge_exp.json", "read, not parsed"},
        {"i_number_neg_int_huge_exp.json", "read, not parsed"},
        {"i_number_pos_double_huge_exp.json", "read, not parsed"},
        {"i_number_real_neg_overflow.json", "read, not parsed"},
        {"i_number_real_pos_overflow.json", "read, not parsed"},
        {"i_number_real_underflow.json", "taken"},
        {"i_number_too_big_neg_int.json", "taken"},
        {"i_number_too_big_pos_int.json", "taken"},
        {"i_number_very_big_negative_int.json", "taken"},
        {"i_object_key_lone_2nd_surrogate.json", "refused"},
        {"i_string_1st_surrogate_but_2nd_missing.json", "refused"},
        {"i_string_1st_valid_surrogate_2nd_invalid.json", "refused"},
        {"i_string_UTF-16LE_with_BOM.json", "refused"},
        {"i_string_UTF-8_invalid_sequence.json", "refused"},
        {"i_string_UTF8_surrogate_U+D800.json", "refused"},
        {"i_string_incomplete_surrogate_and_escape_valid.json", "refused"},
        {"i_string_incomplete_surrogate_pair.json", "refused"},
        {"i_string_incomplete_surrogates_escape_valid.json", "refused"},
        {"i_string_invalid_lonely_surrogate.json", "refused"},
        {"i_string_invalid_surrogate.json", "refused"},
        {"i_string_invalid_utf-8.json", "refused"},
        {"i_string_inverted_surrogates_U+1D11E.json", "refused"},
        {"i_string_iso_latin_1.json", "refused"},
        {"i_string_lone_second_surrogate.json", "refused"},
        {"i_string_lone_utf8_continuation_byte.json", "refused"},
        {"i_string_not_in_unicode_range.json", "refused"},
        {"i_string_overlong_sequence_2_bytes.json", "refused"},
        {"i_string_overlong_sequence_6_bytes.json", "refused"},
        {"i_string_overlong_sequence_6_bytes_null.json", "refused"},
        {"i_string_truncated-utf-8.json", "refused"},
        {"i_string_utf16BE_no_BOM.json", "refused"},
        {"i_string_utf16LE_no_BOM.json", "refused"},
        {"i_structure_500_nested_arrays.json", "taken"},
        {"i_structure_UTF-8_BOM_empty_object.json", "refused"},
    };
    EXPECT_EQ(implementation_defined, expected);
}

// --- the reader ---

namespace {
    // The tokens of a text given to the reader through a stream in pieces
    // of n bytes (0: the text in memory)
    std::optional<std::string> tokens_in_pieces(const std::string& data, size_t n, sgcl::encoding::error* err = nullptr) {
        sgcl::tracked_ptr<json::reader> r = n ? sgcl::make_tracked<json::reader>(sgcl::make_tracked<dribble>(data, n)) : sgcl::make_tracked<json::reader>(text(data));
        auto t = all_tokens(*r);
        if (err && r->last_error()) {
            *err = *r->last_error();
        }
        return t;
    }
}

// A token cut anywhere by the end of a block reads as it does whole: every
// file of the suite fed 1, 2, 3 and 7 bytes at a time, and the named cuts
// (an escape \u12|34, a UTF-8 sequence C5|BC, a number 12|.5e|3)
TEST(JsonReader_Tests, TokensInPiecesAreTheTokensWhole) {
    std::vector<std::string> texts = {
        R"({"a\u1234b": [12.5e3, -0.0e-1, true, false, null, "\ud83d\ude00\n", {}], "\u00e9": "x\u0041y"})",
        "[\"\xc5\xbc\xc5\xbc\xf0\x9f\x98\x80\", 123456789012345678901234567890, \"" + std::string(20000, 'x') + "\\n" + std::string(9000, 'y') + "\"]",
        "1 2 [3] {\"a\":4} \"five\" true",
    };
    auto dir = suite_dir();
    if (std::filesystem::exists(dir)) {
        for (auto& f : json_oracle::suite) {
            texts.push_back(read_file(dir / std::string(f.name)));
        }
    }
    for (auto& t : texts) {
        sgcl::encoding::error whole_error;
        auto whole = tokens_in_pieces(t, 0, &whole_error);
        for (size_t n : {1, 2, 3, 7, 4096}) {
            sgcl::encoding::error e;
            auto pieces = tokens_in_pieces(t, n, &e);
            EXPECT_EQ(pieces, whole) << n << " " << t.substr(0, 80);
            if (!whole) {
                EXPECT_EQ(e.code(), whole_error.code()) << n << " " << t.substr(0, 80);
                EXPECT_EQ(e.offset(), whole_error.offset()) << n << " " << t.substr(0, 80);
                EXPECT_EQ(e.line(), whole_error.line()) << n << " " << t.substr(0, 80);
                EXPECT_EQ(e.column(), whole_error.column()) << n << " " << t.substr(0, 80);
            }
        }
    }
}

// Values read whole, from a stream in pieces: NDJSON through more() and
// read(), an array of records one at a time
TEST(JsonReader_Tests, ValuesReadWholeInPieces) {
    std::string ndjson;
    for (int i = 0; i < 300; ++i) {
        // strings with brackets, quotes and backslashes inside: the extent
        // of a value is found past them
        ndjson += "{\"i\": " + std::to_string(i) + ", \"s\": \"" + std::string(size_t(i * 37 % 200), 'z') + "\", \"t\": \"}]\\\"{[\\\\\", \"a\": [1, [2, {\"b\": null}]]}\n";
    }
    for (size_t n : {0, 1, 3, 7, 1000}) {
        sgcl::tracked_ptr<json::reader> r = n ? sgcl::make_tracked<json::reader>(sgcl::make_tracked<dribble>(ndjson, n)) : sgcl::make_tracked<json::reader>(text(ndjson));
        int i = 0;
        while (r->more()) {
            auto v = r->read();
            ASSERT_TRUE(v) << n << " " << i << " " << (r->last_error() ? r->last_error()->message().c_str() : "");
            EXPECT_EQ((*v)["i"].as_int(), i);
            EXPECT_EQ((*v)["s"].as_string()->size(), size_t(i * 37 % 200));
            EXPECT_EQ((*v)["a"][1][1]["b"].is_null(), true);
            EXPECT_EQ((*v)["t"].as_string(), sgcl::string("}]\"{[\\"));
            ++i;
        }
        EXPECT_EQ(i, 300);
        EXPECT_FALSE(r->last_error());
        EXPECT_FALSE(r->read());
        EXPECT_FALSE(r->last_error());
    }
    // an array of records, the brackets by next(), the records by read()
    std::string array = "[";
    for (int i = 0; i < 100; ++i) {
        array += (i ? ", " : "") + std::string("{\"n\": ") + std::to_string(i) + "}";
    }
    array += "]";
    for (size_t n : {0, 1, 2, 7}) {
        sgcl::tracked_ptr<json::reader> r = n ? sgcl::make_tracked<json::reader>(sgcl::make_tracked<dribble>(array, n)) : sgcl::make_tracked<json::reader>(text(array));
        ASSERT_EQ(r->next()->type(), json::token::kind::begin_array);
        int i = 0;
        while (r->more()) {
            auto v = r->read();
            ASSERT_TRUE(v);
            EXPECT_EQ((*v)["n"].as_int(), i++);
        }
        EXPECT_EQ(i, 100);
        EXPECT_EQ(r->next()->type(), json::token::kind::end_array);
        EXPECT_FALSE(r->next());
        EXPECT_FALSE(r->last_error());
    }
}

// The members of an object read as tokens and values: a key by next(),
// its value by read() or skip(); skip() of a key skips the member
TEST(JsonReader_Tests, MembersKeysAndSkips) {
    json::reader r(text(R"({"a": [1, {"x": 2}], "b": {"c": 3}, "d": "e", "f": 4})"));
    EXPECT_EQ(r.next()->type(), json::token::kind::begin_object);
    auto k = r.next();
    EXPECT_EQ(k->type(), json::token::kind::key);
    EXPECT_EQ(k->text().view(), "a");
    EXPECT_TRUE(r.skip());
    EXPECT_EQ(r.next()->text().view(), "b");
    EXPECT_EQ(r.read()->to_string(), "{\"c\":3}");
    EXPECT_TRUE(r.skip());   // "d": "e", key and value
    EXPECT_EQ(r.read()->as_string(), sgcl::string("f"));   // a key read as a value is the key
    EXPECT_EQ(r.read()->as_int(), 4);
    EXPECT_FALSE(r.more());
    EXPECT_EQ(r.next()->type(), json::token::kind::end_object);
    EXPECT_FALSE(r.skip());
    EXPECT_FALSE(r.last_error());
    EXPECT_EQ(r.depth(), 0u);
    // the tokens' values
    json::reader n(text("[12, 1e2, 1.5, true, false, null, 18446744073709551615]"));
    n.next();
    EXPECT_EQ(n.next()->as_int(), 12);
    EXPECT_EQ(n.next()->as_int(), 100);
    auto f = n.next();
    EXPECT_EQ(f->as_int(), std::nullopt);
    EXPECT_EQ(f->as_double(), 1.5);
    EXPECT_EQ(n.next()->as_bool(), true);
    EXPECT_EQ(n.next()->as_bool(), false);
    EXPECT_EQ(n.next()->type(), json::token::kind::null);
    EXPECT_EQ(n.next()->as_uint(), UINT64_MAX);
}

// Where the reader stops, and what it says: the offset, the line and the
// column, counted across the blocks let go
TEST(JsonReader_Tests, ErrorsAndTheirPlaces) {
    struct Case { std::string_view text; errc code; uint64_t offset; uint32_t line; uint32_t column; };
    const Case cases[] = {
        {"[1, 2,]", errc::syntax, 6, 1, 7},
        {"[1 2]", errc::syntax, 3, 1, 4},
        {"{\"a\" 1}", errc::syntax, 5, 1, 6},
        {"{\"a\": 1,}", errc::syntax, 8, 1, 9},
        {"{1: 2}", errc::syntax, 1, 1, 2},
        {"[tru]", errc::syntax, 4, 1, 5},
        {"[nul", errc::unexpected_end, 4, 1, 5},
        {"[-]", errc::syntax, 2, 1, 3},
        {"[01]", errc::syntax, 2, 1, 3},
        {"[1.]", errc::syntax, 3, 1, 4},
        {"[1e+]", errc::syntax, 4, 1, 5},
        {"\n\n  [\"ż\", x]", errc::syntax, 11, 3, 9},
        {"[\"a\",\n \"b\"\n", errc::unexpected_end, 11, 3, 1},
        {"{\"a\": 1, \"a\": 2}", errc::duplicate_key, 9, 1, 10},
        {"\xEF\xBB\xBF{}", errc::syntax, 0, 1, 1},
        {"]", errc::syntax, 0, 1, 1},
        {"[}", errc::syntax, 1, 1, 2},
        {"{]", errc::syntax, 1, 1, 2},
    };
    for (auto& c : cases) {
        for (size_t n : {0, 1, 2, 5}) {
            sgcl::encoding::error e;
            auto t = tokens_in_pieces(std::string(c.text), n, &e);
            ASSERT_FALSE(t) << c.text;
            EXPECT_EQ(e.code(), c.code) << c.text << " " << n;
            EXPECT_EQ(e.offset(), c.offset) << c.text << " " << n;
            EXPECT_EQ(e.line(), c.line) << c.text << " " << n;
            EXPECT_EQ(e.column(), c.column) << c.text << " " << n;
        }
        auto p = json::parse(text(c.text));
        ASSERT_FALSE(p) << c.text;
        EXPECT_EQ(p.error().code(), c.code) << c.text;
        EXPECT_EQ(p.error().offset(), c.offset) << c.text;
        EXPECT_EQ(p.error().line(), c.line) << c.text;
        EXPECT_EQ(p.error().column(), c.column) << c.text;
    }
    // the message
    auto e = json::parse(text("{\"a\": [1, 2,]}")).error();
    EXPECT_EQ(e.message(), "1:13: invalid character ']' where a value was expected");
    // an error is kept: every call after it says nothing more
    json::reader r(text("[1, x, 2]"));
    EXPECT_TRUE(r.next());
    EXPECT_TRUE(r.next());
    EXPECT_FALSE(r.next());
    EXPECT_FALSE(r.next());
    EXPECT_FALSE(r.read());
    EXPECT_FALSE(r.skip());
    EXPECT_FALSE(r.more());
    EXPECT_EQ(r.last_error()->offset(), 4u);
}

// Nesting as deep as max_depth is taken, one more is depth_limit, by the
// reader, by read() and by parse
TEST(JsonReader_Tests, DepthLimit) {
    for (uint32_t depth : {1u, 10u, 512u}) {
        json::options o;
        o.max_depth = depth;
        std::string ok = std::string(depth, '[') + std::string(depth, ']');
        std::string deep = std::string(depth + 1, '[') + std::string(depth + 1, ']');
        EXPECT_TRUE(json::parse(text(ok), o)) << depth;
        auto e = json::parse(text(deep), o);
        ASSERT_FALSE(e);
        EXPECT_EQ(e.error().code(), errc::depth_limit);
        EXPECT_EQ(e.error().offset(), depth);
        json::reader r1(text(deep), o);
        EXPECT_FALSE(all_tokens(r1));
        EXPECT_EQ(r1.last_error()->code(), errc::depth_limit);
        json::reader r2(text(ok), o);
        EXPECT_TRUE(r2.read());
        json::reader r3(text("[" + deep + "]"), o);
        r3.next();
        EXPECT_FALSE(r3.read());
        EXPECT_EQ(r3.last_error()->code(), errc::depth_limit);
        EXPECT_EQ(r3.last_error()->offset(), depth);
    }
    // the default is 512
    EXPECT_TRUE(json::parse(text(std::string(512, '[') + std::string(512, ']'))));
    EXPECT_EQ(json::parse(text(std::string(513, '[') + std::string(513, ']'))).error().code(), errc::depth_limit);
    // a million brackets fail at 513 without a stack of calls
    EXPECT_EQ(json::parse(text(std::string(1000000, '['))).error().offset(), 512u);
}

TEST(JsonReader_Tests, EmptyAndTrailing) {
    EXPECT_EQ(json::parse(text("")).error().code(), errc::unexpected_end);
    EXPECT_EQ(json::parse(text("  \n ")).error().code(), errc::unexpected_end);
    EXPECT_EQ(json::parse(text("{} x")).error().code(), errc::syntax);
    EXPECT_EQ(json::parse(text("{} x")).error().offset(), 3u);
    EXPECT_EQ(json::parse(text("1 2")).error().offset(), 2u);
    EXPECT_TRUE(json::parse(text(" \t\r\n1 \t\r\n")));
    json::reader r(text("  "));
    EXPECT_FALSE(r.next());
    EXPECT_FALSE(r.last_error());
    EXPECT_FALSE(r.more());
}

TEST(JsonReader_Tests, DuplicateKeys) {
    EXPECT_EQ(json::parse(text(R"({"a": 1, "b": {"a": 2}, "c": 3})")).value()["b"]["a"].as_int(), 2);   // not the same object
    auto e = json::parse(text(R"({"a": 1, "\u0061": 2})"));
    EXPECT_EQ(e.error().code(), errc::duplicate_key);
    EXPECT_EQ(e.error().offset(), 9u);
    json::options allow;
    allow.allow_duplicate_keys = true;
    auto v = json::parse(text(R"({"a": 1, "b": 2, "a": 3})"), allow).value();
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v["a"].as_int(), 3);
    EXPECT_EQ(v.to_string(), R"({"b":2,"a":3})");
    // a large object: a key given twice among a thousand, found by hash
    std::string big = "{";
    for (int i = 0; i < 1000; ++i) {
        big += "\"k" + std::to_string(i) + "\": " + std::to_string(i) + ",";
    }
    std::string ok = big + "\"z\": 0}";
    std::string dup = big + "\"k500\": 0}";
    EXPECT_TRUE(json::parse(text(ok)));
    EXPECT_EQ(json::parse(text(dup)).error().code(), errc::duplicate_key);
    EXPECT_EQ(json::parse(text(dup)).error().offset(), big.size());
    json::reader r(text(dup));
    EXPECT_FALSE(all_tokens(r));
    EXPECT_EQ(r.last_error()->code(), errc::duplicate_key);
    EXPECT_EQ(r.last_error()->offset(), big.size());
    json::reader ra(text(dup), allow);
    EXPECT_TRUE(all_tokens(ra));
    auto large = json::parse(text(dup), allow).value();
    EXPECT_EQ(large.size(), 1000u);
    EXPECT_EQ(large["k500"].as_int(), 0);
    EXPECT_EQ(large["k999"].as_int(), 999);
}

// A value parsed from a stream: the whole stream is one value
TEST(JsonReader_Tests, ParseOfAStream) {
    for (size_t n : {1, 3, 1000}) {
        auto v = json::parse(sgcl::make_tracked<dribble>(std::string(R"( {"a": [1, 2.5, "x"]} )"), n));
        ASSERT_TRUE(v);
        EXPECT_EQ(v->to_string(), R"({"a":[1,2.5,"x"]})");
        auto two = json::parse(sgcl::make_tracked<dribble>(std::string("1 2"), n));
        EXPECT_FALSE(two);
        auto none = json::parse(sgcl::make_tracked<dribble>(std::string("  "), n));
        EXPECT_EQ(none.error().code(), errc::unexpected_end);
    }
    auto failed = json::parse(sgcl::make_tracked<failing>(std::string("[1, 2")));
    ASSERT_FALSE(failed);
    EXPECT_EQ(failed.error().code(), errc::io);
    EXPECT_TRUE(failed.error().io_error());
}

// The operations in a task: the same tokens, values and errors
TEST(JsonReader_Tests, AsyncForms) {
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        std::string data = R"([{"a": 1}, "x\u0041", 3.5, [true, null]] {"b": 2})";
        json::reader r(sgcl::make_tracked<dribble>(data, 2));
        std::string tokens;
        auto first = co_await r.async_next();
        if (!first || first->type() != json::token::kind::begin_array) {
            co_return -1;
        }
        auto v = co_await r.async_read();
        if (!v || v->to_string() != "{\"a\":1}") {
            co_return -2;
        }
        if (!(co_await r.async_more())) {
            co_return -3;
        }
        auto s = co_await r.async_next();
        if (!s || s->text().view() != "xA") {
            co_return -4;
        }
        if (!(co_await r.async_skip()) || !(co_await r.async_skip())) {
            co_return -5;
        }
        if (co_await r.async_more()) {
            co_return -6;
        }
        co_await r.async_next();
        auto b = co_await r.async_read();
        if (!b || (*b)["b"].as_int() != 2) {
            co_return -7;
        }
        auto parsed = co_await json::async_parse(sgcl::make_tracked<dribble>(std::string("[1, 2, 3]"), 1));
        if (!parsed || parsed->size() != 3) {
            co_return -8;
        }
        json::reader bad(sgcl::make_tracked<dribble>(std::string("[1, 2, x]"), 1));
        while (co_await bad.async_next()) {
        }
        if (!bad.last_error() || bad.last_error()->offset() != 7) {
            co_return -9;
        }
        co_return 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

// The block and the decoded text survive a collection between the calls
TEST(JsonReader_Tests, SurvivesACollection) {
    std::string data = "[";
    for (int i = 0; i < 2000; ++i) {
        data += (i ? "," : "") + std::string("{\"k\\u0041\": \"v") + std::to_string(i) + "\\n\"}";
    }
    data += "]";
    json::reader r(sgcl::make_tracked<dribble>(data, 333));
    r.next();
    int i = 0;
    while (r.more()) {
        if (i % 100 == 0) {
            sgcl::collector::force_collect(true);
        }
        auto v = r.read();
        ASSERT_TRUE(v);
        EXPECT_EQ((*v)["kA"].as_string(), sgcl::string("v" + std::to_string(i) + "\n"));
        ++i;
    }
    EXPECT_EQ(i, 2000);
}

// --- the writer ---

TEST(JsonWriter_Tests, CompactAndPretty) {
    sgcl::tracked_ptr out = sgcl::make_tracked<sink>();
    json::writer w(out);
    w.begin_object().key("a").value(1).key("b").begin_array().value(true).value(nullptr).value("x").value(2.5).value(0.1f).end_array().key("c").begin_object().end_object().end_object();
    w.value(json::parse(text("[1, {}]")).value());
    EXPECT_TRUE(w.flush());
    EXPECT_EQ(out->text, "{\"a\":1,\"b\":[true,null,\"x\",2.5,0.1],\"c\":{}}\n[1,{}]\n");
    sgcl::tracked_ptr pretty = sgcl::make_tracked<sink>();
    json::writer p(pretty, json::pretty);
    p.begin_object().key("a").value(1).key("b").begin_array().value(1).begin_array().end_array().end_array().end_object();
    EXPECT_TRUE(p.flush());
    EXPECT_EQ(pretty->text, "{\n  \"a\": 1,\n  \"b\": [\n    1,\n    []\n  ]\n}\n");
    // what was flushed is not written again
    p.value(3);
    EXPECT_TRUE(p.flush());
    EXPECT_EQ(pretty->text, "{\n  \"a\": 1,\n  \"b\": [\n    1,\n    []\n  ]\n}\n3\n");
}

// A mistake in the structure is kept and reported by flush
TEST(JsonWriter_Tests, MistakesInTheStructure) {
    auto mistake = [](auto build) {
        sgcl::tracked_ptr out = sgcl::make_tracked<sink>();
        json::writer w(out);
        build(w);
        auto r = w.flush();
        return r ? std::error_code() : r.error().code();
    };
    using sgcl::encoding::make_error_code;
    EXPECT_EQ(mistake([](json::writer& w) { w.end_array(); }), make_error_code(errc::syntax));
    EXPECT_EQ(mistake([](json::writer& w) { w.begin_array().end_object(); }), make_error_code(errc::syntax));
    EXPECT_EQ(mistake([](json::writer& w) { w.key("a"); }), make_error_code(errc::syntax));
    EXPECT_EQ(mistake([](json::writer& w) { w.begin_object().value(1); }), make_error_code(errc::syntax));
    EXPECT_EQ(mistake([](json::writer& w) { w.begin_object().key("a").key("b"); }), make_error_code(errc::syntax));
    EXPECT_EQ(mistake([](json::writer& w) { w.begin_object().key("a").end_object(); }), make_error_code(errc::syntax));
    EXPECT_EQ(mistake([](json::writer& w) { w.value(std::nan("")); }), make_error_code(errc::unsupported_value));
    EXPECT_EQ(mistake([](json::writer& w) { w.value(-INFINITY); }), make_error_code(errc::unsupported_value));
    EXPECT_EQ(mistake([](json::writer& w) { w.begin_array().value(1).end_array(); }), std::error_code());
    // the stream's failure, kept
    sgcl::tracked_ptr b = sgcl::make_tracked<broken>();
    json::writer w(b);
    w.value(1);
    auto r = w.flush();
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), std::make_error_code(std::errc::broken_pipe));
    EXPECT_FALSE(w.flush());
}

TEST(JsonWriter_Tests, AsyncFlush) {
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        sgcl::tracked_ptr out = sgcl::make_tracked<sink>();
        json::writer w(out, json::pretty);
        w.begin_array().value(1).value("a").end_array();
        auto r = co_await w.async_flush();
        co_return r && out->text == "[\n  1,\n  \"a\"\n]\n" ? 1 : 0;
    }());
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

// What a reader of a stream holds at once is bounded: a token or a value
// longer than options.max_token_size is errc::out_of_range, where the
// memory would have grown with whatever the network sent
TEST(JsonReader_Tests, ATokenPastTheBoundIsAnError) {
    std::string text = "[\"" + std::string(100000, 'a') + "\", 1]";
    sgcl::tracked_ptr src = sgcl::make_tracked<sgcl::io::buffer>(sgcl::string(text));
    sgcl::encoding::json::options o;
    o.max_token_size = 20000;
    sgcl::encoding::json::reader r(src, o);
    while (r.next()) {
    }
    ASSERT_TRUE(r.last_error());
    EXPECT_EQ(r.last_error()->code(), sgcl::encoding::errc::out_of_range);
    sgcl::tracked_ptr again = sgcl::make_tracked<sgcl::io::buffer>(sgcl::string(text));
    sgcl::encoding::json::reader whole(again);                      // the default, 64 MB, takes it
    size_t n = 0;
    while (whole.next()) {
        ++n;
    }
    EXPECT_FALSE(whole.last_error());
    EXPECT_EQ(n, 4u);
}
