//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The writer: what it writes, compact and indented; the mistakes it keeps
// and flush() gives; a stream that fails under it; the async form; what it
// writes reads back as the same tokens.
#include "xml_common.h"

#include <chrono>

using namespace sgcl::encoding;
using namespace xml_test;
using enc_test::sink;

TEST(XmlWriter_Tests, Writes) {
    sgcl::tracked_ptr out = make_tracked<sink>();
    xml::writer w(out);
    w.declaration().start("catalog").attribute("xmlns", "urn:c").start("book").attribute("id", "7")
     .start("title").text("Dune & more").end().comment(" c ").instruction("page", "12").start("empty").end()
     .cdata("<a>]]>b").end().end();
    EXPECT_EQ(w.depth(), 0u);
    ASSERT_TRUE(w.flush().has_value());
    EXPECT_EQ(out->text, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><catalog xmlns=\"urn:c\"><book id=\"7\"><title>Dune &amp; more</title>"
                         "<!-- c --><?page 12?><empty/><![CDATA[<a>]]]]><![CDATA[>b]]></book></catalog>");
    // the CDATA section split around "]]>" reads back as the text
    auto back = xml::parse(string(out->text)).value();
    EXPECT_EQ(back.child("book").text(), "Dune & more<a>]]>b");
    // flushed text is gone from the writer; what comes next goes out next
    w.comment("after");
    ASSERT_TRUE(w.flush().has_value());
    EXPECT_TRUE(out->text.ends_with("<!--after-->"));
    EXPECT_TRUE(w.flush().has_value());   // nothing to write
}

TEST(XmlWriter_Tests, Indented) {
    sgcl::tracked_ptr out = make_tracked<sink>();
    xml::writer w(out, xml::style{2, true});
    w.start("a").start("b").attribute("x", "1").end().comment("c").start("p").text("mixed ").start("i").text("in").end().text(" here").end()
     .start("c").start("d").end().end().end();
    ASSERT_TRUE(w.flush().has_value());
    EXPECT_EQ(out->text, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<a>\n  <b x=\"1\"/>\n  <!--c-->\n  <p>mixed <i>in</i> here</p>\n  <c>\n    <d/>\n  </c>\n</a>");
}

// A mistake is kept, nothing after it is written, and flush() gives it
TEST(XmlWriter_Tests, Mistakes) {
    struct Case {
        std::function<void(xml::writer&)> write;
        errc code;
    };
    const Case cases[] = {
        {[](xml::writer& w) { w.start("a").text("t").attribute("x", "1"); }, errc::syntax},
        {[](xml::writer& w) { w.attribute("x", "1"); }, errc::syntax},
        {[](xml::writer& w) { w.end(); }, errc::mismatched_tag},
        {[](xml::writer& w) { w.start("a").end().end(); }, errc::mismatched_tag},
        {[](xml::writer& w) { w.start("1a"); }, errc::syntax},
        {[](xml::writer& w) { w.start("a:b:c"); }, errc::syntax},
        {[](xml::writer& w) { w.start("a").attribute("b c", "1"); }, errc::syntax},
        {[](xml::writer& w) { w.start("a").attribute("x", "1").attribute("x", "2"); }, errc::duplicate_key},
        {[](xml::writer& w) { w.start("a").comment("a--b"); }, errc::syntax},
        {[](xml::writer& w) { w.start("a").comment("ends-"); }, errc::syntax},
        {[](xml::writer& w) { w.instruction("xml", "v"); }, errc::syntax},
        {[](xml::writer& w) { w.instruction("p", "a?>b"); }, errc::syntax},
        {[](xml::writer& w) { w.instruction("p:q", ""); }, errc::syntax},
        {[](xml::writer& w) { w.start("a").declaration(); }, errc::syntax},
    };
    for (auto& c : cases) {
        sgcl::tracked_ptr out = make_tracked<sink>();
        xml::writer w(out);
        c.write(w);
        w.start("after").end();
        ASSERT_TRUE(w.last_error().has_value());
        EXPECT_EQ(w.last_error()->code(), c.code) << w.last_error()->message().view();
        auto r = w.flush();
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().code(), make_error_code(c.code));
        EXPECT_TRUE(out->text.empty());
        EXPECT_FALSE(w.flush().has_value());   // for good
    }
}

