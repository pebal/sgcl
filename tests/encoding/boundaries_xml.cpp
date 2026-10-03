//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The boundaries of XML (DESIGN 408): the names and the declarations of
// Namespaces in XML no document may hold, made by hand or written; an
// instruction's data starting with white space; the default, moved-from
// and self-given node and builder; the depth at 0 and past any stack of
// calls; the text of a pretty tree past what a string holds; a token of a
// stream at its exact bound; a stream failing half-way. What the suites
// beside it cover is not repeated: the names that are not names
// (XmlTree_Tests.MakingNodes, XmlWriter_Tests.Mistakes), the attacks and
// the depth of 512 (XmlReader_Tests.Attacks, DepthLimit), a token past its
// bound (TokenSizeLimit), a stream failing (AFailingStream), the W3C suite.
#include "xml_common.h"

#include <functional>

using namespace xml_test;
using namespace enc_test;
using sgcl::encoding::errc;
using sgcl::string;

namespace {
    // What a writer made of one call keeps as its mistake, or none
    template<class F>
    std::optional<errc> mistake(F write) {
        sgcl::tracked_ptr out = make_tracked<sink>();
        xml::writer w(out);
        write(w);
        if (!w.last_error()) {
            return std::nullopt;
        }
        return w.last_error()->code();
    }

    // n elements inside one another, made by hand
    xml nested(size_t n) {
        xml v("a");
        for (size_t i = 1; i < n; ++i) {
            v = xml("a").push_back(v);
        }
        return v;
    }
}

// An element of the prefix xmlns is in no document (Namespaces in XML,
// section 3): refused when made and when written, as the reader refuses it;
// an element named xmlns alone is one
TEST(XmlBoundaries_Tests, AnElementOfThePrefixXmlns) {
    EXPECT_THROW(xml(string("xmlns:a")), std::invalid_argument);
    EXPECT_THROW(xml(string("xmlns:a"), string("t")), std::invalid_argument);
    EXPECT_THROW(xml::builder(string("xmlns:a")), std::invalid_argument);
    EXPECT_EQ(mistake([](xml::writer& w) { w.start("xmlns:a"); }), errc::syntax);
    EXPECT_EQ(error_of(xml::parse("<xmlns:a/>")).code(), errc::syntax);
    xml plain("xmlns");
    EXPECT_EQ(value_of(xml::parse(plain.to_string())), plain);
    EXPECT_EQ(mistake([](xml::writer& w) { w.start("xmlns").end(); }), std::nullopt);
}

// The writer's own test of a name (its first character taken before the
// loop, 'x' told on the way) answers as the test of a QName everywhere
// else: at the first character, ASCII or not, at the colon, at bytes that
// are no UTF-8; a name starting with 'x' is marked, and only such a name
TEST(XmlBoundaries_Tests, TheWritersNameTestIsTheQNameTest) {
    namespace d = sgcl::encoding::detail;
    const std::string names[] = {
        "", "a", "x", "X", "y", "z", "_", ":", "-", ".", "1", "a1", "a-b.c", "xa", "xml", "xmlns", "xmlns:p", "x:y",
        "xlink:href", "a:", ":a", "a::b", "a:b:c", "a:1", "a:-", "a:\xC3\xA9", "\xC3\xA9", "\xC3\xA9t\xC3\xA9", "a\xC2\xB7",
        "\xC2\xB7", "\xE2\x80\xBF", "a\xE2\x80\xBF", "\xF0\x90\x80\x80", "a b", "a\x80", "\x80", "\xC3", "a\xC3", "x\xC3\xA9",
        "\xEF\xBF\xBE", "a\xED\xA0\x80",
    };
    for (auto& n : names) {
        uint8_t kind = d::xml_qname_kind(n);
        bool qname = d::xml_qname(n);
        EXPECT_EQ(kind != d::XmlNameWrong, qname) << n;
        EXPECT_EQ(kind == d::XmlNameX, qname && n[0] == 'x') << n;
        if (n.starts_with("xmlns")) {
            continue;
        }
        auto expected = qname ? std::nullopt : std::optional<errc>(errc::syntax);
        EXPECT_EQ(mistake([&](xml::writer& w) { w.start(string(n)).end(); }), expected) << n;
        EXPECT_EQ(mistake([&](xml::writer& w) { w.start("a").attribute(string(n), "v").end(); }), expected) << n;
    }
}

