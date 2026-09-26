//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A program's own types in XML through describe(field_list&): written and
// read back, read from documents that order and repeat their elements as
// they please, the errors with their paths, the kinds XML has no form for,
// the reader a value at a time, the writer, and Go's xml.Marshal as the
// oracle of what is written.
#include "xml_common.h"
#include "xml_go_tests.h"

using namespace sgcl::encoding;
using namespace xml_test;

namespace {
    enum class genre { novel, poetry, essay };

    struct author {
        string first;
        string last;

        void describe(field_list& f) {
            f.add("first", first).attribute();
            f.add("last", last).attribute();
        }

        friend bool operator==(const author&, const author&) = default;
    };

    struct price {
        string currency = "PLN";
        double amount = 0;

        void describe(field_list& f) {
            f.add("currency", currency).attribute();
            f.add("amount", amount).text();
        }

        friend bool operator==(const price&, const price&) = default;
    };

    struct book {
        string id;
        bool available = false;
        string title;
        vector<author> authors;
        optional<price> cost;
        vector<string> tags;
        genre kind = genre::novel;
        int8_t rating = 0;
        uint64_t isbn = 0;
        float weight = 0;
        array<int, 3> dimensions{};
        optional<int> pages;
        optional<string> note;

        void describe(field_list& f) {
            f.add("id", id).attribute().required();
            f.add("available", available).attribute();
            f.add("title", title).required();
            f.add("author", authors);
            f.add("price", cost);
            f.add("tag", tags);
            f.add("genre", kind).names({"novel", "poetry", "essay"});
            f.add("rating", rating);
            f.add("isbn", isbn).omit_empty();
            f.add("weight", weight);
            f.add("dimension", dimensions);
            f.add("pages", pages).attribute();
            f.add("note", note);
        }

        friend bool operator==(const book& a, const book& b) {
            return a.id == b.id && a.available == b.available && a.title == b.title && std::equal(a.authors.begin(), a.authors.end(), b.authors.begin(), b.authors.end())
                && a.cost == b.cost && std::equal(a.tags.begin(), a.tags.end(), b.tags.begin(), b.tags.end()) && a.kind == b.kind && a.rating == b.rating
                && a.isbn == b.isbn && a.weight == b.weight && a.dimensions[0] == b.dimensions[0] && a.dimensions[1] == b.dimensions[1]
                && a.dimensions[2] == b.dimensions[2] && a.pages == b.pages && a.note == b.note;
        }
    };

    struct catalog {
        string name;
        vector<book> books;
        tracked_ptr<catalog> next;

        void describe(field_list& f) {
            f.add("name", name).attribute();
            f.add("book", books);
            f.add("next", next);
        }
    };

    book lalka() {
        book b;
        b.id = "b1";
        b.available = true;
        b.title = "Lalka & <more>";
        b.authors.push_back(author{"Boles\xC5\x82" "aw", "Prus"});
        b.authors.push_back(author{"Jan", "\"Editor\""});
        b.cost = price{"EUR", 9.5};
        b.tags.push_back("classic");
        b.tags.push_back("warsaw");
        b.kind = genre::poetry;
        b.rating = -5;
        b.isbn = 9788373271890ull;
        b.weight = 0.1f;
        b.dimensions = {20, 13, 4};
        b.pages = 700;
        return b;
    }
}

