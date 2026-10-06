//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/dynamic_array.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "detail/html_tree.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// HTML as a browser reads it: the WHATWG HTML parsing algorithm — the
// tokenizer, every insertion mode of the tree construction, the adoption
// agency, foster parenting, templates, SVG and MathML — over UTF-8 text, a
// document of immutable nodes over its result, the HTML serialization
// algorithm, and an allowlist sanitizer built on the two.
//
//     auto doc = txt::html_document::parse(page);
//     for (auto a : doc.root().elements("a")) {
//         println("{}", a.attribute("href").value_or(""));
//     }
//     string safe = txt::sanitize_html(user_comment);
//
// Parsing never fails: the algorithm recovers from every error, as a browser
// does, and reports them only when asked (html_options::collect_errors).
// gumbo-parser is the oracle of the tests; where it lags the standard (it
// does not join adjacent text nodes, does not know <search>, replaces C1
// controls) the standard is followed.
namespace sgcl::txt {
    enum class html_node_kind : uint8_t {
        document,
        doctype,
        element,
        text,
        comment,
        fragment,   // a template's contents
    };

    enum class html_namespace : uint8_t {
        html,
        svg,
        mathml,
    };

    // An attribute: its name as the tokenizer lowercased it (an SVG or MathML
    // name adjusted: viewBox, definitionURL; a foreign prefix kept: xlink:href)
    struct html_attribute {
        string name;
        string value;
    };

    struct html_parse_error {
        size_t offset;   // in the text after its line breaks were normalized
        string code;     // the standard's code: "unexpected-null-character"
    };

    struct html_options {
        bool scripting = true;          // the scripting flag: <noscript> holds text when on
        bool collect_errors = false;    // keep the parse errors
    };

    class html_node;
    class html_document;

    namespace detail {
        struct HtmlNodeData;
        struct HtmlDocData;
        struct HtmlBuilder;
    }

    // A node of a parsed document: a handle of one word to an immutable node,
    // shared by copies; the empty handle is no node
    class html_node {
    public:
        html_node() noexcept = default;

        explicit operator bool() const noexcept {
            return bool(_n);
        }

        html_node_kind kind() const noexcept;

        // An element's local name ("div", "svg", "foreignObject"); empty for
        // other nodes
        string name() const noexcept;

        html_namespace ns() const noexcept;

        slice<const html_attribute> attributes() const noexcept;

        // An attribute's value by its name
        optional<string> attribute(const string& name) const noexcept;

        // A text's or a comment's text, a DOCTYPE's name
        string data() const noexcept;

        html_node parent() const noexcept;

        // the children
        size_t size() const noexcept;
        html_node operator[](size_t i) const noexcept;
        html_node first_child() const noexcept;
        html_node next_sibling() const noexcept;
        html_node previous_sibling() const noexcept;

        // A template's contents (a fragment); no node for anything else
        html_node content() const noexcept;

        // textContent: the text of every descendant text node, in order
        string text() const;

        // The HTML fragment serialization algorithm (13.3) of the children,
        // and of the node itself
        string inner_html() const;
        string outer_html() const;

        // The descendant elements in document order, of a name or all
        vector<html_node> elements(const string& name = string()) const;

        // The first descendant element whose id is id
        html_node element_by_id(const string& id) const noexcept;

        friend bool operator==(const html_node& a, const html_node& b) noexcept {
            return a._n.get() == b._n.get();
        }

    private:
        tracked_ptr<const detail::HtmlNodeData> _n;

        explicit html_node(tracked_ptr<const detail::HtmlNodeData> n) noexcept
        : _n(std::move(n)) {
        }

        friend struct detail::HtmlBuilder;
        friend class html_document;
    };

    // A parsed document: a handle of one word, shared by copies
    class html_document {
    public:
        html_document() noexcept = default;

        // A document of any text: the standard's algorithm, which never fails
        static html_document parse(const string& text, const html_options& o = {});

