//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of CSV (DESIGN 408): a default and a moved-from row, a
// header of names given twice, a record of no fields, the bound of a
// record read as a type, the writer's options and its stream failing, the
// numbers of a type at their limits and the words the writer writes for
// NaN and the infinities read back. What csv.cpp covers is not repeated:
// the texts of Go's reader (empty texts and lines, quotes cut by the end,
// a '\r' at the end: Csv_Tests.ReadsAsGoReads), the bound of a record
// read as a row at its exact length (ARecordPastTheBoundInOneBlockIsAnError),
// the separators refused (HeaderAndRows), the files that do not open
// (EncodingFiles_Tests.ErrorsOutsideTheTextHaveNoPlace).
#include "common.h"

#include <cmath>
#include <limits>

using namespace enc_test;
using sgcl::encoding::csv;
using sgcl::encoding::errc;
using sgcl::encoding::field_list;
using sgcl::string;

namespace {
    struct named {
        string name;
        void describe(field_list& f) {
            f.add("name", name);
        }
    };

    struct numbers {
        int8_t i8 = 0;
        uint8_t u8 = 0;
        int64_t i64 = 0;
        uint64_t u64 = 0;
        double d = 0;
        float f = 0;
        void describe(field_list& l) {
            l.add("i8", i8);
            l.add("u8", u8);
            l.add("i64", i64);
            l.add("u64", u64);
            l.add("d", d);
            l.add("f", f);
        }
    };
}

// A row made by no reader: no fields, no line, no header; one moved from
// is still the row (its text is a string's word, which a move copies)
TEST(CsvBoundaries_Tests, ADefaultAndAMovedFromRow) {
    csv::row r;
    EXPECT_EQ(r.size(), 0u);
    EXPECT_TRUE(r.empty());
    EXPECT_EQ(r.line(), 0u);
    EXPECT_EQ(r.position(0), (sgcl::pair<uint32_t, uint32_t>(0, 0)));
    EXPECT_THROW((void)r.at(0), std::out_of_range);
    EXPECT_FALSE(r["a"]);
    EXPECT_EQ(r.get("a", "?"), "?");
    EXPECT_TRUE(r.begin() == r.end());
    csv::reader reader(string("a,b\n1,2\n"));
    reader.read_header();
    auto row = *reader.next();
    csv::row copy = row;
    auto& same = copy;
    copy = same;
    EXPECT_EQ(copy.get("b", "?"), "2");
    csv::row moved = std::move(copy);
    EXPECT_EQ(moved.get("b", "?"), "2");
    EXPECT_EQ(copy.size(), 2u);
    EXPECT_EQ(string(copy[0]), "1");
}

// A header with a name given twice: the first column is the name's; a row
// shorter than the header has nothing in the columns past it; an empty name
// is a name
TEST(CsvBoundaries_Tests, HeaderNamesAndShortRows) {
    csv::options o;
    o.same_field_count = false;
    csv::reader r(string("a,a,,b\n1,2,3\n"), o);
    auto h = r.read_header();
    ASSERT_TRUE(h);
    EXPECT_EQ(r.header().size(), 4u);
    auto row = r.next();
    ASSERT_TRUE(row);
    EXPECT_EQ(string(*(*row)["a"]), "1");
    EXPECT_EQ(string(*(*row)[""]), "3");
    EXPECT_FALSE((*row)["b"]);
    EXPECT_EQ(row->get("b", "none"), "none");
    // no records at all: no header, no row, no error
    csv::reader empty(string(""));
    EXPECT_FALSE(empty.read_header());
    EXPECT_TRUE(empty.header().empty());
    EXPECT_FALSE(empty.read<named>());
    EXPECT_FALSE(empty.last_error());
    // a header and nothing after it
    csv::reader only(string("name\n"));
    EXPECT_FALSE(only.read<named>());
    EXPECT_FALSE(only.last_error());
    EXPECT_EQ(only.header().size(), 1u);
}

// A record read as a type is held to max_record_size as a row is, also
// when it came whole in one block (a row's bound: csv.cpp)
TEST(CsvBoundaries_Tests, ARecordReadAsATypeIsBounded) {
    std::string text = "name\n" + std::string(31, 'x') + "\n" + std::string(32, 'y') + "\n";
    for (size_t piece : {size_t(1), size_t(7), size_t(4096)}) {
        csv::options o;
        o.max_record_size = 31;
        csv::reader r(make_tracked<dribble>(text, piece), o);
        auto exact = r.read<named>();
        ASSERT_TRUE(exact) << piece;
        EXPECT_EQ(exact->name.size(), 31u);
        EXPECT_FALSE(r.read<named>()) << piece;
        ASSERT_TRUE(r.last_error()) << piece;
        EXPECT_EQ(r.last_error()->code(), errc::out_of_range) << piece;
        EXPECT_EQ(r.last_error()->line(), 3u) << piece;
    }
    // a bound of 0: a stream of empty lines is no record; any field is past it
    csv::options zero;
    zero.max_record_size = 0;
    csv::reader blank(make_tracked<dribble>(std::string("\n\n"), 1), zero);
    EXPECT_FALSE(blank.next());
    EXPECT_FALSE(blank.last_error());
    csv::reader one(make_tracked<dribble>(std::string("x\n"), 1), zero);
    EXPECT_FALSE(one.next());
    ASSERT_TRUE(one.last_error());
    EXPECT_EQ(one.last_error()->code(), errc::out_of_range);
}

