//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "regex_ast.h"
#include "skip_table.h"

#include <array>
#include <string>

// The tree turned into a program: a list of instructions for a machine
// that has a position in the text, a program counter and nothing else.
// Thompson's construction, with Pike's two additions — an instruction that
// writes down where a group began or ended, and an ordering of the
// branches so that the machine can answer what a backtracking engine would
// have answered without backtracking.
//
// Five instructions do the matching (a code point, a class, any code
// point, a place in the text, "this is a match") and three do the control
// (split, jump, save). Nothing in here can loop without consuming a code
// point unless the machine notices, which is the whole guarantee: the
// matcher visits each instruction at most once per position in the text,
// so the work is the length of the text times the length of the program,
// and no pattern can make it more.
namespace sgcl::txt::detail {
    enum class opcode : uint8_t {
        literal,    // ch, fold: one code point
        klass,      // x: which class
        any,        // newline: whether a line feed counts
        split,      // x first, y second — the priority is the whole of the leftmost-first rule
        jump,       // x
        save,       // x: which slot takes the position
        check,      // assertion: a place between code points, no code point consumed
        match,
    };

    struct inst {
        opcode op = opcode::match;
        bool fold = false;
        bool newline = false;
        assertion_kind assertion = assertion_kind::begin_text;
        char32_t ch = 0;
        uint32_t x = 0;
        uint32_t y = 0;
    };

    struct program {
        std::vector<inst> insts;
        std::vector<char_class> classes;
        std::vector<group_name> names;
        uint32_t groups = 0;
        uint32_t start = 0;

        // The most entries a thread list can ever hold. Not the length of
        // the program: the machine's _add walks jump, split, save and check
        // through without storing them and only ever appends an instruction
        // that reads a code point, so the list is as long as there are such
        // instructions and no longer. It is between a sixth and four fifths
        // of the program, and it is the most rows the matcher's two lists
        // ever move up to — the group slots are a row of 2*(groups+1) words
        // each, so sizing them by the whole program was paying for the
        // splits and the saves twice over. They start at far fewer: see
        // matcher::_grow.
        uint32_t listed = 0;

        // Every match must begin where the text does (\A, or ^ outside
        // multiline): the search then tries one position instead of all
        // of them
        bool anchored = false;

        // The bytes a match can begin with, when they are fewer than all
        // of them. Where the thread list runs dry the search skips to the
        // next byte in this set instead of asking the program at every
        // position — which is where most of the time of an unanchored
        // search over a long text goes.
        bool first_known = false;
        uint64_t first[4] = {};

        constexpr bool may_begin_with(uint8_t b) const noexcept {
            return (first[b >> 6] >> (b & 63)) & 1;
        }

        // A run of bytes every match must contain, and whether it must
        // contain it at its own beginning. Where it must, the search
        // jumps to the next occurrence of it instead of stepping a byte
        // at a time; where it need not, the run still says at once that
        // a text without it holds no match at all, which is what most
        // of a search that finds nothing is. Empty where the pattern
        // gives no such run — one that begins with a class or an
        // alternation gives none.
        std::string required;
        bool required_at_start = false;
        skip_table required_skip;
    };

    // How far up a code point whose simple case mapping lands in ASCII can
    // stand. Four of them do (U+0130, U+0131, U+017F, U+212A) and the highest
    // is the Kelvin sign, so the sweep below stops above it rather than
    // walking the million code points of Unicode at every translation unit.
    // The bound is asserted by a test that does walk all of them.
    inline constexpr char32_t AsciiFoldLimit = 0x2200;

    // A bit per ASCII code point some code point above U+007F folds to. Read
    // off the case tables at compile time rather than written out, so that a
    // regenerated table brings its own answer. `a` is ASCII, so equal_fold(a, d)
    // holds only when the two lowers agree or the two uppers do; the ASCII
    // code points sharing a lower L are L and to_upper(L), and those sharing
    // an upper U are U and to_lower(U), which is what the four marks are.
    constexpr auto ascii_folds_outside() noexcept {
        std::array<uint64_t, 2> mask{};
        auto mark = [&mask](char32_t a) {
            if (a < 128) {
                mask[a >> 6] |= uint64_t(1) << (a & 63);
            }
        };
        for (char32_t d = 128; d < AsciiFoldLimit; ++d) {
            char32_t lo = unicode::to_lower(d);
            char32_t up = unicode::to_upper(d);
            if (lo < 128) {
                mark(lo);
                mark(unicode::to_upper(lo));
            }
            if (up < 128) {
                mark(up);
                mark(unicode::to_lower(up));
            }
        }
        return mask;
    }

