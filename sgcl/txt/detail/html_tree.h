//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "html_tokenizer.h"

#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// The tree construction of the WHATWG HTML standard (13.2.6), written from its
// insertion modes: the stack of open elements, the list of active formatting
// elements with its Noah's Ark clause, the adoption agency, foster parenting,
// the template insertion modes, foreign content (SVG, MathML) with its
// integration points and adjustments, the quirks modes of the DOCTYPE, and
// the fragment case. The tree is built in an arena of nodes linked by index,
// which html.h then turns into the immutable nodes of a document.
namespace sgcl::txt::detail::html {
    enum Ns : uint8_t { NsHtml, NsSvg, NsMath };
    enum Kind : uint8_t { KDocument, KDoctype, KElement, KText, KComment, KFragment };

    struct Node {
        uint8_t kind = KElement;
        uint8_t ns = NsHtml;
        std::string name;
        std::vector<Attr> attrs;
        std::string data;              // text, comment; DOCTYPE name
        std::string public_id, system_id;
        int parent = -1, first = -1, last = -1, next = -1, prev = -1;
        int content = -1;              // a template's contents
        int open = 0;                  // the times it stands on the stack of open elements
    };

    struct Tree {
        std::vector<Node> nodes;
        int document = 0;
        bool quirks = false, limited_quirks = false;
        std::vector<ParseError> errors;
        int fragment_root = -1;        // the fragment case: the html element whose children are the fragment
    };

    inline bool one_of(std::string_view n, std::initializer_list<std::string_view> list) noexcept {
        for (std::string_view x : list) {
            if (n == x) {
                return true;
            }
        }
        return false;
    }

    // The stack of open elements, which also counts its HTML elements by name:
    // the scope checks of a deep document (a hundred thousand nested divs)
    // ask for names that are not open at all, and answer that at once
    class OpenStack {
    public:
        explicit OpenStack(Tree& t) noexcept
        : _t(t) {
        }

        void push_back(int i) {
            _v.push_back(i);
            add(i, 1);
        }

        void pop_back() {
            add(_v.back(), -1);
            _v.pop_back();
        }

        void erase_at(size_t k) {
            add(_v[k], -1);
            _v.erase(_v.begin() + long(k));
        }

        void insert_at(size_t k, int i) {
            _v.insert(_v.begin() + long(k), i);
            add(i, 1);
        }

        void set(size_t k, int i) {
            add(_v[k], -1);
            _v[k] = i;
            add(i, 1);
        }

        int back() const noexcept {
            return _v.back();
        }

        bool empty() const noexcept {
            return _v.empty();
        }

        size_t size() const noexcept {
            return _v.size();
        }

        int operator[](size_t k) const noexcept {
            return _v[k];
        }

        std::vector<int>::const_iterator begin() const noexcept {
            return _v.begin();
        }

        std::vector<int>::const_iterator end() const noexcept {
            return _v.end();
        }

        // how many HTML elements of the name are open
        int count(std::string_view name) const {
            auto it = _counts.find(name);
            return it == _counts.end() ? 0 : it->second;
        }

    private:
        Tree& _t;
        std::vector<int> _v;
        std::map<std::string, int, std::less<>> _counts;

        void add(int i, int d) {
            Node& e = _t.nodes[size_t(i)];
            e.open += d;
            if (e.kind == KElement && e.ns == NsHtml) {
                auto it = _counts.find(e.name);
                if (it == _counts.end()) {
                    _counts.emplace(e.name, d);
                } else {
                    it->second += d;
                }
            }
        }
    };

    enum Mode : uint8_t {
        MInitial, MBeforeHtml, MBeforeHead, MInHead, MInHeadNoscript, MAfterHead, MInBody, MText, MInTable,
        MInTableText, MInCaption, MInColumnGroup, MInTableBody, MInRow, MInCell, MInSelect, MInSelectInTable,
        MInTemplate, MAfterBody, MInFrameset, MAfterFrameset, MAfterAfterBody, MAfterAfterFrameset,
    };

    class TreeBuilder {
    public:
        TreeBuilder(std::string_view input, bool scripting, bool collect_errors)
        : _tok(input, collect_errors ? &_tree.errors : nullptr)
        , _scripting(scripting)
        , _collect(collect_errors) {
            _tree.nodes.push_back(Node());
            _tree.nodes[0].kind = KDocument;
        }

        // The whole document
        Tree parse() {
            run();
            return std::move(_tree);
        }

        // The fragment case (13.4): children of a context element of this name
        Tree parse_fragment(std::string_view context, uint8_t context_ns = NsHtml) {
            int ctx = new_node(KElement);
            _tree.nodes[ctx].name = std::string(context);
            _tree.nodes[ctx].ns = context_ns;
            _context = ctx;
            if (context_ns == NsHtml) {
                if (one_of(context, {"title", "textarea"})) {
                    _tok.state = SRcdata;
                } else if (one_of(context, {"style", "xmp", "iframe", "noembed", "noframes"})
                           || (context == "noscript" && _scripting)) {
                    _tok.state = SRawtext;
                } else if (context == "script") {
                    _tok.state = SScriptData;
                } else if (context == "plaintext") {
                    _tok.state = SPlaintext;
                }
            }
            int root = new_node(KElement);
            _tree.nodes[root].name = "html";
            append(_tree.document, root);
            _open.push_back(root);
            if (context_ns == NsHtml && context == "template") {
                _template_modes.push_back(MInTemplate);
            }
            _tok.last_start_tag = std::string(context);
            reset_mode();
            // the nearest form ancestor of the context: none here (the context has no parent)
            _tree.fragment_root = root;
            run();
            return std::move(_tree);
        }

    private:
        Tree _tree;
        Tokenizer _tok;
        bool _scripting;
        bool _collect;
        uint8_t _mode = MInitial;
        uint8_t _original = MInitial;
        OpenStack _open{_tree};
        std::vector<int> _formatting;          // -1: a marker
        std::vector<Token> _formatting_tokens; // the token each formatting element was made for
        std::vector<uint8_t> _template_modes;
        int _head = -1, _form = -1;
        int _context = -1;
        bool _frameset_ok = true;
        bool _foster = false;
        bool _skip_lf = false;
        bool _stopped = false;
        std::string _pending;                  // in table text
        bool _pending_nonws = false;
        Token _t;                              // the token being processed

        // ---- the arena ----
        int new_node(uint8_t kind) {
            _tree.nodes.push_back(Node());
            _tree.nodes.back().kind = kind;
            return int(_tree.nodes.size() - 1);
        }

        Node& n(int i) noexcept {
            return _tree.nodes[size_t(i)];
        }

        void append(int parent, int child) {
            Node& c = n(child);
            c.parent = parent;
            c.next = -1;
            c.prev = n(parent).last;
            if (c.prev >= 0) {
                n(c.prev).next = child;
            } else {
                n(parent).first = child;
            }
            n(parent).last = child;
        }

        void insert_before(int parent, int child, int ref) {
            if (ref < 0) {
                append(parent, child);
                return;
            }
            Node& c = n(child);
            c.parent = parent;
            c.next = ref;
            c.prev = n(ref).prev;
            if (c.prev >= 0) {
                n(c.prev).next = child;
            } else {
                n(parent).first = child;
            }
            n(ref).prev = child;
        }

        void detach(int child) {
            Node& c = n(child);
            if (c.parent < 0) {
                return;
            }
            if (c.prev >= 0) {
                n(c.prev).next = c.next;
            } else {
                n(c.parent).first = c.next;
            }
            if (c.next >= 0) {
                n(c.next).prev = c.prev;
            } else {
                n(c.parent).last = c.prev;
            }
            c.parent = c.prev = c.next = -1;
        }

        bool is(int i, std::string_view name, uint8_t ns = NsHtml) noexcept {
            return i >= 0 && n(i).kind == KElement && n(i).ns == ns && n(i).name == name;
        }

        bool is_html(int i) noexcept {
            return i >= 0 && n(i).kind == KElement && n(i).ns == NsHtml;
        }

        int current() noexcept {
            return _open.empty() ? -1 : _open.back();
        }

        int adjusted_current() noexcept {
            if (_context >= 0 && _open.size() == 1) {
                return _context;
            }
            return current();
        }

        void error(const char* code) {
            if (_collect) {
                _tree.errors.push_back(ParseError{_t.offset, code});
            }
        }

        // ---- element categories ----
        static bool special(const Node& e) noexcept {
            if (e.ns == NsHtml) {
                return one_of(e.name, {"address", "applet", "area", "article", "aside", "base", "basefont", "bgsound",
                    "blockquote", "body", "br", "button", "caption", "center", "col", "colgroup", "dd", "details", "dir",
                    "div", "dl", "dt", "embed", "fieldset", "figcaption", "figure", "footer", "form", "frame",
                    "frameset", "h1", "h2", "h3", "h4", "h5", "h6", "head", "header", "hgroup", "hr", "html", "iframe",
                    "img", "input", "keygen", "li", "link", "listing", "main", "marquee", "menu", "meta", "nav",
                    "noembed", "noframes", "noscript", "object", "ol", "p", "param", "plaintext", "pre", "script",
                    "search", "section", "select", "source", "style", "summary", "table", "tbody", "td", "template",
                    "textarea", "tfoot", "th", "thead", "title", "tr", "track", "ul", "wbr", "xmp"});
            }
            if (e.ns == NsMath) {
                return one_of(e.name, {"mi", "mo", "mn", "ms", "mtext", "annotation-xml"});
            }
            return one_of(e.name, {"foreignObject", "desc", "title"});
        }

        static bool formatting(std::string_view name) noexcept {
            return one_of(name, {"a", "b", "big", "code", "em", "font", "i", "nobr", "s", "small", "strike", "strong",
                                 "tt", "u"});
        }

