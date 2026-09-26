//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/utf8.h"
#include "../properties.h"
#include "regex_tables.h"

#include <string_view>
#include <vector>

// The pattern read into a tree. Everything here is constexpr and holds no
// managed object, so the same parser runs twice: once where the program is
// compiled, over a literal, only to say whether the pattern is well formed,
// and once where it runs, to build something. That is what format.h does
// with its pattern, and it is the reason a bad literal is a message from
// the compiler rather than an empty optional at the customer's.
//
// What the parser refuses is as much of the design as what it accepts. A
// backreference and a lookaround are not missing features: an engine that
// walks the text once, carrying every alternative at the same time, cannot
// have either, and the whole point of walking it once is that the time is
// linear in the length of the text. So they are rejected by name, with the
// reason, rather than quietly parsed and then failed on.
namespace sgcl::txt::detail {
    // What a pattern may not exceed. None of these is a limit of the
    // algorithm; they are the line past which a pattern is more likely a
    // mistake or an attack than a question, and they keep the parser's
    // own recursion and the program's arrays bounded.
    inline constexpr size_t MaxRegexDepth = 200;         // ( and [ nested
    inline constexpr size_t MaxRegexRepeat = 1000;       // the n and m of {n,m}
    inline constexpr size_t MaxRegexGroups = 250;        // the ( ) that capture
    inline constexpr size_t MaxRegexInsts = 20000;       // the program, after {n,m} is spelled out

    // Turns of a spelled-out repetition, counted whether or not the turn
    // wrote anything. MaxRegexInsts cannot stand in for this: the ceiling is
    // raised where an instruction is written, so a body that writes none —
    // (?:), (?i:), a group holding nothing but a flag — never reaches it and
    // its turns multiply through the nesting unchecked. Nothing that fits in
    // MaxRegexInsts comes anywhere near this number: a turn that writes at
    // least one instruction can happen 20000 times at most, and the rest is
    // room for the nesting.
    inline constexpr size_t MaxRegexExpansion = 100000;

    enum class regex_fault : uint8_t {
        none,
        unclosed_group,
        unopened_group,
        unclosed_class,
        empty_class,
        trailing_backslash,
        unknown_escape,
        backreference,
        lookaround,
        atomic_group,
        conditional,
        possessive,
        nothing_to_repeat,
        double_repeat,
        bad_repeat,
        repeat_too_large,
        bad_class_range,
        boundary_in_class,
        bad_property,
        unknown_property,
        bad_escape_value,
        bad_group_name,
        duplicate_group_name,
        bad_flag,
        too_deep,
        too_many_groups,
        too_large,
    };

    // One sentence a person can act on. The position is added by whoever
    // reports it, since it is the same sentence wherever it happens.
    constexpr const char* text_of(regex_fault f) noexcept {
        switch (f) {
        case regex_fault::none:                 return "";
        case regex_fault::unclosed_group:       return "a '(' with no ')' after it";
        case regex_fault::unopened_group:       return "a ')' with no '(' before it";
        case regex_fault::unclosed_class:       return "a '[' with no ']' after it";
        case regex_fault::empty_class:          return "an empty class: a ']' of its own is a literal only when it is written '\\]'";
        case regex_fault::trailing_backslash:   return "a '\\' at the end of the pattern, with nothing to escape";
        case regex_fault::unknown_escape:       return "an escape this engine does not know";
        case regex_fault::backreference:        return "a backreference: this engine matches in time linear in the length of the text, carrying every alternative at once, and nothing in it can be asked to repeat what another part matched";
        case regex_fault::lookaround:           return "a lookahead or a lookbehind: this engine matches in time linear in the length of the text, in one pass, and a lookaround cannot be had that way";
        case regex_fault::atomic_group:         return "an atomic group: it exists to cut a backtracking engine short, and there is no backtracking here to cut";
        case regex_fault::conditional:          return "a conditional group: it asks whether another group matched, which is a backreference by another name, and a backreference is what an engine of linear time cannot have";
        case regex_fault::possessive:           return "a possessive quantifier: it exists to cut a backtracking engine short, and there is no backtracking here to cut";
        case regex_fault::nothing_to_repeat:    return "a quantifier with nothing before it to repeat";
        case regex_fault::double_repeat:        return "a quantifier on a quantifier";
        case regex_fault::bad_repeat:           return "a '{' that is not a count: write '\\{' for a literal brace";
        case regex_fault::repeat_too_large:     return "a repetition count past the engine's limit";
        case regex_fault::bad_class_range:      return "a range inside a class whose end comes before its start";
        case regex_fault::boundary_in_class:    return "'\\b' inside a class: a word boundary is a place, not a character";
        case regex_fault::bad_property:         return "a '\\p' without a '{name}' after it";
        case regex_fault::unknown_property:     return "a property no general category and no script goes by";
        case regex_fault::bad_escape_value:     return "an escape whose digits are not a code point";
        case regex_fault::bad_group_name:       return "a group name that is not a letter followed by letters, digits and underscores";
        case regex_fault::duplicate_group_name: return "two groups of the same name";
        case regex_fault::bad_flag:             return "a flag this engine does not know: only 'i', 'm' and 's'";
        case regex_fault::too_deep:             return "a pattern nested deeper than the engine allows";
        case regex_fault::too_many_groups:      return "more capturing groups than the engine allows";
        case regex_fault::too_large:            return "a pattern that spells out to more instructions than the engine allows";
        }
        return "";
    }