    inline constexpr auto AsciiFoldsOutside = ascii_folds_outside();

    // The tree walked once, depth first, writing instructions as it goes.
    // A {n,m} is spelled out here rather than counted at match time: the
    // machine has no counter, and giving it one is exactly the door
    // through which a pattern could cost more than linear time.
    class regex_compiler {
    public:
        constexpr regex_compiler(const regex_tree& tree, program& out) noexcept
        : _tree(tree)
        , _out(out) {
        }

        constexpr regex_fault_at compile() {
            _out.classes = _tree.classes;
            _out.names = _tree.names;
            _out.groups = _tree.groups;
            _out.start = 0;
            _emit(inst{opcode::save, false, false, {}, 0, 0, 0});
            _walk(_tree.root);
            inst close;
            close.op = opcode::save;
            close.x = 1;
            _emit(close);
            _emit(inst{opcode::match, false, false, {}, 0, 0, 0});
            if (_bad()) {
                return {_fault, 0};
            }
            _out.anchored = _anchored(_tree.root);
            _out.listed = _listed();
            _first_bytes();
            _required_run();
            return {};
        }

    private:
        constexpr bool _bad() const noexcept {
            return _fault != regex_fault::none;
        }

        constexpr uint32_t _here() const noexcept {
            return uint32_t(_out.insts.size());
        }

        // Past the limit the fault is raised but the instruction is still
        // written: every walk below returns the moment it sees a fault,
        // so only a handful more can follow, and the indices already
        // handed out stay pointing at what they were meant to. Nothing of
        // such a program is ever run.
        constexpr uint32_t _emit(inst i) {
            if (_out.insts.size() >= MaxRegexInsts) {
                _fault = regex_fault::too_large;
            }
            _out.insts.push_back(i);
            return uint32_t(_out.insts.size() - 1);
        }

        constexpr void _walk(uint32_t index) {
            if (_bad() || index == NoNode) {
                return;
            }
            const node& n = _tree.nodes[index];
            switch (n.kind) {
            case node_kind::empty:
                return;
            case node_kind::literal: {
                inst i;
                i.op = opcode::literal;
                i.ch = n.literal;
                i.fold = n.fold;
                _emit(i);
                return;
            }
            case node_kind::any: {
                inst i;
                i.op = opcode::any;
                i.newline = n.newline;
                _emit(i);
                return;
            }
            case node_kind::klass: {
                inst i;
                i.op = opcode::klass;
                i.x = n.klass;
                _emit(i);
                return;
            }
            case node_kind::assertion: {
                inst i;
                i.op = opcode::check;
                i.assertion = n.assertion;
                _emit(i);
                return;
            }
            case node_kind::concat:
                for (uint32_t kid : n.kids) {
                    _walk(kid);
                }
                return;
            case node_kind::alternate: {
                std::vector<uint32_t> leaving;
                for (size_t k = 0; k + 1 < n.kids.size(); ++k) {
                    inst s;
                    s.op = opcode::split;
                    uint32_t at = _emit(s);
                    _out.insts[at].x = _here();
                    _walk(n.kids[k]);
                    inst j;
                    j.op = opcode::jump;
                    leaving.push_back(_emit(j));
                    _out.insts[at].y = _here();
                    if (_bad()) {
                        return;
                    }
                }
                _walk(n.kids.back());
                for (uint32_t at : leaving) {
                    _out.insts[at].x = _here();
                }
                return;
            }
            case node_kind::capture: {
                inst open;
                open.op = opcode::save;
                open.x = 2 * n.group;
                _emit(open);
                _walk(n.kids[0]);
                inst close;
                close.op = opcode::save;
                close.x = 2 * n.group + 1;
                _emit(close);
                return;
            }
            case node_kind::repeat:
                _repeat(n);
                return;
            }
        }

