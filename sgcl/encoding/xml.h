//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "detail/xml_names.h"
#include "detail/xml_scanner.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/dynamic_array.h"
#include "../core/expected.h"
#include "../core/generator.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/utf8.h"
#include "../core/vector.h"
#include "../io/error.h"
#include "../io/stream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace sgcl::encoding {
    namespace detail {
        using namespace sgcl::detail;

        // What every node of a tree starts with: which of the four it is
        struct XmlNode {
            uint8_t kind = 0;
        };

        template<class Xml> class XmlTreeBuilder;
        class XmlOut;
        class XmlMapper;
    }

    // XML 1.0 (fifth edition) with Namespaces in XML 1.0: a reader of
    // tokens that takes a document from memory or from a stream a piece
    // at a time, a writer, and a tree — this class: one node of it, a
    // value that never changes, as json is and the containers of
    // immutable are.
    //
    //     xml catalog = xml::parse(text).value();
    //     for (auto book : catalog.children("book")) {
    //         string id = book.attribute("id").value_or("");
    //         string title = book.child("title").text();
    //     }
    //
    // A node is an element, a text, a comment or a processing
    // instruction; xml() is none of them, what child() gives when there is
    // no such child, so that a chain of child() calls needs no check at
    // each step. A copy is a copy of the handle, a node is shared between
    // threads without a lock, and a change makes a new node (set, erase,
    // push_back), the old one staying as it was; for a node of many
    // children, xml::builder.
    //
    // What is deliberately not here: the DTD. A DOCTYPE declaration is
    // read for its shape and passed on as a token, never interpreted, so
    // there are no entities but the five of XML (&lt; &gt; &amp; &apos;
    // &quot;), nothing is ever loaded from outside, and an entity that
    // expands into more of them cannot exist; &nbsp; is
    // errc::undefined_entity. No default values of attributes either,
    // which only a DTD gives. A name is matched as the document writes
    // it ("svg:rect"), or by its namespace and local name written
    // "{http://www.w3.org/2000/svg}rect".
    class xml {
    public:
        using error = encoding::error;

        enum class kind : uint8_t {
            none,
            element,
            text,
            comment,
            instruction
        };

        // An attribute as the document writes it: the name with its
        // prefix, the value with its references replaced and its white
        // space normalized (XML 1.0, 3.3.3), and the namespace the prefix
        // stands for ("" for a name without one; xmlns attributes are in
        // http://www.w3.org/2000/xmlns/)
        struct attribute {
            string name;
            string value;
            string namespace_uri;

            friend bool operator==(const attribute&, const attribute&) = default;
        };

    private:
        // the struct by a name the method attribute() below does not hide
        using attribute_type = attribute;

    public:

        // What a parse or a reader accepts
        struct options {
            uint32_t max_depth = 512;                  // elements nested deeper: errc::depth_limit
            size_t max_token_size = size_t(16) << 20;  // a tag, text or comment of a stream held longer: errc::out_of_range
            bool keep_comments = false;                // in a tree and in reader::read(); next() gives them always
            bool keep_whitespace = false;              // text of white space alone between elements, the same
        };

        // How a node is written: compact, or indented by `indent` spaces a
        // level, with or without the XML declaration in front. An element
        // holding text is written as it is inside, since the white space
        // of its content is its content.
        struct style {
            uint8_t indent = 0;
            bool declaration = false;
        };

        static const style compact;
        static const style pretty;

        class token;
        class reader;
        class writer;
        class builder;

        // --- making ---

        xml() noexcept = default;

        // An empty element; a name that is not a qualified name
        // (Namespaces in XML) is invalid_argument
        explicit xml(const string& name);

        // <name>text</name>
        xml(const string& name, const string& text);

        static xml text_node(const string& text);

        // A comment; text holding "--" or ending with '-', which a comment
        // cannot, is invalid_argument
        static xml comment(const string& text);

        // <?target data?>; a target that is not a name, or is "xml", or
        // data holding "?>" is invalid_argument
        static xml instruction(const string& target, const string& data = {});

        // --- reading ---

        // The root element of a document; the prolog and what follows the
        // root are read and checked, and left out
        static expected<xml, error> parse(const string& text);
        static expected<xml, error> parse(const string& text, const options& o);

        // The same from a stream, read to its end
        static expected<xml, error> parse(const io::reader& in);
        static expected<xml, error> parse(const io::reader& in, const options& o);

        // The same in a task: `co_await xml::async_parse(in)`
        static async::task<expected<xml, error>> async_parse(const io::reader& in);
        static async::task<expected<xml, error>> async_parse(io::reader in, options o);   // o by value: a task is lazy (json the same)

        // --- a program's own types (describe(field_list&), detail/xml_fields.h) ---

        // The root element as a value of T: a field is a child element of
        // its name, one marked attribute() an attribute, one marked text()
        // the element's text; a sequence is the element repeated
        template<class T>
        static expected<T, error> parse(const string& text);
        template<class T>
        static expected<T, error> parse(const string& text, const options& o);
        template<class T>
        static expected<T, error> parse(const io::reader& in);
        template<class T>
        static expected<T, error> parse(const io::reader& in, const options& o);
        template<class T>
        static async::task<expected<T, error>> async_parse(const io::reader& in);
        template<class T>
        static async::task<expected<T, error>> async_parse(io::reader in, options o);

        // This element as a value of T
        template<class T>
        expected<T, error> as() const;

        // The element `name` made of a value, and written
        template<class T>
        static expected<xml, error> from(const string& name, const T& value);

        template<class T>
        static expected<string, error> stringify(const string& name, const T& value, const style& s = compact);

        // --- what it is ---

        kind type() const noexcept {
            return _node ? kind(_node->kind) : kind::none;
        }

        bool exists() const noexcept {
            return bool(_node);
        }

        bool is_element() const noexcept {
            return type() == kind::element;
        }

        bool is_text() const noexcept {
            return type() == kind::text;
        }

        // The name as the document writes it ("svg:rect"); the target of
        // an instruction; empty for the rest. By value — a word — since
        // xml() has no string to refer to.
        string name() const noexcept;

        // The name without its prefix ("rect")
        string local_name() const noexcept;

        // The namespace the name's prefix (or the default namespace)
        // stands for where the element was read; "" for none
        string namespace_uri() const noexcept;

        // --- inside an element ---

        // The value of the attribute of this name ("id", "xlink:href",
        // "{http://www.w3.org/1999/xlink}href"); nullopt when there is none
        optional<string> attribute(const string& name) const;

        // In the order of the document; empty for a node that is not an element
        slice<const struct attribute> attributes() const noexcept;

        // Every node inside, in order: elements, texts, and the comments
        // and instructions a tree keeps
        slice<const xml> children() const noexcept;

        // The first element of this name inside, or xml() when there is none
        xml child(const string& name) const;

        // The elements of this name inside, in order
        generator<xml> children(const string& name) const;

        // The text: of a text node, a comment and an instruction their
        // own; of an element, every text inside it and its descendants,
        // joined
        string text() const;

        // --- new versions (the node itself never changes) ---

        // This element with the attribute set, replacing one of the same
        // name or added at the end
        xml set(const string& name, const string& value) const;

        // This element without the attribute of that name
        xml erase(const string& name) const;

        // This element with the node added as its last child
        xml push_back(const xml& child) const;

        // --- writing ---

        string to_string(const style& s = compact) const;

        // The same node: kinds, names, namespaces, values, children in
        // order; attributes in any order, as XML has them
        friend bool operator==(const xml& a, const xml& b) {
            return _equal(a, b);
        }

    private:
        template<class> friend class detail::XmlTreeBuilder;
        template<class> friend class detail::XmlScanner;
        friend class detail::XmlOut;
        friend class detail::XmlMapper;
        friend class builder;

        // nodes the reader makes, of text it has checked
        static xml comment_of(const string& text);
        static xml instruction_of(const string& target, const string& data);

        static expected<xml, error> _parse_with(reader& r);

        template<class T>
        expected<T, error> _as(uint64_t offset) const;

        template<class T>
        static expected<T, error> _typed(reader& r);
        static generator<xml> _children(xml self, string wanted);

        explicit xml(const tracked_ptr<const detail::XmlNode>& node) noexcept
        : _node(node) {
        }

        static bool _equal(const xml& a, const xml& b);
        static bool _matches(std::string_view qname, const string& local, const string& uri, std::string_view wanted) noexcept;

        tracked_ptr<const detail::XmlNode> _node;

    };

    inline constexpr xml::style xml::compact {};
    inline constexpr xml::style xml::pretty {2, false};

    namespace detail {
        struct XmlTextNode : XmlNode {
            string text;
        };

        struct XmlInstructionNode : XmlNode {
            string target;
            string data;
        };

        struct XmlElementNode : XmlNode {
            string name;
            string local;
            string uri;
            dynamic_array<struct xml::attribute> attributes;
            dynamic_array<xml> children;
        };

        inline const XmlElementNode* xml_element(const tracked_ptr<const XmlNode>& n) noexcept {
            return n && n->kind == uint8_t(xml::kind::element) ? static_cast<const XmlElementNode*>(n.get()) : nullptr;
        }

        // The part of a name after its prefix
        inline std::string_view xml_local(std::string_view qname) noexcept {
            auto c = qname.find(':');
            return c == std::string_view::npos ? qname : qname.substr(c + 1);
        }
    }

    // One token of a document, as xml::reader::next() gives it: the start
    // of an element with its name and attributes, its end, a run of text
    // (a CDATA section is text too, and the text of an element may come in
    // more than one token: before and after a CDATA section or a comment),
    // a comment, a processing instruction — the XML declaration is one,
    // named "xml" — or the DOCTYPE declaration, whole and uninterpreted.
    // A value: kept as long as wanted, after the reader has moved on.
    class xml::token {
    public:
        enum class kind : uint8_t {
            start_element,
            end_element,
            text,
            comment,
            instruction,
            doctype
        };

        kind type() const noexcept {
            return _kind;
        }

        // The element's name as written ("svg:rect"), the target of an
        // instruction, the root's name a DOCTYPE declaration gives
        const string& name() const noexcept {
            return _name;
        }

        const string& local_name() const noexcept {
            return _local;
        }

        const string& namespace_uri() const noexcept {
            return _uri;
        }

        // Of a start: in the order of the tag
        slice<const struct xml::attribute> attributes() const noexcept {
            return _attributes;
        }

        // By name as written or {namespace}local; nullopt when absent
        optional<string> attribute(const string& name) const {
            for (auto& a : _attributes) {
                if (xml::_matches(a.name.view(), string(detail::xml_local(a.name.view())), a.namespace_uri, name.view())) {
                    return a.value;
                }
            }
            return nullopt;
        }

        // The text, the comment, the instruction's data (the declaration's
        // is `version="1.0" encoding="UTF-8"`), or the DOCTYPE declaration
        // after its keyword ("html PUBLIC ...")
        const string& text() const noexcept {
            return _text;
        }

        bool is_start(const string& name) const noexcept {
            return _kind == kind::start_element && xml::_matches(_name.view(), _local, _uri, name.view());
        }

        bool is_end(const string& name) const noexcept {
            return _kind == kind::end_element && xml::_matches(_name.view(), _local, _uri, name.view());
        }

    private:
        friend class detail::XmlScanner<xml>;
        template<class> friend class detail::XmlTreeBuilder;

        kind _kind = kind::text;
        string _name;
        string _local;
        string _uri;
        string _text;
        dynamic_array<struct xml::attribute> _attributes;
    };

    namespace detail {
        // A tree built out of tokens: elements on a stack as they open,
        // their children on one shared stack, the text of an element
        // gathered across its pieces (text, CDATA, text) into one node
        template<class Xml>
        class XmlTreeBuilder {
        public:
            using token = typename Xml::token;
            using kind = typename token::kind;

            explicit XmlTreeBuilder(const typename Xml::options& o)
            : _keep_comments(o.keep_comments), _keep_whitespace(o.keep_whitespace) {
            }

            bool open() const noexcept {
                return !_frames.empty();
            }

            // A token inside an element being built, or the first of one;
            // the element once its end came
            optional<Xml> feed(token&& t) {
                switch (t._kind) {
                    case kind::start_element: {
                        _flush_text();
                        Frame f;
                        f.name = std::move(t._name);
                        f.local = std::move(t._local);
                        f.uri = std::move(t._uri);
                        f.attributes = std::move(t._attributes);
                        f.first = _children.size();
                        _frames.push_back(std::move(f));
                        return nullopt;
                    }
                    case kind::end_element: {
                        _flush_text();
                        auto& f = _frames.back();
                        auto e = make_tracked<XmlElementNode>();
                        e->kind = uint8_t(Xml::kind::element);
                        e->name = std::move(f.name);
                        e->local = std::move(f.local);
                        e->uri = std::move(f.uri);
                        e->attributes = std::move(f.attributes);
                        size_t first = f.first;
                        if (_children.size() > first) {
                            e->children = dynamic_array<Xml>(_children.begin() + ptrdiff_t(first), _children.end());
                            _children.resize(first);
                        }
                        _frames.pop_back();
                        Xml node{tracked_ptr<const XmlNode>(std::move(e))};
                        if (_frames.empty()) {
                            return node;
                        }
                        _children.push_back(std::move(node));
                        return nullopt;
                    }
                    case kind::text:
                        if (_pieces == 0) {
                            _first_piece = std::move(t._text);
                        } else {
                            if (_pieces == 1) {
                                _text.assign(_first_piece.data(), _first_piece.size());
                            }
                            _text.append(t._text.data(), t._text.size());
                        }
                        ++_pieces;
                        return nullopt;
                    case kind::comment:
                        if (_keep_comments) {
                            _flush_text();
                            _children.push_back(Xml::comment_of(std::move(t._text)));
                        }
                        return nullopt;
                    case kind::instruction:
                        _flush_text();
                        _children.push_back(Xml::instruction_of(std::move(t._name), std::move(t._text)));
                        return nullopt;
                    case kind::doctype:
                        return nullopt;
                }
                return nullopt;
            }

            // The text gathered outside any element: a text node, or
            // nullopt when there is none or it is white space the options
            // leave out
            optional<Xml> take_text() {
                if (!_pieces) {
                    return nullopt;
                }
                string t = _joined();
                if (!_keep_whitespace && _blank(t)) {
                    return nullopt;
                }
                return Xml::text_node(t);
            }

            bool has_text() const noexcept {
                return _pieces != 0;
            }

        private:
            struct Frame {
                string name;
                string local;
                string uri;
                dynamic_array<typename Xml::attribute_type> attributes;
                size_t first = 0;
            };

            static bool _blank(const string& t) noexcept {
                return std::all_of(t.begin(), t.end(), [](char c) { return xml_space(c); });
            }

            string _joined() {
                string t = _pieces == 1 ? std::move(_first_piece) : string(_text);
                _pieces = 0;
                _first_piece = string();
                _text.clear();
                return t;
            }

            void _flush_text() {
                if (!_pieces) {
                    return;
                }
                string t = _joined();
                if (!_keep_whitespace && _blank(t)) {
                    return;
                }
                _children.push_back(Xml::text_node(t));
            }

            bool _keep_comments;
            bool _keep_whitespace;
            vector<Frame> _frames;
            vector<Xml> _children;
            string _first_piece;
            std::string _text;
            size_t _pieces = 0;
        };
    }

    // The tokens of a document, one at a time, from a string or from a
    // stream: next() gives the next token, peek() the one next() will
    // give, read() the next node whole — an element with everything inside
    // it — and skip() passes over it. Each reads on the thread that calls
    // it; in a task, async_next(), async_peek(), async_read() and
    // async_skip() give the worker back while the stream waits (a document
    // in memory never waits).
    //
    //     xml::reader r(io::open("feed.xml").value());
    //     while (auto t = r.peek()) {
    //         if (t->is_start("entry")) {
    //             xml entry = r.read().value();   // the whole <entry>
    //             ...
    //         } else {
    //             r.next();
    //         }
    //     }
    //     if (auto& e = r.last_error()) { ... e->message() ... }
    //
    // nullopt at the end of the document, and on an error, which
    // last_error() then keeps: the style of a loop, as with
    // buffered_reader::lines(). read() and skip() stop, giving nullopt and
    // false, at the end tag of the element they are inside (which stays
    // for next()), so that `while (auto child = r.read())` goes over
    // the children of the element whose start was the last token. They
    // leave out what a tree leaves out (options: keep_comments,
    // keep_whitespace), and the XML and DOCTYPE declarations.
    //
    // Nothing of the document is held past the token being read: a tag,
    // a text or a comment at a time, up to options::max_token_size.
    class xml::reader {
    public:
        explicit reader(const string& text)
        : reader(text, options()) {
        }

        reader(const string& text, const options& o)
        : _scanner(text, o), _options(o) {
        }

        explicit reader(const io::reader& in)
        : reader(in, options()) {
        }

        reader(const io::reader& in, const options& o)
        : _scanner(o), _in(in), _options(o) {
        }

        reader(const reader&) = delete;
        reader& operator=(const reader&) = delete;
        reader(reader&&) = default;
        reader& operator=(reader&&) = default;

        // What went wrong, with its offset, line, column and the path of
        // the elements open ("/catalog/book"); nullopt while nothing has
        const optional<error>& last_error() const noexcept {
            return _typed_error ? _typed_error : _scanner.last_error();
        }

        // The byte of the input where the next token starts
        uint64_t offset() const noexcept {
            return _peeked ? _peeked_offset : _scanner.offset();
        }

        // The elements open around the next token
        uint32_t depth() const noexcept {
            return _peeked ? _peeked_depth : _scanner.depth();
        }

    private:
        friend class xml;
        using Scanner = detail::XmlScanner<xml>;
        using Step = typename Scanner::Step;

        // A step of the scanner that needs no input: the token, or what
        // stopped it
        Step _step(token& t) {
            return _scanner.step(t);
        }

        bool _ignorable(const token& t) const noexcept {
            switch (t.type()) {
                case token::kind::comment:
                    return !_options.keep_comments;
                case token::kind::doctype:
                    return true;
                case token::kind::instruction:
                    return t.name().view() == "xml";
                default:
                    return false;
            }
        }

        // More of the stream into the scanner
        void _fill() {
            if (!_in) {
                _scanner.received(expected<size_t, io::error>(size_t(0)));
                return;
            }
            _scanner.received(_in.read(_scanner.room()));
        }

        async::task<void> _async_fill() {
            if (!_in) {
                _scanner.received(expected<size_t, io::error>(size_t(0)));
                co_return;
            }
            _scanner.received(co_await _in.async_read(_scanner.room()));
        }

        // The token in front, into _peeked; false at the end or on an error
        bool _front() {
            if (_typed_error) {
                return false;
            }
            if (_peeked) {
                return true;
            }
            uint64_t at = _scanner.offset();
            uint32_t depth = _scanner.depth();
            token t;
            for (;;) {
                auto s = _step(t);
                if (s == Step::token) {
                    break;
                }
                if (s != Step::more) {
                    return false;
                }
                _fill();
                at = _scanner.offset();
            }
            _peeked = std::move(t);
            _peeked_offset = at;
            _peeked_depth = depth;
            return true;
        }

        async::task<bool> _async_front() {
            if (_typed_error) {
                co_return false;
            }
            if (_peeked) {
                co_return true;
            }
            uint64_t at = _scanner.offset();
            uint32_t depth = _scanner.depth();
            token t;
            for (;;) {
                auto s = _step(t);
                if (s == Step::token) {
                    break;
                }
                if (s != Step::more) {
                    co_return false;
                }
                co_await _async_fill();
                at = _scanner.offset();
            }
            _peeked = std::move(t);
            _peeked_offset = at;
            _peeked_depth = depth;
            co_return true;
        }

        token _take() {
            token t = std::move(*_peeked);
            _peeked.reset();
            return t;
        }

    public:
        // The next token; nullopt at the end or on an error
        optional<token> next() {
            if (_typed_error) {
                return nullopt;
            }
            if (_peeked) {
                return _take();
            }
            // nothing peeked: the token made where it is returned, without
            // the moves through _peeked
            optional<token> t(std::in_place);
            for (;;) {
                auto s = _step(*t);
                if (s == Step::token) {
                    return t;
                }
                if (s != Step::more) {
                    return nullopt;
                }
                _fill();
            }
        }

        // The same in a task: `co_await r.async_next()`
        async::task<optional<token>> async_next() {
            if (!co_await _async_front()) {
                co_return nullopt;
            }
            co_return _take();
        }

        // The token next() gives next, left where it is
        optional<token> peek() {
            if (!_front()) {
                return nullopt;
            }
            return *_peeked;
        }

        async::task<optional<token>> async_peek() {
            if (!co_await _async_front()) {
                co_return nullopt;
            }
            co_return *_peeked;
        }

    private:
        // One turn of read(): the token in front given to the builder, or
        // a node complete. `done` is set when read() gives what it has.
        optional<xml> _read_turn(detail::XmlTreeBuilder<xml>& b, bool front, bool& done) {
            if (!front) {
                done = true;
                if (last_error()) {
                    return nullopt;
                }
                return b.open() ? nullopt : b.take_text();
            }
            auto& t = *_peeked;
            if (!b.open()) {
                _node_offset = _peeked_offset;
                if (t.type() == token::kind::end_element) {
                    done = true;
                    return b.take_text();
                }
                if (t.type() == token::kind::text) {
                    b.feed(_take());
                    return nullopt;
                }
                if (b.has_text()) {
                    if (auto n = b.take_text()) {
                        done = true;
                        return n;
                    }
                }
                if (_ignorable(t)) {
                    _take();
                    return nullopt;
                }
                if (t.type() == token::kind::comment) {
                    done = true;
                    return xml::comment_of(_take().text());
                }
                if (t.type() == token::kind::instruction) {
                    done = true;
                    auto i = _take();
                    return xml::instruction_of(i.name(), i.text());
                }
            }
            auto n = b.feed(_take());
            if (n) {
                done = true;
            }
            return n;
        }

    public:
        // The next node whole: an element with its subtree, a text (the
        // pieces of one text joined), a comment or an instruction as the
        // options keep them; nullopt at the end tag of the element around
        // it, at the end of the document, on an error
        optional<xml> read() {
            detail::XmlTreeBuilder<xml> b(_options);
            for (;;) {
                bool done = false;
                auto n = _read_turn(b, _front(), done);
                if (done) {
                    return n;
                }
            }
        }

        // The next element as a value of T (detail/xml_fields.h); nullopt
        // at the end tag of the element around it, at the end, and when
        // the element is not a T, which last_error() then says, with the
        // path inside it ("/book/price") and the offset of its start
        template<class T>
        optional<T> read();

        template<class T>
        async::task<optional<T>> async_read();

        async::task<optional<xml>> async_read() {
            detail::XmlTreeBuilder<xml> b(_options);
            for (;;) {
                bool done = false;
                bool front = co_await _async_front();
                auto n = _read_turn(b, front, done);
                if (done) {
                    co_return n;
                }
            }
        }

    private:
        // One turn of skip(): as read(), with the depth counted in place
        // of a tree built
        bool _skip_turn(bool front, uint32_t& depth, bool& text, bool& done) {
            if (!front) {
                done = true;
                return !last_error() && depth == 0 && text;
            }
            auto& t = *_peeked;
            if (depth == 0) {
                if (t.type() == token::kind::end_element) {
                    done = true;
                    return text;
                }
                if (t.type() == token::kind::text) {
                    text = text || _options.keep_whitespace || !std::all_of(t.text().begin(), t.text().end(), [](char c) { return detail::xml_space(c); });
                    _take();
                    return false;
                }
                if (text) {
                    done = true;
                    return true;
                }
                if (_ignorable(t)) {
                    _take();
                    return false;
                }
            }
            auto k = _take().type();
            if (k == token::kind::start_element) {
                ++depth;
                return false;
            }
            if (k == token::kind::end_element) {
                --depth;
            }
            if (depth == 0) {
                done = true;
                return true;
            }
            return false;
        }

    public:
        // Passes over the node read() would give: true, or false where
        // read() gives nullopt
        bool skip() {
            uint32_t depth = 0;
            bool text = false;
            for (;;) {
                bool done = false;
                bool r = _skip_turn(_front(), depth, text, done);
                if (done) {
                    return r;
                }
            }
        }

        async::task<bool> async_skip() {
            uint32_t depth = 0;
            bool text = false;
            for (;;) {
                bool done = false;
                bool front = co_await _async_front();
                bool r = _skip_turn(front, depth, text, done);
                if (done) {
                    co_return r;
                }
            }
        }

    private:
        template<class T>
        optional<T> _typed_node(optional<xml> node);

        Scanner _scanner;
        io::reader _in;
        options _options;
        optional<token> _peeked;
        uint64_t _peeked_offset = 0;
        uint32_t _peeked_depth = 0;
        uint64_t _node_offset = 0;           // where the node read() gave last began
        optional<error> _typed_error;        // a node that was not the T asked for
    };

    namespace detail {
        // The writing both xml::writer and to_string do: into a
        // std::string, the tag of an element left open for its attributes
        // until something comes after them, names checked, text escaped,
        // and the first mistake kept
        class XmlOut {
        public:
            explicit XmlOut(uint8_t indent)
            : _indent(indent) {
            }

            std::string out;
            optional<error> failure;

            void declaration() {
                if (_any) {
                    return _fail(errc::syntax, "the XML declaration after something was written");
                }
                out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
                _any = true;
            }

            void start(std::string_view name) {
                if (failure) {
                    return;
                }
                if (!xml_qname(name)) {
                    return _fail(errc::syntax, "'" + std::string(name) + "' is not a qualified name");
                }
                _close_tag();
                _break(_open.size());
                out += '<';
                out += name;
                _open.push_back(Level{std::string(name)});
                _tag_open = true;
                _tag_attributes.clear();
                _any = true;
            }

            void attribute(std::string_view name, std::string_view value) {
                if (failure) {
                    return;
                }
                if (!_tag_open) {
                    return _fail(errc::syntax, "an attribute with no start tag open to hold it");
                }
                if (!xml_qname(name)) {
                    return _fail(errc::syntax, "'" + std::string(name) + "' is not a qualified name");
                }
                // pairwise for a few, through a set for many: a tag of a
                // hundred thousand attributes read from somewhere is not
                // 10^10 comparisons when it is written back
                bool twice = false;
                if (_tag_attributes.size() < 16) {
                    for (auto& a : _tag_attributes) {
                        twice = twice || a == name;
                    }
                    _tag_attributes.emplace_back(name);
                    if (_tag_attributes.size() == 16) {
                        _tag_set.clear();
                        _tag_set.insert(_tag_attributes.begin(), _tag_attributes.end());
                    }
                } else {
                    twice = !_tag_set.emplace(name).second;
                }
                if (twice) {
                    return _fail(errc::duplicate_key, "the attribute " + std::string(name) + " twice in one tag");
                }
                out += ' ';
                out += name;
                out += "=\"";
                _escape(value, true);
                out += '"';
            }

            void text(std::string_view t) {
                if (failure || t.empty()) {
                    return;
                }
                _close_tag();
                if (!_open.empty()) {
                    _open.back().mixed = true;
                }
                _escape(t, false);
                _any = true;
            }

            void cdata(std::string_view t) {
                if (failure) {
                    return;
                }
                _close_tag();
                if (!_open.empty()) {
                    _open.back().mixed = true;
                }
                out += "<![CDATA[";
                size_t from = 0;
                for (;;) {
                    auto at = t.find("]]>", from);
                    if (at == std::string_view::npos) {
                        _clean(t.substr(from));
                        break;
                    }
                    _clean(t.substr(from, at + 2 - from));
                    out += "]]><![CDATA[";
                    from = at + 2;
                }
                out += "]]>";
                _any = true;
            }

            void comment(std::string_view t) {
                if (failure) {
                    return;
                }
                if (t.find("--") != std::string_view::npos || (!t.empty() && t.back() == '-')) {
                    return _fail(errc::syntax, "a comment cannot hold \"--\" or end with '-'");
                }
                _close_tag();
                _break(_open.size());
                out += "<!--";
                _clean(t);
                out += "-->";
                _content();
            }

            void instruction(std::string_view target, std::string_view data) {
                if (failure) {
                    return;
                }
                if (!xml_ncname(target) || (target.size() == 3 && (target[0] | 0x20) == 'x' && (target[1] | 0x20) == 'm' && (target[2] | 0x20) == 'l')) {
                    return _fail(errc::syntax, "'" + std::string(target) + "' cannot be the target of an instruction");
                }
                if (data.find("?>") != std::string_view::npos) {
                    return _fail(errc::syntax, "an instruction's data cannot hold \"?>\"");
                }
                _close_tag();
                _break(_open.size());
                out += "<?";
                out += target;
                if (!data.empty()) {
                    out += ' ';
                    _clean(data);
                }
                out += "?>";
                _content();
            }

            void end() {
                if (failure) {
                    return;
                }
                if (_open.empty()) {
                    return _fail(errc::mismatched_tag, "an end with no element open");
                }
                if (_tag_open) {
                    out += "/>";
                    _tag_open = false;
                } else {
                    auto& top = _open.back();
                    if (top.content && !top.mixed) {
                        _break(_open.size() - 1);
                    }
                    out += "</";
                    out += top.name;
                    out += '>';
                }
                _open.pop_back();
                _content();
            }

            void node(const xml& n);

            size_t depth() const noexcept {
                return _open.size();
            }

        private:
            struct Level {
                std::string name;
                bool content = false;   // an element or a comment inside
                bool mixed = false;     // text inside: written as it is, no lines added
            };

            void _fail(errc code, const std::string& what) {
                if (!failure) {
                    failure = error(code, out.size(), string(what));
                }
            }

            void _close_tag() {
                if (_tag_open) {
                    out += '>';
                    _tag_open = false;
                }
            }

            void _content() {
                if (!_open.empty()) {
                    _open.back().content = true;
                }
                _any = true;
            }

            // A new line and the indentation of `level`, where one goes: in
            // an element without text of its own, or between the nodes
            // outside every element
            void _break(size_t level) {
                if (!_indent) {
                    return;
                }
                if (_open.empty() || level == 0) {
                    if (_any) {
                        out += '\n';
                    }
                    return;
                }
                if (_open.back().mixed) {
                    return;
                }
                if (level == _open.size()) {
                    _open.back().content = true;
                }
                out += '\n';
                out.append(size_t(_indent) * level, ' ');
            }

            // Text as character data or as an attribute's value: the five
            // characters XML writes as references where they would be read
            // as markup, the white space an attribute's value would lose to
            // normalization as character references, and a character XML
            // cannot hold at all (a control, an invalid byte of UTF-8) as
            // U+FFFD, as Go writes it
            void _escape(std::string_view t, bool attribute) {
                const char* p = t.data();
                const char* e = p + t.size();
                uint8_t plain = attribute ? XmlValue : XmlText;
                while (p < e) {
                    const char* run = p;
                    while (run < e && (XmlBytes[uint8_t(*run)] & plain) && *run != '>') {
                        ++run;
                    }
                    out.append(p, size_t(run - p));
                    p = run;
                    if (p >= e) {
                        break;
                    }
                    char c = *p;
                    switch (c) {
                        case '<': out += "&lt;"; ++p; continue;
                        case '>': out += "&gt;"; ++p; continue;
                        case '&': out += "&amp;"; ++p; continue;
                        case '"': out += attribute ? "&quot;" : "\""; ++p; continue;
                        case '\'': out += '\''; ++p; continue;
                        case ']': out += ']'; ++p; continue;
                        case '\r': out += "&#xD;"; ++p; continue;
                        case '\n': out += attribute ? "&#xA;" : "\n"; ++p; continue;
                        case '\t': out += attribute ? "&#x9;" : "\t"; ++p; continue;
                        default: break;
                    }
                    p += _character(p, e);
                }
            }

            // Text written as it is (a comment, an instruction, CDATA), with
            // what XML cannot hold as U+FFFD
            void _clean(std::string_view t) {
                const char* p = t.data();
                const char* e = p + t.size();
                while (p < e) {
                    const char* run = p;
                    while (run < e && uint8_t(*run) < 0x80 && (uint8_t(*run) >= 0x20 || *run == '\t' || *run == '\n' || *run == '\r')) {
                        ++run;
                    }
                    out.append(p, size_t(run - p));
                    p = run;
                    if (p < e) {
                        p += _character(p, e);
                    }
                }
            }

            // One character that is a control or not ASCII: as it is when
            // XML holds it, U+FFFD otherwise; the bytes it took
            size_t _character(const char* p, const char* e) {
                if (uint8_t(*p) < 0x80) {
                    out += "\xEF\xBF\xBD";
                    return 1;
                }
                auto r = xml_rune(p, e);
                if (!r.width || !xml_char(r.c)) {
                    out += "\xEF\xBF\xBD";
                    return r.width ? r.width : 1;
                }
                out.append(p, r.width);
                return r.width;
            }

            uint8_t _indent;
            bool _tag_open = false;
            bool _any = false;
            std::vector<Level> _open;
            std::vector<std::string> _tag_attributes;
            std::unordered_set<std::string> _tag_set;
        };

        // A node and everything inside it, walked with a stack of its own
        // rather than recursion: a tree a program built may be deeper than
        // a thread's stack would take
        inline void XmlOut::node(const xml& root) {
            struct Item {
                const XmlElementNode* element;
                size_t next;
            };
            std::vector<Item> stack;
            auto visit = [&](const xml& n) {
                switch (n.type()) {
                    case xml::kind::none:
                        return;
                    case xml::kind::text:
                        text(static_cast<const XmlTextNode*>(n._node.get())->text.view());
                        return;
                    case xml::kind::comment:
                        comment(static_cast<const XmlTextNode*>(n._node.get())->text.view());
                        return;
                    case xml::kind::instruction: {
                        auto i = static_cast<const XmlInstructionNode*>(n._node.get());
                        instruction(i->target.view(), i->data.view());
                        return;
                    }
                    case xml::kind::element: {
                        auto e = static_cast<const XmlElementNode*>(n._node.get());
                        start(e->name.view());
                        for (auto& a : e->attributes) {
                            attribute(a.name.view(), a.value.view());
                        }
                        stack.push_back(Item{e, 0});
                        return;
                    }
                }
            };
            visit(root);
            while (!stack.empty() && !failure) {
                auto& top = stack.back();
                if (top.next == top.element->children.size()) {
                    stack.pop_back();
                    end();
                    continue;
                }
                const xml& child = top.element->children[top.next++];
                visit(child);
            }
        }
    }

    // A writer of XML onto a stream: calls that make the document in
    // order, each returning the writer so that they chain, gathered in
    // memory until flush() writes them out. A mistake — a name that is
    // not a qualified name, an attribute after the content began, end()
    // with no element open, a comment holding "--" — is kept and given by
    // flush() (and last_error()), not thrown; nothing after it is written.
    //
    //     xml::writer w(io::stdout(), xml::pretty);
    //     w.start("catalog").start("book").attribute("id", "7").text("Dune").end().end();
    //     w.flush().value();
    //
    // Text and values are escaped as they need; a character XML cannot
    // hold is written as U+FFFD, as Go writes it. An element without
    // content is written <empty/>. The writer makes no namespace
    // declarations of its own: an xmlns attribute is an attribute.
    class xml::writer {
    public:
        explicit writer(const io::writer& out, const style& s = compact)
        : _out(out), _core(s.indent) {
            if (s.declaration) {
                _core.declaration();
            }
        }

        // <?xml version="1.0" encoding="UTF-8"?>: only first
        writer& declaration() {
            _core.declaration();
            return *this;
        }

        writer& start(const string& name) {
            _core.start(name.view());
            return *this;
        }

        writer& attribute(const string& name, const string& value) {
            _core.attribute(name.view(), value.view());
            return *this;
        }

        writer& text(const string& t) {
            _core.text(t.view());
            return *this;
        }

        // A CDATA section; one holding "]]>" is written as two
        writer& cdata(const string& t) {
            _core.cdata(t.view());
            return *this;
        }

        writer& comment(const string& t) {
            _core.comment(t.view());
            return *this;
        }

        writer& instruction(const string& target, const string& data = {}) {
            _core.instruction(target.view(), data.view());
            return *this;
        }

        // The end of the element started last
        writer& end() {
            _core.end();
            return *this;
        }

        // A value of a program's type as the element `name`
        // (detail/xml_fields.h); a value with no form in XML is a mistake
        // kept as the others are
        template<class T>
        writer& value(const string& name, const T& v);

        // A node of a tree, whole
        writer& node(const xml& n) {
            _core.node(n);
            return *this;
        }

        const optional<error>& last_error() const noexcept {
            return _core.failure;
        }

        // The elements open
        size_t depth() const noexcept {
            return _core.depth();
        }

    private:
        optional<expected<void, io::error>> _before_flush() {
            if (_core.failure) {
                auto& e = *_core.failure;
                return expected<void, io::error>(io::detail::fail(e.io_error() ? *e.io_error() : io::error(make_error_code(e.code()), "encode", "xml")));
            }
            if (_failed) {
                return expected<void, io::error>(io::detail::fail(*_failed));
            }
            if (_core.out.empty()) {
                return expected<void, io::error>();
            }
            return nullopt;
        }

        slice<const byte> _pending() const noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(_core.out.data()), _core.out.size());
        }

    public:
        // Writes what was gathered: the error of the stream, or the first
        // mistake made (errc of the encoding category inside the io::error;
        // last_error() has it whole)
        expected<void, io::error> flush() {
            if (auto r = _before_flush()) {
                return std::move(*r);
            }
            auto w = _out.write(_pending());
            _core.out.clear();
            if (!w) {
                _failed = w.error();
                return io::detail::fail(w);
            }
            return {};
        }

        // The same in a task: `co_await w.async_flush()`
        async::task<expected<void, io::error>> async_flush() {
            if (auto r = _before_flush()) {
                co_return std::move(*r);
            }
            auto w = co_await _out.async_write(_pending());
            _core.out.clear();
            if (!w) {
                _failed = w.error();
                co_return io::detail::fail(w);
            }
            co_return expected<void, io::error>();
        }

    private:
        io::writer _out;
        detail::XmlOut _core;
        optional<io::error> _failed;
    };

    // An element made a child and an attribute at a time, without the copy
    // of the whole node each push_back of a value makes: for an element
    // of many children, built in a loop
    //
    //     xml::builder list("list");
    //     for (auto i : range(1000)) {
    //         list.push_back(xml("item", txt::format("{}", i)));
    //     }
    //     xml done = list.build();
    class xml::builder {
    public:
        explicit builder(const string& name);

        builder& set(const string& name, const string& value);
        builder& push_back(const xml& child);

        // The element; the builder is left empty, with the same name
        xml build();

    private:
        string _name;
        vector<struct xml::attribute> _attributes;
        vector<xml> _children;
    };
}

