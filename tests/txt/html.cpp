//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::html_document against gumbo-parser: the documents of
// tests/txt/html_vectors.h (tools/html_vectors.py) parsed into the trees gumbo
// builds (its split text nodes joined, as the standard joins them), read
// through the public nodes; then the serialization, the fragments, the
// errors, the sanitizer and the boundaries.
#include "tests/types.h"
#include "tests/txt/html_vectors.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {
    string s(const char* t) {
        return string(t);
    }

    void line(std::string& out, int depth) {
        out += "| ";
        for (int i = 0; i < depth; ++i) {
            out += "  ";
        }
    }

    std::string attr_display(const txt::html_node& e, const std::string& name) {
        if (e.ns() != txt::html_namespace::html) {
            for (const char* p : {"xlink:actuate", "xlink:arcrole", "xlink:href", "xlink:role", "xlink:show",
                                  "xlink:title", "xlink:type", "xml:lang", "xml:space", "xmlns:xlink"}) {
                if (name == p) {
                    std::string r = name;
                    r[r.find(':')] = ' ';
                    return r;
                }
            }
            if (name == "xmlns") {
                return "xmlns xmlns";
            }
        }
        return name;
    }

    // html5lib's format, through the public nodes
    void dump(std::string& out, const txt::html_node& n, int depth) {
        switch (n.kind()) {
            case txt::html_node_kind::element: {
                line(out, depth);
                out += n.ns() == txt::html_namespace::svg ? "<svg " : n.ns() == txt::html_namespace::mathml ? "<math " : "<";
                out += std::string(n.name().view()) + ">\n";
                std::vector<std::pair<std::string, std::string>> attrs;
                for (const auto& a : n.attributes()) {
                    attrs.emplace_back(attr_display(n, std::string(a.name.view())), std::string(a.value.view()));
                }
                std::sort(attrs.begin(), attrs.end());
                for (auto& a : attrs) {
                    line(out, depth + 1);
                    out += a.first + "=\"" + a.second + "\"\n";
                }
                if (n.content()) {
                    line(out, depth + 1);
                    out += "content\n";
                    for (size_t k = 0; k < n.content().size(); ++k) {
                        dump(out, n.content()[k], depth + 2);
                    }
                }
                for (size_t k = 0; k < n.size(); ++k) {
                    dump(out, n[k], depth + 1);
                }
                break;
            }
            case txt::html_node_kind::text:
                line(out, depth);
                out += "\"" + std::string(n.data().view()) + "\"\n";
                break;
            case txt::html_node_kind::comment:
                line(out, depth);
                out += "<!-- " + std::string(n.data().view()) + " -->\n";
                break;
            case txt::html_node_kind::doctype:
                line(out, depth);
                out += "<!DOCTYPE " + std::string(n.data().view()) + ">\n";
                break;
            default:
                break;
        }
    }

    std::string tree(const txt::html_document& d) {
        std::string out;
        for (size_t k = 0; k < d.root().size(); ++k) {
            dump(out, d.root()[k], 0);
        }
        return out;
    }
}

TEST(Html_Tests, GumboVectors) {
    txt::html_options o;
    o.scripting = false;   // as gumbo parses
    size_t bad = 0;
    for (const auto& v : HtmlVectors) {
        auto d = txt::html_document::parse(string(std::string_view(v.document, v.size)), o);
        std::string got = tree(d);
        std::string want = v.tree;
        // the vectors print a DOCTYPE's identifiers, which the dump above leaves out
        if (want.rfind("| <!DOCTYPE ", 0) == 0) {
            size_t e = want.find('\n');
            std::string first = want.substr(0, e);
            size_t q = first.find(" \"");
            if (q != std::string::npos) {
                want = first.substr(0, q) + ">" + want.substr(e);
            }
        }
        if (got != want && ++bad <= 10) {
            ADD_FAILURE() << std::string(v.document, v.size) << "\n--- got\n" << got << "--- want\n" << want;
        }
    }
    EXPECT_EQ(bad, 0u) << "of " << std::size(HtmlVectors);
}

