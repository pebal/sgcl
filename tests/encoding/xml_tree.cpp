//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The tree: a node read, walked, changed into new versions, built, compared
// and written; what it keeps and leaves out; a tree deeper than a thread's
// stack; the collector and threads around it.
#include "xml_common.h"
#include "xml_go_tests.h"

#include <atomic>
#include <thread>

using namespace sgcl::encoding;
using namespace xml_test;

namespace {
    const char* Catalog = R"(<?xml version="1.0"?>
<!-- the catalog -->
<catalog xmlns="urn:books" xmlns:dc="http://purl.org/dc/elements/1.1/">
  <book id="1" dc:lang="pl">
    <title>Lalka</title>
    <dc:creator>Prus</dc:creator>
  </book>
  <book id="2">
    <title>Dune &amp; <![CDATA[<more>]]></title>
    <?page 12?>
  </book>
  <magazine id="3"/>
</catalog>)";
}

TEST(XmlTree_Tests, Walking) {
    auto doc = xml::parse(string(Catalog));
    ASSERT_TRUE(doc.has_value()) << doc.error().message().view();
    xml catalog = *doc;
    EXPECT_EQ(catalog.type(), xml::kind::element);
    EXPECT_TRUE(catalog.is_element());
    EXPECT_TRUE(catalog.exists());
    EXPECT_EQ(catalog.name(), "catalog");
    EXPECT_EQ(catalog.local_name(), "catalog");
    EXPECT_EQ(catalog.namespace_uri(), "urn:books");
    EXPECT_EQ(catalog.attributes().size(), 2u);
    EXPECT_EQ(catalog.children().size(), 3u);   // white space between elements left out
    std::vector<std::string> ids;
    for (auto book : catalog.children("book")) {
        ids.push_back(std::string(book.attribute("id").value_or("?").view()));
    }
    EXPECT_EQ(ids, (std::vector<std::string>{"1", "2"}));
    ids.clear();
    for (auto book : catalog.children("{urn:books}book")) {
        ids.push_back(std::string(book.attribute("id").value_or("?").view()));
    }
    EXPECT_EQ(ids, (std::vector<std::string>{"1", "2"}));
    EXPECT_EQ(catalog.children("{urn:other}book").begin(), catalog.children("x").end());
    xml first = catalog.child("book");
    EXPECT_EQ(first.child("title").text(), "Lalka");
    EXPECT_EQ(first.child("dc:creator").text(), "Prus");
    EXPECT_EQ(first.child("{http://purl.org/dc/elements/1.1/}creator").local_name(), "creator");
    EXPECT_EQ(first.child("{http://purl.org/dc/elements/1.1/}creator").namespace_uri(), "http://purl.org/dc/elements/1.1/");
    EXPECT_EQ(first.attribute("dc:lang").value(), "pl");
    EXPECT_EQ(first.attribute("{http://purl.org/dc/elements/1.1/}lang").value(), "pl");
    EXPECT_FALSE(first.attribute("lang").has_value());
    EXPECT_EQ(first.attribute("{}id").value(), "1");
    // a chain through what is not there is xml() at each step
    xml none = catalog.child("film").child("title").child("x");
    EXPECT_FALSE(none.exists());
    EXPECT_EQ(none.type(), xml::kind::none);
    EXPECT_EQ(none.text(), "");
    EXPECT_EQ(none.name(), "");
    EXPECT_TRUE(none.children().empty());
    EXPECT_TRUE(none.attributes().empty());
    EXPECT_FALSE(none.attribute("id").has_value());
    // text: joined across a reference, a CDATA section and elements
    xml second = catalog.children()[1];
    EXPECT_EQ(second.child("title").text(), "Dune & <more>");
    EXPECT_EQ(second.child("title").children().size(), 1u);   // one text node, not three
    EXPECT_EQ(catalog.text(), "LalkaPrusDune & <more>");
    auto pi = second.children()[1];
    EXPECT_EQ(pi.type(), xml::kind::instruction);
    EXPECT_EQ(pi.name(), "page");
    EXPECT_EQ(pi.text(), "12");
    EXPECT_TRUE(catalog.children()[2].children().empty());
    // the comment and white space kept when asked
    xml::options keep;
    keep.keep_comments = true;
    keep.keep_whitespace = true;
    auto kept = xml::parse(string("<a> <!--c--> <b/> </a>"), keep).value();
    EXPECT_EQ(kept.children().size(), 5u);
    EXPECT_EQ(kept.children()[1].type(), xml::kind::comment);
    EXPECT_EQ(kept.children()[1].text(), "c");
    EXPECT_EQ(xml::parse(string("<a> <!--c--> <b/> </a>")).value().children().size(), 1u);
    // white space next to other text is text
    EXPECT_EQ(xml::parse(string("<a> x <b/> </a>")).value().children()[0].text(), " x ");
}

