//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// CSV (E5) against Go's encoding/csv (tools/csv_oracle.go): the records,
// the places of their fields and the errors of named texts and of a
// hundred thousand random ones; the reader in pieces; the writer; the
// header; records as types of the program.
#include "common.h"
#include "csv_tests.h"

#include <cmath>

using namespace enc_test;
using sgcl::encoding::csv;
using sgcl::encoding::errc;
using sgcl::encoding::field_list;
using sgcl::string;

namespace {
    struct rng {
        uint64_t state;
        uint64_t next() {
            state += 0x9E3779B97F4A7C15ull;
            uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }
    };

    // What the reader reads of a text, in the oracle's form: a record a
    // line, <line:column>field joined by \x1f; the error as !kind line:column
    std::string read_as_oracle(csv::reader& r) {
        std::string out;
        while (auto row = r.next()) {
            for (size_t i = 0; i < row->size(); ++i) {
                if (i) {
                    out += '\x1f';
                }
                auto [line, column] = row->position(i);
                out += "<" + std::to_string(line) + ":" + std::to_string(column) + ">";
                out.append((*row)[i].view());
            }
            out += '\n';
        }
        if (auto& e = r.last_error()) {
            std::string kind = e->code() == errc::field_count ? "count" : e->code() == errc::unexpected_end ? "quote" : std::string(e->message().view()).find("bare") != std::string::npos ? "bare" : "quote";
            out += "!" + kind + " " + std::to_string(e->line()) + ":" + std::to_string(e->column()) + "\n";
        }
        return out;
    }

    csv::options options_of(char separator, char comment, bool lazy, bool trim, bool same) {
        csv::options o;
        o.separator = separator;
        o.comment = comment;
        o.lazy_quotes = lazy;
        o.trim_leading_space = trim;
        o.same_field_count = same;
        return o;
    }

    std::string read_text(std::string_view text, const csv::options& o, size_t pieces) {
        if (pieces) {
            csv::reader r(sgcl::make_tracked<dribble>(std::string(text), pieces), o);
            return read_as_oracle(r);
        }
        csv::reader r(string(text), o);
        return read_as_oracle(r);
    }
}

// Every named text as Go reads it, whole and fed 1, 2, 3 and 7 bytes at a time
TEST(Csv_Tests, ReadsAsGoReads) {
    for (auto& c : csv_oracle::reads) {
        auto o = options_of(c.separator, c.comment, c.lazy, c.trim, c.same_count);
        std::string expected(c.read);
        if (c.text.starts_with("\xef\xbb\xbf")) {
            // a difference by name: the column counts code points here, the
            // BOM's three bytes are one (Go counts bytes); the BOM itself
            // is a byte of the first field, as in Go
            expected.replace(expected.find("<1:6>"), 5, "<1:4>");
        }
        for (size_t n : {0, 1, 2, 3, 7}) {
            EXPECT_EQ(read_text(c.text, o, n), expected) << "text " << c.text << " pieces " << n;
        }
    }
}

// A hundred thousand random texts of the characters that matter, with
// four sets of options, as Go reads them
TEST(Csv_Tests, RandomTextsAsGoReads) {
    const std::string alphabet = ",;\"\n\r #\tab";
    const csv::options sets[] = {
        options_of(',', 0, false, false, true),
        options_of(',', 0, false, false, false),
        options_of(';', '#', false, false, false),
        options_of(',', 0, true, true, false),
    };
    rng r{2026};
    uint64_t h = 0xcbf29ce484222325ull;
    for (int i = 0; i < 100000; ++i) {
        size_t n = size_t(r.next() % 24);
        std::string text;
        for (size_t k = 0; k < n; ++k) {
            text += alphabet[r.next() % alphabet.size()];
        }
        auto read = read_text(text, sets[i % 4], 0);
        for (char c : read) {
            h ^= uint8_t(c);
            h *= 0x100000001b3ull;
        }
        if (i % 97 == 0) {
            EXPECT_EQ(read_text(text, sets[i % 4], 1 + i % 5), read) << text;
        }
    }
    EXPECT_EQ(h, csv_oracle::random_hash);
}