TEST(Html_Tests, Document) {
    auto d = txt::html_document::parse(s("<!DOCTYPE html><title> A \n b </title><p id=x class='c d'>Hi <a href='/u?a=1&amp;b'>link</a>"
                                         "<template><b>t</b></template><svg viewbox='0 0 1 1'><circle/></svg>"));
    EXPECT_FALSE(d.quirks());
    EXPECT_EQ(d.title(), s("A b"));
    EXPECT_EQ(d.html().name(), s("html"));
    EXPECT_EQ(d.head().name(), s("head"));
    EXPECT_EQ(d.body().name(), s("body"));
    auto p = d.root().element_by_id(s("x"));
    ASSERT_TRUE(bool(p));
    EXPECT_EQ(p.name(), s("p"));
    EXPECT_EQ(*p.attribute(s("class")), s("c d"));
    EXPECT_FALSE(p.attribute(s("style")).has_value());
    EXPECT_EQ(p.parent(), d.body());
    EXPECT_EQ(p.size(), 4u);
    EXPECT_EQ(p.first_child().data(), s("Hi "));
    EXPECT_EQ(p.first_child().next_sibling().name(), s("a"));
    EXPECT_EQ(p[1].previous_sibling(), p[0]);
    EXPECT_FALSE(bool(p[3].next_sibling()));
    EXPECT_FALSE(bool(p[9]));
    auto tmpl = p[2];
    EXPECT_EQ(tmpl.size(), 0u);
    EXPECT_EQ(tmpl.content().kind(), txt::html_node_kind::fragment);
    EXPECT_EQ(tmpl.inner_html(), s("<b>t</b>"));
    auto svg = p[3];
    EXPECT_EQ(svg.ns(), txt::html_namespace::svg);
    EXPECT_EQ(*svg.attribute(s("viewBox")), s("0 0 1 1"));
    EXPECT_EQ(svg[0].name(), s("circle"));
    EXPECT_EQ(d.body().text(), s("Hi link"));   // a template's contents are not text of the document
    EXPECT_EQ(d.root().elements(s("a")).size(), 1u);
    EXPECT_EQ(d.root().elements().size(), 9u);   // html head title body p a template svg circle, in order
    EXPECT_EQ(d.root().elements()[0].name(), s("html"));
    EXPECT_EQ(d.to_string(), s("<!DOCTYPE html><html><head><title> A \n b </title></head><body><p id=\"x\" class=\"c d\">Hi "
                               "<a href=\"/u?a=1&amp;b\">link</a><template><b>t</b></template><svg viewBox=\"0 0 1 1\">"
                               "<circle></circle></svg></p></body></html>"));
    // the serialization reads back to the same tree
    auto again = txt::html_document::parse(d.to_string());
    EXPECT_EQ(again.to_string(), d.to_string());
}

TEST(Html_Tests, Serialization) {
    auto d = txt::html_document::parse(s("<p title='a\"b&c<d>e&nbsp;'>x &amp; y &lt; z&nbsp;<br>"
                                         "<script>if (a < b && c) {}</script><style>p > q {}</style><textarea>&lt;</textarea>"));
    EXPECT_EQ(d.body().inner_html(),
              s("<p title=\"a&quot;b&amp;c&lt;d&gt;e&nbsp;\">x &amp; y &lt; z&nbsp;<br><script>if (a < b && c) {}</script>"
                "<style>p > q {}</style><textarea>&lt;</textarea></p>"));
    // noscript is raw text when scripting is on, markup when off (in a body: before it, in the head)
    EXPECT_EQ(txt::html_document::parse(s("<body><noscript><b>x</b></noscript>")).body().inner_html(),
              s("<noscript><b>x</b></noscript>"));
    txt::html_options off;
    off.scripting = false;
    EXPECT_EQ(txt::html_document::parse(s("<body><noscript><b>x</b></noscript>"), off).body().inner_html(),
              s("<noscript><b>x</b></noscript>"));
    EXPECT_EQ(txt::html_document::parse(s("<body><noscript><b>x</b></noscript>")).body()[0][0].kind(),
              txt::html_node_kind::text);
    EXPECT_EQ(txt::html_document::parse(s("<body><noscript><b>x</b></noscript>"), off).body()[0][0].name(), s("b"));
    EXPECT_EQ(txt::html_document::parse(s("<noscript><b>x</b></noscript>")).head()[0].name(), s("noscript"));
    // comments and the DOCTYPE
    EXPECT_EQ(txt::html_document::parse(s("<!--a--><!DOCTYPE html><!--b-->")).to_string(),
              s("<!--a--><!DOCTYPE html><!--b--><html><head></head><body></body></html>"));
}