        // One turn of a spelled-out repetition, counted whether or not it
        // wrote anything. The instruction ceiling cannot stand in for this:
        // it is _emit that raises it, so a body emitting nothing — (?:),
        // (?i:), a group holding only a flag — never reaches it, and the
        // turns of such a body multiply through the nesting unchecked.
        // (?:(?:(?:(?:){1000}){1000}){1000}){1000} is forty bytes and 10^12
        // turns, and the depth limit of 200 allows 1000^200.
        constexpr bool _turn() noexcept {
            if (++_turns > MaxRegexExpansion) {
                _fault = regex_fault::too_large;
                return false;
            }
            return true;
        }

        constexpr void _repeat(const node& n) {
            uint32_t body = n.kids[0];
            // The n copies that must be there
            uint32_t last = 0;
            for (uint32_t k = 0; k < n.min && !_bad(); ++k) {
                if (!_turn()) {
                    return;
                }
                last = _here();
                _walk(body);
            }
            if (_bad()) {
                return;
            }
            if (n.max == Unbounded) {
                if (n.min > 0) {
                    // the last copy already written is the one that
                    // repeats: L: body; split L, out — a whole copy of
                    // the body fewer than a star written after it
                    _star_after(last, n.greedy);
                } else {
                    _star(body, n.greedy);
                }
                return;
            }
            // The m - n that may be there: each one guarded by a split
            // that can leave, all of them leaving to the same place
            std::vector<uint32_t> leaving;
            for (uint32_t k = n.min; k < n.max && !_bad(); ++k) {
                if (!_turn()) {
                    break;
                }
                inst s;
                s.op = opcode::split;
                uint32_t at = _emit(s);
                leaving.push_back(at);
                _out.insts[at].x = _here();
                _walk(body);
            }
            for (uint32_t at : leaving) {
                if (n.greedy) {
                    _out.insts[at].y = _here();
                } else {
                    _out.insts[at].y = _out.insts[at].x;
                    _out.insts[at].x = _here();
                }
            }
        }

        // L: split body, out; body; jump L; out:
        constexpr void _star(uint32_t body, bool greedy) {
            uint32_t loop = _here();
            inst s;
            s.op = opcode::split;
            uint32_t at = _emit(s);
            uint32_t enter = _here();
            _walk(body);
            inst j;
            j.op = opcode::jump;
            j.x = loop;
            _emit(j);
            uint32_t out = _here();
            _out.insts[at].x = greedy ? enter : out;
            _out.insts[at].y = greedy ? out : enter;
        }

        // The last copy of the body has just been written and begins at
        // `loop`: split loop, out — so that copy is the one repeated
        constexpr void _star_after(uint32_t loop, bool greedy) {
            inst s;
            s.op = opcode::split;
            uint32_t at = _emit(s);
            uint32_t out = _here();
            _out.insts[at].x = greedy ? loop : out;
            _out.insts[at].y = greedy ? out : loop;
        }

        // How many instructions the machine's _add can ever put in a thread
        // list: the ones that read a code point, and `match`. Everything
        // else it walks through without storing, so a list can hold no more
        // than these however long the program is.
        constexpr uint32_t _listed() const noexcept {
            uint32_t n = 0;
            for (const inst& i : _out.insts) {
                if (i.op == opcode::literal || i.op == opcode::klass
                    || i.op == opcode::any || i.op == opcode::match) {
                    ++n;
                }
            }
            return n ? n : 1;
        }

        // Whether every match must begin where the text does
        constexpr bool _anchored(uint32_t index) const {
            if (index == NoNode) {
                return false;
            }
            const node& n = _tree.nodes[index];
            switch (n.kind) {
            case node_kind::assertion:
                return n.assertion == assertion_kind::begin_text;
            case node_kind::concat:
                for (uint32_t kid : n.kids) {
                    const node& k = _tree.nodes[kid];
                    if (_anchored(kid)) {
                        return true;
                    }
                    // a piece that can match nothing does not stop the
                    // anchor from being the next one's
                    if (!(k.kind == node_kind::repeat && k.min == 0) && k.kind != node_kind::empty) {
                        return false;
                    }
                }
                return false;
            case node_kind::capture:
                return _anchored(n.kids[0]);
            case node_kind::repeat:
                return n.min >= 1 && _anchored(n.kids[0]);
            case node_kind::alternate:
                for (uint32_t kid : n.kids) {
                    if (!_anchored(kid)) {
                        return false;
                    }
                }
                return !n.kids.empty();
            default:
                return false;
            }
        }

