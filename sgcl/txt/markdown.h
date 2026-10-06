//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/dynamic_array.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Markdown as CommonMark 0.31.2 reads it, with the extensions of GitHub
// Flavored Markdown: tables, strikethrough, extended autolinks, task list
// items and the tag filter.
//
//     auto html = txt::markdown_to_html("# Title\n\nSome *text*.\n");
//     auto doc = txt::markdown_document::parse(text);         // a tree to walk
//
// Safe by default: raw HTML is left out (as cmark leaves it, a comment in
// its place) and links to javascript:, vbscript:, file: and data: (but
// images) lose their URL, unless the options say otherwise.
namespace sgcl::txt {
    enum class markdown_kind : uint8_t {
        document,
        block_quote,
        list,
        item,
        code_block,
        html_block,
        paragraph,
        heading,
        thematic_break,
        table,
        table_row,
        table_cell,
        text,
        soft_break,
        line_break,
        code,
        html_inline,
        emphasis,
        strong,
        strikethrough,
        link,
        image,
    };

    enum class markdown_align : uint8_t {
        none,
        left,
        center,
        right,
    };

    struct markdown_options {
        bool tables = true;
        bool strikethrough = true;
        bool autolinks = true;
        bool task_lists = true;
        bool raw_html = false;
        bool tag_filter = true;
        bool hard_breaks = false;
        bool safe_urls = true;
    };

    class markdown_node;
    class markdown_document;

    namespace detail {
        struct MarkdownNodeData;
        struct MarkdownDocData;
        struct MarkdownBuilder;
    }
}

#include "detail/markdown.h"

namespace sgcl::txt {
    // A node of a parsed document: a handle of one word to an immutable
    // node, shared by copies; the empty handle is no node
    class markdown_node {
    public:
        markdown_node() noexcept = default;

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return bool(_n);
        }

        SGCL_INLINE_HOT bool operator==(const markdown_node& other) const noexcept {
            return _n.get() == other._n.get();
        }

        markdown_kind kind() const noexcept;

        // the children
        size_t size() const noexcept;
        markdown_node operator[](size_t i) const noexcept;
        markdown_node parent() const noexcept;

        // text, code, a code block, raw HTML: its text
        string literal() const noexcept;

        // a link's or an image's destination and title
        string url() const noexcept;
        string title() const noexcept;

        // a code block's info string ("cpp"); empty for an indented block
        string info() const noexcept;

        // a heading's level, 1 to 6; 0 for other nodes
        int level() const noexcept;

        // a list's kind, first number and spacing
        bool ordered() const noexcept;
        int start() const noexcept;
        bool tight() const noexcept;

        // a task list item's box; nothing for an item that is no task
        optional<bool> checked() const noexcept;

        // a table cell's alignment
        markdown_align align() const noexcept;

        // a table row or cell of the header
        bool header() const noexcept;

        // The text of every descendant, line breaks as '\n'
        string text() const;

        // The node and its descendants as HTML
        string to_html(const markdown_options& o = {}) const;

    private:
        friend struct detail::MarkdownBuilder;
        tracked_ptr<const detail::MarkdownNodeData> _n;

