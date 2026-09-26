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
//
// A search by collation (section 8 of the algorithm) counts as equal
// whatever the collator counts as equal, so at primary strength
// "resume" finds "résumé". Weighing the text is the whole cost of it —
// more so than folding or decomposing one, since every letter goes
// through the tables — and that cost is the text's, not the pattern's.
// So both sides can be prepared, and the shapes are the ones search.h
// has for its two searches: a collated_searcher is a pattern weighed
// once, a collated_text is a text weighed once and asked many times,
// and collated_matches is every occurrence of one in the other with the
// text weighed a single time. collator::find, which weighs both on
// every call, is the short way to ask once and the wrong way to ask in
// a loop.
namespace sgcl::txt {
    // How much of a difference counts. At primary strength "resume",
    // "résumé" and "RESUME" are one word, which is what a search wants;
    // at tertiary they are three, which is what a sorted list wants.
    // The fourth level holds the punctuation that was shifted aside and
    // nothing else, so it is worth asking for only together with
    // punctuation::shifted, and then only to keep "re-sume" and "resume"
    // apart rather than merely together.
    enum class strength : uint8_t {
        primary,
        secondary,
        tertiary,
        quaternary,
    };

    // What becomes of punctuation, spaces and symbols — of what the
    // algorithm calls the variable elements. Counted, a hyphen is a
    // character like any other and "re-sume" is as far from "resume" as
    // "rezume" is. Shifted, it weighs nothing at the first three levels
    // and only at the fourth, so that a search for "resume" finds
    // "re-sume": look as though the hyphen were not there. CLDR calls
    // this `alternate` and Thai asks for it.
    enum class punctuation : uint8_t {
        counted,
        shifted,
    };

    // Which case comes first where the letters and the accents are the
    // same. The root puts the small letter first; Danish, Maltese and
    // Church Slavonic ask for the capital, and so do the lists a lawyer
    // reads. CLDR's `caseFirst`; `lower_first` is what the root does
    // anyway and is here so that a caller can say so over a language
    // that asks for the other.
    enum class case_order : uint8_t {
        natural,
        upper_first,
        lower_first,
    };

    // What a collator does beside putting the letters in order: the
    // settings CLDR names, under the names it gives them. They are a
    // value handed to the constructor rather than methods that change a
    // collator afterwards, because a collator is a comparator — it is
    // copied into a sort and shared between threads, and a setter would
    // make it a thing that can change under one of them. An aggregate
    // also reads at the call site the way the setting reads in CLDR:
    // collator{locale("da"), {.numeric = true}}.
    //
    // Four of them are things a language asks for, and those are the
    // four that are not set here: a collator made for a locale starts
    // with what that language's rules ask for, and what is written here
    // is what the caller wants instead. `numeric` is not among them
    // because no language asks for it — it is a thing a program wants
    // for its file names, never a thing a language wants for its words.
    struct options {
        txt::strength strength = txt::strength::tertiary;
        optional<txt::punctuation> punctuation;    // CLDR ka / alternate
        optional<txt::case_order> case_order;      // CLDR kf / caseFirst
        optional<bool> case_level;                 // CLDR kc / caseLevel
        optional<bool> backwards;                  // CLDR kb, the accents from the end
        bool numeric = false;                      // CLDR kn, file9 before file10
    };

    namespace detail {
        // The options as the elements are made and read: the questions
        // asked of every element, settled once when the collator is
        // built. `plain` is the answer to all of them at once, and it is
        // what the common collator is.
        struct shape {
            bool numeric = false;
            bool shifted = false;
            bool case_level = false;
            bool upper_first = false;
            bool backwards = false;
            // whether the case of a letter is read at all: it is where
            // it is a level of its own and where the capitals come
            // first, and nowhere else
            bool cased = false;

            constexpr bool operator==(const shape&) const noexcept = default;
        };

        // The heaviest weight the algorithm calls variable, in the space
        // the comparison works in
        inline constexpr uint32_t VariableLimit = uint32_t(VariableTop) << PrimaryShift;

        // A shifted element is marked in its third weight by a value
        // no weight of any table reaches — the root's largest is 0x1E
        // shifted up, a tailored one falls between two of the root's,
        // and where the case is read as well the top two bits are the
        // case and the weight is two bits shorter still. The mark says
        // that the element weighs nothing at the first three levels and
        // that what its first weight holds is the fourth level's weight
        // — the weight it had before it was shifted, or zero for what
        // follows a variable element and is ignorable itself.
        inline constexpr uint16_t ShiftedMark = 0xFFFF;

        inline bool is_shifted(const Element& e) noexcept {
            return e.tertiary == ShiftedMark;
        }

        // Everything that is not shifted weighs the same at the fourth
        // level, which is above any weight a variable element can carry
        inline constexpr uint32_t QuaternaryHigh = 0xFFFFFFFF;

        // The case of a letter, where a collator was asked about it.
        //
        // It is a property of the letter and not of its weights. The
        // root writes it into the third weight — the capitals in a band
        // above the small letters of the same kind — but a language
        // that moves a letter gives it a weight of its own between two
        // of the root's, and there the band says nothing: Danish "Œ"
        // and Maltese "Għ" come out of their rules with weights that
        // are in no band at all, and a collator that sorts the capitals
        // first has to see them for what they are. So the case is read
        // from the code points the letter is written with, as ICU reads
        // it, and carried in the two bits above the weight where the
        // weight is read at all. A letter written with both cases —
        // Danish sorts "Aa" as a letter — is neither, and sorts between
        // them, which is what ICU does with it.
        inline constexpr int CaseShift = 14;
        inline constexpr uint8_t CaseLower = 1;      // and everything with no case
        inline constexpr uint8_t CaseMixed = 2;      // a titlecase letter, or "Aa"
        inline constexpr uint8_t CaseUpper = 3;

        // The case of a letter, over the code points it is written with
        inline uint8_t case_rank(const char32_t* points, size_t size) noexcept {
            bool upper = false, lower = false, mixed = false;
            for (size_t i = 0; i < size; ++i) {
                auto what = category_of_fn(points[i]);
                if (what == category::titlecase_letter) {
                    mixed = true;
                } else if (what == category::uppercase_letter) {
                    upper = true;
                } else if (is_cased(points[i])) {
                    lower = true;
                }
            }
            if (mixed || (upper && lower)) {
                return CaseMixed;
            }
            return upper ? CaseUpper : CaseLower;
        }

        // The weight as the levels below the accents read it: the case
        // over the third weight, and the capitals first where they were
        // asked for. An element the third level passes over keeps its
        // zero, since it has no case either.
        inline uint16_t with_case(uint16_t tertiary, uint8_t rank, bool upper_first) noexcept {
            if (!tertiary) {
                return 0;
            }
            uint8_t where = upper_first ? uint8_t(CaseUpper + CaseLower - rank) : rank;
            return uint16_t((uint16_t(where) << CaseShift) | (tertiary >> 2));
        }

        // How the weights of an element are read. A collator that was
        // asked for none of the settings that touch them reads them as
        // they stand, and it says so once, in a template argument: the
        // questions below are then not asked at all rather than asked
        // and answered no, and the comparison and the key are the code
        // they were before any of this was here. Everything is correct
        // with Plain false; Plain true is only the promise that there is
        // nothing to do.
        template<bool Plain>
        inline uint32_t primary_of(const Element& e) noexcept {
            if constexpr (Plain) {
                return e.primary;
            } else {
                return is_shifted(e) ? 0 : e.primary;
            }
        }

        template<bool Plain>
        inline uint16_t secondary_of(const Element& e) noexcept {
            if constexpr (Plain) {
                return e.secondary;
            } else {
                return is_shifted(e) ? 0 : e.secondary;
            }
        }

        // The third weight as the options want it read, which is as it
        // stands: where the case was asked for, the stream has already
        // put it over the weight, so both the case level and the third
        // level read the one field.
        template<bool Plain>
        inline uint16_t tertiary_of(const Element& e) noexcept {
            if constexpr (Plain) {
                return e.tertiary;
            } else {
                return is_shifted(e) ? 0 : e.tertiary;
            }
        }