TEST(XmlTree_Tests, MakingNodes) {
    xml book("book");
    EXPECT_EQ(book.name(), "book");
    EXPECT_TRUE(book.children().empty());
    xml title("title", "Dune");
    EXPECT_EQ(title.text(), "Dune");
    EXPECT_EQ(title.children()[0].type(), xml::kind::text);
    EXPECT_TRUE(xml("empty", "").children().empty());
    xml prefixed("svg:rect");
    EXPECT_EQ(prefixed.local_name(), "rect");
    EXPECT_EQ(prefixed.namespace_uri(), "");
    EXPECT_EQ(xml::text_node("t").type(), xml::kind::text);
    EXPECT_EQ(xml::comment("c").type(), xml::kind::comment);
    EXPECT_EQ(xml::instruction("t", "d").text(), "d");
    for (auto bad : {"", "1a", "a b", "a:b:c", ":a", "a:", "<a>"}) {
        EXPECT_THROW(xml{string(bad)}, std::invalid_argument) << bad;
    }
    EXPECT_THROW(xml::comment("a--b"), std::invalid_argument);
    EXPECT_THROW(xml::comment("a-"), std::invalid_argument);
    EXPECT_THROW(xml::instruction("xml", ""), std::invalid_argument);
    EXPECT_THROW(xml::instruction("XmL", ""), std::invalid_argument);
    EXPECT_THROW(xml::instruction("a:b", ""), std::invalid_argument);
    EXPECT_THROW(xml::instruction("t", "a?>b"), std::invalid_argument);
}

// A change is a new node; the node changed stays as it was
TEST(XmlTree_Tests, NewVersions) {
    xml a = xml("book").set("id", "7").set("lang", "pl");
    xml b = a.set("id", "8");
    xml c = b.erase("lang");
    xml d = c.push_back(xml("title", "Dune"));
    EXPECT_EQ(a.to_string(), "<book id=\"7\" lang=\"pl\"/>");
    EXPECT_EQ(b.to_string(), "<book id=\"8\" lang=\"pl\"/>");
    EXPECT_EQ(c.to_string(), "<book id=\"8\"/>");
    EXPECT_EQ(d.to_string(), "<book id=\"8\"><title>Dune</title></book>");
    EXPECT_EQ(c.erase("nothing").to_string(), c.to_string());
    EXPECT_EQ(d.set("xml:lang", "en").attributes()[1].namespace_uri, "http://www.w3.org/XML/1998/namespace");
    auto ns = xml("p:a").set("xmlns:p", "urn:p").set("p:b", "1");
    EXPECT_EQ(ns.attribute("{urn:p}b").value(), "1");
    EXPECT_EQ(ns.attribute("{http://www.w3.org/2000/xmlns/}p").value(), "urn:p");
    EXPECT_THROW(xml::text_node("t").set("a", "1"), std::invalid_argument);
    EXPECT_THROW(xml().push_back(xml("a")), std::invalid_argument);
    EXPECT_THROW(xml("a").push_back(xml()), std::invalid_argument);
    EXPECT_THROW(xml("a").set("1", "x"), std::invalid_argument);
    EXPECT_THROW(xml::comment("c").erase("a"), std::invalid_argument);
}