// A namespace declaration no document may hold is refused by set, by the
// builder and by the writer, as the reader refuses it; the ones it may
// hold are taken and read back
TEST(XmlBoundaries_Tests, DeclarationsNamespacesForbid) {
    const std::string xml_ns = "http://www.w3.org/XML/1998/namespace";
    const std::string xmlns_ns = "http://www.w3.org/2000/xmlns/";
    const std::pair<std::string, std::string> wrong[] = {
        {"xmlns:xmlns", "u"},        // the prefix xmlns declared
        {"xmlns:xml", "u"},          // xml bound elsewhere
        {"xmlns:p", ""},             // a prefix undeclared: XML 1.1's, not 1.0's
        {"xmlns:p", xml_ns},         // another prefix bound to the XML namespace
        {"xmlns", xml_ns},
        {"xmlns:p", xmlns_ns},       // anything bound to the xmlns namespace
        {"xmlns", xmlns_ns},
        {"xmlns:xml", xmlns_ns},
    };
    for (auto& [name, value] : wrong) {
        EXPECT_THROW(xml("a").set(string(name), string(value)), std::invalid_argument) << name << "=" << value;
        xml::builder b("a");
        EXPECT_THROW(b.set(string(name), string(value)), std::invalid_argument) << name << "=" << value;
        EXPECT_EQ(mistake([&](xml::writer& w) { w.start("a").attribute(string(name), string(value)); }), errc::syntax) << name;
        EXPECT_EQ(error_of(xml::parse(string("<a " + name + "='" + value + "'/>"))).code(), errc::syntax) << name;
    }
    const std::pair<std::string, std::string> right[] = {
        {"xmlns:xml", xml_ns},       // xml declared as what it is
        {"xmlns", ""},               // the default namespace undeclared
        {"xmlns:p", "u"},
        {"xmlns", "u"},
    };
    for (auto& [name, value] : right) {
        xml e = xml("a").set(string(name), string(value));
        auto back = xml::parse(e.to_string());
        ASSERT_TRUE(back) << name << ": " << back.error().message().view();
        EXPECT_EQ(*back, e) << name;
        EXPECT_EQ(mistake([&](xml::writer& w) { w.start("a").attribute(string(name), string(value)).end(); }), std::nullopt) << name;
    }
}

// An instruction's data starting with white space would lose it when read
// (the white space after the target is the separator): refused; white space
// at its end is data and reads back
TEST(XmlBoundaries_Tests, AnInstructionsDataAtItsEdges) {
    for (const char* data : {" a", "\ta", "\na", "\r\na", " "}) {
        EXPECT_THROW(xml::instruction("p", data), std::invalid_argument) << shown(data);
        EXPECT_EQ(mistake([&](xml::writer& w) { w.instruction("p", data); }), errc::syntax) << shown(data);
    }
    xml::options keep;
    keep.keep_comments = true;
    for (const char* data : {"a ", "a?", "", "a b"}) {
        xml e = xml("r").push_back(xml::instruction("p", data));
        auto back = xml::parse(e.to_string(), keep);
        ASSERT_TRUE(back) << data;
        EXPECT_EQ(*back, e) << shown(data);
    }
}

// --- the tree ---

TEST(XmlBoundaries_Tests, DefaultMovedAndSelfGivenNodes) {
    xml none;
    EXPECT_FALSE(none.exists());
    EXPECT_EQ(none.type(), xml::kind::none);
    EXPECT_EQ(none.name(), "");
    EXPECT_EQ(none.local_name(), "");
    EXPECT_EQ(none.namespace_uri(), "");
    EXPECT_EQ(none.text(), "");
    EXPECT_EQ(none.to_string(), "");
    EXPECT_FALSE(none.attribute("a"));
    EXPECT_EQ(none.attribute("a", "f"), "f");
    EXPECT_TRUE(none.attributes().empty());
    EXPECT_TRUE(none.children().empty());
    EXPECT_FALSE(none.child("a").exists());
    EXPECT_EQ(none, xml());
    EXPECT_THROW(none.set("a", "1"), std::invalid_argument);
    EXPECT_THROW(none.erase("a"), std::invalid_argument);
    EXPECT_THROW(none.push_back(xml("b")), std::invalid_argument);
    EXPECT_THROW(xml("a").push_back(none), std::invalid_argument);
    // moved from: the node it was (a tracked word's move is a copy)
    xml a = xml("a").set("k", "v");
    xml b = std::move(a);
    EXPECT_EQ(a, b);
    EXPECT_EQ(a.to_string(), "<a k=\"v\"/>");
    // itself as its own child, assigned to itself
    xml twice = b.push_back(b);
    EXPECT_EQ(twice.to_string(), "<a k=\"v\"><a k=\"v\"/></a>");
    auto& same = b;
    b = same;
    EXPECT_EQ(b.to_string(), "<a k=\"v\"/>");
    // erase of what is not there is the same element; an empty attribute value
    EXPECT_EQ(b.erase("x"), b);
    EXPECT_EQ(xml("a").set("v", "").to_string(), "<a v=\"\"/>");
}