// --- definitions -------------------------------------------------------------

namespace sgcl::encoding {
    namespace detail {
        inline void xml_check_name(const string& name) {
            if (!xml_qname(name.view())) {
                throw invalid_argument("sgcl::encoding::xml: '" + std::string(name.view()) + "' is not a qualified name");
            }
        }

        // The namespace a new attribute's prefix stands for, as far as the
        // element itself tells: its own declarations and its own prefix
        inline string xml_attribute_namespace(std::string_view name, const XmlElementNode* e) {
            if (name == "xmlns" || name.starts_with("xmlns:")) {
                return string(XmlnsNamespace);
            }
            auto c = name.find(':');
            if (c == std::string_view::npos) {
                return string();
            }
            auto prefix = name.substr(0, c);
            if (prefix == "xml") {
                return string(XmlNamespace);
            }
            if (e) {
                for (auto& a : e->attributes) {
                    if (a.name.view().starts_with("xmlns:") && a.name.view().substr(6) == prefix) {
                        return a.value;
                    }
                }
                auto ec = e->name.view().find(':');
                if (ec != std::string_view::npos && e->name.view().substr(0, ec) == prefix) {
                    return e->uri;
                }
            }
            return string();
        }

        inline tracked_ptr<XmlElementNode> xml_element_copy(const XmlElementNode& from) {
            auto e = make_tracked<XmlElementNode>();
            e->kind = uint8_t(xml::kind::element);
            e->name = from.name;
            e->local = from.local;
            e->uri = from.uri;
            return e;
        }
    }