        // A level of its own for the case alone, between the accents
        // and the third level: the two bits the stream wrote, and zero
        // for an element that has no case. Only a letter has one. An
        // accent has none — it is the same mark over a capital and over
        // a small letter — and the level would otherwise say that
        // "côte" differs from "cote" in its case, which is what the
        // accents are for and not what this level is. So an element the
        // first level passes over is passed over here as well, which is
        // what ICU makes of it too.
        inline uint8_t case_of(const Element& e) noexcept {
            return is_shifted(e) || !e.primary ? 0 : uint8_t(e.tertiary >> CaseShift);
        }

        inline uint32_t quaternary_of(const Element& e) noexcept {
            if (is_shifted(e)) {
                return e.primary;             // what it weighed before it was shifted
            }
            return e.primary || e.secondary || e.tertiary ? QuaternaryHigh : 0;
        }

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
            small_vector() = default;

            // The inline part is left uninitialised past _size, so a
            // copy takes what has been written and not the whole array.
            // The compiler's own copy took all N of them, which for the
            // elements of a pattern is two kilobytes of indeterminate
            // bytes copied to carry a handful — and where such a copy
            // lands inside a managed object, as it did when a range
            // kept a prepared searcher, the collector reads those bytes
            // as words and a stale address among them is a root that
            // keeps something dead alive.
            small_vector(const small_vector& other)
            : _size(other._size)
            , _spill(other._spill) {
                _take(other);
            }

            small_vector(small_vector&& other) noexcept
            : _size(other._size)
            , _spill(std::move(other._spill)) {
                _take(other);
                other._size = 0;
            }

            small_vector& operator=(const small_vector& other) {
                _size = other._size;
                _spill = other._spill;
                _take(other);
                return *this;
            }

            small_vector& operator=(small_vector&& other) noexcept {
                _size = other._size;
                _spill = std::move(other._spill);
                _take(other);
                other._size = 0;
                return *this;
            }

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
            // as much of the inline part as has been written, which for
            // a pattern of six letters is six elements and not 256
            void _take(const small_vector& other) noexcept {
                for (size_t i = 0, n = _size < N ? _size : N; i < n; ++i) {
                    _inline[i] = other._inline[i];
                }
            }

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

        // Where a combining sequence begins and ends is normalize.h's
        // joins_sequence and opens_sequence, above both searches so that
        // they cannot answer it differently. It matters twice here. The
        // canonical order is settled one sequence at a time, and a
        // sequence has to be complete before it is read: "0FB2 1D165
        // 0F81" decomposes to four code points of which two come out of
        // the 0F81, and they sort in front of the mark that was written
        // before them. And a match may neither begin nor end inside one.

        // Where a text's code points came from, kept only where
        // somebody asks: a comparison and a key never do, and the array
        // and the work of filling it are not there for them
        struct no_places {
            constexpr bool drop_front(size_t) const noexcept {
                return true;
            }
        };

        template<bool Track>
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

            // Where in the original bytes the combining sequence that
            // holds the i-th code point begins, and where it ends. A
            // search answers with positions in the text it was given,
            // and it is these two that it answers with: a match runs
            // from the start of a sequence to the end of one, so that it
            // never begins or ends inside a letter's marks.
            size_t from(size_t i) const noexcept requires (Track) {
                return _where[_at + i] & ~SequenceHead;
            }

            bool heads_sequence(size_t i) const noexcept requires (Track) {
                return (_where[_at + i] & SequenceHead) != 0;
            }

            size_t upto(size_t i) noexcept requires (Track) {
                uint32_t begin = _where[_at + i] & ~SequenceHead;
                for (size_t j = _at + i + 1; j < _where.size(); ++j) {
                    uint32_t where = _where[j] & ~SequenceHead;
                    if (where != begin) {
                        return where;                  // the next sequence begins where this one ends
                    }
                }
                return _read;                          // nothing further has been read: it ends there
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
                    if (_points.drop_front(_at) && _taken.drop_front(_at)
                        && _where.drop_front(_at)) {
                        _at = 0;
                    }
                }
            }

        private:
            // The top bit of a recorded position, which no position in a
            // text of two gigabytes reaches: this code point is the
            // first of its sequence
            static constexpr uint32_t SequenceHead = 0x80000000;

