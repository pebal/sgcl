//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "case.h"
#include "detail/collate_tables.h"

#include <vector>

// Putting text in the order a reader expects, which is not the order the
// code points fall in. "Zebra" would come before "ada", "résumé" would
// land far from "resume", and a Polish list would put "łoś" after
// "zebra". The algorithm of UTS #10 gives every character weights at
// three levels — the letter, the accent, the case — and compares them a
// level at a time, so that a difference of letters settles it before an
// accent is looked at.
//
// The root order is the DUCET, which puts every letter of every alphabet
// somewhere. A language then moves a handful of them: Polish wants ą
// beside a, Danish wants å after z, Hungarian wants cs to be one letter
// between c and d. Those differences are what a locale asks for here.
namespace sgcl::txt {
    // How much of a difference counts. At primary strength "resume",
    // "résumé" and "RESUME" are one word, which is what a search wants;
    // at tertiary they are three, which is what a sorted list wants.
    enum class strength : uint8_t {
        primary,
        secondary,
        tertiary,
    };

    namespace detail {
        // A buffer on the stack, with the standard vector behind it for
        // what does not fit. Nothing it holds is a pointer — code points
        // and collation elements keep nothing alive — so the standard
        // vector is the right one here: the managed heap is for what the
        // collector has to know about, and a block issued zeroed and
        // left to the sweep is the wrong shape for a scratch buffer that
        // dies inside the call.
        //
        // The inline part is left uninitialised, so it costs a stack
        // frame and nothing else, and it is sized so that the text these
        // are asked about does not reach the vector at all: a comparison
        // of two sentences in place of a word is a third dearer when it
        // does.
        template<class T, size_t N>
        class small_vector {
        public:
            size_t size() const noexcept {
                return _size;
            }

            void clear() noexcept {
                _size = 0;
                _spill.clear();
            }

            void push_back(const T& x) {
                if (_size < N) {
                    _inline[_size] = x;
                } else {
                    _spill.push_back(x);
                }
                ++_size;
            }

            T& operator[](size_t i) noexcept {
                return i < N ? _inline[i] : _spill[i - N];
            }

            const T& operator[](size_t i) const noexcept {
                return i < N ? _inline[i] : _spill[i - N];
            }

            // Only while everything is still on the stack, which is the
            // case for any text the standard calls stream safe
            bool drop_front(size_t n) noexcept {
                if (!_spill.empty()) {
                    return false;
                }
                for (size_t i = n; i < _size; ++i) {
                    _inline[i - n] = _inline[i];
                }
                _size -= n;
                return true;
            }

        private:
            T _inline[N];          // left uninitialised on purpose: _size says
                                   // what has been written, and zeroing a
                                   // buffer this size shows up in a comparison
            size_t _size = 0;
            std::vector<T> _spill;
        };

        // The code points of a text in NFD, one combining sequence at a
        // time. The algorithm is defined on the decomposed form, but a
        // text does not have to be decomposed whole to be compared: two
        // words that differ in their first letter are settled before the
        // second is looked at. Nearly every sequence is one code point
        // and nearly all text is already decomposed, so the common path
        // decodes into the window and touches no table.
        // The code points of one combining sequence and the lookahead a
        // contraction needs: UAX #15 promises at most 30 non-starters in
        // stream-safe text and a tailored contraction is at most six
        // points, so the window holds any sequence whole
        inline constexpr size_t WindowPoints = 64;

        // A code point opens a new combining sequence when its combining
        // class is zero — except for three in the whole of Unicode whose
        // decomposition begins with a mark, so that they belong with the
        // sequence before them rather than starting one. The generator
        // asserts that these three are the only ones.
        //
        // It matters because the canonical order is settled one sequence
        // at a time here, and a sequence has to be complete before it is
        // read: "0FB2 1D165 0F81" decomposes to four code points of which
        // two come out of the 0F81, and they sort in front of the mark
        // that was written before them.
        constexpr bool joins_sequence(char32_t c) noexcept {
            return c == 0x0F73 || c == 0x0F75 || c == 0x0F81;
        }

        class nfd_window {
        public:
            explicit nfd_window(std::string_view text) noexcept
            : _text(text) {
            }

            // At least n code points are ready, or the text has ended
            bool ensure(size_t n) {
                while (_points.size() - _at < n && _more) {
                    _fill();
                }
                return _points.size() - _at >= n;
            }

