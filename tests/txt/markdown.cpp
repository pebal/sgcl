//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt's Markdown against md4c: the documents of tests/txt/markdown_vectors.h
// (tools/markdown_vectors.py, a corpus over CommonMark 0.31 and GitHub's
// extensions and random documents) whose HTML md4c writes alike; then the
// places where md4c leaves the specifications, the safe defaults, the
// options, the tree and the boundaries.
#include "tests/types.h"
#include "tests/txt/markdown_vectors.h"

#include <chrono>
#include <string>

namespace {
    string s(const char* t) {
        return string(t);
    }

    std::string html(const char* text, const txt::markdown_options& o = {}) {
        return std::string(txt::markdown_to_html(string(text), o).view());
    }

    // as md4c writes: raw HTML kept, URLs unfiltered
    txt::markdown_options like_md4c(bool github) {
        return txt::markdown_options{.tables = github, .strikethrough = github, .autolinks = github,
                                     .task_lists = github, .raw_html = true, .tag_filter = false, .safe_urls = false};
    }
}

TEST(Markdown_Tests, Md4cVectors) {
    size_t n = 0;
    for (const auto& v : MarkdownVectors) {
        auto o = like_md4c(v.github);
        EXPECT_EQ(html(v.text, o), v.html) << v.text;
        EXPECT_EQ(std::string(txt::markdown_document::parse(s(v.text), o).to_html().view()), v.html) << v.text;
        ++n;
    }
    EXPECT_GT(n, 2000u);
}

// Where md4c leaves CommonMark 0.31.2 or GFM 0.29, the specifications' answers
TEST(Markdown_Tests, WhereMd4cDiffers) {
    auto c = like_md4c(false);
    auto g = like_md4c(true);
    // a code span's line ending is a space, the spaces before it kept (spec examples 335, 336)
    EXPECT_EQ(html("``\nfoo\nbar  \nbaz\n``\n", c), "<p><code>foo bar   baz</code></p>\n");
    EXPECT_EQ(html("`foo   bar \nbaz`\n", c), "<p><code>foo   bar  baz</code></p>\n");
    // an item that starts blank cannot interrupt a paragraph, trailing spaces or not
    EXPECT_EQ(html("a\n*  \nb\n", c), "<p>a\n*<br />\nb</p>\n");
    EXPECT_EQ(html("a\n1. \nb\n", c), "<p>a\n1.\nb</p>\n");
    // an HTML block starts after at most three spaces of indentation
    EXPECT_EQ(html("a\n    <div>\n", c), "<p>a\n<div></p>\n");
    EXPECT_EQ(html("a\n\t<!-- c -->\n", c), "<p>a\n<!-- c --></p>\n");
    // a fence at the margin is not inside the item: the item ends, a new fence opens
    EXPECT_EQ(html("- ```\n  b\n```\nc\n", c), "<ul>\n<li>\n<pre><code>b\n</code></pre>\n</li>\n</ul>\n<pre><code>c\n</code></pre>\n");
    // an unclosed fence in an item does not swallow the line after the item
    EXPECT_EQ(html("- ~~~\n~\n", c), "<ul>\n<li>\n<pre><code></code></pre>\n</li>\n</ul>\n<p>~</p>\n");
    // a blank line does not end an item whose heading came first
    EXPECT_EQ(html("- # h\n\n    ###### x\n", c), "<ul>\n<li>\n<h1>h</h1>\n<h6>x</h6>\n</li>\n</ul>\n");
    // an autolink's scheme starts with a letter
    EXPECT_EQ(html("<<http://a.b>\n", c), "<p>&lt;<a href=\"http://a.b\">http://a.b</a></p>\n");
    // tables: the header and the delimiter row agree in cells, a pipe in a code span splits
    EXPECT_EQ(html("| abc | def |\n| --- |\n| bar |\n", g), "<p>| abc | def |\n| --- |\n| bar |</p>\n");
    EXPECT_EQ(html("| a |\n| - |\n| `x|y` |\n", g),
              "<table>\n<thead>\n<tr>\n<th>a</th>\n</tr>\n</thead>\n<tbody>\n<tr>\n<td>`x</td>\n</tr>\n</tbody>\n</table>\n");
    EXPECT_EQ(html("| f\\|oo |\n| --- |\n| b `\\|` az |\n", g),
              "<table>\n<thead>\n<tr>\n<th>f|oo</th>\n</tr>\n</thead>\n<tbody>\n<tr>\n<td>b <code>|</code> az</td>\n"
              "</tr>\n</tbody>\n</table>\n");
    // a block start ends a table
    EXPECT_EQ(html("| a |\n| - |\n<!-- c -->\n", g),
              "<table>\n<thead>\n<tr>\n<th>a</th>\n</tr>\n</thead>\n</table>\n<!-- c -->\n");
    // GFM's extended autolinks (examples 625, 626, 627)
    EXPECT_EQ(html("www.google.com/search?q=(business))+ok\n", g),
              "<p><a href=\"http://www.google.com/search?q=(business))+ok\">www.google.com/search?q=(business))+ok</a></p>\n");
    EXPECT_EQ(html("www.google.com/search?q=commonmark&hl;\n", g),
              "<p><a href=\"http://www.google.com/search?q=commonmark\">www.google.com/search?q=commonmark</a>&amp;hl;</p>\n");
    EXPECT_EQ(html("www.commonmark.org/he<lp\n", g),
              "<p><a href=\"http://www.commonmark.org/he\">www.commonmark.org/he</a>&lt;lp</p>\n");
}