        // The longest run of bytes every match must contain, read off the
        // tree rather than off the program: the tree still says which
        // pieces stand one after another, where the program has already
        // turned that into jumps. Everything that consumes a code point
        // but is not one fixed code point — a class, a dot, an
        // alternation, a repetition that may be skipped — breaks the run
        // and the scan goes on after it, since a concatenation must
        // match all of its pieces in order however wide each one is. An
        // assertion breaks nothing: it consumes nothing, so the literals
        // on either side of it are still next to each other in the text.
        static constexpr char32_t Opaque = 0x200000;

        // `work` counts every call, not every character written, because the
        // spelling-out below multiplies through the nesting the same way
        // _repeat's does and for the same reason: a body that writes nothing
        // makes `out` no longer, so the guard on out.size() never fires and
        // (?:(?:(?:){8}){8}){8}... walks 8^depth times having written nothing.
        // Running out of budget breaks the run, which only ever shortens the
        // required string and can never pass over a match.
        constexpr void _flatten(uint32_t index, std::vector<char32_t>& out, size_t depth,
                                size_t& work) const {
            if (index == NoNode || depth > MaxRegexDepth || out.size() > 512
                || ++work > MaxRegexExpansion) {
                out.push_back(Opaque);
                return;
            }
            const node& n = _tree.nodes[index];
            switch (n.kind) {
            case node_kind::empty:
            case node_kind::assertion:
                return;
            case node_kind::literal:
                out.push_back(n.fold ? Opaque : n.literal);
                return;
            case node_kind::any:
            case node_kind::klass:
            case node_kind::alternate:
                out.push_back(Opaque);
                return;
            case node_kind::concat:
                for (uint32_t kid : n.kids) {
                    _flatten(kid, out, depth + 1, work);
                }
                return;
            case node_kind::capture:
                _flatten(n.kids[0], out, depth + 1, work);
                return;
            case node_kind::repeat:
                // a count that is exact and small is spelled out, so
                // (ab){2} gives "abab"; anything else gives what one
                // turn of it must be and then stops, since the next
                // turn may or may not be there
                if (n.min == n.max && n.min <= 8) {
                    for (uint32_t k = 0; k < n.min; ++k) {
                        _flatten(n.kids[0], out, depth + 1, work);
                    }
                    return;
                }
                if (n.min >= 1) {
                    _flatten(n.kids[0], out, depth + 1, work);
                }
                out.push_back(Opaque);
                return;
            }
        }

        constexpr void _required_run() {
            std::vector<char32_t> flat;
            size_t work = 0;
            _flatten(_tree.root, flat, 0, work);
            std::string best;
            std::string run;
            bool at_start = true;
            bool best_at_start = false;
            auto close = [&] {
                if (run.size() > best.size()) {
                    best = run;
                    best_at_start = at_start;
                }
                run.clear();
                at_start = false;
            };
            for (char32_t c : flat) {
                if (c == Opaque || run.size() > 1024) {
                    close();
                    continue;
                }
                utf8::encoded e(c);
                for (size_t k = 0; k < e.size; ++k) {
                    run.push_back(e.bytes[k]);
                }
            }
            close();
            _out.required = best;
            _out.required_at_start = best_at_start;
            if (!best.empty()) {
                _out.required_skip.prepare(best);
            }
        }