            char32_t operator[](size_t i) const noexcept {
                return _points[_at + i];
            }

            bool taken(size_t i) const noexcept {
                return _taken[_at + i] != 0;
            }

            void take(size_t i) noexcept {
                _taken[_at + i] = 1;
            }

            // Whether everything filled has been consumed, and where the
            // reading has got to in the bytes: what the stream needs to
            // walk a plain letter without filling anything at all
            bool spent() const noexcept {
                return _points.size() == _at;
            }

            std::string_view text() const noexcept {
                return _text;
            }

            size_t byte() const noexcept {
                return _read;
            }

            void skip_bytes(size_t n) noexcept {
                _read += n;
            }

            void skip(size_t n) noexcept {
                _at += n;
                if (_at >= WindowPoints / 2) {
                    if (_points.drop_front(_at) && _taken.drop_front(_at)) {
                        _at = 0;
                    }
                }
            }

        private:
            // one combining sequence: a code point and the marks that
            // belong with it, taken apart and put in canonical order
            void _fill() {
                if (_read >= _text.size()) {
                    _more = false;
                    return;
                }
                size_t first = _points.size();
                auto [c, n] = utf8::decode(_text, _read);
                _read += n;
                decompose_into<false>(_points, c);
                while (_read < _text.size()) {
                    auto [mark, width] = utf8::decode(_text, _read);
                    if (ccc_fn(mark) == 0 && !joins_sequence(mark)) {
                        break;
                    }
                    _read += width;
                    decompose_into<false>(_points, mark);
                }
                canonical_order(_points, first);
                while (_taken.size() < _points.size()) {
                    _taken.push_back(0);
                }
            }

            std::string_view _text;
            size_t _read = 0;                                  // into the bytes
            size_t _at = 0;                                    // into the code points
            bool _more = true;
            small_vector<char32_t, WindowPoints> _points;
            small_vector<uint8_t, WindowPoints> _taken;
        };

        // The most elements one entry of any table can stand for
        inline constexpr size_t MaxElements = 24;

        // The elements of a text held while it is compared or keyed. A
        // letter is one or two of them, so this is a sentence: two of
        // these are live during a comparison, four kilobytes of stack,
        // and past them the buffer costs a third more a letter.
        inline constexpr size_t TextElements = 256;

        struct element_sink {
            Element* at;
            uint8_t count = 0;

            void push_back(const Element& e) noexcept {
                at[count++] = e;
            }
        };

        // Whether any contraction of the root begins with this code
        // point, and where its run of them starts. Both tables are
        // sorted, so neither is walked.
        constexpr size_t first_contraction(char32_t c) noexcept {
            size_t lo = 0, hi = std::size(collate_tables::Contractions);
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (collate_tables::Contractions[mid].first < c) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            return lo;
        }

        constexpr bool starts_contraction(char32_t c) noexcept {
            return in_set(c, collate_tables::ContractionStarter);
        }

        // A code point that stands for itself: it does not come apart,
        // no contraction begins with it, and it is not a mark that could
        // be put in another order. In ASCII only L and l begin a
        // contraction (the Catalan L·L) and nothing there decomposes or
        // combines, so the whole question is one comparison.
        inline bool self_contained(char32_t c) noexcept {
            if (c < 0x80) {
                return (c | 0x20) != U'l';
            }
            // That it comes apart does not matter to the root order:
            // every code point with a canonical decomposition has an
            // entry of its own in the table (the generator asserts it)
            // and, where no mark follows, the two roads give the same
            // weights — which is the only use those entries have, since
            // the text is otherwise decomposed before it is looked up. A
            // Hangul syllable is the exception: it comes apart by
            // arithmetic and has no entry of its own.
            return !is_hangul_syllable(c)
                && !joins_sequence(c)
                && ccc_fn(c) == 0
                && !starts_contraction(c);
        }

        // Whether what follows would join the letter before it — a mark,
        // or one of the three code points whose decomposition begins with
        // one. This is the condition of UAX #15's FCD: where no mark
        // follows, a letter cannot be reordered and needs no decomposing,
        // whatever form the text arrived in.
        inline bool mark_ahead(std::string_view text, size_t at) noexcept {
            if (at >= text.size() || uint8_t(text[at]) < 0x80) {
                return false;                     // ASCII is never a mark
            }
            char32_t c = utf8::decode(text, at).first;
            return ccc_fn(c) != 0 || joins_sequence(c);
        }

