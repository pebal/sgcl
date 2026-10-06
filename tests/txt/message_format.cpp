//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::message_format against ICU 78's MessageFormat: the messages of
// tests/txt/message_vectors.h (tools/message_vectors.py), answered as ICU
// answers them, refused where ICU refuses them; then the boundaries.
#include "tests/types.h"
#include "tests/txt/message_args.h"
#include "tests/txt/message_vectors.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {
    string s(const char* t) {
        return string(t);
    }

    txt::locale loc(const char* tag) {
        return txt::locale(string(tag));
    }

    string fmt(const char* tag, const char* pattern, const txt::value& args) {
        auto r = txt::format_message(string(pattern), loc(tag), args);
        return r ? *r : string("ERROR");
    }
}

TEST(MessageFormat_Tests, IcuVectors) {
    size_t bad = 0;
    for (const auto& v : MessageVectors) {
        auto r = txt::format_message(s(v.pattern), loc(v.locale), message_args::parse(v.args));
        bool ok = v.answer ? (r && *r == s(v.answer)) : !r;
        if (!ok && ++bad <= 20) {
            ADD_FAILURE() << v.locale << " | " << v.pattern << " | " << v.args << "\n  got "
                          << (r ? std::string(r->view()) : "ERROR " + std::string(r.error().message().view()))
                          << "\n  want " << (v.answer ? v.answer : "ERROR");
        }
    }
    EXPECT_EQ(bad, 0u) << "of " << std::size(MessageVectors);
}

TEST(MessageFormat_Tests, Basics) {
    txt::message_format m(s("{n, plural, one {# plik} few {# pliki} other {# plików}} w {dir}"), loc("pl"));
    EXPECT_EQ(m.format(txt::object{{"n", 1}, {"dir", "src"}}), s("1 plik w src"));
    EXPECT_EQ(m.format(txt::object{{"n", 3}, {"dir", "src"}}), s("3 pliki w src"));
    EXPECT_EQ(m.format(txt::object{{"n", 5}, {"dir", "src"}}), s("5 plików w src"));
    EXPECT_EQ(m.format(txt::object{{"n", 1.5}, {"dir", "src"}}), s("1,5 plików w src"));
    EXPECT_EQ(m.where(), loc("pl"));
    EXPECT_EQ(m.pattern(), s("{n, plural, one {# plik} few {# pliki} other {# plików}} w {dir}"));
    // by position
    EXPECT_EQ(fmt("en", "{1} before {0}", txt::list{"a", 2}), s("2 before a"));
    // a missing argument: {name}, as ICU writes it
    EXPECT_EQ(fmt("en", "{who} has {n, plural, one {# file} other {# files}}", txt::object{}), s("{who} has {n}"));
    EXPECT_EQ(fmt("en", "{0}", txt::list{}), s("{0}"));
    // a value of another type is written as its text
    EXPECT_EQ(fmt("en", "{n, number}", txt::object{{"n", "abc"}}), s("abc"));
    EXPECT_EQ(fmt("en", "{b}", txt::object{{"b", true}}), s("true"));
    EXPECT_EQ(fmt("en", "{s, select, a {A} other {O}}", txt::object{{"s", 5}}), s("O"));
    EXPECT_EQ(fmt("en", "{n, plural, one {one} other {#}}", txt::object{{"n", "1"}}), s("one"));
    // # belongs to the nearest plural, not to a select inside it
    EXPECT_EQ(fmt("en", "# {n, plural, other {{g, select, other {#}} #}}", txt::object{{"n", 2}, {"g", "x"}}),
              s("# # 2"));
    // apostrophes
    EXPECT_EQ(fmt("en", "it's '{'x'}' '' '#' {n, plural, other {'#' #}}", txt::object{{"n", 3}}),
              s("it's {x} ' '#' # 3"));
    EXPECT_EQ(fmt("en", "open 'quote", txt::object{}), s("open 'quote"));
    EXPECT_EQ(fmt("en", "a '{b", txt::object{}), s("a {b"));
    EXPECT_EQ(fmt("en", "} alone", txt::object{}), s("} alone"));
    // numbers
    EXPECT_EQ(fmt("en", "{n, number, integer}", txt::object{{"n", 2.5}}), s("2"));
    EXPECT_EQ(fmt("en", "{n, number, percent}", txt::object{{"n", 0.256}}), s("26%"));
    EXPECT_EQ(fmt("en", "{n, number, ::percent}", txt::object{{"n", 25}}), s("25%"));
    EXPECT_EQ(fmt("en", "{n, number, ::currency/EUR}", txt::object{{"n", -1234.5}}), s("-\xE2\x82\xAC" "1,234.50"));
    EXPECT_EQ(fmt("en", "[{n, number, #,##0.00}]", txt::object{{"n", 1234.5}}), s("[ 1,234.50]"));
    EXPECT_EQ(fmt("en", "{n, number,#,##,##0}", txt::object{{"n", 12345678}}), s("1,23,45,678"));
    EXPECT_EQ(fmt("en", "{n, number,0.00E+00}", txt::object{{"n", 0.00012345}}), s("1.23E-04"));
    EXPECT_EQ(fmt("en", "{n, number,##0.##E0}", txt::object{{"n", 12345}}), s("12.3E3"));
    EXPECT_EQ(fmt("en", "{n, number,#.00;(#)}", txt::object{{"n", -0.5}}), s("(.50)"));
    EXPECT_EQ(fmt("en", "{n, number,'#'0%}", txt::object{{"n", 0.5}}), s("#50%"));
    // the empty message, and the default one
    EXPECT_EQ(fmt("en", "", txt::object{}), s(""));
    txt::message_format empty;
    EXPECT_EQ(empty.format(txt::object{{"a", 1}}), s(""));
    EXPECT_EQ(empty.where(), txt::locale());
    EXPECT_EQ(empty.pattern(), s(""));
}

