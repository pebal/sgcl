//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The XML reader: tokens of named documents written out by hand, errors
// with their code, offset, line, column and path, the encodings a
// declaration or a byte order mark names, the attacks a reader of
// someone else's input must survive, the limits, and a stream cut into
// pieces of every small size giving what the whole document gives.
#include "xml_common.h"
#include "sgcl/txt/encoding.h"

#include <chrono>
#include <string>

using namespace sgcl::encoding;
using namespace xml_test;

namespace {
    struct Case {
        const char* doc;
        const char* tokens;
    };

    // Documents and their tokens, one a line (xml_common.h: line())
    const Case Tokens[] = {
        {"<a/>", "S {}a\nE {}a\n"},
        {"<?xml version=\"1.0\"?><a b=\"1\" c='2'>x</a>",
         "P xml \"version=\\x221.0\\x22\"\nS {}a {}b=\"1\" {}c=\"2\"\nT \"x\"\nE {}a\n"},
        {"<?xml version='1.0' encoding='UTF-8' standalone='yes' ?><a/>",
         "P xml \"version='1.0' encoding='UTF-8' standalone='yes' \"\nS {}a\nE {}a\n"},
        {"<p:a xmlns:p=\"urn:p\" xmlns=\"urn:d\"><b p:x=\"1\" y=\"2\"/></p:a>",
         "S {urn:p}a {xmlns}p=\"urn:p\" {}xmlns=\"urn:d\"\nS {urn:d}b {urn:p}x=\"1\" {}y=\"2\"\nE {urn:d}b\nE {urn:p}a\n"},
        // the default namespace undeclared inside, a prefix bound again
        {"<a xmlns=\"urn:1\" xmlns:p=\"urn:p\"><b xmlns=\"\"><p:c xmlns:p=\"urn:q\"/><p:d/></b><e/></a>",
         "S {urn:1}a {}xmlns=\"urn:1\" {xmlns}p=\"urn:p\"\nS {}b {}xmlns=\"\"\nS {urn:q}c {xmlns}p=\"urn:q\"\nE {urn:q}c\n"
         "S {urn:p}d\nE {urn:p}d\nE {}b\nS {urn:1}e\nE {urn:1}e\nE {urn:1}a\n"},
        {"<x:a xmlns:x=\"urn:x\" xml:lang=\"pl\"/>",
         "S {urn:x}a {xmlns}x=\"urn:x\" {http://www.w3.org/XML/1998/namespace}lang=\"pl\"\nE {urn:x}a\n"},
        {"<a>&lt;&gt;&amp;&apos;&quot;&#65;&#x42;&#x1F600;&#169;</a>",
         "S {}a\nT \"<>&'\\x22AB\xF0\x9F\x98\x80\xC2\xA9\"\nE {}a\n"},
        {"<a>x<![CDATA[<y>&amp;]]]]>z</a>", "S {}a\nT \"x<y>&amp;]]z\"\nE {}a\n"},
        {"<!--c--><a><?pi data here?><?empty?></a><!--d-->",
         "C \"c\"\nS {}a\nP pi \"data here\"\nP empty \"\"\nE {}a\nC \"d\"\n"},
        {"<a>1\r\n2\r3\n\r\n</a>", "S {}a\nT \"1\\x0a2\\x0a3\\x0a\\x0a\"\nE {}a\n"},
        {"<!DOCTYPE a [<!ELEMENT a ANY><!-- ] > --><!ENTITY x \"]>\"><?p ]>?> %pe;]><a/>", "D a\nS {}a\nE {}a\n"},
        {"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"><html/>",
         "D html\nS {}html\nE {}html\n"},
        {"<?xml version='1.0'?>\n<a/>\n", "P xml \"version='1.0'\"\nT \"\\x0a\"\nS {}a\nE {}a\nT \"\\x0a\"\n"},
        {"\xEF\xBB\xBF<a/>", "S {}a\nE {}a\n"},
        {"\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?><a/>", "P xml \"version=\\x221.0\\x22 encoding=\\x22utf-8\\x22\"\nS {}a\nE {}a\n"},
        {"<a  b = \"1\"\n\tc\r\n=\r\n'2' ></a >", "S {}a {}b=\"1\" {}c=\"2\"\nE {}a\n"},
        {"<\xC5\xBC\xC3\xB3\xC5\x82w \xC4\x85=\"\xC4\x99\"/>", "S {}\xC5\xBC\xC3\xB3\xC5\x82w {}\xC4\x85=\"\xC4\x99\"\nE {}\xC5\xBC\xC3\xB3\xC5\x82w\n"},
        {"<a>]]</a>", "S {}a\nT \"]]\"\nE {}a\n"},
        {"<a>]</a>", "S {}a\nT \"]\"\nE {}a\n"},
        {"<a b=\"'\" c='\"'>\"'</a>", "S {}a {}b=\"'\" {}c=\"\\x22\"\nT \"\\x22'\"\nE {}a\n"},
        {"<a><!----></a>", "S {}a\nC \"\"\nE {}a\n"},
        {"<a>\xEF\xBF\xBD\xF4\x8F\xBF\xBD</a>", "S {}a\nT \"\xEF\xBF\xBD\xF4\x8F\xBF\xBD\"\nE {}a\n"},
    };

    struct Failure {
        const char* doc;
        errc code;
        uint64_t offset;
        uint32_t line;
        uint32_t column;
        const char* path;
    };