        // A fragment (13.4) read as the children of an element of the
        // context's name (in the HTML namespace): what innerHTML reads; its
        // root is an html element holding the fragment
        static html_document parse_fragment(const string& text, const string& context = string("body"),
                                            const html_options& o = {});

        // The document node (for a fragment, the html element holding it)
        html_node root() const noexcept;

        html_node html() const noexcept;
        html_node head() const noexcept;
        html_node body() const noexcept;

        // The text of the first <title>, its white space collapsed
        string title() const;

        slice<const html_parse_error> errors() const noexcept;

        // The quirks mode the DOCTYPE set (or its absence)
        bool quirks() const noexcept;
        bool limited_quirks() const noexcept;

        // The serialization of the document's children (a fragment: of its children)
        string to_string() const;

    private:
        tracked_ptr<const detail::HtmlDocData> _d;

        friend struct detail::HtmlBuilder;
    };

    // What a sanitizer keeps: an allowlist. Empty lists take the defaults:
    // the elements and attributes of text people write (paragraphs, links,
    // emphasis, lists, quotes, code, headings, images, tables), URLs of http,
    // https or mailto (and relative ones), comments dropped; script, style,
    // iframe, object, embed, template, noscript, textarea, select and title
    // dropped with their content, any other element not allowed unwrapped
    // (its children kept).
    struct html_sanitizer_options {
        vector<string> elements;        // empty: the defaults
        vector<string> attributes;      // "a:href", "*:title"; empty: the defaults
        vector<string> url_schemes;     // empty: http, https, mailto
        bool relative_urls = true;      // URLs without a scheme
        bool nofollow = false;          // rel="nofollow noopener noreferrer" on every link
        bool keep_comments = false;
    };

    // HTML that can be put into a page: parsed as a fragment of a body,
    // the allowed part written by the serialization algorithm
    string sanitize_html(const string& html, const html_sanitizer_options& o = {});

    namespace detail {
        struct HtmlNodeData {
            uint8_t kind = 0;
            uint8_t ns = 0;
            uint32_t index = 0;                     // in its parent's children
            string name;
            string data;
            string public_id, system_id;
            dynamic_array<html_attribute> attrs;
            dynamic_array<html_node> children;
            tracked_ptr<const HtmlNodeData> parent;
            html_node content;
        };

        struct HtmlDocData {
            html_node root;
            dynamic_array<html_parse_error> errors;
            bool quirks = false;
            bool limited_quirks = false;
            bool fragment = false;
            bool scripting = true;
        };

        // The arena of the tree construction turned into managed nodes
        struct HtmlBuilder {
            const html::Tree& t;
            int unused = 0;

            static string str(const std::string& s) {
                return s.empty() ? string() : string(std::string_view(s));
            }

            tracked_ptr<HtmlNodeData> one(int i, const tracked_ptr<const HtmlNodeData>& parent, uint32_t index) {
                const html::Node& src = t.nodes[size_t(i)];
                auto d = make_tracked<HtmlNodeData>();
                d->kind = src.kind;
                d->ns = src.ns;
                d->index = index;
                d->name = str(src.name);
                d->data = str(src.data);
                d->public_id = str(src.public_id);
                d->system_id = str(src.system_id);
                d->parent = parent;
                if (!src.attrs.empty()) {
                    d->attrs = dynamic_array<html_attribute>(src.attrs.size());
                    for (size_t k = 0; k < src.attrs.size(); ++k) {
                        d->attrs[k] = html_attribute{str(src.attrs[k].name), str(src.attrs[k].value)};
                    }
                }
                size_t count = 0;
                for (int c = src.first; c >= 0; c = t.nodes[size_t(c)].next) {
                    ++count;
                }
                if (count) {
                    d->children = dynamic_array<html_node>(count);
                }
                return d;
            }