// A stream failing half-way: the records before it, then its error with
// the place reached; every call after it gives nothing
TEST(CsvBoundaries_Tests, AStreamFailingHalfWay) {
    csv::reader r(make_tracked<failing>("a,b\nc,"));
    auto first = r.next();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->size(), 2u);
    EXPECT_FALSE(r.next());
    ASSERT_TRUE(r.last_error());
    EXPECT_EQ(r.last_error()->code(), errc::io);
    EXPECT_EQ(r.last_error()->offset(), 6u);
    ASSERT_TRUE(r.last_error()->io_error());
    EXPECT_EQ(r.last_error()->io_error()->path(), "failing");
    EXPECT_FALSE(r.next());
    EXPECT_FALSE(r.read<named>());
}

// The writer is given the options the reader is: a comment character the
// reader refuses is refused, and a record whose first field starts with the
// comment character is quoted, so that the reader with the same options
// reads it and does not pass it over as a comment
TEST(CsvBoundaries_Tests, TheWritersCommentCharacter) {
    for (char bad : {',', '"', '\n', '\r', char(0x80)}) {
        csv::options o;
        o.comment = bad;
        EXPECT_THROW(csv::writer(make_tracked<sink>(), o), std::invalid_argument) << int(bad);
    }
    csv::options o;
    o.comment = '#';
    sgcl::tracked_ptr out = make_tracked<sink>();
    csv::writer w(out, o);
    w.write({"#not a comment", "x"}).write({"a", "#b"}).write({"#"});
    ASSERT_TRUE(w.flush());
    EXPECT_EQ(out->text, "\"#not a comment\",x\na,#b\n\"#\"\n");
    csv::options read = o;
    read.same_field_count = false;
    csv::reader r(string(out->text), read);
    auto a = r.next();
    ASSERT_TRUE(a);
    EXPECT_EQ(string((*a)[0]), "#not a comment");
    ASSERT_TRUE(r.next());
    auto c = r.next();
    ASSERT_TRUE(c);
    EXPECT_EQ(string((*c)[0]), "#");
    EXPECT_FALSE(r.next());
    EXPECT_FALSE(r.last_error());
    // without a comment character nothing is quoted for it
    sgcl::tracked_ptr plain = make_tracked<sink>();
    csv::writer p(plain);
    p.write({"#a"});
    ASSERT_TRUE(p.flush());
    EXPECT_EQ(plain->text, "#a\n");
}

// A record of no fields is an empty line, which every reader passes over;
// one of an empty field is written "" (Csv_Tests.ARecordOfOneEmptyFieldSurvives)
TEST(CsvBoundaries_Tests, ARecordOfNoFields) {
    sgcl::tracked_ptr out = make_tracked<sink>();
    csv::writer w(out);
    w.write(std::vector<std::string>{});
    ASSERT_TRUE(w.flush());
    EXPECT_EQ(out->text, "\n");
    // flushed with nothing gathered: nothing written
    out->writes = 0;
    ASSERT_TRUE(w.flush());
    EXPECT_EQ(out->writes, 0u);
}

// Once the stream failed, the writer gathers nothing more: what it is
// given after is dropped, not held in memory for a flush that never comes
TEST(CsvBoundaries_Tests, AWriterAfterItsStreamFailed) {
    sgcl::tracked_ptr out = make_tracked<flaky>();
    csv::writer w(out);
    w.write({"a", "b"});
    out->fail_next = 1;
    auto f = w.flush();
    ASSERT_FALSE(f);
    EXPECT_EQ(f.error().path(), "flaky");
    w.write({"c", "d"});
    w.write(named{"e"});
    auto again = w.flush();   // the stream works again; the writer keeps its failure
    ASSERT_FALSE(again);
    EXPECT_EQ(again.error().path(), "flaky");
    EXPECT_EQ(out->text, "");
    // the same in a task
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        sgcl::tracked_ptr out = make_tracked<flaky>();
        csv::writer w(out);
        w.write({"a"});
        out->fail_next = 1;
        if (co_await w.async_flush()) {
            co_return -1;
        }
        w.write({"b"});
        if (co_await w.async_flush()) {
            co_return -2;
        }
        co_return out->text.empty() ? 1 : -3;
    }());
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