    // Documents that are not well formed, and where each fails
    const Failure Failures[] = {
        {"", errc::unexpected_end, 0, 1, 1, ""},
        {"   ", errc::unexpected_end, 3, 1, 4, ""},
        {"<a>", errc::unexpected_end, 3, 1, 4, "/a"},
        {"<a", errc::unexpected_end, 2, 1, 3, ""},
        {"<a b=\"1", errc::unexpected_end, 7, 1, 8, ""},
        {"<a></b>", errc::mismatched_tag, 3, 1, 4, "/a"},
        {"</a>", errc::mismatched_tag, 0, 1, 1, ""},
        {"<a>&nbsp;</a>", errc::undefined_entity, 3, 1, 4, "/a"},
        {"<a>&#0;</a>", errc::invalid_escape, 3, 1, 4, "/a"},
        {"<a>&#xD800;</a>", errc::invalid_escape, 3, 1, 4, "/a"},
        {"<a>&#xFFFE;</a>", errc::invalid_escape, 3, 1, 4, "/a"},
        {"<a>&#x110000;</a>", errc::invalid_escape, 3, 1, 4, "/a"},
        {"<a>&#99999999999999999999;</a>", errc::invalid_escape, 3, 1, 4, "/a"},
        {"<a>&#;</a>", errc::invalid_escape, 3, 1, 4, "/a"},
        {"<a>&#x41</a>", errc::invalid_escape, 3, 1, 4, "/a"},
        {"<a>&amp</a>", errc::syntax, 3, 1, 4, "/a"},
        {"<a>& b</a>", errc::syntax, 3, 1, 4, "/a"},
        {"<a x=\"1\" x=\"2\"/>", errc::duplicate_key, 9, 1, 10, ""},
        {"<a p:x=\"1\"/>", errc::syntax, 3, 1, 4, ""},
        {"<p:a/>", errc::syntax, 1, 1, 2, ""},
        {"<a xmlns:p=\"u\" xmlns:q=\"u\" p:x=\"1\" q:x=\"2\"/>", errc::duplicate_key, 35, 1, 36, ""},
        {"<a>]]></a>", errc::syntax, 3, 1, 4, "/a"},
        {"<a/><b/>", errc::syntax, 4, 1, 5, ""},
        {"<a/>x", errc::syntax, 4, 1, 5, ""},
        {"x<a/>", errc::syntax, 0, 1, 1, ""},
        {"<a/>&amp;", errc::syntax, 4, 1, 5, ""},
        {"<a>\x01</a>", errc::invalid_character, 3, 1, 4, "/a"},
        {"<a>\xC3\x28</a>", errc::invalid_utf8, 3, 1, 4, "/a"},
        {"<a>\xED\xA0\x80</a>", errc::invalid_utf8, 3, 1, 4, "/a"},
        {"<a>\xC0\xAF</a>", errc::invalid_utf8, 3, 1, 4, "/a"},
        {"<a>\xEF\xBF\xBE</a>", errc::invalid_character, 3, 1, 4, "/a"},
        {"<a b=\"\x02\"/>", errc::invalid_character, 6, 1, 7, ""},
        {"<!-- a -- b --><a/>", errc::syntax, 7, 1, 8, ""},
        {"<!-- a ---><a/>", errc::syntax, 7, 1, 8, ""},
        {" <?xml version=\"1.0\"?><a/>", errc::syntax, 1, 1, 2, ""},
        {"<a/><?xml version=\"1.0\"?>", errc::syntax, 4, 1, 5, ""},
        {"<?XML version='1.0'?><a/>", errc::syntax, 0, 1, 1, ""},
        {"<?xml encoding='UTF-8'?><a/>", errc::syntax, 6, 1, 7, ""},
        {"<?xml version='2.0'?><a/>", errc::syntax, 5, 1, 6, ""},
        {"<?xml version='1.0' standalone='maybe'?><a/>", errc::syntax, 32, 1, 33, ""},
        {"<?xml version='1.0'encoding='UTF-8'?><a/>", errc::syntax, 19, 1, 20, ""},
        {"<a b=1/>", errc::syntax, 5, 1, 6, ""},
        {"<a b/>", errc::syntax, 4, 1, 5, ""},
        {"<a b=\"<\"/>", errc::syntax, 6, 1, 7, ""},
        {"<a b=\"1\"c=\"2\"/>", errc::syntax, 8, 1, 9, ""},
        {"<a xmlns:xml=\"urn:other\"/>", errc::syntax, 3, 1, 4, ""},
        {"<a xmlns:p=\"\"/>", errc::syntax, 3, 1, 4, ""},
        {"<a xmlns:xmlns=\"urn:x\"/>", errc::syntax, 3, 1, 4, ""},
        {"<a xmlns=\"http://www.w3.org/XML/1998/namespace\"/>", errc::syntax, 3, 1, 4, ""},
        {"<xmlns:a/>", errc::syntax, 1, 1, 2, ""},
        {"<a:b:c xmlns:a='u'/>", errc::syntax, 1, 1, 2, ""},
        {"<:a/>", errc::syntax, 1, 1, 2, ""},
        {"<a:/>", errc::syntax, 1, 1, 2, ""},
        {"<?a:b x?><a/>", errc::syntax, 2, 1, 3, ""},
        {"<a/ >", errc::syntax, 2, 1, 3, ""},
        {"<a></a b>", errc::syntax, 7, 1, 8, "/a"},
        {"< a/>", errc::syntax, 1, 1, 2, ""},
        {"<a><![CDATA[x]]</a>", errc::unexpected_end, 19, 1, 20, "/a"},
        {"<![CDATA[x]]><a/>", errc::syntax, 0, 1, 1, ""},
        {"<a><![CDATA x]]></a>", errc::syntax, 3, 1, 4, "/a"},
        {"<a/><!DOCTYPE a>", errc::syntax, 4, 1, 5, ""},
        {"<!DOCTYPE a><!DOCTYPE a><a/>", errc::syntax, 12, 1, 13, ""},
        {"<!DOCTYPE a [ <!FOO x> ]><a/>", errc::syntax, 14, 1, 15, ""},
        {"<!DOCTYPE a [ junk ]><a/>", errc::syntax, 14, 1, 15, ""},
        {"<!DOCTYPE a PUBLIC \"a{b\" \"c\"><a/>", errc::syntax, 21, 1, 22, ""},
        {"<!DOCTYPEa><a/>", errc::syntax, 9, 1, 10, ""},
        {"<a>\n  <b>\n    &x;\n</b></a>", errc::undefined_entity, 14, 3, 5, "/a/b"},
        {"<a>\xC4\x85\xC4\x99&x;</a>", errc::undefined_entity, 7, 1, 6, "/a"},
        {"<a>\r\n<b></c></b></a>", errc::mismatched_tag, 8, 2, 4, "/a/b"},
    };
}

