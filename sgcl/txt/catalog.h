//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/dynamic_array.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../core/detail/bytes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// A catalog of translations the way GNU gettext keeps them: a .po file
// (text, what translators edit) or a .mo file (what msgfmt compiles it to),
// each the messages of one language, looked up by the program's own text —
// the message id — with an optional context, and plural forms chosen by the
// catalog's own rule, the C expression of its Plural-Forms header.
//
//     auto pl = txt::catalog::parse_po(text).value();
//     pl.translate("Open");                       // "Otwórz"
//     pl.translate("Open", "menu");               // pgettext: the one of the menu
//     pl.translate("file", "files", 5);           // ngettext: "plików"
//
// What gettext answers is what this answers, to the byte, and libintl
// is the oracle of the tests: a message with no translation comes back as
// its id (for a plural, the id for n == 1 and the plural id otherwise); a
// plural form the translation lacks gives its first; an entry marked fuzzy, or
// with an empty first form, is not a translation (msgfmt drops both); the
// rule of a .mo that is not one is the Germanic n != 1, as libintl takes it,
// while a .po whose rule does not parse is refused, as msgfmt --check
// refuses it.
//
// Read once, then only read: a catalog is a handle of one word to an
// immutable table, shared by copies and safe to read from any number of
// threads. A translation comes back as a string held by the catalog, so
// translate allocates nothing.
namespace sgcl::txt {
    class catalog;

    namespace detail {
        struct CatalogData;
        struct CatalogReader;
    }

    // Where a .po or .mo stopped being one, for whoever has to fix it. A .po
    // gives the line and the column from one; a .mo, which has no lines,
    // gives 0 for both.
    class catalog_error {
    public:
        SGCL_INLINE_HOT catalog_error(size_t offset, size_t line, size_t column, const char* reason) noexcept
        : _offset(offset)
        , _line(line)
        , _column(column)
        , _reason(reason) {
        }

        // The byte the reading stopped on
        SGCL_INLINE_HOT size_t offset() const noexcept {
            return _offset;
        }

        SGCL_INLINE_HOT size_t line() const noexcept {
            return _line;
        }

        SGCL_INLINE_HOT size_t column() const noexcept {
            return _column;
        }

        // Why, in a few words
        SGCL_INLINE_HOT string message() const noexcept {
            return string(_reason);
        }

    private:
        size_t _offset;
        size_t _line;
        size_t _column;
        const char* _reason;
    };

    namespace detail {
        //----------------------------------------------------------------
        // The plural rule: the C subset of gettext's plural.y — n, decimal
        // constants, ! * / % + - < > <= >= == != && || ?: and parentheses,
        // with C's precedence — over unsigned long, read into a tree of
        // nodes held in an array and evaluated as plural-eval.c does
        // (&& || ?: short), except that a division by zero, a trap there,
        // makes the whole rule answer form 0.
        //----------------------------------------------------------------
        struct PluralNode {
            uint8_t op;          // PluralOp
            uint32_t a, b, c;    // operands (node indices)
            uint64_t value;      // a constant
        };

        enum PluralOp : uint8_t {
            PlNum, PlVar, PlNot, PlMul, PlDiv, PlMod, PlAdd, PlSub, PlLt, PlGt, PlLe, PlGe, PlEq, PlNe,
            PlAnd, PlOr, PlIf,
        };

        struct PluralRule {
            std::vector<PluralNode> nodes;   // empty: the Germanic n != 1
            uint32_t root = 0;
            uint32_t nplurals = 2;
        };

        class PluralParser {
        public:
            explicit PluralParser(std::string_view text) noexcept
            : _s(text) {
            }

            // The expression and what follows it (';', a line end or the
            // end); false on anything else
            bool parse(PluralRule& r) noexcept {
                _nodes = &r.nodes;
                _next();
                uint32_t root;
                if (!_ternary(root, 0) || _tok != TEnd) {
                    r.nodes.clear();
                    return false;
                }
                r.root = root;
                return true;
            }

            size_t position() const noexcept {
                return _at;
            }

        private:
            enum Tok : uint8_t {
                TEnd, TBad, TNum, TVar, TNot, TMul, TDiv, TMod, TAdd, TSub, TLt, TGt, TLe, TGe, TEq, TNe,
                TAnd, TOr, TQuest, TColon, TOpen, TClose,
            };

            static constexpr uint32_t MaxDepth = 200;

            std::string_view _s;
            size_t _i = 0;
            size_t _at = 0;
            Tok _tok = TEnd;
            uint64_t _num = 0;
            std::vector<PluralNode>* _nodes = nullptr;

            void _next() noexcept {
                while (_i < _s.size() && (_s[_i] == ' ' || _s[_i] == '\t')) {
                    ++_i;
                }
                _at = _i;
                if (_i >= _s.size()) {
                    _tok = TEnd;
                    return;
                }
                char c = _s[_i++];
                char d = _i < _s.size() ? _s[_i] : '\0';
                switch (c) {
                    case '\0': case ';': case '\n':
                        --_i;
                        _tok = TEnd;
                        return;
                    case 'n': _tok = TVar; return;
                    case '*': _tok = TMul; return;
                    case '/': _tok = TDiv; return;
                    case '%': _tok = TMod; return;
                    case '+': _tok = TAdd; return;
                    case '-': _tok = TSub; return;
                    case '?': _tok = TQuest; return;
                    case ':': _tok = TColon; return;
                    case '(': _tok = TOpen; return;
                    case ')': _tok = TClose; return;
                    case '!':
                        if (d == '=') {
                            ++_i;
                            _tok = TNe;
                        } else {
                            _tok = TNot;
                        }
                        return;
                    case '=':
                        _tok = d == '=' ? (++_i, TEq) : TBad;
                        return;
                    case '<':
                        _tok = d == '=' ? (++_i, TLe) : TLt;
                        return;
                    case '>':
                        _tok = d == '=' ? (++_i, TGe) : TGt;
                        return;
                    case '&':
                        _tok = d == '&' ? (++_i, TAnd) : TBad;
                        return;
                    case '|':
                        _tok = d == '|' ? (++_i, TOr) : TBad;
                        return;
                    default:
                        break;
                }
                if (c >= '0' && c <= '9') {
                    uint64_t v = uint64_t(c - '0');
                    bool over = false;
                    while (_i < _s.size() && _s[_i] >= '0' && _s[_i] <= '9') {
                        uint64_t digit = uint64_t(_s[_i++] - '0');
                        if (v > (UINT64_MAX - digit) / 10) {
                            over = true;   // strtoul's answer: the largest
                        } else {
                            v = v * 10 + digit;
                        }
                    }
                    _num = over ? UINT64_MAX : v;
                    _tok = TNum;
                    return;
                }
                _tok = TBad;
            }