    // The faults that are not mistakes of spelling but of kind: what the
    // pattern asked for cannot exist in an engine of this shape. They get
    // one line of their own where the compiler reports a bad literal,
    // because that is the one a reader needs to be told the reason for.
    constexpr bool asks_for_backtracking(regex_fault f) noexcept {
        return f == regex_fault::backreference || f == regex_fault::lookaround
            || f == regex_fault::atomic_group || f == regex_fault::conditional
            || f == regex_fault::possessive;
    }

    struct regex_fault_at {
        regex_fault fault = regex_fault::none;
        size_t at = 0;

        constexpr explicit operator bool() const noexcept {
            return fault != regex_fault::none;
        }
    };

    //--------------------------------------------------------------------
    // A class of code points: [a-z], \d, \p{Lu}, and the negations.
    //
    // Not a set of ranges. \p{L} as ranges is some 700 of them and \p{Lo}
    // alone would cost more to spell out than the two-stage table already
    // in properties.h answers in two reads; walking 0..10FFFF to build one
    // would cost milliseconds at every compile. So a class stays a short
    // list of questions — a range, a set of general categories, a script,
    // a word character, a space — and asks them in order. The list is one
    // or two items long in nearly every pattern.
    //
    // What makes that cheap is the bitmap: the answer for the 128 ASCII
    // code points is worked out once, with the negation and the folding
    // already in it, so the common case is a shift and a test and the
    // items are only reached above U+007F.
    enum class class_kind : uint8_t {
        range,        // lo..hi
        categories,   // a bit per general category
        script,       // one script
        word,         // a letter, a number or '_' — what \w is and what a \b boundary is drawn around
        space,        // White_Space, which core's unicode answers
    };

    struct class_item {
        class_kind kind = class_kind::range;
        bool negate = false;      // \P{...}, \D, \W, \S: this item alone, not the class
        char32_t lo = 0;
        char32_t hi = 0;
        uint32_t categories = 0;  // a bit per value of enum category
        uint16_t script = 0;
    };

    // The mask of every category whose two-letter code begins with the
    // given letter. Cn (unassigned) is zero in the enum and belongs to C,
    // which is why this is a loop over the names and not a pair of bounds.
    constexpr uint32_t categories_of_group(char letter) noexcept {
        uint32_t mask = 0;
        for (size_t i = 0; i < std::size(CategoryNames); ++i) {
            if (CategoryNames[i][0] == letter) {
                mask |= uint32_t(1) << i;
            }
        }
        return mask;
    }

    constexpr bool is_word_point(char32_t c) noexcept {
        if (c < 0x80) {
            return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z')
                || (c >= U'0' && c <= U'9') || c == U'_';
        }
        auto k = category_of_fn(c);
        return (k >= category::uppercase_letter && k <= category::other_letter)
            || (k >= category::decimal_number && k <= category::other_number);
    }

    constexpr bool item_holds(const class_item& it, char32_t c) noexcept {
        bool in = false;
        switch (it.kind) {
        case class_kind::range:
            return c >= it.lo && c <= it.hi;   // a range is never negated on its own
        case class_kind::categories:
            in = (it.categories >> uint32_t(category_of_fn(c))) & 1;
            break;
        case class_kind::script:
            in = uint16_t(script_of_fn(c)) == it.script;
            break;
        case class_kind::word:
            in = is_word_point(c);
            break;
        case class_kind::space:
            in = unicode::is_space(c);
            break;
        }
        return in != it.negate;
    }

    struct char_class {
        std::vector<class_item> items;
        bool negated = false;    // the '^' of [^...]
        bool fold = false;       // under (?i): the other case of c is asked too
        uint64_t ascii[2] = {};  // the finished answer for 0..127, negation and folding included

        // Whether c is in the class, before the bitmap is built (the
        // bitmap is built out of this)
        constexpr bool slow_holds(char32_t c) const noexcept {
            char32_t lower = fold ? unicode::to_lower(c) : c;
            char32_t upper = fold ? unicode::to_upper(c) : c;
            for (const auto& it : items) {
                if (item_holds(it, c)
                    || (lower != c && item_holds(it, lower))
                    || (upper != c && item_holds(it, upper))) {
                    return !negated;
                }
            }
            return negated;
        }