    inline xml::xml(const string& name) {
        detail::xml_check_name(name);
        auto e = make_tracked<detail::XmlElementNode>();
        e->kind = uint8_t(kind::element);
        e->name = name;
        e->local = string(detail::xml_local(name.view()));
        if (name.view().starts_with("xml:")) {
            e->uri = string(detail::XmlNamespace);
        }
        _node = std::move(e);
    }

    inline xml::xml(const string& name, const string& text)
    : xml(name) {
        if (!text.empty()) {
            auto e = static_pointer_cast<detail::XmlElementNode>(const_pointer_cast<detail::XmlNode>(_node));
            e->children = dynamic_array<xml>{text_node(text)};
        }
    }

    inline xml xml::comment_of(const string& text) {
        auto n = make_tracked<detail::XmlTextNode>();
        n->kind = uint8_t(kind::comment);
        n->text = text;
        return xml(tracked_ptr<const detail::XmlNode>(std::move(n)));
    }

    inline xml xml::instruction_of(const string& target, const string& data) {
        auto n = make_tracked<detail::XmlInstructionNode>();
        n->kind = uint8_t(kind::instruction);
        n->target = target;
        n->data = data;
        return xml(tracked_ptr<const detail::XmlNode>(std::move(n)));
    }

    inline xml xml::text_node(const string& text) {
        auto n = make_tracked<detail::XmlTextNode>();
        n->kind = uint8_t(kind::text);
        n->text = text;
        return xml(tracked_ptr<const detail::XmlNode>(std::move(n)));
    }