        // 10.1.3: the ideographs and everything unassigned weigh by
        // arithmetic rather than by a table, which is why a file that
        // covers the whole of Unicode has forty thousand entries and not
        // a million. Three bases, in the order they sort: the ideographs
        // of the two blocks that come first, the ideographs of the
        // extensions, and everything else.
        inline void implicit_weights(element_sink& out, char32_t c) {
            uint32_t aaaa, bbbb;
            for (auto& r : collate_tables::Implicit) {
                if (c >= r.lo && c <= r.hi) {
                    // the distance is counted from the first range of
                    // that base: Tangut is written as two of them and the
                    // supplement weighs after the components, not beside
                    // the first ideograph
                    char32_t from = r.lo;
                    for (auto& x : collate_tables::Implicit) {
                        if (x.base == r.base) {
                            from = x.lo;
                            break;
                        }
                    }
                    out.push_back(element_of({r.base, DefaultSecondary, DefaultTertiary}));
                    out.push_back(element_of({uint16_t((c - from) | 0x8000), 0, 0}));
                    return;
                }
            }
            if (in_set(c, collate_tables::UnifiedIdeograph)) {
                aaaa = (in_set(c, collate_tables::CoreIdeograph) ? 0xFB40 : 0xFB80) + (c >> 15);
            } else {
                aaaa = 0xFBC0 + (c >> 15);
            }
            bbbb = (c & 0x7FFF) | 0x8000;
            out.push_back(element_of({uint16_t(aaaa), DefaultSecondary, DefaultTertiary}));
            out.push_back(element_of({uint16_t(bbbb), 0, 0}));
        }

        // The weights of one code point, from the ranges where they run
        // with it, from the index where they do not, and by arithmetic
        // where there is no table at all
        inline void weights_of(element_sink& out, char32_t c) {
            // the two-stage table answers in two reads: a weight of zero
            // means the code point is not one of the letters whose weight
            // runs with it, and the index below has it
            if (uint16_t primary = value_of(c, collate_tables::Primary)) {
                out.push_back(element_of({primary, DefaultSecondary, DefaultTertiary}));
                return;
            }
            // and the letters that are not plain — every letter with an
            // accent — through a table that says which row of the index
            // is theirs, where a binary search over all of them used to
            // be: 15.3 ns a letter against 0.7 for one that lands in the
            // ranges above
            if (uint16_t row = value_of(c, collate_tables::RunIndex)) {
                auto& r = collate_tables::Runs[row - 1];
                for (size_t i = 0; i < r.size; ++i) {
                    out.push_back(element_of(collate_tables::Pool[r.at + i]));
                }
                return;
            }
            implicit_weights(out, c);
        }

        // A contraction of two or three code points, or nothing. S2.1.1
        // to S2.1.3 let the second one stand further off, with combining
        // marks of a lower class between, so "a" plus a cedilla plus an
        // acute finds the contraction of "a" and the acute.
        struct contraction_match {
            const Contraction* entry = nullptr;
            size_t used_second = 0;
            size_t used_third = 0;

            size_t points() const noexcept {
                return entry ? (entry->third ? 3 : 2) : 0;
            }
        };

        inline contraction_match match_contraction(nfd_window& points) {
            contraction_match best;
            if (!starts_contraction(points[0])) {
                return best;
            }
            for (size_t row = first_contraction(points[0]);
                 row < std::size(collate_tables::Contractions)
                 && collate_tables::Contractions[row].first == points[0]; ++row) {
                auto& c = collate_tables::Contractions[row];
                // the second code point, itself or past marks of a lower class
                size_t second = 0;
                uint8_t last_class = 0;
                for (size_t i = 1; points.ensure(i + 1); ++i) {
                    uint8_t cc = ccc_fn(points[i]);
                    if (points[i] == c.second && (i == 1 || cc > last_class)) {
                        second = i;
                        break;
                    }
                    if (cc == 0) {
                        break;
                    }
                    last_class = cc;
                }
                if (!second) {
                    continue;
                }
                size_t third = 0;
                if (c.third) {
                    last_class = 0;
                    for (size_t i = second + 1; points.ensure(i + 1); ++i) {
                        uint8_t cc = ccc_fn(points[i]);
                        if (points[i] == c.third && (i == second + 1 || cc > last_class)) {
                            third = i;
                            break;
                        }
                        if (cc == 0) {
                            break;
                        }
                        last_class = cc;
                    }
                    if (!third) {
                        continue;
                    }
                }
                if (!best.entry
                    || (third ? third : second) > (best.used_third ? best.used_third : best.used_second)) {
                    best = {&c, second, third};
                }
            }
            return best;
        }