        constexpr void seal() noexcept {
            for (char32_t c = 0; c < 128; ++c) {
                if (slow_holds(c)) {
                    ascii[c >> 6] |= uint64_t(1) << (c & 63);
                }
            }
        }

        constexpr bool holds(char32_t c) const noexcept {
            if (c < 128) {
                return (ascii[c >> 6] >> (c & 63)) & 1;
            }
            return slow_holds(c);
        }

        // Whether anything above U+007F can be in it. A pattern of plain
        // ASCII — which most are — then needs no byte above the bitmap
        // looked at when the engine decides where a match could begin.
        constexpr bool may_hold_non_ascii() const noexcept {
            if (negated || fold) {
                // folding reaches out of ASCII: [a-z] under (?i) holds
                // the Kelvin sign, whose lower case is 'k'
                return true;
            }
            for (const auto& it : items) {
                if (it.kind != class_kind::range || it.hi >= 128 || it.negate) {
                    return true;
                }
            }
            return false;
        }
    };

    //--------------------------------------------------------------------
    // The tree. A node holds its children by index into one arena, so the
    // whole tree is two vectors and copying it is copying them.
    enum class node_kind : uint8_t {
        empty,
        literal,
        any,          // '.'
        klass,
        concat,
        alternate,
        repeat,
        capture,
        assertion,
    };

    enum class assertion_kind : uint8_t {
        begin_text,   // \A, and ^ without (?m)
        end_text,     // \z, and $ without (?m)
        begin_line,   // ^ under (?m)
        end_line,     // $ under (?m)
        word,         // \b
        not_word,     // \B
    };

    inline constexpr uint32_t Unbounded = uint32_t(-1);
    inline constexpr uint32_t NoNode = uint32_t(-1);

    struct node {
        node_kind kind = node_kind::empty;
        bool fold = false;         // literal: compared by simple case folding
        bool greedy = true;        // repeat
        bool newline = false;      // any: whether '.' takes a line feed too
        assertion_kind assertion = assertion_kind::begin_text;
        char32_t literal = 0;
        uint32_t klass = 0;        // index into the classes
        uint32_t group = 0;        // capture: its number, 1 upwards
        uint32_t min = 0;
        uint32_t max = 0;
        std::vector<uint32_t> kids;
    };

    // A name is kept as where it stands in the pattern rather than as a
    // copy of it: a compiled regex holds the pattern's text anyway, and a
    // std::string_view into a managed string would be a piece of text
    // with no claim on the object it lives in, which is the one thing
    // this library's types exist not to be.
    struct group_name {
        uint32_t at = 0;
        uint32_t size = 0;
        uint32_t group = 0;
    };

    // The three flags, as a pattern may turn them on and off inside
    // itself: (?i), (?-i), (?ims), (?i:...). Nothing of this reaches the
    // matcher — a flag is spent where the program is built, on which
    // instruction is written, and never read again while the text is
    // walked.
    struct regex_flags {
        bool fold = false;        // i: without regard to case
        bool multiline = false;   // m: ^ and $ are the edges of a line
        bool dot_all = false;     // s: '.' takes a line feed too
    };

    struct regex_tree {
        std::vector<node> nodes;
        std::vector<char_class> classes;
        std::vector<group_name> names;
        uint32_t root = NoNode;
        uint32_t groups = 0;
    };

    // The parser. One pass, recursive descent, no allocation per
    // character, and every path out of it is a value: nothing here throws,
    // which is what lets the whole of it run where the program is
    // compiled.
    class regex_parser {
    public:
        constexpr explicit regex_parser(std::string_view pattern) noexcept
        : _text(pattern) {
        }

        constexpr regex_fault_at parse(regex_tree& out) {
            regex_flags flags;
            uint32_t root = _alternate(flags, 0);
            if (_bad()) {
                return {_fault, _fault_at};
            }
            if (_at < _text.size()) {
                // the only way out of _alternate with text left
                return _fail(regex_fault::unopened_group, _at);
            }
            for (auto& c : _classes) {
                c.seal();
            }
            out.nodes = std::move(_nodes);
            out.classes = std::move(_classes);
            out.names = std::move(_names);
            out.root = root;
            out.groups = _groups;
            return {};
        }

    private:
        constexpr bool _bad() const noexcept {
            return _fault != regex_fault::none;
        }

        constexpr regex_fault_at _fail(regex_fault f, size_t at) noexcept {
            if (!_bad()) {
                _fault = f;
                _fault_at = at;
            }
            return {_fault, _fault_at};
        }

        constexpr uint32_t _add(node n) {
            _nodes.push_back(std::move(n));
            return uint32_t(_nodes.size() - 1);
        }

        constexpr bool _more() const noexcept {
            return _at < _text.size();
        }

