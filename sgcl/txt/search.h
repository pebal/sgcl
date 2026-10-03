//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "case.h"
#include "detail/skip_table.h"

#include <algorithm>

// Finding a text inside a text. Three ways, for three questions. The
// bytes as they stand, which is what a parser wants and what `find` on a
// string already does — here with the pattern prepared once, for the
// loop that looks for the same thing in many texts. Blind to case, which
// is what a person searching wants. And blind to the way the text was
// written, which is what a search over names and file paths wants, since
// "é" typed in two code points must find "é" stored in one.
//
// The last two cannot work on the bytes, because folding and decomposing
// change them: both sides are mapped to code points first and searched
// there, the position of every one of them in the original text kept so
// that a match can be reported where the caller can use it. That mapping
// is the whole cost of such a search, and it is the text's, not the
// pattern's — so both sides can be prepared: a fold_searcher is a pattern
// mapped once, a folded_text is a text mapped once and asked many times,
// and fold_matches is every occurrence of one in the other with the text
// built a single time. find_fold, which prepares both on every call, is
// the short way to ask once and the wrong way to ask in a loop.
//
// A match of either of them takes whole characters of the text it is
// found in: it may not cut an expansion or a decomposition in two. "ss"
// finds a "ß", which is the whole of what folding makes of it, and "s"
// does not find half of one; "e" does not find the "e" that an "é" comes
// apart into. Without the rule a match could begin and end in the middle
// of a character, and what came back was a position with no text at it —
// nothing a caller can cut out or draw a box round.
//
// And it takes whole combining sequences: a letter with the marks that
// belong to it. "cafe" is not found in "café" however that text is
// written — neither with the acute as a code point of its own nor with
// the "é" as one — because the acute belongs to the letter the match
// would stop on. This is the same rule the collated search of collate.h
// keeps, asked here of the code points and there of the elements, and
// both of them ask normalize.h's opens_sequence so that they cannot
// drift apart: a search blind to the way a text is written may not
// answer two ways about two spellings of one text.
namespace sgcl::txt {
    // A text as a search without regard to case, or to the way it was
    // written, sees it (folded_text::points, fold_matches::points): its
    // code points, folded or decomposed and put in canonical order, and
    // the byte of the original text each of them came from — one entry
    // more than the points, the size of the text, so that the end of a
    // match is a position too. Where one character's points end and the
    // next one's begin is where the position changes.
    struct mapped_text {
        vector<char32_t> points;
        vector<size_t> at;
    };

    // A pattern prepared once: Boyer–Moore–Horspool over the bytes
    // (detail/skip_table.h), which needs no more than a table of skips
    // and finds a pattern of m bytes in n bytes in about n/m steps on
    // ordinary text. The table and the walk are in the detail header
    // because regex.h's machine uses the same two over the bytes it is
    // already holding, to jump to the next place a match could begin;
    // one implementation means one place for a fault in it to be.
    class searcher {
    public:
        explicit searcher(const string& pattern) noexcept
        : _pattern(pattern) {
            _table.prepare(_pattern.view());
        }

        // The byte position of the first occurrence at or after `from`,
        // or npos. An empty pattern is found at once, as it is in a
        // std::string.
        size_t find(const string& text, size_t from = 0) const noexcept {
            return _table.find(text.view(), _pattern.view(), from);
        }

        // The same over a piece of a text, which is the other way this
        // module takes one
        size_t find(const slice<const char>& text, size_t from = 0) const noexcept {
            return _table.find({text.data(), text.size()}, _pattern.view(), from);
        }

