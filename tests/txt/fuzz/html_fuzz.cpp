//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The HTML parser and the sanitizer on any input. The first byte picks the
// path (and a fragment's context); the rest is the input, read from
// libFuzzer's own buffer (never a managed copy, where ASan does not see a read
// past the end). What must hold:
//   - a document or a fragment of any bytes parses into a tree whose every
//     node is its parent's child at its index, whose serialization is valid
//     UTF-8, and whose serialization parses again without failing;
//   - what sanitize_html writes, parsed again, holds no element outside the
//     allowlist, no event handler, no style attribute and no URL of a scheme
//     other than http, https or mailto — whatever the input did to the tree.
// Built with libFuzzer (tests/fuzz/run.sh tests/txt/fuzz/html_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/txt/txt.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void well_formed(const txt::html_node& root) {
        vector<txt::html_node> stack;   // handles in managed memory
        stack.push_back(root);
        size_t seen = 0;
        while (!stack.empty() && seen < 1000000) {
            txt::html_node n = stack.back();
            stack.pop_back();
            ++seen;
            for (size_t k = 0; k < n.size(); ++k) {
                txt::html_node c = n[k];
                check(bool(c));
                check(c.parent() == n);
                if (k > 0) {
                    check(c.previous_sibling() == n[k - 1]);
                }
                stack.push_back(c);
            }
            if (n.content()) {
                stack.push_back(n.content());
            }
        }
    }

    const char* const Contexts[] = {"body", "div", "table", "tr", "select", "textarea", "script", "title", "template",
                                    "svg", "html", "head", "frameset", "plaintext"};

    void parse_path(std::string_view in, uint8_t mode) {
        txt::html_options o;
        o.scripting = mode & 1;
        o.collect_errors = mode & 2;
        txt::html_document d = (mode & 4)
            ? txt::detail::HtmlBuilder::parse_fragment(in, Contexts[(mode >> 3) % std::size(Contexts)], o)
            : txt::detail::HtmlBuilder::parse(in, o);
        well_formed(d.root());
        string html = d.to_string();
        check(utf8::valid(html.view()));
        txt::html_document again = txt::html_document::parse(html, o);
        well_formed(again.root());
        (void)d.title();
        (void)d.root().elements();
    }

    bool lower_starts(std::string_view s, std::string_view p) {
        if (s.size() < p.size()) {
            return false;
        }
        for (size_t i = 0; i < p.size(); ++i) {
            char c = s[i] >= 'A' && s[i] <= 'Z' ? char(s[i] + 32) : s[i];
            if (c != p[i]) {
                return false;
            }
        }
        return true;
    }

    void sanitize_path(std::string_view in, uint8_t mode) {
        txt::html_sanitizer_options o;
        o.nofollow = mode & 1;
        o.keep_comments = mode & 2;
        string out = txt::detail::HtmlSanitizer::run(in, o);
        check(utf8::valid(out.view()));
        txt::html_options po;
        po.scripting = false;
        auto d = txt::html_document::parse_fragment(out, string("body"), po);
        static const char* const allowed[] = {"a", "abbr", "acronym", "address", "article", "aside", "b", "bdi", "bdo",
            "blockquote", "br", "caption", "cite", "code", "col", "colgroup", "dd", "del", "details", "dfn", "div", "dl",
            "dt", "em", "figcaption", "figure", "footer", "h1", "h2", "h3", "h4", "h5", "h6", "header", "hr", "i", "img",
            "ins", "kbd", "li", "mark", "ol", "p", "pre", "q", "rp", "rt", "ruby", "s", "samp", "section", "small",
            "span", "strike", "strong", "sub", "summary", "sup", "table", "tbody", "td", "tfoot", "th", "thead", "time",
            "tr", "tt", "u", "ul", "var", "wbr"};
        for (const txt::html_node& e : d.root().elements()) {
            check(e.ns() == txt::html_namespace::html);
            bool ok = false;
            for (const char* a : allowed) {
                ok = ok || e.name().view() == a;
            }
            check(ok);
            for (const auto& a : e.attributes()) {
                std::string_view n = a.name.view();
                check(!(n.size() >= 2 && (n[0] | 32) == 'o' && (n[1] | 32) == 'n'));
                check(n != "style");
                if (n == "href" || n == "src" || n == "cite") {
                    std::string v;
                    for (char c : a.value.view()) {
                        if ((unsigned char)c > 0x20) {
                            v.push_back(c);
                        }
                    }
                    size_t colon = v.find(':');
                    size_t stop = v.find_first_of("/?#");
                    if (colon != std::string::npos && (stop == std::string::npos || colon < stop)) {
                        check(lower_starts(v, "http:") || lower_starts(v, "https:") || lower_starts(v, "mailto:"));
                    }
                }
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 16384) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    if (mode & 0x80) {
        sanitize_path(rest, mode);
    } else {
        parse_path(rest, mode);
    }
    return 0;
}
