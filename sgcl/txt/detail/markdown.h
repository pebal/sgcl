//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The parser of sgcl/txt/markdown.h: CommonMark 0.31.2 by the strategy of its
// appendix — the blocks of a line by line, each line matched against the
// open blocks and then tried for new ones, the inlines of each leaf by the
// delimiter stack and the bracket stack — and the extensions of GitHub
// Flavored Markdown. The tree is built in an arena of indices.

#include "../../core/unicode.h"
#include "../../core/utf8.h"
#include "../case.h"
#include "../properties.h"
#include "html_entities.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sgcl::txt::detail::md {
    using kind = markdown_kind;

    // ---- characters ----

    inline bool is_space_tab(char c) noexcept {
        return c == ' ' || c == '\t';
    }

    inline bool is_ascii_punct(char c) noexcept {
        unsigned char u = static_cast<unsigned char>(c);
        return (u >= 0x21 && u <= 0x2F) || (u >= 0x3A && u <= 0x40) || (u >= 0x5B && u <= 0x60) ||
               (u >= 0x7B && u <= 0x7E);
    }

    inline bool is_ascii_alpha(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    inline bool is_ascii_digit(char c) noexcept {
        return c >= '0' && c <= '9';
    }

    inline bool is_ascii_alnum(char c) noexcept {
        return is_ascii_alpha(c) || is_ascii_digit(c);
    }

    inline char ascii_lower(char c) noexcept {
        return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
    }

    // Unicode white space (Zs, tab, line feed, form feed, carriage return)
    inline bool is_unicode_space(char32_t c) noexcept {
        if (c < 0x80) {
            return c == ' ' || c == '\t' || c == '\n' || c == '\f' || c == '\r';
        }
        return category_of_fn(c) == category::space_separator;
    }

    // Unicode punctuation: the categories P and S (CommonMark 0.31)
    inline bool is_unicode_punct(char32_t c) noexcept {
        if (c < 0x80) {
            return is_ascii_punct(char(c));
        }
        return is_category(c, category::connector_punctuation, category::other_symbol);
    }

    inline void put_utf8(std::string& out, char32_t c) {
        if (c < 0x80) {
            out.push_back(char(c));
        } else if (c < 0x800) {
            out.push_back(char(0xC0 | (c >> 6)));
            out.push_back(char(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            out.push_back(char(0xE0 | (c >> 12)));
            out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(char(0x80 | (c & 0x3F)));
        } else {
            out.push_back(char(0xF0 | (c >> 18)));
            out.push_back(char(0x80 | ((c >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(char(0x80 | (c & 0x3F)));
        }
    }

    // An entity or a numeric reference at s[i] ('&'): its text appended and
    // its length, or 0 when there is none
    inline size_t entity(std::string_view s, size_t i, std::string& out) {
        if (i + 2 >= s.size() || s[i] != '&') {
            return 0;
        }
        if (s[i + 1] == '#') {
            size_t j = i + 2;
            bool hex = j < s.size() && (s[j] == 'x' || s[j] == 'X');
            if (hex) {
                ++j;
            }
            size_t begin = j;
            uint32_t v = 0;
            while (j < s.size() && (hex ? std::isxdigit(static_cast<unsigned char>(s[j])) : is_ascii_digit(s[j]))) {
                if (j - begin >= (hex ? 6 : 7)) {
                    return 0;
                }
                char c = s[j];
                v = v * (hex ? 16 : 10) + uint32_t(is_ascii_digit(c) ? c - '0' : ascii_lower(c) - 'a' + 10);
                ++j;
            }
            if (j == begin || j >= s.size() || s[j] != ';') {
                return 0;
            }
            if (v == 0 || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF)) {
                v = 0xFFFD;
            }
            put_utf8(out, char32_t(v));
            return j + 1 - i;
        }
        size_t j = i + 1;
        while (j < s.size() && is_ascii_alnum(s[j]) && j - i <= 32) {
            ++j;
        }
        if (j == i + 1 || j >= s.size() || s[j] != ';') {
            return 0;
        }
        std::string_view name = s.substr(i + 1, j - i);   // with its semicolon
        const auto* lo = std::begin(html::NamedReferences);
        const auto* hi = std::end(html::NamedReferences);
        const auto* it = std::lower_bound(lo, hi, name, [](const html::NamedReference& r, std::string_view k) {
            return r.name < k;
        });
        if (it == hi || it->name != name) {
            return 0;
        }
        put_utf8(out, it->first);
        if (it->second) {
            put_utf8(out, it->second);
        }
        return j + 1 - i;
    }

    // Backslash escapes and entities of a text resolved (a link's
    // destination, title, a code block's info string)
    inline std::string unescape(std::string_view s) {
        std::string out;
        for (size_t i = 0; i < s.size();) {
            if (s[i] == '\\' && i + 1 < s.size() && is_ascii_punct(s[i + 1])) {
                out.push_back(s[i + 1]);
                i += 2;
            } else if (s[i] == '&') {
                size_t n = entity(s, i, out);
                if (n) {
                    i += n;
                } else {
                    out.push_back('&');
                    ++i;
                }
            } else {
                out.push_back(s[i++]);
            }
        }
        return out;
    }

    // A link label made a key: case folded (Unicode's full folding),
    // white space collapsed to one space and trimmed
    inline std::string label_key(std::string_view s) {
        std::string out;
        bool space = false;
        for (size_t i = 0; i < s.size();) {
            auto [c, n] = utf8::decode(s, i);
            i += n;
            if (is_unicode_space(c)) {
                space = !out.empty();
                continue;
            }
            if (space) {
                out.push_back(' ');
                space = false;
            }
            if (c < 0x80) {
                out.push_back(ascii_lower(char(c)));
            } else if (auto d = full_of(c, case_tables::FullFold)) {
                std::vector<char32_t> cps;
                append(cps, d);
                for (char32_t x : cps) {
                    put_utf8(out, x);
                }
            } else {
                put_utf8(out, unicode::to_lower(c));
            }
        }
        return out;
    }

    // ---- the tree ----

    // A node: small, the rare parts aside (a link's destination and title,
    // a code block's info string in Tree::extras; what reading blocks needs
    // in Blocks)
    struct Node {
        kind k = kind::document;
        markdown_align align = markdown_align::none;
        int8_t checked = -1;
        bool open = true;
        bool hidden = false;     // a paragraph of only link reference definitions
        bool ordered = false;
        bool tight = true;
        char marker = 0;         // a list's bullet ('.' or ')' if ordered); 1 on a delimiter's text
        bool in_chars = false;   // the literal is in Tree::chars, not Tree::raw
        int parent = -1, first = -1, last = -1, prev = -1, next = -1;
        int level = 0;
        int start = 1;
        int extra = -1;          // in Tree::extras
        int state = -1;          // in Blocks::states
        uint32_t lit = 0, lit_len = 0;   // the literal: in Tree::raw (a slice of a leaf's content) or Tree::chars
    };

    struct Extra {
        std::string url, title, info;
    };

    struct Tree {
        std::vector<Node> nodes;
        std::vector<Extra> extras;
        std::string raw;     // the leaves' content while blocks are read, code and raw HTML
        std::string chars;   // the inlines' texts

        std::string_view literal(int n) const noexcept {
            const Node& x = nodes[size_t(n)];
            return std::string_view(x.in_chars ? chars : raw).substr(x.lit, x.lit_len);
        }

        bool in_raw(std::string_view piece) const noexcept {
            return !raw.empty() && piece.data() >= raw.data() && piece.data() + piece.size() <= raw.data() + raw.size();
        }

        // a slice of the raw content, without a copy
        void raw_slice(int n, std::string_view piece) noexcept {
            Node& x = nodes[size_t(n)];
            x.in_chars = false;
            x.lit = uint32_t(piece.data() - raw.data());
            x.lit_len = uint32_t(piece.size());
        }

        // inline text after n's: joined in place when it follows n's in the
        // raw content, else n's text moved to the end of chars and joined there
        void join(int n, std::string_view more) {
            Node& x = nodes[size_t(n)];
            if (!x.in_chars && in_raw(more) && raw.data() + x.lit + x.lit_len == more.data()) {
                x.lit_len += uint32_t(more.size());
                return;
            }
            to_chars_end(n);
            chars.append(more);
            nodes[size_t(n)].lit_len += uint32_t(more.size());
        }

        // n's text made the last of chars
        void to_chars_end(int n) {
            Node& x = nodes[size_t(n)];
            if (x.in_chars && x.lit + x.lit_len == chars.size()) {
                return;
            }
            uint32_t at = uint32_t(chars.size());
            if (x.in_chars) {
                chars.append(chars, x.lit, x.lit_len);
            } else {
                chars.append(raw, x.lit, x.lit_len);
            }
            x.in_chars = true;
            x.lit = at;
        }

        // the pool's text of n made to end the pool, then more appended
        static void grow(std::string& pool, Node& x, std::string_view more) {
            if (x.lit_len == 0) {
                x.lit = uint32_t(pool.size());
            } else if (x.lit + x.lit_len != pool.size()) {
                size_t from = x.lit;
                x.lit = uint32_t(pool.size());
                pool.append(pool, from, x.lit_len);   // a copy of itself: std::string allows it
            }
            pool.append(more);
            x.lit_len += uint32_t(more.size());
        }

        void raw_append(int n, std::string_view more) {
            grow(raw, nodes[size_t(n)], more);
        }

        void chars_append(int n, std::string_view more) {
            grow(chars, nodes[size_t(n)], more);
        }

        void chars_set(int n, std::string_view value) {
            Node& x = nodes[size_t(n)];
            x.in_chars = true;
            x.lit = uint32_t(chars.size());
            x.lit_len = uint32_t(value.size());
            chars.append(value);
        }

        void raw_set(int n, std::string_view value) {
            Node& x = nodes[size_t(n)];
            x.lit = uint32_t(raw.size());
            x.lit_len = uint32_t(value.size());
            raw.append(value);
        }

        // the literal's first and last bytes dropped
        void trim(int n, size_t front, size_t back) noexcept {
            Node& x = nodes[size_t(n)];
            x.lit += uint32_t(front);
            x.lit_len -= uint32_t(front + back);
        }

        Extra& ex(int n) {
            Node& x = nodes[size_t(n)];
            if (x.extra < 0) {
                x.extra = int(extras.size());
                extras.emplace_back();
            }
            return extras[size_t(x.extra)];
        }

        std::string_view url(int n) const noexcept {
            int e = nodes[size_t(n)].extra;
            return e < 0 ? std::string_view() : std::string_view(extras[size_t(e)].url);
        }

        std::string_view title(int n) const noexcept {
            int e = nodes[size_t(n)].extra;
            return e < 0 ? std::string_view() : std::string_view(extras[size_t(e)].title);
        }

        std::string_view info(int n) const noexcept {
            int e = nodes[size_t(n)].extra;
            return e < 0 ? std::string_view() : std::string_view(extras[size_t(e)].info);
        }

        int make(kind k) {
            nodes.emplace_back();
            nodes.back().k = k;
            return int(nodes.size() - 1);
        }

        void append_child(int parent, int c) {
            Node& p = nodes[size_t(parent)];
            Node& n = nodes[size_t(c)];
            n.parent = parent;
            n.prev = p.last;
            n.next = -1;
            if (p.last >= 0) {
                nodes[size_t(p.last)].next = c;
            } else {
                p.first = c;
            }
            nodes[size_t(parent)].last = c;
        }

        void unlink(int c) {
            Node& n = nodes[size_t(c)];
            if (n.prev >= 0) {
                nodes[size_t(n.prev)].next = n.next;
            } else if (n.parent >= 0) {
                nodes[size_t(n.parent)].first = n.next;
            }
            if (n.next >= 0) {
                nodes[size_t(n.next)].prev = n.prev;
            } else if (n.parent >= 0) {
                nodes[size_t(n.parent)].last = n.prev;
            }
            n.parent = n.prev = n.next = -1;
        }

        void insert_after(int ref, int c) {
            Node& r = nodes[size_t(ref)];
            Node& n = nodes[size_t(c)];
            n.parent = r.parent;
            n.prev = ref;
            n.next = r.next;
            if (r.next >= 0) {
                nodes[size_t(r.next)].prev = c;
            } else if (r.parent >= 0) {
                nodes[size_t(r.parent)].last = c;
            }
            nodes[size_t(ref)].next = c;
        }

        Node& operator[](int i) {
            return nodes[size_t(i)];
        }
    };

    struct Reference {
        std::string url, title;
    };

    using References = std::unordered_map<std::string, Reference>;

    // ---- link pieces, shared by the definitions and the inline links ----

    // A link destination at s[i]: its unescaped text and the end, or npos
    inline size_t scan_destination(std::string_view s, size_t i, std::string& out) {
        if (i < s.size() && s[i] == '<') {
            size_t j = i + 1;
            while (j < s.size()) {
                char c = s[j];
                if (c == '>') {
                    out = unescape(s.substr(i + 1, j - i - 1));
                    return j + 1;
                }
                if (c == '<' || c == '\n' || c == '\r') {
                    return std::string_view::npos;
                }
                j += c == '\\' && j + 1 < s.size() && is_ascii_punct(s[j + 1]) ? 2 : 1;
            }
            return std::string_view::npos;
        }
        size_t j = i;
        int depth = 0;
        while (j < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[j]);
            if (c == '\\' && j + 1 < s.size() && is_ascii_punct(s[j + 1])) {
                j += 2;
                continue;
            }
            if (c <= 0x20 || c == 0x7F) {
                break;
            }
            if (c == '(') {
                if (++depth > 32) {
                    return std::string_view::npos;
                }
            } else if (c == ')') {
                if (depth == 0) {
                    break;
                }
                --depth;
            }
            ++j;
        }
        if (j == i || depth != 0) {
            return std::string_view::npos;
        }
        out = unescape(s.substr(i, j - i));
        return j;
    }

    // A link title at s[i]: "…", '…' or (…), its unescaped text and the end, or npos
    inline size_t scan_title(std::string_view s, size_t i, std::string& out) {
        if (i >= s.size()) {
            return std::string_view::npos;
        }
        char open = s[i];
        char close = open == '(' ? ')' : open;
        if (open != '"' && open != '\'' && open != '(') {
            return std::string_view::npos;
        }
        size_t j = i + 1;
        while (j < s.size()) {
            char c = s[j];
            if (c == '\\' && j + 1 < s.size() && is_ascii_punct(s[j + 1])) {
                j += 2;
                continue;
            }
            if (c == close) {
                out = unescape(s.substr(i + 1, j - i - 1));
                return j + 1;
            }
            if (open == '(' && c == '(') {
                return std::string_view::npos;
            }
            ++j;
        }
        return std::string_view::npos;
    }

    // A link label at s[i] ('['): the end past ']', or npos; at most 999
    // characters, no unescaped bracket, not only white space
    inline size_t scan_label(std::string_view s, size_t i) {
        if (i >= s.size() || s[i] != '[') {
            return std::string_view::npos;
        }
        size_t j = i + 1;
        bool content = false;
        while (j < s.size()) {
            char c = s[j];
            if (c == '\\' && j + 1 < s.size() && is_ascii_punct(s[j + 1])) {
                content = true;
                j += 2;
            } else if (c == '[') {
                return std::string_view::npos;
            } else if (c == ']') {
                if (!content || j - i - 1 > 999) {
                    return std::string_view::npos;
                }
                return j + 1;
            } else {
                if (!is_space_tab(c) && c != '\n' && c != '\r') {
                    content = true;
                }
                ++j;
            }
            if (j - i - 1 > 999) {
                return std::string_view::npos;
            }
        }
        return std::string_view::npos;
    }

    // white space with at most one line ending
    inline size_t skip_space_one_newline(std::string_view s, size_t i, bool& newline) {
        newline = false;
        while (i < s.size() && is_space_tab(s[i])) {
            ++i;
        }
        if (i < s.size() && s[i] == '\n') {
            newline = true;
            ++i;
            while (i < s.size() && is_space_tab(s[i])) {
                ++i;
            }
        }
        return i;
    }

    // The link reference definitions at the start of a paragraph's content
    // taken out of it into refs (the first of a label wins); the bytes they
    // took
    inline size_t take_definitions(std::string_view s, References& refs) {
        size_t pos = 0;
        while (pos < s.size() && s[pos] == '[') {
            size_t end = scan_label(s, pos);
            if (end == std::string_view::npos || end >= s.size() || s[end] != ':') {
                break;
            }
            std::string label(s.substr(pos + 1, end - pos - 2));
            bool nl;
            size_t i = skip_space_one_newline(s, end + 1, nl);
            std::string url;
            size_t after = scan_destination(s, i, url);
            if (after == std::string_view::npos || (after == i)) {
                break;
            }
            // the destination must be followed by white space or the end
            size_t j = after;
            size_t k = skip_space_one_newline(s, j, nl);
            std::string title;
            size_t t_end = std::string_view::npos;
            if (k > j && k < s.size()) {
                t_end = scan_title(s, k, title);
            }
            size_t line_end = std::string_view::npos;
            if (t_end != std::string_view::npos) {
                size_t e = t_end;
                while (e < s.size() && is_space_tab(s[e])) {
                    ++e;
                }
                if (e >= s.size() || s[e] == '\n') {
                    line_end = e;
                } else {
                    title.clear();
                }
            }
            if (line_end == std::string_view::npos) {
                // no title: the destination ends its line
                size_t e = j;
                while (e < s.size() && is_space_tab(s[e])) {
                    ++e;
                }
                if (e < s.size() && s[e] != '\n') {
                    break;
                }
                if (j < s.size() && e == j && s[j] != '\n') {
                    break;
                }
                line_end = e;
                title.clear();
            }
            std::string key = label_key(label);
            if (!key.empty() && refs.find(key) == refs.end()) {
                refs.emplace(std::move(key), Reference{std::move(url), std::move(title)});
            }
            pos = line_end < s.size() ? line_end + 1 : s.size();
        }
        return pos;
    }

    // ---- blocks ----

    inline bool can_contain(kind parent, kind child) noexcept {
        switch (parent) {
            case kind::document:
            case kind::block_quote:
            case kind::item:
                return child != kind::item;
            case kind::list:
                return child == kind::item;
            default:
                return false;
        }
    }

    // The tag names of an HTML block of type 6
    inline bool block_tag(std::string_view name) noexcept {
        static constexpr std::string_view names[] = {
            "address", "article", "aside", "base", "basefont", "blockquote", "body", "caption", "center", "col",
            "colgroup", "dd", "details", "dialog", "dir", "div", "dl", "dt", "fieldset", "figcaption", "figure",
            "footer", "form", "frame", "frameset", "h1", "h2", "h3", "h4", "h5", "h6", "head", "header", "hr", "html",
            "iframe", "legend", "li", "link", "main", "menu", "menuitem", "nav", "noframes", "ol", "optgroup",
            "option", "p", "param", "search", "section", "summary", "table", "tbody", "td", "tfoot", "th", "thead",
            "title", "tr", "track", "ul"};
        for (std::string_view n : names) {
            if (n.size() == name.size()) {
                bool same = true;
                for (size_t i = 0; i < n.size() && same; ++i) {
                    same = ascii_lower(name[i]) == n[i];
                }
                if (same) {
                    return true;
                }
            }
        }
        return false;
    }

    inline bool starts_with_ci(std::string_view s, size_t i, std::string_view p) noexcept {
        if (s.size() - i < p.size() || i > s.size()) {
            return false;
        }
        for (size_t k = 0; k < p.size(); ++k) {
            if (ascii_lower(s[i + k]) != p[k]) {
                return false;
            }
        }
        return true;
    }

    // ---- raw HTML (6.6), shared by the blocks of type 7 and the inlines ----

    inline size_t skip_html_space(std::string_view s, size_t i) noexcept {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' || s[i] == '\f')) {
            ++i;
        }
        return i;
    }

    // An open tag at s[i] ('<'): its end, or npos
    inline size_t scan_open_tag(std::string_view s, size_t i) noexcept {
        size_t j = i + 1;
        if (j >= s.size() || !is_ascii_alpha(s[j])) {
            return std::string_view::npos;
        }
        while (j < s.size() && (is_ascii_alnum(s[j]) || s[j] == '-')) {
            ++j;
        }
        for (;;) {
            size_t k = skip_html_space(s, j);
            if (k >= s.size()) {
                return std::string_view::npos;
            }
            if (s[k] == '>') {
                return k + 1;
            }
            if (s[k] == '/') {
                return k + 1 < s.size() && s[k + 1] == '>' ? k + 2 : std::string_view::npos;
            }
            if (k == j) {
                return std::string_view::npos;   // an attribute needs white space before it
            }
            // an attribute name
            if (!(is_ascii_alpha(s[k]) || s[k] == '_' || s[k] == ':')) {
                return std::string_view::npos;
            }
            ++k;
            while (k < s.size() && (is_ascii_alnum(s[k]) || s[k] == '_' || s[k] == '.' || s[k] == ':' || s[k] == '-')) {
                ++k;
            }
            j = k;
            // a value
            size_t v = skip_html_space(s, k);
            if (v < s.size() && s[v] == '=') {
                v = skip_html_space(s, v + 1);
                if (v >= s.size()) {
                    return std::string_view::npos;
                }
                if (s[v] == '"' || s[v] == '\'') {
                    size_t e = s.find(s[v], v + 1);
                    if (e == std::string_view::npos) {
                        return std::string_view::npos;
                    }
                    j = e + 1;
                } else {
                    size_t e = v;
                    while (e < s.size() && !std::strchr(" \t\n\r\f\"'=<>`", s[e])) {
                        ++e;
                    }
                    if (e == v) {
                        return std::string_view::npos;
                    }
                    j = e;
                }
            }
        }
    }

    // A closing tag at s[i] ('<'): its end, or npos
    inline size_t scan_close_tag(std::string_view s, size_t i) noexcept {
        size_t j = i + 2;
        if (i + 1 >= s.size() || s[i + 1] != '/' || j >= s.size() || !is_ascii_alpha(s[j])) {
            return std::string_view::npos;
        }
        while (j < s.size() && (is_ascii_alnum(s[j]) || s[j] == '-')) {
            ++j;
        }
        j = skip_html_space(s, j);
        return j < s.size() && s[j] == '>' ? j + 1 : std::string_view::npos;
    }

    // Any raw HTML at s[i] ('<'): a tag, a comment, a processing
    // instruction, a declaration or a CDATA section; its end, or npos
    inline size_t scan_raw_html(std::string_view s, size_t i) noexcept {
        if (i + 1 >= s.size()) {
            return std::string_view::npos;
        }
        char c = s[i + 1];
        if (is_ascii_alpha(c)) {
            return scan_open_tag(s, i);
        }
        if (c == '/') {
            return scan_close_tag(s, i);
        }
        if (c == '?') {
            size_t e = s.find("?>", i + 2);
            return e == std::string_view::npos ? e : e + 2;
        }
        if (c != '!') {
            return std::string_view::npos;
        }
        if (s.substr(i, 4) == "<!--") {
            if (s.substr(i + 4, 1) == ">") {
                return i + 5;
            }
            if (s.substr(i + 4, 2) == "->") {
                return i + 6;
            }
            size_t e = s.find("-->", i + 4);
            return e == std::string_view::npos ? e : e + 3;
        }
        if (s.substr(i, 9) == "<![CDATA[") {
            size_t e = s.find("]]>", i + 9);
            return e == std::string_view::npos ? e : e + 3;
        }
        if (i + 2 < s.size() && is_ascii_alpha(s[i + 2])) {
            size_t e = s.find('>', i + 2);
            return e == std::string_view::npos ? e : e + 1;
        }
        return std::string_view::npos;
    }

    // The cells of a row of a table: split at the pipes not escaped, the
    // outer ones dropped, each trimmed, "\|" made "|"
    inline std::vector<std::string> table_cells(std::string_view row) {
        size_t b = 0, e = row.size();
        while (b < e && is_space_tab(row[b])) {
            ++b;
        }
        while (e > b && is_space_tab(row[e - 1])) {
            --e;
        }
        if (b < e && row[b] == '|') {
            ++b;
        }
        if (e > b && row[e - 1] == '|' && !(e - 1 > b && row[e - 2] == '\\')) {
            --e;
        }
        std::vector<std::string> cells;
        std::string cell;
        for (size_t i = b; i < e; ++i) {
            if (row[i] == '\\' && i + 1 < e && row[i + 1] == '|') {
                cell.push_back('|');
                ++i;
            } else if (row[i] == '|') {
                cells.push_back(std::move(cell));
                cell.clear();
            } else {
                cell.push_back(row[i]);
            }
        }
        cells.push_back(std::move(cell));
        for (std::string& c : cells) {
            size_t x = 0, y = c.size();
            while (x < y && is_space_tab(c[x])) {
                ++x;
            }
            while (y > x && is_space_tab(c[y - 1])) {
                --y;
            }
            c = c.substr(x, y - x);
        }
        return cells;
    }

    // A delimiter row of a table: its alignments, or empty when it is not
    // one (a pipe somewhere, each cell :?-+:?)
    inline std::vector<markdown_align> delimiter_row(std::string_view row) {
        if (row.find('|') == std::string_view::npos) {
            return {};
        }
        std::vector<markdown_align> out;
        for (const std::string& c : table_cells(row)) {
            if (c.empty()) {
                return {};
            }
            size_t i = 0, n = c.size();
            bool left = c[0] == ':', right = c[n - 1] == ':';
            if (left) {
                ++i;
            }
            if (right && n > (left ? 1u : 0u)) {
                --n;
            }
            if (i >= n) {
                return {};
            }
            for (size_t k = i; k < n; ++k) {
                if (c[k] != '-') {
                    return {};
                }
            }
            out.push_back(left && right ? markdown_align::center
                          : left        ? markdown_align::left
                          : right       ? markdown_align::right
                                        : markdown_align::none);
        }
        return out;
    }

    // What reading blocks keeps of a block
    struct BlockState {
        int start_line = 0, end_line = 0;
        int marker_offset = 0, padding = 0;
        bool fenced = false;
        char fence_char = 0;
        int fence_length = 0, fence_offset = 0;
        int html_type = 0;
        bool item_blank_start = false;   // an item whose first line was only its marker
        std::vector<markdown_align> aligns;
    };

    class Blocks {
    public:
        Blocks(Tree& tree, References& refs, const markdown_options& o)
        : t(tree)
        , refs(refs)
        , o(o) {
            doc = t.make(kind::document);
            states.reserve(256);
            tip = doc;
        }

        int doc = -1;
        std::vector<int> leaves;   // paragraphs, headings and table cells, in order

        void run(std::string_view text) {
            size_t i = 0;
            bool cr = text.find('\r') != std::string_view::npos;
            while (i < text.size()) {
                size_t e;
                if (!cr) {
                    e = text.find('\n', i);
                    e = e == std::string_view::npos ? text.size() : e;
                } else {
                    e = i;
                    while (e < text.size() && text[e] != '\n' && text[e] != '\r') {
                        ++e;
                    }
                }
                line(text.substr(i, e - i));
                if (e < text.size() && text[e] == '\r' && e + 1 < text.size() && text[e + 1] == '\n') {
                    ++e;
                }
                i = e + 1;
            }
            while (tip >= 0) {
                int p = t[tip].parent;
                finalize(tip);
                tip = p;
            }
        }

    private:
        Tree& t;
        References& refs;
        const markdown_options& o;
        int tip = -1;
        int line_no = 0;
        std::string_view ln;
        size_t offset = 0, column = 0, first_nonspace = 0, first_nonspace_column = 0, indent = 0;
        bool blank = false, partial_tab = false;
        int last_matched = -1;
        bool unmatched_closed = false;
        int marked = -1;   // the deepest block whose marker this line held ('>' or a list marker)
        std::vector<BlockState> states;

        BlockState& st(int n) {
            Node& x = t[n];
            if (x.state < 0) {
                x.state = int(states.size());
                states.emplace_back();
            }
            return states[size_t(x.state)];
        }
        int uniform_line = 0;
        size_t uniform_from = 0;
        char uniform_char = 0;

        char peek(size_t i) const noexcept {
            return i < ln.size() ? ln[i] : '\0';
        }

        void find_nonspace() {
            size_t i = offset, col = column;
            if (partial_tab && i < ln.size() && ln[i] == '\t') {
                col += 4 - col % 4;
                ++i;
            }
            while (i < ln.size()) {
                if (ln[i] == ' ') {
                    ++col;
                } else if (ln[i] == '\t') {
                    col += 4 - col % 4;
                } else {
                    break;
                }
                ++i;
            }
            first_nonspace = i;
            first_nonspace_column = col;
            indent = col - column;
            blank = i >= ln.size();
        }

        void advance(size_t count, bool columns) {
            while (count > 0 && offset < ln.size()) {
                if (ln[offset] == '\t') {
                    size_t to_tab = 4 - column % 4;
                    if (columns) {
                        partial_tab = to_tab > count;
                        size_t n = std::min(to_tab, count);
                        column += n;
                        offset += partial_tab ? 0 : 1;
                        count -= n;
                    } else {
                        partial_tab = false;
                        column += to_tab;
                        ++offset;
                        --count;
                    }
                } else {
                    partial_tab = false;
                    ++offset;
                    ++column;
                    --count;
                }
            }
        }

        void advance_to_nonspace() {
            if (first_nonspace > offset || partial_tab) {
                partial_tab = false;
                offset = first_nonspace;
                column = first_nonspace_column;
            }
        }

        // the rest of the line into a leaf that keeps its white space
        void add_line(int b) {
            if (partial_tab) {
                ++offset;
                t.raw_append(b, std::string_view("    ", 4 - column % 4));
                partial_tab = false;
            }
            if (offset < ln.size()) {
                t.raw_append(b, ln.substr(offset));
            }
            t.raw_append(b, "\n");
        }

        void close_unmatched() {
            if (!unmatched_closed) {
                while (tip != last_matched) {
                    int p = t[tip].parent;
                    finalize(tip);
                    tip = p;
                }
                unmatched_closed = true;
            }
        }

        int add_child(int parent, kind k) {
            close_unmatched();
            while (!can_contain(t[parent].k, k)) {
                int up = t[parent].parent;
                finalize(parent);
                parent = up;
            }
            int n = t.make(k);
            t.append_child(parent, n);
            if (k == kind::paragraph || k == kind::heading) {
                leaves.push_back(n);
            }
            st(n).start_line = line_no;
            st(n).end_line = line_no;
            tip = n;
            return n;
        }

        // an item that holds a block (a paragraph of only definitions is gone)
        bool has_content(int item) const {
            for (int c = t.nodes[size_t(item)].first; c >= 0; c = t.nodes[size_t(c)].next) {
                if (!t.nodes[size_t(c)].hidden) {
                    return true;
                }
            }
            return false;
        }

        void close_leaf(int b) {
            finalize(b);
            tip = t[b].parent;
        }

        // ---- the starts of blocks, at first_nonspace ----

        bool atx_heading(int& container) {
            size_t i = first_nonspace;
            int level = 0;
            while (peek(i) == '#' && level < 7) {
                ++level;
                ++i;
            }
            if (level == 0 || level > 6 || (i < ln.size() && !is_space_tab(ln[i]))) {
                return false;
            }
            std::string_view rest = ln.substr(std::min(i, ln.size()));
            size_t b = 0, e = rest.size();
            while (b < e && is_space_tab(rest[b])) {
                ++b;
            }
            while (e > b && is_space_tab(rest[e - 1])) {
                --e;
            }
            // a closing sequence: #s after white space (or the whole rest)
            size_t h = e;
            while (h > b && rest[h - 1] == '#') {
                --h;
            }
            if (h < e && (h == b || is_space_tab(rest[h - 1]))) {
                e = h;
                while (e > b && is_space_tab(rest[e - 1])) {
                    --e;
                }
            }
            int n = add_child(container, kind::heading);
            t[n].level = level;
            t.raw_set(n, rest.substr(b, e - b));
            close_leaf(n);
            container = n;
            return true;
        }

        bool fence_start(int& container) {
            char c = peek(first_nonspace);
            if (c != '`' && c != '~') {
                return false;
            }
            size_t i = first_nonspace;
            while (peek(i) == c) {
                ++i;
            }
            size_t len = i - first_nonspace;
            if (len < 3) {
                return false;
            }
            std::string_view info = ln.substr(i);
            if (c == '`' && info.find('`') != std::string_view::npos) {
                return false;
            }
            size_t b = 0, e = info.size();
            while (b < e && is_space_tab(info[b])) {
                ++b;
            }
            while (e > b && is_space_tab(info[e - 1])) {
                --e;
            }
            int n = add_child(container, kind::code_block);
            st(n).fenced = true;
            st(n).fence_char = c;
            st(n).fence_length = int(len);
            st(n).fence_offset = int(first_nonspace_column - column);
            t.ex(n).info = unescape(info.substr(b, e - b));
            offset = ln.size();
            container = n;
            return true;
        }

        int html_start(bool interrupts_paragraph) const {
            size_t i = first_nonspace;
            if (peek(i) != '<') {
                return 0;
            }
            for (std::string_view tag : {"script", "pre", "style", "textarea"}) {
                if (starts_with_ci(ln, i + 1, tag)) {
                    char after = peek(i + 1 + tag.size());
                    if (after == '\0' || after == ' ' || after == '\t' || after == '>') {
                        return 1;
                    }
                }
            }
            if (ln.substr(i, 4) == "<!--") {
                return 2;
            }
            if (ln.substr(i, 2) == "<?") {
                return 3;
            }
            if (ln.substr(i, 9) == "<![CDATA[") {
                return 5;
            }
            if (ln.substr(i, 2) == "<!" && is_ascii_alpha(peek(i + 2))) {
                return 4;
            }
            size_t j = i + 1 + (peek(i + 1) == '/' ? 1 : 0);
            size_t k = j;
            while (k < ln.size() && (is_ascii_alnum(ln[k]) || ln[k] == '-')) {
                ++k;
            }
            if (k > j && is_ascii_alpha(ln[j]) && block_tag(ln.substr(j, k - j))) {
                char after = peek(k);
                if (after == '\0' || after == ' ' || after == '\t' || after == '>' ||
                    (after == '/' && peek(k + 1) == '>')) {
                    return 6;
                }
            }
            if (!interrupts_paragraph) {
                size_t end = peek(i + 1) == '/' ? scan_close_tag(ln, i) : scan_open_tag(ln, i);
                if (end != std::string_view::npos) {
                    std::string name;
                    for (char x : ln.substr(j, k - j)) {
                        name.push_back(ascii_lower(x));
                    }
                    bool raw = name == "script" || name == "style" || name == "pre" || name == "textarea";
                    size_t r = end;
                    while (r < ln.size() && is_space_tab(ln[r])) {
                        ++r;
                    }
                    if (r >= ln.size() && !raw) {
                        return 7;
                    }
                }
            }
            return 0;
        }

        static bool html_ends(int type, std::string_view s) {
            switch (type) {
                case 1:
                    for (std::string_view tag : {"</script>", "</pre>", "</style>", "</textarea>"}) {
                        for (size_t i = 0; i + tag.size() <= s.size(); ++i) {
                            if (starts_with_ci(s, i, tag)) {
                                return true;
                            }
                        }
                    }
                    return false;
                case 2:
                    return s.find("-->") != std::string_view::npos;
                case 3:
                    return s.find("?>") != std::string_view::npos;
                case 4:
                    return s.find('>') != std::string_view::npos;
                case 5:
                    return s.find("]]>") != std::string_view::npos;
                default:
                    return false;
            }
        }

        // a setext underline: its level, or 0
        int setext_level() const {
            char c = peek(first_nonspace);
            if (c != '=' && c != '-') {
                return 0;
            }
            size_t i = first_nonspace;
            while (peek(i) == c) {
                ++i;
            }
            while (i < ln.size() && is_space_tab(ln[i])) {
                ++i;
            }
            return i >= ln.size() ? (c == '=' ? 1 : 2) : 0;
        }

        bool thematic_break() {
            char c = peek(first_nonspace);
            if (c != '*' && c != '-' && c != '_') {
                return false;
            }
            // the line's end of one character and white space, found once a
            // line: a line of nested markers ("- - - … a") asks at each of them
            if (uniform_line != line_no) {
                uniform_line = line_no;
                size_t e = ln.size();
                char u = 0;
                while (e > 0) {
                    char x = ln[e - 1];
                    if (is_space_tab(x)) {
                        --e;
                    } else if (u == 0 || x == u) {
                        u = x;
                        --e;
                    } else {
                        break;
                    }
                }
                uniform_from = e;
                uniform_char = u;
            }
            if (first_nonspace < uniform_from || c != uniform_char) {
                return false;
            }
            int count = 0;
            for (size_t i = first_nonspace; i < ln.size(); ++i) {
                if (ln[i] == c) {
                    ++count;
                } else if (!is_space_tab(ln[i])) {
                    return false;
                }
            }
            return count >= 3;
        }

        // a list marker: its kind and the item it opens
        bool list_item(int& container, bool interrupts_paragraph) {
            size_t i = first_nonspace;
            char c = peek(i);
            bool ordered = false;
            int start = 1;
            char marker;
            if (c == '-' || c == '+' || c == '*') {
                marker = c;
                ++i;
            } else if (is_ascii_digit(c)) {
                size_t d = i;
                long v = 0;
                while (is_ascii_digit(peek(i)) && i - d < 9) {
                    v = v * 10 + (peek(i) - '0');
                    ++i;
                }
                if (is_ascii_digit(peek(i)) || (peek(i) != '.' && peek(i) != ')')) {
                    return false;
                }
                marker = peek(i);
                ++i;
                ordered = true;
                start = int(v);
            } else {
                return false;
            }
            if (i < ln.size() && !is_space_tab(ln[i])) {
                return false;
            }
            // what follows the marker
            size_t j = i;
            while (j < ln.size() && is_space_tab(ln[j])) {
                ++j;
            }
            bool empty = j >= ln.size();
            if (interrupts_paragraph && (empty || (ordered && start != 1))) {
                return false;
            }
            int marker_offset = int(indent);
            size_t width = i - first_nonspace;
            advance_to_nonspace();
            advance(width, false);
            // the spaces after the marker, in columns
            size_t col0 = column;
            size_t spaces = 0;
            {
                size_t k = offset, col = column;
                while (k < ln.size() && is_space_tab(ln[k]) && col - col0 <= 4) {
                    col += ln[k] == '\t' ? 4 - col % 4 : 1;
                    ++k;
                }
                spaces = col - col0;
            }
            int padding;
            if (empty) {
                padding = int(width) + 1;
            } else if (spaces >= 5) {
                padding = int(width) + 1;
                advance(1, true);
            } else {
                padding = int(width + spaces);
                advance(spaces, true);
            }
            // a list of the same type, or a new one
            if (!(t[container].k == kind::list && t[container].ordered == ordered && t[container].marker == marker)) {
                container = add_child(container, kind::list);
                t[container].ordered = ordered;
                t[container].marker = marker;
                t[container].start = start;
            }
            container = add_child(container, kind::item);
            st(container).marker_offset = marker_offset;
            st(container).padding = padding;
            st(container).item_blank_start = empty;
            marked = container;
            return true;
        }

        // ---- one line ----

        void line(std::string_view text) {
            ++line_no;
            ln = text;
            offset = column = 0;
            partial_tab = false;
            unmatched_closed = false;
            marked = -1;
            int container = doc;
            bool all_matched = true;
            bool fence_opened = false;
            // 1. the open blocks that go on
            int child;
            while ((child = t[container].last) >= 0 && t[child].open) {
                container = child;
                find_nonspace();
                Node& b = t[container];
                bool goes_on = true;
                switch (b.k) {
                    case kind::block_quote:
                        if (indent <= 3 && peek(first_nonspace) == '>') {
                            advance_to_nonspace();
                            advance(1, false);
                            if (offset < ln.size() && is_space_tab(ln[offset])) {
                                advance(1, true);
                            }
                            marked = container;
                        } else {
                            goes_on = false;
                        }
                        break;
                    case kind::item:
                        if (indent >= size_t(st(container).marker_offset + st(container).padding)) {
                            advance(size_t(st(container).marker_offset + st(container).padding), true);
                        } else if (blank && has_content(container)) {
                            advance_to_nonspace();
                        } else {
                            goes_on = false;
                        }
                        break;
                    case kind::code_block:
                        if (st(container).fenced) {
                            if (!blank && indent <= 3 && peek(first_nonspace) == st(container).fence_char) {
                                size_t i = first_nonspace;
                                while (peek(i) == st(container).fence_char) {
                                    ++i;
                                }
                                size_t len = i - first_nonspace;
                                while (i < ln.size() && is_space_tab(ln[i])) {
                                    ++i;
                                }
                                if (len >= size_t(st(container).fence_length) && i >= ln.size()) {
                                    // the closing fence: the block ends with this line
                                    last_matched = container;
                                    mark_lines(container);
                                    close_unmatched_from(container);
                                    finalize(container);
                                    tip = t[container].parent;
                                    return;
                                }
                            }
                            size_t skip = size_t(st(container).fence_offset);
                            while (skip > 0 && offset < ln.size() && is_space_tab(ln[offset])) {
                                size_t before = column;
                                advance(1, true);
                                skip -= std::min(skip, column - before);
                            }
                        } else if (indent >= 4) {
                            advance(4, true);
                        } else if (blank) {
                            advance_to_nonspace();
                        } else {
                            goes_on = false;
                        }
                        break;
                    case kind::html_block:
                        goes_on = !(blank && st(container).html_type >= 6);
                        break;
                    case kind::paragraph:
                    case kind::table:
                        goes_on = !blank;
                        break;
                    case kind::list:
                        break;   // its items decide
                    default:
                        goes_on = false;
                }
                if (!goes_on) {
                    container = t[container].parent;
                    all_matched = false;
                    break;
                }
            }
            last_matched = container;
            bool maybe_lazy = t[tip].k == kind::paragraph;
            // 2. new blocks
            while (t[container].k != kind::code_block && t[container].k != kind::html_block) {
                find_nonspace();
                bool indented = indent >= 4;
                char c = peek(first_nonspace);
                bool in_paragraph = t[container].k == kind::paragraph;
                if (!indented && c == '>') {
                    advance_to_nonspace();
                    advance(1, false);
                    if (offset < ln.size() && is_space_tab(ln[offset])) {
                        advance(1, true);
                    }
                    container = add_child(container, kind::block_quote);
                    marked = container;
                    continue;
                }
                if (!indented && c == '#' && atx_heading(container)) {
                    break;
                }
                if (!indented && (c == '`' || c == '~') && fence_start(container)) {
                    fence_opened = true;
                    break;
                }
                if (!indented && c == '<') {
                    int type = html_start(in_paragraph || (maybe_lazy && !all_matched));
                    if (type) {
                        container = add_child(container, kind::html_block);
                        st(container).html_type = type;
                        break;
                    }
                }
                if (!indented && in_paragraph && (c == '=' || c == '-')) {
                    int level = setext_level();
                    if (level) {
                        // the definitions first: a paragraph of only them is no heading
                        size_t used = take_definitions(t.literal(container), refs);
                        t.trim(container, used, 0);
                        if (t[container].lit_len > 0) {
                            close_unmatched();
                            t[container].k = kind::heading;
                            t[container].level = level;
                            std::string_view lit = t.literal(container);
                            size_t back = 0;
                            while (back < lit.size() && (lit[lit.size() - 1 - back] == '\n' ||
                                                         is_space_tab(lit[lit.size() - 1 - back]))) {
                                ++back;
                            }
                            t.trim(container, 0, back);
                            st(container).end_line = line_no;
                            mark_lines(container);
                            t[container].open = false;
                            tip = t[container].parent;
                            return;
                        }
                    }
                }
                if (!indented && in_paragraph && o.tables && c != '\0') {
                    std::string_view lit = t.literal(container);
                    size_t nl = lit.find('\n');
                    if (nl + 1 == lit.size()) {
                        auto aligns = delimiter_row(ln.substr(first_nonspace));
                        if (!aligns.empty()) {
                            std::vector<std::string> head = table_cells(lit.substr(0, nl));
                            if (head.size() == aligns.size()) {
                                close_unmatched();
                                Node& p = t[container];
                                p.k = kind::table;
                                st(container).aligns = aligns;
                                p.lit_len = 0;
                                add_row(container, head, true);
                                mark_lines(container);
                                return;
                            }
                        }
                    }
                }
                if (!indented && thematic_break()) {
                    int n = add_child(container, kind::thematic_break);
                    close_leaf(n);
                    container = n;
                    break;
                }
                if (!indented && (c == '-' || c == '+' || c == '*' || is_ascii_digit(c)) &&
                    list_item(container, in_paragraph)) {
                    continue;
                }
                if (indented && !maybe_lazy && !blank) {
                    advance(4, true);
                    container = add_child(container, kind::code_block);
                    break;
                }
                break;
            }
            // 3. the rest of the line
            find_nonspace();
            if (!unmatched_closed && !all_matched && !blank && t[tip].k == kind::paragraph && container == last_matched) {
                // a lazy continuation line
                advance_to_nonspace();
                add_line(tip);
                mark_lines(tip);
                return;
            }
            close_unmatched();
            Node& b = t[container];
            if (fence_opened) {
                mark_lines(container);
            } else if (b.k == kind::code_block || b.k == kind::html_block) {
                add_line(container);
                if (!blank || (b.k == kind::code_block && st(container).fenced)) {
                    mark_lines(container);
                }
                if (b.k == kind::html_block && html_ends(st(container).html_type, ln.substr(std::min(offset, ln.size())))) {
                    close_leaf(container);
                }
            } else if (blank) {
                if (marked >= 0) {
                    mark_lines(marked);
                }
            } else if (b.k == kind::heading || b.k == kind::thematic_break) {
                mark_lines(container);
            } else if (b.k == kind::paragraph) {
                advance_to_nonspace();
                add_line(container);
                mark_lines(container);
            } else if (b.k == kind::table) {
                add_row(container, table_cells(ln.substr(first_nonspace)), false);
                mark_lines(container);
            } else {
                advance_to_nonspace();
                int n = add_child(container, kind::paragraph);
                add_line(n);
                mark_lines(n);
            }
        }

        void close_unmatched_from(int keep) {
            while (tip != keep && tip >= 0) {
                int p = t[tip].parent;
                finalize(tip);
                tip = p;
            }
            unmatched_closed = true;
        }

        // this line belongs to b and its ancestors: their last line
        void mark_lines(int b) {
            for (int x = b; x >= 0; x = t[x].parent) {
                st(x).end_line = line_no;
            }
        }

        void add_row(int table, std::vector<std::string> cells, bool head) {
            size_t n = st(table).aligns.size();
            cells.resize(n);
            int row = t.make(kind::table_row);
            t.append_child(table, row);
            st(row).start_line = st(row).end_line = line_no;
            t[row].open = false;
            t[row].level = head ? 1 : 0;   // the header row
            for (size_t k = 0; k < n; ++k) {
                int cell = t.make(kind::table_cell);
                t.append_child(row, cell);
                t[cell].align = st(table).aligns[k];
                t.raw_set(cell, cells[k]);
                leaves.push_back(cell);
                t[cell].open = false;
                t[cell].level = head ? 1 : 0;
            }
        }

        void finalize(int b) {
            Node& n = t[b];
            if (!n.open) {
                return;
            }
            n.open = false;
            switch (n.k) {
                case kind::paragraph: {
                    size_t used = take_definitions(t.literal(b), refs);
                    t.trim(b, used, 0);
                    if (t.literal(b).find_first_not_of(" \t\n") == std::string_view::npos) {
                        // only definitions: gone, once the lists around know a block stood here
                        n.hidden = true;
                    }
                    break;
                }
                case kind::code_block:
                    if (!st(b).fenced) {
                        // trailing blank lines are not the code's
                        std::string_view c = t.literal(b);
                        size_t keep = c.size();
                        while (keep > 0) {
                            size_t begin = keep >= 2 ? c.rfind('\n', keep - 2) : std::string::npos;
                            begin = begin == std::string::npos ? 0 : begin + 1;
                            bool white = true;
                            for (size_t x = begin; x + 1 < keep && white; ++x) {
                                white = is_space_tab(c[x]);
                            }
                            if (!white) {
                                break;
                            }
                            keep = begin;
                        }
                        t.trim(b, 0, c.size() - keep);
                    }
                    break;
                case kind::list: {
                    bool tight = true;
                    for (int item = n.first; item >= 0 && tight; item = t[item].next) {
                        if (t[item].next >= 0 && st(t[item].next).start_line > st(item).end_line + 1) {
                            tight = false;
                        }
                        for (int c = t[item].first; c >= 0 && tight; c = t[c].next) {
                            if (t[c].next >= 0 && st(t[c].next).start_line > st(c).end_line + 1) {
                                tight = false;
                            }
                        }
                    }
                    t[b].tight = tight;
                    break;
                }
                default:
                    break;
            }
        }
    };

    // ---- inlines ----

    class Inlines {
    public:
        Inlines(Tree& tree, const References& refs, const markdown_options& o)
        : t(tree)
        , refs(refs)
        , o(o) {
            for (unsigned char c : std::string_view("\n\\`*_[]!<&")) {
                specials[c] = true;
            }
            specials[static_cast<unsigned char>('~')] = o.strikethrough;
        }

        // the inlines of a leaf's text, as its children
        void parse(int leaf, std::string_view text) {
            parent = leaf;
            s = text;
            pos = 0;
            delims.clear();
            top = -1;
            brackets.clear();
            no_closer.clear();
            for (bool& k : html_end_known) {
                k = false;
            }
            while (pos < s.size()) {
                step();
            }
            process_emphasis(-1);
            tidy(leaf);
        }

    private:
        Tree& t;
        const References& refs;
        const markdown_options& o;
        bool specials[256] = {};   // the bytes where plain text stops
        int parent = -1;
        std::string_view s;
        size_t pos = 0;

        struct Delim {
            int node;
            char ch;
            int count, orig;
            bool can_open, can_close;
            int prev, next;
        };
        std::vector<Delim> delims;
        int top = -1;

        struct Bracket {
            int node;
            int bottom;      // the delimiter on top when it was opened
            size_t after;    // the text's first byte
            bool image;
            bool active;
        };
        std::vector<Bracket> brackets;
        std::unordered_map<size_t, size_t> no_closer;   // a backtick run's length: no closing run after here
        size_t html_end[4] = {0, 0, 0, 0};             // "-->", "?>", "]]>", ">": the next one found
        bool html_end_known[4] = {false, false, false, false};

        int add(kind k) {
            int n = t.make(k);
            t[n].open = false;
            t.append_child(parent, n);
            return n;
        }

        // plain text, joined to the text before it unless that is a delimiter's
        void text(std::string_view x) {
            int last = t[parent].last;
            if (last >= 0 && t[last].k == kind::text && t[last].marker == 0) {
                t.join(last, x);
                return;
            }
            int n = add(kind::text);
            if (t.in_raw(x)) {
                t.raw_slice(n, x);
            } else {
                t.chars_set(n, x);
            }
        }

        // a delimiter's or a bracket's text: a slice of the content
        int special_text(std::string_view x) {
            int n = add(kind::text);
            t.raw_slice(n, x);
            t[n].marker = 1;
            return n;
        }

        bool special(char c) const noexcept {
            return specials[static_cast<unsigned char>(c)];
        }

        void skip_line_start() {
            while (pos < s.size() && is_space_tab(s[pos])) {
                ++pos;
            }
        }

        void step() {
            char c = s[pos];
            switch (c) {
                case '\n':
                    newline();
                    return;
                case '\\':
                    if (pos + 1 < s.size() && s[pos + 1] == '\n') {
                        add(kind::line_break);
                        pos += 2;
                        skip_line_start();
                    } else if (pos + 1 < s.size() && is_ascii_punct(s[pos + 1])) {
                        text(s.substr(pos + 1, 1));
                        pos += 2;
                    } else {
                        text("\\");
                        ++pos;
                    }
                    return;
                case '`':
                    code_span();
                    return;
                case '*':
                case '_':
                    delimiter_run(c);
                    return;
                case '~':
                    if (o.strikethrough) {
                        delimiter_run(c);
                        return;
                    }
                    break;
                case '[': {
                    int n = special_text(s.substr(pos, 1));
                    brackets.push_back(Bracket{n, top, pos + 1, false, true});
                    ++pos;
                    return;
                }
                case '!':
                    if (pos + 1 < s.size() && s[pos + 1] == '[') {
                        int n = special_text(s.substr(pos, 2));
                        brackets.push_back(Bracket{n, top, pos + 2, true, true});
                        pos += 2;
                        return;
                    }
                    text("!");
                    ++pos;
                    return;
                case ']':
                    close_bracket();
                    return;
                case '<':
                    angle();
                    return;
                case '&': {
                    std::string out;
                    size_t n = entity(s, pos, out);
                    if (n) {
                        text(out);
                        pos += n;
                    } else {
                        text("&");
                        ++pos;
                    }
                    return;
                }
                default:
                    break;
            }
            size_t e = pos + 1;
            while (e < s.size() && !special(s[e])) {
                ++e;
            }
            text(s.substr(pos, e - pos));
            pos = e;
        }

        void newline() {
            ++pos;
            bool hard = false;
            int last = t[parent].last;
            if (last >= 0 && t[last].k == kind::text && t[last].marker == 0) {
                std::string_view l = t.literal(last);
                size_t spaces = 0;
                while (spaces < l.size() && l[l.size() - 1 - spaces] == ' ') {
                    ++spaces;
                }
                hard = spaces >= 2;
                t.trim(last, 0, spaces);
            }
            add(hard ? kind::line_break : kind::soft_break);
            skip_line_start();
        }

        void code_span() {
            size_t start = pos;
            while (pos < s.size() && s[pos] == '`') {
                ++pos;
            }
            size_t n = pos - start;
            // a run of this length with no closer after an earlier one has none after this one
            auto known = no_closer.find(n);
            if (known != no_closer.end() && known->second <= pos) {
                text(s.substr(start, n));
                return;
            }
            size_t i = pos;
            while (i < s.size()) {
                size_t j = s.find('`', i);
                if (j == std::string_view::npos) {
                    break;
                }
                size_t k = j;
                while (k < s.size() && s[k] == '`') {
                    ++k;
                }
                if (k - j == n) {
                    std::string content(s.substr(pos, j - pos));
                    for (char& c : content) {
                        if (c == '\n') {
                            c = ' ';
                        }
                    }
                    if (content.size() >= 2 && content.front() == ' ' && content.back() == ' ' &&
                        content.find_first_not_of(' ') != std::string::npos) {
                        content = content.substr(1, content.size() - 2);
                    }
                    int c = add(kind::code);
                    t.chars_set(c, content);
                    pos = k;
                    return;
                }
                i = k;
            }
            no_closer[n] = pos;
            text(s.substr(start, n));
        }

        char32_t before(size_t i) const {
            if (i == 0) {
                return U'\n';
            }
            return utf8::decode_last(s, i).first;
        }

        char32_t after(size_t i) const {
            if (i >= s.size()) {
                return U'\n';
            }
            return utf8::decode(s, i).first;
        }

        void delimiter_run(char c) {
            size_t start = pos;
            while (pos < s.size() && s[pos] == c) {
                ++pos;
            }
            int n = int(pos - start);
            if (c == '~' && n > 2) {
                text(s.substr(start, size_t(n)));
                return;
            }
            char32_t b = before(start), a = after(pos);
            bool space_b = is_unicode_space(b), space_a = is_unicode_space(a);
            bool punct_b = is_unicode_punct(b), punct_a = is_unicode_punct(a);
            bool left = !space_a && (!punct_a || space_b || punct_b);
            bool right = !space_b && (!punct_b || space_a || punct_a);
            bool can_open = left, can_close = right;
            if (c == '_') {
                can_open = left && (!right || punct_b);
                can_close = right && (!left || punct_a);
            }
            int node = special_text(s.substr(start, size_t(n)));
            if (!can_open && !can_close) {
                t[node].marker = 0;
                return;
            }
            delims.push_back(Delim{node, c, n, n, can_open, can_close, top, -1});
            int d = int(delims.size() - 1);
            if (top >= 0) {
                delims[size_t(top)].next = d;
            }
            top = d;
        }

        void remove_delim(int d) {
            Delim& x = delims[size_t(d)];
            if (x.prev >= 0) {
                delims[size_t(x.prev)].next = x.next;
            }
            if (x.next >= 0) {
                delims[size_t(x.next)].prev = x.prev;
            }
            if (top == d) {
                top = x.prev;
            }
            t[x.node].marker = 0;
        }

        static int char_index(char c) noexcept {
            return c == '*' ? 0 : c == '_' ? 1 : 2;
        }

        // the emphasis of the delimiters above bottom (CommonMark's
        // "process emphasis")
        void process_emphasis(int bottom) {
            int closer = top;
            if (closer < 0 || closer == bottom) {
                return;
            }
            while (delims[size_t(closer)].prev != bottom && delims[size_t(closer)].prev >= 0) {
                closer = delims[size_t(closer)].prev;
            }
            int openers_bottom[3][2][3];
            for (auto& a : openers_bottom) {
                for (auto& b : a) {
                    for (int& c : b) {
                        c = bottom;
                    }
                }
            }
            while (closer >= 0) {
                Delim& cl = delims[size_t(closer)];
                if (!cl.can_close) {
                    closer = cl.next;
                    continue;
                }
                int ci = char_index(cl.ch);
                int& ob = openers_bottom[ci][cl.can_open ? 1 : 0][cl.orig % 3];
                int opener = cl.prev;
                bool found = false;
                while (opener >= 0 && opener != bottom && opener != ob) {
                    Delim& op = delims[size_t(opener)];
                    if (op.ch == cl.ch && op.can_open) {
                        bool skip;
                        if (cl.ch == '~') {
                            skip = op.count != cl.count;
                        } else {
                            skip = (op.can_close || cl.can_open) && (op.orig + cl.orig) % 3 == 0 &&
                                   !(op.orig % 3 == 0 && cl.orig % 3 == 0);
                        }
                        if (!skip) {
                            found = true;
                            break;
                        }
                    }
                    opener = op.prev;
                }
                if (!found) {
                    ob = cl.prev;
                    int next = cl.next;
                    if (!cl.can_open) {
                        remove_delim(closer);
                    }
                    closer = next;
                    continue;
                }
                Delim& op = delims[size_t(opener)];
                int use;
                kind k;
                if (cl.ch == '~') {
                    use = cl.count;
                    k = kind::strikethrough;
                } else {
                    use = cl.count >= 2 && op.count >= 2 ? 2 : 1;
                    k = use == 2 ? kind::strong : kind::emphasis;
                }
                int on = op.node, cn = cl.node;
                op.count -= use;
                cl.count -= use;
                t.trim(on, 0, size_t(use));
                t.trim(cn, size_t(use), 0);
                int e = t.make(k);
                t[e].open = false;
                for (int x = t[on].next; x != cn && x >= 0;) {
                    int nx = t[x].next;
                    t.unlink(x);
                    t.append_child(e, x);
                    x = nx;
                }
                t.insert_after(on, e);
                for (int d = delims[size_t(opener)].next; d != closer && d >= 0;) {
                    int nd = delims[size_t(d)].next;
                    remove_delim(d);
                    d = nd;
                }
                if (delims[size_t(opener)].count == 0) {
                    t.unlink(on);
                    remove_delim(opener);
                }
                if (delims[size_t(closer)].count == 0) {
                    int next = delims[size_t(closer)].next;
                    t.unlink(cn);
                    remove_delim(closer);
                    closer = next;
                }
            }
            while (top >= 0 && top != bottom) {
                remove_delim(top);
            }
        }

        size_t skip_space(size_t i) const {
            while (i < s.size() && is_space_tab(s[i])) {
                ++i;
            }
            return i;
        }

        void close_bracket() {
            size_t close_at = pos;
            ++pos;
            if (brackets.empty()) {
                text("]");
                return;
            }
            Bracket b = brackets.back();
            if (!b.active) {
                brackets.pop_back();
                t[b.node].marker = 0;
                text("]");
                return;
            }
            std::string url, title;
            bool matched = false;
            size_t after_link = pos;
            if (pos < s.size() && s[pos] == '(') {
                bool nl;
                size_t i = skip_space_one_newline(s, pos + 1, nl);
                if (i < s.size() && s[i] == ')') {
                    matched = true;
                    after_link = i + 1;
                } else {
                    size_t d = scan_destination(s, i, url);
                    if (d != std::string_view::npos) {
                        size_t j = skip_space_one_newline(s, d, nl);
                        if (j < s.size() && s[j] == ')') {
                            matched = true;
                            after_link = j + 1;
                        } else if (j > d && j < s.size()) {
                            size_t te = scan_title(s, j, title);
                            if (te != std::string_view::npos) {
                                size_t k = skip_space_one_newline(s, te, nl);
                                if (k < s.size() && s[k] == ')') {
                                    matched = true;
                                    after_link = k + 1;
                                }
                            }
                        }
                    }
                }
                if (!matched) {
                    url.clear();
                    title.clear();
                }
            }
            if (!matched) {
                std::string key;
                size_t label_end = pos < s.size() && s[pos] == '[' ? scan_label(s, pos) : std::string_view::npos;
                bool collapsed = pos + 1 < s.size() && s[pos] == '[' && s[pos + 1] == ']';
                if (label_end != std::string_view::npos) {
                    key = label_key(s.substr(pos + 1, label_end - pos - 2));
                    after_link = label_end;
                } else {
                    std::string_view inner = s.substr(b.after, close_at - b.after);
                    if (inner.size() <= 999) {
                        std::string bracketed = "[" + std::string(inner) + "]";
                        if (scan_label(bracketed, 0) == bracketed.size()) {
                            key = label_key(inner);
                        }
                    }
                    after_link = collapsed ? pos + 2 : pos;
                }
                if (!key.empty()) {
                    auto it = refs.find(key);
                    if (it != refs.end()) {
                        url = it->second.url;
                        title = it->second.title;
                        matched = true;
                    }
                }
            }
            if (!matched) {
                brackets.pop_back();
                t[b.node].marker = 0;
                text("]");
                return;
            }
            int link = t.make(b.image ? kind::image : kind::link);
            t[link].open = false;
            if (!url.empty() || !title.empty()) {
                Extra& x = t.ex(link);
                x.url = std::move(url);
                x.title = std::move(title);
            }
            for (int x = t[b.node].next; x >= 0;) {
                int nx = t[x].next;
                t.unlink(x);
                t.append_child(link, x);
                x = nx;
            }
            t.insert_after(b.node, link);
            process_emphasis(b.bottom);
            t.unlink(b.node);
            brackets.pop_back();
            if (!b.image) {
                for (Bracket& x : brackets) {
                    if (!x.image) {
                        x.active = false;
                    }
                }
            }
            pos = after_link;
        }

        void angle() {
            // an autolink: a scheme of 2 to 32 characters, or an e-mail address
            size_t i = pos + 1;
            if (i < s.size() && is_ascii_alpha(s[i])) {
                size_t j = i + 1;
                while (j < s.size() && (is_ascii_alnum(s[j]) || s[j] == '+' || s[j] == '.' || s[j] == '-')) {
                    ++j;
                }
                if (j < s.size() && s[j] == ':' && j - i >= 2 && j - i <= 32) {
                    size_t k = j + 1;
                    while (k < s.size() && static_cast<unsigned char>(s[k]) > 0x20 && s[k] != '<' && s[k] != '>' &&
                           s[k] != 0x7F) {
                        ++k;
                    }
                    if (k < s.size() && s[k] == '>') {
                        int link = add(kind::link);
                        t.ex(link).url.assign(s.substr(i, k - i));
                        int tx = t.make(kind::text);
                        t[tx].open = false;
                        t.chars_set(tx, s.substr(i, k - i));
                        t.append_child(link, tx);
                        pos = k + 1;
                        return;
                    }
                }
            }
            size_t e = email_autolink(i);
            if (e != std::string_view::npos) {
                int link = add(kind::link);
                t.ex(link).url = "mailto:" + std::string(s.substr(i, e - i));
                int tx = t.make(kind::text);
                t[tx].open = false;
                t.chars_set(tx, s.substr(i, e - i));
                t.append_child(link, tx);
                pos = e + 1;
                return;
            }
            size_t h = raw_html_at(pos);
            if (h != std::string_view::npos) {
                int n = add(kind::html_inline);
                t.raw_slice(n, s.substr(pos, h - pos));
                pos = h;
                return;
            }
            text("<");
            ++pos;
        }

        // the next terminator of a comment, a processing instruction, a CDATA
        // section or a declaration, found once for the whole text
        size_t find_end(int which, std::string_view what, size_t from) {
            if (!html_end_known[which] || (html_end[which] != std::string_view::npos && html_end[which] < from)) {
                html_end[which] = s.find(what, from);
                html_end_known[which] = true;
            }
            return html_end[which];
        }

        // raw HTML at s[i] ('<'), its terminators searched once
        size_t raw_html_at(size_t i) {
            if (i + 1 >= s.size()) {
                return std::string_view::npos;
            }
            char c = s[i + 1];
            if (c == '?') {
                size_t e = find_end(1, "?>", i + 2);
                return e == std::string_view::npos ? e : e + 2;
            }
            if (c == '!') {
                if (s.substr(i, 4) == "<!--") {
                    if (s.substr(i + 4, 1) == ">") {
                        return i + 5;
                    }
                    if (s.substr(i + 4, 2) == "->") {
                        return i + 6;
                    }
                    size_t e = find_end(0, "-->", i + 4);
                    return e == std::string_view::npos ? e : e + 3;
                }
                if (s.substr(i, 9) == "<![CDATA[") {
                    size_t e = find_end(2, "]]>", i + 9);
                    return e == std::string_view::npos ? e : e + 3;
                }
                if (i + 2 < s.size() && is_ascii_alpha(s[i + 2])) {
                    size_t e = find_end(3, ">", i + 2);
                    return e == std::string_view::npos ? e : e + 1;
                }
                return std::string_view::npos;
            }
            return scan_raw_html(s, i);
        }

        // an e-mail address at s[i] closed by '>': the position of '>', or npos
        size_t email_autolink(size_t i) const {
            static constexpr std::string_view local = ".!#$%&'*+/=?^_`{|}~-";
            size_t j = i;
            while (j < s.size() && (is_ascii_alnum(s[j]) || local.find(s[j]) != std::string_view::npos)) {
                ++j;
            }
            if (j == i || j >= s.size() || s[j] != '@') {
                return std::string_view::npos;
            }
            ++j;
            for (;;) {
                size_t b = j;
                while (j < s.size() && (is_ascii_alnum(s[j]) || s[j] == '-') && j - b < 63) {
                    ++j;
                }
                if (j == b || s[b] == '-' || s[j - 1] == '-') {
                    return std::string_view::npos;
                }
                if (j < s.size() && s[j] == '.') {
                    ++j;
                    continue;
                }
                return j < s.size() && s[j] == '>' ? j : std::string_view::npos;
            }
        }

        // adjacent texts joined, empty ones dropped
        void tidy(int leaf) {
            std::vector<int> todo{leaf};
            while (!todo.empty()) {
                int n = todo.back();
                todo.pop_back();
                for (int c = t[n].first; c >= 0;) {
                    int next = t[c].next;
                    if (t[c].k == kind::text) {
                        t[c].marker = 0;
                        while (next >= 0 && t[next].k == kind::text) {
                            Node& a = t[c];
                            const Node& b = t[next];
                            if (a.in_chars == b.in_chars && a.lit + a.lit_len == b.lit) {
                                a.lit_len += b.lit_len;   // side by side in one pool
                            } else if (b.lit_len > 0) {
                                uint32_t from = b.lit, size = b.lit_len;
                                bool b_chars = b.in_chars;
                                t.to_chars_end(c);
                                t.chars.append(b_chars ? t.chars : t.raw, from, size);
                                t[c].lit_len += size;
                            }
                            int nn = t[next].next;
                            t.unlink(next);
                            next = nn;
                        }
                        if (t[c].lit_len == 0) {
                            t.unlink(c);
                        }
                    } else if (t[c].first >= 0) {
                        todo.push_back(c);
                    }
                    c = next;
                }
            }
        }
    };

    // ---- GitHub's extended autolinks, over the texts outside links ----

    inline bool autolink_boundary(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\n' || c == '*' || c == '_' || c == '~' || c == '(';
    }

    // a domain at s[i]: its end, or npos (segments of letters, digits, '-'
    // and '_' between dots, at least one dot; no '_' in the last two)
    inline size_t scan_domain(std::string_view s, size_t i, bool need_dot) {
        size_t j = i;
        int dots = 0;
        bool underscore_last = false, underscore_before = false;
        while (j < s.size()) {
            size_t b = j;
            bool underscore = false;
            while (j < s.size() && (is_ascii_alnum(s[j]) || s[j] == '-' || s[j] == '_' ||
                                    static_cast<unsigned char>(s[j]) >= 0x80)) {
                underscore = underscore || s[j] == '_';
                ++j;
            }
            if (j == b) {
                break;
            }
            underscore_before = underscore_last;
            underscore_last = underscore;
            if (j + 1 < s.size() && s[j] == '.' &&
                (is_ascii_alnum(s[j + 1]) || s[j + 1] == '-' || s[j + 1] == '_' ||
                 static_cast<unsigned char>(s[j + 1]) >= 0x80)) {
                ++dots;
                ++j;
                continue;
            }
            break;
        }
        if (j == i || (need_dot && dots == 0) || underscore_last || underscore_before) {
            return std::string_view::npos;
        }
        return j;
    }

    // the end of a link's path, its trailing punctuation and unbalanced
    // parentheses left out
    inline size_t trim_autolink(std::string_view s, size_t begin, size_t end) {
        end = std::min(end, s.size());
        for (;;) {
            if (end <= begin) {
                return end;
            }
            char c = s[end - 1];
            if (c == '?' || c == '!' || c == '.' || c == ',' || c == ':' || c == '*' || c == '_' || c == '~' ||
                c == '\'' || c == '"') {
                --end;
                continue;
            }
            if (c == ')') {
                int open = 0, close = 0;
                for (size_t k = begin; k < end; ++k) {
                    open += s[k] == '(';
                    close += s[k] == ')';
                }
                if (close > open) {
                    --end;
                    continue;
                }
                return end;
            }
            if (c == ';') {
                size_t k = end - 1;
                while (k > begin && is_ascii_alnum(s[k - 1])) {
                    --k;
                }
                if (k > begin && s[k - 1] == '&' && k < end - 1) {
                    end = k - 1;
                    continue;
                }
                return end;
            }
            return end;
        }
    }

    // whether a text may hold an extended autolink: an '@', a "://" or a "www."
    inline bool may_autolink(std::string_view s) noexcept {
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            if (c == '@') {
                return true;
            }
            if (c == ':' && i + 2 < s.size() && s[i + 1] == '/' && s[i + 2] == '/') {
                return true;
            }
            if (c == '.' && i >= 3 && (s[i - 1] | 32) == 'w' && (s[i - 2] | 32) == 'w' && (s[i - 3] | 32) == 'w') {
                return true;
            }
        }
        return false;
    }

    // One text node: its extended autolinks made link nodes. Its start is a
    // boundary when the line starts there or a delimiter stands before it
    inline void autolink_text(Tree& t, int node) {
        int before = t[node].prev;
        bool start_boundary = before < 0 || t[before].k == kind::soft_break || t[before].k == kind::line_break ||
                              t[before].k == kind::emphasis || t[before].k == kind::strong ||
                              t[before].k == kind::strikethrough;
        // the pieces are slices of the node's text in the pool: a copy of the
        // view, which new nodes do not move
        uint32_t base = t[node].lit;
        bool pool = t[node].in_chars;
        std::string lit(t.literal(node));
        std::string_view s(lit);
        std::vector<int> pieces;   // new nodes, in order
        size_t done = 0;
        auto slice = [&](int n, size_t b, size_t e) {
            t[n].in_chars = pool;
            t[n].lit = base + uint32_t(b);
            t[n].lit_len = uint32_t(e - b);
        };
        auto flush_text = [&](size_t upto) {
            if (upto > done) {
                int n = t.make(kind::text);
                t[n].open = false;
                slice(n, done, upto);
                pieces.push_back(n);
            }
        };
        auto link = [&](size_t b, size_t e, std::string url) {
            flush_text(b);
            int l = t.make(kind::link);
            t[l].open = false;
            t.ex(l).url = std::move(url);
            int x = t.make(kind::text);
            t[x].open = false;
            slice(x, b, e);
            t.append_child(l, x);
            pieces.push_back(l);
            done = e;
        };
        size_t i = 0;
        while (i < s.size()) {
            bool boundary = i == 0 ? start_boundary : autolink_boundary(s[i - 1]);
            char c = s[i];
            if (boundary && (c == 'w' || c == 'W') && starts_with_ci(s, i, "www.")) {
                size_t d = scan_domain(s, i, true);
                if (d != std::string_view::npos) {
                    size_t e = d;
                    while (e < s.size() && !is_space_tab(s[e]) && s[e] != '\n' && s[e] != '<') {
                        ++e;
                    }
                    e = trim_autolink(s, i, e);
                    if (e > d || e == d) {
                        link(i, e, "http://" + std::string(s.substr(i, e - i)));
                        i = e;
                        continue;
                    }
                }
            }
            if (boundary && (c == 'h' || c == 'H' || c == 'f' || c == 'F')) {
                size_t scheme = starts_with_ci(s, i, "https://") ? 8
                                : starts_with_ci(s, i, "http://") ? 7
                                : starts_with_ci(s, i, "ftp://")  ? 6
                                                                  : 0;
                if (scheme) {
                    size_t d = scan_domain(s, i + scheme, false);
                    if (d != std::string_view::npos) {
                        size_t e = d;
                        while (e < s.size() && !is_space_tab(s[e]) && s[e] != '\n' && s[e] != '<') {
                            ++e;
                        }
                        e = trim_autolink(s, i, e);
                        link(i, e, std::string(s.substr(i, e - i)));
                        i = e;
                        continue;
                    }
                }
            }
            if (c == '@' && i > done) {
                // the local part before, the domain after
                size_t b = i;
                while (b > done && (is_ascii_alnum(s[b - 1]) || s[b - 1] == '.' || s[b - 1] == '-' ||
                                    s[b - 1] == '_' || s[b - 1] == '+')) {
                    --b;
                }
                if (b < i && (b == 0 ? start_boundary : autolink_boundary(s[b - 1]))) {
                    size_t j = i + 1;
                    int dots = 0;
                    size_t last_ok = std::string_view::npos;
                    while (j < s.size()) {
                        size_t seg = j;
                        while (j < s.size() && (is_ascii_alnum(s[j]) || s[j] == '-' || s[j] == '_')) {
                            ++j;
                        }
                        if (j == seg) {
                            break;
                        }
                        if (dots > 0) {
                            last_ok = j;
                        }
                        if (j + 1 < s.size() && s[j] == '.' && (is_ascii_alnum(s[j + 1]) || s[j + 1] == '-' ||
                                                                 s[j + 1] == '_')) {
                            ++dots;
                            ++j;
                            continue;
                        }
                        break;
                    }
                    if (last_ok != std::string_view::npos && s[last_ok - 1] != '-' && s[last_ok - 1] != '_') {
                        link(b, last_ok, "mailto:" + std::string(s.substr(b, last_ok - b)));
                        i = last_ok;
                        continue;
                    }
                }
            }
            ++i;
        }
        if (pieces.empty()) {
            return;
        }
        flush_text(s.size());
        int at = node;
        for (int p : pieces) {
            t.insert_after(at, p);
            at = p;
        }
        t.unlink(node);
    }

    // ---- the whole document ----

    inline int parse_document(Tree& t, std::string_view text, const markdown_options& o) {
        // a NUL is U+FFFD
        std::string input;
        if (text.find('\0') != std::string_view::npos) {
            input.reserve(text.size() + 16);
            for (char c : text) {
                if (c == '\0') {
                    input.append("\xEF\xBF\xBD");
                } else {
                    input.push_back(c);
                }
            }
            text = input;
        }
        References refs;
        Blocks blocks(t, refs, o);
        blocks.run(text);
        int doc = blocks.doc;
        Inlines inlines(t, refs, o);
        std::vector<int> todo;
        for (int n : blocks.leaves) {
            if (t[n].hidden) {
                t.unlink(n);
                continue;
            }
            kind k = t[n].k;
            if (k != kind::paragraph && k != kind::heading && k != kind::table_cell) {
                continue;   // a paragraph made a table
            }
            std::string_view content = t.literal(n);
            // a task list item: [ ], [x] or [X] at the start of its first paragraph
            if (o.task_lists && k == kind::paragraph && t[n].parent >= 0 && t[t[n].parent].k == kind::item &&
                t[t[n].parent].first == n && content.size() >= 3 && content[0] == '[' && content[2] == ']' &&
                (content[1] == ' ' || content[1] == 'x' || content[1] == 'X') &&
                (content.size() == 3 || is_space_tab(content[3]) || content[3] == '\n')) {
                t[t[n].parent].checked = content[1] == ' ' ? 0 : 1;
                size_t skip = 3;
                while (skip < content.size() && (is_space_tab(content[skip]) || content[skip] == '\n')) {
                    ++skip;
                }
                content.remove_prefix(skip);
            }
            while (!content.empty() && (content.back() == '\n' || is_space_tab(content.back()))) {
                content.remove_suffix(1);
            }
            inlines.parse(n, content);
            if (o.autolinks) {
                // the texts of this leaf outside links
                todo.assign(1, n);
                while (!todo.empty()) {
                    int x = todo.back();
                    todo.pop_back();
                    for (int c = t[x].last; c >= 0;) {
                        int prev = t[c].prev;
                        kind ck = t[c].k;
                        if (ck == kind::text) {
                            if (may_autolink(t.literal(c))) {
                                autolink_text(t, c);
                            }
                        } else if (ck != kind::link && ck != kind::image && t[c].first >= 0) {
                            todo.push_back(c);
                        }
                        c = prev;
                    }
                }
            }
        }
        return doc;
    }

    // ---- HTML ----

    inline void escape_html(std::string& out, std::string_view s) {
        static constexpr auto marks = [] {
            std::array<bool, 256> m{};
            m['&'] = m['<'] = m['>'] = m['"'] = true;
            return m;
        }();
        // eight bytes a step while none is one of the four (a zero byte of x ^ c
        // is the byte c)
        constexpr uint64_t ones = 0x0101010101010101ull, highs = 0x8080808080808080ull;
        auto has = [](uint64_t x, uint64_t c) {
            uint64_t v = x ^ (c * 0x0101010101010101ull);
            return (v - 0x0101010101010101ull) & ~v & 0x8080808080808080ull;
        };
        (void)ones;
        (void)highs;
        size_t done = 0, i = 0;
        while (i < s.size()) {
            if (i + 8 <= s.size()) {
                uint64_t x;
                std::memcpy(&x, s.data() + i, 8);
                if (!(has(x, '&') | has(x, '<') | has(x, '>') | has(x, '"'))) {
                    i += 8;
                    continue;
                }
            }
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (marks[c]) {
                out.append(s.data() + done, i - done);
                out.append(c == '&' ? "&amp;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : "&quot;");
                done = i + 1;
            }
            ++i;
        }
        out.append(s.data() + done, s.size() - done);
    }

    // a URL as an attribute: the characters a URL may hold kept, the rest
    // percent-encoded (an encoding already there kept), '&' an entity
    inline void escape_href(std::string& out, std::string_view s) {
        static const char* hex = "0123456789ABCDEF";
        for (size_t i = 0; i < s.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (is_ascii_alnum(char(c)) || std::strchr("-_.+!*(),%#@?=;:/$~", c) != nullptr) {
                out.push_back(char(c));
            } else if (c == '&') {
                out += "&amp;";
            } else {
                out.push_back('%');
                out.push_back(hex[c >> 4]);
                out.push_back(hex[c & 15]);
            }
        }
    }

    inline bool unsafe_url(std::string_view url) {
        auto has = [&](std::string_view p) { return starts_with_ci(url, 0, p); };
        if (has("data:")) {
            return !(has("data:image/png") || has("data:image/gif") || has("data:image/jpeg") ||
                     has("data:image/webp"));
        }
        return has("javascript:") || has("vbscript:") || has("file:");
    }

    // GitHub's tag filter: the '<' of these tags written as "&lt;"
    inline void filtered_html(std::string& out, std::string_view s) {
        static constexpr std::string_view tags[] = {"title", "textarea", "style", "xmp", "iframe",
                                                    "noembed", "noframes", "script", "plaintext"};
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '<') {
                size_t j = i + 1 + (i + 1 < s.size() && s[i + 1] == '/' ? 1 : 0);
                bool hit = false;
                for (std::string_view tag : tags) {
                    if (starts_with_ci(s, j, tag)) {
                        char after = j + tag.size() < s.size() ? s[j + tag.size()] : '\0';
                        if (after == '\0' || after == '>' || after == '/' || after == ' ' || after == '\t' ||
                            after == '\n' || after == '\r' || after == '\f') {
                            hit = true;
                            break;
                        }
                    }
                }
                if (hit) {
                    out += "&lt;";
                    continue;
                }
            }
            out.push_back(s[i]);
        }
    }

    // The HTML of a tree, as cmark writes it, over a view of the arena or of
    // the managed nodes; without recursion
    template<class V>
    class HtmlWriter {
    public:
        using N = typename V::node;

        HtmlWriter(const V& view, const markdown_options& o, std::string& out)
        : v(view)
        , o(o)
        , out(out) {
        }

        void run(N root) {
            // a walk by the links of the tree, without a stack: down to the
            // first child, on to the next sibling, up past the last
            N n = root;
            for (;;) {
                bool open = enter(n);
                if (open && v.valid(v.first(n))) {
                    n = v.first(n);
                    continue;
                }
                if (open) {
                    leave(n);
                }
                while (n != root && !v.valid(v.next(n))) {
                    n = v.parent(n);
                    leave(n);
                }
                if (n == root) {
                    return;
                }
                n = v.next(n);
            }
        }

    private:
        const V& v;
        const markdown_options& o;
        std::string& out;

        void cr() {
            if (!out.empty() && out.back() != '\n') {
                out.push_back('\n');
            }
        }

        bool tight_paragraph(N n) const {
            N item = v.parent(n);
            if (!v.valid(item) || v.k(item) != kind::item) {
                return false;
            }
            N list = v.parent(item);
            return v.valid(list) && v.tight(list);
        }

        void raw(std::string_view html) {
            if (!o.raw_html) {
                out += "<!-- raw HTML omitted -->";
            } else if (o.tag_filter) {
                filtered_html(out, html);
            } else {
                out.append(html);
            }
        }

        void href(std::string_view url) {
            if (!(o.safe_urls && unsafe_url(url))) {
                escape_href(out, url);
            }
        }

        // the plain text of an image's description, for its alt
        void plain(N n) {
            std::vector<N> stack, kids;
            v.children(n, kids);
            for (size_t i = kids.size(); i-- > 0;) {
                stack.push_back(kids[i]);
            }
            while (!stack.empty()) {
                N c = stack.back();
                stack.pop_back();
                switch (v.k(c)) {
                    case kind::text:
                    case kind::code:
                    case kind::html_inline:
                        escape_html(out, v.literal(c));
                        break;
                    case kind::soft_break:
                    case kind::line_break:
                        out.push_back(' ');
                        break;
                    default:
                        kids.clear();
                        v.children(c, kids);
                        for (size_t i = kids.size(); i-- > 0;) {
                            stack.push_back(kids[i]);
                        }
                }
            }
        }

        bool enter(N n) {
            switch (v.k(n)) {
                case kind::document:
                    return true;
                case kind::block_quote:
                    cr();
                    out += "<blockquote>\n";
                    return true;
                case kind::list:
                    cr();
                    if (!v.ordered(n)) {
                        out += "<ul>\n";
                    } else if (v.start(n) == 1) {
                        out += "<ol>\n";
                    } else {
                        out += "<ol start=\"" + std::to_string(v.start(n)) + "\">\n";
                    }
                    return true;
                case kind::item:
                    cr();
                    out += "<li>";
                    if (v.checked(n) >= 0) {
                        out += v.checked(n) ? "<input type=\"checkbox\" checked=\"\" disabled=\"\" /> "
                                            : "<input type=\"checkbox\" disabled=\"\" /> ";
                    }
                    return true;
                case kind::paragraph:
                    if (!tight_paragraph(n)) {
                        cr();
                        out += "<p>";
                    }
                    return true;
                case kind::heading:
                    cr();
                    out += "<h" + std::to_string(v.level(n)) + ">";
                    return true;
                case kind::code_block: {
                    cr();
                    out += "<pre><code";
                    std::string_view info = v.info(n);
                    size_t word = 0;
                    while (word < info.size() && !is_space_tab(info[word])) {
                        ++word;
                    }
                    if (word > 0) {
                        out += " class=\"language-";
                        escape_html(out, info.substr(0, word));
                        out += "\"";
                    }
                    out += ">";
                    escape_html(out, v.literal(n));
                    out += "</code></pre>\n";
                    return false;
                }
                case kind::html_block:
                    cr();
                    raw(v.literal(n));
                    cr();
                    return false;
                case kind::thematic_break:
                    cr();
                    out += "<hr />\n";
                    return false;
                case kind::table:
                    cr();
                    out += "<table>\n";
                    return true;
                case kind::table_row:
                    if (v.header(n)) {
                        out += "<thead>\n";
                    } else if (v.valid(v.previous(n)) && v.header(v.previous(n))) {
                        out += "<tbody>\n";
                    }
                    out += "<tr>\n";
                    return true;
                case kind::table_cell: {
                    out += v.header(n) ? "<th" : "<td";
                    markdown_align a = v.align(n);
                    if (a != markdown_align::none) {
                        out += a == markdown_align::left ? " align=\"left\""
                               : a == markdown_align::center ? " align=\"center\""
                                                             : " align=\"right\"";
                    }
                    out += ">";
                    return true;
                }
                case kind::text:
                    escape_html(out, v.literal(n));
                    return false;
                case kind::soft_break:
                    out += o.hard_breaks ? "<br />\n" : "\n";
                    return false;
                case kind::line_break:
                    out += "<br />\n";
                    return false;
                case kind::code:
                    out += "<code>";
                    escape_html(out, v.literal(n));
                    out += "</code>";
                    return false;
                case kind::html_inline:
                    raw(v.literal(n));
                    return false;
                case kind::emphasis:
                    out += "<em>";
                    return true;
                case kind::strong:
                    out += "<strong>";
                    return true;
                case kind::strikethrough:
                    out += "<del>";
                    return true;
                case kind::link:
                    out += "<a href=\"";
                    href(v.url(n));
                    out += "\"";
                    if (!v.title(n).empty()) {
                        out += " title=\"";
                        escape_html(out, v.title(n));
                        out += "\"";
                    }
                    out += ">";
                    return true;
                case kind::image:
                    out += "<img src=\"";
                    href(v.url(n));
                    out += "\" alt=\"";
                    plain(n);
                    out += "\"";
                    if (!v.title(n).empty()) {
                        out += " title=\"";
                        escape_html(out, v.title(n));
                        out += "\"";
                    }
                    out += " />";
                    return false;
            }
            return false;
        }

        void leave(N n) {
            switch (v.k(n)) {
                case kind::block_quote:
                    cr();
                    out += "</blockquote>\n";
                    break;
                case kind::list:
                    cr();
                    out += v.ordered(n) ? "</ol>\n" : "</ul>\n";
                    break;
                case kind::item:
                    out += "</li>\n";
                    break;
                case kind::paragraph:
                    if (!tight_paragraph(n)) {
                        out += "</p>\n";
                    }
                    break;
                case kind::heading:
                    out += "</h" + std::to_string(v.level(n)) + ">\n";
                    break;
                case kind::table: {
                    // a body ends with the table
                    bool body = false;
                    std::vector<N> rows;
                    v.children(n, rows);
                    for (N r : rows) {
                        body = body || !v.header(r);
                    }
                    if (body) {
                        out += "</tbody>\n";
                    }
                    out += "</table>\n";
                    break;
                }
                case kind::table_row:
                    out += "</tr>\n";
                    if (v.header(n)) {
                        out += "</thead>\n";
                    }
                    break;
                case kind::table_cell:
                    out += v.header(n) ? "</th>\n" : "</td>\n";
                    break;
                case kind::emphasis:
                    out += "</em>";
                    break;
                case kind::strong:
                    out += "</strong>";
                    break;
                case kind::strikethrough:
                    out += "</del>";
                    break;
                case kind::link:
                    out += "</a>";
                    break;
                default:
                    break;
            }
        }
    };

    // The arena seen by the writer
    struct ArenaView {
        const Tree& t;
        using node = int;

        bool valid(int n) const noexcept {
            return n >= 0;
        }
        kind k(int n) const noexcept {
            return t.nodes[size_t(n)].k;
        }
        int parent(int n) const noexcept {
            return t.nodes[size_t(n)].parent;
        }
        int previous(int n) const noexcept {
            return t.nodes[size_t(n)].prev;
        }
        void children(int n, std::vector<int>& out) const {
            for (int c = t.nodes[size_t(n)].first; c >= 0; c = t.nodes[size_t(c)].next) {
                out.push_back(c);
            }
        }
        int first(int n) const noexcept {
            return t.nodes[size_t(n)].first;
        }
        int next(int n) const noexcept {
            return t.nodes[size_t(n)].next;
        }
        std::string_view literal(int n) const noexcept {
            return t.literal(n);
        }
        std::string_view url(int n) const noexcept {
            return t.url(n);
        }
        std::string_view title(int n) const noexcept {
            return t.title(n);
        }
        std::string_view info(int n) const noexcept {
            return t.info(n);
        }
        int level(int n) const noexcept {
            return t.nodes[size_t(n)].level;
        }
        bool ordered(int n) const noexcept {
            return t.nodes[size_t(n)].ordered;
        }
        int start(int n) const noexcept {
            return t.nodes[size_t(n)].start;
        }
        bool tight(int n) const noexcept {
            return t.nodes[size_t(n)].tight;
        }
        int checked(int n) const noexcept {
            return t.nodes[size_t(n)].checked;
        }
        markdown_align align(int n) const noexcept {
            return t.nodes[size_t(n)].align;
        }
        bool header(int n) const noexcept {
            return t.nodes[size_t(n)].level == 1;
        }
    };

    inline std::string to_html(std::string_view text, const markdown_options& o) {
        Tree t;
        t.nodes.reserve(text.size() / 8 + 16);
        int doc = parse_document(t, text, o);
        std::string out;
        out.reserve(text.size() + text.size() / 4);
        ArenaView view{t};
        HtmlWriter<ArenaView>(view, o, out).run(doc);
        return out;
    }
}