    inline xml xml::comment(const string& text) {
        auto v = text.view();
        if (v.find("--") != std::string_view::npos || (!v.empty() && v.back() == '-')) {
            throw invalid_argument("sgcl::encoding::xml: a comment cannot hold \"--\" or end with '-'");
        }
        auto n = make_tracked<detail::XmlTextNode>();
        n->kind = uint8_t(kind::comment);
        n->text = text;
        return xml(tracked_ptr<const detail::XmlNode>(std::move(n)));
    }

    inline xml xml::instruction(const string& target, const string& data) {
        auto t = target.view();
        if (!detail::xml_ncname(t) || (t.size() == 3 && (t[0] | 0x20) == 'x' && (t[1] | 0x20) == 'm' && (t[2] | 0x20) == 'l')) {
            throw invalid_argument("sgcl::encoding::xml: '" + std::string(t) + "' cannot be the target of an instruction");
        }
        if (data.view().find("?>") != std::string_view::npos) {
            throw invalid_argument("sgcl::encoding::xml: an instruction's data cannot hold \"?>\"");
        }
        auto n = make_tracked<detail::XmlInstructionNode>();
        n->kind = uint8_t(kind::instruction);
        n->target = target;
        n->data = data;
        return xml(tracked_ptr<const detail::XmlNode>(std::move(n)));
    }