TEST(XmlTyped_Tests, WrittenAndReadBack) {
    book b = lalka();
    auto text = xml::stringify("book", b, xml::pretty);
    ASSERT_TRUE(text.has_value()) << text.error().message().view();
    EXPECT_EQ(std::string(text->view()),
        "<book id=\"b1\" available=\"true\" pages=\"700\">\n"
        "  <title>Lalka &amp; &lt;more&gt;</title>\n"
        "  <author first=\"Boles\xC5\x82" "aw\" last=\"Prus\"/>\n"
        "  <author first=\"Jan\" last=\"&quot;Editor&quot;\"/>\n"
        "  <price currency=\"EUR\">9.5</price>\n"
        "  <tag>classic</tag>\n"
        "  <tag>warsaw</tag>\n"
        "  <genre>poetry</genre>\n"
        "  <rating>-5</rating>\n"
        "  <isbn>9788373271890</isbn>\n"
        "  <weight>0.1</weight>\n"
        "  <dimension>20</dimension>\n"
        "  <dimension>13</dimension>\n"
        "  <dimension>4</dimension>\n"
        "</book>");
    auto back = xml::parse<book>(*text);
    ASSERT_TRUE(back.has_value()) << back.error().message().view();
    EXPECT_TRUE(*back == b);
    // empty ones: omit_empty, no optional, no elements of a sequence
    book empty;
    empty.id = "e";
    empty.title = "t";
    auto e = xml::stringify("book", empty);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(std::string(e->view()), "<book id=\"e\" available=\"false\"><title>t</title><genre>novel</genre><rating>0</rating><weight>0</weight>"
                  "<dimension>0</dimension><dimension>0</dimension><dimension>0</dimension></book>");
    EXPECT_TRUE(*xml::parse<book>(*e) == empty);
}

// A document orders and repeats its elements as it pleases: the elements
// of a sequence among others, unknown elements and attributes passed over,
// white space around numbers and booleans, CDATA, references
TEST(XmlTyped_Tests, ReadFromWhatDocumentsWrite) {
    auto b = xml::parse<book>(R"(<?xml version="1.0"?>
<!-- a book -->
<book pages=" 12 " id="x" extra="ignored" available="1">
  <tag>a</tag>
  <unknown><deep>ignored</deep></unknown>
  <title><![CDATA[<T>]]> &amp; co</title>
  <tag>b</tag>
  <rating>
    7
  </rating>
  <author first="A" last="B"/>
  <genre> essay </genre>
  <tag>c</tag>
  <weight>1e-3</weight>
  <dimension>1</dimension><dimension>2</dimension><dimension>3</dimension>
  <note></note>
</book>)");
    ASSERT_TRUE(b.has_value()) << b.error().message().view();
    EXPECT_EQ(b->id, "x");
    EXPECT_TRUE(b->available);
    EXPECT_EQ(b->title, "<T> & co");
    EXPECT_EQ(b->tags.size(), 3u);
    EXPECT_EQ(b->tags[2], "c");
    EXPECT_EQ(b->rating, 7);
    EXPECT_EQ(b->kind, genre::essay);
    EXPECT_EQ(b->weight, 0.001f);
    EXPECT_EQ(b->pages, 12);
    EXPECT_EQ(b->authors.size(), 1u);
    EXPECT_FALSE(b->cost.has_value());
    ASSERT_TRUE(b->note.has_value());
    EXPECT_EQ(*b->note, "");
    // a node read from a tree
    auto tree = xml::parse(string("<author first='F' last='L'/>")).value();
    EXPECT_EQ(tree.as<author>().value(), (author{"F", "L"}));
    // namespaces: a field named {uri}local matches whatever the prefix
    struct entry {
        string title;
        string id;
        void describe(field_list& f) {
            f.add("{http://www.w3.org/2005/Atom}title", title);
            f.add("{}id", id).attribute();
        }
    };
    auto a = xml::parse<entry>(string("<a:entry xmlns:a='http://www.w3.org/2005/Atom' id='7'><a:title>T</a:title></a:entry>"));
    ASSERT_TRUE(a.has_value()) << a.error().message().view();
    EXPECT_EQ(a->title, "T");
    EXPECT_EQ(a->id, "7");
}

