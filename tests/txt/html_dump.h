//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A parsed tree in html5lib's test format, as tools/html_oracle.c prints
// gumbo's: shared by the tests and tools/html_driver.cpp.
#pragma once

#include "sgcl/txt/detail/html_tree.h"

#include <algorithm>
#include <string>
#include <vector>

namespace html_dump {
    using namespace sgcl::txt::detail::html;

    inline void line(std::string& out, int depth) {
        out += "| ";
        for (int i = 0; i < depth; ++i) {
            out += "  ";
        }
    }

    // a foreign attribute's display: xlink/xml/xmlns prefix and local name
    inline std::string attr_display(const Node& e, const std::string& name) {
        if (e.ns != NsHtml) {
            static const char* const prefixed[] = {"xlink:actuate", "xlink:arcrole", "xlink:href", "xlink:role",
                                                   "xlink:show", "xlink:title", "xlink:type", "xml:lang", "xml:space",
                                                   "xmlns:xlink"};
            for (const char* p : prefixed) {
                if (name == p) {
                    std::string s = name;
                    s[s.find(':')] = ' ';
                    return s;
                }
            }
            if (name == "xmlns") {
                return "xmlns xmlns";
            }
        }
        return name;
    }

    inline void node(std::string& out, const Tree& t, int i, int depth) {
        const Node& e = t.nodes[size_t(i)];
        switch (e.kind) {
            case KElement: {
                line(out, depth);
                out += e.ns == NsSvg ? "<svg " : e.ns == NsMath ? "<math " : "<";
                out += e.name + ">\n";
                std::vector<std::pair<std::string, std::string>> attrs;
                for (const Attr& a : e.attrs) {
                    attrs.emplace_back(attr_display(e, a.name), a.value);
                }
                std::sort(attrs.begin(), attrs.end());
                for (auto& a : attrs) {
                    line(out, depth + 1);
                    out += a.first + "=\"" + a.second + "\"\n";
                }
                int d = depth + 1;
                if (e.content >= 0) {
                    line(out, depth + 1);
                    out += "content\n";
                    for (int c = t.nodes[size_t(e.content)].first; c >= 0; c = t.nodes[size_t(c)].next) {
                        node(out, t, c, depth + 2);
                    }
                }
                for (int c = e.first; c >= 0; c = t.nodes[size_t(c)].next) {
                    node(out, t, c, d);
                }
                break;
            }
            case KText:
                line(out, depth);
                out += "\"" + e.data + "\"\n";
                break;
            case KComment:
                line(out, depth);
                out += "<!-- " + e.data + " -->\n";
                break;
            case KDoctype:
                line(out, depth);
                if (!e.public_id.empty() || !e.system_id.empty()) {
                    out += "<!DOCTYPE " + e.data + " \"" + e.public_id + "\" \"" + e.system_id + "\">\n";
                } else {
                    out += "<!DOCTYPE " + e.data + ">\n";
                }
                break;
            default:
                break;
        }
    }

    inline std::string tree(const Tree& t) {
        std::string out;
        const Node& doc = t.nodes[size_t(t.document)];
        for (int c = doc.first; c >= 0; c = t.nodes[size_t(c)].next) {
            node(out, t, c, 0);
        }
        return out;
    }
}