        //----------------------------------------------------------------
        // the tailorings: where a language parts from the root order
        //----------------------------------------------------------------
        inline const Tailoring* tailoring_of(locale where) noexcept {
            uint32_t language = where.subtag();
            if (!language) {
                return nullptr;
            }
            size_t lo = 0, hi = std::size(collate_tables::Tailorings);
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (collate_tables::Tailorings[mid].language < language) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            return lo < std::size(collate_tables::Tailorings)
                && collate_tables::Tailorings[lo].language == language
                 ? collate_tables::Tailorings + lo : nullptr;
        }

        // The rows of a tailoring are sorted by the code points they
        // stand for, so the ones beginning with a given code point are a
        // run and a search finds where it starts
        template<class Row, class First>
        constexpr size_t first_row(const Row* rows, size_t lo, size_t hi, char32_t c, First first) noexcept {
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (first(rows[mid]) < c) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            return lo;
        }

        inline bool same_points(nfd_window& points, uint16_t pool_at, size_t size) {
            if (!points.ensure(size)) {
                return false;
            }
            for (size_t i = 0; i < size; ++i) {
                if (points[i] != collate_tables::TailoringPoints[pool_at + i]) {
                    return false;
                }
            }
            return true;
        }

        // What the language makes of the code points from here on: how
        // many of them it takes, and where the elements it gives them
        // are. The longest match wins, so Hungarian's "dzs" beats its
        // "dz" — and the root's own contractions are longest-matched
        // against this one rather than passed over, since a tailoring
        // changes the root table and does not replace it.
        struct tailored_match {
            size_t size = 0;
            const TailoredSequence* sequence = nullptr;
            const TailoredExpansion* expansion = nullptr;
            const TailoredPoint* point = nullptr;
        };

        inline tailored_match tailored(nfd_window& points, const Tailoring& language) {
            using namespace collate_tables;
            tailored_match best;
            char32_t c = points[0];
            // A language moves a handful of letters, and the three
            // searches below are made for every character of every text
            // to find that out. The mask settles most of them here: a
            // bit that is clear says no entry of this language begins
            // with a code point of that residue.
            if (!((language.starters >> (c & 63)) & 1)) {
                return best;
            }

            size_t last = size_t(language.sequences_at) + language.sequences_size;
            for (size_t i = first_row(TailoredSequences, language.sequences_at, last, c,
                                      [](const TailoredSequence& r) { return TailoringPoints[r.at]; });
                 i < last && TailoringPoints[TailoredSequences[i].at] == c; ++i) {
                auto& row = TailoredSequences[i];
                if (row.size > best.size && same_points(points, row.at, row.size)) {
                    best = {row.size, &row, nullptr, nullptr};
                }
            }
            last = size_t(language.expansions_at) + language.expansions_size;
            for (size_t i = first_row(TailoredExpansions, language.expansions_at, last, c,
                                      [](const TailoredExpansion& r) { return TailoringPoints[r.points_at]; });
                 i < last && TailoringPoints[TailoredExpansions[i].points_at] == c; ++i) {
                auto& row = TailoredExpansions[i];
                if (row.points_size > best.size && same_points(points, row.points_at, row.points_size)) {
                    best = {row.points_size, nullptr, &row, nullptr};
                }
            }
            if (best.size) {
                return best;
            }

            // and one code point on its own
            last = size_t(language.points_at) + language.points_size;
            size_t point = first_row(TailoredPoints, language.points_at, last, c,
                                     [](const TailoredPoint& r) { return r.cp; });
            if (point < last && TailoredPoints[point].cp == c) {
                return {1, nullptr, nullptr, TailoredPoints + point};
            }
            return {};
        }

        inline void put_tailored(element_sink& out, const tailored_match& m) {
            if (m.sequence) {
                out.push_back({m.sequence->primary, m.sequence->secondary, m.sequence->tertiary});
            } else if (m.expansion) {
                for (size_t k = 0; k < m.expansion->size; ++k) {
                    out.push_back(collate_tables::TailoringElements[m.expansion->at + k]);
                }
            } else {
                out.push_back({m.point->primary, m.point->secondary, m.point->tertiary});
            }
        }