            uint32_t _add(uint8_t op, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0, uint64_t v = 0) {
                _nodes->push_back(PluralNode{op, a, b, c, v});
                return uint32_t(_nodes->size() - 1);
            }

            // exp ? exp : exp, right associative, the lowest
            bool _ternary(uint32_t& out, uint32_t depth) noexcept {
                if (depth > MaxDepth || !_binary(out, 0, depth)) {
                    return false;
                }
                if (_tok != TQuest) {
                    return true;
                }
                _next();
                uint32_t yes, no;
                if (!_ternary(yes, depth + 1) || _tok != TColon) {
                    return false;
                }
                _next();
                if (!_ternary(no, depth + 1)) {
                    return false;
                }
                out = _add(PlIf, out, yes, no);
                return true;
            }

            // the binary operators by level: || && (== !=) (< > <= >=) (+ -) (* / %)
            static int _level(Tok t) noexcept {
                switch (t) {
                    case TOr: return 0;
                    case TAnd: return 1;
                    case TEq: case TNe: return 2;
                    case TLt: case TGt: case TLe: case TGe: return 3;
                    case TAdd: case TSub: return 4;
                    case TMul: case TDiv: case TMod: return 5;
                    default: return -1;
                }
            }

            static uint8_t _op(Tok t) noexcept {
                switch (t) {
                    case TOr: return PlOr;
                    case TAnd: return PlAnd;
                    case TEq: return PlEq;
                    case TNe: return PlNe;
                    case TLt: return PlLt;
                    case TGt: return PlGt;
                    case TLe: return PlLe;
                    case TGe: return PlGe;
                    case TAdd: return PlAdd;
                    case TSub: return PlSub;
                    case TMul: return PlMul;
                    case TDiv: return PlDiv;
                    default: return PlMod;
                }
            }

            bool _binary(uint32_t& out, int level, uint32_t depth) noexcept {
                if (level > 5) {
                    return _unary(out, depth);
                }
                if (!_binary(out, level + 1, depth)) {
                    return false;
                }
                while (_level(_tok) == level) {
                    uint8_t op = _op(_tok);
                    _next();
                    uint32_t right;
                    if (!_binary(right, level + 1, depth)) {
                        return false;
                    }
                    out = _add(op, out, right);
                }
                return true;
            }

            bool _unary(uint32_t& out, uint32_t depth) noexcept {
                if (depth > MaxDepth) {
                    return false;
                }
                switch (_tok) {
                    case TNot: {
                        _next();
                        uint32_t a;
                        if (!_unary(a, depth + 1)) {
                            return false;
                        }
                        out = _add(PlNot, a);
                        return true;
                    }
                    case TVar:
                        _next();
                        out = _add(PlVar);
                        return true;
                    case TNum:
                        out = _add(PlNum, 0, 0, 0, _num);
                        _next();
                        return true;
                    case TOpen:
                        _next();
                        if (!_ternary(out, depth + 1) || _tok != TClose) {
                            return false;
                        }
                        _next();
                        return true;
                    default:
                        return false;
                }
            }
        };

        // false: a division by zero, the whole rule gives form 0
        inline bool plural_eval(const PluralNode* nodes, uint32_t i, uint64_t n, uint64_t& out) noexcept {
            const PluralNode& e = nodes[i];
            uint64_t a, b;
            switch (e.op) {
                case PlNum: out = e.value; return true;
                case PlVar: out = n; return true;
                case PlNot:
                    if (!plural_eval(nodes, e.a, n, a)) {
                        return false;
                    }
                    out = !a;
                    return true;
                case PlAnd:
                case PlOr:
                    if (!plural_eval(nodes, e.a, n, a)) {
                        return false;
                    }
                    if ((e.op == PlAnd) == !a) {
                        out = e.op == PlOr;
                        return true;
                    }
                    if (!plural_eval(nodes, e.b, n, b)) {
                        return false;
                    }
                    out = b != 0;
                    return true;
                case PlIf:
                    if (!plural_eval(nodes, e.a, n, a)) {
                        return false;
                    }
                    return plural_eval(nodes, a ? e.b : e.c, n, out);
                default:
                    break;
            }
            if (!plural_eval(nodes, e.a, n, a) || !plural_eval(nodes, e.b, n, b)) {
                return false;
            }
            switch (e.op) {
                case PlMul: out = a * b; return true;
                case PlDiv:
                    if (b == 0) {
                        return false;
                    }
                    out = a / b;
                    return true;
                case PlMod:
                    if (b == 0) {
                        return false;
                    }
                    out = a % b;
                    return true;
                case PlAdd: out = a + b; return true;
                case PlSub: out = a - b; return true;
                case PlLt: out = a < b; return true;
                case PlGt: out = a > b; return true;
                case PlLe: out = a <= b; return true;
                case PlGe: out = a >= b; return true;
                case PlEq: out = a == b; return true;
                default: out = a != b; return true;
            }
        }

        // The form of n: what libintl's plural_lookup computes before it
        // walks the translation (an index past nplurals is 0)
        inline uint32_t plural_index(const PluralRule& r, uint64_t n) noexcept {
            uint64_t v;
            if (r.nodes.empty()) {
                v = n != 1;
            } else if (!plural_eval(r.nodes.data(), r.root, n, v)) {
                v = 0;
            }
            return v >= r.nplurals ? 0 : uint32_t(v);
        }

        inline bool is_space(char c) noexcept {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
        }