TEST(XmlReader_Tests, NamedDocuments) {
    for (auto& c : Tokens) {
        auto d = dump(c.doc);
        EXPECT_TRUE(d.ok) << shown(c.doc) << ": " << (d.error ? std::string(d.error->message().view()) : "");
        EXPECT_EQ(d.tokens, c.tokens) << shown(c.doc);
    }
}

TEST(XmlReader_Tests, Failures) {
    for (auto& c : Failures) {
        auto d = dump(c.doc);
        ASSERT_FALSE(d.ok) << shown(c.doc);
        auto& e = *d.error;
        EXPECT_EQ(e.code(), c.code) << shown(c.doc) << ": " << e.message().view();
        EXPECT_EQ(e.offset(), c.offset) << shown(c.doc) << ": " << e.message().view();
        EXPECT_EQ(e.line(), c.line) << shown(c.doc);
        EXPECT_EQ(e.column(), c.column) << shown(c.doc);
        EXPECT_EQ(std::string(e.path().view()), c.path) << shown(c.doc);
    }
}

// Every document above, and the failures, read through a stream handing
// out 1, 2, 3, 7 and 4096 bytes a read: the same tokens, the same error at
// the same place
TEST(XmlReader_Tests, AStreamInPiecesIsTheWholeDocument) {
    std::vector<std::string> docs;
    for (auto& c : Tokens) {
        docs.push_back(c.doc);
    }
    for (auto& c : Failures) {
        docs.push_back(c.doc);
    }
    // one larger document with every kind of token, cut everywhere
    std::string big = "\xEF\xBB\xBF<?xml version=\"1.0\"?>\r\n<!DOCTYPE r [ <!ENTITY e \"x\"> <!-- ]> --> <?pi ?> ]>\n<!-- pro -->\n"
                      "<r xmlns=\"urn:r\" xmlns:q=\"urn:q\">";
    for (int i = 0; i < 40; ++i) {
        big += "<q:item n=\"" + std::to_string(i) + "\" v='&lt;\xC5\xBC&#x1F600;\r\n'>t\xC4\x85&amp;\r\n<![CDATA[c]]]>d]]&gt;"
               "<!--c-->\xF0\x9F\x98\x80<?pi x?><e/></q:item>\n";
    }
    big += "</r>\n<!--end-->";
    docs.push_back(big);
    for (auto& doc : docs) {
        auto whole = dump(doc);
        for (size_t piece : {1, 2, 3, 7, 4096}) {
            auto cut = dump_stream(doc, piece);
            EXPECT_EQ(cut.tokens, whole.tokens) << shown(doc) << " in pieces of " << piece;
            EXPECT_EQ(cut.ok, whole.ok) << shown(doc) << " in pieces of " << piece;
            if (whole.error && cut.error) {
                EXPECT_EQ(*cut.error, *whole.error) << shown(doc) << " in pieces of " << piece << ": "
                    << cut.error->message().view() << " / " << whole.error->message().view();
            }
        }
    }
    auto checked = dump(big);
    EXPECT_TRUE(checked.ok) << (checked.error ? std::string(checked.error->message().view()) : "");
}

// Text in an attribute's value is normalized (XML 1.0, 3.3.3): each
// literal tab, line feed and carriage return — a CR LF pair as one — is a
// space; a character reference to one is that character. Go keeps the
// literal ones as they are.
TEST(XmlReader_Tests, AttributeValuesAreNormalized) {
    xml::reader r(string("<a x=\"1&#9;2\t3\n4\r\n5\r6&#10;7&#13;8\"/>"));
    auto t = r.next();
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->attribute("x").value(), "1\t2 3 4 5 6\n7\r8");
    EXPECT_EQ(line(*t, false), "S {}a {}x=\"1\\x092 3 4 5 6\\x0a7\\x0d8\"");
}

TEST(XmlReader_Tests, TokensSayWhatTheyAre) {
    xml::reader r(string("<?xml version='1.0'?><!DOCTYPE d SYSTEM 'd.dtd'><s:d xmlns:s='urn:s' s:a='1' b='2'><!--c-->t<?p q?></s:d>"));
    auto decl = r.next().value();
    EXPECT_EQ(decl.type(), xml::token::kind::instruction);
    EXPECT_EQ(decl.name(), "xml");
    EXPECT_EQ(decl.text(), "version='1.0'");
    auto doctype = r.next().value();
    EXPECT_EQ(doctype.type(), xml::token::kind::doctype);
    EXPECT_EQ(doctype.name(), "d");
    EXPECT_EQ(doctype.text(), "d SYSTEM 'd.dtd'");
    auto start = r.next().value();
    EXPECT_TRUE(start.is_start("s:d"));
    EXPECT_TRUE(start.is_start("{urn:s}d"));
    EXPECT_FALSE(start.is_start("d"));
    EXPECT_FALSE(start.is_end("s:d"));
    EXPECT_EQ(start.local_name(), "d");
    EXPECT_EQ(start.namespace_uri(), "urn:s");
    EXPECT_EQ(start.attributes().size(), 3u);
    EXPECT_EQ(start.attribute("s:a").value(), "1");
    EXPECT_EQ(start.attribute("{urn:s}a").value(), "1");
    EXPECT_EQ(start.attribute("{}b").value(), "2");
    EXPECT_EQ(start.attribute("b").value(), "2");
    EXPECT_FALSE(start.attribute("a").has_value());
    EXPECT_EQ(start.attribute("{http://www.w3.org/2000/xmlns/}s").value(), "urn:s");
    EXPECT_EQ(r.depth(), 1u);
    auto comment = r.next().value();
    EXPECT_EQ(comment.type(), xml::token::kind::comment);
    EXPECT_EQ(comment.text(), "c");
    EXPECT_EQ(r.next().value().text(), "t");
    auto pi = r.next().value();
    EXPECT_EQ(pi.name(), "p");
    EXPECT_EQ(pi.text(), "q");
    auto end = r.next().value();
    EXPECT_TRUE(end.is_end("{urn:s}d"));
    EXPECT_EQ(r.depth(), 0u);
    EXPECT_FALSE(r.next().has_value());
    EXPECT_FALSE(r.last_error().has_value());
    EXPECT_FALSE(r.next().has_value());   // the end stays the end
}