            // The tree from a node, without recursion: a document nests as deep
            // as its text says, deeper than a thread's stack
            tracked_ptr<HtmlNodeData> node(int root) {
                tracked_ptr<HtmlNodeData> top = one(root, tracked_ptr<const HtmlNodeData>(), 0);
                // what is still to be filled, in managed memory where the collector sees it
                vector<pair<int, tracked_ptr<HtmlNodeData>>> todo;
                todo.push_back(pair<int, tracked_ptr<HtmlNodeData>>(root, top));
                while (!todo.empty()) {
                    int i = todo.back().first;
                    tracked_ptr<HtmlNodeData> d = todo.back().second;
                    todo.pop_back();
                    const html::Node& src = t.nodes[size_t(i)];
                    tracked_ptr<const HtmlNodeData> self(d);
                    uint32_t k = 0;
                    for (int c = src.first; c >= 0; c = t.nodes[size_t(c)].next, ++k) {
                        tracked_ptr<HtmlNodeData> child = one(c, self, k);
                        d->children[k] = html_node(tracked_ptr<const HtmlNodeData>(child));
                        todo.push_back(pair<int, tracked_ptr<HtmlNodeData>>(c, child));
                    }
                    if (src.content >= 0) {
                        tracked_ptr<HtmlNodeData> content = one(src.content, self, 0);
                        d->content = html_node(tracked_ptr<const HtmlNodeData>(content));
                        todo.push_back(pair<int, tracked_ptr<HtmlNodeData>>(src.content, content));
                    }
                }
                return top;
            }

            static html_document document(const html::Tree& t, bool fragment, bool scripting) {
                HtmlBuilder b{t, 0};
                auto d = make_tracked<HtmlDocData>();
                int root = fragment ? t.fragment_root : t.document;
                d->root = html_node(tracked_ptr<const HtmlNodeData>(b.node(root)));
                if (!t.errors.empty()) {
                    d->errors = dynamic_array<html_parse_error>(t.errors.size());
                    for (size_t k = 0; k < t.errors.size(); ++k) {
                        d->errors[k] = html_parse_error{t.errors[k].offset, string(t.errors[k].code)};
                    }
                }
                d->quirks = t.quirks;
                d->limited_quirks = t.limited_quirks;
                d->fragment = fragment;
                d->scripting = scripting;
                html_document doc;
                doc._d = tracked_ptr<const HtmlDocData>(std::move(d));
                return doc;
            }

            // a document of text where it lies (a fuzzer's buffer)
            static html_document parse(std::string_view text, const html_options& o) {
                std::string input = html::preprocess(text);
                html::TreeBuilder tb(input, o.scripting, o.collect_errors);
                html::Tree tree = tb.parse();
                return document(tree, false, o.scripting);
            }

            static html_document parse_fragment(std::string_view text, std::string_view context,
                                                const html_options& o) {
                std::string input = html::preprocess(text);
                html::TreeBuilder tb(input, o.scripting, o.collect_errors);
                std::string ctx;
                for (char c : context) {
                    ctx.push_back(html::ascii_lower(c));
                }
                html::Tree tree = tb.parse_fragment(ctx.empty() ? std::string_view("body") : std::string_view(ctx));
                return document(tree, true, o.scripting);
            }

            static const HtmlNodeData* data(const html_node& n) noexcept {
                return n._n.get();
            }
        };

        //----------------------------------------------------------------
        // The serialization algorithm (13.3)
        //----------------------------------------------------------------
        inline bool html_void(std::string_view name) noexcept {
            return html::one_of(name, {"area", "base", "basefont", "bgsound", "br", "col", "embed", "frame", "hr", "img",
                                       "input", "keygen", "link", "meta", "param", "source", "track", "wbr"});
        }

        inline void html_escape(std::string& out, std::string_view s, bool attribute) {
            for (size_t i = 0; i < s.size(); ++i) {
                char c = s[i];
                if (c == '&') {
                    out.append("&amp;");
                } else if (c == '\xC2' && i + 1 < s.size() && s[i + 1] == '\xA0') {
                    out.append("&nbsp;");
                    ++i;
                } else if (attribute && c == '"') {
                    out.append("&quot;");
                } else if (c == '<') {
                    out.append("&lt;");
                } else if (c == '>') {
                    out.append("&gt;");
                } else {
                    out.push_back(c);
                }
            }
        }