        // the scope boundaries (13.2.4.2)
        bool scope_boundary(int i, int kind) noexcept {
            const Node& e = n(i);
            if (kind == 3) {   // table scope
                return e.ns == NsHtml && one_of(e.name, {"html", "table", "template"});
            }
            if (kind == 4) {   // select scope: everything but optgroup and option
                return !(e.ns == NsHtml && one_of(e.name, {"optgroup", "option"}));
            }
            bool base = (e.ns == NsHtml && one_of(e.name, {"applet", "caption", "html", "table", "td", "th",
                                                           "marquee", "object", "template"}))
                || (e.ns == NsMath && one_of(e.name, {"mi", "mo", "mn", "ms", "mtext", "annotation-xml"}))
                || (e.ns == NsSvg && one_of(e.name, {"foreignObject", "desc", "title"}));
            if (base) {
                return true;
            }
            if (kind == 1) {   // list item scope
                return e.ns == NsHtml && one_of(e.name, {"ol", "ul"});
            }
            if (kind == 2) {   // button scope
                return e.ns == NsHtml && e.name == "button";
            }
            return false;
        }

        // 0 default, 1 list item, 2 button, 3 table, 4 select
        bool in_scope(std::string_view name, int kind = 0) noexcept {
            if (_open.count(name) == 0) {
                return false;
            }
            for (size_t k = _open.size(); k-- > 0;) {
                int i = _open[k];
                if (is_html(i) && n(i).name == name) {
                    return true;
                }
                if (scope_boundary(i, kind)) {
                    return false;
                }
            }
            return false;
        }

        bool node_in_scope(int target) noexcept {
            for (size_t k = _open.size(); k-- > 0;) {
                int i = _open[k];
                if (i == target) {
                    return true;
                }
                if (scope_boundary(i, 0)) {
                    return false;
                }
            }
            return false;
        }

        bool heading_in_scope() noexcept {
            for (size_t k = _open.size(); k-- > 0;) {
                int i = _open[k];
                if (is_html(i) && one_of(n(i).name, {"h1", "h2", "h3", "h4", "h5", "h6"})) {
                    return true;
                }
                if (scope_boundary(i, 0)) {
                    return false;
                }
            }
            return false;
        }

        bool in_stack(int i) noexcept {
            return n(i).open > 0;
        }

        bool stack_has(std::string_view name) noexcept {
            return _open.count(name) > 0;
        }

        void pop_until(std::string_view name) {
            while (!_open.empty()) {
                int i = _open.back();
                _open.pop_back();
                if (is(i, name)) {
                    return;
                }
            }
        }

        void pop_until_heading() {
            while (!_open.empty()) {
                int i = _open.back();
                _open.pop_back();
                if (is_html(i) && one_of(n(i).name, {"h1", "h2", "h3", "h4", "h5", "h6"})) {
                    return;
                }
            }
        }

        void remove_from_stack(int target) {
            for (size_t k = _open.size(); k-- > 0;) {
                if (_open[k] == target) {
                    _open.erase_at(k);
                    return;
                }
            }
        }

        void generate_implied(std::string_view except = {}) {
            while (!_open.empty()) {
                int i = _open.back();
                if (is_html(i) && one_of(n(i).name, {"dd", "dt", "li", "optgroup", "option", "p", "rb", "rp", "rt", "rtc"})
                    && n(i).name != except) {
                    _open.pop_back();
                } else {
                    return;
                }
            }
        }

        void generate_implied_thoroughly() {
            while (!_open.empty()) {
                int i = _open.back();
                if (is_html(i) && one_of(n(i).name, {"caption", "colgroup", "dd", "dt", "li", "optgroup", "option", "p",
                                                     "rb", "rp", "rt", "rtc", "tbody", "td", "tfoot", "th", "thead", "tr"})) {
                    _open.pop_back();
                } else {
                    return;
                }
            }
        }

        void close_p() {
            generate_implied("p");
            if (!is(current(), "p")) {
                error("unexpected-end-tag");
            }
            pop_until("p");
        }

        // ---- inserting ----
        struct Place {
            int parent;
            int before;   // -1: append
        };

        Place appropriate_place(int override_target = -1) {
            int target = override_target >= 0 ? override_target : current();
            Place p{target, -1};
            if (_foster && is_html(target) && one_of(n(target).name, {"table", "tbody", "tfoot", "thead", "tr"})) {
                int last_template = -1, last_table = -1;
                size_t t_at = 0, tb_at = 0;
                for (size_t k = _open.size(); k-- > 0;) {
                    if (last_template < 0 && is(_open[k], "template")) {
                        last_template = _open[k];
                        t_at = k;
                    }
                    if (last_table < 0 && is(_open[k], "table")) {
                        last_table = _open[k];
                        tb_at = k;
                    }
                }
                if (last_template >= 0 && (last_table < 0 || t_at > tb_at)) {
                    p = Place{n(last_template).content, -1};
                } else if (last_table < 0) {
                    p = Place{_open[0], -1};
                } else if (n(last_table).parent >= 0) {
                    p = Place{n(last_table).parent, last_table};
                } else {
                    p = Place{_open[tb_at - 1], -1};
                }
            }
            if (is(p.parent, "template")) {
                p = Place{n(p.parent).content, -1};
            }
            return p;
        }

        // the SVG and MathML names and attributes adjusted (13.2.6.2-4)
        static void adjust_svg_name(std::string& name) {
            static constexpr std::string_view pairs[][2] = {
                {"altglyph", "altGlyph"}, {"altglyphdef", "altGlyphDef"}, {"altglyphitem", "altGlyphItem"},
                {"animatecolor", "animateColor"}, {"animatemotion", "animateMotion"},
                {"animatetransform", "animateTransform"}, {"clippath", "clipPath"}, {"feblend", "feBlend"},
                {"fecolormatrix", "feColorMatrix"}, {"fecomponenttransfer", "feComponentTransfer"},
                {"fecomposite", "feComposite"}, {"feconvolvematrix", "feConvolveMatrix"},
                {"fediffuselighting", "feDiffuseLighting"}, {"fedisplacementmap", "feDisplacementMap"},
                {"fedistantlight", "feDistantLight"}, {"fedropshadow", "feDropShadow"}, {"feflood", "feFlood"},
                {"fefunca", "feFuncA"}, {"fefuncb", "feFuncB"}, {"fefuncg", "feFuncG"}, {"fefuncr", "feFuncR"},
                {"fegaussianblur", "feGaussianBlur"}, {"feimage", "feImage"}, {"femerge", "feMerge"},
                {"femergenode", "feMergeNode"}, {"femorphology", "feMorphology"}, {"feoffset", "feOffset"},
                {"fepointlight", "fePointLight"}, {"fespecularlighting", "feSpecularLighting"},
                {"fespotlight", "feSpotLight"}, {"fetile", "feTile"}, {"feturbulence", "feTurbulence"},
                {"foreignobject", "foreignObject"}, {"glyphref", "glyphRef"}, {"lineargradient", "linearGradient"},
                {"radialgradient", "radialGradient"}, {"textpath", "textPath"},
            };
            for (const auto& p : pairs) {
                if (name == p[0]) {
                    name = std::string(p[1]);
                    return;
                }
            }
        }

        static void adjust_svg_attrs(std::vector<Attr>& attrs) {
            static constexpr std::string_view pairs[][2] = {
                {"attributename", "attributeName"}, {"attributetype", "attributeType"}, {"basefrequency", "baseFrequency"},
                {"baseprofile", "baseProfile"}, {"calcmode", "calcMode"}, {"clippathunits", "clipPathUnits"},
                {"diffuseconstant", "diffuseConstant"}, {"edgemode", "edgeMode"}, {"filterunits", "filterUnits"},
                {"glyphref", "glyphRef"}, {"gradienttransform", "gradientTransform"}, {"gradientunits", "gradientUnits"},
                {"kernelmatrix", "kernelMatrix"}, {"kernelunitlength", "kernelUnitLength"}, {"keypoints", "keyPoints"},
                {"keysplines", "keySplines"}, {"keytimes", "keyTimes"}, {"lengthadjust", "lengthAdjust"},
                {"limitingconeangle", "limitingConeAngle"}, {"markerheight", "markerHeight"},
                {"markerunits", "markerUnits"}, {"markerwidth", "markerWidth"}, {"maskcontentunits", "maskContentUnits"},
                {"maskunits", "maskUnits"}, {"numoctaves", "numOctaves"}, {"pathlength", "pathLength"},
                {"patterncontentunits", "patternContentUnits"}, {"patterntransform", "patternTransform"},
                {"patternunits", "patternUnits"}, {"pointsatx", "pointsAtX"}, {"pointsaty", "pointsAtY"},
                {"pointsatz", "pointsAtZ"}, {"preservealpha", "preserveAlpha"},
                {"preserveaspectratio", "preserveAspectRatio"}, {"primitiveunits", "primitiveUnits"},
                {"refx", "refX"}, {"refy", "refY"}, {"repeatcount", "repeatCount"}, {"repeatdur", "repeatDur"},
                {"requiredextensions", "requiredExtensions"}, {"requiredfeatures", "requiredFeatures"},
                {"specularconstant", "specularConstant"}, {"specularexponent", "specularExponent"},
                {"spreadmethod", "spreadMethod"}, {"startoffset", "startOffset"}, {"stddeviation", "stdDeviation"},
                {"stitchtiles", "stitchTiles"}, {"surfacescale", "surfaceScale"}, {"systemlanguage", "systemLanguage"},
                {"tablevalues", "tableValues"}, {"targetx", "targetX"}, {"targety", "targetY"},
                {"textlength", "textLength"}, {"viewbox", "viewBox"}, {"viewtarget", "viewTarget"},
                {"xchannelselector", "xChannelSelector"}, {"ychannelselector", "yChannelSelector"},
                {"zoomandpan", "zoomAndPan"},
            };
            for (Attr& a : attrs) {
                for (const auto& p : pairs) {
                    if (a.name == p[0]) {
                        a.name = std::string(p[1]);
                        break;
                    }
                }
            }
        }

        static void adjust_math_attrs(std::vector<Attr>& attrs) {
            for (Attr& a : attrs) {
                if (a.name == "definitionurl") {
                    a.name = "definitionURL";
                }
            }
        }

        int create_element(const Token& t, uint8_t ns) {
            int e = new_node(KElement);
            n(e).ns = ns;
            n(e).name = t.name;
            n(e).attrs = t.attrs;
            if (ns == NsHtml && t.name == "template") {
                int frag = new_node(KFragment);
                n(e).content = frag;
            }
            return e;
        }