TEST(XmlTree_Tests, Builder) {
    xml::builder list("list");
    list.set("n", "1000").set("n", "1001");
    for (int i = 0; i < 1000; ++i) {
        list.push_back(xml("item", string(std::to_string(i))));
    }
    xml built = list.build();
    EXPECT_EQ(built.children().size(), 1000u);
    EXPECT_EQ(built.attribute("n").value(), "1001");
    EXPECT_EQ(built.children()[999].text(), "999");
    xml again = list.build();
    EXPECT_EQ(again.name(), "list");
    EXPECT_TRUE(again.children().empty());
    EXPECT_TRUE(again.attributes().empty());
    EXPECT_THROW(xml::builder("1x"), std::invalid_argument);
    EXPECT_THROW(list.push_back(xml()), std::invalid_argument);
    xml::builder nsb("p:a");
    nsb.set("xmlns:p", "urn:p").set("p:x", "1");
    EXPECT_EQ(nsb.build().attribute("{urn:p}x").value(), "1");
}

TEST(XmlTree_Tests, Equality) {
    auto p = [](const char* s) { return xml::parse(string(s)).value(); };
    EXPECT_EQ(p("<a x='1' y='2'><b/>t</a>"), p("<a y=\"2\" x=\"1\"><b></b>t</a>"));
    EXPECT_NE(p("<a x='1'/>"), p("<a x='2'/>"));
    EXPECT_NE(p("<a x='1'/>"), p("<a y='1'/>"));
    EXPECT_NE(p("<a><b/><c/></a>"), p("<a><c/><b/></a>"));
    EXPECT_NE(p("<a>t</a>"), p("<a>u</a>"));
    EXPECT_NE(p("<p:a xmlns:p='urn:1'/>"), p("<p:a xmlns:p='urn:2'/>"));
    EXPECT_EQ(p("<a>x<![CDATA[y]]>z</a>"), p("<a>xyz</a>"));
    EXPECT_NE(xml::comment("c"), xml::text_node("c"));
    EXPECT_EQ(xml(), xml());
    EXPECT_NE(xml(), xml("a"));
    EXPECT_EQ(xml::instruction("t", "d"), xml::instruction("t", "d"));
    EXPECT_NE(xml::instruction("t", "d"), xml::instruction("t", "e"));
    // many attributes: compared through a sort
    std::string forward = "<a", backward = "<a";
    for (int i = 0; i < 40; ++i) {
        forward += " a" + std::to_string(i) + "='" + std::to_string(i) + "'";
        backward += " a" + std::to_string(39 - i) + "='" + std::to_string(39 - i) + "'";
    }
    EXPECT_EQ(p((forward + "/>").c_str()), p((backward + "/>").c_str()));
    EXPECT_NE(p((forward + "/>").c_str()), p((backward + " b='1'/>").c_str()));
}