        constexpr char _peek(size_t ahead = 0) const noexcept {
            return _at + ahead < _text.size() ? _text[_at + ahead] : '\0';
        }

        constexpr bool _take(char c) noexcept {
            if (_peek() == c) {
                ++_at;
                return true;
            }
            return false;
        }

        // a|b|c. The flags are the group's own and are shared by every
        // branch: (?i) in one of them holds for the ones after it, which
        // is what a flag set inside a group means everywhere else.
        constexpr uint32_t _alternate(regex_flags& flags, size_t depth) {
            if (depth > MaxRegexDepth) {
                _fail(regex_fault::too_deep, _at);
                return NoNode;
            }
            std::vector<uint32_t> branches;
            branches.push_back(_concat(flags, depth));
            while (!_bad() && _take('|')) {
                branches.push_back(_concat(flags, depth));
            }
            if (_bad()) {
                return NoNode;
            }
            if (branches.size() == 1) {
                return branches[0];
            }
            node n;
            n.kind = node_kind::alternate;
            n.kids = std::move(branches);
            return _add(std::move(n));
        }

        constexpr uint32_t _concat(regex_flags& flags, size_t depth) {
            std::vector<uint32_t> parts;
            while (!_bad() && _more() && _peek() != '|' && _peek() != ')') {
                uint32_t part = _repeat(flags, depth);
                if (_bad()) {
                    return NoNode;
                }
                if (part != NoNode) {
                    parts.push_back(part);
                }
            }
            if (_bad()) {
                return NoNode;
            }
            if (parts.empty()) {
                return _add(node{});
            }
            if (parts.size() == 1) {
                return parts[0];
            }
            node n;
            n.kind = node_kind::concat;
            n.kids = std::move(parts);
            return _add(std::move(n));
        }

        // An atom and the quantifier after it. A second quantifier on the
        // same atom is refused rather than read: a*+ is a possessive star
        // in Perl and a repeated one here, and guessing which the writer
        // meant would be worse than saying so.
        constexpr uint32_t _repeat(regex_flags& flags, size_t depth) {
            uint32_t atom = _atom(flags, depth);
            if (_bad()) {
                return NoNode;
            }
            if (atom == NoNode) {
                return NoNode;   // a flag-setting group: (?i) is not an atom
            }
            bool quantified = false;
            while (_more()) {
                uint32_t min = 0;
                uint32_t max = 0;
                char c = _peek();
                size_t mark = _at;
                if (c == '*') {
                    ++_at;
                    min = 0;
                    max = Unbounded;
                } else if (c == '+') {
                    ++_at;
                    min = 1;
                    max = Unbounded;
                } else if (c == '?') {
                    ++_at;
                    min = 0;
                    max = 1;
                } else if (c == '{' && _counted(min, max)) {
                    // _counted has moved past it, or left _at alone and
                    // said no, in which case '{' is an ordinary character
                    if (_bad()) {
                        return NoNode;
                    }
                } else {
                    break;
                }
                if (quantified) {
                    _fail(regex_fault::double_repeat, mark);
                    return NoNode;
                }
                if (_nodes[atom].kind == node_kind::assertion) {
                    // ^* is not a repetition of anything a text has
                    _fail(regex_fault::nothing_to_repeat, mark);
                    return NoNode;
                }
                quantified = true;
                node n;
                n.kind = node_kind::repeat;
                n.min = min;
                n.max = max;
                n.greedy = true;
                if (_peek() == '?') {
                    ++_at;
                    n.greedy = false;
                } else if (_peek() == '+') {
                    _fail(regex_fault::possessive, _at);
                    return NoNode;
                }
                n.kids.push_back(atom);
                atom = _add(std::move(n));
            }
            return atom;
        }

        // {n}, {n,}, {n,m}. Anything else leaves _at where it was and
        // says no, so that a '{' nobody meant as a count is a character:
        // "a{b}" is three of them, as it is in RE2 and in Python.
        constexpr bool _counted(uint32_t& min, uint32_t& max) {
            size_t save = _at;
            size_t at = _at + 1;
            auto number = [&](uint32_t& out) {
                size_t digits = 0;
                uint64_t v = 0;
                while (at < _text.size() && _text[at] >= '0' && _text[at] <= '9') {
                    v = v * 10 + uint64_t(_text[at] - '0');
                    if (v > MaxRegexRepeat + 1) {
                        v = MaxRegexRepeat + 1;   // held down, so that {99999999999} does not wrap
                    }
                    ++at;
                    ++digits;
                }
                out = uint32_t(v);
                return digits != 0;
            };
            if (!number(min)) {
                return false;
            }
            if (at < _text.size() && _text[at] == ',') {
                ++at;
                if (at < _text.size() && _text[at] == '}') {
                    max = Unbounded;
                } else if (!number(max)) {
                    return false;
                }
            } else {
                max = min;
            }
            if (at >= _text.size() || _text[at] != '}') {
                return false;
            }
            _at = at + 1;
            if (min > MaxRegexRepeat || (max != Unbounded && max > MaxRegexRepeat)) {
                _fail(regex_fault::repeat_too_large, save);
            } else if (max != Unbounded && max < min) {
                _fail(regex_fault::bad_repeat, save);
            }
            return true;
        }