        // the start of a node, and whether its children follow
        inline bool html_open(std::string& out, const HtmlNodeData& n, bool scripting) {
            switch (n.kind) {
                case html::KElement:
                    out.push_back('<');
                    out.append(n.name.view());
                    for (const html_attribute& a : n.attrs) {
                        out.push_back(' ');
                        out.append(a.name.view());
                        out.append("=\"");
                        html_escape(out, a.value.view(), true);
                        out.push_back('"');
                    }
                    out.push_back('>');
                    return !(n.ns == html::NsHtml && html_void(n.name.view()));
                case html::KText: {
                    const HtmlNodeData* p = n.parent.get();
                    bool raw = p && p->kind == html::KElement && p->ns == html::NsHtml
                        && (html::one_of(p->name.view(), {"style", "script", "xmp", "iframe", "noembed", "noframes",
                                                          "plaintext"})
                            || (scripting && p->name.view() == "noscript"));
                    if (raw) {
                        out.append(n.data.view());
                    } else {
                        html_escape(out, n.data.view(), false);
                    }
                    return false;
                }
                case html::KComment:
                    out.append("<!--");
                    out.append(n.data.view());
                    out.append("-->");
                    return false;
                case html::KDoctype:
                    out.append("<!DOCTYPE ");
                    out.append(n.data.view());
                    out.push_back('>');
                    return false;
                default:
                    return true;
            }
        }

        // The serialization algorithm (13.3) of a node's children (and of the
        // node itself), without recursion
        inline void html_serialize_walk(std::string& out, const HtmlNodeData& start, bool self, bool scripting) {
            struct Frame {
                const HtmlNodeData* node;
                size_t next;             // the next child; the template's contents first
                bool in_content;
                bool close;
            };
            std::vector<Frame> stack;
            auto push = [&](const HtmlNodeData& n, bool close) {
                stack.push_back(Frame{&n, 0, bool(n.content), close});
            };
            if (self) {
                if (!html_open(out, start, scripting)) {
                    return;
                }
                push(start, start.kind == html::KElement);
            } else {
                push(start, false);
            }
            while (!stack.empty()) {
                Frame& f = stack.back();
                const HtmlNodeData& owner = f.in_content ? *HtmlBuilder::data(f.node->content) : *f.node;
                if (f.next < owner.children.size()) {
                    const HtmlNodeData& c = *HtmlBuilder::data(owner.children[f.next++]);
                    if (html_open(out, c, scripting)) {
                        push(c, c.kind == html::KElement);
                    }
                    continue;
                }
                if (f.in_content) {
                    f.in_content = false;
                    f.next = 0;
                    continue;
                }
                if (f.close) {
                    out.append("</");
                    out.append(f.node->name.view());
                    out.push_back('>');
                }
                stack.pop_back();
            }
        }

        inline void html_serialize_children(std::string& out, const HtmlNodeData& n, bool scripting) {
            html_serialize_walk(out, n, false, scripting);
        }

        inline void html_serialize(std::string& out, const HtmlNodeData& n, bool scripting) {
            html_serialize_walk(out, n, true, scripting);
        }

        inline void html_text(std::string& out, const HtmlNodeData& n) {
            std::vector<const HtmlNodeData*> stack{&n};
            while (!stack.empty()) {
                const HtmlNodeData* d = stack.back();
                stack.pop_back();
                if (d->kind == html::KText) {
                    out.append(d->data.view());
                    continue;
                }
                if (d->kind == html::KComment || d->kind == html::KDoctype) {
                    continue;
                }
                for (size_t k = d->children.size(); k-- > 0;) {
                    stack.push_back(HtmlBuilder::data(d->children[k]));
                }
            }
        }
    }