TEST(Html_Tests, Fragments) {
    auto f = txt::html_document::parse_fragment(s("<td>a</td><p>b"));
    EXPECT_EQ(f.to_string(), s("a<p>b</p>"));   // a td outside a table is dropped, its text kept
    auto t = txt::html_document::parse_fragment(s("<td>a</td>"), s("tr"));
    EXPECT_EQ(t.to_string(), s("<td>a</td>"));
    EXPECT_EQ(txt::html_document::parse_fragment(s("<b>x</b>"), s("textarea")).to_string(), s("&lt;b&gt;x&lt;/b&gt;"));
    EXPECT_EQ(txt::html_document::parse_fragment(s("<b>x</b>"), s("script")).root()[0].data(), s("<b>x</b>"));
    EXPECT_EQ(txt::html_document::parse_fragment(s("<tr><td>1"), s("TABLE")).to_string(),
              s("<tbody><tr><td>1</td></tr></tbody>"));
    EXPECT_EQ(txt::html_document::parse_fragment(s("<option>a<option>b"), s("select")).to_string(),
              s("<option>a</option><option>b</option>"));
    EXPECT_EQ(f.root().name(), s("html"));
    EXPECT_EQ(f.html(), f.root());
}

TEST(Html_Tests, ErrorsAndQuirks) {
    txt::html_options o;
    o.collect_errors = true;
    static const char text[] = "<p>a\0b</p></div><a b=1 b=2>";
    auto d = txt::html_document::parse(string(std::string_view(text, sizeof text - 1)), o);
    std::vector<std::string> codes;
    for (const auto& e : d.errors()) {
        codes.emplace_back(e.code.view());
    }
    auto has = [&](const char* c) { return std::find(codes.begin(), codes.end(), c) != codes.end(); };
    EXPECT_TRUE(has("missing-doctype"));
    EXPECT_TRUE(has("unexpected-null-character"));
    EXPECT_TRUE(has("duplicate-attribute"));
    EXPECT_TRUE(has("unexpected-end-tag"));
    EXPECT_TRUE(d.quirks());
    EXPECT_TRUE(txt::html_document::parse(s("<p>")).errors().empty());   // not collected unless asked
    EXPECT_FALSE(txt::html_document::parse(s("<!DOCTYPE html>")).quirks());
    EXPECT_TRUE(txt::html_document::parse(s("<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">")).quirks());
    EXPECT_TRUE(txt::html_document::parse(s("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"x\">"))
                    .limited_quirks());
    // quirks: a table does not close an open p
    EXPECT_EQ(txt::html_document::parse(s("<p><table>")).body().inner_html(), s("<p><table></table></p>"));
    EXPECT_EQ(txt::html_document::parse(s("<!DOCTYPE html><p><table>")).body().inner_html(), s("<p></p><table></table>"));
}

TEST(Html_Tests, Input) {
    // CR and CRLF, a byte-order mark kept as text, bytes that are not UTF-8
    auto d = txt::html_document::parse(s("<p>a\r\nb\rc</p><p>\xff\xe2\x82</p>"));
    EXPECT_EQ(d.body()[0].text(), s("a\nb\nc"));
    EXPECT_EQ(d.body()[1].text(), s("\xEF\xBF\xBD\xEF\xBF\xBD"));
    // character references
    auto r = txt::html_document::parse(s("<p>&amp;&lt;&notin;&notit;&#x41;&#65;&#0;&#128;&#xD800;&AElig&nonexistent;"
                                         "<a title='&notit=1&amp;x=&lt'>"));
    EXPECT_EQ(r.body()[0].first_child().data(),
              s("&<\xE2\x88\x89\xC2\xAC" "it;AA\xEF\xBF\xBD\xE2\x82\xAC\xEF\xBF\xBD\xC3\x86&nonexistent;"));
    EXPECT_EQ(*r.root().elements(s("a"))[0].attribute(s("title")), s("&notit=1&x=<"));
    // the empty document, and the empty handles
    auto e = txt::html_document::parse(s(""));
    EXPECT_EQ(e.to_string(), s("<html><head></head><body></body></html>"));
    txt::html_document none;
    EXPECT_FALSE(bool(none.root()));
    EXPECT_EQ(none.to_string(), s(""));
    EXPECT_EQ(none.title(), s(""));
    txt::html_node n;
    EXPECT_EQ(n.size(), 0u);
    EXPECT_EQ(n.name(), s(""));
    EXPECT_EQ(n.text(), s(""));
    EXPECT_EQ(n.outer_html(), s(""));
    EXPECT_TRUE(n.elements().empty());
    // a document nested deeper than a thread's stack
    std::string deep;
    for (int i = 0; i < 100000; ++i) {
        deep += "<div>";
    }
    auto big = txt::html_document::parse(string(std::string_view(deep)));
    EXPECT_EQ(big.root().elements(s("div")).size(), 100000u);
    EXPECT_EQ(big.to_string().size(), 39u + 100000u * 11u);
    EXPECT_EQ(txt::sanitize_html(string(std::string_view(deep))).size(), 100000u * 11u);
}

TEST(Html_Tests, Sanitizer) {
    EXPECT_EQ(txt::sanitize_html(s("<p onclick=x style='color:red'>Hi<script>alert(1)</script><style>p{}</style>"
                                   "<a href='javascript:x' title=t>a</a><a href=http://x>b</a><img src=data:x alt=y>"
                                   "<unknown>z</unknown><!--c--><iframe src=x>f</iframe>")),
              s("<p>Hi<a title=\"t\">a</a><a href=\"http://x\">b</a><img alt=\"y\">z</p>"));
    // what a browser strips from a URL before its scheme does not hide one
    EXPECT_EQ(txt::sanitize_html(s("<a href=' java\nscript:x'>a</a><a href='JAVASCRIPT:x'>b</a><a href='/rel?x:y'>c</a>")),
              s("<a>a</a><a>b</a><a href=\"/rel?x:y\">c</a>"));
    txt::html_sanitizer_options o;
    o.nofollow = true;
    o.keep_comments = true;
    EXPECT_EQ(txt::sanitize_html(s("<a href=https://x rel=me>a</a>"), o),
              s("<a href=\"https://x\" rel=\"nofollow noopener noreferrer\">a</a>"));
    EXPECT_EQ(txt::sanitize_html(s("<!--a--b>-->x"), o), s("<!--a  b -->x"));
    txt::html_sanitizer_options custom;
    custom.elements = {s("b"), s("a")};
    custom.attributes = {s("*:class"), s("a:href"), s("b:onclick")};
    custom.url_schemes = {s("ftp")};
    custom.relative_urls = false;
    EXPECT_EQ(txt::sanitize_html(s("<p class=c><b class=d onclick=x>1</b><a href=ftp://x>2</a><a href=http://y>3</a>"
                                   "<a href=/z>4</a></p>"), custom),
              s("<b class=\"d\">1</b><a href=\"ftp://x\">2</a><a>3</a><a>4</a>"));   // never an event handler
    // escaped text and attribute values, a void element, a table repaired
    EXPECT_EQ(txt::sanitize_html(s("1 < 2 & <b title='\"'>x</b><br><table><td>c")),
              s("1 &lt; 2 &amp; <b title=\"&quot;\">x</b><br><table><tbody><tr><td>c</td></tr></tbody></table>"));
    EXPECT_EQ(txt::sanitize_html(s("")), s(""));
    EXPECT_EQ(txt::sanitize_html(s("<svg><script>x</script></svg><math>y</math>")), s(""));
}