        constexpr uint32_t _atom(regex_flags& flags, size_t depth) {
            size_t mark = _at;
            char c = _peek();
            if (c == '(') {
                return _group(flags, depth);
            }
            if (c == ')') {
                _fail(regex_fault::unopened_group, mark);
                return NoNode;
            }
            if (c == '[') {
                return _class(flags);
            }
            if (c == '*' || c == '+' || c == '?') {
                _fail(regex_fault::nothing_to_repeat, mark);
                return NoNode;
            }
            if (c == '{') {
                uint32_t min = 0, max = 0;
                size_t save = _at;
                if (_counted(min, max)) {
                    _at = save;
                    _fail(regex_fault::nothing_to_repeat, mark);
                    return NoNode;
                }
                _at = save;
            }
            if (c == '.') {
                ++_at;
                node n;
                n.kind = node_kind::any;
                n.newline = flags.dot_all;
                return _add(std::move(n));
            }
            if (c == '^') {
                ++_at;
                node n;
                n.kind = node_kind::assertion;
                n.assertion = flags.multiline ? assertion_kind::begin_line : assertion_kind::begin_text;
                return _add(std::move(n));
            }
            if (c == '$') {
                ++_at;
                node n;
                n.kind = node_kind::assertion;
                n.assertion = flags.multiline ? assertion_kind::end_line : assertion_kind::end_text;
                return _add(std::move(n));
            }
            if (c == '\\') {
                return _escape(flags, false);
            }
            auto [value, width] = utf8::decode(_text, _at);
            _at += width;
            node n;
            n.kind = node_kind::literal;
            n.literal = value;
            n.fold = flags.fold;
            return _add(std::move(n));
        }

        //----------------------------------------------------------------
        // ( ... ) and everything that begins (? .
        constexpr uint32_t _group(regex_flags& flags, size_t depth) {
            size_t open = _at;
            ++_at;   // '('
            bool capturing = true;
            uint32_t name_at = 0;
            uint32_t name_size = 0;
            regex_flags inner = flags;
            if (_take('?')) {
                char c = _peek();
                if (c == ':') {
                    ++_at;
                    capturing = false;
                } else if (c == '=' || c == '!') {
                    _fail(regex_fault::lookaround, open);
                    return NoNode;
                } else if (c == '>') {
                    _fail(regex_fault::atomic_group, open);
                    return NoNode;
                } else if (c == '(') {
                    _fail(regex_fault::conditional, open);
                    return NoNode;
                } else if (c == '<' || c == 'P') {
                    // (?<name>...) and Python's (?P<name>...) are the
                    // same thing; (?<= and (?<! are a lookbehind, and
                    // (?P= a backreference
                    size_t angle = _at + (c == 'P' ? 1 : 0);
                    if (c == 'P' && _peek(1) != '<') {
                        _fail(_peek(1) == '=' ? regex_fault::backreference : regex_fault::unknown_escape, open);
                        return NoNode;
                    }
                    char after = angle + 1 < _text.size() ? _text[angle + 1] : '\0';
                    if (after == '=' || after == '!') {
                        _fail(regex_fault::lookaround, open);
                        return NoNode;
                    }
                    _at = angle + 1;
                    if (!_group_name(name_at, name_size)) {
                        return NoNode;
                    }
                } else if (c == 'i' || c == 'm' || c == 's' || c == '-') {
                    if (!_flag_letters(inner)) {
                        return NoNode;
                    }
                    if (_take(')')) {
                        // (?i) — no group at all, only a setting that
                        // holds to the end of the one it stands in
                        flags = inner;
                        return NoNode;
                    }
                    if (!_take(':')) {
                        _fail(regex_fault::bad_flag, _at);
                        return NoNode;
                    }
                    capturing = false;
                } else {
                    _fail(regex_fault::bad_flag, _at);
                    return NoNode;
                }
            }
            uint32_t number = 0;
            if (capturing) {
                if (_groups >= MaxRegexGroups) {
                    _fail(regex_fault::too_many_groups, open);
                    return NoNode;
                }
                number = ++_groups;
                if (name_size) {
                    std::string_view name = _text.substr(name_at, name_size);
                    for (const auto& g : _names) {
                        if (_text.substr(g.at, g.size) == name) {
                            _fail(regex_fault::duplicate_group_name, open);
                            return NoNode;
                        }
                    }
                    _names.push_back(group_name{name_at, name_size, number});
                }
            }
            uint32_t body = _alternate(inner, depth + 1);
            if (_bad()) {
                return NoNode;
            }
            if (!_take(')')) {
                _fail(regex_fault::unclosed_group, open);
                return NoNode;
            }
            if (!capturing) {
                return body;
            }
            node n;
            n.kind = node_kind::capture;
            n.group = number;
            n.kids.push_back(body);
            return _add(std::move(n));
        }