// peek() leaves the token for next(); read() gives a node whole and stops
// at the end of the element it is in; skip() passes over what read() would
// give; offset() and depth() are those of the token in front
TEST(XmlReader_Tests, PeekReadAndSkip) {
    xml::reader r(string("<?xml version='1.0'?>\n<!--p--><list>\n  <item id='1'>a<b/>c</item>\n  <!--x-->\n  <item id='2'/>\n  text <![CDATA[more]]> here\n  <item id='3'/>\n</list>"));
    auto p = r.peek();
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p->name(), "xml");
    EXPECT_EQ(r.offset(), 0u);
    EXPECT_EQ(r.next()->name(), "xml");
    // read() at the top: the declaration, the white space and the comment
    // are left out; the root comes whole
    xml::reader whole(string("<?xml version='1.0'?>\n<!--p--><list><i/></list>\n<!--after-->"));
    auto root = whole.read();
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(root->name(), "list");
    EXPECT_EQ(root->children().size(), 1u);
    EXPECT_FALSE(whole.read().has_value());
    EXPECT_FALSE(whole.last_error().has_value());

    // into <list>, then its children one by one
    while (auto t = r.peek()) {
        if (t->is_start("list")) {
            break;
        }
        r.next();
    }
    EXPECT_EQ(r.depth(), 0u);
    r.next();
    EXPECT_EQ(r.depth(), 1u);
    auto first = r.read();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->attribute("id").value(), "1");
    EXPECT_EQ(first->text(), "ac");
    EXPECT_EQ(first->children().size(), 3u);
    auto second = r.read();   // the comment and the white space are left out
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->attribute("id").value(), "2");
    auto text = r.read();     // text, CDATA, text: one text node
    ASSERT_TRUE(text.has_value());
    EXPECT_TRUE(text->is_text());
    EXPECT_EQ(text->text(), "\n  text more here\n  ");
    EXPECT_TRUE(r.skip());    // <item id='3'/>
    EXPECT_FALSE(r.read().has_value());   // </list>: stays
    EXPECT_FALSE(r.skip());
    EXPECT_FALSE(r.last_error().has_value());
    auto end = r.next();
    ASSERT_TRUE(end.has_value());
    EXPECT_TRUE(end->is_end("list"));
    EXPECT_FALSE(r.next().has_value());

    // skip() over whole elements and over text
    xml::reader s(string("<a><b><c/>x</b>  y  <d/></a>"));
    s.next();
    EXPECT_TRUE(s.skip());    // <b>...</b>
    EXPECT_TRUE(s.skip());    // "  y  "
    EXPECT_EQ(s.peek()->name(), "d");
    EXPECT_TRUE(s.skip());
    EXPECT_FALSE(s.skip());
    EXPECT_TRUE(s.next()->is_end("a"));

    // options: comments and white space kept
    xml::options keep;
    keep.keep_comments = true;
    keep.keep_whitespace = true;
    xml::reader k(string("<a> <!--c--> <b/></a>"), keep);
    k.next();
    EXPECT_EQ(k.read()->text(), " ");
    EXPECT_EQ(k.read()->type(), xml::kind::comment);
    EXPECT_EQ(k.read()->text(), " ");
    EXPECT_EQ(k.read()->name(), "b");
    EXPECT_FALSE(k.read().has_value());

    // an error inside read(): nullopt, and last_error() says where
    xml::reader bad(string("<a><b>&x;</b></a>"));
    bad.next();
    EXPECT_FALSE(bad.read().has_value());
    ASSERT_TRUE(bad.last_error().has_value());
    EXPECT_EQ(bad.last_error()->code(), errc::undefined_entity);
    EXPECT_EQ(bad.last_error()->path(), "/a/b");
}

TEST(XmlReader_Tests, OffsetsOfTokens) {
    std::string doc = "<a>\n <b x='1'>t</b><!--c--></a>";
    xml::reader r{string(doc)};
    std::vector<uint64_t> at;
    for (;;) {
        uint64_t o = r.offset();
        if (!r.next()) {
            break;
        }
        at.push_back(o);
    }
    EXPECT_EQ(at, (std::vector<uint64_t>{0, 3, 5, 14, 15, 19, 27}));
}