        // The rule of a header, found as libintl finds it (plural-exp.c):
        // "nplurals=" and "plural=" anywhere in it, the number then the
        // expression. 0: no rule there (the Germanic one); 1: read; -1: a
        // rule that is not one, with where it stopped
        inline int read_plural_header(std::string_view header, PluralRule& r, size_t& where) noexcept {
            size_t p = header.find("plural=");
            size_t np = header.find("nplurals=");
            r = PluralRule();
            if (p == std::string_view::npos || np == std::string_view::npos) {
                return 0;
            }
            np += 9;
            while (np < header.size() && is_space(header[np])) {
                ++np;
            }
            if (np >= header.size() || header[np] < '0' || header[np] > '9') {
                where = np;
                return -1;
            }
            uint64_t count = 0;
            while (np < header.size() && header[np] >= '0' && header[np] <= '9') {
                uint64_t digit = uint64_t(header[np++] - '0');
                count = count > (UINT64_MAX - digit) / 10 ? UINT64_MAX : count * 10 + digit;
            }
            p += 7;
            PluralParser parser(header.substr(p));
            PluralRule rule;
            rule.nplurals = count > UINT32_MAX ? UINT32_MAX : uint32_t(count);
            if (!parser.parse(rule)) {
                where = p + parser.position();
                return -1;
            }
            r = std::move(rule);
            return 1;
        }

        //----------------------------------------------------------------
        // The table. Per message: its id, its context, its plural id and
        // its forms, each a string of the library (so a translation goes
        // back as a word), and the hash of id and context; an
        // open-addressing index over them, a power of two at most half
        // full.
        //----------------------------------------------------------------
        struct CatalogData {
            dynamic_array<string> ids;
            dynamic_array<string> contexts;
            dynamic_array<string> plurals;
            dynamic_array<string> forms;
            std::vector<uint32_t> first;     // per message its first form; one more at the end
            std::vector<uint64_t> hashes;
            std::vector<uint8_t> has_context;
            std::vector<uint32_t> index;     // message + 1, 0 empty
            string header;                   // the translation of "", the header entry
            bool has_header = false;
            PluralRule rule;
        };

        // A context, even an empty one (msgctxt ""), is part of the key;
        // none is not a context
        SGCL_INLINE_HOT uint64_t catalog_hash(uint64_t id_hash, std::string_view context, bool has_context) noexcept {
            if (!has_context) {
                return id_hash;
            }
            uint64_t c = string::hash_of(context);
            return id_hash ^ (c + 0x9e3779b97f4a7c15ull + (id_hash << 6) + (id_hash >> 2));
        }

        // One message as read, before the table is built
        struct CatalogEntry {
            std::string context;
            std::string id;
            std::string plural;
            std::vector<std::string> forms;
            bool has_context = false;
            bool has_plural = false;
            bool fuzzy = false;
            size_t offset = 0, line = 0, column = 0;
        };
    }

    namespace detail {
        inline expected<catalog, catalog_error> parse_po_text(std::string_view text) noexcept;
    }

    class catalog {
    public:
        // An empty catalog: every id comes back as it is (n == 1: the
        // id, otherwise the plural id), with the Germanic rule
        catalog() noexcept = default;

        // A .po file as gettext writes it, UTF-8 (the bytes are kept as
        // they are, whatever the header's charset): its messages, the
        // header entry, the rule of its Plural-Forms; refused where msgfmt
        // refuses: a string or escape that is not one, a keyword out of
        // place, a message defined twice, a plural without its forms, a
        // translation whose line feeds at either end are not its id's, a
        // Plural-Forms that is not a rule (msgfmt --check)
        static expected<catalog, catalog_error> parse_po(const string& text) noexcept;

        // A .mo file (either byte order, revision 0 or 1): its messages
        // and its header; refused when a table or a string reaches past
        // the end. A rule that is not one is the Germanic rule, as libintl
        // takes it
        static expected<catalog, catalog_error> parse_mo(const slice<const byte>& data) noexcept;

        // gettext: the translation of id, or id
        string translate(const string& id) const noexcept {
            const detail::CatalogData* d = _data.get();
            uint32_t m = d ? _find(*d, id, {}, false) : NotFound;
            return m == NotFound ? id : d->forms[d->first[m]];
        }

        // pgettext: the translation of id in a context ("menu"), or id
        string translate(const string& id, const string& context) const noexcept {
            const detail::CatalogData* d = _data.get();
            uint32_t m = d ? _find(*d, id, context.view(), true) : NotFound;
            return m == NotFound ? id : d->forms[d->first[m]];
        }

        // ngettext: the form of n of id's translation (its first when it
        // has no such form); with none, id when n is 1 and plural_id
        // otherwise
        string translate(const string& id, const string& plural_id, uint64_t n) const noexcept {
            return _plural(id, plural_id, n, {}, false);
        }

        // npgettext: the same in a context
        string translate(const string& id, const string& plural_id, uint64_t n,
                         const string& context) const noexcept {
            return _plural(id, plural_id, n, context.view(), true);
        }

        // Whether id, with no context or in a context, has a translation
        // here
        bool contains(const string& id) const noexcept {
            const detail::CatalogData* d = _data.get();
            return d && _find(*d, id, {}, false) != NotFound;
        }

        bool contains(const string& id, const string& context) const noexcept {
            const detail::CatalogData* d = _data.get();
            return d && _find(*d, id, context.view(), true) != NotFound;
        }

        // The messages with a translation, the header not counted
        size_t size() const noexcept {
            const detail::CatalogData* d = _data.get();
            return d ? d->ids.size() - (d->has_header ? 1 : 0) : 0;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return size() == 0;
        }

        // A field of the header entry ("Language", "Plural-Forms",
        // "Content-Type"), its name in any case, the value without its
        // surrounding blanks; empty when there is no such field
        string header(const string& field) const noexcept;

        // nplurals of the rule (2 without one)
        uint32_t plural_forms() const noexcept {
            const detail::CatalogData* d = _data.get();
            return d ? d->rule.nplurals : 2;
        }