            // one combining sequence: a code point and the marks that
            // belong with it, taken apart and put in canonical order
            void _fill() {
                if (_read >= _text.size()) {
                    _more = false;
                    return;
                }
                size_t first = _points.size();
                size_t begin = _read;
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
                    if constexpr (Track) {
                        _where.push_back(uint32_t(begin)
                                         | (_where.size() == first ? SequenceHead : 0));
                    }
                    _taken.push_back(0);
                }
                (void)begin;
            }

            std::string_view _text;
            size_t _read = 0;                                  // into the bytes
            size_t _at = 0;                                    // into the code points
            bool _more = true;
            small_vector<char32_t, WindowPoints> _points;
            small_vector<uint8_t, WindowPoints> _taken;
            // the bytes each point came from, where anybody asks
            [[no_unique_address]]
            std::conditional_t<Track, small_vector<uint32_t, WindowPoints>, no_places> _where;
        };

        // The most elements one entry of any table can stand for. It is
        // the size of the buffer _batch fills through a sink that does
        // not test its bound, so it is not a guess: the generator holds
        // the three tables that stand for a letter to it (MAX_ELEMENTS
        // in tools/unicode_tables.py) and will not write a header the
        // number is too small for. Today the widest run is the 18 of
        // U+FDFA, a contraction is at most three and the widest tailored
        // expansion is 22, so there is room for two. Raise the two
        // together or not at all.
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

        template<bool Track>
        inline contraction_match match_contraction(nfd_window<Track>& points) {
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

        template<bool Track>
        inline bool same_points(nfd_window<Track>& points, uint16_t pool_at, size_t size) {
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

        template<bool Track>
        inline tailored_match tailored(nfd_window<Track>& points, const Tailoring& language) {
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

        // A run of decimal digits weighs as the number it spells, so that
        // "plik9" comes before "plik10". The number may be longer than
        // any integer holds — a file name is not obliged to be
        // reasonable — so it is never made into one: the leading zeros
        // come off, and what is compared is first how many digits are
        // left and then the digits themselves. Both are written as
        // weights in the space between the digit zero and the digit one,
        // where no weight of the root falls, so a number sorts where the
        // digits it is written with sort and nothing else moves.
        //
        // The elements of a number, in the order they are compared: one
        // that says how many elements the count of digits takes, that
        // many holding the count, and then the digits four to an
        // element. Two numbers of different length differ in the first
        // of those, two of the same length in the digits.
        inline constexpr uint32_t NumericBase = uint32_t(DigitZero) << PrimaryShift;
        inline constexpr uint32_t NumericChunk = 60000;   // the count of digits, a piece at a time
        inline constexpr uint32_t NumericPack = 10000;    // four digits in one element

        // The elements of a text, one at a time and no further than they
        // are asked for. This is what makes a comparison cheap: two words
        // that differ in their first letter cost one letter each.
        // Track says whether anybody wants to know where in the bytes
        // an element came from — a search does, a comparison does not —
        // and Plain that nothing was asked for that changes what an
        // element weighs. Under Plain the stream is the one that was
        // here before any setting was: no question is asked of any
        // element on its way out.
        template<bool Track = false, bool Plain = false>
        class element_stream {
        public:
            element_stream(std::string_view text, const Tailoring* language,
                           const shape& how = {}) noexcept
            : _points(text)
            , _language(language)
            , _how(how) {
            }

            bool next(Element& e) {
                while (_read == _count) {
                    if (!_fill()) {
                        return false;
                    }
                }
                e = _pending[_read++];
                if constexpr (!Plain) {
                    if (_how.shifted) {
                        _shift(e);
                    }
                }
                return true;
            }

            // Where the element last handed over came from, in the bytes
            // of the text as it was given: the combining sequence it
            // belongs to, whether that sequence begins there, and
            // whether the element is the first or the last of what one
            // letter weighs. A search needs all four — it answers with
            // positions in the original text, and a match may neither
            // begin nor end inside a letter.
            size_t from() const noexcept {
                return _from;
            }

            size_t upto() const noexcept {
                return _to;
            }

            bool heads_sequence() const noexcept {
                return _head;
            }

            bool first_of_letter() const noexcept {
                return _read == 1;
            }

            bool last_of_letter() const noexcept {
                return _read == _count;
            }

        private:
            // S2.3 of the algorithm, where the punctuation is shifted:
            // a variable element weighs nothing at the first three
            // levels and its old first weight at the fourth, and what
            // follows it and is ignorable of itself weighs nothing at
            // any level at all — which is how "re-sume" and "resume"
            // come to be one word.
            void _shift(Element& e) noexcept {
                if (e.primary) {
                    if (e.primary <= VariableLimit) {
                        e.secondary = 0;
                        e.tertiary = ShiftedMark;      // the first weight stays: the fourth level reads it
                        _after_variable = true;
                    } else {
                        _after_variable = false;
                    }
                } else if (_after_variable || (!e.secondary && !e.tertiary)) {
                    e = {0, 0, ShiftedMark};
                }
            }

            // The digits from here on, and the number they spell. Only
            // the decimal digits of any script count — the value of a
            // superscript or of a Roman numeral is not written with
            // digits and is nobody's file name.
            static int _digit(char32_t c) noexcept {
                return c < 0x80 ? (c >= U'0' && c <= U'9' ? int(c - U'0') : -1)
                                : numeric_value_fn(c);
            }

            // The digits of the number being weighed are not gathered
            // anywhere: they are read again, in order, as the elements
            // that hold them are written, from the bytes of the text or
            // from the window the run was found in. That is what lets a
            // number be of any length at all — a file name is not
            // obliged to be reasonable — without a buffer that has to
            // grow, and it costs one walk over the digits.
            int _next_digit() noexcept {
                if (_run_points) {
                    return _digit(_points[_run_at++]);
                }
                auto [c, n] = utf8::decode(_points.text(), _run_at);
                _run_at += n;
                return _digit(c);
            }

            // As many of the number's elements as fit in one batch; what
            // is left waits for the next call, a number of a hundred
            // digits taking more elements than a batch holds.
            void _number(element_sink& out) {
                size_t n = _count_digits;
                size_t chunks = 1;
                for (size_t left = n; left >= NumericChunk; left /= NumericChunk) {
                    ++chunks;
                }
                size_t total = 1 + chunks + (n + 3) / 4;
                while (_wrote < total && out.count < MaxElements) {
                    size_t i = _wrote++;
                    uint32_t weight;
                    if (i == 0) {
                        weight = 1 + uint32_t(chunks);
                    } else if (i <= chunks) {
                        size_t value = n;
                        for (size_t k = chunks - i; k; --k) {
                            value /= NumericChunk;
                        }
                        weight = 1 + uint32_t(value % NumericChunk);
                    } else {
                        uint32_t pack = 0;
                        for (size_t k = 0; k < 4; ++k) {
                            pack = pack * 10 + (_left ? uint32_t(_next_digit()) : 0);
                            _left -= _left ? 1 : 0;
                        }
                        weight = 1 + pack;
                    }
                    out.push_back({NumericBase + weight, 0, 0});
                }
                _writing = _wrote < total;
                if (!_writing) {
                    _wrote = 0;
                    if (_run_points) {
                        _points.skip(_run_size);   // the digits were left where they stood
                    }
                }
            }

            // A run of digits where the text stands, taken from the
            // bytes where nothing has been decomposed and from the
            // window where something has. The leading zeros are not part
            // of the number: "007" and "7" are one number, and what is
            // counted and written is what is left after them.
            bool _digit_run(element_sink& out) {
                if (_points.spent()) {
                    auto text = _points.text();
                    size_t at = _points.byte();
                    if (at >= text.size() || _digit(utf8::decode(text, at).first) < 0) {
                        return false;
                    }
                    size_t end = at, first = at, digits = 0;
                    while (end < text.size()) {
                        auto [c, n] = utf8::decode(text, end);
                        int value = _digit(c);
                        if (value < 0) {
                            break;
                        }
                        end += n;
                        if (!digits && !value) {
                            first = end;               // another zero in front of the number
                        } else {
                            ++digits;
                        }
                    }
                    _points.skip_bytes(end - at);
                    if constexpr (Track) {
                        _from = at;
                        _to = end;
                        _head = true;
                    }
                    _run_points = false;
                    _run_at = first;
                    _run_size = end - first;
                    _count_digits = digits;
                } else {
                    if (_points.taken(0) || _digit(_points[0]) < 0) {
                        return false;
                    }
                    size_t k = 0, first = 0, digits = 0;
                    while (_points.ensure(k + 1) && !_points.taken(k)) {
                        int value = _digit(_points[k]);
                        if (value < 0) {
                            break;
                        }
                        ++k;
                        if (!digits && !value) {
                            first = k;
                        } else {
                            ++digits;
                        }
                    }
                    if constexpr (Track) {
                        _from = _points.from(0);
                        _head = _points.heads_sequence(0);
                        _to = _points.upto(k - 1);
                    }
                    _run_points = true;
                    _run_at = first;
                    _run_size = k;                     // skipped once the number is written out
                    _count_digits = digits;
                }
                _wrote = 0;
                _left = _count_digits;
                _number(out);
                return true;
            }

            // The elements of one letter, with the case of that letter
            // written over them where a collator was asked about it.
            // Which code points the letter was written with is what
            // _batch leaves behind: the case is theirs and not their
            // weights'.
            bool _fill() {
                if (!_batch()) {
                    return false;
                }
                if constexpr (!Plain) {
                    if (_how.cased) {
                        uint8_t rank = case_rank(_letter, _letters);
                        for (uint8_t i = 0; i < _count; ++i) {
                            _pending[i].tertiary =
                                with_case(_pending[i].tertiary, rank, _how.upper_first);
                        }
                    }
                }
                return true;
            }

            bool _batch() {
                _read = 0;
                if constexpr (!Plain) {
                    _letters = 0;
                }
                element_sink out{_pending};

                // a number longer than one batch of elements holds, the
                // rest of which is written here
                if constexpr (!Plain) {
                    if (_writing) {
                        _number(out);
                        _count = out.count;
                        return true;
                    }
                    if (_how.numeric && _digit_run(out)) {
                        _count = out.count;
                        return true;
                    }
                }

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
                                _note(c);
                                _bytes(at, n);
                                _count = out.count;
                                return true;
                            }
                            // the language may have a rule here, and
                            // one over a single code point is read the
                            // same way; anything longer belongs to the
                            // window
                            if (auto one = tailored_one(c, *_language)) {
                                out.push_back({one->primary, one->secondary, one->tertiary});
                                _note(c);
                                _bytes(at, n);
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
                if constexpr (Track) {
                    _from = _points.from(0);
                    _head = _points.heads_sequence(0);
                }
                // the root's contraction is two or three code points, and
                // it stands only where the language has not claimed as
                // many of them itself
                if (theirs.size && theirs.size >= (root.entry ? root.points() : 1)) {
                    put_tailored(out, theirs);
                    _took(theirs.size);
                    _count = out.count;
                    if constexpr (Track) {
                        _to = _points.upto(theirs.size - 1);
                    }
                    _points.skip(theirs.size);
                    return true;
                }
                if (root.entry) {
                    for (size_t k = 0; k < root.entry->size; ++k) {
                        out.push_back(element_of(collate_tables::ContractionPool[root.entry->at + k]));
                    }
                    _note(_points[0]);
                    _note(_points[root.used_second]);
                    _points.take(root.used_second);
                    if (root.used_third) {
                        _note(_points[root.used_third]);
                        _points.take(root.used_third);
                    }
                    _count = out.count;
                    if constexpr (Track) {
                        _to = _points.upto(root.used_third ? root.used_third : root.used_second);
                    }
                    _points.skip(1);
                    return true;
                }
                weights_of(out, _points[0]);
                _note(_points[0]);
                _count = out.count;
                if constexpr (Track) {
                    _to = _points.upto(0);
                }
                _points.skip(1);
                return true;
            }

            // The code points a letter was written with, kept for the
            // case that is read from them. A stream that was not asked
            // about the case keeps nothing: every one of these would
            // otherwise be a store that a comparison never makes.
            void _note(char32_t c) noexcept {
                if constexpr (!Plain) {
                    if (_letters < std::size(_letter)) {
                        _letter[_letters++] = c;
                    }
                }
            }

            void _took(size_t n) noexcept {
                for (size_t i = 0; i < n; ++i) {
                    _note(_points[i]);
                }
            }

            // where the letter just weighed stands in the bytes, for the
            // one path that reads them as they are written
            void _bytes(size_t at, size_t n) noexcept {
                _points.skip_bytes(n);
                if constexpr (Track) {
                    _from = at;
                    _to = at + n;
                    _head = true;
                }
            }

            nfd_window<Track> _points;
            const Tailoring* _language;
            shape _how;
            Element _pending[MaxElements];
            uint8_t _count = 0;
            uint8_t _read = 0;
            bool _after_variable = false;
            bool _writing = false;                    // a number is half written
            // and what it is. None of these is set until a run of
            // digits is found, and none of them is read until one has
            // been: _writing is what says so, and a stream that is
            // asked for no numeric order never touches them. Zeroing
            // them costs a store apiece on every text compared.
            bool _run_points;                         // its digits stand in the window, not in the bytes
            size_t _wrote;                            // how many of its elements are out
            size_t _run_at;                           // the next digit of it to be read
            size_t _run_size;                         // how far the run of digits reaches
            size_t _count_digits;                     // how many of them are the number
            size_t _left;                             // and how many are still to be written
            // where the batch came from, which only a stream that is
            // asked to keep track of it ever writes or reads
            size_t _from, _to;
            bool _head;
            // and the code points the letter last weighed was written
            // with, which is where its case is read from
            char32_t _letter[8];
            uint8_t _letters = 0;
        };

        // The whole text at once, for a caller that needs every element:
        // the sort key does, a comparison does not
        template<bool Plain>
        inline void collation_elements(small_vector<Element, TextElements>& out, const string& text,
                                       const Tailoring* language, const shape& how) {
            element_stream<false, Plain> stream(text.view(), language, how);
            Element e;
            while (stream.next(e)) {
                out.push_back(e);
            }
        }

        // An element of a text with the bytes it came from: what a
        // search walks, since it answers with positions in the text it
        // was given and not in any form of its own
        struct placed {
            Element element;
            uint32_t from, to;
            uint8_t flags;
        };

        inline constexpr uint8_t HeadsSequence = 1;    // a combining sequence begins here
        inline constexpr uint8_t FirstOfLetter = 2;    // and this is the first element it weighs
        inline constexpr uint8_t LastOfLetter = 4;     // or the last

    }

    // A collator puts texts in order. It holds the locale it was made
    // with, how much of a difference counts and what else was asked of
    // it; it is a value, cheap to copy, and it may be used as the
    // comparator of a sorted container or of a sort.
    class collator {
    public:
        // Where a match was found, in the bytes of the text as it was
        // given. The size is the text's own and not the pattern's: six
        // letters of a pattern may be found in nine bytes of text, an
        // accent taking bytes that the pattern never had.
        struct match {
            size_t at;
            size_t size;
        };

        collator() noexcept = default;

        explicit collator(locale where, strength level = strength::tertiary) noexcept
        : collator(where, options{.strength = level}) {
        }

        explicit collator(strength level) noexcept
        : collator(locale(), options{.strength = level}) {
        }

        explicit collator(const options& how) noexcept
        : collator(locale(), how) {
        }

        // What the language asks for is what the collator starts with,
        // and what the caller writes is what it takes instead. Danish
        // sorts its capitals first and Thai shifts its punctuation
        // aside; a caller who wants the Danish order with the small
        // letters first says so and the language is overruled, and one
        // who says nothing gets the language's own answer rather than
        // silence.
        collator(locale where, const options& how) noexcept
        : _tailoring(detail::tailoring_of(where))
        , _locale(where)
        , _strength(how.strength) {
            uint8_t asked = _tailoring ? _tailoring->settings : 0;
            _how.numeric = how.numeric;
            _how.shifted = how.punctuation ? *how.punctuation == punctuation::shifted
                                           : (asked & detail::SettingShifted) != 0;
            _how.case_level = how.case_level ? *how.case_level
                                             : (asked & detail::SettingCaseLevel) != 0;
            _how.upper_first = how.case_order ? *how.case_order == case_order::upper_first
                                              : (asked & detail::SettingUpperFirst) != 0;
            _how.cased = _how.case_level || _how.upper_first;
            _how.backwards = how.backwards ? *how.backwards
                                           : (asked & detail::SettingBackwards) != 0;
            // Nothing was asked for that touches what an element
            // weighs, how a weight is read or which way a level is
            // walked, so the comparison, the key and the stream that
            // feeds them are the ones that were here before any of this
            // was, and every question below is asked where the collator
            // is built rather than at every letter of every text.
            _plain = !_how.shifted && !_how.case_level && !_how.upper_first && !_how.numeric
                  && !_how.backwards;
        }

        // Negative when a comes first, zero when the texts are one and
        // the same to this collator.
        //
        // The first level is walked through both texts side by side and
        // the walk stops at the first difference, which in a sorted list
        // is nearly always the first letter: neither text is taken apart
        // any further than that. What has been read is kept, because the
        // levels below need it when the first says nothing.
        int compare(const string& a, const string& b) const {
            return _plain ? _compare<true>(a, b) : _compare<false>(a, b);
        }

        bool equal(const string& a, const string& b) const {
            return compare(a, b) == 0;
        }

        // A key that compares byte by byte the way the collator compares
        // texts: worth making once for a text that is sorted or looked up
        // many times, and worth storing in an index
        vector<byte> key(const string& text) const {
            return _plain ? _key<true>(text) : _key<false>(text);
        }

        // The same key into a buffer the caller owns, which is what a
        // sort and an index builder want: there a key lives no longer
        // than the pass that uses it, and one allocation a word is the
        // whole cost. Returns the bytes the key takes; when that is more
        // than the buffer holds, nothing is written — a truncated key
        // would compare as a different text, which is worse than none —
        // so a caller may ask with an empty buffer first, or try again
        // with a larger one.
        size_t key_to(const slice<byte>& buffer, const string& text) const {
            elements weights;
            if (_plain) {
                detail::collation_elements<true>(weights, text, _tailoring, _how);
                size_t size = _key_size<true>(weights);
                if (size <= buffer.size()) {
                    _write_key<true>(weights, buffer.data());
                }
                return size;
            }
            detail::collation_elements<false>(weights, text, _tailoring, _how);
            size_t size = _key_size<false>(weights);
            if (size <= buffer.size()) {
                _write_key<false>(weights, buffer.data());
            }
            return size;
        }

        //----------------------------------------------------------------
        // finding one text inside another, section 8 of the algorithm
        //----------------------------------------------------------------
        // Where the pattern is found in the text, counting as equal
        // whatever this collator counts as equal: at primary strength
        // "resume" finds "résumé", and with the punctuation shifted
        // "resume" finds "re-sume". The position and the size are in the
        // bytes of the text as it was given, not of any decomposed or
        // folded form of it.
        //
        // A match begins and ends on a boundary, and the boundary taken
        // here is the combining sequence: a letter with the marks that
        // belong to it. That is the definition because it is the one the
        // elements are already made on — the algorithm gathers a letter
        // and its marks before it weighs them — so a match can neither
        // begin in the middle of an "é" written as two code points nor
        // end before the accent that belongs to the letter it ends on.
        // Two smaller things follow from the same rule: a match may not
        // begin or end inside what one letter weighs, so a pattern of
        // "a" does not match the first half of an "æ" that a language
        // weighs as two letters, and it may not cut a contraction, a
        // Czech "ch" being one letter and not an occurrence of "c".
        //
        // What is left of the letter a match ends on may only be what
        // this collator does not look at, and that is what makes the
        // answer the same for both spellings of a text: at the first
        // strength "resume" finds a "résumé" written either way, accent
        // and all, and at any strength above it finds neither, because
        // there the accent is a difference and the match would be
        // stopping in front of it.
        //
        // The whole text is weighed to answer, so a loop over every
        // occurrence weighs it once for each: `from` is there for a
        // caller who wants the next one, not to make that loop cheap.
        optional<match> find(const string& text, const string& pattern, size_t from = 0) const;
        bool contains(const string& text, const string& pattern) const;
        bool starts_with(const string& text, const string& pattern) const;
        bool ends_with(const string& text, const string& pattern) const;

        // Whether two collators put text in the same order, which is
        // what a prepared searcher and a prepared text have to agree on
        // before one can answer about the other. Every member takes
        // part, and the two that are worked out rather than given — the
        // language's own table and the note that nothing was asked for
        // — follow from the ones that are, so comparing them costs a
        // word and cannot say no where the rest said yes.
        bool operator==(const collator&) const noexcept = default;

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

        // What the collator settled on: the language's answer where the
        // caller gave none, the caller's where it gave one. The
        // `optional` of the options is there to say "not given, so take
        // the language's", and that is a question about what was asked
        // for; these are the answers, and an answer is a bool.
        //
        // Each of the five has two values that matter once it is
        // settled — the punctuation is counted or shifted, the capitals
        // come first or they do not — so a bool says all of it, and a
        // name that reads as a question keeps the type names free:
        // a member called `punctuation` would hide the type of that
        // name inside this class.
        bool shifts_punctuation() const noexcept {     // CLDR ka / alternate
            return _how.shifted;
        }

        bool capitals_first() const noexcept {         // CLDR kf / caseFirst
            return _how.upper_first;
        }

        bool case_level() const noexcept {             // CLDR kc / caseLevel
            return _how.case_level;
        }

        bool backwards() const noexcept {              // CLDR kb, the accents from the end
            return _how.backwards;
        }

        bool numeric() const noexcept {                // CLDR kn, file9 before file10
            return _how.numeric;
        }

        // Whether the library has an order of its own for that language,
        // or puts its text in the root order
        bool tailored() const noexcept {
            return _tailoring != nullptr;
        }

    private:
        // The three shapes of a prepared search, which weigh a text or a
        // pattern with this collator and then ask it what is equal to
        // what. They are the collator's own machinery under other names.
        friend class collated_searcher;
        friend class collated_text;
        friend class collated_matches;

        using elements = detail::small_vector<detail::Element, detail::TextElements>;
        using places = vector<detail::placed>;

        bool _at_least(strength level) const noexcept {
            return _strength >= level;
        }

        template<bool Plain>
        vector<byte> _key(const string& text) const {
            elements weights;
            detail::collation_elements<Plain>(weights, text, _tailoring, _how);
            vector<byte> out(_key_size<Plain>(weights));
            _write_key<Plain>(weights, out.data());
            return out;
        }

        template<bool Plain>
        size_t _key(const string& text, const slice<byte>& buffer) const {
            elements weights;
            detail::collation_elements<Plain>(weights, text, _tailoring, _how);
            size_t size = _key_size<Plain>(weights);
            if (size <= buffer.size()) {
                _write_key<Plain>(weights, buffer.data());
            }
            return size;
        }

        // Negative when a comes first. The first level is walked through
        // both texts side by side and the walk stops at the first
        // difference; what has been read is kept, because the levels
        // below need it when the first says nothing.
        template<bool Plain>
        int _compare(const string& a, const string& b) const {
            if (a == b) {
                return 0;
            }
            detail::element_stream<false, Plain> x(a.view(), _tailoring, _how);
            detail::element_stream<false, Plain> y(b.view(), _tailoring, _how);
            elements read_x, read_y;
            detail::Element e;
            for (;;) {
                uint32_t px = 0, py = 0;
                while (!px && x.next(e)) {
                    read_x.push_back(e);
                    px = detail::primary_of<Plain>(e);
                }
                while (!py && y.next(e)) {
                    read_y.push_back(e);
                    py = detail::primary_of<Plain>(e);
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
            if (_strength == strength::primary && !_how.case_level) {
                return 0;
            }
            // the rest of both texts, which the first level did not need
            while (x.next(e)) {
                read_x.push_back(e);
            }
            while (y.next(e)) {
                read_y.push_back(e);
            }
            return _below_first<Plain>(read_x, read_y);
        }

        // The levels below the first, in the order they are compared,
        // which is also the order a key writes them in
        template<bool Plain>
        int _below_first(const elements& x, const elements& y) const {
            if (_at_least(strength::secondary)) {
                auto weight = [](const detail::Element& e) {
                    return uint32_t(detail::secondary_of<Plain>(e));
                };
                // the accents of Canadian French are read from the end
                // of the word, which is the one level ever walked
                // backwards
                int d = !Plain && _how.backwards ? _walk_back(x, y, weight) : _walk(x, y, weight);
                if (d) {
                    return d;
                }
            }
            if constexpr (!Plain) {
                if (_how.case_level) {
                    int d = _walk(x, y, [](const detail::Element& e) {
                        return uint32_t(detail::case_of(e));
                    });
                    if (d) {
                        return d;
                    }
                }
            }
            if (_at_least(strength::tertiary)) {
                int d = _walk(x, y, [](const detail::Element& e) {
                    return uint32_t(detail::tertiary_of<Plain>(e));
                });
                if (d) {
                    return d;
                }
            }
            if constexpr (!Plain) {
                if (_how.shifted && _at_least(strength::quaternary)) {
                    return _walk(x, y, [](const detail::Element& e) {
                        return detail::quaternary_of(e);
                    });
                }
            }
            return 0;
        }

        // One level of the two element sequences, the weights of zero
        // passed over: they are what the level does not see
        template<class Weight>
        static int _walk(const elements& x, const elements& y, Weight weight) {
            size_t i = 0, j = 0;
            for (;;) {
                while (i < x.size() && !weight(x[i])) {
                    ++i;
                }
                while (j < y.size() && !weight(y[j])) {
                    ++j;
                }
                if (i == x.size() || j == y.size()) {
                    return i == x.size() && j == y.size() ? 0 : (i == x.size() ? -1 : 1);
                }
                if (weight(x[i]) != weight(y[j])) {
                    return weight(x[i]) < weight(y[j]) ? -1 : 1;
                }
                ++i;
                ++j;
            }
        }

        template<class Weight>
        static int _walk_back(const elements& x, const elements& y, Weight weight) {
            size_t i = x.size(), j = y.size();
            for (;;) {
                while (i && !weight(x[i - 1])) {
                    --i;
                }
                while (j && !weight(y[j - 1])) {
                    --j;
                }
                if (!i || !j) {
                    return !i && !j ? 0 : (!i ? -1 : 1);
                }
                --i;
                --j;
                if (weight(x[i]) != weight(y[j])) {
                    return weight(x[i]) < weight(y[j]) ? -1 : 1;
                }
            }
        }

        // What a key of these weights takes: four bytes a letter, two an
        // accent, one a case, two for the third level, four for what was
        // shifted aside, and two for each boundary between levels. The
        // weights of zero are the ones a level passes over, and they are
        // not counted here for the same reason they are not written.
        // This is one loop and the writing below is another, and the two
        // are held together by a test that asks for the key of a
        // thousand texts under every setting and sees that what was
        // written is what was measured.
        template<bool Plain>
        size_t _key_size(const elements& w) const noexcept {
            size_t primary = 0, secondary = 0, cases = 0, tertiary = 0, quaternary = 0;
            for (size_t i = 0; i < w.size(); ++i) {
                primary += detail::primary_of<Plain>(w[i]) ? 1 : 0;
                secondary += detail::secondary_of<Plain>(w[i]) ? 1 : 0;
                tertiary += detail::tertiary_of<Plain>(w[i]) ? 1 : 0;
                if constexpr (!Plain) {
                    cases += detail::case_of(w[i]) ? 1 : 0;
                    quaternary += detail::quaternary_of(w[i]) ? 1 : 0;
                }
            }
            size_t size = primary * 4;
            if (_at_least(strength::secondary)) {
                size += 2 + secondary * 2;
            }
            if (!Plain && _how.case_level) {
                size += 2 + cases;
            }
            if (_at_least(strength::tertiary)) {
                size += 2 + tertiary * 2;
            }
            if (!Plain && _how.shifted && _at_least(strength::quaternary)) {
                size += 2 + quaternary * 4;
            }
            return size;
        }

        // The weight is asked for twice rather than kept in a local
        // between the test and the writing, which reads worse and
        // measures better: a key of a five-letter word costs 14.3 ns
        // written this way and 18.5 with a local, the same instructions
        // in a worse order. It is the shape the header had before the
        // settings came in, and it is kept for that reason.
        template<bool Plain>
        void _write_key(const elements& w, byte* out) const noexcept {
            auto put = [&out](uint32_t x, int bytes) {
                while (bytes--) {
                    *out++ = byte((x >> (8 * bytes)) & 0xFF);
                }
            };
            for (size_t i = 0; i < w.size(); ++i) {
                if (detail::primary_of<Plain>(w[i])) {
                    put(detail::primary_of<Plain>(w[i]), 4);
                }
            }
            if constexpr (Plain) {
                if (!_at_least(strength::secondary)) {
                    return;
                }
            } else if (!_at_least(strength::secondary)) {
                // there is no level of accents at primary strength, but
                // there may still be one of case below it: a search
                // that wants the letters and the case and nothing else
                // asks for exactly that
                _below_secondary<Plain>(w, put);
                return;
            }
            put(0, 2);
            if constexpr (!Plain) {
                // the accents of Canadian French are written from the
                // end of the word, which is the one level ever written
                // backwards
                if (_how.backwards) {
                    for (size_t i = w.size(); i--;) {
                        if (detail::secondary_of<Plain>(w[i])) {
                            put(detail::secondary_of<Plain>(w[i]), 2);
                        }
                    }
                    _below_secondary<Plain>(w, put);
                    return;
                }
            }
            for (size_t i = 0; i < w.size(); ++i) {
                if (detail::secondary_of<Plain>(w[i])) {
                    put(detail::secondary_of<Plain>(w[i]), 2);
                }
            }
            _below_secondary<Plain>(w, put);
        }

        // the case, the third level and what was shifted aside: what a
        // key holds below the accents, and the part of it that does not
        // depend on which way the accents were written
        template<bool Plain, class Put>
        void _below_secondary(const elements& w, Put put) const noexcept {
            if constexpr (!Plain) {
                if (_how.case_level) {
                    put(0, 2);
                    for (size_t i = 0; i < w.size(); ++i) {
                        if (detail::case_of(w[i])) {
                            put(detail::case_of(w[i]), 1);
                        }
                    }
                }
            }
            if (!_at_least(strength::tertiary)) {
                return;
            }
            put(0, 2);
            for (size_t i = 0; i < w.size(); ++i) {
                if (detail::tertiary_of<Plain>(w[i])) {
                    put(detail::tertiary_of<Plain>(w[i]), 2);
                }
            }
            if constexpr (!Plain) {
                if (_how.shifted && _at_least(strength::quaternary)) {
                    put(0, 2);
                    for (size_t i = 0; i < w.size(); ++i) {
                        if (detail::quaternary_of(w[i])) {
                            put(detail::quaternary_of(w[i]), 4);
                        }
                    }
                }
            }
        }

        //----------------------------------------------------------------
        // what a search asks of an element
        //----------------------------------------------------------------
        // Whether any level this collator looks at sees the element at
        // all. An accent is nothing to a primary search, and so is a
        // hyphen where the punctuation is shifted: both are passed over
        // on either side, which is what makes "resume" find "résumé".
        bool _counts(const detail::Element& e) const noexcept {
            if (detail::primary_of<false>(e)) {
                return true;
            }
            if (_at_least(strength::secondary) && detail::secondary_of<false>(e)) {
                return true;
            }
            if (_how.case_level && detail::case_of(e)) {
                return true;
            }
            if (_at_least(strength::tertiary) && detail::tertiary_of<false>(e)) {
                return true;
            }
            return _how.shifted && _at_least(strength::quaternary) && detail::quaternary_of(e);
        }

        bool _same(const detail::Element& a, const detail::Element& b) const noexcept {
            if (detail::primary_of<false>(a) != detail::primary_of<false>(b)) {
                return false;
            }
            if (_at_least(strength::secondary)
                && detail::secondary_of<false>(a) != detail::secondary_of<false>(b)) {
                return false;
            }
            if (_how.case_level && detail::case_of(a) != detail::case_of(b)) {
                return false;
            }
            if (_at_least(strength::tertiary)
                && detail::tertiary_of<false>(a) != detail::tertiary_of<false>(b)) {
                return false;
            }
            return !(_how.shifted && _at_least(strength::quaternary))
                || detail::quaternary_of(a) == detail::quaternary_of(b);
        }

        // The text as its elements, each with the bytes it came from
        places _places(const slice<const char>& text) const {
            places out;
            out.reserve(text.size());
            detail::element_stream<true, false> stream(text.view(), _tailoring, _how);
            detail::Element e;
            while (stream.next(e)) {
                uint8_t flags = uint8_t((stream.heads_sequence() ? detail::HeadsSequence : 0)
                                        | (stream.first_of_letter() ? detail::FirstOfLetter : 0)
                                        | (stream.last_of_letter() ? detail::LastOfLetter : 0));
                out.push_back({e, uint32_t(stream.from()), uint32_t(stream.upto()), flags});
            }
            return out;
        }

        // and the pattern as the elements a search looks at, the ones it
        // passes over left out
        void _wanted(const string& pattern, elements& out) const {
            detail::element_stream<false, false> stream(pattern.view(), _tailoring, _how);
            detail::Element e;
            while (stream.next(e)) {
                if (_counts(e)) {
                    out.push_back(e);
                }
            }
        }

        // The pattern against the text from the i-th element on, with
        // both boundaries asked for
        optional<match> _match(const places& text, size_t i, const elements& wanted,
                               size_t* ended = nullptr) const {
            auto& first = text[i];
            if (!_counts(first.element) || !(first.flags & detail::HeadsSequence)
                || !(first.flags & detail::FirstOfLetter)) {
                return nullopt;
            }
            size_t k = 0, j = i, last = i;
            while (j < text.size() && k < wanted.size()) {
                if (!_counts(text[j].element)) {
                    ++j;
                    continue;
                }
                if (!_same(text[j].element, wanted[k])) {
                    return nullopt;
                }
                last = j;
                ++j;
                ++k;
            }
            if (k < wanted.size()) {
                return nullopt;
            }
            // The letter the match ends on has to end there, and so has
            // the combining sequence it belongs to. What is left of
            // either may only be what this collator does not look at —
            // the acute of a "résumé" found by a primary search for
            // "resume", which is part of the letter and so part of the
            // match. Anything else would be a match that cuts a letter
            // in half (an "a" inside an "æ" that a language weighs as
            // two, which is not an occurrence of "a") or one that stops
            // in front of a mark belonging to the letter it stops on.
            //
            // The two tests are not one test. A letter written as one
            // code point brings its marks with it out of the table, so
            // they are elements of the same letter and only the first
            // catches them; a letter written as a base and a mark
            // weighs them as two letters of one sequence, and only the
            // second does. Without both, "cafe" was found in a "café"
            // written in two code points and not in one written in one,
            // which are the same text.
            while (!(text[last].flags & detail::LastOfLetter)
                   || (last + 1 < text.size()
                       && !(text[last + 1].flags & detail::HeadsSequence))) {
                if (++last >= text.size() || _counts(text[last].element)) {
                    return nullopt;
                }
            }
            if (ended) {
                *ended = last + 1;
            }
            return match{first.from, size_t(text[last].to) - first.from};
        }

        const detail::Tailoring* _tailoring = nullptr;
        locale _locale;
        strength _strength = strength::tertiary;
        detail::shape _how;
        bool _plain = true;
    };

    //--------------------------------------------------------------------
    // a search prepared: the pattern weighed once, the text weighed once
    //--------------------------------------------------------------------
    // A pattern weighed once, for the loop that asks the same question
    // of many texts. It keeps the pattern as it was given, as
    // txt::searcher does, so that it can say what it looks for, and the
    // collator it was weighed with, since the weights are that
    // collator's.
    //
    // A searcher made with one collator and asked of a text made with
    // another answers about neither: its elements were filtered by what
    // the one collator looks at and are then compared by what the other
    // calls equal. A searcher weighed with the punctuation shifted has
    // no hyphen among its elements, and a text weighed with it counted
    // once found "re-sume" inside "resume" that way — an answer neither
    // collator gives. So the text weighs the pattern again, with its
    // own, and the answer is the one the text's collator would have
    // given a pattern of its own making.
    //
    // Weighing again rather than refusing, because the two are both
    // answers a caller cannot check and only one of them is right: a
    // refusal reads as "not found", which is a wrong answer that looks
    // like an ordinary one, while the reweighing costs the pattern and
    // never the text and lands on the answer find(const string&) gives.
    // It happens where a caller has mixed two collators, which is a
    // mistake and not a road, so it is not on anybody's loop.
    class collated_searcher {
    public:
        collated_searcher(const collator& by, const string& pattern)
        : _by(by)
        , _pattern(pattern) {
            by._wanted(pattern, _wanted);
        }

        const string& pattern() const noexcept {
            return _pattern;
        }

        const collator& by() const noexcept {
            return _by;
        }

        // The elements the collator looks at, which is not the length of
        // the pattern in letters: an "æ" a language weighs as two counts
        // twice, and an accent at primary strength counts not at all
        size_t size() const noexcept {
            return _wanted.size();
        }

        bool empty() const noexcept {
            return _wanted.size() == 0;
        }

    private:
        friend class collated_text;
        friend class collated_matches;

        collator _by;
        string _pattern;
        collator::elements _wanted;
    };

    // A text weighed once and asked many times. Weighing is the whole
    // cost of a search by collation — every letter goes through the
    // tables, the contractions are matched, the marks are put in
    // canonical order — so a caller with more than one question builds
    // this and keeps it. The positions it answers with are in the text
    // as it was given.
    class collated_text {
    public:
        using searcher_type = collated_searcher;
        using match = collator::match;

        collated_text() noexcept = default;

        // The text is kept as the string it is and not as a slice of
        // it. A slice is two raw pointers into a managed buffer beside
        // the owner that keeps it alive, which is what a slice is for
        // and is right on a stack — but a collated_matches holds one of
        // these inside a managed object, and a raw pointer from one
        // managed object into another is the one thing the collector
        // does not read as data: it said so on stderr in every build
        // without NDEBUG. A string is a single tracked_ptr and the
        // slice is made where it is asked for.
        collated_text(const collator& by, const string& text)
        : _by(by)
        , _text(text)
        , _places(by._places(text.as_slice())) {
        }

        // A piece of a text keeps that piece, which is a copy of those
        // bytes: the positions this answers with are the piece's own,
        // so what it holds has to be the piece and not the text behind
        // it, and there is no way to name a piece of a managed buffer
        // without a raw pointer into it
        collated_text(const collator& by, const slice<const char>& text)
        : collated_text(by, string(text.data(), text.size())) {
        }

        // A pattern weighed the way this text was, which is the only
        // kind this text can be asked about
        searcher_type searcher(const string& pattern) const {
            return searcher_type(_by, pattern);
        }

        // Where the pattern is found at or after the byte position
        // `from`, or nothing. The position and the size are in the bytes
        // of the text as it was given; the size is the text's own and
        // not the pattern's, an accent taking bytes the pattern never
        // had.
        optional<match> find(const searcher_type& pattern, size_t from = 0) const {
            if (_weighed_elsewhere(pattern)) {
                return find(searcher(pattern.pattern()), from);
            }
            if (pattern.empty()) {
                // An empty pattern is found where it is looked for, as
                // it is in a string's own find. A pattern that is not
                // empty but that the collator does not look at — an
                // accent on its own where the accents are not compared
                // — is not the same thing and is not found: it would
                // otherwise be found everywhere, which is no answer.
                return pattern.pattern().empty() && from <= _text.size()
                     ? optional<match>({from, 0}) : nullopt;
            }
            for (size_t i = _from(from); i < _places.size(); ++i) {
                if (_places[i].from < from) {
                    continue;
                }
                if (auto hit = _by._match(_places, i, pattern._wanted)) {
                    return hit;
                }
            }
            return nullopt;
        }

        optional<match> find(const string& pattern, size_t from = 0) const {
            return find(searcher(pattern), from);
        }

        bool contains(const searcher_type& pattern) const {
            return find(pattern).has_value();
        }

        bool contains(const string& pattern) const {
            return find(pattern).has_value();
        }

        // Whether the text begins with the pattern, and whether it ends
        // with it — the same equality and the same boundaries as find,
        // asked at one end of the text rather than everywhere in it
        // At one end of the text means: with nothing in front of it, or
        // nothing behind it, that this collator looks at. Not at byte
        // zero and not up to the last byte, because those are questions
        // about the bytes and this is a search by collation — with the
        // punctuation shifted a hyphen is not there to be found around,
        // and it is not there at the ends either. `find` has always
        // answered that way and these two did not: "-resume" contained
        // "resume" and did not start with it, which is two rules where
        // the header promises one.
        bool starts_with(const searcher_type& pattern) const {
            if (_weighed_elsewhere(pattern)) {
                return starts_with(searcher(pattern.pattern()));
            }
            if (pattern.empty()) {
                return pattern.pattern().empty();
            }
            for (size_t i = 0; i < _places.size(); ++i) {
                if (_by._match(_places, i, pattern._wanted)) {
                    return true;
                }
                if (_by._counts(_places[i].element)) {
                    break;         // the first letter of the text is not the pattern's
                }
            }
            return false;
        }

        bool starts_with(const string& pattern) const {
            return starts_with(searcher(pattern));
        }

        // A match that ends at the end of the text takes the last
        // elements this collator looks at and no others — one fewer and
        // something it looks at would be left behind it, one more and
        // the pattern would not be as long as it is — so there is a
        // single element it can begin at, and finding it is a walk back
        // over the pattern's length rather than over the text's. It was
        // the text's: every position was tried, from the last to the
        // first, and over a hundred kilobytes that is 658 us against the
        // 6 ns starts_with costs for the same question at the other end.
        bool ends_with(const searcher_type& pattern) const {
            if (_weighed_elsewhere(pattern)) {
                return ends_with(searcher(pattern.pattern()));
            }
            if (pattern.empty()) {
                return pattern.pattern().empty();
            }
            size_t counted = 0, i = _places.size();
            while (i--) {
                if (_by._counts(_places[i].element) && ++counted == pattern._wanted.size()) {
                    break;
                }
            }
            if (counted < pattern._wanted.size()) {
                return false;                  // the text has too few to end with it
            }
            size_t ended = i + 1;
            return _by._match(_places, i, pattern._wanted, &ended) && _nothing_after(ended);
        }

        bool ends_with(const string& pattern) const {
            return ends_with(searcher(pattern));
        }

        // The occurrences that do not overlap, counted left to right
        size_t count(const searcher_type& pattern) const {
            if (_weighed_elsewhere(pattern)) {
                return count(searcher(pattern.pattern()));
            }
            if (pattern.empty()) {
                return 0;
            }
            size_t n = 0;
            for (size_t i = 0; i < _places.size();) {
                size_t ended = i + 1;
                if (_by._match(_places, i, pattern._wanted, &ended)) {
                    ++n;
                    i = ended;
                } else {
                    ++i;
                }
            }
            return n;
        }

        size_t count(const string& pattern) const {
            return count(searcher(pattern));
        }

        // The text this was built from, which the matches are positions
        // in. A slice made here rather than kept, for the reason the
        // constructor gives.
        slice<const char> text() const noexcept {
            return _text.as_slice();
        }

        const string& bytes() const noexcept {
            return _text;
        }

        const collator& by() const noexcept {
            return _by;
        }

        // How many elements it weighed to
        size_t size() const noexcept {
            return _places.size();
        }

        // And where in the text the i-th of them came from: the start
        // of the combining sequence that produced it, which is what the
        // bisection below rests on — these ascend
        size_t at(size_t i) const noexcept {
            return _places[i].from;
        }

        bool empty() const noexcept {
            return _places.empty();
        }

    private:
        friend class collated_matches;

        // Whether a searcher was weighed by some other collator than
        // the one this text was weighed by, in which case it answers
        // about neither and this text weighs the pattern again with its
        // own. One comparison of a few words where the two agree, which
        // is every road but the mistake.
        bool _weighed_elsewhere(const searcher_type& pattern) const noexcept {
            return !(pattern.by() == _by);
        }

        // Whether everything from the k-th element on is something this
        // collator does not look at, which is what "at the end of the
        // text" means to a search by collation
        bool _nothing_after(size_t k) const noexcept {
            for (; k < _places.size(); ++k) {
                if (_by._counts(_places[k].element)) {
                    return false;
                }
            }
            return true;
        }

        // The first element at or after a byte position. The positions
        // ascend — an element belongs to the combining sequence it came
        // from, and the sequences are weighed in the order they stand —
        // so this is a bisection and not a walk, which is what keeps a
        // loop over the occurrences linear. The canonical ordering that
        // moves a mark inside a sequence cannot move it out of one, and
        // every element of a sequence carries that sequence's bytes.
        size_t _from(size_t from) const noexcept {
            if (!from) {
                return 0;
            }
            size_t lo = 0, hi = _places.size();
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (_places[mid].from < from) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            return lo;
        }

        collator _by;
        string _text;
        collator::places _places;
    };

    // Every occurrence, the text weighed once for all of them. A range
    // in the manner of segment.h and of the match ranges of search.h:
    // constructed from the text and the pattern, deciding as it walks,
    // the element a slice of the original text — the bytes the match
    // covers, which need not be as many as the pattern has, since a
    // pattern of "resume" matches eight of them in "résumé".
    //
    // The weighed text is too large to copy into every iterator and must
    // outlive the range where an iterator is taken from a temporary, so
    // one tracked object holds it and the iterator holds a pointer to
    // that rather than to the range. A loop over a temporary is safe
    // here as it is there, and so is an iterator that outlives the range
    // it came from.
    //
    // The occurrences do not overlap: the next is looked for past the
    // end of the last.
    class collated_matches
    : public mixin::enumerable<collated_matches> {
    public:
        using searcher_type = collated_searcher;
        using value_type = slice<const char>;
        using size_type = size_t;

    private:
        struct state {
            // A pattern weighed by another collator is weighed again
            // here, once, rather than at every step of the walk: this
            // is the one place the range brings the two together
            state(const collator& by, const string& t, searcher_type p)
            : text(by, t)
            , pattern(p.by() == by ? std::move(p) : searcher_type(by, p.pattern())) {
            }

            collated_text text;
            searcher_type pattern;

            // An empty pattern matches nowhere here, as count counts
            // none of it: a range of every position is not what anyone
            // asking this question wants, and it would not end
            size_t next(size_t i, size_t& ended) const noexcept {
                if (pattern.empty()) {
                    return npos;
                }
                for (; i < text._places.size(); ++i) {
                    ended = i + 1;
                    if (text._by._match(text._places, i, pattern._wanted, &ended)) {
                        return i;
                    }
                }
                return npos;
            }
        };

    public:
        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = slice<const char>;
            using difference_type = ptrdiff_t;
            using reference = slice<const char>;
            using pointer = void;

            iterator() noexcept = default;

            slice<const char> operator*() const noexcept {
                return _state->text.bytes().as_slice(pos(), size());
            }

            iterator& operator++() noexcept {
                _at = _state->next(_ended, _ended);
                return *this;
            }

            iterator operator++(int) noexcept {
                iterator t = *this;
                ++*this;
                return t;
            }

            friend bool operator==(const iterator& a, const iterator& b) noexcept {
                return a._at == b._at;
            }

            // The byte position of the match in the text, and its size
            size_t pos() const noexcept {
                return _state->text._places[_at].from;
            }

            size_t size() const noexcept {
                size_t from = pos();
                size_t to = _state->text._places[_ended - 1].to;
                return to > from ? to - from : 0;
            }

        private:
            friend class collated_matches;

            iterator(tracked_ptr<const state> s, size_t at, size_t ended) noexcept
            : _state(std::move(s))
            , _at(at)
            , _ended(ended) {
            }

            tracked_ptr<const state> _state;
            size_t _at = npos;
            size_t _ended = 0;
        };

        using const_iterator = iterator;

        collated_matches() noexcept = default;

        collated_matches(const collator& by, const string& text, const string& pattern)
        : collated_matches(by, text, searcher_type(by, pattern)) {
        }

        collated_matches(const collator& by, const string& text, searcher_type pattern)
        : _state(make_tracked<state>(by, text, std::move(pattern))) {
        }

        // A piece of a text is kept as that piece, which copies those
        // bytes: collated_text says why
        collated_matches(const collator& by, const slice<const char>& text, searcher_type pattern)
        : collated_matches(by, string(text.data(), text.size()), std::move(pattern)) {
        }

        iterator begin() const noexcept {
            size_t ended = 0;
            size_t at = _state ? _state->next(0, ended) : npos;
            return iterator(_state, at, ended);
        }

        iterator end() const noexcept {
            return iterator(_state, npos, 0);
        }

        bool empty() const noexcept {
            size_t ended = 0;
            return !_state || _state->next(0, ended) == npos;
        }

        // Walked and counted, not stored
        size_type count() const noexcept {
            return _state ? _state->text.count(_state->pattern) : 0;
        }

        slice<const char> text() const noexcept {
            return _state->text.text();
        }

        const searcher_type& pattern() const noexcept {
            return _state->pattern;
        }

    private:
        tracked_ptr<state> _state;
    };

    // The short way to ask once: both sides weighed on the call, which
    // is what makes a loop over the occurrences quadratic. A
    // collated_text kept, or collated_matches, is the way to ask more
    // than once.
    inline optional<collator::match> collator::find(const string& text, const string& pattern,
                                                    size_t from) const {
        return collated_text(*this, text).find(pattern, from);
    }

    inline bool collator::contains(const string& text, const string& pattern) const {
        return collated_text(*this, text).contains(pattern);
    }

    inline bool collator::starts_with(const string& text, const string& pattern) const {
        return collated_text(*this, text).starts_with(pattern);
    }

    inline bool collator::ends_with(const string& text, const string& pattern) const {
        return collated_text(*this, text).ends_with(pattern);
    }
}
