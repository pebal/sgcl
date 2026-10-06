//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::html_stencil against Go's html/template: the templates of
// tests/txt/html_stencil_vectors.h (tools/html_stencil_vectors.py), written as
// Go writes them and refused where Go refuses them; then the boundaries.
#include "tests/types.h"
#include "tests/txt/html_stencil_data.h"
#include "tests/txt/html_stencil_vectors.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {
    string s(const char* t) {
        return string(t);
    }

    string page(const char* source, const txt::value& data) {
        auto t = txt::html_stencil::parse(string(source));
        return t ? t->render(data) : string("ERROR");
    }
}

TEST(HtmlStencil_Tests, GoVectors) {
    size_t bad = 0;
    for (const auto& v : HtmlStencilVectors) {
        auto t = txt::html_stencil::parse(s(v.source));
        bool ok;
        std::string got;
        if (!t) {
            ok = v.page == nullptr;
            got = "ERROR " + std::string(t.error().message().view());
        } else {
            got = std::string(t->render(html_stencil_data::parse(v.data)).view());
            ok = v.page && got == std::string(v.page, v.page_size);
        }
        if (!ok && ++bad <= 20) {
            ADD_FAILURE() << v.source << " | " << v.data << "\n  got  " << got << "\n  want "
                          << (v.page ? v.page : "ERROR");
        }
    }
    EXPECT_EQ(bad, 0u) << "of " << std::size(HtmlStencilVectors);
}