        // The form the rule gives n (0 past nplurals or on a division by
        // zero)
        uint32_t plural_index(uint64_t n) const noexcept {
            const detail::CatalogData* d = _data.get();
            return d ? detail::plural_index(d->rule, n) : uint32_t(n != 1);
        }

        // The catalog as msgfmt writes it: a .mo of revision 0 in the
        // byte order of this machine, the messages sorted by id, with
        // gettext's hash table
        vector<byte> to_mo() const noexcept;

    private:
        static constexpr uint32_t NotFound = UINT32_MAX;

        tracked_ptr<const detail::CatalogData> _data;

        SGCL_INLINE_HOT explicit catalog(tracked_ptr<const detail::CatalogData> d) noexcept
        : _data(std::move(d)) {
        }

        friend struct detail::CatalogReader;

        static uint32_t _find(const detail::CatalogData& d, const string& id, std::string_view context,
                              bool has_context) noexcept {
            if (d.index.empty()) {
                return NotFound;
            }
            uint64_t h = detail::catalog_hash(id.hash(), context, has_context);
            size_t mask = d.index.size() - 1;
            for (size_t i = size_t(h) & mask;; i = (i + 1) & mask) {
                uint32_t e = d.index[i];
                if (e == 0) {
                    return NotFound;
                }
                --e;
                if (d.hashes[e] == h && d.ids[e] == id && d.has_context[e] == has_context &&
                    d.contexts[e].view() == context) {
                    return e;
                }
            }
        }

        string _plural(const string& id, const string& plural_id, uint64_t n, std::string_view context,
                       bool has_context) const noexcept {
            const detail::CatalogData* d = _data.get();
            uint32_t m = d ? _find(*d, id, context, has_context) : NotFound;
            if (m == NotFound) {
                return n == 1 ? id : plural_id;
            }
            uint32_t form = detail::plural_index(d->rule, n);
            uint32_t first = d->first[m];
            if (form >= d->first[m + 1] - first) {
                form = 0;   // libintl: a form the translation lacks is its first
            }
            return d->forms[first + form];
        }
    };

    namespace detail {
        // Reads .po and .mo into a catalog's table
        struct CatalogReader {
            static size_t table_size(size_t n) noexcept {
                size_t s = 8;
                while (s < n * 2) {
                    s *= 2;
                }
                return s;
            }

            static string to_string(const std::string& s) {
                return s.empty() ? string() : string(std::string_view(s));
            }

            // The table of entries already checked (no duplicates)
            static catalog build(std::vector<CatalogEntry>& entries, const std::string* header) {
                auto d = make_tracked<CatalogData>();
                size_t n = entries.size(), forms = 0;
                for (auto& e : entries) {
                    forms += e.forms.size();
                }
                d->ids = dynamic_array<string>(n);
                d->contexts = dynamic_array<string>(n);
                d->plurals = dynamic_array<string>(n);
                d->forms = dynamic_array<string>(forms);
                d->first.reserve(n + 1);
                d->hashes.reserve(n);
                d->has_context.reserve(n);
                d->index.assign(table_size(n), 0);
                size_t mask = d->index.size() - 1, f = 0;
                for (size_t i = 0; i < n; ++i) {
                    auto& e = entries[i];
                    d->ids[i] = to_string(e.id);
                    d->contexts[i] = to_string(e.context);
                    d->plurals[i] = to_string(e.plural);
                    d->first.push_back(uint32_t(f));
                    for (auto& s : e.forms) {
                        d->forms[f++] = to_string(s);
                    }
                    uint64_t h = catalog_hash(d->ids[i].hash(), e.context, e.has_context);
                    d->hashes.push_back(h);
                    d->has_context.push_back(e.has_context);
                    size_t j = size_t(h) & mask;
                    while (d->index[j]) {
                        j = (j + 1) & mask;
                    }
                    d->index[j] = uint32_t(i + 1);
                }
                d->first.push_back(uint32_t(f));
                if (header) {
                    d->header = to_string(*header);
                    d->has_header = true;
                }
                return catalog(tracked_ptr<const CatalogData>(std::move(d)));
            }

            static void set_rule(catalog& c, const PluralRule& r) noexcept {
                if (c._data) {
                    const_cast<CatalogData*>(c._data.get())->rule = r;
                }
            }

            static const CatalogData* data(const catalog& c) noexcept {
                return c._data.get();
            }

            static catalog empty_with_rule(const PluralRule& r) {
                auto d = make_tracked<CatalogData>();
                d->rule = r;
                return catalog(tracked_ptr<const CatalogData>(std::move(d)));
            }
        };

        //----------------------------------------------------------------
        // .po, as gettext's po-lex.c and read-catalog.c read it
        //----------------------------------------------------------------
        class PoReader {
        public:
            explicit PoReader(std::string_view text) noexcept
            : _s(text) {
                if (_s.size() >= 3 && _s.substr(0, 3) == "\xEF\xBB\xBF") {
                    _i = 3;
                }
                _line_start = _i;
            }