    //--------------------------------------------------------------------
    // html_node
    //--------------------------------------------------------------------
    SGCL_INLINE_HOT html_node_kind html_node::kind() const noexcept {
        return _n ? html_node_kind(_n->kind) : html_node_kind::document;
    }

    SGCL_INLINE_HOT string html_node::name() const noexcept {
        return _n && _n->kind == detail::html::KElement ? _n->name : string();
    }

    SGCL_INLINE_HOT html_namespace html_node::ns() const noexcept {
        return _n ? html_namespace(_n->ns) : html_namespace::html;
    }

    SGCL_INLINE_HOT slice<const html_attribute> html_node::attributes() const noexcept {
        return _n ? _n->attrs.as_slice() : slice<const html_attribute>();
    }

    inline optional<string> html_node::attribute(const string& name) const noexcept {
        if (_n) {
            for (const html_attribute& a : _n->attrs) {
                if (a.name == name) {
                    return a.value;
                }
            }
        }
        return nullopt;
    }

    SGCL_INLINE_HOT string html_node::data() const noexcept {
        return _n ? _n->data : string();
    }

    SGCL_INLINE_HOT html_node html_node::parent() const noexcept {
        return _n ? html_node(_n->parent) : html_node();
    }

    SGCL_INLINE_HOT size_t html_node::size() const noexcept {
        return _n ? _n->children.size() : 0;
    }

    inline html_node html_node::operator[](size_t i) const noexcept {
        return _n && i < _n->children.size() ? _n->children[i] : html_node();
    }

    SGCL_INLINE_HOT html_node html_node::first_child() const noexcept {
        return (*this)[0];
    }

    inline html_node html_node::next_sibling() const noexcept {
        if (!_n || !_n->parent) {
            return html_node();
        }
        const auto& siblings = _n->parent->children;
        return _n->index + 1 < siblings.size() && siblings[_n->index]._n.get() == _n.get() ? siblings[_n->index + 1]
                                                                                           : html_node();
    }

    inline html_node html_node::previous_sibling() const noexcept {
        if (!_n || !_n->parent || _n->index == 0) {
            return html_node();
        }
        const auto& siblings = _n->parent->children;
        return siblings[_n->index]._n.get() == _n.get() ? siblings[_n->index - 1] : html_node();
    }

    SGCL_INLINE_HOT html_node html_node::content() const noexcept {
        return _n ? _n->content : html_node();
    }

    inline string html_node::text() const {
        if (!_n) {
            return string();
        }
        std::string out;
        detail::html_text(out, *_n);
        return string(std::string_view(out));
    }

    inline string html_node::inner_html() const {
        if (!_n) {
            return string();
        }
        std::string out;
        detail::html_serialize_children(out, *_n, true);
        return string(std::string_view(out));
    }

    inline string html_node::outer_html() const {
        if (!_n) {
            return string();
        }
        std::string out;
        detail::html_serialize(out, *_n, true);
        return string(std::string_view(out));
    }

    inline vector<html_node> html_node::elements(const string& name) const {
        vector<html_node> out;
        if (!_n) {
            return out;
        }
        // an explicit stack: a document may nest deeper than a thread's stack allows
        std::vector<const detail::HtmlNodeData*> stack;
        for (size_t k = _n->children.size(); k-- > 0;) {
            stack.push_back(_n->children[k]._n.get());
        }
        while (!stack.empty()) {
            const detail::HtmlNodeData* d = stack.back();
            stack.pop_back();
            if (d->kind == detail::html::KElement && (name.empty() || d->name == name)) {
                // the handle: the parent's child word (kept alive by the parent)
                const detail::HtmlNodeData* p = d->parent.get();
                out.push_back(p ? p->children[d->index] : html_node());
            }
            for (size_t k = d->children.size(); k-- > 0;) {
                stack.push_back(d->children[k]._n.get());
            }
        }
        return out;
    }