// A builder moved from is as build() leaves it: its name, nothing else
TEST(XmlBoundaries_Tests, ABuilderMovedFromAndBuiltTwice) {
    xml::builder b("list");
    b.set("n", "1").push_back(xml("item"));
    xml::builder c = std::move(b);
    EXPECT_EQ(b.build().to_string(), "<list/>");
    EXPECT_EQ(c.build().to_string(), "<list n=\"1\"><item/></list>");
    EXPECT_EQ(c.build().to_string(), "<list/>");
    EXPECT_THROW(c.push_back(xml()), std::invalid_argument);
    c.set("n", "1").set("n", "2");
    EXPECT_EQ(c.build().to_string(), "<list n=\"2\"/>");
}

// A depth of 0 takes no element; a tree deeper than any stack of calls,
// read with the bound lifted or made by hand, is written, compared and
// its text gathered with no recursion
TEST(XmlBoundaries_Tests, DepthAtItsEdges) {
    xml::options zero;
    zero.max_depth = 0;
    EXPECT_EQ(error_of(xml::parse("<a/>", zero)).code(), errc::depth_limit);
    const size_t deep = 100000;
    std::string doc;
    for (size_t i = 0; i < deep; ++i) {
        doc += "<a>";
    }
    doc += "t";
    for (size_t i = 0; i < deep; ++i) {
        doc += "</a>";
    }
    xml::options lifted;
    lifted.max_depth = UINT32_MAX;
    auto read = xml::parse(string(doc), lifted);
    ASSERT_TRUE(read);
    EXPECT_EQ(read->text(), "t");
    EXPECT_EQ(read->to_string(), doc);
    xml made = nested(deep);
    EXPECT_EQ(made, nested(deep));
    EXPECT_NE(made, *read);
    EXPECT_EQ(made.to_string().size(), deep * 7 - 3);
}

// A pretty text past the 4 GiB a string holds is length_error, found as
// the text passes it: the writing stops there, not after a text of
// terabytes (a tree 100000 deep indented by 255 would be 1.3e12 characters)
TEST(XmlBoundaries_Tests, APrettyTextPastTheStringLimit) {
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
    GTEST_SKIP() << "4 GiB of text under a sanitizer";
#endif
#endif
    xml v = nested(100000);
    EXPECT_THROW((void)v.to_string(xml::style{255, false}), std::length_error);
    EXPECT_EQ(v.to_string().size(), 100000u * 7 - 3);
}

// --- the reader ---

// A token of exactly max_token_size bytes is read however the stream hands
// it out; held past the bound and asking for more, out_of_range
TEST(XmlBoundaries_Tests, ATokenAtTheExactBound) {
    xml::options o;
    o.max_token_size = 5;
    for (size_t piece : {size_t(1), size_t(2), size_t(4096)}) {
        EXPECT_TRUE(dump_stream("<a>xxxxx</a>", piece, o).ok) << piece;
    }
    auto past = dump_stream("<a>xxxxxx</a>", 1, o);
    ASSERT_FALSE(past.ok);
    EXPECT_EQ(past.error->code(), errc::out_of_range);
    xml::options zero;
    zero.max_token_size = 0;
    xml::reader nothing(make_tracked<dribble>(std::string(), 1), zero);
    EXPECT_FALSE(nothing.next());
    ASSERT_TRUE(nothing.last_error());   // no root element: the error of an empty document
    EXPECT_NE(nothing.last_error()->code(), errc::out_of_range);
}

// A document of nothing, of a declaration alone, and cut at every kind of
// token: no root, and the end of the document inside the token
TEST(XmlBoundaries_Tests, EmptyAndCutDocuments) {
    for (const char* doc : {"", " ", "<?xml version=\"1.0\"?>", "<!-- c -->"}) {
        auto r = xml::parse(doc);
        ASSERT_FALSE(r) << doc;
        EXPECT_EQ(r.error().code(), errc::unexpected_end) << doc << ": " << r.error().message().view();
    }
    for (const char* doc : {"<a", "<a>", "<a b='", "<a>&am", "<a><!--x", "<a><![CDATA[x", "<a><?p x", "<a></a"}) {
        for (size_t piece : {size_t(1), size_t(4096)}) {
            auto d = dump_stream(doc, piece);
            ASSERT_FALSE(d.ok) << doc;
            EXPECT_EQ(d.error->code(), errc::unexpected_end) << doc << ": " << d.error->message().view();
            EXPECT_EQ(d.error->offset(), std::string_view(doc).size()) << doc;
        }
    }
}