TEST(MessageFormat_Tests, Dates) {
    // milliseconds since 1970 in UTC, or RFC 3339 text with its offset
    EXPECT_EQ(fmt("en", "{d, date, ::yMMMd} {d, time, short}", txt::object{{"d", 1700000000000ll}}),
              s("Nov 14, 2023 10:13\xE2\x80\xAFPM"));
    EXPECT_EQ(fmt("en", "{d, date, ::yMMMd} {d, time, HH:mm xxx}", txt::object{{"d", "2026-10-06T14:05:00+02:00"}}),
              s("Oct 6, 2026  14:05 +02:00"));
    EXPECT_EQ(fmt("pl", "{d, date, long}", txt::object{{"d", 0}}), s("1 stycznia 1970"));
    // a value that is not an instant: its text
    EXPECT_EQ(fmt("en", "{d, date}", txt::object{{"d", "yesterday"}}), s("yesterday"));
}

TEST(MessageFormat_Tests, Errors) {
    struct Bad {
        const char* pattern;
        size_t offset;
    };
    const Bad cases[] = {
        {"{", 1},
        {"ab {n", 3},
        {"{n,}", 3},
        {"{n, foo}", 4},
        {"{n, spellout}", 4},
        {"{n, choice, 0#a}", 4},
        {"{n, plural, one {x}}", 19},
        {"{n, select}", 10},
        {"{01}", 1},
        {"{n, number, ::bogus}", 4},
        {"{n, number, #,##0.0E0}", 4},
        {"{n, plural, offset:1}", 20},
        {"{n, plural, =x {a} other {b}}", 13},
        {"{n, plural, one {a} offset:1 other {b}}", 20},
        {"{n, plural, other {a}", 21},
        {"{n, select, other a}", 18},
        {"{a b}", 3},
        {", {, number}", 3},
        {"{2147483648}", 1},
    };
    for (const auto& b : cases) {
        auto m = txt::message_format::parse(s(b.pattern), loc("en"));
        ASSERT_FALSE(m.has_value()) << b.pattern;
        EXPECT_EQ(m.error().offset(), b.offset) << b.pattern << ": " << m.error().message();
        EXPECT_FALSE(m.error().message().empty());
    }
    EXPECT_THROW(txt::message_format(s("{n, plural}"), loc("en")), bad_expected_access<txt::message_error>);
    // nesting deeper than 64 messages
    std::string deep;
    for (int i = 0; i < 70; ++i) {
        deep += "{s, select, other {";
    }
    deep += "x";
    for (int i = 0; i < 70; ++i) {
        deep += "}}";
    }
    EXPECT_FALSE(txt::message_format::parse(string(std::string_view(deep)), loc("en")).has_value());
    std::string fine;
    for (int i = 0; i < 20; ++i) {
        fine += "{s, select, other {";
    }
    fine += "x";
    for (int i = 0; i < 20; ++i) {
        fine += "}}";
    }
    EXPECT_EQ(fmt("en", fine.c_str(), txt::object{{"s", "a"}}), s("x"));
}

TEST(MessageFormat_Tests, SharedAcrossThreads) {
    txt::message_format m(s("{n, plural, one {# file} other {# files}}"), loc("en"));
    std::atomic<size_t> wrong{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&m, &wrong] {
            for (int i = 0; i < 1000; ++i) {
                string want = i == 1 ? string("1 file") : string(std::to_string(i) + " files");
                wrong += m.format(txt::object{{"n", i}}) != want;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(wrong.load(), 0u);
}