            optional<catalog_error> read(std::vector<CatalogEntry>& out, std::string& header, bool& has_header) {
                CatalogEntry cur;
                enum State : uint8_t { Start, Context, Id, Plural, Str } state = Start;
                bool fuzzy = false;
                // the string that continuation lines go to
                std::string* target = nullptr;
                auto finish = [&]() -> optional<catalog_error> {
                    if (state == Start) {
                        return nullopt;
                    }
                    if (state != Str) {
                        return _error(cur.offset, cur.line, cur.column, "a message without msgstr");
                    }
                    if (cur.has_plural && cur.forms.size() == 0) {
                        return _error(cur.offset, cur.line, cur.column, "msgid_plural without msgstr[0]");
                    }
                    if (cur.id.find('\x04') != std::string::npos || cur.context.find('\x04') != std::string::npos) {
                        return _error(cur.offset, cur.line, cur.column,
                                      "a byte 4 in a message id or context, which a .mo keeps for the context");
                    }
                    bool is_header = cur.id.empty() && !cur.has_context;
                    if (is_header) {
                        if (has_header) {
                            return _error(cur.offset, cur.line, cur.column, "duplicate message definition");
                        }
                        has_header = true;
                        header = cur.forms.empty() ? std::string() : cur.forms[0];
                        header_offset = cur.offset;
                        header_line = cur.line;
                        header_column = cur.column;
                    } else if (!cur.fuzzy && !cur.forms.empty() && !cur.forms[0].empty()) {
                        if (const char* why = _newlines(cur)) {
                            return _error(cur.offset, cur.line, cur.column, why);
                        }
                        out.push_back(std::move(cur));
                    } else {
                        _skipped.push_back({std::move(cur.context), std::move(cur.id), cur.has_context, cur.offset,
                                            cur.line, cur.column});
                    }
                    cur = CatalogEntry();
                    state = Start;
                    target = nullptr;
                    return nullopt;
                };
                while (_i < _s.size()) {
                    _skip_blanks();
                    if (_i >= _s.size()) {
                        break;
                    }
                    char c = _s[_i];
                    if (c == '\n' || c == '\r') {
                        _newline();
                        continue;
                    }
                    if (c == '#') {
                        char d = _i + 1 < _s.size() ? _s[_i + 1] : '\0';
                        if (d == ',') {
                            fuzzy = fuzzy || _has_fuzzy_flag();
                        }
                        _skip_line();
                        continue;
                    }
                    if (c == '"') {
                        if (!target) {
                            return _error(_i, _line, _column(), "a string without a keyword");
                        }
                        if (auto e = _string(*target)) {
                            return e;
                        }
                        continue;
                    }
                    size_t kw_at = _i, kw_col = _column();
                    std::string_view word = _word();
                    if (word == "msgctxt" || word == "msgid") {
                        if (state == Str) {
                            if (auto e = finish()) {
                                return e;
                            }
                        }
                        if (word == "msgctxt") {
                            if (state != Start) {
                                return _error(kw_at, _line, kw_col, "msgctxt out of place");
                            }
                            cur.offset = kw_at;
                            cur.line = _line;
                            cur.column = kw_col;
                            cur.has_context = true;
                            cur.fuzzy = fuzzy;
                            fuzzy = false;
                            state = Context;
                            target = &cur.context;
                        } else {
                            if (state != Start && state != Context) {
                                return _error(kw_at, _line, kw_col, "msgid out of place");
                            }
                            if (state == Start) {
                                cur.offset = kw_at;
                                cur.line = _line;
                                cur.column = kw_col;
                                cur.fuzzy = fuzzy;
                                fuzzy = false;
                            }
                            state = Id;
                            target = &cur.id;
                        }
                    } else if (word == "msgid_plural") {
                        if (state != Id) {
                            return _error(kw_at, _line, kw_col, "msgid_plural out of place");
                        }
                        cur.has_plural = true;
                        state = Plural;
                        target = &cur.plural;
                    } else if (word == "msgstr") {
                        if (state != Id && state != Plural && state != Str) {
                            return _error(kw_at, _line, kw_col, "msgstr without msgid");
                        }
                        if (_i < _s.size() && _s[_i] == '[') {
                            if (!cur.has_plural) {
                                return _error(kw_at, _line, kw_col, "msgstr[] without msgid_plural");
                            }
                            ++_i;
                            uint64_t k = 0;
                            size_t digits = 0;
                            while (_i < _s.size() && _s[_i] >= '0' && _s[_i] <= '9' && digits < 9) {
                                k = k * 10 + uint64_t(_s[_i++] - '0');
                                ++digits;
                            }
                            if (!digits || _i >= _s.size() || _s[_i] != ']') {
                                return _error(_i, _line, _column(), "a bad index of msgstr[]");
                            }
                            ++_i;
                            if (k != cur.forms.size()) {
                                return _error(kw_at, _line, kw_col, "msgstr[] indices out of order");
                            }
                        } else {
                            if (cur.has_plural) {
                                return _error(kw_at, _line, kw_col, "msgstr where msgstr[0] is expected");
                            }
                            if (state == Str) {
                                return _error(kw_at, _line, kw_col, "msgstr twice");
                            }
                        }
                        cur.forms.emplace_back();
                        state = Str;
                        target = &cur.forms.back();
                    } else {
                        return _error(kw_at, _line, kw_col, "not a keyword of a .po file");
                    }
                    _skip_blanks();
                    if (_i >= _s.size() || _s[_i] != '"') {
                        return _error(_i, _line, _column(), "a keyword without its string");
                    }
                }
                if (auto e = finish()) {
                    return e;
                }
                return nullopt;
            }

            // messages defined but not translated, to catch a definition twice
            struct Skipped {
                std::string context, id;
                bool has_context;
                size_t offset, line, column;
            };

            std::vector<Skipped> _skipped;
            size_t header_offset = 0, header_line = 0, header_column = 0;

        private:
            std::string_view _s;
            size_t _i = 0;
            size_t _line = 1;
            size_t _line_start = 0;

            size_t _column() const noexcept {
                return _i - _line_start + 1;
            }

            catalog_error _error(size_t at, size_t line, size_t column, const char* why) const noexcept {
                return catalog_error(at, line, column, why);
            }

            void _skip_blanks() noexcept {
                while (_i < _s.size() && (_s[_i] == ' ' || _s[_i] == '\t' || _s[_i] == '\f' || _s[_i] == '\v')) {
                    ++_i;
                }
            }

            void _newline() noexcept {
                if (_s[_i] == '\r' && _i + 1 < _s.size() && _s[_i + 1] == '\n') {
                    ++_i;
                }
                ++_i;
                ++_line;
                _line_start = _i;
            }

            void _skip_line() noexcept {
                while (_i < _s.size() && _s[_i] != '\n' && _s[_i] != '\r') {
                    ++_i;
                }
            }

            bool _has_fuzzy_flag() const noexcept {
                size_t e = _i;
                while (e < _s.size() && _s[e] != '\n' && _s[e] != '\r') {
                    ++e;
                }
                std::string_view flags = _s.substr(_i + 2, e - _i - 2);
                size_t p = 0;
                while (p <= flags.size()) {
                    size_t q = flags.find(',', p);
                    if (q == std::string_view::npos) {
                        q = flags.size();
                    }
                    std::string_view f = flags.substr(p, q - p);
                    while (!f.empty() && is_space(f.front())) {
                        f.remove_prefix(1);
                    }
                    while (!f.empty() && is_space(f.back())) {
                        f.remove_suffix(1);
                    }
                    if (f == "fuzzy") {
                        return true;
                    }
                    p = q + 1;
                }
                return false;
            }