TEST(XmlTree_Tests, Writing) {
    auto doc = xml::parse(string(Catalog)).value();
    EXPECT_EQ(doc.to_string(),
        "<catalog xmlns=\"urn:books\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><book id=\"1\" dc:lang=\"pl\">"
        "<title>Lalka</title><dc:creator>Prus</dc:creator></book><book id=\"2\"><title>Dune &amp; &lt;more&gt;</title>"
        "<?page 12?></book><magazine id=\"3\"/></catalog>");
    EXPECT_EQ(doc.to_string(xml::pretty),
        "<catalog xmlns=\"urn:books\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
        "  <book id=\"1\" dc:lang=\"pl\">\n"
        "    <title>Lalka</title>\n"
        "    <dc:creator>Prus</dc:creator>\n"
        "  </book>\n"
        "  <book id=\"2\">\n"
        "    <title>Dune &amp; &lt;more&gt;</title>\n"
        "    <?page 12?>\n"
        "  </book>\n"
        "  <magazine id=\"3\"/>\n"
        "</catalog>");
    EXPECT_EQ(xml("a").to_string(xml::style{0, true}), "<?xml version=\"1.0\" encoding=\"UTF-8\"?><a/>");
    EXPECT_EQ(xml("a").to_string(xml::style{4, true}), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<a/>");
    // mixed content is written as it is
    auto mixed = xml::parse(string("<p>Some <b>bold</b> text<br/></p>")).value();
    EXPECT_EQ(mixed.to_string(xml::pretty), "<p>Some <b>bold</b> text<br/></p>");
    // what text and values need
    auto escaped = xml("a").set("v", "<&>\"'\t\n\r").push_back(xml::text_node("<&>\"'\t\n\r]]>"));
    EXPECT_EQ(escaped.to_string(), "<a v=\"&lt;&amp;&gt;&quot;'&#x9;&#xA;&#xD;\">&lt;&amp;&gt;\"'\t\n&#xD;]]&gt;</a>");
    EXPECT_EQ(xml::parse(escaped.to_string()).value(), escaped);
    // what XML cannot hold at all: U+FFFD
    auto controls = xml("a").set("v", string(std::string("x\x01y\xC3", 4))).push_back(xml::text_node(string(std::string("\x02\xEF\xBF\xBE\xF0\x9F\x98\x80", 8))));
    EXPECT_EQ(controls.to_string(), "<a v=\"x\xEF\xBF\xBDy\xEF\xBF\xBD\">\xEF\xBF\xBD\xEF\xBF\xBD\xF0\x9F\x98\x80</a>");
    // a node that is not an element
    EXPECT_EQ(xml::text_node("a<b").to_string(), "a&lt;b");
    EXPECT_EQ(xml::comment("c").to_string(), "<!--c-->");
    EXPECT_EQ(xml::instruction("t").to_string(), "<?t?>");
    EXPECT_EQ(xml().to_string(), "");
}

// A tree written and read again is the same tree: every document the Go
// oracle names, and the catalog, compact and indented
TEST(XmlTree_Tests, WrittenAndReadAgain) {
    std::vector<std::string> docs{Catalog};
    for (auto& g : GoNamed) {
        docs.push_back(g.doc);
    }
    for (auto& doc : docs) {
        auto t = xml::parse(string(doc));
        ASSERT_TRUE(t.has_value()) << shown(doc);
        for (auto& s : {xml::compact, xml::pretty, xml::style{3, true}}) {
            auto text = t->to_string(s);
            auto again = xml::parse(text);
            ASSERT_TRUE(again.has_value()) << text.view();
            EXPECT_EQ(*again, *t) << shown(doc) << " as " << text.view();
            EXPECT_EQ(again->to_string(s), text);
        }
    }
}

// A tree deeper than any stack: built, compared, written, joined and read
// again with loops of their own, no recursion
TEST(XmlTree_Tests, DeeperThanAStack) {
    const int depth = 200000;
    xml node = xml("leaf", "x");
    for (int i = 0; i < depth; ++i) {
        node = xml("e").push_back(node);
    }
    EXPECT_EQ(node.text(), "x");
    auto text = node.to_string();
    EXPECT_EQ(text.size(), size_t(depth) * 7 + 14);
    xml::options deep;
    deep.max_depth = depth + 1;
    auto again = xml::parse(text, deep);
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(*again, node);
    EXPECT_FALSE(xml::parse(text).has_value());   // past the default 512
}

// A tree held only by a tracked pointer on the stack, walked while the
// collector runs, and read from threads at once: nothing is freed under it,
// nothing is written to it
TEST(XmlTree_Tests, CollectorAndThreads) {
    std::string doc = "<r>";
    for (int i = 0; i < 2000; ++i) {
        doc += "<i n='" + std::to_string(i) + "'>" + std::to_string(i * i) + "</i>";
    }
    doc += "</r>";
    xml tree = xml::parse(string(doc)).value();
    sgcl::collector::force_collect(true);
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&tree, &failures, t] {
            for (int round = 0; round < 5; ++round) {
                long sum = 0;
                for (auto& c : tree.children()) {
                    sum += std::stol(std::string(c.text().view()));
                }
                if (sum != 1999L * 2000 * 3999 / 6) {
                    ++failures;
                }
                if (t == 0) {
                    sgcl::collector::force_collect(true);
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_EQ(failures.load(), 0);
    EXPECT_EQ(xml::parse(tree.to_string()).value(), tree);
}
