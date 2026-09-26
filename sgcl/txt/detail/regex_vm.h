//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "regex_program.h"

#include <memory>
#include <utility>

// Pike's machine: the program of regex_program.h run over the text with
// every alternative alive at once, in one pass, left to right.
//
// The list of threads is the whole idea. At a position in the text the
// machine holds the set of instructions some alternative could be at; it
// reads one code point, and each thread either dies or joins the list for
// the next position. A thread that reaches an instruction another thread
// has already reached at this position is dropped, since from there on
// the two are the same — and that one line is what turns the exponential
// search of a backtracking engine into a walk. The list can hold no more
// entries than the program has instructions, so a position costs at most
// the length of the program and the whole match costs that times the
// length of the text. There is no pattern that costs more; that is the
// property the engine is for.
//
// The order of the list is the other half. Threads are added in the order
// a backtracking engine would have tried them — the first branch of an
// alternation before the second, the body of a greedy repetition before
// leaving it — so the first thread to reach `match` is the one such an
// engine would have returned, and every thread behind it in the list can
// be dropped on the spot. That is what makes the answers the same as
// Perl's and Python's without their cost: leftmost-first, not the
// leftmost-longest of POSIX.
//
// Everything the machine needs is carved out of one block: the visit
// stamps, the two thread lists with their group positions, the stack of
// the walk over what consumes nothing, and the slots being written. The
// sizes all follow from the program and are known before the first code
// point is read, so a search allocates once rather than five times, and a
// matcher kept across several searches — which is what a loop over the
// occurrences, replace and split all do — allocates once for all of them.
// The lists are the exception: they start small and a search that keeps
// more threads alive than they hold moves them out to blocks of their own.
namespace sgcl::txt::detail {
    inline constexpr size_t NoPos = size_t(-1);

    class matcher {
    public:
        // `stamp` is where the visit marks start counting. Nothing but the
        // test that has to reach the wrap of a 32-bit counter without two
        // billion positions of text ever passes it.
        matcher(const program& p, std::string_view text, size_t ncap, uint32_t stamp = 0)
        : _p(p)
        , _text(text)
        , _ncap(ncap)
        , _insts(p.insts.size())
        , _stamp(stamp) {
            // in words of eight bytes, every piece aligned by being a
            // whole number of them
            size_t stamps = (_insts + 1) / 2;
            // Every instruction is taken off the stack at most once, and
            // taking one off puts at most two back (a split's two
            // branches, a save's slot and what follows it), so twice the
            // program plus the one it starts with is a bound nothing can
            // pass
            size_t stack = _stack_words * (2 * _insts + 4);
            // The two lists are the one piece whose size is not the program's.
            // program::listed is what a list COULD reach, and sizing the rows
            // by it multiplied the program by 2*(groups+1) words a row: 146
            // MB for a pattern of 250 groups spelled out to 19003
            // instructions, asked for by every find() over five bytes. What a
            // list reaches is another number — the instructions reachable at
            // one position without reading a code point, which for nearly
            // every pattern is a handful: over the module's own benchmark the
            // widest was 16 rows, the rest between 1 and 5. So the lists
            // start at that many rows, carved out of the same block so that
            // a short search still allocates once, and a list that runs out
            // moves to a block of its own of program::listed rows — see
            // _grow for why all of them at once.
            size_t rows = _start_rows();
            size_t caps = rows * _ncap;
            size_t pcs = (rows + 1) / 2;
            // for_overwrite, because make_unique would zero the whole block
            // and only the stamps need it: the stack, the slots being
            // written and both lists are written before they are read, the
            // lists only ever below the size each one carries.
            _block = std::make_unique_for_overwrite<uint64_t[]>(stamps + _ncap + stack
                                                                + 2 * (caps + pcs));
            uint64_t* at = _block.get();
            _gen = reinterpret_cast<uint32_t*>(at);
            at += stamps;
            _scratch = reinterpret_cast<size_t*>(at);
            at += _ncap;
            _stack = reinterpret_cast<step*>(at);
            at += stack;
            for (thread_list* list : {&_clist, &_nlist}) {
                list->caps = reinterpret_cast<size_t*>(at);
                at += caps;
                list->pc = reinterpret_cast<uint32_t*>(at);
                at += pcs;
                list->rows = rows;
            }
            _clear_gen();
        }