TEST(Markdown_Tests, SafeByDefault) {
    // raw HTML left out, as cmark leaves it
    EXPECT_EQ(html("<b>x</b>\n\n<div>\nz\n</div>\n"),
              "<p><!-- raw HTML omitted -->x<!-- raw HTML omitted --></p>\n<!-- raw HTML omitted -->\n");
    // dangerous URLs lose their destination; data: images keep theirs
    EXPECT_EQ(html("[a](javascript:alert(1)) [b](JavaScript:x) [c](vbscript:x) [d](file:///etc/passwd)\n"),
              "<p><a href=\"\">a</a> <a href=\"\">b</a> <a href=\"\">c</a> <a href=\"\">d</a></p>\n");
    EXPECT_EQ(html("![i](data:image/png;base64,AA) [t](data:text/html,x)\n"),
              "<p><img src=\"data:image/png;base64,AA\" alt=\"i\" /> <a href=\"\">t</a></p>\n");
    // with raw HTML, GitHub's tag filter
    txt::markdown_options raw{.raw_html = true};
    EXPECT_EQ(html("<b>x</b> <script>y</script>\n\n<xmp>\nz\n</xmp>\n", raw),
              "<p><b>x</b> &lt;script>y&lt;/script></p>\n&lt;xmp>\nz\n&lt;/xmp>\n");
    raw.tag_filter = false;
    EXPECT_EQ(html("<iframe src=x></iframe>\n", raw), "<iframe src=x></iframe>\n");
    // URLs percent-encoded, '&' an entity
    EXPECT_EQ(html("[a](/my%20uri?x=1&y=ą\"')\n"), "<p><a href=\"/my%20uri?x=1&amp;y=%C4%85%22%27\">a</a></p>\n");
}

TEST(Markdown_Tests, Options) {
    txt::markdown_options none{.tables = false, .strikethrough = false, .autolinks = false, .task_lists = false};
    EXPECT_EQ(html("| a |\n| - |\n\n~~x~~ www.x.org\n\n- [ ] t\n", none),
              "<p>| a |\n| - |</p>\n<p>~~x~~ www.x.org</p>\n<ul>\n<li>[ ] t</li>\n</ul>\n");
    EXPECT_EQ(html("| a |\n| - |\n\n~~x~~ www.x.org\n\n- [ ] t\n"),
              "<table>\n<thead>\n<tr>\n<th>a</th>\n</tr>\n</thead>\n</table>\n<p><del>x</del> "
              "<a href=\"http://www.x.org\">www.x.org</a></p>\n<ul>\n<li><input type=\"checkbox\" disabled=\"\" /> t</li>\n</ul>\n");
    EXPECT_EQ(html("a\nb\n", {.hard_breaks = true}), "<p>a<br />\nb</p>\n");
    EXPECT_EQ(html("- [x] done\n"), "<ul>\n<li><input type=\"checkbox\" checked=\"\" disabled=\"\" /> done</li>\n</ul>\n");
    EXPECT_EQ(html("~one~ ~~two~~ ~~~three~~~\n"), "<p><del>one</del> <del>two</del> ~~~three~~~</p>\n");
    EXPECT_EQ(html("mail me@example.com or see https://x.org/a_b.\n"),
              "<p>mail <a href=\"mailto:me@example.com\">me@example.com</a> or see "
              "<a href=\"https://x.org/a_b\">https://x.org/a_b</a>.</p>\n");
}