TEST(XmlTyped_Tests, Errors) {
    struct Case {
        const char* doc;
        errc code;
        const char* path;
    };
    const Case cases[] = {
        {"<book><title>t</title></book>", errc::missing_field, "/book/@id"},
        {"<book id='1'/>", errc::missing_field, "/book/title"},
        {"<book id='1'><title>t</title><rating>128</rating></book>", errc::out_of_range, "/book/rating"},
        {"<book id='1'><title>t</title><rating>1.5</rating></book>", errc::type_mismatch, "/book/rating"},
        {"<book id='1'><title>t</title><rating>x</rating></book>", errc::type_mismatch, "/book/rating"},
        {"<book id='1' available='yes'><title>t</title></book>", errc::type_mismatch, "/book/@available"},
        {"<book id='1'><title>t</title><genre>drama</genre></book>", errc::type_mismatch, "/book/genre"},
        {"<book id='1'><title><b>t</b></title></book>", errc::type_mismatch, "/book/title"},
        {"<book id='1'><title>t</title><author first='a' last='b'/><author first='c' last='d'/><price currency='X'>cheap</price></book>", errc::type_mismatch, "/book/price/text()"},
        {"<book id='1'><title>t</title><dimension>1</dimension><dimension>2</dimension></book>", errc::type_mismatch, "/book/dimension"},
        {"<book id='1'><title>t</title><dimension>1</dimension><dimension>2</dimension><dimension>x</dimension></book>", errc::type_mismatch, "/book/dimension[3]"},
        {"<book id='1' pages='many'><title>t</title></book>", errc::type_mismatch, "/book/@pages"},
        {"<book id='1'><title>t</title><isbn>-1</isbn></book>", errc::out_of_range, "/book/isbn"},
    };
    for (auto& c : cases) {
        auto r = xml::parse<book>(string(c.doc));
        ASSERT_FALSE(r.has_value()) << c.doc;
        EXPECT_EQ(r.error().code(), c.code) << c.doc << ": " << r.error().message().view();
        EXPECT_EQ(r.error().path(), c.path) << c.doc << ": " << r.error().message().view();
        EXPECT_EQ(r.error().offset(), 0u) << c.doc;
        EXPECT_EQ(r.error().line(), 1u) << c.doc;
    }
    // an index into a sequence of records, and the place of the root
    auto nested = xml::parse<catalog>(string("<?xml version='1.0'?>\n<catalog>\n<book id='1'><title>a</title></book>\n<book id='2'><title>b</title><rating>x</rating></book>\n</catalog>"));
    ASSERT_FALSE(nested.has_value());
    EXPECT_EQ(nested.error().path(), "/catalog/book[2]/rating");
    EXPECT_EQ(nested.error().offset(), 22u);
    EXPECT_EQ(nested.error().line(), 2u);
    EXPECT_EQ(nested.error().message(), "2:1 /catalog/book[2]/rating: expected an integer, found \"x\"");
    // a document that is not well formed is that error, not the mapping's
    EXPECT_EQ(xml::parse<book>(string("<book>")).error().code(), errc::unexpected_end);
}