            std::string_view _word() noexcept {
                size_t b = _i;
                while (_i < _s.size() && ((_s[_i] >= 'a' && _s[_i] <= 'z') || _s[_i] == '_')) {
                    ++_i;
                }
                return _s.substr(b, _i - b);
            }

            // msgfmt's check of a translation: the id, the plural id and
            // every form all begin with a line feed or none does, and the
            // same for the end
            static const char* _newlines(const CatalogEntry& e) noexcept {
                if (e.id.empty()) {
                    return nullptr;
                }
                bool begins = e.id.front() == '\n';
                bool ends = e.id.back() == '\n';
                auto begin_of = [](const std::string& t) noexcept {
                    return !t.empty() && t.front() == '\n';
                };
                auto end_of = [](const std::string& t) noexcept {
                    return !t.empty() && t.back() == '\n';
                };
                if (e.has_plural && begin_of(e.plural) != begins) {
                    return "msgid and msgid_plural do not both begin with a line feed";
                }
                for (const auto& f : e.forms) {
                    if (begin_of(f) != begins) {
                        return "msgid and msgstr do not both begin with a line feed";
                    }
                }
                if (e.has_plural && end_of(e.plural) != ends) {
                    return "msgid and msgid_plural do not both end with a line feed";
                }
                for (const auto& f : e.forms) {
                    if (end_of(f) != ends) {
                        return "msgid and msgstr do not both end with a line feed";
                    }
                }
                return nullptr;
            }

            static int _hex(char c) noexcept {
                if (c >= '0' && c <= '9') {
                    return c - '0';
                }
                if (c >= 'a' && c <= 'f') {
                    return c - 'a' + 10;
                }
                if (c >= 'A' && c <= 'F') {
                    return c - 'A' + 10;
                }
                return -1;
            }

            // "...", C's escapes, appended to out
            optional<catalog_error> _string(std::string& out) {
                size_t start = _i, col = _column();
                ++_i;
                size_t run = _i;
                while (true) {
                    if (_i >= _s.size() || _s[_i] == '\n' || _s[_i] == '\r') {
                        return _error(start, _line, col, "a string without its closing quote");
                    }
                    char c = _s[_i];
                    if (c == '"') {
                        out.append(_s.data() + run, _i - run);
                        ++_i;
                        break;
                    }
                    if (c == '\0') {
                        return _error(_i, _line, _column(), "a NUL byte in a string");
                    }
                    if (c != '\\') {
                        ++_i;
                        continue;
                    }
                    out.append(_s.data() + run, _i - run);
                    size_t esc = _i, esc_col = _column();
                    ++_i;
                    if (_i >= _s.size()) {
                        return _error(esc, _line, esc_col, "an escape at the end");
                    }
                    char e = _s[_i++];
                    switch (e) {
                        case 'n': out.push_back('\n'); break;
                        case 't': out.push_back('\t'); break;
                        case 'b': out.push_back('\b'); break;
                        case 'r': out.push_back('\r'); break;
                        case 'f': out.push_back('\f'); break;
                        case 'v': out.push_back('\v'); break;
                        case 'a': out.push_back('\a'); break;
                        case '\\': out.push_back('\\'); break;
                        case '"': out.push_back('"'); break;
                        case '\'': out.push_back('\''); break;
                        case '?': out.push_back('?'); break;
                        case 'x': {
                            int v = 0, digits = 0;
                            while (_i < _s.size() && _hex(_s[_i]) >= 0) {
                                v = (v * 16 + _hex(_s[_i++])) & 0xFF;
                                ++digits;
                            }
                            if (!digits) {
                                return _error(esc, _line, esc_col, "\\x without hex digits");
                            }
                            if (v == 0) {
                                return _error(esc, _line, esc_col, "a NUL byte in a string");
                            }
                            out.push_back(char(v));
                            break;
                        }
                        default:
                            if (e >= '0' && e <= '7') {
                                int v = e - '0', digits = 1;
                                while (digits < 3 && _i < _s.size() && _s[_i] >= '0' && _s[_i] <= '7') {
                                    v = v * 8 + (_s[_i++] - '0');
                                    ++digits;
                                }
                                if ((v & 0xFF) == 0) {
                                    return _error(esc, _line, esc_col, "a NUL byte in a string");
                                }
                                out.push_back(char(v & 0xFF));
                                break;
                            }
                            return _error(esc, _line, esc_col, "an escape that C does not have");
                    }
                    run = _i;
                }
                return nullopt;
            }
        };

        inline uint32_t mo_u32(const unsigned char* p, bool swap) noexcept {
            uint32_t v = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
            return swap ? (v >> 24 | (v >> 8 & 0xFF00) | (v << 8 & 0xFF0000) | v << 24) : v;
        }

        // gettext's hashpjw over a key (to its first NUL)
        inline uint32_t mo_hash(std::string_view s) noexcept {
            uint32_t h = 0;
            for (char ch : s) {
                if (ch == '\0') {
                    break;
                }
                h = (h << 4) + uint8_t(ch);
                uint32_t g = h & 0xF0000000u;
                if (g) {
                    h ^= g >> 24;
                    h ^= g;
                }
            }
            return h;
        }

        inline bool mo_is_prime(uint32_t n) noexcept {
            if (n < 4) {
                return n > 1;
            }
            if (n % 2 == 0) {
                return false;
            }
            for (uint32_t d = 3; uint64_t(d) * d <= n; d += 2) {
                if (n % d == 0) {
                    return false;
                }
            }
            return true;
        }
    }

    inline expected<catalog, catalog_error> catalog::parse_po(const string& text) noexcept {
        return detail::parse_po_text(text.view());
    }