    inline html_node html_node::element_by_id(const string& id) const noexcept {
        if (!_n) {
            return html_node();
        }
        std::vector<const detail::HtmlNodeData*> stack;
        for (size_t k = _n->children.size(); k-- > 0;) {
            stack.push_back(_n->children[k]._n.get());
        }
        while (!stack.empty()) {
            const detail::HtmlNodeData* d = stack.back();
            stack.pop_back();
            if (d->kind == detail::html::KElement) {
                for (const html_attribute& a : d->attrs) {
                    if (a.name.view() == "id" && a.value == id) {
                        return d->parent->children[d->index];
                    }
                }
            }
            for (size_t k = d->children.size(); k-- > 0;) {
                stack.push_back(d->children[k]._n.get());
            }
        }
        return html_node();
    }

    //--------------------------------------------------------------------
    // html_document
    //--------------------------------------------------------------------
    inline html_document html_document::parse(const string& text, const html_options& o) {
        return detail::HtmlBuilder::parse(text.view(), o);
    }

    inline html_document html_document::parse_fragment(const string& text, const string& context,
                                                       const html_options& o) {
        return detail::HtmlBuilder::parse_fragment(text.view(), context.view(), o);
    }

    SGCL_INLINE_HOT html_node html_document::root() const noexcept {
        return _d ? _d->root : html_node();
    }

    inline html_node html_document::html() const noexcept {
        html_node r = root();
        if (_d && _d->fragment) {
            return r;
        }
        for (size_t k = 0; k < r.size(); ++k) {
            if (r[k].kind() == html_node_kind::element && r[k].name().view() == "html") {
                return r[k];
            }
        }
        return html_node();
    }

    inline html_node html_document::head() const noexcept {
        html_node h = html();
        for (size_t k = 0; k < h.size(); ++k) {
            if (h[k].kind() == html_node_kind::element && h[k].name().view() == "head" && h[k].ns() == html_namespace::html) {
                return h[k];
            }
        }
        return html_node();
    }

    inline html_node html_document::body() const noexcept {
        html_node h = html();
        for (size_t k = 0; k < h.size(); ++k) {
            if (h[k].kind() == html_node_kind::element && h[k].ns() == html_namespace::html
                && (h[k].name().view() == "body" || h[k].name().view() == "frameset")) {
                return h[k];
            }
        }
        return html_node();
    }

    inline string html_document::title() const {
        auto titles = root().elements(string("title"));
        for (const html_node& t : titles) {
            if (t.ns() != html_namespace::html) {
                continue;
            }
            std::string s(t.text().view()), out;
            bool space = false;
            for (char c : s) {
                if (detail::html::html_ws(c)) {
                    space = !out.empty();
                    continue;
                }
                if (space) {
                    out.push_back(' ');
                    space = false;
                }
                out.push_back(c);
            }
            return string(std::string_view(out));
        }
        return string();
    }

    SGCL_INLINE_HOT slice<const html_parse_error> html_document::errors() const noexcept {
        return _d ? _d->errors.as_slice() : slice<const html_parse_error>();
    }

    SGCL_INLINE_HOT bool html_document::quirks() const noexcept {
        return _d && _d->quirks;
    }

    SGCL_INLINE_HOT bool html_document::limited_quirks() const noexcept {
        return _d && _d->limited_quirks;
    }

    inline string html_document::to_string() const {
        if (!_d) {
            return string();
        }
        std::string out;
        detail::html_serialize_children(out, *detail::HtmlBuilder::data(_d->root), _d->scripting);
        return string(std::string_view(out));
    }

    //--------------------------------------------------------------------
    // The sanitizer
    //--------------------------------------------------------------------
    namespace detail {
        struct HtmlSanitizer {
            const html_sanitizer_options& o;
            std::vector<std::string> elements, attributes, schemes;
            std::string out;