// What XML has no form for: a map, a tuple, a variant, a list of lists, a
// structure in an attribute; and a cycle through tracked pointers
TEST(XmlTyped_Tests, WhatHasNoForm) {
    struct with_map {
        map<string, int> m;
        void describe(field_list& f) {
            f.add("m", m);
        }
    };
    with_map wm;
    wm.m.insert_or_assign("a", 1);
    auto w = xml::stringify("x", wm);
    ASSERT_FALSE(w.has_value());
    EXPECT_EQ(w.error().code(), errc::unsupported_value);
    EXPECT_EQ(w.error().path(), "/x/m");
    EXPECT_EQ(xml::parse<with_map>(string("<x><m/></x>")).error().code(), errc::type_mismatch);

    struct nested_lists {
        vector<vector<int>> v;
        void describe(field_list& f) {
            f.add("v", v);
        }
    };
    nested_lists nl;
    nl.v.push_back(vector<int>{1});
    EXPECT_EQ(xml::stringify("x", nl).error().code(), errc::unsupported_value);

    struct record_attribute {
        author a;
        void describe(field_list& f) {
            f.add("a", a).attribute();
        }
    };
    EXPECT_EQ(xml::stringify("x", record_attribute{}).error().code(), errc::unsupported_value);
    EXPECT_EQ(xml::stringify("1x", author{}).error().code(), errc::unsupported_value);

    tracked_ptr<catalog> loop = make_tracked<catalog>();
    loop->next = loop;
    auto c = xml::stringify("catalog", *loop);
    ASSERT_FALSE(c.has_value());
    EXPECT_EQ(c.error().code(), errc::unsupported_value);
    // a chain as deep as the limit is fine; its reading too
    tracked_ptr<catalog> chain = make_tracked<catalog>();
    tracked_ptr<catalog> at = chain;
    for (int i = 0; i < 500; ++i) {
        at->next = make_tracked<catalog>();
        at = at->next;
    }
    auto deep = xml::stringify("catalog", *chain);
    ASSERT_TRUE(deep.has_value());
    auto read = xml::parse<catalog>(*deep);
    ASSERT_TRUE(read.has_value()) << read.error().message().view();
    int n = 0;
    for (auto p = read->next; p; p = p->next) {
        ++n;
    }
    EXPECT_EQ(n, 500);
    // floats that JSON has no text for
    struct special {
        double d = 0;
        void describe(field_list& f) {
            f.add("d", d);
        }
    };
    special s;
    s.d = -INFINITY;
    EXPECT_EQ(*xml::stringify("s", s), "<s><d>-INF</d></s>");
    EXPECT_TRUE(std::isnan(xml::parse<special>(string("<s><d>NaN</d></s>"))->d));
    EXPECT_EQ(xml::parse<special>(string("<s><d>INF</d></s>"))->d, INFINITY);
}

// The reader a value at a time: a document of any length read in the
// memory of one element; an element that is not a T stops the reading
// with its path and the offset of its start
TEST(XmlTyped_Tests, ReaderAndWriter) {
    std::string doc = "<catalog>";
    for (int i = 0; i < 200; ++i) {
        doc += "<book id='b" + std::to_string(i) + "'><title>T" + std::to_string(i) + "</title><rating>" + std::to_string(i % 100) + "</rating></book>";
    }
    std::string bad_at = "<book id='bad'><title>x</title><rating>300</rating></book>";
    size_t bad_offset = doc.size();
    doc += bad_at + "</catalog>";
    xml::reader r(make_tracked<enc_test::dribble>(doc, 7));
    r.next();
    int n = 0;
    while (auto b = r.read<book>()) {
        EXPECT_EQ(b->id, "b" + std::to_string(n));
        ++n;
    }
    EXPECT_EQ(n, 200);
    ASSERT_TRUE(r.last_error().has_value());
    EXPECT_EQ(r.last_error()->code(), errc::out_of_range);
    EXPECT_EQ(r.last_error()->path(), "/book/rating");
    EXPECT_EQ(r.last_error()->offset(), bad_offset);
    EXPECT_FALSE(r.read<book>().has_value());
    EXPECT_FALSE(r.next().has_value());

    sgcl::tracked_ptr out = make_tracked<enc_test::sink>();
    xml::writer w(out);
    book second;
    second.id = "b2";
    second.title = "Second";
    w.start("catalog").value("book", lalka()).value("book", second).end();
    ASSERT_TRUE(w.flush().has_value());
    auto back = xml::parse<catalog>(string(out->text));
    ASSERT_TRUE(back.has_value()) << back.error().message().view();
    EXPECT_EQ(back->books.size(), 2u);
    EXPECT_TRUE(back->books[0] == lalka());
    // a map has no form: the mistake is kept, as the writer keeps its others
    struct with_map {
        map<string, int> m;
        void describe(field_list& f) {
            f.add("m", m);
        }
    };
    xml::writer bad(out);
    bad.start("a").value("x", with_map{}).end();
    ASSERT_TRUE(bad.last_error().has_value());
    EXPECT_EQ(bad.last_error()->code(), errc::unsupported_value);
    EXPECT_FALSE(bad.flush().has_value());
}