        // The name of (?<name>...), the '<' already passed
        constexpr bool _group_name(uint32_t& at, uint32_t& size) {
            size_t from = _at;
            while (_more() && _peek() != '>') {
                ++_at;
            }
            if (!_more()) {
                _fail(regex_fault::bad_group_name, from);
                return false;
            }
            std::string_view name = _text.substr(from, _at - from);
            ++_at;   // '>'
            if (name.empty() || !_name_start(name[0])) {
                _fail(regex_fault::bad_group_name, from);
                return false;
            }
            for (char c : name) {
                if (!_name_start(c) && !(c >= '0' && c <= '9')) {
                    _fail(regex_fault::bad_group_name, from);
                    return false;
                }
            }
            at = uint32_t(from);
            size = uint32_t(name.size());
            return true;
        }

        static constexpr bool _name_start(char c) noexcept {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        }

        constexpr bool _flag_letters(regex_flags& flags) {
            bool on = true;
            bool any = false;
            while (_more()) {
                char c = _peek();
                if (c == '-') {
                    if (!on) {
                        _fail(regex_fault::bad_flag, _at);
                        return false;
                    }
                    on = false;
                    ++_at;
                    any = false;
                    continue;
                }
                if (c == 'i') {
                    flags.fold = on;
                } else if (c == 'm') {
                    flags.multiline = on;
                } else if (c == 's') {
                    flags.dot_all = on;
                } else {
                    break;
                }
                any = true;
                ++_at;
            }
            if (!any) {
                _fail(regex_fault::bad_flag, _at);
                return false;
            }
            return true;
        }

        //----------------------------------------------------------------
        // [ ... ]
        constexpr uint32_t _class(regex_flags& flags) {
            size_t open = _at;
            ++_at;   // '['
            char_class cc;
            cc.fold = flags.fold;
            cc.negated = _take('^');
            while (true) {
                if (!_more()) {
                    _fail(regex_fault::unclosed_class, open);
                    return NoNode;
                }
                if (_peek() == ']') {
                    ++_at;
                    break;
                }
                char32_t lo = 0;
                bool single = true;
                if (_peek() == '\\') {
                    if (!_class_escape(cc, lo, single)) {
                        return NoNode;
                    }
                } else {
                    auto [value, width] = utf8::decode(_text, _at);
                    _at += width;
                    lo = value;
                }
                if (!single) {
                    continue;   // \d and its kind: a whole item, not the end of a range
                }
                // a-z, but a trailing '-' before the ']' is a character
                if (_peek() == '-' && _peek(1) != ']' && _at + 1 < _text.size()) {
                    size_t dash = _at;
                    ++_at;
                    char32_t hi = 0;
                    bool hi_single = true;
                    if (_peek() == '\\') {
                        char_class scratch;
                        if (!_class_escape(scratch, hi, hi_single)) {
                            return NoNode;
                        }
                        if (!hi_single) {
                            // [a-\d] asks for a range ending in a class
                            _fail(regex_fault::bad_class_range, dash);
                            return NoNode;
                        }
                    } else {
                        auto [value, width] = utf8::decode(_text, _at);
                        _at += width;
                        hi = value;
                    }
                    if (hi < lo) {
                        _fail(regex_fault::bad_class_range, dash);
                        return NoNode;
                    }
                    class_item it;
                    it.kind = class_kind::range;
                    it.lo = lo;
                    it.hi = hi;
                    cc.items.push_back(it);
                    continue;
                }
                class_item it;
                it.kind = class_kind::range;
                it.lo = lo;
                it.hi = lo;
                cc.items.push_back(it);
            }
            if (cc.items.empty()) {
                _fail(regex_fault::empty_class, open);
                return NoNode;
            }
            _classes.push_back(std::move(cc));
            node n;
            n.kind = node_kind::klass;
            n.klass = uint32_t(_classes.size() - 1);
            return _add(std::move(n));
        }

        // An escape inside a class. Either one character (single stays
        // true and comes back in `value`) or a whole item pushed into cc.
        constexpr bool _class_escape(char_class& cc, char32_t& value, bool& single) {
            size_t mark = _at;
            ++_at;   // '\'
            if (!_more()) {
                _fail(regex_fault::trailing_backslash, mark);
                return false;
            }
            char c = _peek();
            if (c == 'b' || c == 'B') {
                _fail(regex_fault::boundary_in_class, mark);
                return false;
            }
            class_item it;
            if (_shorthand(it)) {
                cc.items.push_back(it);
                single = false;
                return true;
            }
            if (_bad()) {
                return false;
            }
            return _char_escape(value);
        }

