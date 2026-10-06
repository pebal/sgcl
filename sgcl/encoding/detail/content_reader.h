//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../content_line.h"
#include "../error.h"
#include "utf8_check.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// What iCalendar and vCard share past one line: the text unfolded and its
// BEGIN:x ... END:x read into a tree of components
namespace sgcl::encoding::detail {
    // A component as read: its name, its lines, the indexes of the
    // components inside it (in ContentTree::nodes)
    struct ContentNode {
        string name;
        vector<content_line> lines;
        vector<size_t> children;
    };

    struct ContentTree {
        vector<ContentNode> nodes;
        vector<size_t> roots;   // the top components, in order
    };

    struct ContentLimits {
        uint32_t max_depth = 64;
        size_t max_size = size_t(64) << 20;
    };

    // The text's components: lines unfolded (CRLF or LF, then one space or
    // tab), every line inside a BEGIN/END, blank lines passed over. The
    // place of an error is the start of its physical line, or the
    // character within it on the first physical line
    inline expected<ContentTree, error> content_read(const string& text, bool vcard, const ContentLimits& limits) noexcept {
        std::string_view s = text.view();
        auto fail = [&](errc code, size_t at, const std::string& why) {
            error e(code, at, string(why));
            return expected<ContentTree, error>(unexpected<error>(std::move(e.locate(text))));
        };
        if (s.size() > limits.max_size) {
            return fail(errc::limit_exceeded, 0, "a text past max_size");
        }
        if (s.find('\0') != std::string_view::npos) {
            return fail(errc::invalid_character, s.find('\0'), "a null character");
        }
        ContentTree tree;
        std::vector<size_t> open;   // the indexes of the components not ended
        size_t at = s.substr(0, 3) == "\xEF\xBB\xBF" ? 3 : 0;
        std::string line;
        while (at < s.size()) {
            size_t start = at;
            line.clear();
            // a logical line: physical lines joined, each continuation's first blank taken off
            for (;;) {
                size_t end = s.find('\n', at);
                size_t next = end == std::string_view::npos ? s.size() : end + 1;
                size_t stop = end == std::string_view::npos ? s.size() : end;
                if (stop > at && s[stop - 1] == '\r') {
                    --stop;
                }
                line.append(s.substr(at, stop - at));
                at = next;
                if (at < s.size() && (s[at] == ' ' || s[at] == '\t')) {
                    ++at;
                    continue;
                }
                break;
            }
            if (line.empty()) {
                continue;
            }
            // UTF-8 checked unfolded: a fold may fall inside a sequence (RFC 5545 §3.1)
            if (!utf8_text_valid(line.data(), line.data() + line.size())) {
                return fail(errc::invalid_utf8, start, "invalid UTF-8");
            }
            string group, name, value;
            vector<content_line::parameter> params;
            ContentLineParse pe;
            if (!cl_parse(line, vcard, group, name, params, value, pe)) {
                // a place on the first physical line is exact, past it the line's start
                size_t first_len = s.find('\n', start);
                first_len = (first_len == std::string_view::npos ? s.size() : first_len) - start;
                return fail(pe.code, start + (pe.error_at < first_len ? pe.error_at : 0), pe.why);
            }
            if (name.view() == "BEGIN") {
                if (open.size() >= limits.max_depth) {
                    return fail(errc::depth_limit, start, "components nested deeper than max_depth");
                }
                std::string component = cl_upper(value.view());
                if (!cl_valid_name(component)) {
                    return fail(errc::syntax, start, "a BEGIN of no component's name");
                }
                tree.nodes.push_back(ContentNode{string(component), {}, {}});
                size_t index = tree.nodes.size() - 1;
                if (open.empty()) {
                    tree.roots.push_back(index);
                } else {
                    tree.nodes[open.back()].children.push_back(index);
                }
                open.push_back(index);
                continue;
            }
            if (name.view() == "END") {
                if (open.empty()) {
                    return fail(errc::mismatched_tag, start, "an END without its BEGIN");
                }
                std::string component = cl_upper(value.view());
                if (tree.nodes[open.back()].name.view() != component) {
                    return fail(errc::mismatched_tag, start, "END:" + component + " where END:" + std::string(tree.nodes[open.back()].name.view()) + " belongs");
                }
                open.pop_back();
                continue;
            }
            if (open.empty()) {
                return fail(errc::syntax, start, "a line outside BEGIN and END");
            }
            tree.nodes[open.back()].lines.push_back(ContentLineAccess::make(std::move(group), std::move(name), std::move(params), std::move(value)));
        }
        if (!open.empty()) {
            return fail(errc::unexpected_end, s.size(), "BEGIN:" + std::string(tree.nodes[open.back()].name.view()) + " without its END");
        }
        return tree;
    }
}