// The typed async_parse the same: made with its options a temporary,
// started after the full expression that made it
TEST(XmlTyped_Tests, AsyncParseHoldsItsOptions) {
    std::string doc = "<catalog name='c'><book id='1'><title>a</title></book></catalog>";
    auto plain = xml::async_parse<catalog>(make_tracked<enc_test::dribble>(doc, 3));
    auto shallow = xml::async_parse<catalog>(make_tracked<enc_test::dribble>(doc, 3), xml::options{2});
    auto c = sgcl::async::spawn(std::move(plain)).wait();
    ASSERT_TRUE(c);
    EXPECT_EQ(c->books.size(), 1u);
    auto refused = sgcl::async::spawn(std::move(shallow)).wait();
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), errc::depth_limit);
    sgcl::async::scheduler::stop();
}

TEST(XmlTyped_Tests, AsyncForms) {
    auto t = sgcl::async::spawn([]() -> async::task<int> {
        std::string doc = "<catalog name='c'><book id='1'><title>a</title></book><book id='2'><title>b</title></book></catalog>";
        auto c = co_await xml::async_parse<catalog>(make_tracked<enc_test::dribble>(doc, 3));
        if (!c || c->books.size() != 2 || c->name != "c") {
            co_return -1;
        }
        xml::reader r(make_tracked<enc_test::dribble>(doc, 2));
        co_await r.async_next();
        auto b = co_await r.async_read<book>();
        if (!b || b->title != "a") {
            co_return -2;
        }
        co_return 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

// Go's xml.Marshal of the same structures (tools/xml_oracle.go, GoTyped):
// attributes, chardata, nested and repeated elements, escaping, the text
// of booleans and integers
TEST(XmlTyped_Tests, WrittenAsGoWritesIt) {
    struct item {
        string sku;
        int count = 0;
        bool gift = false;
        string text;
        void describe(field_list& f) {
            f.add("sku", sku).attribute();
            f.add("count", count).attribute();
            f.add("gift", gift).attribute();
            f.add("text", text).text();
        }
    };
    struct order {
        int64_t id = 0;
        string customer;
        vector<item> items;
        vector<string> notes;
        uint32_t total = 0;
        void describe(field_list& f) {
            f.add("id", id).attribute();
            f.add("customer", customer);
            f.add("item", items);
            f.add("note", notes);
            f.add("total", total);
        }
    };
    order o;
    o.id = -9007199254740993;
    o.customer = "Kowalski & S\xC3\xB3n <sp. z o.o.>";
    o.items.push_back(item{"A-1", 2, true, "red \"big\" one"});
    o.items.push_back(item{"B'2", 0, false, ""});
    o.notes.push_back("fragile");
    o.notes.push_back("");
    o.total = 4294967295u;
    ASSERT_GE(std::size(GoTyped), 1u);
    auto mine = xml::stringify("order", o);
    ASSERT_TRUE(mine.has_value());
    // Go writes an empty element as a start and an end, escapes '"' and '\''
    // in text as &#34; and &#39;, and an attribute's '\'' as &#39;: the same
    // XML, compared as the tokens a reader gives
    EXPECT_EQ(dump(mine->view()).tokens, dump(GoTyped[0]).tokens) << mine->view() << "\n" << GoTyped[0];
    auto back = xml::parse<order>(string(GoTyped[0]));
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back->id, o.id);
    EXPECT_EQ(back->customer, o.customer);
    EXPECT_EQ(back->items.size(), 2u);
    EXPECT_EQ(back->items[0].text, o.items[0].text);
    EXPECT_EQ(back->total, o.total);
}