    // a .po where it lies (a fuzzer's buffer): what parse_po reads
    inline expected<catalog, catalog_error> detail::parse_po_text(std::string_view text) noexcept {
        std::vector<detail::CatalogEntry> entries;
        std::string header;
        bool has_header = false;
        detail::PoReader reader(text);
        if (auto e = reader.read(entries, header, has_header)) {
            return unexpected<catalog_error>(*e);
        }
        // a message defined twice, translated or not
        struct Key {
            std::string_view context, id;
            bool has_context;
            size_t offset, line, column;
        };
        std::vector<Key> keys;
        keys.reserve(entries.size() + reader._skipped.size());
        for (auto& e : entries) {
            keys.push_back({e.context, e.id, e.has_context, e.offset, e.line, e.column});
        }
        for (auto& s : reader._skipped) {
            keys.push_back({s.context, s.id, s.has_context, s.offset, s.line, s.column});
        }
        auto less = [](const Key& a, const Key& b) noexcept {
            if (a.has_context != b.has_context) {
                return a.has_context < b.has_context;
            }
            if (a.context != b.context) {
                return a.context < b.context;
            }
            return a.id < b.id;
        };
        std::sort(keys.begin(), keys.end(), less);
        for (size_t i = 1; i < keys.size(); ++i) {
            if (!less(keys[i - 1], keys[i])) {
                // the later of the two, as msgfmt names it
                const Key& k = keys[i - 1].offset > keys[i].offset ? keys[i - 1] : keys[i];
                return unexpected<catalog_error>(catalog_error(k.offset, k.line, k.column,
                                                               "duplicate message definition"));
            }
        }
        detail::PluralRule rule;
        if (has_header) {
            size_t where = 0;
            if (detail::read_plural_header(header, rule, where) < 0) {
                return unexpected<catalog_error>(catalog_error(reader.header_offset, reader.header_line,
                                                               reader.header_column,
                                                               "Plural-Forms is not a plural rule"));
            }
            entries.insert(entries.begin(), detail::CatalogEntry());
            entries.front().forms.push_back(header);
        }
        if (entries.empty()) {
            return detail::CatalogReader::empty_with_rule(rule);
        }
        catalog c = detail::CatalogReader::build(entries, has_header ? &header : nullptr);
        detail::CatalogReader::set_rule(c, rule);
        return c;
    }

    inline expected<catalog, catalog_error> catalog::parse_mo(const slice<const byte>& data) noexcept {
        const auto* p = reinterpret_cast<const unsigned char*>(data.data());
        size_t size = data.size();
        auto fail = [](size_t at, const char* why) {
            return unexpected<catalog_error>(catalog_error(at, 0, 0, why));
        };
        if (size < 20) {
            return fail(0, "too short for a .mo header");
        }
        uint32_t magic = detail::mo_u32(p, false);
        bool swap;
        if (magic == 0x950412deu) {
            swap = false;
        } else if (magic == 0xde120495u) {
            swap = true;
        } else {
            return fail(0, "not a .mo file: a bad magic number");
        }
        uint32_t revision = detail::mo_u32(p + 4, swap);
        if ((revision >> 16) > 1) {
            return fail(4, "a .mo revision this reader does not know");
        }
        uint32_t n = detail::mo_u32(p + 8, swap);
        uint32_t orig = detail::mo_u32(p + 12, swap);
        uint32_t trans = detail::mo_u32(p + 16, swap);
        if (uint64_t(orig) + uint64_t(n) * 8 > size || uint64_t(trans) + uint64_t(n) * 8 > size) {
            return fail(8, "a string table past the end");
        }
        auto string_at = [&](uint32_t table, uint32_t i, std::string_view& out) -> bool {
            uint32_t len = detail::mo_u32(p + table + size_t(i) * 8, swap);
            uint32_t off = detail::mo_u32(p + table + size_t(i) * 8 + 4, swap);
            if (uint64_t(off) + len >= size || p[size_t(off) + len] != 0) {
                return false;   // the string and its NUL inside the file
            }
            out = std::string_view(reinterpret_cast<const char*>(p) + off, len);
            return true;
        };
        std::vector<detail::CatalogEntry> entries;
        entries.reserve(n);
        std::string header;
        bool has_header = false;
        for (uint32_t i = 0; i < n; ++i) {
            std::string_view key, value;
            if (!string_at(orig, i, key)) {
                return fail(orig + size_t(i) * 8, "a message id past the end");
            }
            if (!string_at(trans, i, value)) {
                return fail(trans + size_t(i) * 8, "a translation past the end");
            }
            detail::CatalogEntry e;
            size_t nul = key.find('\0');
            std::string_view id = key.substr(0, nul);
            if (nul != std::string_view::npos) {
                e.plural.assign(key.substr(nul + 1));
                e.has_plural = true;
            }
            size_t eot = id.find('\x04');
            if (eot != std::string_view::npos) {
                e.context.assign(id.substr(0, eot));
                e.has_context = true;
                id = id.substr(eot + 1);
            }
            e.id.assign(id);
            size_t b = 0;
            while (true) {
                size_t z = value.find('\0', b);
                if (z == std::string_view::npos) {
                    e.forms.emplace_back(value.substr(b));
                    break;
                }
                e.forms.emplace_back(value.substr(b, z - b));
                b = z + 1;
            }
            if (!e.has_context && e.id.empty()) {
                if (has_header) {
                    continue;
                }
                header.assign(value);
                has_header = true;
            }
            entries.push_back(std::move(e));
        }
        // a key twice (a file no msgfmt wrote): the first is kept, as a
        // binary search would find either
        std::vector<uint32_t> order(entries.size());
        for (uint32_t i = 0; i < order.size(); ++i) {
            order[i] = i;
        }
        auto key_less = [&](uint32_t a, uint32_t b) noexcept {
            const auto& x = entries[a];
            const auto& y = entries[b];
            if (x.has_context != y.has_context) {
                return x.has_context < y.has_context;
            }
            if (x.context != y.context) {
                return x.context < y.context;
            }
            return x.id < y.id;
        };
        std::stable_sort(order.begin(), order.end(), key_less);
        std::vector<bool> drop(entries.size(), false);
        bool any = false;
        for (size_t i = 1; i < order.size(); ++i) {
            if (!key_less(order[i - 1], order[i])) {
                drop[order[i]] = true;
                any = true;
            }
        }
        if (any) {
            std::vector<detail::CatalogEntry> kept;
            for (size_t i = 0; i < entries.size(); ++i) {
                if (!drop[i]) {
                    kept.push_back(std::move(entries[i]));
                }
            }
            entries.swap(kept);
        }
        detail::PluralRule rule;
        if (has_header) {
            size_t where = 0;
            if (detail::read_plural_header(header, rule, where) < 0) {
                rule = detail::PluralRule();   // libintl: the Germanic rule
            }
        }
        if (entries.empty()) {
            return detail::CatalogReader::empty_with_rule(rule);
        }
        catalog c = detail::CatalogReader::build(entries, has_header ? &header : nullptr);
        detail::CatalogReader::set_rule(c, rule);
        return c;
    }