TEST(Csv_Tests, WritesAsGoWrites) {
    for (auto& c : csv_oracle::writes) {
        sgcl::tracked_ptr out = sgcl::make_tracked<sink>();
        csv::options o;
        o.separator = c.separator;
        csv::writer w(out, o);
        if (c.crlf) {
            w.use_crlf();
        }
        for (auto record : string(c.fields).split('\x1e')) {
            sgcl::vector<string> fields;
            for (auto f : string(record).split('\x1f')) {
                fields.push_back(string(f));
            }
            w.write(fields);
        }
        EXPECT_TRUE(w.flush());
        EXPECT_EQ(out->text, c.text);
    }
}

// Found by the fuzzer of the codecs (tests/encoding/fuzz/codecs_fuzz.cpp):
// a record of one empty field was written as an empty line, as Go writes
// it, and every reader (this one and Go's) passes over an empty line — the
// record was lost between the writing and the reading. It is written ""
// now (a deliberate difference from Go's writer; RFC 4180 allows it,
// Python writes it), which Go's reader reads as one empty field (the
// oracle's case "\"\"\n" in ReadsAsGoReads)
TEST(Csv_Tests, ARecordOfOneEmptyFieldSurvives) {
    for (bool crlf : {false, true}) {
        sgcl::tracked_ptr out = sgcl::make_tracked<sink>();
        csv::writer w(out);
        if (crlf) {
            w.use_crlf();
        }
        w.write({string("")});
        w.write({string("a"), string("")});
        w.write({string(""), string("")});
        w.write({string("")});
        EXPECT_TRUE(w.flush());
        EXPECT_EQ(out->text, crlf ? "\"\"\r\na,\r\n,\r\n\"\"\r\n" : "\"\"\na,\n,\n\"\"\n");
        csv::options o;
        o.same_field_count = false;
        csv::reader r(string(out->text), o);
        std::vector<size_t> sizes;
        while (auto row = r.next()) {
            sizes.push_back(row->size());
            for (auto f : *row) {
                EXPECT_TRUE(f.empty() || f.view() == "a");
            }
        }
        EXPECT_FALSE(r.last_error());
        EXPECT_EQ(sizes, (std::vector<size_t>{1, 2, 2, 1}));
    }
}

// The header: rows asked by the column's name; a row kept stays valid
TEST(Csv_Tests, HeaderAndRows) {
    csv::reader r(string("name,age,city\nAla,30,Kraków\nOla,25,\n"));
    auto h = r.read_header();
    ASSERT_TRUE(h);
    EXPECT_EQ(r.header().size(), 3u);
    EXPECT_EQ(r.header()[2], string("city"));
    sgcl::vector<csv::row> kept;
    for (auto row : r.rows()) {
        kept.push_back(row);
    }
    ASSERT_EQ(kept.size(), 2u);
    EXPECT_EQ(kept[0]["name"]->view(), "Ala");
    EXPECT_EQ(kept[0]["city"]->view(), "Kraków");
    EXPECT_EQ(kept[1]["city"]->view(), "");
    EXPECT_FALSE(kept[1]["country"]);
    EXPECT_EQ(kept[1][1].view(), "25");
    EXPECT_EQ(kept[1].line(), 3u);
    EXPECT_THROW((void)kept[1].at(3), std::out_of_range);   // operator[] does not check, as a vector's does not
    EXPECT_EQ(kept[1].at(0), kept[1][0]);
    std::string joined;
    for (auto f : kept[0]) {
        joined += std::string(f.view()) + "|";
    }
    EXPECT_EQ(joined, "Ala|30|Kraków|");
    // columns in code points
    csv::reader u(string("ż,\"ą\",x\n"));
    auto row = u.next();
    EXPECT_EQ(row->position(1), (sgcl::pair<uint32_t, uint32_t>{1, 3}));
    EXPECT_EQ(row->position(2), (sgcl::pair<uint32_t, uint32_t>{1, 7}));
    csv::reader bad(string("\"ż\"x\n"));
    EXPECT_FALSE(bad.next());
    EXPECT_EQ(bad.last_error()->column(), 3u);
    EXPECT_EQ(bad.last_error()->message(), string("1:3: extraneous or missing \" in a quoted field"));
    // invalid options
    csv::options quote_sep;
    quote_sep.separator = '"';
    EXPECT_THROW(csv::reader(string("a"), quote_sep), std::invalid_argument);
    csv::options same;
    same.comment = ',';
    EXPECT_THROW(csv::reader(string("a"), same), std::invalid_argument);
}