        // The first match at or after `from`, leftmost and then by the
        // priority of the branches. The slots — the whole match in 0 and
        // 1, then two to a group — come back in `caps`, NoPos where a
        // group took no part in the match.
        //
        // `whole` asks a different question: whether the text from `from`
        // to its end is a match, all of it. It is not find() with the
        // ends compared afterwards — the machine would have stopped at
        // the first match a backtracking engine would have preferred,
        // which may be shorter — so instead a thread that reaches `match`
        // anywhere but at the end of the text is simply not one.
        bool run(size_t from, size_t* caps, bool whole = false) {
            bool matched = false;
            const bool anchored = _p.anchored || whole;
            // A run of bytes every match must contain, and no such run
            // in the text: there is nothing to find and no reason to
            // start the machine at all. Remembered rather than asked
            // again, because a walk over the occurrences calls this from
            // one position after another and a fresh search from each
            // would make the whole walk cost the square of the text.
            if (!_p.required.empty()) {
                if (!_literal_known || from < _literal_from || from > _literal_at) {
                    _literal_at = _p.required_skip.find(_text, _p.required, from);
                    _literal_from = from;
                    _literal_known = true;
                }
                if (_literal_at == NoPos) {
                    return false;
                }
            }
            _clist.size = 0;
            _clist.stamp = _next_stamp();
            size_t pos = from;
            for (;;) {
                if (!_clist.size) {
                    if (matched) {
                        break;
                    }
                    if (anchored && pos != from) {
                        break;
                    }
                    // Nothing is alive, so no position before the next
                    // byte a match could begin with is worth a step.
                    // This is where the time of a search that finds
                    // nothing goes, and skipping it is most of what
                    // makes such a search quick.
                    if (!anchored && _p.required_at_start) {
                        // Every match begins with those bytes, so the
                        // next place one could begin is the next place
                        // they stand — found by the same skip table
                        // txt::searcher uses, in about a step for every
                        // byte of the run rather than one for every byte
                        // of the text
                        pos = _p.required_skip.find(_text, _p.required, pos);
                        if (pos == NoPos) {
                            break;
                        }
                    } else if (!anchored && _p.first_known) {
                        while (pos < _text.size() && !_p.may_begin_with(uint8_t(_text[pos]))) {
                            ++pos;
                        }
                    }
                }
                if (!matched && (!anchored || pos == from)) {
                    for (size_t k = 0; k < _ncap; ++k) {
                        _scratch[k] = NoPos;
                    }
                    _add(_clist, _p.start, pos);
                }
                if (!_clist.size && pos >= _text.size()) {
                    break;
                }
                char32_t c = 0;
                size_t width = 1;
                if (pos < _text.size()) {
                    auto decoded = utf8::decode(_text, pos);
                    c = decoded.first;
                    width = decoded.second;
                }
                _nlist.size = 0;
                _nlist.stamp = _next_stamp();
                size_t live = _clist.size;
                for (size_t t = 0; t < live; ++t) {
                    uint32_t pc = _clist.pc[t];
                    const inst& i = _p.insts[pc];
                    if (i.op == opcode::match) {
                        if (whole && pos != _text.size()) {
                            continue;
                        }
                        matched = true;
                        const size_t* held = _clist.caps + t * _ncap;
                        for (size_t k = 0; k < _ncap; ++k) {
                            caps[k] = held[k];
                        }
                        // every thread after this one carries a branch
                        // the writer of the pattern put second
                        break;
                    }
                    bool go = false;
                    if (pos < _text.size()) {
                        switch (i.op) {
                        case opcode::literal:
                            go = i.fold ? unicode::equal_fold(c, i.ch) : c == i.ch;
                            break;
                        case opcode::klass:
                            go = _p.classes[i.x].holds(c);
                            break;
                        case opcode::any:
                            go = i.newline || c != U'\n';
                            break;
                        default:
                            break;
                        }
                    }
                    if (go) {
                        const size_t* held = _clist.caps + t * _ncap;
                        for (size_t k = 0; k < _ncap; ++k) {
                            _scratch[k] = held[k];
                        }
                        _add(_nlist, pc + 1, pos + width);
                    }
                }
                std::swap(_clist, _nlist);
                if (pos >= _text.size()) {
                    break;
                }
                pos += width;
            }
            return matched;
        }