TEST(XmlWriter_Tests, AFailingStream) {
    xml::writer w(make_tracked<enc_test::broken>());
    w.start("a").end();
    auto r = w.flush();
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().code(), std::errc::broken_pipe);
    w.start("b").end();
    EXPECT_FALSE(w.flush().has_value());   // the stream's failure kept
    EXPECT_FALSE(w.last_error().has_value());     // no mistake of the document's
}

TEST(XmlWriter_Tests, AsyncFlush) {
    auto t = sgcl::async::spawn([]() -> async::task<int> {
        sgcl::tracked_ptr out = make_tracked<sink>();
        xml::writer w(out);
        w.start("a").text("x").end();
        auto r = co_await w.async_flush();
        if (!r || out->text != "<a>x</a>") {
            co_return -1;
        }
        xml::writer broken(make_tracked<enc_test::broken>());
        broken.start("a").end();
        auto b = co_await broken.async_flush();
        co_return b ? -2 : 1;
    }());
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

// What the writer writes, the reader reads as the same tokens: a document
// written token by token from the tokens of another
TEST(XmlWriter_Tests, TokensWrittenReadBack) {
    std::string doc = "<?xml version=\"1.0\"?><r xmlns=\"urn:r\" xmlns:q=\"urn:q\"><q:i a='&lt;&quot;&#9;&#10;'>t&amp;\xC5\xBC<![CDATA[c]]></q:i><!--x--><?p d?><e/></r>";
    sgcl::tracked_ptr out = make_tracked<sink>();
    xml::writer w(out);
    xml::reader r{string(doc)};
    while (auto t = r.next()) {
        switch (t->type()) {
            case xml::token::kind::start_element:
                w.start(t->name());
                for (auto& a : t->attributes()) {
                    w.attribute(a.name, a.value);
                }
                break;
            case xml::token::kind::end_element: w.end(); break;
            case xml::token::kind::text: w.text(t->text()); break;
            case xml::token::kind::comment: w.comment(t->text()); break;
            case xml::token::kind::instruction:
                if (t->name() != "xml") {
                    w.instruction(t->name(), t->text());
                }
                break;
            case xml::token::kind::doctype: break;
        }
    }
    ASSERT_TRUE(w.flush().has_value());
    auto a = dump(doc), b = dump(out->text);
    auto without_declaration = a.tokens.substr(a.tokens.find('\n') + 1);
    EXPECT_EQ(b.tokens, without_declaration);
    xml::reader values{string(out->text)};
    values.next();
    EXPECT_EQ(values.next()->attribute("a").value(), "<\"\t\n");
}

// A tag of many attributes: checked for a repeated name through a set, so
// that a tree read from somewhere is written back in linear time
TEST(XmlWriter_Tests, ManyAttributes) {
    std::string doc = "<a";
    for (int i = 0; i < 100000; ++i) {
        doc += " a" + std::to_string(i) + "='" + std::to_string(i) + "'";
    }
    doc += "/>";
    auto t0 = std::chrono::steady_clock::now();
    auto tree = xml::parse(string(doc)).value();
    EXPECT_EQ(tree.to_string().size(), doc.size());   // the same but for the quotes, one for one
    EXPECT_EQ(xml::parse(tree.to_string()).value(), tree);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_LT(ms, 3000);
    sgcl::tracked_ptr out = make_tracked<sink>();
    xml::writer w(out);
    w.start("a");
    for (int i = 0; i < 40; ++i) {
        w.attribute(string("a" + std::to_string(i)), "1");
    }
    w.attribute("a3", "2");
    ASSERT_TRUE(w.last_error().has_value());
    EXPECT_EQ(w.last_error()->code(), errc::duplicate_key);
    xml::writer v(out);
    v.start("a");
    for (int i = 0; i < 20; ++i) {
        v.attribute(string("a" + std::to_string(i)), "1");
    }
    v.end().start("b").attribute("a0", "1").attribute("a19", "1").end();
    EXPECT_FALSE(v.last_error().has_value());
}