// A document in UTF-16 (little or big endian, with a byte order mark or
// with the '<?' of its declaration) or in a single byte encoding its
// declaration names reads as the same document in UTF-8; the offset of an
// error counts the bytes of the input as it was
TEST(XmlReader_Tests, Encodings) {
    std::string body = "<a x='za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87'>ge\xC5\x9Bl\xC4\x85 ja\xC5\xBA\xC5\x84</a>";
    auto utf8 = dump("<?xml version='1.0'?>" + body);
    ASSERT_TRUE(utf8.ok);
    auto bytes = [](const sgcl::vector<byte>& v) { return std::string(reinterpret_cast<const char*>(v.data()), v.size()); };
    auto check = [&](const std::string& doc, const std::string& decl, const std::string& what) {
        std::string expected = "P xml " + shown(decl) + "\n" + utf8.tokens.substr(utf8.tokens.find('\n') + 1);
        for (size_t piece : {1, 3, 7, 4096}) {
            auto d = dump_stream(doc, piece);
            EXPECT_TRUE(d.ok) << what << " in pieces of " << piece << ": " << (d.error ? std::string(d.error->message().view()) : "");
            EXPECT_EQ(d.tokens, expected) << what << " in pieces of " << piece;
        }
        auto d = dump(doc);
        EXPECT_TRUE(d.ok) << what;
        EXPECT_EQ(d.tokens, expected) << what;
    };
    for (auto [enc, bom, name] : {std::tuple{txt::encoding::utf16le, std::string("\xFF\xFE"), "UTF-16"},
                                  std::tuple{txt::encoding::utf16be, std::string("\xFE\xFF"), "utf-16"}}) {
        std::string decl = std::string("version='1.0' encoding='") + name + "'";
        std::string text = "<?xml " + decl + "?>" + body;
        auto raw = bytes(txt::encode(string(text), enc));
        check(bom + raw, decl, std::string("UTF-16 with its mark, ") + name);
        check(raw, decl, std::string("UTF-16 without a mark, ") + name);
        // an emoji: a surrogate pair, and one cut from its pair
        auto pair = bytes(txt::encode(string("<a>\xF0\x9F\x98\x80</a>"), enc));
        auto d = dump(bom + pair);
        EXPECT_TRUE(d.ok);
        EXPECT_EQ(d.tokens, "S {}a\nT \"\xF0\x9F\x98\x80\"\nE {}a\n");
        std::string lone = bom + pair.substr(0, 6) + pair.substr(8);   // the high surrogate alone
        auto e = dump(lone);
        ASSERT_FALSE(e.ok);
        EXPECT_EQ(e.error->code(), errc::invalid_character);
        EXPECT_EQ(e.error->offset(), 8u);
        for (size_t piece : {1, 3}) {
            auto s = dump_stream(lone, piece);
            ASSERT_FALSE(s.ok);
            EXPECT_EQ(s.error->offset(), 8u);
            EXPECT_EQ(s.tokens, "S {}a\n");
        }
        // the offset of an error in the input's bytes: the mark, three
        // characters of two bytes each
        auto undefined = dump(bom + bytes(txt::encode(string("<a>\xC5\xBC&x;</a>"), enc)));
        ASSERT_FALSE(undefined.ok);
        EXPECT_EQ(undefined.error->code(), errc::undefined_entity);
        EXPECT_EQ(undefined.error->offset(), 2u + 2 * 4);
        EXPECT_EQ(undefined.error->column(), 5u);
        // a document cut in the middle of a character
        auto cut = bom + pair.substr(0, 7);
        EXPECT_EQ(dump(cut).error->code(), errc::unexpected_end);
        EXPECT_EQ(dump_stream(cut, 1).error->code(), errc::unexpected_end);
    }
    for (auto [enc, name] : {std::tuple{txt::encoding::iso8859_2, "ISO-8859-2"}, std::tuple{txt::encoding::windows1250, "windows-1250"},
                             std::tuple{txt::encoding::iso8859_2, "latin2"}}) {
        std::string decl = std::string("version='1.0' encoding='") + name + "'";
        auto raw = bytes(txt::encode(string("<?xml " + decl + "?>" + body), enc));
        EXPECT_EQ(raw.size(), string("<?xml " + decl + "?>" + body).rune_count());   // a byte a character
        check(raw, decl, name);
        // the offset of an error counts the input's bytes, a byte a character
        auto undefined = bytes(txt::encode(string("<?xml " + decl + "?><a>\xC5\xBC\xC5\xBC&x;</a>"), enc));
        auto d = dump(undefined);
        ASSERT_FALSE(d.ok);
        EXPECT_EQ(d.error->offset(), 8 + decl.size() + 5);
        EXPECT_EQ(d.error->column(), 8 + decl.size() + 6);
        auto s = dump_stream(undefined, 1);
        ASSERT_FALSE(s.ok);
        EXPECT_EQ(s.error->offset(), 8 + decl.size() + 5);
    }
    // what is refused
    const std::pair<std::string, errc> refused[] = {
        {"<?xml version='1.0' encoding='Shift_JIS'?><a/>", errc::unsupported_encoding},
        {"<?xml version='1.0' encoding='UTF-16'?><a/>", errc::unsupported_encoding},
        {"<?xml version='1.0' encoding='utf-32'?><a/>", errc::unsupported_encoding},
        {"<?xml version='1.0' encoding='bogus-8'?><a/>", errc::unsupported_encoding},
        {"\xEF\xBB\xBF<?xml version='1.0' encoding='ISO-8859-2'?><a/>", errc::unsupported_encoding},
        {std::string("\xFF\xFE\0\0<\0\0\0", 8), errc::unsupported_encoding},
        {std::string("\0\0\0<", 4), errc::unsupported_encoding},
        {"\x4C\x6F\xA7\x94", errc::unsupported_encoding},
        {"<?xml version='1.0' encoding='8859-2'?><a/>", errc::syntax},
    };
    for (auto& [doc, code] : refused) {
        auto d = dump(doc);
        ASSERT_FALSE(d.ok) << shown(doc);
        EXPECT_EQ(d.error->code(), code) << shown(doc) << ": " << d.error->message().view();
    }
    // UTF-16 declaring another encoding
    auto mismatch = std::string("\xFF\xFE") + bytes(txt::encode(string("<?xml version='1.0' encoding='ISO-8859-1'?><a/>"), txt::encoding::utf16le));
    EXPECT_EQ(dump(mismatch).error->code(), errc::unsupported_encoding);
}

