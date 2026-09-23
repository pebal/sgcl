//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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

        // BD16 says two brackets are a pair when they are canonically the
        // same, so U+2329 pairs with U+3009 as well as with U+232A
        inline char32_t bracket_canonical(char32_t c) noexcept {
            auto d = decomposition_of(c, normalize_tables::CanonicalDecomposition);
            return d && d.size == 1 ? char32_t(d.units[0]) : c;
        }

        inline constexpr uint8_t MaxDepth = 125;

        // The whole of the algorithm over one paragraph. The text is kept
        // as code points, because every rule of it looks left and right
        // and does so many times.
        class paragraph {
        public:
            paragraph(std::string_view text, direction ask) {
                for (size_t i = 0; i < text.size();) {
                    auto [c, n] = utf8::decode(text, i);
                    _points.push_back(c);
                    _at.push_back(i);
                    i += n;
                }
                _initial.reserve(_points.size());
                for (auto c : _points) {
                    _initial.push_back(bidi_of(c));
                }
                _types = _initial;
                _levels.assign(_points.size(), 0);
                _level = ask == direction::left_to_right ? 0
                       : ask == direction::right_to_left ? 1
                       : _first_strong(0, _points.size());
                _explicit_levels();
                // X10 and the runs are built from the levels X1 to X8
                // set, not from the ones I1 and I2 go on to change: a
                // sequence resolved early would otherwise decide what the
                // next one sees on its side
                _explicit = _levels;
                _sequences();
                _reset_whitespace();
            }

            uint8_t level() const noexcept {
                return _level;
            }

            const vector<uint8_t>& levels() const noexcept {
                return _levels;
            }

            const vector<size_t>& positions() const noexcept {
                return _at;
            }

            bool removed(size_t i) const noexcept {
                return is_removed_by_x9(_initial[i]);
            }

            size_t size() const noexcept {
                return _points.size();
            }

            // L2: the characters in the order they are drawn, the ones
            // X9 removed left out
            vector<size_t> order() const {
                vector<size_t> out;
                for (size_t i = 0; i < _points.size(); ++i) {
                    if (!removed(i)) {
                        out.push_back(i);
                    }
                }
                uint8_t highest = 0;
                uint8_t lowest_odd = MaxDepth + 2;
                for (auto i : out) {
                    highest = std::max(highest, _levels[i]);
                    if (_levels[i] % 2) {
                        lowest_odd = std::min(lowest_odd, _levels[i]);
                    }
                }
                for (uint8_t l = highest; l >= lowest_odd && l > 0; --l) {
                    for (size_t i = 0; i < out.size();) {
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
                return out;
            }

        private:
            // P2, P3: the first strong character decides, and what stands
            // between an isolate initiator and its match does not count
            uint8_t _first_strong(size_t from, size_t to) const {
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
            size_t _matching_pdi(size_t at, size_t to) const {
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
            void _explicit_levels() {
                struct entry {
                    uint8_t level;
                    bidi override_status;   // on when there is none
                    bool isolate;
                };
                vector<entry> stack;
                stack.push_back({_level, bidi::on, false});
                unsigned overflow_isolate = 0, overflow_embedding = 0, valid_isolate = 0;
                for (size_t i = 0; i < _points.size(); ++i) {
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
                                rtl = _first_strong(i + 1, _matching_pdi(i, _points.size())) == 1;
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
            void _sequences() {
                vector<size_t> significant;
                for (size_t i = 0; i < _points.size(); ++i) {
                    if (!is_removed_by_x9(_initial[i])) {
                        significant.push_back(i);
                    }
                }
                if (significant.empty()) {
                    return;
                }
                // the level runs, over the characters X9 left
                vector<vector<size_t>> runs;
                for (size_t k = 0; k < significant.size();) {
                    size_t j = k;
                    uint8_t l = _explicit[significant[k]];
                    while (j < significant.size() && _explicit[significant[j]] == l) {
                        ++j;
                    }
                    runs.emplace_back(significant.begin() + ptrdiff_t(k), significant.begin() + ptrdiff_t(j));
                    k = j;
                }
                vector<bool> used(runs.size(), false);
                for (size_t r = 0; r < runs.size(); ++r) {
                    if (used[r]) {
                        continue;
                    }
                    // a sequence starts at a run whose first character is
                    // not a PDI that closes an isolate initiator
                    size_t first = runs[r].front();
                    if (_initial[first] == bidi::pdi && _opens_of(first) != _points.size()) {
                        continue;
                    }
                    vector<size_t> sequence;
                    size_t at = r;
                    for (;;) {
                        used[at] = true;
                        sequence.insert(sequence.end(), runs[at].begin(), runs[at].end());
                        size_t last = runs[at].back();
                        if (!is_isolate_initiator(_initial[last])) {
                            break;
                        }
                        size_t pdi = _matching_pdi(last, _points.size());
                        if (pdi == _points.size()) {
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

            size_t _run_of(const vector<vector<size_t>>& runs, size_t at) const {
                for (size_t i = 0; i < runs.size(); ++i) {
                    if (runs[i].front() == at) {
                        return i;
                    }
                }
                return runs.size();
            }

            // The isolate initiator this PDI closes, or the end
            size_t _opens_of(size_t pdi) const {
                int depth = 1;
                for (size_t i = pdi; i > 0;) {
                    --i;
                    if (_initial[i] == bidi::pdi) {
                        ++depth;
                    } else if (is_isolate_initiator(_initial[i]) && --depth == 0) {
                        return i;
                    }
                }
                return _points.size();
            }

            void _resolve(const vector<size_t>& s) {
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
            void _brackets(const vector<size_t>& s, bidi sos, uint8_t level) {
                struct open { char32_t closing; size_t at; };
                vector<open> stack;
                vector<std::pair<size_t, size_t>> pairs;
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
                                pairs.emplace_back(stack[d].at, k);
                                stack.resize(d);
                                break;
                            }
                        }
                    }
                }
                std::sort(pairs.begin(), pairs.end());
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
            bidi _side(size_t at, uint8_t level, bool before) const {
                uint8_t other = _level;
                if (before) {
                    for (size_t i = at; i > 0;) {
                        --i;
                        if (!is_removed_by_x9(_initial[i])) {
                            other = _explicit[i];
                            break;
                        }
                    }
                } else if (!is_isolate_initiator(_initial[at]) || _matching_pdi(at, _points.size()) != _points.size()) {
                    other = _level;
                    for (size_t i = at + 1; i < _points.size(); ++i) {
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
            void _reset_whitespace() {
                auto resettable = [&](size_t i) {
                    auto t = _initial[i];
                    return t == bidi::ws || is_isolate_initiator(t) || t == bidi::pdi
                        || is_removed_by_x9(t);
                };
                for (size_t i = 0; i < _points.size(); ++i) {
                    auto t = _initial[i];
                    if (t == bidi::s || t == bidi::b) {
                        _levels[i] = _level;
                        for (size_t j = i; j > 0;) {
                            --j;
                            if (!resettable(j)) {
                                break;
                            }
                            _levels[j] = _level;
                        }
                    }
                }
                for (size_t i = _points.size(); i > 0;) {
                    --i;
                    if (!resettable(i)) {
                        break;
                    }
                    _levels[i] = _level;
                }
            }

            vector<char32_t> _points;
            vector<size_t> _at;
            vector<bidi> _initial;
            vector<bidi> _types;
            vector<uint8_t> _levels;
            vector<uint8_t> _explicit;    // what X1 to X8 set, before I1 and I2
            uint8_t _level = 0;
        };
    }

    // The class of a code point: what the algorithm knows about it before
    // it looks at anything around it
    inline constexpr sgcl::detail::code_point_fn<detail::bidi_of> direction_of {};

    // Which way the paragraph runs, by its first strong character. A
    // paragraph of numbers and punctuation alone runs left to right.
    inline direction paragraph_direction(const string& text) {
        detail::paragraph p(text.view(), direction::automatic);
        return p.level() % 2 ? direction::right_to_left : direction::left_to_right;
    }

    // The level of every code point of the text: even runs left to right,
    // odd right to left. What a renderer needs when it lays the text out
    // itself; bidi_runs is the same thing already cut into pieces.
    inline vector<uint8_t> levels(const string& text, direction paragraph = direction::automatic) {
        return detail::paragraph(text.view(), paragraph).levels();
    }

    // The byte position of every code point in the order it is drawn,
    // the ones rule X9 removes left out — what a caret moving through
    // mixed text steps over, and what a renderer that lays out character
    // by character walks
    inline vector<size_t> visual_order(const string& text, direction paragraph = direction::automatic) {
        detail::paragraph p(text.view(), paragraph);
        auto& at = p.positions();
        vector<size_t> out;
        for (auto i : p.order()) {
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

        explicit bidi_runs(const string& text, direction paragraph = direction::automatic)
        : bidi_runs(text.as_slice(), paragraph) {
        }

        explicit bidi_runs(slice<const char> text, direction paragraph = direction::automatic)
        : _text(text) {
            detail::paragraph p(text.view(), paragraph);
            _level = p.level();
            auto order = p.order();
            auto& levels = p.levels();
            auto& at = p.positions();
            auto end_of = [&](size_t i) {
                return i + 1 < p.size() ? at[i + 1] : text.size();
            };
            for (size_t k = 0; k < order.size();) {
                size_t j = k;
                uint8_t l = levels[order[k]];
                // a piece is as long as the characters stay next to each
                // other in the text and at one level
                while (j + 1 < order.size() && levels[order[j + 1]] == l
                       && (l % 2 ? order[j + 1] + 1 == order[j] : order[j] + 1 == order[j + 1])) {
                    ++j;
                }
                size_t from = l % 2 ? order[j] : order[k];
                size_t to = l % 2 ? order[k] : order[j];
                _runs.push_back({text.subslice(at[from], end_of(to) - at[from]), l});
                k = j + 1;
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
        slice<const char> _text;
        vector<run> _runs;
        uint8_t _level = 0;
    };
}
