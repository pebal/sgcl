//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "normalize.h"
#include "detail/bidi_tables.h"

// Text that runs both ways at once. Arabic, Hebrew, Persian and Urdu are
// written right to left, but the numbers inside them run left to right,
// and so does a Latin word quoted in them. One line then has pieces going
// both ways, and the order the characters are stored in — the logical
// order, which is the order they are typed and read — is not the order
// they are drawn in.
//
//   stored:  Nazwa: שלום 123 OK
//   drawn:   Nazwa: OK 123 םולש
//
// The algorithm of UAX #9 gives every character a level: 0 runs left to
// right, 1 right to left, 2 left to right inside a right to left piece,
// and so on. What is drawn is then the pieces of odd level turned round.
// This is for whoever draws the text — the user interface, a terminal —
// and for moving a caret through it; storing, searching and comparing
// need none of it.
namespace sgcl::txt {
    using detail::bidi;

    // What the paragraph as a whole runs as, and what a caller may ask
    // for instead of letting the text say
    enum class direction : uint8_t {
        automatic,        // the first strong character decides
        left_to_right,
        right_to_left,
    };

    namespace detail {
        constexpr bidi bidi_of(char32_t c) noexcept {
            return bidi(value_of(c, bidi_tables::BidiClass));
        }

        constexpr bool is_isolate_initiator(bidi t) noexcept {
            return t == bidi::lri || t == bidi::rli || t == bidi::fsi;
        }

        constexpr bool is_removed_by_x9(bidi t) noexcept {
            return t == bidi::rle || t == bidi::lre || t == bidi::rlo || t == bidi::lro
                || t == bidi::pdf || t == bidi::bn;
        }

        constexpr const BracketPair* bracket_of(char32_t c) noexcept {
            size_t lo = 0, hi = std::size(bidi_tables::Brackets);
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (c < bidi_tables::Brackets[mid].bracket) {
                    hi = mid;
                } else if (c > bidi_tables::Brackets[mid].bracket) {
                    lo = mid + 1;
                } else {
                    return &bidi_tables::Brackets[mid];
                }
            }
            return nullptr;
        }

        // Rule L4: a character is drawn mirrored when it is resolved
        // right to left and its Bidi_Mirrored property is yes. That
        // property is the wider of the two things here — 554 code points
        // against the 428 that BidiMirroring.txt gives a mirror of their
        // own, an integral sign being drawn the other way round without
        // there being a second one to name it — so the set is asked and
        // not the mapping.
        constexpr bool is_mirrored_fn(char32_t c) noexcept {
            // The eight of ASCII — the three pairs of brackets and the
            // two angle signs — as two words rather than as the seven
            // steps of the bisection below, which is what most of a text
            // would otherwise pay to be told no: over a line of ASCII
            // 74 ns against 274, and over a mixed one 79 against 227
            if (c < 0x80) {
                uint64_t bits = c < 64 ? 0x5000030000000000ull : 0x2800000028000000ull;
                return (bits >> (c & 63)) & 1;
            }
            if (c < 0x10000) {
                return find_range(c, bidi_tables::MirroredBmp, std::size(bidi_tables::MirroredBmp))
                    != nullptr;
            }
            return find_range(c, bidi_tables::MirroredHigh, std::size(bidi_tables::MirroredHigh))
                != nullptr;
        }