// What a reader of someone else's input must survive: no DTD is read, so
// no entity is declared, none is loaded from outside, none expands
TEST(XmlReader_Tests, Attacks) {
    std::string laughs = "<?xml version=\"1.0\"?>\n<!DOCTYPE lolz [\n <!ENTITY lol \"lol\">\n";
    laughs += " <!ENTITY lol1 \"&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;\">\n";
    for (int i = 2; i <= 9; ++i) {
        laughs += " <!ENTITY lol" + std::to_string(i) + " \"";
        for (int k = 0; k < 10; ++k) {
            laughs += "&lol" + std::to_string(i - 1) + ";";
        }
        laughs += "\">\n";
    }
    laughs += "]>\n<lolz>&lol9;</lolz>";
    const std::pair<std::string, const char*> attacks[] = {
        {laughs, "billion laughs"},
        {"<!DOCTYPE foo [<!ENTITY xxe SYSTEM \"file:///etc/passwd\">]><foo>&xxe;</foo>", "XXE"},
        {"<!DOCTYPE foo [<!ENTITY xxe SYSTEM \"http://example.com/evil\">]><foo a=\"&xxe;\"/>", "XXE in an attribute"},
        {"<!DOCTYPE foo SYSTEM \"http://example.com/evil.dtd\"><foo>&e;</foo>", "an external DTD's entity"},
        {"<!DOCTYPE foo [<!ENTITY a \"" + std::string(100000, 'a') + "\">]><foo>" + [] { std::string s; for (int i = 0; i < 1000; ++i) s += "&a;"; return s; }() + "</foo>", "quadratic blowup"},
    };
    for (auto& [doc, what] : attacks) {
        auto t0 = std::chrono::steady_clock::now();
        auto d = dump(doc);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        ASSERT_FALSE(d.ok) << what;
        EXPECT_EQ(d.error->code(), errc::undefined_entity) << what << ": " << d.error->message().view();
        EXPECT_LT(ms, 1000) << what;
        auto tree = xml::parse(string(doc));
        EXPECT_FALSE(tree.has_value());
    }
    // a parameter entity is never interpreted: the subset is skipped, the
    // document is what it is
    auto pe = dump("<!DOCTYPE foo [<!ENTITY % p SYSTEM \"http://example.com/evil.dtd\"> %p;]><foo/>");
    EXPECT_TRUE(pe.ok);
    EXPECT_EQ(pe.tokens, "D foo\nS {}foo\nE {}foo\n");
}

TEST(XmlReader_Tests, DepthLimit) {
    for (uint32_t limit : {1u, 10u, 512u}) {
        xml::options o;
        o.max_depth = limit;
        std::string ok, over;
        for (uint32_t i = 0; i < limit; ++i) {
            ok += "<e>";
        }
        for (uint32_t i = 0; i < limit; ++i) {
            ok += "</e>";
        }
        over = "<e>" + ok + "</e>";
        EXPECT_TRUE(dump(ok, o).ok) << limit;
        EXPECT_TRUE(xml::parse(string(ok), o).has_value()) << limit;
        auto d = dump(over, o);
        ASSERT_FALSE(d.ok);
        EXPECT_EQ(d.error->code(), errc::depth_limit);
        EXPECT_EQ(d.error->offset(), 3 * limit);
        EXPECT_FALSE(xml::parse(string(over), o).has_value());
    }
    // the default is 512
    std::string deep;
    for (int i = 0; i < 513; ++i) {
        deep += "<e>";
    }
    EXPECT_EQ(dump(deep).error->code(), errc::depth_limit);
}

// A token held longer than options::max_token_size while a stream brings
// it is out_of_range; a document in memory is there already and has no
// such limit
TEST(XmlReader_Tests, TokenSizeLimit) {
    xml::options o;
    o.max_token_size = 64 * 1024;
    for (std::string doc : {"<a>" + std::string(100000, 'x') + "</a>", "<a b='" + std::string(100000, 'x') + "'/>",
                            "<a><!--" + std::string(100000, 'x') + "--></a>"}) {
        auto d = dump_stream(doc, 4096, o);
        ASSERT_FALSE(d.ok);
        EXPECT_EQ(d.error->code(), errc::out_of_range) << d.error->message().view();
        EXPECT_TRUE(dump(doc, o).ok);
    }
    std::string fits = "<a>" + std::string(60000, 'x') + "</a>";
    EXPECT_TRUE(dump_stream(fits, 4096, o).ok);
}

// A large token given a byte at a time: the finder follows the new bytes
// and the token is read once, not once a byte — linear, where a reader
// that reads again from the token's start on each byte takes 10^10 steps
TEST(XmlReader_Tests, ALargeTokenFedSlowlyIsReadOnce) {
    const size_t n = 200000;
    std::string docs[] = {
        "<a>" + std::string(n, 'x') + "</a>",
        "<a b='" + std::string(n, 'x') + "'/>",
        "<a b=\"" + std::string(n, '>') + "\"/>",
        "<a><!--" + [&] { std::string s; for (size_t i = 0; i < n / 2; ++i) s += "-x"; return s; }() + "--></a>",
        "<a><![CDATA[" + std::string(n, ']') + "x]]></a>",
        "<a><?p " + std::string(n, '?') + "x?></a>",
        "<!DOCTYPE a [<!ENTITY e '" + std::string(n, '>') + "'>]><a/>",
    };
    for (auto& doc : docs) {
        auto t0 = std::chrono::steady_clock::now();
        auto d = dump_stream(doc, 1);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        EXPECT_TRUE(d.ok) << doc.substr(0, 20) << ": " << (d.error ? std::string(d.error->message().view()) : "");
        EXPECT_LT(ms, 3000) << doc.substr(0, 20);
    }
}