        // The set of bytes a match can begin with, worked out by walking
        // the program from its start over everything that consumes
        // nothing. An assertion is passed through rather than asked,
        // since the set only has to be large enough; reaching the match
        // instruction means the empty text matches, and then every
        // position is a candidate and the set is worth nothing.
        constexpr void _first_bytes() {
            std::vector<uint8_t> seen(_out.insts.size(), 0);
            std::vector<uint32_t> stack;
            stack.push_back(_out.start);
            bool known = true;
            while (!stack.empty() && known) {
                uint32_t pc = stack.back();
                stack.pop_back();
                if (pc >= _out.insts.size() || seen[pc]) {
                    continue;
                }
                seen[pc] = 1;
                const inst& i = _out.insts[pc];
                switch (i.op) {
                case opcode::jump:
                    stack.push_back(i.x);
                    break;
                case opcode::split:
                    stack.push_back(i.x);
                    stack.push_back(i.y);
                    break;
                case opcode::save:
                case opcode::check:
                    stack.push_back(pc + 1);
                    break;
                case opcode::match:
                case opcode::any:
                    known = false;
                    break;
                case opcode::literal:
                    _add_first(i.ch);
                    if (i.fold) {
                        _add_fold_class(i.ch);
                    }
                    break;
                case opcode::klass: {
                    const char_class& c = _out.classes[i.x];
                    for (char32_t b = 0; b < 128; ++b) {
                        if (c.holds(b)) {
                            _out.first[b >> 6] |= uint64_t(1) << (b & 63);
                        }
                    }
                    if (c.may_hold_non_ascii()) {
                        // every lead byte, rather than working out which:
                        // the classes that reach out of ASCII are the ones
                        // whose members cannot be listed cheaply anyway
                        _out.first[2] = ~uint64_t(0);
                        _out.first[3] = ~uint64_t(0);
                    }
                    break;
                }
                }
            }
            _out.first_known = known;
            if (!known) {
                _out.first[0] = _out.first[1] = _out.first[2] = _out.first[3] = 0;
            }
        }

        constexpr void _add_first(char32_t c) noexcept {
            utf8::encoded e(c);
            uint8_t b = uint8_t(e.bytes[0]);
            _out.first[b >> 6] |= uint64_t(1) << (b & 63);
        }

        // Every byte a code point equal_fold takes for `ch` can begin with.
        //
        // The three-line version this replaces added ch, its lower and its
        // upper, and a fold class is not always those three: equal_fold asks
        // whether the lowers agree or the uppers do, so 'k' holds U+212A the
        // Kelvin sign, 's' holds U+017F the long s, and 'i' holds U+0130 and
        // U+0131. A match beginning with one of those was passed over —
        // matches() said yes and contains() said no about the same text.
        //
        // The ASCII half is exact and costs four questions rather than the
        // hundred and twenty-eight a sweep would: an ASCII c is in the class
        // only when to_lower(c) is to_lower(ch) or to_upper(c) is
        // to_upper(ch), and the ASCII code points sharing a lower L are L and
        // to_upper(L), those sharing an upper U are U and to_lower(U). That
        // is four marks, and it is why (?i)zyzykot still costs 585 ns to
        // compile rather than the 880 the sweep cost.
        //
        // Above U+007F the orbits cannot be listed cheaply, so a code point
        // whose class reaches out of ASCII contributes every lead byte, which
        // is what the class path already does for [k] under (?i). Which ASCII
        // code points those are is read off the case tables rather than
        // written out here, so a regenerated table brings its own answer:
        // exactly six of them (i I k K s S). A pattern naming none of the six
        // keeps the narrow set it had — (?i)zyzykot still begins with two
        // bytes and not with half of them.
        constexpr void _add_fold_class(char32_t ch) noexcept {
            char32_t lo = unicode::to_lower(ch);
            char32_t up = unicode::to_upper(ch);
            if (lo < 128) {
                _add_first(lo);
                _add_first(unicode::to_upper(lo));
            }
            if (up < 128) {
                _add_first(up);
                _add_first(unicode::to_lower(up));
            }
            const bool outside = ch >= 128 || ((AsciiFoldsOutside[ch >> 6] >> (ch & 63)) & 1);
            if (outside) {
                _out.first[2] = ~uint64_t(0);
                _out.first[3] = ~uint64_t(0);
            }
        }

        const regex_tree& _tree;
        program& _out;
        regex_fault _fault = regex_fault::none;
        size_t _turns = 0;
    };
}