// The numbers of a type at the limits of their types, and the words the
// writer writes for NaN and the infinities, which the reader reads back
// as strconv.ParseFloat reads them
TEST(CsvBoundaries_Tests, NumbersAtTheirLimits) {
    csv::reader r(string("i8,u8,i64,u64,d,f\n"
                         "-128,255,-9223372036854775808,18446744073709551615,1.7976931348623157e308,3.4028235e38\n"
                         "128,0,0,0,0,0\n"));
    auto a = r.read<numbers>();
    ASSERT_TRUE(a) << r.last_error()->message();
    EXPECT_EQ(a->i8, INT8_MIN);
    EXPECT_EQ(a->u8, UINT8_MAX);
    EXPECT_EQ(a->i64, INT64_MIN);
    EXPECT_EQ(a->u64, UINT64_MAX);
    EXPECT_EQ(a->d, std::numeric_limits<double>::max());
    EXPECT_EQ(a->f, std::numeric_limits<float>::max());
    EXPECT_FALSE(r.read<numbers>());
    EXPECT_EQ(r.last_error()->code(), errc::out_of_range);
    EXPECT_EQ(r.last_error()->path(), "/i8");
    for (auto [text, path] : {std::pair{"0,-1,0,0,0,0", "/u8"}, std::pair{"0,0,9223372036854775808,0,0,0", "/i64"},
                              std::pair{"0,0,0,18446744073709551616,0,0", "/u64"}, std::pair{"0,0,0,0,1e309,0", "/d"},
                              std::pair{"0,0,0,0,0,3.5e38", "/f"}}) {
        csv::reader past(string("i8,u8,i64,u64,d,f\n" + std::string(text) + "\n"));
        EXPECT_FALSE(past.read<numbers>()) << text;
        ASSERT_TRUE(past.last_error()) << text;
        EXPECT_EQ(past.last_error()->code(), errc::out_of_range) << text;
        EXPECT_EQ(past.last_error()->path(), path) << text;
    }
    // NaN and the infinities: written as strconv writes them, read back
    sgcl::tracked_ptr out = make_tracked<sink>();
    csv::writer w(out);
    numbers n;
    n.d = NAN;
    n.f = -INFINITY;
    w.write(n);
    n.d = INFINITY;
    n.f = INFINITY;
    w.write(n);
    ASSERT_TRUE(w.flush());
    EXPECT_EQ(out->text, "i8,u8,i64,u64,d,f\n0,0,0,0,NaN,-Inf\n0,0,0,0,+Inf,+Inf\n");
    csv::reader back{string(out->text)};
    auto x = back.read<numbers>();
    ASSERT_TRUE(x) << back.last_error()->message();
    EXPECT_TRUE(std::isnan(x->d));
    EXPECT_EQ(x->f, -INFINITY);
    auto y = back.read<numbers>();
    ASSERT_TRUE(y) << back.last_error()->message();
    EXPECT_EQ(y->d, INFINITY);
    EXPECT_EQ(y->f, INFINITY);
    // strconv's other spellings; a sign before NaN is not one
    csv::reader words(string("d,f\ninf,Infinity\n-INFINITY,nan\n+nan,0\n"));
    auto p = words.read<numbers>();
    ASSERT_TRUE(p) << words.last_error()->message();
    EXPECT_EQ(p->d, INFINITY);
    EXPECT_EQ(p->f, INFINITY);
    auto q = words.read<numbers>();
    ASSERT_TRUE(q) << words.last_error()->message();
    EXPECT_EQ(q->d, -INFINITY);
    EXPECT_TRUE(std::isnan(q->f));
    EXPECT_FALSE(words.read<numbers>());
    EXPECT_EQ(words.last_error()->code(), errc::type_mismatch);
    // an integer field takes none of them
    csv::reader no(string("i64\nNaN\n"));
    EXPECT_FALSE(no.read<numbers>());
    EXPECT_EQ(no.last_error()->code(), errc::type_mismatch);
}

namespace {
    struct listed {
        sgcl::vector<int> list;
        void describe(field_list& f) {
            f.add("list", list);
        }
    };

    std::vector<std::vector<std::string>> texts_of(const sgcl::vector<csv::row>& rows) {
        std::vector<std::vector<std::string>> out;
        for (auto& r : rows) {
            out.emplace_back();
            for (auto f : r) {
                out.back().emplace_back(f.data(), f.size());
            }
        }
        return out;
    }
}