// A long quoted field trickling in a byte at a time, and the collector in the loop
TEST(Csv_Tests, LongFieldsInPieces) {
    std::string big(100000, 'x');
    std::string text = "a,\"" + big + "\"\"" + big + "\"\nb,c\n";
    csv::reader r(sgcl::make_tracked<dribble>(text, 3));
    auto first = r.next();
    ASSERT_TRUE(first);
    EXPECT_EQ((*first)[1].size(), 200001u);
    sgcl::collector::force_collect(true);
    auto second = r.next();
    ASSERT_TRUE(second);
    EXPECT_EQ((*second)[1].view(), "c");
    EXPECT_EQ((*first)[0].view(), "a");
    EXPECT_FALSE(r.next());
    EXPECT_FALSE(r.last_error());
    // a stream that fails
    csv::reader f(sgcl::make_tracked<failing>(std::string("a,b\nc,")));
    EXPECT_TRUE(f.next());
    EXPECT_FALSE(f.next());
    EXPECT_EQ(f.last_error()->code(), errc::io);
}

namespace records {
    enum class kind { small, large };

    struct item {
        string name;
        int count = 0;
        double price = 0;
        bool active = false;
        sgcl::optional<int> stock;
        kind size = kind::small;
        std::string note;
        void describe(field_list& f) {
            f.add("name", name).required();
            f.add("count", count);
            f.add("price", price);
            f.add("active", active);
            f.add("stock", stock);
            f.add("size", size).names({"small", "large"});
            f.add("note", note);
        }
    };

    struct nested {
        sgcl::vector<int> list;
        void describe(field_list& f) {
            f.add("list", list);
        }
    };
}