        // The same question asked of the bytes rather than of the
        // window, for the fast path: how many bytes the language takes
        // and what it gives them. It may be asked only where the text
        // needs no decomposing — which is what the caller has just
        // established — so the code points are read as they are written.
        struct tailored_bytes {
            tailored_match match;
            size_t bytes = 0;
        };

        inline bool same_bytes(std::string_view text, size_t at, uint16_t pool_at, size_t size,
                               size_t& bytes) {
            bytes = 0;
            for (size_t i = 0; i < size; ++i) {
                if (at + bytes >= text.size()) {
                    return false;
                }
                auto [c, n] = utf8::decode(text, at + bytes);
                if (c != collate_tables::TailoringPoints[pool_at + i]) {
                    return false;
                }
                bytes += n;
            }
            return true;
        }

        // Whether a stretch of text holds a mark, which could have been
        // written out of canonical order and would then have to be put
        // right before it is weighed. Where there is none, the bytes say
        // what the decomposed form would say.
        inline bool marks_inside(std::string_view text, size_t at, size_t bytes) noexcept {
            for (size_t i = at; i < at + bytes;) {
                auto [c, n] = utf8::decode(text, i);
                if (ccc_fn(c) != 0 || joins_sequence(c)) {
                    return true;
                }
                i += n;
            }
            return false;
        }

        // What a language makes of one code point, asked of the bytes
        // rather than of the window. Only the single code points are
        // read here: a rule over several of them is matched against the
        // decomposed form, where the marks are in canonical order and a
        // mark may stand further off than it is written (S2.1.1), and
        // neither is true of the bytes. Where a rule of several could
        // begin, this says so and the window takes over.
        inline const TailoredPoint* tailored_one(char32_t c, const Tailoring& language) noexcept {
            using namespace collate_tables;
            size_t last = size_t(language.sequences_at) + language.sequences_size;
            size_t row = first_row(TailoredSequences, language.sequences_at, last, c,
                                   [](const TailoredSequence& r) { return TailoringPoints[r.at]; });
            if (row < last && TailoringPoints[TailoredSequences[row].at] == c) {
                return nullptr;
            }
            last = size_t(language.expansions_at) + language.expansions_size;
            row = first_row(TailoredExpansions, language.expansions_at, last, c,
                            [](const TailoredExpansion& r) { return TailoringPoints[r.points_at]; });
            if (row < last && TailoringPoints[TailoredExpansions[row].points_at] == c) {
                return nullptr;
            }
            last = size_t(language.points_at) + language.points_size;
            row = first_row(TailoredPoints, language.points_at, last, c,
                            [](const TailoredPoint& r) { return r.cp; });
            return row < last && TailoredPoints[row].cp == c ? TailoredPoints + row : nullptr;
        }

        // The elements of a text, one at a time and no further than they
        // are asked for. This is what makes a comparison cheap: two words
        // that differ in their first letter cost one letter each.
        class element_stream {
        public:
            element_stream(std::string_view text, const Tailoring* language) noexcept
            : _points(text)
            , _language(language) {
            }

            bool next(Element& e) {
                while (_read == _count) {
                    if (!_fill()) {
                        return false;
                    }
                }
                e = _pending[_read++];
                return true;
            }