// parse and stringify, the forms of load and save over a text: no text,
// empty lines alone, one field, no end after the last record, the first
// record taken as a record and not as a header, the options and the
// options refused, an error after records (they are dropped), a header
// alone; stringify of nothing, of rows, of ranges of texts, of values with
// their header, a field it has no text for, the options, and the way back
TEST(CsvBoundaries_Tests, ParseAndStringify) {
    using table = std::vector<std::vector<std::string>>;
    EXPECT_TRUE(value_of(csv::parse("")).empty());
    EXPECT_TRUE(value_of(csv::parse("\n\r\n\n")).empty());
    EXPECT_EQ(texts_of(value_of(csv::parse("a"))), (table{{"a"}}));
    EXPECT_EQ(texts_of(value_of(csv::parse("\"\"\n"))), (table{{""}}));
    auto rows = value_of(csv::parse("a,b\n1,\"x,y\"\r\n2,3"));
    EXPECT_EQ(texts_of(rows), (table{{"a", "b"}, {"1", "x,y"}, {"2", "3"}}));
    EXPECT_FALSE(rows[1]["a"]);   // no header taken
    EXPECT_EQ(rows[2].line(), 3u);
    csv::options semi;
    semi.separator = ';';
    semi.comment = '#';
    EXPECT_EQ(texts_of(value_of(csv::parse("# a note\na;b,c\n", semi))), (table{{"a", "b,c"}}));
    csv::options bad;
    bad.separator = '"';
    EXPECT_THROW((void)csv::parse("a", bad), std::invalid_argument);
    auto short_row = csv::parse("a,b\n1,2\n3\n");
    ASSERT_FALSE(short_row);
    EXPECT_EQ(short_row.error().code(), errc::field_count);
    EXPECT_EQ(short_row.error().line(), 3u);
    auto quote = csv::parse("a\n\"open");
    ASSERT_FALSE(quote);
    EXPECT_EQ(quote.error().code(), errc::unexpected_end);
    // typed: the first line the header
    EXPECT_TRUE(value_of(csv::parse<named>("")).empty());
    EXPECT_TRUE(value_of(csv::parse<named>("name\n")).empty());
    auto people = value_of(csv::parse<named>("name\nAda\n\"Lovelace, A.\""));
    ASSERT_EQ(people.size(), 2u);
    EXPECT_EQ(people[1].name, "Lovelace, A.");
    EXPECT_EQ(value_of(csv::parse<named>("id;name\n1;Ada\n", semi))[0].name, "Ada");
    auto wrong = csv::parse<numbers>("i8\n1\n300\n");
    ASSERT_FALSE(wrong);
    EXPECT_EQ(wrong.error().code(), errc::out_of_range);
    EXPECT_EQ(wrong.error().path(), "/i8");
    EXPECT_THROW((void)csv::parse<named>("name", bad), std::invalid_argument);
    // stringify
    EXPECT_EQ(value_of(csv::stringify(sgcl::vector<named>())), "");
    EXPECT_EQ(value_of(csv::stringify(sgcl::vector<csv::row>())), "");
    EXPECT_EQ(value_of(csv::stringify(rows)), "a,b\n1,\"x,y\"\n2,3\n");
    std::vector<std::vector<std::string>> plain = {{"x", ""}, {""}, {"#y", "q\"q"}};
    EXPECT_EQ(value_of(csv::stringify(plain)), "x,\n\"\"\n#y,\"q\"\"q\"\n");
    EXPECT_EQ(value_of(csv::stringify(plain, semi)), "x;\n\"\"\n\"#y\";\"q\"\"q\"\n");
    EXPECT_EQ(value_of(csv::stringify(people)), "name\nAda\n\"Lovelace, A.\"\n");
    auto back = value_of(csv::parse<named>(value_of(csv::stringify(people, semi)), semi));
    ASSERT_EQ(back.size(), 2u);
    EXPECT_EQ(back[1].name, "Lovelace, A.");
    numbers n;
    n.d = NAN;
    EXPECT_EQ(value_of(csv::stringify(std::vector<numbers>{n})), "i8,u8,i64,u64,d,f\n0,0,0,0,NaN,0\n");
    auto no_text = csv::stringify(sgcl::vector<listed>{listed{{1, 2}}});
    ASSERT_FALSE(no_text);
    EXPECT_EQ(no_text.error().code(), errc::unsupported_value);
    EXPECT_EQ(no_text.error().path(), "/list");
    EXPECT_EQ(no_text.error().message(), "/list: a value CSV has no text for");
    EXPECT_FALSE(no_text.error().io_error());
    EXPECT_THROW((void)csv::stringify(plain, bad), std::invalid_argument);
}