    inline string xml::name() const noexcept {
        switch (type()) {
            case kind::element:
                return static_cast<const detail::XmlElementNode*>(_node.get())->name;
            case kind::instruction:
                return static_cast<const detail::XmlInstructionNode*>(_node.get())->target;
            default:
                return string();
        }
    }

    inline string xml::local_name() const noexcept {
        switch (type()) {
            case kind::element:
                return static_cast<const detail::XmlElementNode*>(_node.get())->local;
            case kind::instruction:
                return static_cast<const detail::XmlInstructionNode*>(_node.get())->target;
            default:
                return string();
        }
    }

    inline string xml::namespace_uri() const noexcept {
        auto e = detail::xml_element(_node);
        return e ? e->uri : string();
    }

    inline bool xml::_matches(std::string_view qname, const string& local, const string& uri, std::string_view wanted) noexcept {
        if (!wanted.empty() && wanted[0] == '{') {
            auto close = wanted.find('}');
            return close != std::string_view::npos && uri.view() == wanted.substr(1, close - 1) && local.view() == wanted.substr(close + 1);
        }
        return qname == wanted;
    }

    inline optional<string> xml::attribute(const string& name) const {
        auto e = detail::xml_element(_node);
        if (!e) {
            return nullopt;
        }
        auto w = name.view();
        bool expanded = !w.empty() && w[0] == '{';
        for (auto& a : e->attributes) {
            if (expanded ? _matches(a.name.view(), string(detail::xml_local(a.name.view())), a.namespace_uri, w) : a.name.view() == w) {
                return a.value;
            }
        }
        return nullopt;
    }