        private:
            bool _fill() {
                _read = 0;
                element_sink out{_pending};

                // The letter that stands for itself, with nothing after
                // it that could join it: its weights come straight out of
                // the tables and the window is not touched — nothing is
                // decomposed, no marks are gathered, no canonical order
                // is settled. Most text is a run of such letters.
                if (_points.spent()) {
                    auto text = _points.text();
                    size_t at = _points.byte();
                    if (at < text.size()) {
                        auto [c, n] = utf8::decode(text, at);
                        // and the language has nothing to say about it,
                        // which its mask answers in a shift. The rules
                        // are written on the decomposed form, so the
                        // generator closes each language canonically:
                        // the composed letters are keys of their own and
                        // a composed ż meets the rule that moves it.
                        if (self_contained(c) && !mark_ahead(text, at + n)) {
                            if (!_language || !((_language->starters >> (c & 63)) & 1)) {
                                weights_of(out, c);
                                _points.skip_bytes(n);
                                _count = out.count;
                                return true;
                            }
                            // the language may have a rule here, and
                            // one over a single code point is read the
                            // same way; anything longer belongs to the
                            // window
                            if (auto one = tailored_one(c, *_language)) {
                                out.push_back({one->primary, one->secondary, one->tertiary});
                                _points.skip_bytes(n);
                                _count = out.count;
                                return true;
                            }
                        }
                    }
                }

                // what a contraction has already taken is passed over
                while (_points.ensure(1) && _points.taken(0)) {
                    _points.skip(1);
                }
                if (!_points.ensure(1)) {
                    _count = 0;
                    return false;
                }
                // nothing is read ahead here: a contraction asks for the
                // code points it needs as it matches them, and a letter
                // that begins none asks for nothing
                tailored_match theirs;
                if (_language) {
                    theirs = tailored(_points, *_language);
                }
                auto root = match_contraction(_points);
                // the root's contraction is two or three code points, and
                // it stands only where the language has not claimed as
                // many of them itself
                if (theirs.size && theirs.size >= (root.entry ? root.points() : 1)) {
                    put_tailored(out, theirs);
                    _count = out.count;
                    _points.skip(theirs.size);
                    return true;
                }
                if (root.entry) {
                    for (size_t k = 0; k < root.entry->size; ++k) {
                        out.push_back(element_of(collate_tables::ContractionPool[root.entry->at + k]));
                    }
                    _points.take(root.used_second);
                    if (root.used_third) {
                        _points.take(root.used_third);
                    }
                    _count = out.count;
                    _points.skip(1);
                    return true;
                }
                weights_of(out, _points[0]);
                _count = out.count;
                _points.skip(1);
                return true;
            }

            nfd_window _points;
            const Tailoring* _language;
            Element _pending[MaxElements];
            uint8_t _count = 0;
            uint8_t _read = 0;
        };

        // The whole text at once, for a caller that needs every element:
        // the sort key does, a comparison does not
        inline void collation_elements(small_vector<Element, TextElements>& out, const string& text,
                                       const Tailoring* language) {
            element_stream stream(text.view(), language);
            Element e;
            while (stream.next(e)) {
                out.push_back(e);
            }
        }
    }

    // A collator puts texts in order. It holds the locale it was made
    // with and how much of a difference counts; it is a value, cheap to
    // copy, and it may be used as the comparator of a sorted container or
    // of a sort.
    class collator {
    public:
        collator() noexcept = default;

        explicit collator(locale where, strength level = strength::tertiary) noexcept
        : _tailoring(detail::tailoring_of(where))
        , _locale(where)
        , _strength(level) {
        }

        explicit collator(strength level) noexcept
        : _strength(level) {
        }

        // Negative when a comes first, zero when the texts are one and
        // the same to this collator.
        //
        // The first level is walked through both texts side by side and
        // the walk stops at the first difference, which in a sorted list
        // is nearly always the first letter: neither text is taken apart
        // any further than that. What has been read is kept, because the
        // second and third levels need it when the first says nothing.
        int compare(const string& a, const string& b) const {
            if (a == b) {
                return 0;
            }
            detail::element_stream x(a.view(), _tailoring);
            detail::element_stream y(b.view(), _tailoring);
            detail::small_vector<detail::Element, detail::TextElements> read_x, read_y;
            detail::Element e;
            for (;;) {
                uint32_t px = 0, py = 0;
                while (!px && x.next(e)) {
                    read_x.push_back(e);
                    px = e.primary;
                }
                while (!py && y.next(e)) {
                    read_y.push_back(e);
                    py = e.primary;
                }
                if (!px || !py) {
                    if (px != py) {
                        return px ? 1 : -1;
                    }
                    break;
                }
                if (px != py) {
                    return px < py ? -1 : 1;
                }
            }
            if (_strength == strength::primary) {
                return 0;
            }
            // the rest of both texts, which the first level did not need
            while (x.next(e)) {
                read_x.push_back(e);
            }
            while (y.next(e)) {
                read_y.push_back(e);
            }
            if (int d = _level(read_x, read_y, 1)) {
                return d;
            }
            if (_strength == strength::secondary) {
                return 0;
            }
            return _level(read_x, read_y, 2);
        }

        bool equal(const string& a, const string& b) const {
            return compare(a, b) == 0;
        }