// Records as types: by the header's names, both ways
TEST(Csv_Tests, Records) {
    using records::item;
    csv::reader r(string("note,name,price,count,active,stock,size,extra\n\"a, b\",pen,1.5,3,true,,large,x\nn,cup,2,1,0,7,small,\n"));
    auto a = r.read<item>();
    ASSERT_TRUE(a) << r.last_error()->message();
    EXPECT_EQ(a->name, string("pen"));
    EXPECT_EQ(a->count, 3);
    EXPECT_EQ(a->price, 1.5);
    EXPECT_TRUE(a->active);
    EXPECT_FALSE(a->stock);
    EXPECT_EQ(a->size, records::kind::large);
    EXPECT_EQ(a->note, "a, b");
    auto b = r.read<item>();
    ASSERT_TRUE(b);
    EXPECT_FALSE(b->active);
    EXPECT_EQ(b->stock, 7);
    EXPECT_FALSE(r.read<item>());
    EXPECT_FALSE(r.last_error());
    // written: the header first, then the records
    sgcl::tracked_ptr out = sgcl::make_tracked<sink>();
    csv::writer w(out);
    w.write(*a).write(*b);
    EXPECT_TRUE(w.flush());
    EXPECT_EQ(out->text, "name,count,price,active,stock,size,note\npen,3,1.5,true,,large,\"a, b\"\ncup,1,2,false,7,small,n\n");
    // and read back
    csv::reader back(string(out->text));
    EXPECT_EQ(back.read<item>()->note, "a, b");
    // errors: a value that is not one, a missing required column, a
    // field CSV has no text for
    csv::reader wrong(string("name,count\nx,1\ny,z\n"));
    EXPECT_TRUE(wrong.read<item>());
    EXPECT_FALSE(wrong.read<item>());
    EXPECT_EQ(wrong.last_error()->code(), errc::type_mismatch);
    EXPECT_EQ(wrong.last_error()->message(), string("3:3 /count: \"z\" is not an integer"));
    csv::reader range(string("name,count\nx,99999999999\n"));
    EXPECT_FALSE(range.read<item>());
    EXPECT_EQ(range.last_error()->code(), errc::out_of_range);
    csv::reader missing(string("count\n1\n"));
    EXPECT_FALSE(missing.read<item>());
    EXPECT_EQ(missing.last_error()->code(), errc::missing_field);
    EXPECT_EQ(missing.last_error()->path(), string("/name"));
    csv::reader named(string("name,size\nx,medium\n"));
    EXPECT_FALSE(named.read<item>());
    EXPECT_EQ(named.last_error()->code(), errc::type_mismatch);
    csv::reader list(string("list\n1\n"));
    EXPECT_FALSE(list.read<records::nested>());
    EXPECT_EQ(list.last_error()->code(), errc::unsupported_value);
    sgcl::tracked_ptr o2 = sgcl::make_tracked<sink>();
    csv::writer w2(o2);
    w2.write(records::nested{{1, 2}});
    auto f = w2.flush();
    ASSERT_FALSE(f);
    EXPECT_EQ(f.error().code(), sgcl::encoding::make_error_code(errc::unsupported_value));
    // NaN and the infinities are written as Go's strconv writes them
    struct measure {
        double v = 0;
        float w = 0.1f;
        void describe(field_list& f) {
            f.add("v", v);
            f.add("w", w);
        }
    };
    sgcl::tracked_ptr o3 = sgcl::make_tracked<sink>();
    csv::writer w3(o3);
    w3.write(measure{NAN}).write(measure{-INFINITY, 1e-7f});
    EXPECT_TRUE(w3.flush());
    EXPECT_EQ(o3->text, "v,w\nNaN,0.1\n-Inf,1e-7\n");
}

TEST(Csv_Tests, AsyncForms) {
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<int> {
        csv::reader r(sgcl::make_tracked<dribble>(std::string("name,count\npen,3\ncup,1\n"), 2));
        auto h = co_await r.async_read_header();
        if (!h || h->size() != 2) {
            co_return -1;
        }
        auto a = co_await r.async_read<records::item>();
        if (!a || a->count != 3) {
            co_return -2;
        }
        auto row = co_await r.async_next();
        if (!row || (*row)["name"]->view() != "cup") {
            co_return -3;
        }
        if (co_await r.async_next()) {
            co_return -4;
        }
        sgcl::tracked_ptr out = sgcl::make_tracked<sink>();
        csv::writer w(out);
        w.write({"x", "y"});
        auto f = co_await w.async_flush();
        co_return f && out->text == "x,y\n" ? 1 : -5;
    }());
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

TEST(Csv_Tests, ARecordPastTheBoundIsAnError) {
    std::string text = "a,b\n" + std::string(100000, 'x') + ",y\n";
    sgcl::io::buffer src{sgcl::string(text)};
    sgcl::encoding::csv::options o;
    o.max_record_size = 20000;
    sgcl::encoding::csv::reader r(src, o);
    while (r.next()) {
    }
    ASSERT_TRUE(r.last_error());
    EXPECT_EQ(r.last_error()->code(), sgcl::encoding::errc::out_of_range);
}