    inline slice<const struct xml::attribute> xml::attributes() const noexcept {
        auto e = detail::xml_element(_node);
        return e ? e->attributes.as_slice() : slice<const struct xml::attribute>();
    }

    inline slice<const xml> xml::children() const noexcept {
        auto e = detail::xml_element(_node);
        return e ? e->children.as_slice() : slice<const xml>();
    }

    inline xml xml::child(const string& name) const {
        auto e = detail::xml_element(_node);
        if (!e) {
            return xml();
        }
        for (auto& c : e->children) {
            auto ce = detail::xml_element(c._node);
            if (ce && _matches(ce->name.view(), ce->local, ce->uri, name.view())) {
                return c;
            }
        }
        return xml();
    }

    inline generator<xml> xml::children(const string& name) const {
        // the node and the name by value, into the frame: the body runs
        // later, when the caller's temporaries are gone
        return _children(*this, name);
    }

    inline generator<xml> xml::_children(xml self, string wanted) {
        auto e = detail::xml_element(self._node);
        if (!e) {
            co_return;
        }
        for (auto& c : e->children) {
            auto ce = detail::xml_element(c._node);
            if (ce && _matches(ce->name.view(), ce->local, ce->uri, wanted.view())) {
                co_yield c;
            }
        }
    }

    inline string xml::text() const {
        switch (type()) {
            case kind::none:
                return string();
            case kind::text:
            case kind::comment:
                return static_cast<const detail::XmlTextNode*>(_node.get())->text;
            case kind::instruction:
                return static_cast<const detail::XmlInstructionNode*>(_node.get())->data;
            case kind::element:
                break;
        }
        // The texts of the subtree in order, with a stack of its own
        std::string out;
        string single;
        size_t pieces = 0;
        struct Item {
            const detail::XmlElementNode* element;
            size_t next;
        };
        std::vector<Item> stack{Item{static_cast<const detail::XmlElementNode*>(_node.get()), 0}};
        while (!stack.empty()) {
            auto& top = stack.back();
            if (top.next == top.element->children.size()) {
                stack.pop_back();
                continue;
            }
            const xml& c = top.element->children[top.next++];
            if (c.type() == kind::text) {
                auto& t = static_cast<const detail::XmlTextNode*>(c._node.get())->text;
                if (pieces++ == 0) {
                    single = t;
                } else {
                    if (pieces == 2) {
                        out.assign(single.data(), single.size());
                    }
                    out.append(t.data(), t.size());
                }
            } else if (auto ce = detail::xml_element(c._node)) {
                stack.push_back(Item{ce, 0});
            }
        }
        return pieces <= 1 ? single : string(out);
    }