            explicit HtmlSanitizer(const html_sanitizer_options& opts) : o(opts) {
                if (o.elements.empty()) {
                    for (const char* e : {"a", "abbr", "acronym", "address", "article", "aside", "b", "bdi", "bdo",
                                          "blockquote", "br", "caption", "cite", "code", "col", "colgroup", "dd", "del",
                                          "details", "dfn", "div", "dl", "dt", "em", "figcaption", "figure", "footer",
                                          "h1", "h2", "h3", "h4", "h5", "h6", "header", "hr", "i", "img", "ins", "kbd",
                                          "li", "mark", "ol", "p", "pre", "q", "rp", "rt", "ruby", "s", "samp",
                                          "section", "small", "span", "strike", "strong", "sub", "summary", "sup",
                                          "table", "tbody", "td", "tfoot", "th", "thead", "time", "tr", "tt", "u", "ul",
                                          "var", "wbr"}) {
                        elements.emplace_back(e);
                    }
                } else {
                    for (const string& e : o.elements) {
                        elements.emplace_back(e.view());
                    }
                }
                if (o.attributes.empty()) {
                    for (const char* a : {"*:title", "*:lang", "*:dir", "a:href", "img:src", "img:alt", "img:width",
                                          "img:height", "td:colspan", "td:rowspan", "th:colspan", "th:rowspan",
                                          "th:scope", "ol:start", "ol:type", "ol:reversed", "li:value", "q:cite",
                                          "blockquote:cite", "del:cite", "ins:cite", "del:datetime", "ins:datetime",
                                          "time:datetime", "abbr:title", "col:span", "colgroup:span", "details:open"}) {
                        attributes.emplace_back(a);
                    }
                } else {
                    for (const string& a : o.attributes) {
                        attributes.emplace_back(a.view());
                    }
                }
                if (o.url_schemes.empty()) {
                    schemes = {"http", "https", "mailto"};
                } else {
                    for (const string& s : o.url_schemes) {
                        std::string l;
                        for (char c : s.view()) {
                            l.push_back(html::ascii_lower(c));
                        }
                        schemes.push_back(l);
                    }
                }
            }

            bool allowed_element(std::string_view n) const {
                for (const std::string& e : elements) {
                    if (e == n) {
                        return true;
                    }
                }
                return false;
            }

            bool allowed_attribute(std::string_view element, std::string_view name) const {
                for (const std::string& a : attributes) {
                    size_t colon = a.find(':');
                    if (colon == std::string::npos) {
                        if (a == name) {
                            return true;
                        }
                        continue;
                    }
                    std::string_view e(a.data(), colon), n(a.data() + colon + 1, a.size() - colon - 1);
                    if ((e == "*" || e == element) && n == name) {
                        return true;
                    }
                }
                return false;
            }

            static bool url_attribute(std::string_view name) noexcept {
                return html::one_of(name, {"href", "src", "cite", "action", "formaction", "poster", "background",
                                           "longdesc", "usemap", "codebase", "data", "srcset", "xlink:href"});
            }

            // a URL of an allowed scheme, or a relative one when those are allowed
            bool allowed_url(std::string_view v) const {
                // the leading and trailing white space and controls a browser strips
                size_t b = 0, e = v.size();
                while (b < e && (unsigned char)v[b] <= 0x20) {
                    ++b;
                }
                while (e > b && (unsigned char)v[e - 1] <= 0x20) {
                    --e;
                }
                std::string u;
                for (size_t k = b; k < e; ++k) {
                    char c = v[k];
                    if (c != '\t' && c != '\n' && c != '\r') {   // the URL parser drops these
                        u.push_back(c);
                    }
                }
                size_t colon = u.find(':');
                size_t stop = u.find_first_of("/?#");
                if (colon == std::string::npos || (stop != std::string::npos && stop < colon)) {
                    return o.relative_urls;
                }
                std::string scheme;
                for (size_t k = 0; k < colon; ++k) {
                    char c = u[k];
                    if (!(html::ascii_alnum(c) || c == '+' || c == '-' || c == '.')) {
                        return false;
                    }
                    scheme.push_back(html::ascii_lower(c));
                }
                if (scheme.empty()) {
                    return false;
                }
                for (const std::string& s : schemes) {
                    if (s == scheme) {
                        return true;
                    }
                }
                return false;
            }