        // How many threads the longer of the two lists has room for. Nothing
        // but the test that holds the lists to what a search reaches asks it.
        size_t list_rows() const noexcept {
            return _clist.rows > _nlist.rows ? _clist.rows : _nlist.rows;
        }

    private:
        struct thread_list {
            // empty until the list has outgrown the rows it was given in
            // the matcher's block
            std::unique_ptr<uint64_t[]> block;
            uint32_t* pc = nullptr;
            size_t* caps = nullptr;
            size_t size = 0;
            size_t rows = 0;      // threads there is room for
            uint32_t stamp = 0;
        };

        // One item of the walk over what consumes nothing. `undo` is how
        // a `save` is taken back when the walk leaves the branch it was
        // written in: the recursion this replaces would have done it by
        // returning, and a stack deep enough for the longest program the
        // engine allows is not one to put on the machine's.
        struct step {
            size_t old = 0;
            uint32_t pc = 0;
            uint32_t slot = 0;
            bool undo = false;
        };

        static constexpr size_t _stack_words = (sizeof(step) + 7) / 8;

        void _clear_gen() noexcept {
            for (size_t i = 0; i < _insts; ++i) {
                _gen[i] = 0;
            }
        }

        // Where a list starts. Sixteen covers every pattern of the module's
        // benchmark — the widest of them reaches 16 — and a list can never
        // want more rows than program::listed, so a small program asks for
        // no more than it could use.
        size_t _start_rows() const noexcept {
            size_t want = _p.listed < 16 ? size_t(_p.listed) : size_t(16);
            return want ? want : 1;
        }

        // All the rows a list can ever want, program::listed, carrying over
        // the ones already there — so a list moves once and never again.
        //
        // Doubling was tried first and measured worse. The pattern of 250
        // groups over "hello" followed by two thousand b's keeps some two
        // thousand threads alive; with the lists sized by listed it peaked
        // at 21.7 MB resident, and doubling took it to 53.5 MB and 7% more
        // time, because every generation is written whole as it is copied
        // and the allocator keeps the freed large blocks resident for reuse.
        // Uncapped it was worse again: 32768 rows where 18251 is the most
        // there can be. A block of listed rows costs nothing for the rows
        // nobody writes — for_overwrite leaves them untouched — so a search
        // that fills its lists pays what it always did, 21.9 MB and level in
        // time, and one that stays within its first rows, which is nearly
        // every search there is, no longer asks for the product at all.
        //
        // The group slots come first so that they keep the alignment of a
        // size_t without a word being wasted on it.
        void _grow(thread_list& list) {
            size_t want = _p.listed;
            size_t caps = want * _ncap;
            size_t pcs = (want + 1) / 2;
            auto block = std::make_unique_for_overwrite<uint64_t[]>(caps + pcs);
            size_t* new_caps = reinterpret_cast<size_t*>(block.get());
            uint32_t* new_pc = reinterpret_cast<uint32_t*>(block.get() + caps);
            for (size_t i = 0; i < list.size; ++i) {
                new_pc[i] = list.pc[i];
            }
            for (size_t i = 0; i < list.size * _ncap; ++i) {
                new_caps[i] = list.caps[i];
            }
            list.block = std::move(block);
            list.caps = new_caps;
            list.pc = new_pc;
            list.rows = want;
        }

        // The next visit stamp, which is where a 32-bit counter has to be
        // told what to do when it runs out.
        //
        // _gen holds zero for an instruction nobody has stood on yet, and
        // _add drops a thread whose instruction already carries this list's
        // stamp — so the moment the counter came back round to zero, every
        // instruction not yet visited read as "already here at this position"
        // and was dropped. The matcher is kept for a whole walk, and the
        // counter climbs once per run() and once per position, so all() over
        // a couple of gigabytes reached it; matches simply stopped being
        // found, with nothing said. Clearing the marks and starting again at
        // one costs a compare per position and a sweep of the program once
        // every four billion stamps, which is nothing, and there is then no
        // stamp any mark can collide with.
        uint32_t _next_stamp() noexcept {
            if (++_stamp == 0) [[unlikely]] {
                _clear_gen();
                _stamp = 1;
            }
            return _stamp;
        }