    inline string catalog::header(const string& field) const noexcept {
        const detail::CatalogData* d = _data.get();
        if (!d || !d->has_header || field.empty()) {
            return string();
        }
        std::string_view h = d->header.view(), f = field.view();
        size_t b = 0;
        while (b < h.size()) {
            size_t e = h.find('\n', b);
            if (e == std::string_view::npos) {
                e = h.size();
            }
            std::string_view line = h.substr(b, e - b);
            b = e + 1;
            size_t colon = line.find(':');
            if (colon == std::string_view::npos) {
                continue;
            }
            std::string_view name = line.substr(0, colon);
            while (!name.empty() && detail::is_space(name.back())) {
                name.remove_suffix(1);
            }
            if (name.size() != f.size()) {
                continue;
            }
            bool same = true;
            for (size_t i = 0; i < f.size() && same; ++i) {
                char x = name[i], y = f[i];
                x = (x >= 'A' && x <= 'Z') ? char(x + 32) : x;
                y = (y >= 'A' && y <= 'Z') ? char(y + 32) : y;
                same = x == y;
            }
            if (!same) {
                continue;
            }
            std::string_view v = line.substr(colon + 1);
            while (!v.empty() && detail::is_space(v.front())) {
                v.remove_prefix(1);
            }
            while (!v.empty() && detail::is_space(v.back())) {
                v.remove_suffix(1);
            }
            return string(v);
        }
        return string();
    }

    inline vector<byte> catalog::to_mo() const noexcept {
        const detail::CatalogData* d = _data.get();
        size_t n = d ? d->ids.size() : 0;
        // the keys as msgfmt writes them: context EOT id, then NUL and the
        // plural id; sorted as strcmp sorts them (to the first NUL)
        std::vector<std::string> keys(n), values(n);
        for (size_t i = 0; i < n; ++i) {
            std::string& k = keys[i];
            if (d->has_context[i]) {
                k.append(d->contexts[i].view());
                k.push_back('\x04');
            }
            k.append(d->ids[i].view());
            if (!d->plurals[i].empty()) {
                k.push_back('\0');
                k.append(d->plurals[i].view());
            }
            for (uint32_t f = d->first[i]; f < d->first[i + 1]; ++f) {
                if (f > d->first[i]) {
                    values[i].push_back('\0');
                }
                values[i].append(d->forms[f].view());
            }
        }
        std::vector<uint32_t> order(n);
        for (uint32_t i = 0; i < n; ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
            return std::string_view(keys[a].c_str()) < std::string_view(keys[b].c_str());
        });
        uint32_t hash_size = n ? uint32_t(n * 4 / 3) : 0;
        if (n) {
            while (!detail::mo_is_prime(hash_size | 1)) {
                hash_size = (hash_size | 1) + 2;
            }
            hash_size |= 1;
            if (hash_size <= 2) {
                hash_size = 3;
            }
        }
        size_t orig = 28, trans = orig + n * 8, hash = trans + n * 8;
        size_t strings = hash + size_t(hash_size) * 4;
        size_t total = strings;
        for (size_t i = 0; i < n; ++i) {
            total += keys[i].size() + 1 + values[i].size() + 1;
        }
        vector<byte> out(total, byte(0));
        auto put = [&](size_t at, uint32_t v) {
            for (int k = 0; k < 4; ++k) {
                if constexpr (std::endian::native == std::endian::little) {
                    out[at + k] = byte(v >> (8 * k));
                } else {
                    out[at + k] = byte(v >> (8 * (3 - k)));
                }
            }
        };
        put(0, 0x950412deu);
        put(4, 0);
        put(8, uint32_t(n));
        put(12, uint32_t(orig));
        put(16, uint32_t(trans));
        put(20, hash_size);
        put(24, uint32_t(hash));
        size_t at = strings;
        for (size_t j = 0; j < n; ++j) {
            const std::string& k = keys[order[j]];
            put(orig + j * 8, uint32_t(k.size()));
            put(orig + j * 8 + 4, uint32_t(at));
            if (!k.empty()) {
                sgcl::detail::copy_bytes(&out[at], k.data(), k.size());
            }
            at += k.size() + 1;
        }
        for (size_t j = 0; j < n; ++j) {
            const std::string& v = values[order[j]];
            put(trans + j * 8, uint32_t(v.size()));
            put(trans + j * 8 + 4, uint32_t(at));
            if (!v.empty()) {
                sgcl::detail::copy_bytes(&out[at], v.data(), v.size());
            }
            at += v.size() + 1;
        }
        if (hash_size) {
            std::vector<uint32_t> table(hash_size, 0);
            for (size_t j = 0; j < n; ++j) {
                uint32_t h = detail::mo_hash(keys[order[j]]);
                uint32_t idx = h % hash_size;
                if (table[idx]) {
                    uint32_t incr = 1 + h % (hash_size - 2);
                    do {
                        idx = idx >= hash_size - incr ? idx - (hash_size - incr) : idx + incr;
                    } while (table[idx]);
                }
                table[idx] = uint32_t(j + 1);
            }
            for (uint32_t j = 0; j < hash_size; ++j) {
                put(hash + size_t(j) * 4, table[j]);
            }
        }
        return out;
    }
}