TEST(HtmlStencil_Tests, Contexts) {
    txt::object d{{"url", "javascript:alert(1)"}, {"title", "\" onmouseover=\"x"}, {"name", "<b>"},
                  {"user", txt::object{{"id", 7}, {"name", "Ada"}}}};
    EXPECT_EQ(page(R"(<a href="{{ url }}" title="{{ title }}">{{ name }}</a>)", d),
              s(R"(<a href="#ZgotmplZ" title="&#34; onmouseover=&#34;x">&lt;b&gt;</a>)"));
    // an object in a script: JSON, in the order its fields were set
    EXPECT_EQ(page("<script>var user = {{ user }};</script>", d), s(R"(<script>var user = {"id":7,"name":"Ada"};</script>)"));
    EXPECT_EQ(page("<script>var n = {{ user.id }};</script>", d), s("<script>var n =  7 ;</script>"));
    // the bare names of stencil work as Go's dotted ones
    EXPECT_EQ(page("<p title={{ name }}>", d), s("<p title=&lt;b&gt;>"));
    EXPECT_EQ(page("{{ range user }}<li>{{ key }}={{ value }}</li>{{ end }}", d), s("<li>id=7</li><li>name=Ada</li>"));
    // stencil's specifications are kept: the text they write is escaped
    EXPECT_EQ(page("<b>{{ name:>5 }}</b>", d), s("<b>  &lt;b&gt;</b>"));
    // trusted values
    EXPECT_EQ(page("{{ name | safe_html }}", d), s("<b>"));
    EXPECT_EQ(page("<a title=\"{{ name | safe_html }}\">", d), s("<a title=\"\">"));    // tags stripped in an attribute
    EXPECT_EQ(page("<a href=\"{{ url | safe_url }}\">", d), s("<a href=\"javascript:alert%281%29\">"));
    EXPECT_EQ(page("<script>{{ url | safe_js }}</script>", d), s("<script>javascript:alert(1)</script>"));
    EXPECT_EQ(page("<a {{ title | safe_attr }}>", txt::object{{"title", "x=1"}}), s("<a x=1>"));
    EXPECT_EQ(page("<p style=\"color: {{ c | safe_css }}\">", txt::object{{"c", "rgb(1,2,3)"}}),
              s("<p style=\"color: rgb(1,2,3)\">"));
    // a trusted value is not trusted in another context
    EXPECT_EQ(page("<a href=\"{{ url | safe_html }}\">", d), s("<a href=\"#ZgotmplZ\">"));
    // comments in the template's text are dropped
    EXPECT_EQ(page("a<!-- {{ name }} -->b<script>/* c */x // d\n</script><style>/* e */p{}</style>", d),
              s("ab<script> x \n</script><style> p{}</style>"));
    // nothing, and a missing name
    EXPECT_EQ(page("<a title=\"{{ nothing }}\">{{ missing }}</a>", d), s("<a title=\"\"></a>"));
}

TEST(HtmlStencil_Tests, Refused) {
    struct Bad {
        const char* source;
        size_t line;
    };
    const Bad cases[] = {
        {"{{ if b }}<a href=\"{{ else }}<b>{{ end }}", 1},     // branches in different contexts
        {"{{ if b }}<script>{{ end }}x", 1},
        {"{{ range l }}<a href=\"{{ end }}", 1},               // a loop that does not come back to where it began
        {"<a href=\"{{ u }}", 1},                               // the end inside an attribute
        {"line one\n<script>{{ u }}", 2},                       // ... inside a script
        {"<script>var r = /[{{ u }}]/;</script>", 1},           // a field in a character class
        {"<a \"x\">", 1},                                       // a quote where a name was expected
        {"<a href=x\"y>", 1},                                   // a quote in an unquoted value
        {"<script>{{ if b }}x = 1{{ else }}return{{ end }} /2/</script>", 1},   // a slash nobody can read
        {"{{ if b }}<a href=\"/x?{{ else }}<a href=\"/x/{{ end }}{{ u }}\">", 1},   // a URL part nobody can tell
    };
    for (const auto& b : cases) {
        auto t = txt::html_stencil::parse(s(b.source));
        ASSERT_FALSE(t.has_value()) << b.source;
        EXPECT_EQ(t.error().line(), b.line) << b.source << ": " << t.error().message();
        EXPECT_FALSE(t.error().message().empty());
    }
    // what stencil refuses is refused the same
    EXPECT_FALSE(txt::html_stencil::parses(s("{{ if }}")));
    EXPECT_THROW(txt::html_stencil(s("<a href=\"{{ u }}")), bad_expected_access<txt::stencil_error>);
    EXPECT_TRUE(txt::html_stencil::parses(s("<p>{{ x }}</p>")));
}

TEST(HtmlStencil_Tests, Boundaries) {
    // the empty template, the default one
    EXPECT_EQ(page("", txt::object{}), s(""));
    txt::html_stencil none;
    EXPECT_EQ(none.render(txt::object{}), s(""));
    EXPECT_EQ(none.source(), s(""));
    // a page longer than the first room of a render, values escaped across its growth
    std::string big;
    for (int i = 0; i < 300; ++i) {
        big += "<p title=\"{{ x }}\">{{ x }}</p>";
    }
    txt::html_stencil t{string(std::string_view(big))};
    string out = t.render(txt::object{{"x", "<&>"}});
    std::string want;
    for (int i = 0; i < 300; ++i) {
        want += "<p title=\"&lt;&amp;&gt;\">&lt;&amp;&gt;</p>";
    }
    EXPECT_EQ(std::string(out.view()), want);
    // render_to: what fits, and the whole size
    char room[8];
    size_t n = t.render_to(slice<char>(room, sizeof room), txt::object{{"x", "<&>"}});
    EXPECT_EQ(n, want.size());
    EXPECT_EQ(std::string(room, sizeof room), want.substr(0, sizeof room));
    // a functions table of the caller's keeps the safe_ functions
    txt::stencil_functions f;
    f.add(string("twice"), [](const txt::value& v, slice<const txt::value>) {
        return txt::value(v.to_string() + v.to_string());
    });
    auto custom = txt::html_stencil::parse(s("{{ x | twice }}|{{ x | twice | safe_html }}"), f);
    ASSERT_TRUE(custom.has_value());
    EXPECT_EQ(custom->render(txt::object{{"x", "<"}}), s("&lt;&lt;|<<"));
    // the source is kept
    EXPECT_EQ(custom->source(), s("{{ x | twice }}|{{ x | twice | safe_html }}"));
}

TEST(HtmlStencil_Tests, SharedAcrossThreads) {
    txt::html_stencil t(s("<a href=\"/u/{{ id }}\" title=\"{{ name }}\">{{ name }}</a>"));
    std::atomic<size_t> wrong{0};
    std::vector<std::thread> threads;
    for (int k = 0; k < 4; ++k) {
        threads.emplace_back([&t, &wrong] {
            for (int i = 0; i < 1000; ++i) {
                string got = t.render(txt::object{{"id", i}, {"name", "<x>"}});
                string want(std::string_view("<a href=\"/u/" + std::to_string(i) +
                                             "\" title=\"&lt;x&gt;\">&lt;x&gt;</a>"));
                wrong += got != want;
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_EQ(wrong.load(), 0u);
}