        // A key that compares byte by byte the way the collator compares
        // texts: worth making once for a text that is sorted or looked up
        // many times, and worth storing in an index
        // A key that compares byte by byte the way the collator compares
        // texts: worth making once for a text that is sorted or looked up
        // many times, and worth storing in an index
        vector<std::byte> key(const string& text) const {
            elements weights;
            detail::collation_elements(weights, text, _tailoring);
            vector<std::byte> out(_key_size(weights));
            _write_key(weights, out.data());
            return out;
        }

        // The same key into a buffer the caller owns, which is what a
        // sort and an index builder want: there a key lives no longer
        // than the pass that uses it, and one allocation a word is the
        // whole cost. Returns the bytes the key takes; when that is more
        // than the buffer holds, nothing is written — a truncated key
        // would compare as a different text, which is worse than none —
        // so a caller may ask with an empty buffer first, or try again
        // with a larger one.
        size_t key(const string& text, slice<std::byte> buffer) const {
            elements weights;
            detail::collation_elements(weights, text, _tailoring);
            size_t size = _key_size(weights);
            if (size <= buffer.size()) {
                _write_key(weights, buffer.data());
            }
            return size;
        }

        // So that a collator may stand where a comparator is asked for
        bool operator()(const string& a, const string& b) const {
            return compare(a, b) < 0;
        }

        locale where() const noexcept {
            return _locale;
        }

        strength level() const noexcept {
            return _strength;
        }

        // Whether the library has an order of its own for that language,
        // or puts its text in the root order
        bool tailored() const noexcept {
            return _tailoring != nullptr;
        }

    private:
        // One level of the two element sequences, the zero weights of
        // that level passed over
        using elements = detail::small_vector<detail::Element, detail::TextElements>;

        // What a key of these weights takes: four bytes a letter, two an
        // accent, two a case, and two for each level boundary. The zero
        // weights are the ones the level passes over, and they are not
        // counted here for the same reason they are not written.
        size_t _key_size(const elements& weights) const noexcept {
            size_t primary = 0, secondary = 0, tertiary = 0;
            for (size_t i = 0; i < weights.size(); ++i) {
                primary += weights[i].primary ? 1 : 0;
                secondary += weights[i].secondary ? 1 : 0;
                tertiary += weights[i].tertiary ? 1 : 0;
            }
            if (_strength == strength::primary) {
                return primary * 4;
            }
            if (_strength == strength::secondary) {
                return primary * 4 + 2 + secondary * 2;
            }
            return primary * 4 + 2 + secondary * 2 + 2 + tertiary * 2;
        }

        void _write_key(const elements& weights, std::byte* out) const noexcept {
            auto put = [&out](uint32_t w, int bytes) {
                while (bytes--) {
                    *out++ = std::byte((w >> (8 * bytes)) & 0xFF);
                }
            };
            for (size_t i = 0; i < weights.size(); ++i) {
                if (weights[i].primary) {
                    put(weights[i].primary, 4);
                }
            }
            if (_strength == strength::primary) {
                return;
            }
            put(0, 2);
            for (size_t i = 0; i < weights.size(); ++i) {
                if (weights[i].secondary) {
                    put(weights[i].secondary, 2);
                }
            }
            if (_strength == strength::secondary) {
                return;
            }
            put(0, 2);
            for (size_t i = 0; i < weights.size(); ++i) {
                if (weights[i].tertiary) {
                    put(weights[i].tertiary, 2);
                }
            }
        }

        static int _level(const elements& x, const elements& y, int which) {
            auto at = [which](const detail::Element& w) -> uint32_t {
                return which == 1 ? w.secondary : w.tertiary;
            };
            size_t i = 0, j = 0;
            for (;;) {
                while (i < x.size() && !at(x[i])) {
                    ++i;
                }
                while (j < y.size() && !at(y[j])) {
                    ++j;
                }
                if (i == x.size() || j == y.size()) {
                    return i == x.size() && j == y.size() ? 0 : (i == x.size() ? -1 : 1);
                }
                if (at(x[i]) != at(y[j])) {
                    return at(x[i]) < at(y[j]) ? -1 : 1;
                }
                ++i;
                ++j;
            }
        }

        const detail::Tailoring* _tailoring = nullptr;
        locale _locale;
        strength _strength = strength::tertiary;
    };
}