TEST(XmlReader_Tests, ManyAttributesAndNamespaces) {
    std::string doc = "<a";
    for (int i = 0; i < 20000; ++i) {
        doc += " a" + std::to_string(i) + "='" + std::to_string(i) + "'";
    }
    auto t0 = std::chrono::steady_clock::now();
    EXPECT_TRUE(dump(doc + "/>").ok);
    auto dup = dump(doc + " a19999='x'/>");
    ASSERT_FALSE(dup.ok);
    EXPECT_EQ(dup.error->code(), errc::duplicate_key);
    EXPECT_EQ(dup.error->offset(), doc.size() + 1);
    // 20000 prefixes, and two attributes of one namespace under two of them
    std::string ns = "<a";
    for (int i = 0; i < 20000; ++i) {
        ns += " xmlns:p" + std::to_string(i) + "='urn:" + std::to_string(i) + "'";
    }
    ns += " xmlns:same='urn:7000'";
    for (int i = 0; i < 20; ++i) {
        ns += " p" + std::to_string(i * 1000) + ":x='1'";
    }
    EXPECT_TRUE(dump(ns + "><p19999:b/></a>").ok);
    auto clash = dump(ns + " same:x='2'/>");
    ASSERT_FALSE(clash.ok);
    EXPECT_EQ(clash.error->code(), errc::duplicate_key);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(ms, 3000);
    // a prefix bound on every level of a deep document, and dropped with it
    std::string deep;
    for (int i = 0; i < 500; ++i) {
        deep += "<p:e xmlns:p='urn:" + std::to_string(i) + "'>";
    }
    for (int i = 0; i < 500; ++i) {
        deep += "</p:e>";
    }
    xml::reader r{string(deep)};
    int level = 0;
    while (auto t = r.next()) {
        if (t->type() == xml::token::kind::start_element) {
            EXPECT_EQ(t->namespace_uri(), "urn:" + std::to_string(level++));
        } else {
            EXPECT_EQ(t->namespace_uri(), "urn:" + std::to_string(--level));
        }
    }
    EXPECT_FALSE(r.last_error().has_value());
}

// The stream failing: the tokens before are read, then the error, with
// the stream's own inside
TEST(XmlReader_Tests, AFailingStream) {
    xml::reader r(make_tracked<enc_test::failing>("<a><b/>te"));
    std::vector<std::string> got;
    while (auto t = r.next()) {
        got.push_back(line(*t));
    }
    EXPECT_EQ(got, (std::vector<std::string>{"S {}a", "S {}b", "E {}b"}));
    ASSERT_TRUE(r.last_error().has_value());
    EXPECT_EQ(r.last_error()->code(), errc::io);
    EXPECT_TRUE(r.last_error()->io_error().has_value());
    EXPECT_EQ(r.last_error()->offset(), 9u);
    auto p = xml::parse(io::reader(make_tracked<enc_test::failing>("<a/>")));
    ASSERT_FALSE(p.has_value());
    EXPECT_EQ(p.error().code(), errc::io);
}

// async_parse made with its options a temporary (the default argument, or
// one written in the call) and started after the full expression: the
// task holds a copy of them, not a reference into the caller's frame
TEST(XmlReader_Tests, AsyncParseHoldsItsOptions) {
    std::string doc = "<a><b><c/></b></a>";
    auto plain = xml::async_parse(make_tracked<enc_test::dribble>(doc, 2));
    auto shallow = xml::async_parse(make_tracked<enc_test::dribble>(doc, 2), xml::options{2});
    auto tree = sgcl::async::spawn(std::move(plain)).wait();
    ASSERT_TRUE(tree);
    EXPECT_EQ(tree->children().size(), 1u);
    auto refused = sgcl::async::spawn(std::move(shallow)).wait();
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error().code(), errc::depth_limit);
    sgcl::async::scheduler::stop();
}