        //----------------------------------------------------------------
        // \ outside a class
        constexpr uint32_t _escape(regex_flags& flags, bool) {
            size_t mark = _at;
            ++_at;   // '\'
            if (!_more()) {
                _fail(regex_fault::trailing_backslash, mark);
                return NoNode;
            }
            char c = _peek();
            if (c == 'b' || c == 'B') {
                ++_at;
                node n;
                n.kind = node_kind::assertion;
                n.assertion = c == 'b' ? assertion_kind::word : assertion_kind::not_word;
                return _add(std::move(n));
            }
            if (c == 'A' || c == 'z') {
                ++_at;
                node n;
                n.kind = node_kind::assertion;
                n.assertion = c == 'A' ? assertion_kind::begin_text : assertion_kind::end_text;
                return _add(std::move(n));
            }
            if (c >= '1' && c <= '9') {
                _fail(regex_fault::backreference, mark);
                return NoNode;
            }
            if (c == 'k' || c == 'g') {
                // \k<name> and \g{n}: a backreference under another name
                _fail(regex_fault::backreference, mark);
                return NoNode;
            }
            class_item it;
            if (_shorthand(it)) {
                char_class cc;
                cc.fold = flags.fold;
                cc.items.push_back(it);
                _classes.push_back(std::move(cc));
                node n;
                n.kind = node_kind::klass;
                n.klass = uint32_t(_classes.size() - 1);
                return _add(std::move(n));
            }
            if (_bad()) {
                return NoNode;
            }
            char32_t value = 0;
            if (!_char_escape(value)) {
                return NoNode;
            }
            node n;
            n.kind = node_kind::literal;
            n.literal = value;
            n.fold = flags.fold;
            return _add(std::move(n));
        }

        // \d \D \w \W \s \S \p{...} \P{...}: the escapes that stand for a
        // class rather than a character. The '\' is already past; the
        // letter is only taken when it is one of these.
        constexpr bool _shorthand(class_item& out) {
            char c = _peek();
            if (c == 'd' || c == 'D') {
                ++_at;
                out.kind = class_kind::categories;
                out.categories = uint32_t(1) << uint32_t(category::decimal_number);
                out.negate = c == 'D';
                return true;
            }
            if (c == 'w' || c == 'W') {
                ++_at;
                out.kind = class_kind::word;
                out.negate = c == 'W';
                return true;
            }
            if (c == 's' || c == 'S') {
                ++_at;
                out.kind = class_kind::space;
                out.negate = c == 'S';
                return true;
            }
            if (c == 'p' || c == 'P') {
                size_t mark = _at;
                ++_at;
                out.negate = c == 'P';
                std::string_view name;
                if (_peek() == '{') {
                    size_t from = ++_at;
                    while (_more() && _peek() != '}') {
                        ++_at;
                    }
                    if (!_more()) {
                        _fail(regex_fault::bad_property, mark);
                        return false;
                    }
                    name = _text.substr(from, _at - from);
                    ++_at;
                } else if (_more() && ((_peek() >= 'A' && _peek() <= 'Z') || (_peek() >= 'a' && _peek() <= 'z'))) {
                    // \pL, the one-letter form
                    name = _text.substr(_at, 1);
                    ++_at;
                } else {
                    _fail(regex_fault::bad_property, mark);
                    return false;
                }
                if (!_property(name, out)) {
                    _fail(regex_fault::unknown_property, mark);
                    return false;
                }
                return true;
            }
            return false;
        }

        // Two letters are a general category, one letter the group of
        // them, "Script=Cyrillic" or "sc=Cyrillic" or a bare script name
        // a script. Names are compared with the case and the separators
        // ignored, so Old_Italic, old-italic and OLDITALIC are one name.
        static constexpr bool _property(std::string_view name, class_item& out) noexcept {
            size_t eq = name.find('=');
            if (eq != std::string_view::npos) {
                std::string_view key = name.substr(0, eq);
                std::string_view value = name.substr(eq + 1);
                if (_loose_equal(key, "script") || _loose_equal(key, "sc")) {
                    return _script(value, out);
                }
                if (_loose_equal(key, "general_category") || _loose_equal(key, "gc")) {
                    return _category(value, out);
                }
                return false;
            }
            // A bare name is a category where one goes by it and a script
            // otherwise. The two sets do not meet: a category's name is
            // one or two letters and no script has such a name.
            return _category(name, out) || _script(name, out);
        }