    inline xml xml::set(const string& name, const string& value) const {
        auto e = detail::xml_element(_node);
        if (!e) {
            throw invalid_argument("sgcl::encoding::xml::set: not an element");
        }
        detail::xml_check_name(name);
        auto n = detail::xml_element_copy(*e);
        size_t count = e->attributes.size();
        size_t at = count;
        for (size_t k = 0; k < count; ++k) {
            if (e->attributes[k].name == name) {
                at = k;
                break;
            }
        }
        n->attributes = dynamic_array<struct attribute>(at == count ? count + 1 : count);
        for (size_t k = 0; k < count; ++k) {
            n->attributes[k] = e->attributes[k];
        }
        if (at == count) {
            n->attributes[at] = {name, value, detail::xml_attribute_namespace(name.view(), e)};
        } else {
            n->attributes[at].value = value;
        }
        n->children = e->children;
        return xml(tracked_ptr<const detail::XmlNode>(std::move(n)));
    }

    inline xml xml::erase(const string& name) const {
        auto e = detail::xml_element(_node);
        if (!e) {
            throw invalid_argument("sgcl::encoding::xml::erase: not an element");
        }
        size_t count = e->attributes.size();
        size_t at = count;
        for (size_t k = 0; k < count; ++k) {
            if (e->attributes[k].name == name) {
                at = k;
                break;
            }
        }
        if (at == count) {
            return *this;
        }
        auto n = detail::xml_element_copy(*e);
        if (count > 1) {
            n->attributes = dynamic_array<struct attribute>(count - 1);
            for (size_t k = 0, o = 0; k < count; ++k) {
                if (k != at) {
                    n->attributes[o++] = e->attributes[k];
                }
            }
        }
        n->children = e->children;
        return xml(tracked_ptr<const detail::XmlNode>(std::move(n)));
    }