        // The glyph to draw in its place, or the code point itself where
        // the mirrored shape has no code point of its own
        constexpr char32_t mirrored_of_fn(char32_t c) noexcept {
            if (c < 0x28 || c > 0xFF63) {
                return c;
            }
            size_t lo = 0, hi = std::size(bidi_tables::Mirroring);
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (c < bidi_tables::Mirroring[mid].cp) {
                    hi = mid;
                } else if (c > bidi_tables::Mirroring[mid].cp) {
                    lo = mid + 1;
                } else {
                    return char32_t(bidi_tables::Mirroring[mid].other);
                }
            }
            return c;
        }

        // BD16 says two brackets are a pair when they are canonically the
        // same, so U+2329 pairs with U+3009 as well as with U+232A
        inline char32_t bracket_canonical(char32_t c) noexcept {
            auto d = decomposition_of(c, normalize_tables::CanonicalDecomposition);
            return d && d.size == 1 ? char32_t(d.units[0]) : c;
        }

        inline constexpr uint8_t MaxDepth = 125;

        // One entry of the stack of X1 to X8
        struct bidi_entry {
            uint8_t level;
            bidi override_status;   // on when there is none
            bool isolate;
        };

        // One pair of brackets of N0, by their places in a sequence
        struct bidi_bracket {
            char32_t closing;
            size_t at;
        };

        // Every array the algorithm works in, over one paragraph. None of
        // them holds a pointer the collector has to see and none leaves
        // the call — what a caller is given, the levels or the visual
        // order, is copied out once, at its size — so they are plain
        // memory, lent by the thread (detail/lent.h). As vectors of the
        // library the arrays, the runs and the sequences were 47 managed
        // objects and 85 KB of garbage for a kilobyte of text given to
        // levels(); as fresh std::vectors, a malloc each, the levels of a
        // short line took two fifths longer than that.
        struct bidi_run {
            size_t first;
            size_t last;
        };

        // One paragraph of the text (rule P1): [first, last) of its code
        // points, the separator the last of them, and the level it runs at
        struct bidi_paragraph {
            size_t first;
            size_t last;
            uint8_t level;
            size_t order_end;     // where its characters end in the visual order
        };

        struct bidi_scratch {
            scratch_vector<char32_t> points;
            scratch_vector<size_t> at;
            scratch_vector<bidi> initial;
            scratch_vector<bidi> types;
            scratch_vector<uint8_t> levels;
            scratch_vector<uint8_t> explicit_levels;    // what X1 to X8 set, before I1 and I2
            scratch_vector<bidi_entry> stack;
            scratch_vector<size_t> significant;
            scratch_vector<bidi_run> runs;              // [first, last) of significant
            scratch_vector<uint8_t> used;
            scratch_vector<size_t> sequence;
            scratch_vector<bidi_bracket> brackets;
            scratch_vector<bidi_run> pairs;             // the places of an opening and a closing bracket
            scratch_vector<size_t> order;
            scratch_vector<bidi_paragraph> paragraphs;
        };

        template<class F>
        void lent_each(bidi_scratch& s, F& f) noexcept {
            f(s.points);
            f(s.at);
            f(s.initial);
            f(s.types);
            f(s.levels);
            f(s.explicit_levels);
            f(s.stack);
            f(s.significant);
            f(s.runs);
            f(s.used);
            f(s.sequence);
            f(s.brackets);
            f(s.pairs);
            f(s.order);
            f(s.paragraphs);
        }

        // The whole of the algorithm over a text. The text is kept as
        // code points, because every rule of it looks left and right and
        // does so many times.
        //
        // P1: the text is cut into paragraphs after every paragraph
        // separator (class B: U+2029, a line feed, a carriage return, the
        // information separators 1C to 1E, U+0085), the separator kept
        // with the paragraph it ends, and each paragraph is resolved on
        // its own — its own level by P2 and P3 where none is asked for,
        // its own embeddings, sequences and lines. Taken whole, a text of
        // two paragraphs had the second take its direction from the
        // first and its brackets and numbers resolved across the break.
        class paragraph {
        public:
            paragraph(std::string_view text, direction ask) noexcept
            : _points(_scratch->points)
            , _at(_scratch->at)
            , _initial(_scratch->initial)
            , _types(_scratch->types)
            , _levels(_scratch->levels)
            , _explicit(_scratch->explicit_levels)
            , _paragraphs(_scratch->paragraphs) {
                _points.reserve(text.size());
                _at.reserve(text.size());
                for (size_t i = 0; i < text.size();) {
                    auto [c, n] = utf8::decode(text, i);
                    _points.push_back(c);
                    _at.push_back(i);
                    i += n;
                }
                _initial.reserve(_points.size());
                size_t first = 0;
                for (size_t i = 0, n = _points.size(); i < n; ++i) {
                    bidi t = bidi_of(_points[i]);
                    _initial.push_back(t);
                    if (t == bidi::b && !_cr_lf(i)) [[unlikely]] {
                        _paragraphs.push_back({first, i + 1, 0, 0});
                        first = i + 1;
                    }
                }
                if (first < _points.size()) {
                    _paragraphs.push_back({first, _points.size(), 0, 0});
                }
                _types = _initial;
                _levels.assign(_points.size(), 0);
                for (auto& para : _paragraphs) {
                    _from = para.first;
                    _to = para.last;
                    para.level = ask == direction::left_to_right ? 0
                               : ask == direction::right_to_left ? 1
                               : _first_strong(_from, _to);
                    _level = para.level;
                    _explicit_levels();
                }
                // X10 and the runs are built from the levels X1 to X8
                // set, not from the ones I1 and I2 go on to change: a
                // sequence resolved early would otherwise decide what the
                // next one sees on its side
                _explicit = _levels;
                for (auto& para : _paragraphs) {
                    _from = para.first;
                    _to = para.last;
                    _level = para.level;
                    _sequences();
                    _reset_whitespace();
                }
                // what the text as a whole is said to run as: its first
                // paragraph, or what was asked of an empty one
                _level = !_paragraphs.empty() ? _paragraphs.front().level
                       : ask == direction::right_to_left ? 1 : 0;
            }

            // The level of the first paragraph
            uint8_t level() const noexcept {
                return _level;
            }

            // The paragraphs, each with where its characters end in
            // order(), once that has been asked
            const scratch_vector<bidi_paragraph>& paragraphs() const noexcept {
                return _paragraphs;
            }

            const scratch_vector<uint8_t>& levels() const noexcept {
                return _levels;
            }

            const scratch_vector<size_t>& positions() const noexcept {
                return _at;
            }

            const scratch_vector<char32_t>& points() const noexcept {
                return _points;
            }

            bool removed(size_t i) const noexcept {
                return is_removed_by_x9(_initial[i]);
            }

            size_t size() const noexcept {
                return _points.size();
            }

            // L2: the characters in the order they are drawn, the ones
            // X9 removed left out, a paragraph after a paragraph — each
            // is a line of its own: in the paragraph's scratch, valid as
            // long as it is
            const scratch_vector<size_t>& order() const noexcept {
                auto& out = _scratch->order;
                out.clear();
                for (auto& para : _paragraphs) {
                    size_t start = out.size();
                    for (size_t i = para.first; i < para.last; ++i) {
                        if (!removed(i)) {
                            out.push_back(i);
                        }
                    }
                    para.order_end = out.size();
                    uint8_t highest = 0;
                    uint8_t lowest_odd = MaxDepth + 2;
                    for (size_t k = start; k < out.size(); ++k) {
                        highest = std::max(highest, _levels[out[k]]);
                        if (_levels[out[k]] % 2) {
                            lowest_odd = std::min(lowest_odd, _levels[out[k]]);
                        }
                    }
                    for (uint8_t l = highest; l >= lowest_odd && l > 0; --l) {
                        for (size_t i = start; i < out.size();) {
                            if (_levels[out[i]] < l) {
                                ++i;
                                continue;
                            }
                            size_t j = i;
                            while (j < out.size() && _levels[out[j]] >= l) {
                                ++j;
                            }
                            std::reverse(out.begin() + ptrdiff_t(i), out.begin() + ptrdiff_t(j));
                            i = j;
                        }
                    }
                }
                return out;
            }

        private:
            // The CR of a CR LF, which is one separator and not two: an LF
            // made a paragraph of its own would run left to right whatever
            // the text around it did
            bool _cr_lf(size_t i) const noexcept {
                return _points[i] == U'\r' && i + 1 < _points.size() && _points[i + 1] == U'\n';
            }

            // P2, P3: the first strong character decides, and what stands
            // between an isolate initiator and its match does not count
            uint8_t _first_strong(size_t from, size_t to) const noexcept {
                for (size_t i = from; i < to; ++i) {
                    auto t = _initial[i];
                    if (is_isolate_initiator(t)) {
                        i = _matching_pdi(i, to);
                        continue;
                    }
                    if (t == bidi::l) {
                        return 0;
                    }
                    if (t == bidi::r || t == bidi::al) {
                        return 1;
                    }
                }
                return 0;
            }

            // BD9: the PDI that closes this initiator, or the end
            size_t _matching_pdi(size_t at, size_t to) const noexcept {
                int depth = 1;
                for (size_t i = at + 1; i < to; ++i) {
                    if (is_isolate_initiator(_initial[i])) {
                        ++depth;
                    } else if (_initial[i] == bidi::pdi && --depth == 0) {
                        return i;
                    }
                }
                return to;
            }

            // X1 to X8
            void _explicit_levels() noexcept {
                auto& stack = _scratch->stack;
                stack.clear();
                stack.push_back({_level, bidi::on, false});
                unsigned overflow_isolate = 0, overflow_embedding = 0, valid_isolate = 0;
                for (size_t i = _from, to = _to; i < to; ++i) {
                    auto t = _initial[i];
                    switch (t) {
                        case bidi::rle:
                        case bidi::lre:
                        case bidi::rlo:
                        case bidi::lro: {
                            _levels[i] = stack.back().level;
                            bool rtl = t == bidi::rle || t == bidi::rlo;
                            uint8_t next = rtl ? uint8_t((stack.back().level + 1) | 1)
                                               : uint8_t((stack.back().level + 2) & ~1);
                            if (next <= MaxDepth && !overflow_isolate && !overflow_embedding) {
                                stack.push_back({next, t == bidi::rlo ? bidi::r
                                                     : t == bidi::lro ? bidi::l : bidi::on, false});
                            } else if (!overflow_isolate) {
                                ++overflow_embedding;
                            }
                            break;
                        }
                        case bidi::rli:
                        case bidi::lri:
                        case bidi::fsi: {
                            bool rtl = t == bidi::rli;
                            if (t == bidi::fsi) {
                                rtl = _first_strong(i + 1, _matching_pdi(i, _to)) == 1;
                            }
                            _levels[i] = stack.back().level;
                            if (stack.back().override_status != bidi::on) {
                                _types[i] = stack.back().override_status;
                            }
                            uint8_t next = rtl ? uint8_t((stack.back().level + 1) | 1)
                                               : uint8_t((stack.back().level + 2) & ~1);
                            if (next <= MaxDepth && !overflow_isolate && !overflow_embedding) {
                                ++valid_isolate;
                                stack.push_back({next, bidi::on, true});
                            } else {
                                ++overflow_isolate;
                            }
                            break;
                        }
                        case bidi::pdi: {
                            if (overflow_isolate) {
                                --overflow_isolate;
                            } else if (valid_isolate) {
                                overflow_embedding = 0;
                                while (!stack.back().isolate) {
                                    stack.pop_back();
                                }
                                stack.pop_back();
                                --valid_isolate;
                            }
                            _levels[i] = stack.back().level;
                            if (stack.back().override_status != bidi::on) {
                                _types[i] = stack.back().override_status;
                            }
                            break;
                        }
                        case bidi::pdf: {
                            _levels[i] = stack.back().level;
                            if (overflow_isolate) {
                                // nothing: an isolate is open above it
                            } else if (overflow_embedding) {
                                --overflow_embedding;
                            } else if (!stack.back().isolate && stack.size() >= 2) {
                                stack.pop_back();
                            }
                            break;
                        }
                        case bidi::b: {
                            stack.resize(1);
                            overflow_isolate = overflow_embedding = valid_isolate = 0;
                            _levels[i] = _level;
                            break;
                        }
                        default: {
                            _levels[i] = stack.back().level;
                            if (stack.back().override_status != bidi::on) {
                                _types[i] = stack.back().override_status;
                            }
                            break;
                        }
                    }
                }
            }

            // X10: the isolating run sequences, and the rules W, N and I
            // over each of them
            void _sequences() noexcept {
                auto& significant = _scratch->significant;
                significant.clear();
                for (size_t i = _from, to = _to; i < to; ++i) {
                    if (!is_removed_by_x9(_initial[i])) {
                        significant.push_back(i);
                    }
                }
                if (significant.empty()) {
                    return;
                }
                // the level runs, over the characters X9 left: each one
                // a stretch [first, last) of the significant ones
                auto& runs = _scratch->runs;
                runs.clear();
                for (size_t k = 0; k < significant.size();) {
                    size_t j = k;
                    uint8_t l = _explicit[significant[k]];
                    while (j < significant.size() && _explicit[significant[j]] == l) {
                        ++j;
                    }
                    runs.push_back({k, j});
                    k = j;
                }
                auto& used = _scratch->used;
                used.assign(runs.size(), 0);
                auto& sequence = _scratch->sequence;
                for (size_t r = 0; r < runs.size(); ++r) {
                    if (used[r]) {
                        continue;
                    }
                    // a sequence starts at a run whose first character is
                    // not a PDI that closes an isolate initiator
                    size_t first = significant[runs[r].first];
                    if (_initial[first] == bidi::pdi && _opens_of(first) != _to) {
                        continue;
                    }
                    sequence.clear();
                    size_t at = r;
                    for (;;) {
                        used[at] = 1;
                        sequence.append(significant.begin() + runs[at].first, significant.begin() + runs[at].last);
                        size_t last = significant[runs[at].last - 1];
                        if (!is_isolate_initiator(_initial[last])) {
                            break;
                        }
                        size_t pdi = _matching_pdi(last, _to);
                        if (pdi == _to) {
                            break;
                        }
                        size_t next = _run_of(runs, pdi);
                        if (next == runs.size() || used[next]) {
                            break;
                        }
                        at = next;
                    }
                    _resolve(sequence);
                }
            }

            size_t _run_of(const scratch_vector<bidi_run>& runs, size_t at) const noexcept {
                auto& significant = _scratch->significant;
                for (size_t i = 0; i < runs.size(); ++i) {
                    if (significant[runs[i].first] == at) {
                        return i;
                    }
                }
                return runs.size();
            }

            // The isolate initiator this PDI closes, or the end of the
            // paragraph
            size_t _opens_of(size_t pdi) const noexcept {
                int depth = 1;
                for (size_t i = pdi; i > _from;) {
                    --i;
                    if (_initial[i] == bidi::pdi) {
                        ++depth;
                    } else if (is_isolate_initiator(_initial[i]) && --depth == 0) {
                        return i;
                    }
                }
                return _to;
            }

            void _resolve(const scratch_vector<size_t>& s) noexcept {
                if (s.empty()) {
                    return;
                }
                uint8_t level = _explicit[s.front()];
                // X10: the direction on either side of the sequence
                bidi sos = _side(s.front(), level, true);
                bidi eos = _side(s.back(), level, false);

                auto type = [&](size_t k) -> bidi& { return _types[s[k]]; };
                size_t n = s.size();

                // W1: a mark takes the type of what it follows
                for (size_t k = 0; k < n; ++k) {
                    if (type(k) == bidi::nsm) {
                        bidi before = k ? type(k - 1) : sos;
                        type(k) = is_isolate_initiator(before) || before == bidi::pdi ? bidi::on : before;
                    }
                }
                // W2: a European number after an Arabic letter is Arabic
                for (size_t k = 0; k < n; ++k) {
                    if (type(k) == bidi::en) {
                        for (size_t j = k; j > 0;) {
                            --j;
                            auto t = type(j);
                            if (t == bidi::l || t == bidi::r || t == bidi::al) {
                                if (t == bidi::al) {
                                    type(k) = bidi::an;
                                }
                                break;
                            }
                            if (j == 0 && sos == bidi::al) {
                                type(k) = bidi::an;
                            }
                        }
                        if (n && sos == bidi::al) {
                            bool strong = false;
                            for (size_t j = k; j > 0;) {
                                --j;
                                auto t = type(j);
                                if (t == bidi::l || t == bidi::r || t == bidi::al) {
                                    strong = true;
                                    break;
                                }
                            }
                            if (!strong) {
                                type(k) = bidi::an;
                            }
                        }
                    }
                }
                // W3: an Arabic letter is a right to left one from here on
                for (size_t k = 0; k < n; ++k) {
                    if (type(k) == bidi::al) {
                        type(k) = bidi::r;
                    }
                }
                // W4: one separator between two numbers of a kind
                for (size_t k = 1; k + 1 < n; ++k) {
                    if (type(k) == bidi::es && type(k - 1) == bidi::en && type(k + 1) == bidi::en) {
                        type(k) = bidi::en;
                    } else if (type(k) == bidi::cs && type(k - 1) == type(k + 1)
                               && (type(k - 1) == bidi::en || type(k - 1) == bidi::an)) {
                        type(k) = type(k - 1);
                    }
                }
                // W5: a run of terminators beside a European number
                for (size_t k = 0; k < n; ++k) {
                    if (type(k) != bidi::et) {
                        continue;
                    }
                    size_t j = k;
                    while (j < n && type(j) == bidi::et) {
                        ++j;
                    }
                    bool before = k > 0 && type(k - 1) == bidi::en;
                    bool after = j < n && type(j) == bidi::en;
                    if (before || after) {
                        for (size_t m = k; m < j; ++m) {
                            type(m) = bidi::en;
                        }
                    }
                    k = j - 1;
                }
                // W6: what is left of the separators and terminators is neutral
                for (size_t k = 0; k < n; ++k) {
                    if (type(k) == bidi::es || type(k) == bidi::et || type(k) == bidi::cs) {
                        type(k) = bidi::on;
                    }
                }
                // W7: a European number after a left to right letter is one
                for (size_t k = 0; k < n; ++k) {
                    if (type(k) == bidi::en) {
                        bidi strong = sos;
                        for (size_t j = k; j > 0;) {
                            --j;
                            if (type(j) == bidi::l || type(j) == bidi::r) {
                                strong = type(j);
                                break;
                            }
                        }
                        if (strong == bidi::l) {
                            type(k) = bidi::l;
                        }
                    }
                }
                _brackets(s, sos, level);
                // N1, N2: a run of neutrals between two of a kind takes
                // that kind, and otherwise the direction of the paragraph
                for (size_t k = 0; k < n; ++k) {
                    if (!_neutral(type(k))) {
                        continue;
                    }
                    size_t j = k;
                    while (j < n && _neutral(type(j))) {
                        ++j;
                    }
                    bidi before = k ? _strong_of(type(k - 1)) : sos;
                    bidi after = j < n ? _strong_of(type(j)) : eos;
                    bidi value = before == after && (before == bidi::l || before == bidi::r)
                               ? before : (level % 2 ? bidi::r : bidi::l);
                    for (size_t m = k; m < j; ++m) {
                        type(m) = value;
                    }
                    k = j - 1;
                }
                // I1, I2: the levels the types ask for
                for (size_t k = 0; k < n; ++k) {
                    auto t = type(k);
                    uint8_t& l = _levels[s[k]];
                    if (level % 2 == 0) {
                        if (t == bidi::r) {
                            l = uint8_t(level + 1);
                        } else if (t == bidi::an || t == bidi::en) {
                            l = uint8_t(level + 2);
                        }
                    } else if (t == bidi::l || t == bidi::an || t == bidi::en) {
                        l = uint8_t(level + 1);
                    }
                }
            }

            static constexpr bool _neutral(bidi t) noexcept {
                return t == bidi::b || t == bidi::s || t == bidi::ws || t == bidi::on
                    || t == bidi::fsi || t == bidi::lri || t == bidi::rli || t == bidi::pdi;
            }

            static constexpr bidi _strong_of(bidi t) noexcept {
                return t == bidi::en || t == bidi::an ? bidi::r : t;
            }

            // N0: a pair of brackets takes one direction, and the text
            // inside it does not pull them apart
            void _brackets(const scratch_vector<size_t>& s, bidi sos, uint8_t level) noexcept {
                auto& stack = _scratch->brackets;
                auto& pairs = _scratch->pairs;
                stack.clear();
                pairs.clear();
                for (size_t k = 0; k < s.size(); ++k) {
                    if (_types[s[k]] != bidi::on) {
                        continue;
                    }
                    auto b = bracket_of(_points[s[k]]);
                    if (!b) {
                        continue;
                    }
                    if (b->opens) {
                        if (stack.size() == 63) {
                            return;                       // BD16: the stack is that deep and no deeper
                        }
                        stack.push_back({bracket_canonical(b->other), k});
                    } else {
                        char32_t here = bracket_canonical(_points[s[k]]);
                        for (size_t d = stack.size(); d > 0;) {
                            --d;
                            if (stack[d].closing == here) {
                                pairs.push_back({stack[d].at, k});
                                stack.resize(d);
                                break;
                            }
                        }
                    }
                }
                std::sort(pairs.begin(), pairs.end(), [](const bidi_run& a, const bidi_run& b) {
                    return a.first != b.first ? a.first < b.first : a.last < b.last;
                });
                bidi embedding = level % 2 ? bidi::r : bidi::l;
                for (auto [from, to] : pairs) {
                    bool strong_embedding = false, strong_other = false;
                    for (size_t k = from + 1; k < to; ++k) {
                        bidi t = _strong_of(_types[s[k]]);
                        if (t != bidi::l && t != bidi::r) {
                            continue;
                        }
                        (t == embedding ? strong_embedding : strong_other) = true;
                    }
                    bidi value = bidi::on;
                    if (strong_embedding) {
                        value = embedding;                 // N0 b
                    } else if (strong_other) {
                        // N0 c: what stands before the pair decides
                        bidi before = sos;
                        for (size_t k = from; k > 0;) {
                            --k;
                            bidi t = _strong_of(_types[s[k]]);
                            if (t == bidi::l || t == bidi::r) {
                                before = t;
                                break;
                            }
                        }
                        value = before == (embedding == bidi::l ? bidi::r : bidi::l)
                              ? (embedding == bidi::l ? bidi::r : bidi::l) : embedding;
                    }
                    if (value == bidi::on) {
                        continue;                          // N0 d: left to the rules that follow
                    }
                    _types[s[from]] = value;
                    _types[s[to]] = value;
                    // the marks that follow a bracket go with it
                    for (size_t k = from + 1; k < s.size() && _initial[s[k]] == bidi::nsm; ++k) {
                        _types[s[k]] = value;
                    }
                    for (size_t k = to + 1; k < s.size() && _initial[s[k]] == bidi::nsm; ++k) {
                        _types[s[k]] = value;
                    }
                }
            }

            // X10: the direction outside an end of the sequence
            bidi _side(size_t at, uint8_t level, bool before) const noexcept {
                uint8_t other = _level;
                if (before) {
                    for (size_t i = at; i > _from;) {
                        --i;
                        if (!is_removed_by_x9(_initial[i])) {
                            other = _explicit[i];
                            break;
                        }
                    }
                } else if (!is_isolate_initiator(_initial[at]) || _matching_pdi(at, _to) != _to) {
                    other = _level;
                    for (size_t i = at + 1; i < _to; ++i) {
                        if (!is_removed_by_x9(_initial[i])) {
                            other = _explicit[i];
                            break;
                        }
                    }
                }
                uint8_t higher = std::max(level, other);
                return higher % 2 ? bidi::r : bidi::l;
            }

            // L1: a separator, and the white space before it or at the
            // end, goes back to the level of the paragraph
            void _reset_whitespace() noexcept {
                auto resettable = [&](size_t i) {
                    auto t = _initial[i];
                    return t == bidi::ws || is_isolate_initiator(t) || t == bidi::pdi
                        || is_removed_by_x9(t);
                };
                size_t from = _from, to = _to;
                uint8_t level = _level;
                for (size_t i = from; i < to; ++i) {
                    auto t = _initial[i];
                    if (t == bidi::s || t == bidi::b) {
                        _levels[i] = level;
                        for (size_t j = i; j > from;) {
                            --j;
                            if (!resettable(j)) {
                                break;
                            }
                            _levels[j] = level;
                        }
                    }
                }
                for (size_t i = to; i > from;) {
                    --i;
                    if (!resettable(i)) {
                        break;
                    }
                    _levels[i] = level;
                }
            }

            lent<bidi_scratch> _scratch;
            scratch_vector<char32_t>& _points;
            scratch_vector<size_t>& _at;
            scratch_vector<bidi>& _initial;
            scratch_vector<bidi>& _types;
            scratch_vector<uint8_t>& _levels;
            scratch_vector<uint8_t>& _explicit;    // what X1 to X8 set, before I1 and I2
            scratch_vector<bidi_paragraph>& _paragraphs;
            // the paragraph being resolved: its code points and its level
            size_t _from = 0;
            size_t _to = 0;
            uint8_t _level = 0;
        };
    }

    // The class of a code point: what the algorithm knows about it before
    // it looks at anything around it
    inline constexpr sgcl::detail::code_point_fn<detail::bidi_of> bidi_class_of {};

    // Whether the code point is drawn the other way round in a right to
    // left run — a parenthesis, a bracket, a chevron, a less-than sign,
    // an integral. Bidi_Mirrored, which is the property rule L4 asks
    // about, and which holds of more code points than have a mirror of
    // their own.
    inline constexpr sgcl::detail::code_point_fn<detail::is_mirrored_fn> is_mirrored {};

    // The code point of the mirrored shape — '(' answers ')', '≤'
    // answers '≥' — or the code point itself where the shape has no
    // code point of its own, which is what BidiMirroring.txt leaves out
    // and what a font draws by reflecting the glyph. A mirroring is its
    // own inverse wherever the file gives one.
    inline constexpr sgcl::detail::code_point_fn<detail::mirrored_of_fn> mirrored_of {};

    // Which way the paragraph runs, by its first strong character. A
    // paragraph of numbers and punctuation alone runs left to right.
    inline direction paragraph_direction(const string& text) noexcept {
        detail::paragraph p(text.view(), direction::automatic);
        return p.level() % 2 ? direction::right_to_left : direction::left_to_right;
    }

    // The level of every code point of the text: even runs left to right,
    // odd right to left. What a renderer needs when it lays the text out
    // itself; bidi_runs is the same thing already cut into pieces.
    inline vector<uint8_t> levels(const string& text, direction paragraph = direction::automatic) noexcept {
        detail::paragraph p(text.view(), paragraph);
        auto& levels = p.levels();
        return vector<uint8_t>(levels.begin(), levels.end());
    }

    namespace detail {
        // Rule L4 over a text whose levels are already worked out. Both
        // forms of mirrored() below come through here, and the loop
        // decodes rather than taking the paragraph's code points, so
        // that a caller who has only the levels needs nothing else.
        // (Every mirrored code point has its original's width in UTF-8,
        // so the text keeps its length and no string can grow too long.)
        inline string mirror_text(const string& text, const uint8_t* levels, size_t count) noexcept {
            auto v = text.view();
            // The width of the answer is the sum of the widths, and a
            // mirror need not be as wide as what it stands for: nothing
            // may be counted in characters here. This pass only asks,
            // and over a text with nothing to mirror — which is most of
            // them — it is the whole of the work.
            ptrdiff_t grew = 0;
            size_t changed = 0;
            for (size_t i = 0, k = 0; i < v.size(); ++k) {
                auto [c, n] = utf8::decode(v, i);
                char32_t m = k < count && (levels[k] & 1) ? mirrored_of_fn(c) : c;
                if (m != c) {
                    ++changed;
                    grew += ptrdiff_t(utf8::width(m)) - ptrdiff_t(n);
                }
                i += n;
            }
            if (!changed) {
                return text;
            }
            // written in place into the string's object
            return sgcl::detail::StringAccess::bounded<string>(size_t(ptrdiff_t(v.size()) + grew), [&](char* chars) {
                char* w = chars;
                size_t done = 0;                 // how much of the text is already copied
                for (size_t i = 0, k = 0; i < v.size(); ++k) {
                    auto [c, n] = utf8::decode(v, i);
                    char32_t m = k < count && (levels[k] & 1) ? mirrored_of_fn(c) : c;
                    if (m != c) {
                        // the run of bytes before it, as they stand, and
                        // then the mirror in place of the character itself
                        sgcl::detail::copy_bytes(w, v.data() + done, i - done);
                        w += i - done;
                        w += utf8::encode(m, w);
                        done = i + n;
                    }
                    i += n;
                }
                sgcl::detail::copy_bytes(w, v.data() + done, v.size() - done);
                w += v.size() - done;
                return size_t(w - chars);
            });
        }
    }

    // The text with rule L4 applied: every character whose resolved level
    // is odd and whose Bidi_Mirrored property is yes swapped for the code
    // point of its mirrored shape, the rest left alone and the whole kept
    // in the order it is stored in. A rasteriser that is handed this and
    // the pieces of bidi_runs has everything it needs — the levels say
    // which pieces to turn round and this says which glyphs to change;
    // neither can be done without the other and a bracket in Arabic would
    // otherwise point the wrong way.
    //
    // A text with nothing to mirror comes back as the object it went in
    // as, which a shared and immutable string is worth holding on to.
    // Most texts are such texts: the first pass over it only asks.
    inline string mirrored(const string& text, direction paragraph = direction::automatic) noexcept {
        detail::paragraph p(text.view(), paragraph);
        return detail::mirror_text(text, p.levels().data(), p.levels().size());
    }

    // The same with the levels already in hand, which whoever draws the
    // text has: bidi_runs and levels() both work the paragraph out, and
    // there is no reason to work it out a second time. The levels are
    // one to a code point, as levels() gives them; a code point the
    // vector does not reach is left where it stands.
    inline string mirrored(const string& text, const vector<uint8_t>& levels) noexcept {
        return detail::mirror_text(text, levels.data(), levels.size());
    }

    // The byte position of every code point in the order it is drawn,
    // the ones rule X9 removes left out — what a caret moving through
    // mixed text steps over, and what a renderer that lays out character
    // by character walks
    inline vector<size_t> visual_order(const string& text, direction paragraph = direction::automatic) noexcept {
        detail::paragraph p(text.view(), paragraph);
        auto& at = p.positions();
        auto& order = p.order();
        vector<size_t> out;
        out.reserve(order.size());
        for (auto i : order) {
            out.push_back(at[i]);
        }
        return out;
    }

    // bidi_runs: the pieces of the text in the order they are drawn, each
    // with the level it runs at. A renderer draws them one after another
    // from left to right and turns the characters of an odd piece round;
    // nothing is copied, every piece being a slice of the text.
    class bidi_runs
    : public mixin::enumerable<bidi_runs> {
    public:
        struct run {
            slice<const char> text;
            uint8_t level = 0;

            bool right_to_left() const noexcept {
                return level % 2 != 0;
            }
        };

        using value_type = run;
        using size_type = size_t;

        bidi_runs() noexcept = default;

        // A copy is the runs again; runs moved from are the empty ones,
        // as the ones made with nothing
        bidi_runs(const bidi_runs&) = default;
        bidi_runs& operator=(const bidi_runs&) = default;

        bidi_runs(bidi_runs&& other) noexcept
        : _text(other._text)
        , _runs(std::move(other._runs))
        , _level(other._level) {
            other._reset();
        }

        bidi_runs& operator=(bidi_runs&& other) noexcept {
            if (this != &other) {
                _text = other._text;
                _runs = std::move(other._runs);
                _level = other._level;
                other._reset();
            }
            return *this;
        }

        explicit bidi_runs(const string& text, direction paragraph = direction::automatic) noexcept
        : bidi_runs(text.as_slice(), paragraph) {
        }

        // A C text — a literal among them, which a string and a slice
        // would both take — as detail::c_text reads it, copied into a
        // string the runs hold
        template<size_t N>
        explicit bidi_runs(const char (&text)[N], direction paragraph = direction::automatic)
        : bidi_runs(detail::c_string(text).as_slice(), paragraph) {
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        explicit bidi_runs(P text, direction paragraph = direction::automatic)
        : bidi_runs(detail::c_string(text).as_slice(), paragraph) {
        }

        explicit bidi_runs(const slice<const char>& text, direction paragraph = direction::automatic) noexcept
        : _text(text) {
            detail::paragraph p(text.view(), paragraph);
            _level = p.level();
            auto& order = p.order();
            auto& levels = p.levels();
            auto& at = p.positions();
            auto end_of = [&](size_t i) {
                return i + 1 < p.size() ? at[i + 1] : text.size();
            };
            // a piece is as long as the characters stay next to each
            // other in the text and at one level; the pieces are found
            // first and the vector made once at their count, where
            // growing it one piece at a time left a managed buffer behind
            // at every doubling
            // A piece ends where its paragraph does, each being a line
            // of its own: it is looked for inside the paragraph's stretch
            // of the order
            auto piece_end = [&](size_t k, size_t end) {
                size_t j = k;
                uint8_t l = levels[order[k]];
                while (j + 1 < end && levels[order[j + 1]] == l
                       && (l % 2 ? order[j + 1] + 1 == order[j] : order[j] + 1 == order[j + 1])) {
                    ++j;
                }
                return j;
            };
            auto& paragraphs = p.paragraphs();
            size_t pieces = 0;
            for (size_t k = 0; const auto& para : paragraphs) {
                for (; k < para.order_end; k = piece_end(k, para.order_end) + 1) {
                    ++pieces;
                }
            }
            _runs.reserve(pieces);
            for (size_t k = 0; const auto& para : paragraphs) {
                while (k < para.order_end) {
                    size_t j = piece_end(k, para.order_end);
                    uint8_t l = levels[order[k]];
                    size_t from = l % 2 ? order[j] : order[k];
                    size_t to = l % 2 ? order[k] : order[j];
                    _runs.push_back({text.subslice(at[from], end_of(to) - at[from]), l});
                    k = j + 1;
                }
            }
        }

        auto begin() const noexcept {
            return _runs.begin();
        }

        auto end() const noexcept {
            return _runs.end();
        }

        bool empty() const noexcept {
            return _runs.empty();
        }

        size_type count() const noexcept {
            return _runs.size();
        }

        // What the paragraph as a whole runs as
        direction paragraph() const noexcept {
            return _level % 2 ? direction::right_to_left : direction::left_to_right;
        }

        const slice<const char>& text() const noexcept {
            return _text;
        }

    private:
        void _reset() noexcept {
            _text = slice<const char>();
            _runs = vector<run>();
            _level = 0;
        }

        slice<const char> _text;
        vector<run> _runs;
        uint8_t _level = 0;
    };
}