        // A C text — a literal among them, which a string and a slice
        // would both take — as detail::c_text reads it
        template<size_t N>
        size_t find(const char (&text)[N], size_t from = 0) const noexcept {
            return find(detail::c_text(text), from);
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        size_t find(P text, size_t from = 0) const noexcept {
            return find(detail::c_text(text), from);
        }

        bool contains(const string& text) const noexcept {
            return find(text) != npos;
        }

        bool contains(const slice<const char>& text) const noexcept {
            return find(text) != npos;
        }

        template<size_t N>
        bool contains(const char (&text)[N]) const noexcept {
            return find(text) != npos;
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        bool contains(P text) const noexcept {
            return find(text) != npos;
        }

        // The occurrences that do not overlap, counted left to right, in
        // every text find takes
        size_t count(const string& text) const noexcept {
            return _count(text.view());
        }

        size_t count(const slice<const char>& text) const noexcept {
            return _count({text.data(), text.size()});
        }

        template<size_t N>
        size_t count(const char (&text)[N]) const noexcept {
            return count(detail::c_text(text));
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        size_t count(P text) const noexcept {
            return count(detail::c_text(text));
        }

        const string& pattern() const noexcept {
            return _pattern;
        }

    private:
        size_t _count(std::string_view text) const noexcept {
            if (_pattern.empty()) {
                return 0;
            }
            size_t n = 0;
            for (size_t at = _table.find(text, _pattern.view(), 0); at != npos;
                 at = _table.find(text, _pattern.view(), at + _pattern.size())) {
                ++n;
            }
            return n;
        }

        string _pattern;
        detail::skip_table _table;
    };

    namespace detail {
        // A text as a sequence of code points, with the byte position in
        // the original that each of them came from, so that a match found
        // in the mapped text can be reported where the caller can use it.
        // `at` holds one entry more than `points`, the size of the text,
        // so that the end of a match is a position too.
        //
        // The positions belong to the places in the mapped text and not to
        // the code points in them. Every mapping writes its points in the
        // order of the text, so the positions ascend as they are written,
        // and the canonical ordering of the decomposed map then moves the
        // points and leaves the positions where they stand. It moves only
        // marks, and never one across a letter (normalize.h's
        // canonical_order), so at every place where a match may begin or
        // end the point is still the one written there and its position
        // is its own; in between, a position is that of the mark written
        // in that place before the ordering moved it, and nothing asks
        // more of it than the bisection does.
        // Two things follow. Where one character's points end and the next
        // one's begin is where the position changes, so the rule against
        // cutting a character in two needs no word of its own. And the
        // positions ascend in every text, so the search can bisect them
        // wherever it starts.
        //
        // Two kinds, one layout. A text mapped for an object that keeps
        // it (a folded_text, the state of a match range) is a mapped_text,
        // in vectors of the library: that is the object's data, and
        // building it there measured a quarter faster than in fresh plain
        // memory over sixty-four kilobytes. A text mapped for one call (find,
        // contains, count, the pattern of a searcher) is scratch and is
        // in plain memory lent by the thread (detail/lent.h): as managed
        // buffers it was 262 KB of garbage for every search through ten
        // kilobytes, and in fresh plain memory the call a quarter slower.
        template<class Points, class At>
        struct basic_mapped_text {
            Points points;
            At at;
        };

        using mapped_scratch = basic_mapped_text<code_points, scratch_vector<size_t>>;

        template<class F>
        void lent_each(mapped_scratch& t, F& f) noexcept {
            f(t.points);
            f(t.at);
        }

        template<class Text, class F>
        SGCL_ALWAYS_INLINE void mapped_into(Text& out, std::string_view text, F&& each) noexcept {
            out.points.reserve(text.size());
            out.at.reserve(text.size() + 1);
            for (size_t i = 0; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                size_t before = out.points.size();
                each(out.points, text, i, i + n, c);
                for (size_t k = before; k < out.points.size(); ++k) {
                    out.at.push_back(i);
                }
                i += n;
            }
            out.at.push_back(text.size());
        }

        // The two mappings, each a per-code-point step and what has to be
        // done to the whole once it is built. They are types rather than
        // functions so that the classes below are one class over them, as
        // the ranges of segment.h are one class over the end of a segment.
        struct fold_mapping {
            template<class Points>
            static void point(Points& out, char32_t c) noexcept {
                // Nothing below 0x80 has a folding of its own beyond the
                // ASCII lowercase, and the table of the full ones is a
                // bisection — where the decomposition of the normalizing
                // map beside it is one read of a set. Paid only where it
                // can answer, the mapping of an ASCII text costs a third
                // of what it did.
                if (c < 0x80) {
                    out.push_back(c | ((c - U'A' < 26) << 5));
                    return;
                }
                if (auto d = full_of(c, case_tables::FullFold)) {
                    append(out, d);
                } else {
                    out.push_back(unicode::to_lower(c));
                }
            }

            template<class Text>
            static void whole(Text&) noexcept {
            }
        };

        struct nfd_mapping {
            template<class Points>
            static void point(Points& out, char32_t c) noexcept {
                decompose_into<false>(out, c);
            }

            // Canonical order, fixed over the whole text rather than per
            // code point, since the marks of two characters can stand in
            // one run. The points move and the positions stay, as the
            // text above says, and it is the same ordering normalize()
            // and the collator's window use.
            template<class Text>
            static void whole(Text& out) noexcept {
                canonical_order(out.points);
            }
        };

        template<class Map, class Text>
        SGCL_ALWAYS_INLINE void map_text_into(Text& out, std::string_view text) noexcept {
            mapped_into(out, text, [](auto& points, std::string_view, size_t, size_t, char32_t c) noexcept {
                Map::point(points, c);
            });
            Map::whole(out);
        }

        template<class Map>
        mapped_text map_text(std::string_view text) noexcept {
            mapped_text out;
            map_text_into<Map>(out, text);
            return out;
        }

        // The first place in the mapped text at or after the byte
        // position `from`. A bisection and not a walk, which is what keeps
        // a loop over the occurrences of a prepared text linear.
        //
        // The positions ascend in every text (mapped_text above), and
        // they did not always: they used to move with the marks, and a
        // text with one pair of marks out of order needed an index of its
        // own built beside them, or had its every search begin at the
        // front — over a hundred kilobytes 306 ms against 973 us.
        template<class Text>
        size_t point_at(const Text& t, size_t from) noexcept {
            size_t lo = 0, hi = t.points.size();
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (t.at[mid] < from) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            return lo;
        }

        // Whether a match may begin, or end, in front of the i-th place of
        // the mapped text: a character of the text begins there, and so
        // does a combining sequence. The first place of a text is such a
        // place whatever stands in it, as the first code point read is in
        // collate.h's window — there is nothing in front of it to join or
        // to cut.
        //
        // What this asks of two neighbours is a question about a set: a
        // match that begins and ends at such places holds every point of
        // every character it touches, and nothing of any other. Within
        // those bounds the ordering may have moved marks about, but not
        // across either of them, since both stand in front of a letter.
        // So the bytes from the position of the one to the position of
        // the other are exactly the characters the match took, which is
        // what it reports, and what the collated search reports too.
        //
        // It is asked of the two ends of a match that has already been
        // found and not of every place scanned, so it costs two reads a
        // match and nothing on the walk.
        template<class Text>
        bool whole_here(const Text& text, size_t i) noexcept {
            return i == 0 || (text.at[i] != text.at[i - 1] && opens_sequence(text.points[i]));
        }

        // The pattern's code points inside the text's, from the code
        // point `i` on, and the index where the match begins, or npos
        template<class Text, class Pattern>
        size_t match_from(const Text& text, const Pattern& pattern, size_t i) noexcept {
            if (pattern.empty() || pattern.size() > text.points.size()) {
                return pattern.empty() && i <= text.points.size() ? i : npos;
            }
            // A scan filtered by the pattern's first code point, and not
            // the skips of the searcher above: Boyer–Moore–Horspool wants
            // a table over the alphabet, and the alphabet here is the
            // whole of Unicode. It would also be answering the wrong
            // question — over sixty-four kilobytes the mapping of the
            // text costs 346 microseconds against 71 for finding every
            // one of its 878 occurrences, so the scan is a sixth of the
            // work and skipping could take back no more than part of it.
            char32_t head = pattern.front();
            size_t m = pattern.size();
            size_t last = text.points.size() - m;
            const char32_t* p = text.points.data();
            for (; i <= last; ++i) {
                if (p[i] != head) {
                    continue;
                }
                size_t k = 1;
                while (k < m && p[i + k] == pattern[k]) {
                    ++k;
                }
                if (k != m) {
                    continue;
                }
                // A match may not cut a character in two, and it may not
                // begin or end inside a combining sequence. Two rules
                // because the two things they forbid are different, and
                // both ends are asked both of them.
                //
                // The first is about what one character of the text
                // became: "ss" finds a "ß", which is the whole of what
                // it folds to, and "s" does not find half of one; "e"
                // does not find the "e" that an "é" comes apart into.
                //
                // The second is about the marks that belong to a
                // letter, and is normalize.h's opens_sequence, which
                // collate.h's search asks as well. Without it "cafe"
                // was found in a "café" written as a base and an acute
                // — where the acute is a character of its own and the
                // first rule has nothing to say — and not in a "café"
                // written as one code point, which is the same text.
                // The two answers were the one thing a search blind to
                // the way a text is written may never do.
                //
                // Where either fails the search goes on from the next
                // position rather than giving up, which is the mistake
                // this is easiest to make.
                if (!whole_here(text, i)) {
                    continue;
                }
                if (i + m < text.points.size() && !whole_here(text, i + m)) {
                    continue;
                }
                return i;
            }
            return npos;
        }

    }

    // Where a search without regard to case, or to the way a text was
    // written, found its pattern: the bytes of the original text the
    // match covers, which need not be as many as the pattern has
    // ("STRASSE" covers the seven bytes of "straße")
    struct occurrence {
        size_t pos = 0;
        size_t size = 0;

        friend bool operator==(const occurrence&, const occurrence&) noexcept = default;
    };

    namespace detail {
        // The first match at or after the byte `from`, with the bytes it
        // covers. An empty pattern is found where it is looked for, as it
        // is in a std::string and in the searcher above, and nowhere past
        // the end of the text. A mapping with no end at all is the one a
        // prepared text was moved out of, which answers as the empty text
        template<class Text, class Pattern>
        optional<occurrence> find_occurrence(const Text& text, const Pattern& pattern, size_t from) noexcept {
            if (pattern.empty()) {
                if (from <= (text.at.empty() ? 0 : text.at.back())) {
                    return occurrence{from, 0};
                }
                return nullopt;
            }
            size_t i = match_from(text, pattern, point_at(text, from));
            if (i == npos) {
                return nullopt;
            }
            return occurrence{text.at[i], text.at[i + pattern.size()] - text.at[i]};
        }

        // A pattern mapped once. It keeps the pattern as it was given,
        // as searcher does, so that it can say what it looks for; find,
        // contains and count map the text on every call (a text asked
        // more than once is a folded_text or a normalized_text).
        template<class Map>
        class mapped_searcher {
        public:
            explicit mapped_searcher(const string& pattern) noexcept
            : _pattern(pattern)
            , _points(_mapped_pattern(pattern)) {
            }

            // A copy is the pattern again; a searcher moved from is the
            // searcher of the empty pattern, its pattern() empty too, and
            // not a pattern that says one thing and finds another
            mapped_searcher(const mapped_searcher&) = default;
            mapped_searcher& operator=(const mapped_searcher&) = default;

            mapped_searcher(mapped_searcher&& other) noexcept
            : _pattern(other._pattern)
            , _points(std::move(other._points)) {
                other._reset();
            }

            mapped_searcher& operator=(mapped_searcher&& other) noexcept {
                if (this != &other) {
                    _pattern = other._pattern;
                    _points = std::move(other._points);
                    other._reset();
                }
                return *this;
            }

            const string& pattern() const noexcept {
                return _pattern;
            }

            // The code points searched for, which is not the length of
            // the pattern in characters: "ß" folds to two
            size_t size() const noexcept {
                return _points.size();
            }

            bool empty() const noexcept {
                return _points.empty();
            }

            const vector<char32_t>& points() const noexcept {
                return _points;
            }

            // The first occurrence in the text at or after the byte `from`
            optional<occurrence> find(const string& text, size_t from = 0) const noexcept {
                lent<mapped_scratch> mapped;
                map_text_into<Map>(*mapped, text.view());
                return find_occurrence(*mapped, _points, from);
            }

            bool contains(const string& text) const noexcept {
                return find(text).has_value();
            }

            // The occurrences that do not overlap, counted left to right
            size_t count(const string& text) const noexcept {
                if (_points.empty()) {
                    return 0;
                }
                lent<mapped_scratch> lent_mapped;
                auto& mapped = *lent_mapped;
                map_text_into<Map>(mapped, text.view());
                size_t n = 0;
                for (size_t i = match_from(mapped, _points, 0); i != npos; i = match_from(mapped, _points, i + _points.size())) {
                    ++n;
                }
                return n;
            }

        private:
            void _reset() noexcept {
                _pattern = string();
                _points = vector<char32_t>();
            }

            // The pattern mapped in scratch and kept at its size: the
            // positions of its code points are of no use to a pattern
            static vector<char32_t> _mapped_pattern(const string& pattern) noexcept {
                lent<mapped_scratch> mapped;
                map_text_into<Map>(*mapped, pattern.view());
                return vector<char32_t>(mapped->points.begin(), mapped->points.end());
            }

            string _pattern;
            vector<char32_t> _points;
        };

        // A text mapped once and asked many times. The mapping is the
        // whole cost of this kind of search — over a text of sixty-four
        // kilobytes it is some three hundred microseconds and a search
        // through it some twenty — so a caller with more than one
        // question builds this and keeps it.
        template<class Map>
        class mapped_search {
        public:
            using searcher_type = mapped_searcher<Map>;

            // An empty text: asked, it answers as one built from ""
            mapped_search() noexcept
            : mapped_search(slice<const char>()) {
            }

            // A copy is the text again; a text moved from is the empty
            // text, as the one above
            mapped_search(const mapped_search&) = default;
            mapped_search& operator=(const mapped_search&) = default;

            mapped_search(mapped_search&& other) noexcept
            : _text(other._text)
            , _mapped(std::move(other._mapped)) {
                other._reset();
            }

            mapped_search& operator=(mapped_search&& other) noexcept {
                if (this != &other) {
                    _text = other._text;
                    _mapped = std::move(other._mapped);
                    other._reset();
                }
                return *this;
            }

            explicit mapped_search(const string& text) noexcept
            : mapped_search(text.as_slice()) {
            }

            explicit mapped_search(const slice<const char>& text) noexcept
            : _text(text)
            , _mapped(map_text<Map>(text.view())) {
            }

            // A C text — a literal among them, which a string and a
            // slice would both take — as detail::c_text reads it, copied
            // into a string the text holds: the matches are slices of it
            template<size_t N>
            explicit mapped_search(const char (&text)[N])
            : mapped_search(c_string(text).as_slice()) {
            }

            template<class P>
            requires std::same_as<P, const char*> || std::same_as<P, char*>
            explicit mapped_search(P text)
            : mapped_search(c_string(text).as_slice()) {
            }

            // The first occurrence at or after the byte position `from`,
            // as the searcher's own find answers it: where it begins and
            // the bytes it covers, in the text as it was given and not in
            // the mapped copy of it — or nothing
            optional<occurrence> find(const searcher_type& pattern, size_t from = 0) const noexcept {
                return find_occurrence(_mapped, pattern.points(), from);
            }

            optional<occurrence> find(const string& pattern, size_t from = 0) const noexcept {
                return find(searcher_type(pattern), from);
            }

            bool contains(const searcher_type& pattern) const noexcept {
                return find(pattern).has_value();
            }

            bool contains(const string& pattern) const noexcept {
                return find(pattern).has_value();
            }

            // The occurrences that do not overlap, counted left to right
            size_t count(const searcher_type& pattern) const noexcept {
                if (pattern.empty()) {
                    return 0;
                }
                size_t n = 0;
                for (size_t i = match_from(_mapped, pattern.points(), 0); i != npos;
                     i = match_from(_mapped, pattern.points(), i + pattern.size())) {
                    ++n;
                }
                return n;
            }

            size_t count(const string& pattern) const noexcept {
                return count(searcher_type(pattern));
            }

            // The text this was built from, which the matches are slices of
            const slice<const char>& text() const noexcept {
                return _text;
            }

            // How many code points it mapped to
            size_t size() const noexcept {
                return _mapped.points.size();
            }

            bool empty() const noexcept {
                return _mapped.points.empty();
            }

            const mapped_text& points() const noexcept {
                return _mapped;
            }

        private:
            void _reset() noexcept {
                _text = slice<const char>();
                _mapped = map_text<Map>(std::string_view());
            }

            slice<const char> _text;
            mapped_text _mapped;
        };

        // Every occurrence, the text mapped once for all of them. A range
        // in the manner of segment.h: constructed from the text and the
        // pattern, deciding as it walks, the element a slice of the
        // original text — the bytes the match covers, which need not be
        // as many as the pattern has, "STRASSE" matching all seven of
        // them in "straße".
        //
        // What it has and the ranges of segment.h have not is a mapped
        // text, which is too large to copy into every iterator and must
        // outlive the range where an iterator is taken from a temporary:
        // one tracked object holds it, the text and the pattern, and the
        // iterator holds a pointer to that rather than to the range. So a
        // loop over a temporary is safe here as it is there, and so is an
        // iterator that outlives the range it came from.
        //
        // The occurrences do not overlap, as searcher::count counts them:
        // the next is looked for past the end of the last.
        template<class Map>
        class match_range
        : public mixin::enumerable<match_range<Map>> {
        public:
            using searcher_type = mapped_searcher<Map>;
            using value_type = slice<const char>;
            using size_type = size_t;

        private:
            struct state {
                state(const string& t, searcher_type p) noexcept
                : text(t)
                , pattern(std::move(p))
                , mapped(map_text<Map>(t.view())) {
                }

                // The text as the string it is and not as a slice of
                // it. A slice is two raw pointers into a managed buffer
                // beside the owner that keeps it alive, which is what a
                // slice is for and is right on a stack; this one lives
                // inside a managed object, and a raw pointer from one
                // managed object into another is the one thing the
                // collector does not read as data. The slice the
                // elements of this range are is made where it is asked
                // for, which is one addition.
                string text;
                searcher_type pattern;
                mapped_text mapped;

                // An empty pattern matches nowhere here, as
                // searcher::count counts none of it: a range of every
                // position is not what anyone asking this question
                // wants, and it would not end
                size_t next(size_t i) const noexcept {
                    return pattern.empty() ? npos : match_from(mapped, pattern.points(), i);
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
                    return _state->text.as_slice(pos(), size());
                }

                iterator& operator++() noexcept {
                    _at = _state->next(_at + _state->pattern.size());
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
                    return _state->mapped.at[_at];
                }

                // A match takes whole characters, so the one past it
                // begins after it
                size_t size() const noexcept {
                    return _state->mapped.at[_at + _state->pattern.size()] - pos();
                }

            private:
                friend class match_range;

                iterator(tracked_ptr<const state> s, size_t at) noexcept
                : _state(std::move(s))
                , _at(at) {
                }

                tracked_ptr<const state> _state;
                size_t _at = npos;
            };

            using const_iterator = iterator;

            // An empty text and an empty pattern, held as the other
            // constructors hold theirs: the range has no occurrence, and
            // its text and its pattern are there to be asked
            match_range() noexcept
            : match_range(string(), string()) {
            }

            match_range(const string& text, const string& pattern) noexcept
            : match_range(text, searcher_type(pattern)) {
            }

            match_range(const string& text, searcher_type pattern) noexcept
            : _state(make_tracked<state>(text, std::move(pattern))) {
            }

            // A copy shares the walk's state; a range moved from is the
            // empty one, as the one made with nothing
            match_range(const match_range&) = default;
            match_range& operator=(const match_range&) = default;

            match_range(match_range&& other) noexcept
            : _state(other._state) {
                other._reset();
            }

            match_range& operator=(match_range&& other) noexcept {
                if (this != &other) {
                    _state = other._state;
                    other._reset();
                }
                return *this;
            }

            // A piece of a text is kept as that piece, which copies
            // those bytes: the positions this answers with are the
            // piece's own, and there is no way to name a piece of a
            // managed buffer without a raw pointer into it
            match_range(const slice<const char>& text, searcher_type pattern)
            : match_range(string(text.data(), text.size()), std::move(pattern)) {
            }

            // A C text — a literal among them, which a string and a
            // slice would both take — as detail::c_text reads it
            template<size_t N>
            match_range(const char (&text)[N], searcher_type pattern)
            : match_range(c_string(text), std::move(pattern)) {
            }

            template<class P>
            requires std::same_as<P, const char*> || std::same_as<P, char*>
            match_range(P text, searcher_type pattern)
            : match_range(c_string(text), std::move(pattern)) {
            }

            iterator begin() const noexcept {
                return iterator(_state, _state ? _state->next(0) : npos);
            }

            iterator end() const noexcept {
                return iterator(_state, npos);
            }

            bool empty() const noexcept {
                return !_state || _state->next(0) == npos;
            }

            // Walked and counted, not stored
            size_type count() const noexcept {
                if (!_state) {
                    return 0;
                }
                size_t n = 0;
                for (size_t i = _state->next(0); i != npos;
                     i = _state->next(i + _state->pattern.size())) {
                    ++n;
                }
                return n;
            }

            slice<const char> text() const noexcept {
                return _state->text.as_slice();
            }

            const searcher_type& pattern() const noexcept {
                return _state->pattern;
            }

            const mapped_text& points() const noexcept {
                return _state->mapped;
            }

        private:
            void _reset() noexcept {
                _state = make_tracked<state>(string(), searcher_type(string()));
            }

            tracked_ptr<state> _state;
        };
    }

    // fold_searcher: a pattern folded once, for the loop that asks the
    // same question of many texts
    using fold_searcher = detail::mapped_searcher<detail::fold_mapping>;

    // normalized_searcher: a pattern decomposed and put in canonical
    // order once, likewise
    using normalized_searcher = detail::mapped_searcher<detail::nfd_mapping>;

    // folded_text: a text folded once and asked many times — find,
    // contains and count over it cost only the search
    using folded_text = detail::mapped_search<detail::fold_mapping>;

    // normalized_text: a text decomposed once, likewise
    using normalized_text = detail::mapped_search<detail::nfd_mapping>;

    // fold_matches: every occurrence of the pattern in the text without
    // regard to case, the text folded a single time for all of them.
    // Each element is the slice of the original text the match covers:
    //
    //   for (auto m : fold_matches(text, pattern)) { ... }
    //
    // The occurrences do not overlap. Where the position rather than the
    // bytes is wanted, the iterator answers pos() and size().
    using fold_matches = detail::match_range<detail::fold_mapping>;

    // normalized_matches: the same without regard to the way either side
    // was written
    using normalized_matches = detail::match_range<detail::nfd_mapping>;

    // The first occurrence of the pattern without regard to case: the
    // bytes of the text it covers (in the text, not in any folded copy
    // of it), or nothing. Both sides are folded ([case]), so "STRASSE"
    // finds "straße", whose bytes are fewer.
    //
    // This folds the text on every call, so a loop over the occurrences
    // of one pattern is quadratic: fold_matches, or a folded_text kept,
    // is the way to ask more than once.
    inline optional<occurrence> find_fold(const string& text, const string& pattern, size_t from = 0) noexcept {
        detail::lent<detail::mapped_scratch> t;
        detail::lent<detail::mapped_scratch> p;
        detail::map_text_into<detail::fold_mapping>(*t, text.view());
        detail::map_text_into<detail::fold_mapping>(*p, pattern.view());
        return detail::find_occurrence(*t, p->points, from);
    }

    inline bool contains_fold(const string& text, const string& pattern) noexcept {
        return find_fold(text, pattern).has_value();
    }

    // The first occurrence without regard to the way either side was
    // written: both are decomposed and put in canonical order first, so
    // an "é" of one code point finds an "é" of two. The position is in
    // the original text, and its size there. normalized_matches is the
    // way to ask for all of them, for the same reason.
    inline optional<occurrence> find_normalized(const string& text, const string& pattern, size_t from = 0) noexcept {
        detail::lent<detail::mapped_scratch> t;
        detail::lent<detail::mapped_scratch> p;
        detail::map_text_into<detail::nfd_mapping>(*t, text.view());
        detail::map_text_into<detail::nfd_mapping>(*p, pattern.view());
        return detail::find_occurrence(*t, p->points, from);
    }

    inline bool contains_normalized(const string& text, const string& pattern) noexcept {
        return find_normalized(text, pattern).has_value();
    }
}