    inline xml xml::push_back(const xml& child) const {
        auto e = detail::xml_element(_node);
        if (!e) {
            throw invalid_argument("sgcl::encoding::xml::push_back: not an element");
        }
        if (!child.exists()) {
            throw invalid_argument("sgcl::encoding::xml::push_back: xml() is no node");
        }
        auto n = detail::xml_element_copy(*e);
        n->attributes = e->attributes;
        size_t count = e->children.size();
        n->children = dynamic_array<xml>(count + 1);
        for (size_t k = 0; k < count; ++k) {
            n->children[k] = e->children[k];
        }
        n->children[count] = child;
        return xml(tracked_ptr<const detail::XmlNode>(std::move(n)));
    }

    inline string xml::to_string(const style& s) const {
        detail::XmlOut o(s.indent);
        if (s.declaration) {
            o.declaration();
        }
        o.node(*this);
        return string(std::string_view(o.out));
    }

    inline bool xml::_equal(const xml& a, const xml& b) {
        std::vector<std::pair<const xml*, const xml*>> work{{&a, &b}};
        while (!work.empty()) {
            auto [x, y] = work.back();
            work.pop_back();
            if (x->_node == y->_node) {
                continue;
            }
            if (x->type() != y->type()) {
                return false;
            }
            switch (x->type()) {
                case kind::none:
                    break;
                case kind::text:
                case kind::comment:
                    if (static_cast<const detail::XmlTextNode*>(x->_node.get())->text != static_cast<const detail::XmlTextNode*>(y->_node.get())->text) {
                        return false;
                    }
                    break;
                case kind::instruction: {
                    auto p = static_cast<const detail::XmlInstructionNode*>(x->_node.get());
                    auto q = static_cast<const detail::XmlInstructionNode*>(y->_node.get());
                    if (p->target != q->target || p->data != q->data) {
                        return false;
                    }
                    break;
                }
                case kind::element: {
                    auto p = static_cast<const detail::XmlElementNode*>(x->_node.get());
                    auto q = static_cast<const detail::XmlElementNode*>(y->_node.get());
                    if (p->name != q->name || p->uri != q->uri || p->attributes.size() != q->attributes.size()
                        || p->children.size() != q->children.size()) {
                        return false;
                    }
                    // attributes in any order: sorted by name, when there are many
                    size_t n = p->attributes.size();
                    if (n <= 16) {
                        for (auto& pa : p->attributes) {
                            bool found = false;
                            for (auto& qa : q->attributes) {
                                if (pa.name == qa.name) {
                                    found = pa == qa;
                                    break;
                                }
                            }
                            if (!found) {
                                return false;
                            }
                        }
                    } else {
                        std::vector<const struct attribute*> pa, qa;
                        for (auto& t : p->attributes) {
                            pa.push_back(&t);
                        }
                        for (auto& t : q->attributes) {
                            qa.push_back(&t);
                        }
                        auto by_name = [](const struct attribute* l, const struct attribute* r) { return l->name.view() < r->name.view(); };
                        std::sort(pa.begin(), pa.end(), by_name);
                        std::sort(qa.begin(), qa.end(), by_name);
                        for (size_t k = 0; k < n; ++k) {
                            if (!(*pa[k] == *qa[k])) {
                                return false;
                            }
                        }
                    }
                    for (size_t k = 0; k < p->children.size(); ++k) {
                        work.emplace_back(&p->children[k], &q->children[k]);
                    }
                    break;
                }
            }
        }
        return true;
    }

    inline expected<xml, xml::error> xml::_parse_with(reader& r) {
        xml root;
        uint64_t at = 0;
        while (auto n = r.read()) {
            if (n->is_element()) {
                root = std::move(*n);
                at = r._node_offset;
            }
        }
        r._node_offset = at;   // the root's, for a mapping's error
        if (r.last_error()) {
            return unexpected<error>(*r.last_error());
        }
        return root;
    }

    inline expected<xml, xml::error> xml::parse(const string& text) {
        return parse(text, options());
    }

    inline expected<xml, xml::error> xml::parse(const io::reader& in) {
        return parse(in, options());
    }

    inline async::task<expected<xml, xml::error>> xml::async_parse(const io::reader& in) {
        return async_parse(in, options());
    }

    inline expected<xml, xml::error> xml::parse(const string& text, const options& o) {
        reader r(text, o);
        return _parse_with(r);
    }

    inline expected<xml, xml::error> xml::parse(const io::reader& in, const options& o) {
        reader r(in, o);
        return _parse_with(r);
    }

    inline async::task<expected<xml, xml::error>> xml::async_parse(io::reader in, options o) {
        reader r(std::move(in), o);
        xml root;
        while (auto n = co_await r.async_read()) {
            if (n->is_element()) {
                root = std::move(*n);
            }
        }
        if (r.last_error()) {
            co_return unexpected<error>(*r.last_error());
        }
        co_return root;
    }

    inline xml::builder::builder(const string& name)
    : _name(name) {
        detail::xml_check_name(name);
    }

    inline xml::builder& xml::builder::set(const string& name, const string& value) {
        detail::xml_check_name(name);
        for (auto& a : _attributes) {
            if (a.name == name) {
                a.value = value;
                return *this;
            }
        }
        _attributes.push_back({name, value, string()});
        return *this;
    }

    inline xml::builder& xml::builder::push_back(const xml& child) {
        if (!child.exists()) {
            throw invalid_argument("sgcl::encoding::xml::builder::push_back: xml() is no node");
        }
        _children.push_back(child);
        return *this;
    }

    inline xml xml::builder::build() {
        xml base(_name);
        auto e = static_pointer_cast<detail::XmlElementNode>(const_pointer_cast<detail::XmlNode>(base._node));
        if (!_attributes.empty()) {
            e->attributes = dynamic_array<struct xml::attribute>(_attributes.begin(), _attributes.end());
            for (auto& a : e->attributes) {
                a.namespace_uri = detail::xml_attribute_namespace(a.name.view(), e.get());
            }
        }
        if (!_children.empty()) {
            e->children = dynamic_array<xml>(_children.begin(), _children.end());
        }
        _attributes.clear();
        _children.clear();
        return base;
    }
}

#include "detail/xml_fields.h"