        // Everything reachable from pc without reading a code point, in
        // the order a backtracking engine would have tried it, with the
        // group positions written down on the way. An instruction already
        // in this list is skipped, which both keeps the list short and
        // makes a loop over something that matches nothing terminate.
        void _add(thread_list& list, uint32_t pc, size_t pos) {
            size_t top = 0;
            _stack[top++] = step{0, pc, 0, false};
            while (top) {
                step e = _stack[--top];
                if (e.undo) {
                    _scratch[e.slot] = e.old;
                    continue;
                }
                if (e.pc >= _insts || _gen[e.pc] == list.stamp) {
                    continue;
                }
                _gen[e.pc] = list.stamp;
                const inst& i = _p.insts[e.pc];
                switch (i.op) {
                case opcode::jump:
                    _stack[top++] = step{0, i.x, 0, false};
                    break;
                case opcode::split:
                    // y goes on first so that x comes off first: that
                    // order is the whole of the leftmost-first rule
                    _stack[top++] = step{0, i.y, 0, false};
                    _stack[top++] = step{0, i.x, 0, false};
                    break;
                case opcode::save:
                    if (i.x < _ncap) {
                        _stack[top++] = step{_scratch[i.x], 0, i.x, true};
                        _scratch[i.x] = pos;
                    }
                    _stack[top++] = step{0, e.pc + 1, 0, false};
                    break;
                case opcode::check:
                    if (_holds(i.assertion, pos)) {
                        _stack[top++] = step{0, e.pc + 1, 0, false};
                    }
                    break;
                default:
                    if (list.size == list.rows) [[unlikely]] {
                        _grow(list);
                    }
                    list.pc[list.size] = e.pc;
                    for (size_t k = 0; k < _ncap; ++k) {
                        list.caps[list.size * _ncap + k] = _scratch[k];
                    }
                    ++list.size;
                    break;
                }
            }
        }

        bool _holds(assertion_kind kind, size_t pos) const noexcept {
            switch (kind) {
            case assertion_kind::begin_text:
                return pos == 0;
            case assertion_kind::end_text:
                return pos == _text.size();
            case assertion_kind::begin_line:
                return pos == 0 || _text[pos - 1] == '\n';
            case assertion_kind::end_line:
                return pos == _text.size() || _text[pos] == '\n';
            case assertion_kind::word:
                return _word_edge(pos);
            case assertion_kind::not_word:
                return !_word_edge(pos);
            }
            return false;
        }

        // A word boundary is where a word character stands on one side
        // and something else on the other, the edges of the text
        // counting as something else. A code point, not a byte: the "ż"
        // of "mężczyzna" is a letter and no boundary falls inside it.
        //
        // Not the boundaries of UAX #29, which segment.h has and which
        // this could have called. They answer a different question. The
        // annex divides a text into words *and the pieces between them*,
        // so it breaks between two commas and between a space and a full
        // stop; \b is asked whether a match may begin here, and every
        // engine draws it around \w. A \b that held between two commas
        // would surprise every reader of a pattern. The annex is the
        // right answer to words(); this is the right answer to \b.
        bool _word_edge(size_t pos) const noexcept {
            bool before = pos > 0 && is_word_point(utf8::decode_last(_text, pos).first);
            bool after = pos < _text.size() && is_word_point(utf8::decode(_text, pos).first);
            return before != after;
        }

        const program& _p;
        std::string_view _text;
        size_t _ncap;
        size_t _insts;
        uint32_t _stamp = 0;
        size_t _literal_from = 0;
        size_t _literal_at = NoPos;
        bool _literal_known = false;
        std::unique_ptr<uint64_t[]> _block;
        uint32_t* _gen = nullptr;
        size_t* _scratch = nullptr;
        step* _stack = nullptr;
        thread_list _clist;
        thread_list _nlist;
    };
}