// The bound holds for every record of a stream, also one that came in one
// block with the records before it; a record of the bound's length is read
TEST(Csv_Tests, ARecordPastTheBoundInOneBlockIsAnError) {
    std::string text = "a,b\n" + std::string(30, 'x') + ",y\n" + std::string(39, 'z') + "\n";
    for (size_t n : {size_t(1), size_t(7), size_t(1000)}) {
        sgcl::encoding::csv::options o;
        o.max_record_size = 31;
        o.same_field_count = false;
        sgcl::encoding::csv::reader r(sgcl::make_tracked<enc_test::dribble>(text, n), o);
        auto header = r.next();
        ASSERT_TRUE(header) << n;
        auto exact = r.next();
        ASSERT_TRUE(exact) << n;   // 30 + 1: the fields' text is the bound's length
        EXPECT_EQ((*exact)[0].size(), 30u);
        EXPECT_FALSE(r.next()) << n;
        ASSERT_TRUE(r.last_error()) << n;
        EXPECT_EQ(r.last_error()->code(), sgcl::encoding::errc::out_of_range) << n;
        EXPECT_EQ(r.last_error()->line(), 3u) << n;
    }
    // a text in memory is not a stream: no bound
    sgcl::encoding::csv::options o;
    o.max_record_size = 31;
    o.same_field_count = false;
    sgcl::encoding::csv::reader t(sgcl::string(text), o);
    size_t records = 0;
    while (t.next()) {
        ++records;
    }
    EXPECT_FALSE(t.last_error());
    EXPECT_EQ(records, 3u);
}

// The reader's line: the one after the record read last; empty lines and
// comments before the next record are counted as it is read
TEST(Csv_Tests, TheLineOfTheReader) {
    sgcl::encoding::csv::options o;
    o.comment = '#';
    sgcl::encoding::csv::reader r(sgcl::string("a\n\n# c\nb\n"), o);
    EXPECT_EQ(r.line(), 1u);
    ASSERT_TRUE(r.next());
    EXPECT_EQ(r.line(), 2u);
    auto b = r.next();
    ASSERT_TRUE(b);
    EXPECT_EQ(b->line(), 4u);
    EXPECT_EQ(r.line(), 5u);
}

// A field by its column's name as a string of its own, or a value for when
// the column is not there or the row is shorter
TEST(Csv_Tests, GetWithAFallback) {
    csv::reader r(string("name,city\nAla,Krakow\nOla\n"), csv::options{.same_field_count = false});
    ASSERT_TRUE(r.read_header());
    std::vector<std::string> got;
    for (auto row : r.rows()) {
        got.push_back(std::string(row.get("name", "?").view()) + "/" + std::string(row.get("city", "?").view()) + "/" + std::string(row.get("zip", "-").view()));
    }
    EXPECT_EQ(got, (std::vector<std::string>{"Ala/Krakow/-", "Ola/?/-"}));
}

// A field holding a run of '\r' before its '\n' (what codecs_fuzz found:
// 84 of them): written, the field is quoted and its bytes go out as they
// are; read, the "\r\n" inside the quotes is a line ending and comes back
// "\n", as Go reads it. So each round of writing and reading takes one
// '\r' off the run, and the text is stable after as many rounds as the run
// is long. Not a fault of the writer or of the reader: what Go's pair does
// (a lone '\r' has no spelling in CSV that survives the "\r\n" rule)
TEST(Csv_Tests, ARunOfCarriageReturnsShortensByOneEachRound) {
    const size_t run = 84;
    std::string field = "x" + std::string(run, '\r') + "\ny";
    auto write = [](const std::string& f) {
        sgcl::tracked_ptr out = sgcl::make_tracked<sink>();
        csv::writer w(out);
        w.write({string(f)});
        EXPECT_TRUE(w.flush());
        return out->text;
    };
    auto read = [](const std::string& text) {
        csv::reader r{string(text)};
        auto row = r.next();
        EXPECT_TRUE(row.has_value());
        EXPECT_EQ(row->size(), 1u);
        return std::string((*row)[0].view());
    };
    std::string text = write(field);
    EXPECT_EQ(text, "\"" + field + "\"\n");   // quoted, the bytes as they are
    for (size_t round = 1; round <= run; ++round) {
        std::string back = read(text);
        EXPECT_EQ(back, "x" + std::string(run - round, '\r') + "\ny") << round;
        text = write(back);
    }
    EXPECT_EQ(write(read(text)), text);   // no '\r' left before the '\n': stable
}