        static constexpr bool _category(std::string_view name, class_item& out) noexcept {
            if (name.size() == 1) {
                uint32_t mask = categories_of_group(_upper(name[0]));
                if (!mask) {
                    return false;
                }
                out.kind = class_kind::categories;
                out.categories = mask;
                return true;
            }
            if (name.size() != 2) {
                return false;
            }
            for (size_t i = 0; i < std::size(CategoryNames); ++i) {
                if (_upper(name[0]) == CategoryNames[i][0] && _lower(name[1]) == CategoryNames[i][1]) {
                    out.kind = class_kind::categories;
                    out.categories = uint32_t(1) << i;
                    return true;
                }
            }
            return false;
        }

        static constexpr bool _script(std::string_view name, class_item& out) noexcept {
            for (size_t i = 0; i < std::size(ScriptNames); ++i) {
                if (_loose_equal(name, ScriptNames[i])) {
                    out.kind = class_kind::script;
                    out.script = uint16_t(i);
                    return true;
                }
            }
            return false;
        }

        static constexpr char _lower(char c) noexcept {
            return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
        }

        static constexpr char _upper(char c) noexcept {
            return c >= 'a' && c <= 'z' ? char(c - 32) : c;
        }

        // Unicode's own loose matching of a property name: case, the
        // spaces, the hyphens and the underscores do not count
        static constexpr bool _loose_equal(std::string_view a, std::string_view b) noexcept {
            size_t i = 0, j = 0;
            auto skip = [](std::string_view s, size_t& k) {
                while (k < s.size() && (s[k] == '_' || s[k] == '-' || s[k] == ' ')) {
                    ++k;
                }
            };
            while (true) {
                skip(a, i);
                skip(b, j);
                if (i == a.size() || j == b.size()) {
                    return i == a.size() && j == b.size();
                }
                if (_lower(a[i]) != _lower(b[j])) {
                    return false;
                }
                ++i;
                ++j;
            }
        }

        // \n, \x41, \x{1F600}, é, and a punctuation mark standing
        // for itself. The '\' is past and the letter has not been taken.
        constexpr bool _char_escape(char32_t& out) {
            size_t mark = _at - 1;
            char c = _peek();
            ++_at;
            switch (c) {
            case 'n': out = U'\n'; return true;
            case 'r': out = U'\r'; return true;
            case 't': out = U'\t'; return true;
            case 'f': out = U'\f'; return true;
            case 'v': out = U'\v'; return true;
            case 'a': out = U'\a'; return true;
            case 'e': out = 0x1B; return true;
            case '0': out = 0; return true;
            case 'x': return _hex(2, true, out, mark);
            case 'u': return _hex(4, false, out, mark);
            case 'U': return _hex(8, false, out, mark);
            default: break;
            }
            // Anything that is not a letter or a digit stands for itself,
            // which is the rule that lets \. \\ \[ \{ be written without
            // a table of them; a letter that got this far is an escape
            // nobody defined, and saying so is better than taking it for
            // the letter itself
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                _fail(regex_fault::unknown_escape, mark);
                return false;
            }
            if (uint8_t(c) >= 0x80) {
                _at = mark + 1;
                auto [value, width] = utf8::decode(_text, _at);
                _at += width;
                out = value;
                return true;
            }
            out = char32_t(uint8_t(c));
            return true;
        }

        // Exactly n hexadecimal digits, or, where braces are allowed
        // (\x{...}), as many as stand between them
        constexpr bool _hex(size_t n, bool braces, char32_t& out, size_t mark) {
            uint64_t v = 0;
            size_t digits = 0;
            if (braces && _take('{')) {
                while (_more() && _peek() != '}') {
                    int d = _hex_digit(_peek());
                    if (d < 0 || v > 0x10FFFF) {
                        _fail(regex_fault::bad_escape_value, mark);
                        return false;
                    }
                    v = v * 16 + uint64_t(d);
                    ++_at;
                    ++digits;
                }
                if (!_take('}') || !digits) {
                    _fail(regex_fault::bad_escape_value, mark);
                    return false;
                }
            } else {
                for (size_t i = 0; i < n; ++i) {
                    int d = _more() ? _hex_digit(_peek()) : -1;
                    if (d < 0) {
                        _fail(regex_fault::bad_escape_value, mark);
                        return false;
                    }
                    v = v * 16 + uint64_t(d);
                    ++_at;
                }
            }
            if (v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF)) {
                // a surrogate is not a code point a text can hold: UTF-8
                // cannot write one, so a pattern cannot ask for one
                _fail(regex_fault::bad_escape_value, mark);
                return false;
            }
            out = char32_t(v);
            return true;
        }

        static constexpr int _hex_digit(char c) noexcept {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }

        std::string_view _text;
        size_t _at = 0;
        regex_fault _fault = regex_fault::none;
        size_t _fault_at = 0;
        uint32_t _groups = 0;
        std::vector<node> _nodes;
        std::vector<char_class> _classes;
        std::vector<group_name> _names;
    };
}