TEST(Markdown_Tests, Tree) {
    auto doc = txt::markdown_document::parse(s("# Title\n\n1. one\n2. *two*\n\n- [x] done\n\n"
                                               "```cpp\nint x;\n```\n\n| a | b |\n|:-|-:|\n| 1 | 2 |\n\n"
                                               "[link](/u \"T\") ![img](/i.png)\n"));
    txt::markdown_node root = doc.root();
    ASSERT_TRUE(bool(root));
    EXPECT_EQ(root.kind(), txt::markdown_kind::document);
    ASSERT_EQ(root.size(), 6u);
    EXPECT_EQ(root[0].kind(), txt::markdown_kind::heading);
    EXPECT_EQ(root[0].level(), 1);
    EXPECT_EQ(std::string(root[0].text().view()), "Title");
    txt::markdown_node list = root[1];
    EXPECT_EQ(list.kind(), txt::markdown_kind::list);
    EXPECT_TRUE(list.ordered());
    EXPECT_EQ(list.start(), 1);
    EXPECT_TRUE(list.tight());
    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(list[1][0][0].kind(), txt::markdown_kind::emphasis);
    EXPECT_TRUE(list[1].parent() == list);
    EXPECT_FALSE(list[0].checked().has_value());
    txt::markdown_node task = root[2][0];
    ASSERT_TRUE(task.checked().has_value());
    EXPECT_TRUE(*task.checked());
    EXPECT_EQ(root[3].kind(), txt::markdown_kind::code_block);
    EXPECT_EQ(std::string(root[3].info().view()), "cpp");
    EXPECT_EQ(std::string(root[3].literal().view()), "int x;\n");
    txt::markdown_node table = root[4];
    EXPECT_EQ(table.kind(), txt::markdown_kind::table);
    ASSERT_EQ(table.size(), 2u);
    EXPECT_TRUE(table[0].header());
    EXPECT_FALSE(table[1].header());
    EXPECT_EQ(table[0][0].align(), txt::markdown_align::left);
    EXPECT_EQ(table[1][1].align(), txt::markdown_align::right);
    EXPECT_EQ(std::string(table[1][1].text().view()), "2");
    txt::markdown_node para = root[5];
    EXPECT_EQ(para[0].kind(), txt::markdown_kind::link);
    EXPECT_EQ(std::string(para[0].url().view()), "/u");
    EXPECT_EQ(std::string(para[0].title().view()), "T");
    EXPECT_EQ(para[2].kind(), txt::markdown_kind::image);
    // a subtree's HTML, and the document's with its options
    EXPECT_EQ(std::string(list.to_html().view()), "<ol>\n<li>one</li>\n<li><em>two</em></li>\n</ol>\n");
    EXPECT_EQ(std::string(doc.to_html().view()), html("# Title\n\n1. one\n2. *two*\n\n- [x] done\n\n"
                                                      "```cpp\nint x;\n```\n\n| a | b |\n|:-|-:|\n| 1 | 2 |\n\n"
                                                      "[link](/u \"T\") ![img](/i.png)\n"));
    // out of range and no node
    EXPECT_FALSE(bool(root[99]));
    EXPECT_FALSE(bool(root.parent()));
    txt::markdown_node none;
    EXPECT_FALSE(bool(none));
    EXPECT_EQ(none.size(), 0u);
    EXPECT_TRUE(none.text().empty());
    EXPECT_TRUE(none.to_html().empty());
    txt::markdown_document empty;
    EXPECT_FALSE(bool(empty.root()));
    EXPECT_TRUE(empty.to_html().empty());
}

TEST(Markdown_Tests, Text) {
    auto doc = txt::markdown_document::parse(s("a *b* `c`\nd  \ne\n"));
    EXPECT_EQ(std::string(doc.root().text().view()), "a b c\nd\ne");
    // an image's alt is its description's text
    EXPECT_EQ(html("![a *b* `c`\nd](/x)\n"), "<p><img src=\"/x\" alt=\"a b c d\" /></p>\n");
}

TEST(Markdown_Tests, Boundaries) {
    EXPECT_EQ(html(""), "");
    EXPECT_EQ(html("\n\n  \n"), "");
    // CR LF and CR end lines; NUL is U+FFFD
    EXPECT_EQ(html("a\r\nb\r\n\r\nc\rd"), "<p>a\nb</p>\n<p>c\nd</p>\n");
    EXPECT_EQ(std::string(txt::markdown_to_html(string(std::string_view("x\0y", 3))).view()), "<p>x\xEF\xBF\xBDy</p>\n");
    // invalid UTF-8 passes through
    EXPECT_EQ(html("\xFF\xC3\n"), "<p>\xFF\xC3</p>\n");
    // deep documents: nothing recurses, nothing is quadratic in the depth
    std::string quotes(20000, '>'), list, emph, brackets;
    quotes += " a\n";
    for (int i = 0; i < 20000; ++i) {
        list += "- ";
        emph += "*a ";
        brackets += "[";
    }
    list += "x\n";
    emph += std::string(20000, '*');
    brackets += std::string(20000, ']');
    auto t0 = std::chrono::steady_clock::now();
    for (const std::string& doc : {quotes, list, emph, brackets}) {
        string text{std::string_view(doc)};
        EXPECT_FALSE(txt::markdown_to_html(text).empty());
        EXPECT_TRUE(bool(txt::markdown_document::parse(text).root()));
    }
    EXPECT_LT(std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), 10.0);
}