        int insert_element(const Token& t, uint8_t ns = NsHtml) {
            Place p = appropriate_place();
            int e = create_element(t, ns);
            insert_before(p.parent, e, p.before);
            _open.push_back(e);
            return e;
        }

        int insert_foreign(Token t, uint8_t ns) {
            if (ns == NsMath) {
                adjust_math_attrs(t.attrs);
            } else if (ns == NsSvg) {
                adjust_svg_attrs(t.attrs);
            }
            return insert_element(t, ns);
        }

        void insert_text(std::string_view data) {
            if (data.empty()) {
                return;
            }
            Place p = appropriate_place();
            if (n(p.parent).kind == KDocument) {
                return;
            }
            int prev = p.before >= 0 ? n(p.before).prev : n(p.parent).last;
            if (prev >= 0 && n(prev).kind == KText) {
                n(prev).data.append(data);
                return;
            }
            int t = new_node(KText);
            n(t).data = std::string(data);
            insert_before(p.parent, t, p.before);
        }

        void insert_comment(const Token& t, int parent = -1) {
            int c = new_node(KComment);
            n(c).data = t.data;
            if (parent >= 0) {
                append(parent, c);
                return;
            }
            Place p = appropriate_place();
            insert_before(p.parent, c, p.before);
        }

        void insert_generic(const Token& t, uint8_t tok_state) {
            insert_element(t);
            _tok.state = tok_state;
            _original = _mode;
            _mode = MText;
        }

        // ---- the list of active formatting elements ----
        void push_formatting(int e, const Token& t) {
            // Noah's Ark: at most three of the same name and attributes after the last marker
            int same = 0;
            size_t earliest = 0;
            for (size_t k = _formatting.size(); k-- > 0;) {
                int f = _formatting[k];
                if (f < 0) {
                    break;
                }
                const Node& a = n(f);
                const Node& b = n(e);
                if (a.name == b.name && a.ns == b.ns && a.attrs.size() == b.attrs.size()) {
                    bool all = true;
                    for (const Attr& x : b.attrs) {
                        bool found = false;
                        for (const Attr& y : a.attrs) {
                            if (x.name == y.name && x.value == y.value) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            all = false;
                            break;
                        }
                    }
                    if (all) {
                        ++same;
                        earliest = k;
                    }
                }
            }
            if (same >= 3) {
                _formatting.erase(_formatting.begin() + long(earliest));
                _formatting_tokens.erase(_formatting_tokens.begin() + long(earliest));
            }
            _formatting.push_back(e);
            _formatting_tokens.push_back(t);
        }

        void push_marker() {
            _formatting.push_back(-1);
            _formatting_tokens.push_back(Token());
        }

        void clear_to_marker() {
            while (!_formatting.empty()) {
                int f = _formatting.back();
                _formatting.pop_back();
                _formatting_tokens.pop_back();
                if (f < 0) {
                    return;
                }
            }
        }

        int formatting_index(int e) noexcept {
            for (size_t k = _formatting.size(); k-- > 0;) {
                if (_formatting[k] == e) {
                    return int(k);
                }
            }
            return -1;
        }

        void reconstruct_formatting() {
            if (_formatting.empty()) {
                return;
            }
            int last = _formatting.back();
            if (last < 0 || in_stack(last)) {
                return;
            }
            size_t k = _formatting.size() - 1;
            while (k > 0) {
                int prev = _formatting[k - 1];
                if (prev < 0 || in_stack(prev)) {
                    break;
                }
                --k;
            }
            for (; k < _formatting.size(); ++k) {
                Token t = _formatting_tokens[k];
                int e = insert_element(t);
                _formatting[k] = e;
            }
        }

        // ---- the adoption agency (13.2.6.4.7) ----
        // false: act as any other end tag
        bool adoption_agency(const std::string& subject) {
            int cur = current();
            if (is(cur, subject) && formatting_index(cur) < 0) {
                _open.pop_back();
                return true;
            }
            for (int outer = 0; outer < 8; ++outer) {
                // the last formatting element of the subject's name after the last marker
                int fe = -1;
                int fe_index = -1;
                for (size_t k = _formatting.size(); k-- > 0;) {
                    int f = _formatting[k];
                    if (f < 0) {
                        break;
                    }
                    if (n(f).name == subject && n(f).ns == NsHtml) {
                        fe = f;
                        fe_index = int(k);
                        break;
                    }
                }
                if (fe < 0) {
                    return false;
                }
                if (!in_stack(fe)) {
                    error("adoption-agency-1.2");
                    _formatting.erase(_formatting.begin() + fe_index);
                    _formatting_tokens.erase(_formatting_tokens.begin() + fe_index);
                    return true;
                }
                if (!node_in_scope(fe)) {
                    error("adoption-agency-4.4");
                    return true;
                }
                if (fe != current()) {
                    error("adoption-agency-1.3");
                }
                // the furthest block
                size_t fe_at = 0;
                for (size_t k = 0; k < _open.size(); ++k) {
                    if (_open[k] == fe) {
                        fe_at = k;
                        break;
                    }
                }
                int fb = -1;
                size_t fb_at = 0;
                for (size_t k = fe_at + 1; k < _open.size(); ++k) {
                    if (special(n(_open[k]))) {
                        fb = _open[k];
                        fb_at = k;
                        break;
                    }
                }
                if (fb < 0) {
                    while (_open.size() > fe_at) {
                        _open.pop_back();
                    }
                    int idx = formatting_index(fe);
                    _formatting.erase(_formatting.begin() + idx);
                    _formatting_tokens.erase(_formatting_tokens.begin() + idx);
                    return true;
                }
                int common = _open[fe_at - 1];
                int bookmark = formatting_index(fe);
                int node = fb, last = fb;
                size_t node_at = fb_at;
                for (int inner = 1;; ++inner) {
                    --node_at;
                    node = _open[node_at];
                    if (node == fe) {
                        break;
                    }
                    int fi = formatting_index(node);
                    if (inner > 3 && fi >= 0) {
                        _formatting.erase(_formatting.begin() + fi);
                        _formatting_tokens.erase(_formatting_tokens.begin() + fi);
                        if (fi < bookmark) {
                            --bookmark;
                        }
                        fi = -1;
                    }
                    if (fi < 0) {
                        _open.erase_at(node_at);
                        continue;
                    }
                    // a new element for the node's token, in the list and the stack
                    Token t = _formatting_tokens[size_t(fi)];
                    int e = create_element(t, NsHtml);
                    _formatting[size_t(fi)] = e;
                    _open.set(node_at, e);
                    node = e;
                    if (last == fb) {
                        bookmark = fi + 1;
                    }
                    detach(last);
                    append(node, last);
                    last = node;
                }
                // the last node into the common ancestor (or its foster place)
                detach(last);
                if (is_html(common) && one_of(n(common).name, {"table", "tbody", "tfoot", "thead", "tr"})) {
                    bool saved = _foster;
                    _foster = true;
                    Place p = appropriate_place(common);
                    _foster = saved;
                    insert_before(p.parent, last, p.before);
                } else if (is(common, "template")) {
                    append(n(common).content, last);
                } else {
                    append(common, last);
                }
                // a new element for the formatting element, taking the furthest block's children
                int fi = formatting_index(fe);
                Token t = _formatting_tokens[size_t(fi)];
                int e = create_element(t, NsHtml);
                while (n(fb).first >= 0) {
                    int c = n(fb).first;
                    detach(c);
                    append(e, c);
                }
                append(fb, e);
                _formatting.erase(_formatting.begin() + fi);
                _formatting_tokens.erase(_formatting_tokens.begin() + fi);
                if (fi < bookmark) {
                    --bookmark;
                }
                if (bookmark > int(_formatting.size())) {
                    bookmark = int(_formatting.size());
                }
                _formatting.insert(_formatting.begin() + bookmark, e);
                _formatting_tokens.insert(_formatting_tokens.begin() + bookmark, t);
                remove_from_stack(fe);
                for (size_t k = 0; k < _open.size(); ++k) {
                    if (_open[k] == fb) {
                        _open.insert_at(k + 1, e);
                        break;
                    }
                }
            }
            return true;
        }

        // ---- reset the insertion mode appropriately (13.2.4.1) ----
        void reset_mode() {
            for (size_t k = _open.size(); k-- > 0;) {
                int node = _open[k];
                bool last = k == 0;
                if (last && _context >= 0) {
                    node = _context;
                }
                if (!is_html(node)) {
                    if (last) {
                        _mode = MInBody;
                        return;
                    }
                    continue;
                }
                const std::string& name = n(node).name;
                if (name == "select") {
                    if (!last) {
                        for (size_t j = k; j-- > 0;) {
                            int anc = _open[j];
                            if (is(anc, "template")) {
                                break;
                            }
                            if (is(anc, "table")) {
                                _mode = MInSelectInTable;
                                return;
                            }
                        }
                    }
                    _mode = MInSelect;
                    return;
                }
                if ((name == "td" || name == "th") && !last) {
                    _mode = MInCell;
                    return;
                }
                if (name == "tr") {
                    _mode = MInRow;
                    return;
                }
                if (name == "tbody" || name == "thead" || name == "tfoot") {
                    _mode = MInTableBody;
                    return;
                }
                if (name == "caption") {
                    _mode = MInCaption;
                    return;
                }
                if (name == "colgroup") {
                    _mode = MInColumnGroup;
                    return;
                }
                if (name == "table") {
                    _mode = MInTable;
                    return;
                }
                if (name == "template") {
                    _mode = _template_modes.empty() ? MInTemplate : _template_modes.back();
                    return;
                }
                if (name == "head" && !last) {
                    _mode = MInHead;
                    return;
                }
                if (name == "body") {
                    _mode = MInBody;
                    return;
                }
                if (name == "frameset") {
                    _mode = MInFrameset;
                    return;
                }
                if (name == "html") {
                    _mode = _head < 0 ? MBeforeHead : MAfterHead;
                    return;
                }
                if (last) {
                    _mode = MInBody;
                    return;
                }
            }
            _mode = MInBody;
        }