TEST(XmlReader_Tests, AsyncForms) {
    auto t = sgcl::async::spawn([]() -> async::task<int> {
        std::string doc = "<list><i n='1'>a</i><i n='2'/><!--c--><i n='3'><j/></i></list>";
        xml::reader r(make_tracked<enc_test::dribble>(doc, 3));
        auto first = co_await r.async_next();
        if (!first || !first->is_start("list")) {
            co_return -1;
        }
        auto peeked = co_await r.async_peek();
        if (!peeked || !peeked->is_start("i")) {
            co_return -2;
        }
        auto one = co_await r.async_read();
        if (!one || one->text() != "a") {
            co_return -3;
        }
        if (!co_await r.async_skip()) {
            co_return -4;
        }
        auto three = co_await r.async_read();
        if (!three || three->attribute("n") != "3" || three->children().size() != 1) {
            co_return -5;
        }
        if (co_await r.async_read()) {
            co_return -6;
        }
        auto end = co_await r.async_next();
        if (!end || !end->is_end("list") || co_await r.async_next() || r.last_error()) {
            co_return -7;
        }
        auto tree = co_await xml::async_parse(make_tracked<enc_test::dribble>(doc, 2));
        if (!tree || tree->children().size() != 3) {
            co_return -8;
        }
        auto bad = co_await xml::async_parse(make_tracked<enc_test::dribble>("<a>", 2));
        if (bad || bad.error().code() != errc::unexpected_end) {
            co_return -9;
        }
        co_return 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

// A reader holds its buffer, the strings of its tokens and the stream under
// it through tracked pointers: read in a loop that forces a collection at
// every step, everything survives
TEST(XmlReader_Tests, SurvivesACollection) {
    std::string doc = "<r xmlns:p='urn:p'>";
    for (int i = 0; i < 300; ++i) {
        doc += "<p:i n='" + std::to_string(i) + "'>text " + std::to_string(i) + "</p:i>";
    }
    doc += "</r>";
    auto whole = dump(doc);
    xml::reader r(make_tracked<enc_test::dribble>(doc, 100));
    Dump d;
    std::string pending;
    sgcl::vector<xml::token> kept;   // tokens hold strings: a managed vector
    while (true) {
        sgcl::collector::force_collect(true);
        auto t = r.next();
        if (!t) {
            break;
        }
        kept.push_back(*t);
        append(d, pending, *t);
    }
    EXPECT_FALSE(r.last_error().has_value());
    EXPECT_EQ(d.tokens, whole.tokens);
    sgcl::collector::force_collect(true);
    std::string again;
    for (auto& t : kept) {
        again += line(t) + "\n";
    }
    EXPECT_EQ(again, d.tokens);
}

// Documents made by mutating the ones above (bytes flipped, markup
// inserted, cut, doubled, two spliced), with nothing to compare them to
// but the reader itself: whole or through a stream in pieces of 1, 2 and 7
// bytes the same tokens and the same error, never a crash (run it under
// ASan), and a document read is a tree that is written and read again as
// itself. tools/xml_oracle.go -fuzz does the same against Go
// (xml_go.cpp: Fuzz).
TEST(XmlReader_Tests, Mutations) {
    std::vector<std::string> corpus;
    for (auto& c : Tokens) {
        corpus.push_back(c.doc);
    }
    for (auto& c : Failures) {
        corpus.push_back(c.doc);
    }
    corpus.push_back("<?xml version=\"1.0\" encoding=\"ISO-8859-2\"?><a x='\xB1'>\xB6<b/></a>");
    corpus.push_back(std::string("\xFF\xFE<\0a\0 \0x\0=\0'\0\x7C\x01'\0>\0t\0<\0/\0a\0>\0", 30));
    const char* pieces[] = {"<", ">", "&", ";", "\"", "'", "/", "!", "?", "[", "]", "-", "=", " ", "\n", "\r", ":", "#",
                            "\xC5\xBC", "\xF0\x9F\x98\x80", "\xFF", "\xC3", "&amp;", "&#x41;", "<a>", "</a>", "<!--", "-->",
                            "<![CDATA[", "]]>", "<?p ", "?>", "xmlns:p='u'", "p:", " a='1'", "<!DOCTYPE d [", "]>"};
    uint64_t state = 7;
    auto next = [&] {
        state += 0x9E3779B97F4A7C15ull;
        uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    };
    auto below = [&](size_t n) { return size_t(next() % n); };
    size_t read = 0;
    for (int round = 0; round < 100000; ++round) {
        std::string d = corpus[below(corpus.size())];
        for (size_t k = 1 + below(4); k > 0; --k) {
            size_t n = d.size();
            switch (below(5)) {
                case 0:
                    if (n) d[below(n)] ^= char(1 << below(8));
                    break;
                case 1:
                    d.insert(below(n + 1), pieces[below(std::size(pieces))]);
                    break;
                case 2:
                    if (n) d.erase(below(n), 1 + below(8));
                    break;
                case 3:
                    if (n) {
                        size_t at = below(n);
                        d.insert(at, d.substr(at, 1 + below(16)));
                    }
                    break;
                case 4: {
                    auto& o = corpus[below(corpus.size())];
                    d = d.substr(0, below(n + 1)) + o.substr(below(o.size() + 1));
                    break;
                }
            }
        }
        auto whole = dump(d);
        for (size_t piece : {1, 2, 7}) {
            auto cut = dump_stream(d, piece);
            ASSERT_EQ(cut.tokens, whole.tokens) << shown(d) << " in pieces of " << piece;
            ASSERT_EQ(cut.ok, whole.ok) << shown(d) << " in pieces of " << piece;
            if (whole.error) {
                ASSERT_EQ(*cut.error, *whole.error) << shown(d) << " in pieces of " << piece << ": "
                    << cut.error->message().view() << " / " << whole.error->message().view();
            }
        }
        if (whole.ok) {
            ++read;
            auto t = xml::parse(string(d));
            ASSERT_TRUE(t.has_value()) << shown(d);
            auto again = xml::parse(t->to_string(xml::pretty));
            ASSERT_TRUE(again.has_value()) << shown(d);
            ASSERT_EQ(*again, *t) << shown(d);
        }
    }
    EXPECT_GT(read, 2000u);
}

// The line and the column of an error found after the buffer of a stream
// moved on: the lines and the characters of the line that were dropped
// are counted when they are dropped
TEST(XmlReader_Tests, PositionsAfterTheBufferMoves) {
    std::string wide = "<a b='\xC5\xBC'>" + std::string(30000, 'x');
    for (int i = 0; i < 3000; ++i) {
        wide += "\xC4\x85<b/>";
    }
    wide += "&bad;</a>";
    std::string tall = "<a>\n";
    for (int i = 0; i < 5000; ++i) {
        tall += "  <b>\xC5\xBC\xC5\xBC</b>\n";
    }
    tall += "  <c>&bad;</c></a>";
    for (auto& doc : {wide, tall}) {
        auto whole = dump(doc);
        ASSERT_FALSE(whole.ok);
        EXPECT_EQ(whole.error->code(), errc::undefined_entity);
        for (size_t piece : {1, 7, 4096, 5000}) {
            auto cut = dump_stream(doc, piece);
            ASSERT_FALSE(cut.ok);
            EXPECT_EQ(*cut.error, *whole.error) << piece << ": " << cut.error->message().view() << " / " << whole.error->message().view();
        }
    }
    EXPECT_EQ(dump(wide).error->column(), 9u + 30000 + 3000 * 5 + 1);
    EXPECT_EQ(dump(tall).error->line(), 5002u);
    EXPECT_EQ(dump(tall).error->column(), 6u);
}
