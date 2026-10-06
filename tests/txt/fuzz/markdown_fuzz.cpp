//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Markdown on any text. The first byte picks the options; the rest is the
// text, read where libFuzzer put it (never a managed copy, where ASan does
// not see a read past the end). What must hold:
//   - the HTML written from the arena equals the HTML of the managed tree;
//   - safe (the defaults), the HTML read back by the HTML parser holds only
//     the elements Markdown writes, no event handler, and no link or image
//     to javascript:, vbscript: or file:.
// Built with libFuzzer (tests/fuzz/run.sh tests/txt/fuzz/markdown_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/txt/txt.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool lower_starts(std::string_view s, std::string_view p) {
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n')) {
            ++i;
        }
        if (s.size() - i < p.size()) {
            return false;
        }
        for (size_t k = 0; k < p.size(); ++k) {
            char c = s[i + k] >= 'A' && s[i + k] <= 'Z' ? char(s[i + k] + 32) : s[i + k];
            if (c != p[k]) {
                return false;
            }
        }
        return true;
    }

    void safe_html(const string& out) {
        static const char* const allowed[] = {"html", "head", "body", "p", "h1", "h2", "h3", "h4", "h5", "h6", "ul",
                                              "ol", "li", "blockquote", "pre", "code", "hr", "table", "thead",
                                              "tbody", "tr", "th", "td", "em", "strong", "del", "a", "img", "br",
                                              "input"};
        txt::html_options po;
        po.scripting = false;
        auto d = txt::detail::HtmlBuilder::parse_fragment(out.view(), "body", po);
        vector<txt::html_node> stack;
        stack.push_back(d.root());
        while (!stack.empty()) {
            txt::html_node n = stack.back();
            stack.pop_back();
            if (n.kind() == txt::html_node_kind::element) {
                bool ok = false;
                for (const char* a : allowed) {
                    ok = ok || n.name() == string(a);
                }
                check(ok);
                for (const auto& at : n.attributes()) {
                    check(!lower_starts(at.name.view(), "on"));
                    if (at.name == string("href") || at.name == string("src")) {
                        check(!lower_starts(at.value.view(), "javascript:") &&
                              !lower_starts(at.value.view(), "vbscript:") && !lower_starts(at.value.view(), "file:"));
                    }
                }
            }
            for (size_t k = 0; k < n.size(); ++k) {
                stack.push_back(n[k]);
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 65536) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view text(reinterpret_cast<const char*>(data + 1), size - 1);
    txt::markdown_options o;
    o.tables = mode & 1;
    o.strikethrough = mode & 2;
    o.autolinks = mode & 4;
    o.task_lists = mode & 8;
    o.hard_breaks = mode & 16;
    bool raw = mode & 32;
    o.raw_html = raw;
    std::string arena = txt::detail::md::to_html(text, o);
    auto doc = txt::detail::MarkdownBuilder::parse(text, o);
    string tree = doc.to_html();
    check(tree.view() == std::string_view(arena));
    if (!raw) {
        safe_html(tree);
    }
    return 0;
}