            static bool dropped_with_content(std::string_view n) noexcept {
                return html::one_of(n, {"script", "style", "iframe", "object", "embed", "template", "noscript",
                                        "textarea", "select", "title", "frameset", "frame", "noframes", "noembed", "xmp",
                                        "plaintext", "math", "svg", "head", "applet", "audio", "video", "canvas",
                                        "form", "button", "input", "option", "optgroup"});
            }

            // a node's start: true when its children are to be walked; close
            // set when an end tag follows them
            bool open(const HtmlNodeData& n, bool& close) {
                close = false;
                switch (n.kind) {
                    case html::KText:
                        html_escape(out, n.data.view(), false);
                        return false;
                    case html::KComment:
                        if (o.keep_comments) {
                            out.append("<!--");
                            // a comment that could close early or open another is made safe
                            std::string d(n.data.view());
                            for (char& c : d) {
                                if (c == '-' || c == '<' || c == '>') {
                                    c = ' ';
                                }
                            }
                            out.append(d);
                            out.append("-->");
                        }
                        return false;
                    case html::KElement:
                        break;
                    default:
                        return true;
                }
                std::string_view name = n.name.view();
                if (n.ns != html::NsHtml || dropped_with_content(name)) {
                    return false;
                }
                if (!allowed_element(name)) {
                    return true;   // unwrapped: its children kept
                }
                out.push_back('<');
                out.append(name);
                bool link = name == "a";
                bool has_href = false;
                for (const html_attribute& a : n.attrs) {
                    std::string_view an = a.name.view();
                    if (!allowed_attribute(name, an)) {
                        continue;
                    }
                    if (an.size() >= 2 && html::ascii_lower(an[0]) == 'o' && html::ascii_lower(an[1]) == 'n') {
                        continue;   // never an event handler, whatever the list says
                    }
                    if (an == "style") {
                        continue;
                    }
                    if (url_attribute(an) && !allowed_url(a.value.view())) {
                        continue;
                    }
                    if (link && o.nofollow && an == "rel") {
                        continue;
                    }
                    has_href = has_href || an == "href";
                    out.push_back(' ');
                    out.append(an);
                    out.append("=\"");
                    html_escape(out, a.value.view(), true);
                    out.push_back('"');
                }
                if (link && o.nofollow && has_href) {
                    out.append(" rel=\"nofollow noopener noreferrer\"");
                }
                out.push_back('>');
                if (html_void(name)) {
                    return false;
                }
                close = true;
                return true;
            }

            // the children of a node, without recursion
            void children(const HtmlNodeData& root) {
                struct Frame {
                    const HtmlNodeData* node;
                    size_t next;
                    bool close;
                };
                std::vector<Frame> stack{Frame{&root, 0, false}};
                while (!stack.empty()) {
                    Frame& f = stack.back();
                    if (f.next < f.node->children.size()) {
                        const HtmlNodeData& c = *HtmlBuilder::data(f.node->children[f.next++]);
                        bool close;
                        if (open(c, close)) {
                            stack.push_back(Frame{&c, 0, close});
                        }
                        continue;
                    }
                    if (f.close) {
                        out.append("</");
                        out.append(f.node->name.view());
                        out.push_back('>');
                    }
                    stack.pop_back();
                }
            }

            // the html of text where it lies
            static string run(std::string_view html, const html_sanitizer_options& o) {
                html_options parse_options;
                parse_options.scripting = false;   // a <noscript>'s content is markup, dropped with it
                html_document doc = HtmlBuilder::parse_fragment(html, "body", parse_options);
                HtmlSanitizer s(o);
                s.children(*HtmlBuilder::data(doc.root()));
                return string(std::string_view(s.out));
            }
        };
    }

    inline string sanitize_html(const string& html, const html_sanitizer_options& o) {
        return detail::HtmlSanitizer::run(html.view(), o);
    }
}