        // ---- the main loop ----
        void run() {
            while (!_stopped) {
                _tok.cdata_allowed = !_open.empty() && !is_html(adjusted_current());
                _tok.next(_t);
                if (_t.type == TkCharacters) {
                    std::string data = std::move(_t.data);
                    size_t i = 0;
                    if (_skip_lf) {
                        _skip_lf = false;
                        if (!data.empty() && data[0] == '\n') {
                            i = 1;
                        }
                    }
                    // segments of one class: white space, NUL, anything else
                    while (i < data.size()) {
                        auto cls = [](char c) { return c == '\0' ? 1 : html_ws(c) ? 0 : 2; };
                        int k = cls(data[i]);
                        size_t j = i + 1;
                        while (j < data.size() && cls(data[j]) == k) {
                            ++j;
                        }
                        _t.type = TkCharacters;
                        _t.data = data.substr(i, j - i);
                        dispatch();
                        i = j;
                    }
                    continue;
                }
                _skip_lf = false;
                dispatch();
                if (_t.type == TkEof) {
                    break;
                }
            }
        }

        // the class of the current character token
        bool ws_chars() const noexcept {
            return !_t.data.empty() && html_ws(_t.data[0]);
        }

        bool nul_chars() const noexcept {
            return !_t.data.empty() && _t.data[0] == '\0';
        }

        bool start(std::string_view name) const noexcept {
            return _t.type == TkStartTag && _t.name == name;
        }

        bool end(std::string_view name) const noexcept {
            return _t.type == TkEndTag && _t.name == name;
        }

        bool start_of(std::initializer_list<std::string_view> l) const noexcept {
            return _t.type == TkStartTag && one_of(_t.name, l);
        }

        bool end_of(std::initializer_list<std::string_view> l) const noexcept {
            return _t.type == TkEndTag && one_of(_t.name, l);
        }

        bool mathml_text_ip(int i) noexcept {
            return i >= 0 && n(i).ns == NsMath && one_of(n(i).name, {"mi", "mo", "mn", "ms", "mtext"});
        }

        bool html_ip(int i) noexcept {
            if (i < 0 || n(i).kind != KElement) {
                return false;
            }
            if (n(i).ns == NsSvg) {
                return one_of(n(i).name, {"foreignObject", "desc", "title"});
            }
            if (n(i).ns == NsMath && n(i).name == "annotation-xml") {
                for (const Attr& a : n(i).attrs) {
                    if (a.name == "encoding") {
                        std::string v;
                        for (char c : a.value) {
                            v.push_back(ascii_lower(c));
                        }
                        return v == "text/html" || v == "application/xhtml+xml";
                    }
                }
            }
            return false;
        }

        void dispatch() {
            int acn = adjusted_current();
            bool html_rules = _open.empty() || is_html(acn)
                || (mathml_text_ip(acn) && _t.type == TkStartTag && _t.name != "mglyph" && _t.name != "malignmark")
                || (mathml_text_ip(acn) && _t.type == TkCharacters)
                || (acn >= 0 && n(acn).ns == NsMath && n(acn).name == "annotation-xml" && start("svg"))
                || (html_ip(acn) && (_t.type == TkStartTag || _t.type == TkCharacters))
                || _t.type == TkEof;
            if (html_rules) {
                process(_mode);
            } else {
                foreign();
            }
        }

        void process(uint8_t mode) {
            switch (mode) {
                case MInitial: initial(); break;
                case MBeforeHtml: before_html(); break;
                case MBeforeHead: before_head(); break;
                case MInHead: in_head(); break;
                case MInHeadNoscript: in_head_noscript(); break;
                case MAfterHead: after_head(); break;
                case MInBody: in_body(); break;
                case MText: text_mode(); break;
                case MInTable: in_table(); break;
                case MInTableText: in_table_text(); break;
                case MInCaption: in_caption(); break;
                case MInColumnGroup: in_column_group(); break;
                case MInTableBody: in_table_body(); break;
                case MInRow: in_row(); break;
                case MInCell: in_cell(); break;
                case MInSelect: in_select(); break;
                case MInSelectInTable: in_select_in_table(); break;
                case MInTemplate: in_template(); break;
                case MAfterBody: after_body(); break;
                case MInFrameset: in_frameset(); break;
                case MAfterFrameset: after_frameset(); break;
                case MAfterAfterBody: after_after_body(); break;
                case MAfterAfterFrameset: after_after_frameset(); break;
                default: break;
            }
        }

        void reprocess(uint8_t mode) {
            _mode = mode;
            dispatch();
        }

        // ---- 13.2.6.4.1 initial ----
        static bool starts_with_ci(std::string_view s, std::string_view p) noexcept {
            if (s.size() < p.size()) {
                return false;
            }
            for (size_t i = 0; i < p.size(); ++i) {
                if (ascii_lower(s[i]) != p[i]) {
                    return false;
                }
            }
            return true;
        }

        static bool equals_ci(std::string_view s, std::string_view p) noexcept {
            return s.size() == p.size() && starts_with_ci(s, p);
        }

        void doctype_modes(const Token& t) {
            static constexpr std::string_view quirk_prefixes[] = {
                "+//silmaril//dtd html pro v0r11 19970101//", "-//as//dtd html 3.0 aswedit + extensions//",
                "-//advasoft ltd//dtd html 3.0 aswedit + extensions//", "-//ietf//dtd html 2.0 level 1//",
                "-//ietf//dtd html 2.0 level 2//", "-//ietf//dtd html 2.0 strict level 1//",
                "-//ietf//dtd html 2.0 strict level 2//", "-//ietf//dtd html 2.0 strict//", "-//ietf//dtd html 2.0//",
                "-//ietf//dtd html 2.1e//", "-//ietf//dtd html 3.0//", "-//ietf//dtd html 3.2 final//",
                "-//ietf//dtd html 3.2//", "-//ietf//dtd html 3//", "-//ietf//dtd html level 0//",
                "-//ietf//dtd html level 1//", "-//ietf//dtd html level 2//", "-//ietf//dtd html level 3//",
                "-//ietf//dtd html strict level 0//", "-//ietf//dtd html strict level 1//",
                "-//ietf//dtd html strict level 2//", "-//ietf//dtd html strict level 3//",
                "-//ietf//dtd html strict//", "-//ietf//dtd html//", "-//metrius//dtd metrius presentational//",
                "-//microsoft//dtd internet explorer 2.0 html strict//", "-//microsoft//dtd internet explorer 2.0 html//",
                "-//microsoft//dtd internet explorer 2.0 tables//", "-//microsoft//dtd internet explorer 3.0 html strict//",
                "-//microsoft//dtd internet explorer 3.0 html//", "-//microsoft//dtd internet explorer 3.0 tables//",
                "-//netscape comm. corp.//dtd html//", "-//netscape comm. corp.//dtd strict html//",
                "-//o'reilly and associates//dtd html 2.0//", "-//o'reilly and associates//dtd html extended 1.0//",
                "-//o'reilly and associates//dtd html extended relaxed 1.0//",
                "-//sq//dtd html 2.0 hotmetal + extensions//",
                "-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//",
                "-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//",
                "-//spyglass//dtd html 2.0 extended//", "-//sun microsystems corp.//dtd hotjava html//",
                "-//sun microsystems corp.//dtd hotjava strict html//", "-//w3c//dtd html 3 1995-03-24//",
                "-//w3c//dtd html 3.2 draft//", "-//w3c//dtd html 3.2 final//", "-//w3c//dtd html 3.2//",
                "-//w3c//dtd html 3.2s draft//", "-//w3c//dtd html 4.0 frameset//", "-//w3c//dtd html 4.0 transitional//",
                "-//w3c//dtd html experimental 19960712//", "-//w3c//dtd html experimental 970421//",
                "-//w3c//dtd w3 html//", "-//w3o//dtd w3 html 3.0//", "-//webtechs//dtd mozilla html 2.0//",
                "-//webtechs//dtd mozilla html//",
            };
            const std::string& pub = t.public_id;
            const std::string& sys = t.system_id;
            bool quirks = t.force_quirks || !t.has_name || t.name != "html";
            if (!quirks && t.has_public) {
                if (equals_ci(pub, "-//w3o//dtd w3 html strict 3.0//en//") || equals_ci(pub, "-/w3c/dtd html 4.0 transitional/en")
                    || equals_ci(pub, "html")) {
                    quirks = true;
                }
                for (std::string_view p : quirk_prefixes) {
                    if (starts_with_ci(pub, p)) {
                        quirks = true;
                        break;
                    }
                }
                if (!t.has_system && (starts_with_ci(pub, "-//w3c//dtd html 4.01 frameset//")
                                      || starts_with_ci(pub, "-//w3c//dtd html 4.01 transitional//"))) {
                    quirks = true;
                }
            }
            if (!quirks && t.has_system && equals_ci(sys, "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd")) {
                quirks = true;
            }
            if (quirks) {
                _tree.quirks = true;
                return;
            }
            if (t.has_public && (starts_with_ci(pub, "-//w3c//dtd xhtml 1.0 frameset//")
                                 || starts_with_ci(pub, "-//w3c//dtd xhtml 1.0 transitional//")
                                 || (t.has_system && (starts_with_ci(pub, "-//w3c//dtd html 4.01 frameset//")
                                                      || starts_with_ci(pub, "-//w3c//dtd html 4.01 transitional//"))))) {
                _tree.limited_quirks = true;
            }
        }

        void initial() {
            if (_t.type == TkCharacters && ws_chars()) {
                return;
            }
            if (_t.type == TkComment) {
                insert_comment(_t, _tree.document);
                return;
            }
            if (_t.type == TkDoctype) {
                if (!_t.has_name || _t.name != "html" || _t.has_public
                    || (_t.has_system && _t.system_id != "about:legacy-compat")) {
                    error("bad-doctype");
                }
                int d = new_node(KDoctype);
                n(d).data = _t.has_name ? _t.name : std::string();
                n(d).public_id = _t.public_id;
                n(d).system_id = _t.system_id;
                append(_tree.document, d);
                doctype_modes(_t);
                _mode = MBeforeHtml;
                return;
            }
            error("missing-doctype");
            _tree.quirks = true;
            reprocess(MBeforeHtml);
        }