        explicit markdown_node(tracked_ptr<const detail::MarkdownNodeData> n) noexcept
        : _n(std::move(n)) {
        }
    };

    // A parsed document: a handle of one word, shared by copies
    class markdown_document {
    public:
        markdown_document() noexcept = default;

        // Never fails: every text is a document
        static markdown_document parse(const string& text, const markdown_options& o = {});

        markdown_node root() const noexcept;

        // The HTML, with the options it was parsed with
        string to_html() const;

    private:
        friend struct detail::MarkdownBuilder;
        tracked_ptr<const detail::MarkdownDocData> _d;
    };

    namespace detail {
        struct MarkdownNodeData {
            markdown_kind kind = markdown_kind::document;
            markdown_align align = markdown_align::none;
            int8_t checked = -1;
            bool ordered = false;
            bool tight = true;
            bool header = false;
            int level = 0;
            int start = 1;
            uint32_t index = 0;
            string literal, url, title, info;
            dynamic_array<markdown_node> children;
            tracked_ptr<const MarkdownNodeData> parent;
        };

        struct MarkdownDocData {
            markdown_node root;
            markdown_options options;
        };

        // The managed nodes seen by the HTML writer
        struct ManagedMarkdownView {
            using node = const MarkdownNodeData*;

            bool valid(node n) const noexcept {
                return n != nullptr;
            }
            md::kind k(node n) const noexcept {
                return n->kind;
            }
            node parent(node n) const noexcept {
                return n->parent.get();
            }
            node previous(node n) const noexcept;
            node first(node n) const noexcept;
            node next(node n) const noexcept;
            void children(node n, std::vector<node>& out) const;
            std::string_view literal(node n) const noexcept {
                return n->literal.view();
            }
            std::string_view url(node n) const noexcept {
                return n->url.view();
            }
            std::string_view title(node n) const noexcept {
                return n->title.view();
            }
            std::string_view info(node n) const noexcept {
                return n->info.view();
            }
            int level(node n) const noexcept {
                return n->level;
            }
            bool ordered(node n) const noexcept {
                return n->ordered;
            }
            int start(node n) const noexcept {
                return n->start;
            }
            bool tight(node n) const noexcept {
                return n->tight;
            }
            int checked(node n) const noexcept {
                return n->checked;
            }
            markdown_align align(node n) const noexcept {
                return n->align;
            }
            bool header(node n) const noexcept {
                return n->header;
            }
        };

        // The arena turned into managed nodes, without recursion
        struct MarkdownBuilder {
            const md::Tree& t;

            static string str(const std::string& s) {
                return s.empty() ? string() : string(std::string_view(s));
            }

            tracked_ptr<MarkdownNodeData> one(int i, const tracked_ptr<const MarkdownNodeData>& parent,
                                              uint32_t index) const {
                const md::Node& src = t.nodes[size_t(i)];
                auto d = make_tracked<MarkdownNodeData>();
                d->kind = src.k;
                d->align = src.align;
                d->checked = src.checked;
                d->ordered = src.ordered;
                d->tight = src.tight;
                d->header = (src.k == markdown_kind::table_row || src.k == markdown_kind::table_cell) && src.level == 1;
                d->level = src.k == markdown_kind::heading ? src.level : 0;
                d->start = src.start;
                d->index = index;
                std::string_view lit = t.literal(i);
                d->literal = lit.empty() ? string() : string(lit);
                if (src.extra >= 0) {
                    const md::Extra& x = t.extras[size_t(src.extra)];
                    d->url = str(x.url);
                    d->title = str(x.title);
                    d->info = str(x.info);
                }
                d->parent = parent;
                size_t count = 0;
                for (int c = src.first; c >= 0; c = t.nodes[size_t(c)].next) {
                    ++count;
                }
                if (count) {
                    d->children = dynamic_array<markdown_node>(count);
                }
                return d;
            }

            tracked_ptr<MarkdownNodeData> tree(int root) const {
                tracked_ptr<MarkdownNodeData> top = one(root, tracked_ptr<const MarkdownNodeData>(), 0);
                vector<pair<int, tracked_ptr<MarkdownNodeData>>> todo;   // in managed memory, seen by the collector
                todo.push_back(pair<int, tracked_ptr<MarkdownNodeData>>(root, top));
                while (!todo.empty()) {
                    int i = todo.back().first;
                    tracked_ptr<MarkdownNodeData> d = todo.back().second;
                    todo.pop_back();
                    tracked_ptr<const MarkdownNodeData> self(d);
                    uint32_t k = 0;
                    for (int c = t.nodes[size_t(i)].first; c >= 0; c = t.nodes[size_t(c)].next, ++k) {
                        tracked_ptr<MarkdownNodeData> child = one(c, self, k);
                        d->children[k] = markdown_node(tracked_ptr<const MarkdownNodeData>(child));
                        todo.push_back(pair<int, tracked_ptr<MarkdownNodeData>>(c, child));
                    }
                }
                return top;
            }

            // a document of text where it lies (a fuzzer's buffer)
            static markdown_document parse(std::string_view text, const markdown_options& o) {
                md::Tree t;
                t.nodes.reserve(text.size() / 8 + 16);
                int root = md::parse_document(t, text, o);
                MarkdownBuilder b{t};
                auto d = make_tracked<MarkdownDocData>();
                d->root = markdown_node(tracked_ptr<const MarkdownNodeData>(b.tree(root)));
                d->options = o;
                markdown_document doc;
                doc._d = tracked_ptr<const MarkdownDocData>(std::move(d));
                return doc;
            }

            static const MarkdownNodeData* data(const markdown_node& n) noexcept {
                return n._n.get();
            }
        };

        inline ManagedMarkdownView::node ManagedMarkdownView::previous(node n) const noexcept {
            const MarkdownNodeData* p = n->parent.get();
            if (p == nullptr || n->index == 0) {
                return nullptr;
            }
            return MarkdownBuilder::data(p->children[n->index - 1]);
        }

        inline ManagedMarkdownView::node ManagedMarkdownView::first(node n) const noexcept {
            return n->children.size() ? MarkdownBuilder::data(n->children[0]) : nullptr;
        }

        inline ManagedMarkdownView::node ManagedMarkdownView::next(node n) const noexcept {
            const MarkdownNodeData* p = n->parent.get();
            if (p == nullptr || n->index + 1 >= p->children.size()) {
                return nullptr;
            }
            return MarkdownBuilder::data(p->children[n->index + 1]);
        }

        inline void ManagedMarkdownView::children(node n, std::vector<node>& out) const {
            for (size_t k = 0; k < n->children.size(); ++k) {
                out.push_back(MarkdownBuilder::data(n->children[k]));
            }
        }
    }

    SGCL_INLINE_HOT markdown_kind markdown_node::kind() const noexcept {
        return _n ? _n->kind : markdown_kind::document;
    }

    SGCL_INLINE_HOT size_t markdown_node::size() const noexcept {
        return _n ? _n->children.size() : 0;
    }

    SGCL_INLINE_HOT markdown_node markdown_node::operator[](size_t i) const noexcept {
        return _n && i < _n->children.size() ? _n->children[i] : markdown_node();
    }

    SGCL_INLINE_HOT markdown_node markdown_node::parent() const noexcept {
        return _n ? markdown_node(_n->parent) : markdown_node();
    }

    SGCL_INLINE_HOT string markdown_node::literal() const noexcept {
        return _n ? _n->literal : string();
    }

    SGCL_INLINE_HOT string markdown_node::url() const noexcept {
        return _n ? _n->url : string();
    }

    SGCL_INLINE_HOT string markdown_node::title() const noexcept {
        return _n ? _n->title : string();
    }

    SGCL_INLINE_HOT string markdown_node::info() const noexcept {
        return _n ? _n->info : string();
    }

    SGCL_INLINE_HOT int markdown_node::level() const noexcept {
        return _n ? _n->level : 0;
    }

    SGCL_INLINE_HOT bool markdown_node::ordered() const noexcept {
        return _n && _n->ordered;
    }

    SGCL_INLINE_HOT int markdown_node::start() const noexcept {
        return _n ? _n->start : 1;
    }

    SGCL_INLINE_HOT bool markdown_node::tight() const noexcept {
        return !_n || _n->tight;
    }

    SGCL_INLINE_HOT optional<bool> markdown_node::checked() const noexcept {
        if (!_n || _n->checked < 0) {
            return nullopt;
        }
        return _n->checked == 1;
    }

    SGCL_INLINE_HOT markdown_align markdown_node::align() const noexcept {
        return _n ? _n->align : markdown_align::none;
    }

    SGCL_INLINE_HOT bool markdown_node::header() const noexcept {
        return _n && _n->header;
    }

    inline string markdown_node::text() const {
        if (!_n) {
            return string();
        }
        std::string out;
        std::vector<const detail::MarkdownNodeData*> stack{_n.get()};
        while (!stack.empty()) {
            const detail::MarkdownNodeData* d = stack.back();
            stack.pop_back();
            switch (d->kind) {
                case markdown_kind::text:
                case markdown_kind::code:
                case markdown_kind::code_block:
                case markdown_kind::html_block:
                case markdown_kind::html_inline:
                    out.append(d->literal.view());
                    break;
                case markdown_kind::soft_break:
                case markdown_kind::line_break:
                    out.push_back('\n');
                    break;
                default:
                    break;
            }
            for (size_t k = d->children.size(); k-- > 0;) {
                stack.push_back(detail::MarkdownBuilder::data(d->children[k]));
            }
        }
        return string(std::string_view(out));
    }

    inline string markdown_node::to_html(const markdown_options& o) const {
        if (!_n) {
            return string();
        }
        std::string out;
        detail::ManagedMarkdownView view;
        detail::md::HtmlWriter<detail::ManagedMarkdownView>(view, o, out).run(_n.get());
        return string(std::string_view(out));
    }

    inline markdown_document markdown_document::parse(const string& text, const markdown_options& o) {
        return detail::MarkdownBuilder::parse(text.view(), o);
    }

    SGCL_INLINE_HOT markdown_node markdown_document::root() const noexcept {
        return _d ? _d->root : markdown_node();
    }

    inline string markdown_document::to_html() const {
        return _d ? _d->root.to_html(_d->options) : string();
    }

    // The HTML of a Markdown text: the document parsed and written at once
    inline string markdown_to_html(const string& text, const markdown_options& o = {}) {
        return string(std::string_view(detail::md::to_html(text.view(), o)));
    }
}