        // ---- 13.2.6.4.2 before html ----
        void before_html() {
            if (_t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (_t.type == TkComment) {
                insert_comment(_t, _tree.document);
                return;
            }
            if (_t.type == TkCharacters && ws_chars()) {
                return;
            }
            if (start("html")) {
                int e = create_element(_t, NsHtml);
                append(_tree.document, e);
                _open.push_back(e);
                _mode = MBeforeHead;
                return;
            }
            if (_t.type == TkEndTag && !one_of(_t.name, {"head", "body", "html", "br"})) {
                error("unexpected-end-tag");
                return;
            }
            Token html;
            html.type = TkStartTag;
            html.name = "html";
            int e = create_element(html, NsHtml);
            append(_tree.document, e);
            _open.push_back(e);
            reprocess(MBeforeHead);
        }

        // ---- 13.2.6.4.3 before head ----
        void before_head() {
            if (_t.type == TkCharacters && ws_chars()) {
                return;
            }
            if (_t.type == TkComment) {
                insert_comment(_t);
                return;
            }
            if (_t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (start("html")) {
                in_body();
                return;
            }
            if (start("head")) {
                _head = insert_element(_t);
                _mode = MInHead;
                return;
            }
            if (_t.type == TkEndTag && !one_of(_t.name, {"head", "body", "html", "br"})) {
                error("unexpected-end-tag");
                return;
            }
            Token head;
            head.type = TkStartTag;
            head.name = "head";
            _head = insert_element(head);
            reprocess(MInHead);
        }

        // ---- 13.2.6.4.4 in head ----
        void in_head() {
            if (_t.type == TkCharacters && ws_chars()) {
                insert_text(_t.data);
                return;
            }
            if (_t.type == TkComment) {
                insert_comment(_t);
                return;
            }
            if (_t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (start("html")) {
                in_body();
                return;
            }
            if (start_of({"base", "basefont", "bgsound", "link"})) {
                insert_element(_t);
                _open.pop_back();
                return;
            }
            if (start("meta")) {
                insert_element(_t);
                _open.pop_back();
                return;
            }
            if (start("title")) {
                insert_generic(_t, SRcdata);
                return;
            }
            if ((start("noscript") && _scripting) || start_of({"noframes", "style"})) {
                insert_generic(_t, SRawtext);
                return;
            }
            if (start("noscript")) {
                insert_element(_t);
                _mode = MInHeadNoscript;
                return;
            }
            if (start("script")) {
                Place p = appropriate_place();
                int e = create_element(_t, NsHtml);
                insert_before(p.parent, e, p.before);
                _open.push_back(e);
                _tok.state = SScriptData;
                _original = _mode;
                _mode = MText;
                return;
            }
            if (end("head")) {
                _open.pop_back();
                _mode = MAfterHead;
                return;
            }
            if (start("template")) {
                insert_element(_t);
                push_marker();
                _frameset_ok = false;
                _mode = MInTemplate;
                _template_modes.push_back(MInTemplate);
                return;
            }
            if (end("template")) {
                if (!stack_has("template")) {
                    error("unexpected-end-tag");
                    return;
                }
                generate_implied_thoroughly();
                if (!is(current(), "template")) {
                    error("unexpected-end-tag");
                }
                pop_until("template");
                clear_to_marker();
                if (!_template_modes.empty()) {
                    _template_modes.pop_back();
                }
                reset_mode();
                return;
            }
            if (start("head") || (_t.type == TkEndTag && !one_of(_t.name, {"body", "html", "br"}))) {
                error("unexpected-token");
                return;
            }
            _open.pop_back();
            reprocess(MAfterHead);
        }

        // ---- 13.2.6.4.5 in head noscript ----
        void in_head_noscript() {
            if (_t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (start("html")) {
                in_body();
                return;
            }
            if (end("noscript")) {
                _open.pop_back();
                _mode = MInHead;
                return;
            }
            if ((_t.type == TkCharacters && ws_chars()) || _t.type == TkComment
                || start_of({"basefont", "bgsound", "link", "meta", "noframes", "style"})) {
                in_head();
                return;
            }
            if (start_of({"head", "noscript"}) || (_t.type == TkEndTag && !end("br"))) {
                error("unexpected-token");
                return;
            }
            error("unexpected-token");
            _open.pop_back();
            reprocess(MInHead);
        }

        // ---- 13.2.6.4.6 after head ----
        void after_head() {
            if (_t.type == TkCharacters && ws_chars()) {
                insert_text(_t.data);
                return;
            }
            if (_t.type == TkComment) {
                insert_comment(_t);
                return;
            }
            if (_t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (start("html")) {
                in_body();
                return;
            }
            if (start("body")) {
                insert_element(_t);
                _frameset_ok = false;
                _mode = MInBody;
                return;
            }
            if (start("frameset")) {
                insert_element(_t);
                _mode = MInFrameset;
                return;
            }
            if (start_of({"base", "basefont", "bgsound", "link", "meta", "noframes", "script", "style", "template",
                          "title"})) {
                error("unexpected-start-tag");
                _open.push_back(_head);
                in_head();
                remove_from_stack(_head);
                return;
            }
            if (end("template")) {
                in_head();
                return;
            }
            if (start("head") || (_t.type == TkEndTag && !one_of(_t.name, {"body", "html", "br"}))) {
                error("unexpected-token");
                return;
            }
            Token body;
            body.type = TkStartTag;
            body.name = "body";
            insert_element(body);
            reprocess(MInBody);
        }

        // ---- 13.2.6.4.7 in body ----
        void any_other_end_tag() {
            for (size_t k = _open.size(); k-- > 0;) {
                int node = _open[k];
                if (is_html(node) && n(node).name == _t.name) {
                    generate_implied(_t.name);
                    if (node != current()) {
                        error("unexpected-end-tag");
                    }
                    while (_open.size() > k) {
                        _open.pop_back();
                    }
                    return;
                }
                if (special(n(node))) {
                    error("unexpected-end-tag");
                    return;
                }
            }
        }

        void in_body() {
            const Token& t = _t;
            if (t.type == TkCharacters) {
                if (nul_chars()) {
                    error("unexpected-null-character");
                    return;
                }
                reconstruct_formatting();
                insert_text(t.data);
                if (!ws_chars()) {
                    _frameset_ok = false;
                }
                return;
            }
            if (t.type == TkComment) {
                insert_comment(t);
                return;
            }
            if (t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (t.type == TkStartTag) {
                const std::string& name = t.name;
                if (name == "html") {
                    error("unexpected-start-tag");
                    if (stack_has("template")) {
                        return;
                    }
                    for (const Attr& a : t.attrs) {
                        bool has = false;
                        for (const Attr& b : n(_open[0]).attrs) {
                            has = has || b.name == a.name;
                        }
                        if (!has) {
                            n(_open[0]).attrs.push_back(a);
                        }
                    }
                    return;
                }
                if (one_of(name, {"base", "basefont", "bgsound", "link", "meta", "noframes", "script", "style",
                                  "template", "title"})) {
                    in_head();
                    return;
                }
                if (name == "body") {
                    error("unexpected-start-tag");
                    if (_open.size() < 2 || !is(_open[1], "body") || stack_has("template")) {
                        return;
                    }
                    _frameset_ok = false;
                    for (const Attr& a : t.attrs) {
                        bool has = false;
                        for (const Attr& b : n(_open[1]).attrs) {
                            has = has || b.name == a.name;
                        }
                        if (!has) {
                            n(_open[1]).attrs.push_back(a);
                        }
                    }
                    return;
                }
                if (name == "frameset") {
                    error("unexpected-start-tag");
                    if (_open.size() < 2 || !is(_open[1], "body") || !_frameset_ok) {
                        return;
                    }
                    detach(_open[1]);
                    while (_open.size() > 1) {
                        _open.pop_back();
                    }
                    insert_element(t);
                    _mode = MInFrameset;
                    return;
                }
                if (one_of(name, {"address", "article", "aside", "blockquote", "center", "details", "dialog", "dir",
                                  "div", "dl", "fieldset", "figcaption", "figure", "footer", "header", "hgroup", "main",
                                  "menu", "nav", "ol", "p", "search", "section", "summary", "ul"})) {
                    if (in_scope("p", 2)) {
                        close_p();
                    }
                    insert_element(t);
                    return;
                }
                if (one_of(name, {"h1", "h2", "h3", "h4", "h5", "h6"})) {
                    if (in_scope("p", 2)) {
                        close_p();
                    }
                    if (is_html(current()) && one_of(n(current()).name, {"h1", "h2", "h3", "h4", "h5", "h6"})) {
                        error("unexpected-start-tag");
                        _open.pop_back();
                    }
                    insert_element(t);
                    return;
                }
                if (name == "pre" || name == "listing") {
                    if (in_scope("p", 2)) {
                        close_p();
                    }
                    insert_element(t);
                    _skip_lf = true;
                    _frameset_ok = false;
                    return;
                }
                if (name == "form") {
                    if (_form >= 0 && !stack_has("template")) {
                        error("unexpected-start-tag");
                        return;
                    }
                    if (in_scope("p", 2)) {
                        close_p();
                    }
                    int e = insert_element(t);
                    if (!stack_has("template")) {
                        _form = e;
                    }
                    return;
                }
                if (name == "li" || name == "dd" || name == "dt") {
                    _frameset_ok = false;
                    for (size_t k = _open.size(); k-- > 0;) {
                        int node = _open[k];
                        bool match = name == "li" ? is(node, "li") : (is(node, "dd") || is(node, "dt"));
                        if (match) {
                            generate_implied(n(node).name);
                            if (!is(current(), n(node).name)) {
                                error("unexpected-start-tag");
                            }
                            pop_until(n(node).name);
                            break;
                        }
                        if (special(n(node)) && !(is_html(node) && one_of(n(node).name, {"address", "div", "p"}))) {
                            break;
                        }
                    }
                    if (in_scope("p", 2)) {
                        close_p();
                    }
                    insert_element(t);
                    return;
                }
                if (name == "plaintext") {
                    if (in_scope("p", 2)) {
                        close_p();
                    }
                    insert_element(t);
                    _tok.state = SPlaintext;
                    return;
                }
                if (name == "button") {
                    if (in_scope("button")) {
                        error("unexpected-start-tag");
                        generate_implied();
                        pop_until("button");
                    }
                    reconstruct_formatting();
                    insert_element(t);
                    _frameset_ok = false;
                    return;
                }
                if (name == "a") {
                    for (size_t k = _formatting.size(); k-- > 0;) {
                        int f = _formatting[k];
                        if (f < 0) {
                            break;
                        }
                        if (is(f, "a")) {
                            error("unexpected-start-tag");
                            std::string subject = "a";
                            Token saved = _t;
                            Token as_end;
                            as_end.type = TkEndTag;
                            as_end.name = "a";
                            _t = as_end;
                            if (!adoption_agency(subject)) {
                                any_other_end_tag();
                            }
                            _t = saved;
                            int fi = formatting_index(f);
                            if (fi >= 0) {
                                _formatting.erase(_formatting.begin() + fi);
                                _formatting_tokens.erase(_formatting_tokens.begin() + fi);
                            }
                            remove_from_stack(f);
                            break;
                        }
                    }
                    reconstruct_formatting();
                    int e = insert_element(_t);
                    push_formatting(e, _t);
                    return;
                }
                if (one_of(name, {"b", "big", "code", "em", "font", "i", "s", "small", "strike", "strong", "tt", "u"})) {
                    reconstruct_formatting();
                    int e = insert_element(t);
                    push_formatting(e, t);
                    return;
                }
                if (name == "nobr") {
                    reconstruct_formatting();
                    if (in_scope("nobr")) {
                        error("unexpected-start-tag");
                        Token saved = _t;
                        Token as_end;
                        as_end.type = TkEndTag;
                        as_end.name = "nobr";
                        _t = as_end;
                        if (!adoption_agency("nobr")) {
                            any_other_end_tag();
                        }
                        _t = saved;
                        reconstruct_formatting();
                    }
                    int e = insert_element(_t);
                    push_formatting(e, _t);
                    return;
                }
                if (one_of(name, {"applet", "marquee", "object"})) {
                    reconstruct_formatting();
                    insert_element(t);
                    push_marker();
                    _frameset_ok = false;
                    return;
                }
                if (name == "table") {
                    if (!_tree.quirks && in_scope("p", 2)) {
                        close_p();
                    }
                    insert_element(t);
                    _frameset_ok = false;
                    _mode = MInTable;
                    return;
                }
                if (one_of(name, {"area", "br", "embed", "img", "keygen", "wbr"})) {
                    reconstruct_formatting();
                    insert_element(t);
                    _open.pop_back();
                    _frameset_ok = false;
                    return;
                }
                if (name == "input") {
                    reconstruct_formatting();
                    insert_element(t);
                    _open.pop_back();
                    const Attr* type = t.attr("type");
                    if (!type || !equals_ci(type->value, "hidden")) {
                        _frameset_ok = false;
                    }
                    return;
                }
                if (one_of(name, {"param", "source", "track"})) {
                    insert_element(t);
                    _open.pop_back();
                    return;
                }
                if (name == "hr") {
                    if (in_scope("p", 2)) {
                        close_p();
                    }
                    insert_element(t);
                    _open.pop_back();
                    _frameset_ok = false;
                    return;
                }
                if (name == "image") {
                    error("unexpected-start-tag");
                    _t.name = "img";
                    in_body();
                    return;
                }
                if (name == "textarea") {
                    insert_element(t);
                    _skip_lf = true;
                    _tok.state = SRcdata;
                    _original = _mode;
                    _frameset_ok = false;
                    _mode = MText;
                    return;
                }
                if (name == "xmp") {
                    if (in_scope("p", 2)) {
                        close_p();
                    }
                    reconstruct_formatting();
                    _frameset_ok = false;
                    insert_generic(t, SRawtext);
                    return;
                }
                if (name == "iframe") {
                    _frameset_ok = false;
                    insert_generic(t, SRawtext);
                    return;
                }
                if (name == "noembed" || (name == "noscript" && _scripting)) {
                    insert_generic(t, SRawtext);
                    return;
                }
                if (name == "select") {
                    reconstruct_formatting();
                    insert_element(t);
                    _frameset_ok = false;
                    if (_mode == MInTable || _mode == MInCaption || _mode == MInTableBody || _mode == MInRow || _mode == MInCell) {
                        _mode = MInSelectInTable;
                    } else {
                        _mode = MInSelect;
                    }
                    return;
                }
                if (name == "optgroup" || name == "option") {
                    if (is(current(), "option")) {
                        _open.pop_back();
                    }
                    reconstruct_formatting();
                    insert_element(t);
                    return;
                }
                if (name == "rb" || name == "rtc") {
                    if (in_scope("ruby")) {
                        generate_implied();
                        if (!is(current(), "ruby")) {
                            error("unexpected-start-tag");
                        }
                    }
                    insert_element(t);
                    return;
                }
                if (name == "rp" || name == "rt") {
                    if (in_scope("ruby")) {
                        generate_implied("rtc");
                        if (!is(current(), "rtc") && !is(current(), "ruby")) {
                            error("unexpected-start-tag");
                        }
                    }
                    insert_element(t);
                    return;
                }
                if (name == "math" || name == "svg") {
                    reconstruct_formatting();
                    Token f = t;
                    insert_foreign(f, name == "math" ? NsMath : NsSvg);
                    if (t.self_closing) {
                        _open.pop_back();
                    }
                    return;
                }
                if (one_of(name, {"caption", "col", "colgroup", "frame", "head", "tbody", "td", "tfoot", "th", "thead",
                                  "tr"})) {
                    error("unexpected-start-tag");
                    return;
                }
                reconstruct_formatting();
                insert_element(t);
                return;
            }
            if (t.type == TkEndTag) {
                const std::string& name = t.name;
                if (name == "template") {
                    in_head();
                    return;
                }
                if (name == "body" || name == "html") {
                    if (!in_scope("body")) {
                        error("unexpected-end-tag");
                        return;
                    }
                    _mode = MAfterBody;
                    if (name == "html") {
                        dispatch();
                    }
                    return;
                }
                if (one_of(name, {"address", "article", "aside", "blockquote", "button", "center", "details", "dialog",
                                  "dir", "div", "dl", "fieldset", "figcaption", "figure", "footer", "header", "hgroup",
                                  "listing", "main", "menu", "nav", "ol", "pre", "search", "section", "summary", "ul"})) {
                    if (!in_scope(name)) {
                        error("unexpected-end-tag");
                        return;
                    }
                    generate_implied();
                    if (!is(current(), name)) {
                        error("unexpected-end-tag");
                    }
                    pop_until(name);
                    return;
                }
                if (name == "form") {
                    if (!stack_has("template")) {
                        int node = _form;
                        _form = -1;
                        if (node < 0 || !node_in_scope(node)) {
                            error("unexpected-end-tag");
                            return;
                        }
                        generate_implied();
                        if (current() != node) {
                            error("unexpected-end-tag");
                        }
                        remove_from_stack(node);
                    } else {
                        if (!in_scope("form")) {
                            error("unexpected-end-tag");
                            return;
                        }
                        generate_implied();
                        if (!is(current(), "form")) {
                            error("unexpected-end-tag");
                        }
                        pop_until("form");
                    }
                    return;
                }
                if (name == "p") {
                    if (!in_scope("p", 2)) {
                        error("unexpected-end-tag");
                        Token p;
                        p.type = TkStartTag;
                        p.name = "p";
                        insert_element(p);
                    }
                    close_p();
                    return;
                }
                if (name == "li") {
                    if (!in_scope("li", 1)) {
                        error("unexpected-end-tag");
                        return;
                    }
                    generate_implied("li");
                    if (!is(current(), "li")) {
                        error("unexpected-end-tag");
                    }
                    pop_until("li");
                    return;
                }
                if (name == "dd" || name == "dt") {
                    if (!in_scope(name)) {
                        error("unexpected-end-tag");
                        return;
                    }
                    generate_implied(name);
                    if (!is(current(), name)) {
                        error("unexpected-end-tag");
                    }
                    pop_until(name);
                    return;
                }
                if (one_of(name, {"h1", "h2", "h3", "h4", "h5", "h6"})) {
                    if (!heading_in_scope()) {
                        error("unexpected-end-tag");
                        return;
                    }
                    generate_implied();
                    if (!is(current(), name)) {
                        error("unexpected-end-tag");
                    }
                    pop_until_heading();
                    return;
                }
                if (formatting(name)) {
                    std::string subject = name;
                    if (!adoption_agency(subject)) {
                        any_other_end_tag();
                    }
                    return;
                }
                if (one_of(name, {"applet", "marquee", "object"})) {
                    if (!in_scope(name)) {
                        error("unexpected-end-tag");
                        return;
                    }
                    generate_implied();
                    if (!is(current(), name)) {
                        error("unexpected-end-tag");
                    }
                    pop_until(name);
                    clear_to_marker();
                    return;
                }
                if (name == "br") {
                    error("unexpected-end-tag");
                    Token br;
                    br.type = TkStartTag;
                    br.name = "br";
                    _t = br;
                    in_body();
                    return;
                }
                any_other_end_tag();
                return;
            }
            // end of file
            if (!_template_modes.empty()) {
                in_template();
                return;
            }
            _stopped = true;
        }

        // ---- 13.2.6.4.8 text ----
        void text_mode() {
            if (_t.type == TkCharacters) {
                insert_text(_t.data);
                return;
            }
            if (_t.type == TkEof) {
                error("eof-in-text");
                _open.pop_back();
                _mode = _original;
                dispatch();
                return;
            }
            if (_t.type == TkEndTag) {
                _open.pop_back();
                _mode = _original;
            }
        }

        // ---- 13.2.6.4.9 in table ----
        void clear_to_table_context(std::initializer_list<std::string_view> names) {
            while (!_open.empty()) {
                int c = current();
                if (is_html(c) && (one_of(n(c).name, names) || n(c).name == "template" || n(c).name == "html")) {
                    return;
                }
                _open.pop_back();
            }
        }

        void in_table() {
            const Token& t = _t;
            if (t.type == TkCharacters && is_html(current())
                && one_of(n(current()).name, {"table", "tbody", "template", "tfoot", "thead", "tr"})) {
                _pending.clear();
                _pending_nonws = false;
                _original = _mode;
                _mode = MInTableText;
                dispatch();
                return;
            }
            if (t.type == TkComment) {
                insert_comment(t);
                return;
            }
            if (t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (t.type == TkStartTag) {
                const std::string& name = t.name;
                if (name == "caption") {
                    clear_to_table_context({"table"});
                    push_marker();
                    insert_element(t);
                    _mode = MInCaption;
                    return;
                }
                if (name == "colgroup") {
                    clear_to_table_context({"table"});
                    insert_element(t);
                    _mode = MInColumnGroup;
                    return;
                }
                if (name == "col") {
                    clear_to_table_context({"table"});
                    Token cg;
                    cg.type = TkStartTag;
                    cg.name = "colgroup";
                    insert_element(cg);
                    reprocess(MInColumnGroup);
                    return;
                }
                if (one_of(name, {"tbody", "tfoot", "thead"})) {
                    clear_to_table_context({"table"});
                    insert_element(t);
                    _mode = MInTableBody;
                    return;
                }
                if (one_of(name, {"td", "th", "tr"})) {
                    clear_to_table_context({"table"});
                    Token tb;
                    tb.type = TkStartTag;
                    tb.name = "tbody";
                    insert_element(tb);
                    reprocess(MInTableBody);
                    return;
                }
                if (name == "table") {
                    error("unexpected-start-tag");
                    if (!in_scope("table", 3)) {
                        return;
                    }
                    pop_until("table");
                    reset_mode();
                    dispatch();
                    return;
                }
                if (one_of(name, {"style", "script", "template"})) {
                    in_head();
                    return;
                }
                if (name == "input") {
                    const Attr* type = t.attr("type");
                    if (type && equals_ci(type->value, "hidden")) {
                        error("unexpected-start-tag");
                        insert_element(t);
                        _open.pop_back();
                        return;
                    }
                }
                if (name == "form") {
                    error("unexpected-start-tag");
                    if (stack_has("template") || _form >= 0) {
                        return;
                    }
                    _form = insert_element(t);
                    _open.pop_back();
                    return;
                }
            }
            if (t.type == TkEndTag) {
                const std::string& name = t.name;
                if (name == "table") {
                    if (!in_scope("table", 3)) {
                        error("unexpected-end-tag");
                        return;
                    }
                    pop_until("table");
                    reset_mode();
                    return;
                }
                if (one_of(name, {"body", "caption", "col", "colgroup", "html", "tbody", "td", "tfoot", "th", "thead",
                                  "tr"})) {
                    error("unexpected-end-tag");
                    return;
                }
                if (name == "template") {
                    in_head();
                    return;
                }
            }
            if (t.type == TkEof) {
                in_body();
                return;
            }
            error("unexpected-token-in-table");
            _foster = true;
            in_body();
            _foster = false;
        }

        // ---- 13.2.6.4.10 in table text ----
        void in_table_text() {
            if (_t.type == TkCharacters) {
                if (nul_chars()) {
                    error("unexpected-null-character");
                    return;
                }
                _pending.append(_t.data);
                if (!ws_chars()) {
                    _pending_nonws = true;
                }
                return;
            }
            if (_pending_nonws) {
                error("unexpected-character-in-table");
                Token saved = _t;
                _t.clear();
                _t.type = TkCharacters;
                _t.data = _pending;
                _foster = true;
                // as in body: formatting reconstructed, the text inserted, frameset-ok off
                reconstruct_formatting();
                insert_text(_pending);
                _frameset_ok = false;
                _foster = false;
                _t = saved;
            } else {
                insert_text(_pending);
            }
            _pending.clear();
            _pending_nonws = false;
            _mode = _original;
            dispatch();
        }

        // ---- 13.2.6.4.11 in caption ----
        void in_caption() {
            const Token& t = _t;
            bool table_start = start_of({"caption", "col", "colgroup", "tbody", "td", "tfoot", "th", "thead", "tr"});
            if (end("caption") || table_start || end("table")) {
                if (!in_scope("caption", 3)) {
                    error("unexpected-token");
                    return;
                }
                generate_implied();
                if (!is(current(), "caption")) {
                    error("unexpected-token");
                }
                pop_until("caption");
                clear_to_marker();
                _mode = MInTable;
                if (!end("caption")) {
                    dispatch();
                }
                return;
            }
            if (t.type == TkEndTag && one_of(t.name, {"body", "col", "colgroup", "html", "tbody", "td", "tfoot", "th",
                                                      "thead", "tr"})) {
                error("unexpected-end-tag");
                return;
            }
            in_body();
        }

        // ---- 13.2.6.4.12 in column group ----
        void in_column_group() {
            const Token& t = _t;
            if (t.type == TkCharacters && ws_chars()) {
                insert_text(t.data);
                return;
            }
            if (t.type == TkComment) {
                insert_comment(t);
                return;
            }
            if (t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (start("html")) {
                in_body();
                return;
            }
            if (start("col")) {
                insert_element(t);
                _open.pop_back();
                return;
            }
            if (end("colgroup")) {
                if (!is(current(), "colgroup")) {
                    error("unexpected-end-tag");
                    return;
                }
                _open.pop_back();
                _mode = MInTable;
                return;
            }
            if (end("col")) {
                error("unexpected-end-tag");
                return;
            }
            if (start("template") || end("template")) {
                in_head();
                return;
            }
            if (t.type == TkEof) {
                in_body();
                return;
            }
            if (!is(current(), "colgroup")) {
                error("unexpected-token");
                return;
            }
            _open.pop_back();
            reprocess(MInTable);
        }

        // ---- 13.2.6.4.13 in table body ----
        void in_table_body() {
            const Token& t = _t;
            if (start("tr")) {
                clear_to_table_context({"tbody", "tfoot", "thead"});
                insert_element(t);
                _mode = MInRow;
                return;
            }
            if (start_of({"th", "td"})) {
                error("unexpected-start-tag");
                clear_to_table_context({"tbody", "tfoot", "thead"});
                Token tr;
                tr.type = TkStartTag;
                tr.name = "tr";
                insert_element(tr);
                reprocess(MInRow);
                return;
            }
            if (end_of({"tbody", "tfoot", "thead"})) {
                if (!in_scope(t.name, 3)) {
                    error("unexpected-end-tag");
                    return;
                }
                clear_to_table_context({"tbody", "tfoot", "thead"});
                _open.pop_back();
                _mode = MInTable;
                return;
            }
            if (start_of({"caption", "col", "colgroup", "tbody", "tfoot", "thead"}) || end("table")) {
                if (!in_scope("tbody", 3) && !in_scope("thead", 3) && !in_scope("tfoot", 3)) {
                    error("unexpected-token");
                    return;
                }
                clear_to_table_context({"tbody", "tfoot", "thead"});
                _open.pop_back();
                reprocess(MInTable);
                return;
            }
            if (end_of({"body", "caption", "col", "colgroup", "html", "td", "th", "tr"})) {
                error("unexpected-end-tag");
                return;
            }
            in_table();
        }

        // ---- 13.2.6.4.14 in row ----
        void in_row() {
            const Token& t = _t;
            if (start_of({"th", "td"})) {
                clear_to_table_context({"tr"});
                insert_element(t);
                _mode = MInCell;
                push_marker();
                return;
            }
            if (end("tr")) {
                if (!in_scope("tr", 3)) {
                    error("unexpected-end-tag");
                    return;
                }
                clear_to_table_context({"tr"});
                _open.pop_back();
                _mode = MInTableBody;
                return;
            }
            if (start_of({"caption", "col", "colgroup", "tbody", "tfoot", "thead", "tr"}) || end("table")) {
                if (!in_scope("tr", 3)) {
                    error("unexpected-token");
                    return;
                }
                clear_to_table_context({"tr"});
                _open.pop_back();
                reprocess(MInTableBody);
                return;
            }
            if (end_of({"tbody", "tfoot", "thead"})) {
                if (!in_scope(t.name, 3)) {
                    error("unexpected-end-tag");
                    return;
                }
                if (!in_scope("tr", 3)) {
                    return;
                }
                clear_to_table_context({"tr"});
                _open.pop_back();
                reprocess(MInTableBody);
                return;
            }
            if (end_of({"body", "caption", "col", "colgroup", "html", "td", "th"})) {
                error("unexpected-end-tag");
                return;
            }
            in_table();
        }

        // ---- 13.2.6.4.15 in cell ----
        void close_cell() {
            generate_implied();
            if (!is(current(), "td") && !is(current(), "th")) {
                error("unexpected-token");
            }
            while (!_open.empty()) {
                int c = current();
                _open.pop_back();
                if (is(c, "td") || is(c, "th")) {
                    break;
                }
            }
            clear_to_marker();
            _mode = MInRow;
        }

        void in_cell() {
            const Token& t = _t;
            if (end_of({"td", "th"})) {
                if (!in_scope(t.name, 3)) {
                    error("unexpected-end-tag");
                    return;
                }
                generate_implied();
                if (!is(current(), t.name)) {
                    error("unexpected-end-tag");
                }
                pop_until(t.name);
                clear_to_marker();
                _mode = MInRow;
                return;
            }
            if (start_of({"caption", "col", "colgroup", "tbody", "td", "tfoot", "th", "thead", "tr"})) {
                if (!in_scope("td", 3) && !in_scope("th", 3)) {
                    error("unexpected-start-tag");
                    return;
                }
                close_cell();
                dispatch();
                return;
            }
            if (end_of({"body", "caption", "col", "colgroup", "html"})) {
                error("unexpected-end-tag");
                return;
            }
            if (end_of({"table", "tbody", "tfoot", "thead", "tr"})) {
                if (!in_scope(t.name, 3)) {
                    error("unexpected-end-tag");
                    return;
                }
                close_cell();
                dispatch();
                return;
            }
            in_body();
        }

        // ---- 13.2.6.4.16 in select ----
        void in_select() {
            const Token& t = _t;
            if (t.type == TkCharacters) {
                if (nul_chars()) {
                    error("unexpected-null-character");
                    return;
                }
                insert_text(t.data);
                return;
            }
            if (t.type == TkComment) {
                insert_comment(t);
                return;
            }
            if (t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (start("html")) {
                in_body();
                return;
            }
            if (start("option")) {
                if (is(current(), "option")) {
                    _open.pop_back();
                }
                insert_element(t);
                return;
            }
            if (start("optgroup")) {
                if (is(current(), "option")) {
                    _open.pop_back();
                }
                if (is(current(), "optgroup")) {
                    _open.pop_back();
                }
                insert_element(t);
                return;
            }
            if (start("hr")) {
                if (is(current(), "option")) {
                    _open.pop_back();
                }
                if (is(current(), "optgroup")) {
                    _open.pop_back();
                }
                insert_element(t);
                _open.pop_back();
                return;
            }
            if (end("optgroup")) {
                if (is(current(), "option") && _open.size() >= 2 && is(_open[_open.size() - 2], "optgroup")) {
                    _open.pop_back();
                }
                if (is(current(), "optgroup")) {
                    _open.pop_back();
                } else {
                    error("unexpected-end-tag");
                }
                return;
            }
            if (end("option")) {
                if (is(current(), "option")) {
                    _open.pop_back();
                } else {
                    error("unexpected-end-tag");
                }
                return;
            }
            if (end("select")) {
                if (!in_scope("select", 4)) {
                    error("unexpected-end-tag");
                    return;
                }
                pop_until("select");
                reset_mode();
                return;
            }
            if (start("select")) {
                error("unexpected-start-tag");
                if (!in_scope("select", 4)) {
                    return;
                }
                pop_until("select");
                reset_mode();
                return;
            }
            if (start_of({"input", "keygen", "textarea"})) {
                error("unexpected-start-tag");
                if (!in_scope("select", 4)) {
                    return;
                }
                pop_until("select");
                reset_mode();
                dispatch();
                return;
            }
            if (start_of({"script", "template"}) || end("template")) {
                in_head();
                return;
            }
            if (t.type == TkEof) {
                in_body();
                return;
            }
            error("unexpected-token-in-select");
        }

        // ---- 13.2.6.4.17 in select in table ----
        void in_select_in_table() {
            const Token& t = _t;
            if (start_of({"caption", "table", "tbody", "tfoot", "thead", "tr", "td", "th"})) {
                error("unexpected-start-tag");
                pop_until("select");
                reset_mode();
                dispatch();
                return;
            }
            if (end_of({"caption", "table", "tbody", "tfoot", "thead", "tr", "td", "th"})) {
                error("unexpected-end-tag");
                if (!in_scope(t.name, 3)) {
                    return;
                }
                pop_until("select");
                reset_mode();
                dispatch();
                return;
            }
            in_select();
        }

        // ---- 13.2.6.4.18 in template ----
        void in_template() {
            const Token& t = _t;
            if (t.type == TkCharacters || t.type == TkComment || t.type == TkDoctype) {
                in_body();
                return;
            }
            if (start_of({"base", "basefont", "bgsound", "link", "meta", "noframes", "script", "style", "template",
                          "title"})
                || end("template")) {
                in_head();
                return;
            }
            auto switch_to = [&](uint8_t mode) {
                if (!_template_modes.empty()) {
                    _template_modes.pop_back();
                }
                _template_modes.push_back(mode);
                reprocess(mode);
            };
            if (start_of({"caption", "colgroup", "tbody", "tfoot", "thead"})) {
                switch_to(MInTable);
                return;
            }
            if (start("col")) {
                switch_to(MInColumnGroup);
                return;
            }
            if (start("tr")) {
                switch_to(MInTableBody);
                return;
            }
            if (start_of({"td", "th"})) {
                switch_to(MInRow);
                return;
            }
            if (t.type == TkStartTag) {
                switch_to(MInBody);
                return;
            }
            if (t.type == TkEndTag) {
                error("unexpected-end-tag");
                return;
            }
            // end of file
            if (!stack_has("template")) {
                _stopped = true;
                return;
            }
            error("eof-in-template");
            pop_until("template");
            clear_to_marker();
            if (!_template_modes.empty()) {
                _template_modes.pop_back();
            }
            reset_mode();
            dispatch();
        }

        // ---- 13.2.6.4.19 after body ----
        void after_body() {
            const Token& t = _t;
            if (t.type == TkCharacters && ws_chars()) {
                in_body();
                return;
            }
            if (t.type == TkComment) {
                insert_comment(t, _open[0]);
                return;
            }
            if (t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (start("html")) {
                in_body();
                return;
            }
            if (end("html")) {
                if (_context >= 0) {
                    error("unexpected-end-tag");
                    return;
                }
                _mode = MAfterAfterBody;
                return;
            }
            if (t.type == TkEof) {
                _stopped = true;
                return;
            }
            error("unexpected-token-after-body");
            reprocess(MInBody);
        }

        // ---- 13.2.6.4.20 in frameset ----
        void in_frameset() {
            const Token& t = _t;
            if (t.type == TkCharacters) {
                // only the white space of a run is kept
                if (ws_chars()) {
                    insert_text(t.data);
                } else {
                    error("unexpected-character");
                }
                return;
            }
            if (t.type == TkComment) {
                insert_comment(t);
                return;
            }
            if (t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (start("html")) {
                in_body();
                return;
            }
            if (start("frameset")) {
                insert_element(t);
                return;
            }
            if (end("frameset")) {
                if (current() == _open[0]) {
                    error("unexpected-end-tag");
                    return;
                }
                _open.pop_back();
                if (_context < 0 && !is(current(), "frameset")) {
                    _mode = MAfterFrameset;
                }
                return;
            }
            if (start("frame")) {
                insert_element(t);
                _open.pop_back();
                return;
            }
            if (start("noframes")) {
                in_head();
                return;
            }
            if (t.type == TkEof) {
                if (current() != _open[0]) {
                    error("eof-in-frameset");
                }
                _stopped = true;
                return;
            }
            error("unexpected-token-in-frameset");
        }

        // ---- 13.2.6.4.21 after frameset ----
        void after_frameset() {
            const Token& t = _t;
            if (t.type == TkCharacters) {
                if (ws_chars()) {
                    insert_text(t.data);
                } else {
                    error("unexpected-character");
                }
                return;
            }
            if (t.type == TkComment) {
                insert_comment(t);
                return;
            }
            if (t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            if (start("html")) {
                in_body();
                return;
            }
            if (end("html")) {
                _mode = MAfterAfterFrameset;
                return;
            }
            if (start("noframes")) {
                in_head();
                return;
            }
            if (t.type == TkEof) {
                _stopped = true;
                return;
            }
            error("unexpected-token-after-frameset");
        }

        // ---- 13.2.6.4.22-23 after after body, after after frameset ----
        void after_after_body() {
            const Token& t = _t;
            if (t.type == TkComment) {
                insert_comment(t, _tree.document);
                return;
            }
            if (t.type == TkDoctype || (t.type == TkCharacters && ws_chars()) || start("html")) {
                in_body();
                return;
            }
            if (t.type == TkEof) {
                _stopped = true;
                return;
            }
            error("unexpected-token-after-body");
            reprocess(MInBody);
        }

        void after_after_frameset() {
            const Token& t = _t;
            if (t.type == TkComment) {
                insert_comment(t, _tree.document);
                return;
            }
            if (t.type == TkDoctype || (t.type == TkCharacters && ws_chars()) || start("html")) {
                in_body();
                return;
            }
            if (t.type == TkEof) {
                _stopped = true;
                return;
            }
            if (start("noframes")) {
                in_head();
                return;
            }
            error("unexpected-token-after-frameset");
        }

        // ---- 13.2.6.5 foreign content ----
        void foreign() {
            Token& t = _t;
            if (t.type == TkCharacters) {
                if (nul_chars()) {
                    error("unexpected-null-character");
                    std::string r;
                    for (size_t k = 0; k < t.data.size(); ++k) {
                        r.append("\xEF\xBF\xBD");
                    }
                    insert_text(r);
                    return;
                }
                insert_text(t.data);
                if (!ws_chars()) {
                    _frameset_ok = false;
                }
                return;
            }
            if (t.type == TkComment) {
                insert_comment(t);
                return;
            }
            if (t.type == TkDoctype) {
                error("unexpected-doctype");
                return;
            }
            bool breakout = t.type == TkStartTag
                && (one_of(t.name, {"b", "big", "blockquote", "body", "br", "center", "code", "dd", "div", "dl", "dt",
                                    "em", "embed", "h1", "h2", "h3", "h4", "h5", "h6", "head", "hr", "i", "img", "li",
                                    "listing", "menu", "meta", "nobr", "ol", "p", "pre", "ruby", "s", "small", "span",
                                    "strong", "strike", "sub", "sup", "table", "tt", "u", "ul", "var"})
                    || (t.name == "font" && (t.attr("color") || t.attr("face") || t.attr("size"))));
            if (breakout || end("br") || end("p")) {
                error("unexpected-html-element-in-foreign-content");
                while (!_open.empty()) {
                    int c = current();
                    if (is_html(c) || mathml_text_ip(c) || html_ip(c)) {
                        break;
                    }
                    _open.pop_back();
                }
                process(_mode);
                return;
            }
            if (t.type == TkStartTag) {
                int acn = adjusted_current();
                uint8_t ns = n(acn).ns;
                Token f = t;
                if (ns == NsSvg) {
                    adjust_svg_name(f.name);
                }
                insert_foreign(f, ns);
                if (t.self_closing) {
                    _open.pop_back();
                }
                return;
            }
            if (t.type == TkEndTag) {
                // the end tag of a foreign element of its name, case-insensitively
                if (t.name == "script" && is(current(), "script", NsSvg)) {
                    _open.pop_back();
                    return;
                }
                size_t k = _open.size() - 1;
                int node = _open[k];
                auto lower_eq = [&](int i) {
                    const std::string& a = n(i).name;
                    if (a.size() != t.name.size()) {
                        return false;
                    }
                    for (size_t j = 0; j < a.size(); ++j) {
                        if (ascii_lower(a[j]) != t.name[j]) {
                            return false;
                        }
                    }
                    return true;
                };
                if (!lower_eq(node)) {
                    error("unexpected-end-tag");
                }
                while (true) {
                    if (k == 0) {
                        return;
                    }
                    if (lower_eq(node)) {
                        while (_open.size() > k) {
                            _open.pop_back();
                        }
                        return;
                    }
                    --k;
                    node = _open[k];
                    if (is_html(node)) {
                        process(_mode);
                        return;
                    }
                }
            }
        }
    };
}
