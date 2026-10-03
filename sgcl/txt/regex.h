//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/bytes.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/mixin/enumerable.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/vector.h"
#include "detail/regex_vm.h"
#include "format.h"

#include <algorithm>
#include <memory>
#include <vector>

// Patterns, in the style of RE2: everything a pattern can ask for here can
// be answered in one pass over the text, and nothing else is offered.
//
// The price of that is two features and the reason is worth stating once.
// A backreference — \1 for "whatever group one matched" — and a lookaround
// — (?=...) for "and this holds here too" — both need the engine to be
// able to go back and try again, and an engine that can go back can be
// made to go back exponentially often: (a+)+b over thirty a's and no b is
// a second of work in Perl, in Python and in std::regex, and over forty
// it is a fortnight. Every one of those is a denial of service waiting for
// a pattern or a text from outside the program. So the parser refuses them
// by name, with that sentence, rather than accepting them and hoping.
//
// What is left is the whole of what most patterns are written for, and it
// costs the length of the text times the length of the pattern, whatever
// the two are. The machine is Pike's (detail/regex_vm.h): every
// alternative alive at once, in the order a backtracking engine would have
// tried them, so the answers are the ones Perl and Python give —
// leftmost-first, greedy and lazy quantifiers meaning what they mean
// there — and only the time is different.
//
// The text is UTF-8 and the walk is over code points, not bytes: '.'
// takes one code point however many bytes it is written in, [а-я] is a
// range of Cyrillic letters and not of bytes, \w is a letter in any
// script, and a word boundary never falls inside a character.
//
// A pattern that is a literal is read where the program is compiled, as
// txt::format's is: regex re("(\\d+)"); is checked by the compiler and
// cannot fail at run time. A pattern that arrives while the program runs
// goes through compile(), which hands back either the regex or the reason
// it is not one.
namespace sgcl::txt {
    namespace detail {
        // A compiled pattern: the program the machine runs and the text
        // it was made from, which the group names point into. It lives in
        // a managed object, so a regex is one word to copy and a match
        // keeps alive exactly what it needs to answer questions about
        // itself after the regex it came from is gone.
        struct regex_state {
            program prog;
            string pattern;
        };

        // Where the groups of a match begin and end, two positions to a
        // group. Up to four groups they are in the match itself and past
        // that in plain memory: positions are no pointers, so nothing
        // here is the collector's, and a match with groups — every one
        // that is found — no longer makes a managed buffer to carry them.
        //
        // Only the positions there are are written and copied: a match of
        // a pattern without groups — most of the ones a loop over all()
        // makes — carries the room and does nothing with it.
        class match_slots {
        public:
            match_slots() noexcept {
            }

            match_slots(const match_slots& other) noexcept {
                _copy(other);
            }

            match_slots& operator=(const match_slots& other) noexcept {
                if (this != &other) {
                    _copy(other);
                }
                return *this;
            }

            void assign(const size_t* from, size_t n) noexcept {
                _size = n;
                if (n <= Inline) {
                    sgcl::detail::copy_bytes(_inline, from, n * sizeof(size_t));
                } else {
                    _more.reset(new size_t[n]);
                    sgcl::detail::copy_bytes(_more.get(), from, n * sizeof(size_t));
                }
            }

            size_t size() const noexcept {
                return _size;
            }

            size_t operator[](size_t i) const noexcept {
                return _size <= Inline ? _inline[i] : _more[i];
            }

        private:
            static constexpr size_t Inline = 8;

            void _copy(const match_slots& other) noexcept {
                if (other._size <= Inline) {
                    _size = other._size;
                    sgcl::detail::copy_bytes(_inline, other._inline, _size * sizeof(size_t));
                } else {
                    assign(other._more.get(), other._size);
                }
            }

            size_t _inline[Inline];    // the first _size of them, when there are no more than Inline
            size_t _size = 0;
            std::unique_ptr<size_t[]> _more;
        };

        // The positions a walk fills on the stack, the whole match's two
        // and the groups' that a match keeps inline; a pattern with more
        // groups takes them from plain memory
        inline constexpr size_t InlineCaps = 10;
    }

    // Why a pattern is not one: the sentence and where in the pattern it
    // went wrong. The sentence is the point — "a lookahead: this engine
    // matches in time linear in the length of the text" tells a reader
    // what to do, and an empty optional does not.
    class regex_error {
    public:
        regex_error(const string& message, size_t offset) noexcept
        : _message(message)
        , _offset(offset) {
        }

        string message() const noexcept {
            return _message;
        }

        // The byte of the pattern
        size_t offset() const noexcept {
            return _offset;
        }

    private:
        string _message;
        size_t _offset;
    };

    // What a match found: the whole of it and each group, as slices of the
    // text it was found in. A slice holds the object its characters live
    // in, so a match outlives the string it came from and a loop over the
    // matches of a temporary is safe — which a std::smatch over a
    // std::string_view is not.
    class match {
    public:
        match() noexcept = default;

        // The whole match
        slice<const char> text() const noexcept {
            return _subject.subslice(_begin, _end - _begin);
        }

        size_t begin_at() const noexcept {
            return _begin;
        }

        size_t end_at() const noexcept {
            return _end;
        }

        bool empty() const noexcept {
            return _begin == _end;
        }

        // The number of groups the pattern has, the whole match not
        // counted: group(1) to group(group_count()) are the ones there are
        size_t group_count() const noexcept {
            return _slots.size() / 2;
        }

        // Group n, or the whole match for 0. A group that took no part in
        // the match — the second branch of (a)|(b) — is nothing, which an
        // empty group is not: (a?) that matched nothing is an empty
        // slice, and that difference is why this is an optional.
        optional<slice<const char>> group(size_t n) const noexcept {
            if (!n) {
                return text();
            }
            if (n > group_count()) {
                return nullopt;
            }
            size_t from = _slots[2 * (n - 1)];
            size_t to = _slots[2 * (n - 1) + 1];
            if (from == detail::NoPos || to == detail::NoPos) {
                return nullopt;
            }
            return _subject.subslice(from, to - from);
        }

        optional<slice<const char>> group(const string& name) const noexcept {
            auto n = _index_of(name.view());
            return n ? group(*n) : nullopt;
        }

        // A C text: an array up to its first NUL or its end, a pointer up
        // to its NUL (detail::c_text)
        template<size_t N>
        optional<slice<const char>> group(const char (&name)[N]) const noexcept {
            auto n = _index_of(detail::c_text(name).view());
            return n ? group(*n) : nullopt;
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        optional<slice<const char>> group(P name) const noexcept {
            auto n = _index_of(detail::c_text(name).view());
            return n ? group(*n) : nullopt;
        }

        // The same, with a group that took no part reading as empty, for
        // the caller who does not care about the difference
        slice<const char> operator[](size_t n) const noexcept {
            auto g = group(n);
            return g ? *g : slice<const char>();
        }

        // The text the match was found in, whole
        const slice<const char>& subject() const noexcept {
            return _subject;
        }


    private:
        friend class regex;
        friend class regex_matches;

        optional<size_t> _index_of(std::string_view name) const noexcept {
            if (!_state) {
                return nullopt;
            }
            for (const auto& g : _state->prog.names) {
                if (_state->pattern.view().substr(g.at, g.size) == name) {
                    return size_t(g.group);
                }
            }
            return nullopt;
        }

        // The slots the machine filled, turned into a match. The state is
        // held only where the pattern has groups or names, so a pattern
        // with neither — which is most of what contains() and split() are
        // given — costs no reference count at all.
        static match _made(const tracked_ptr<const detail::regex_state>& state,
                           const slice<const char>& subject, const size_t* caps) noexcept {
            match m;
            m._subject = subject;
            m._begin = caps[0];
            m._end = caps[1];
            size_t groups = state->prog.groups;
            if (groups) {
                m._slots.assign(caps + 2, 2 * groups);
            }
            if (groups || !state->prog.names.empty()) {
                m._state = state;
            }
            return m;
        }

        slice<const char> _subject;
        tracked_ptr<const detail::regex_state> _state;
        detail::match_slots _slots;
        size_t _begin = 0;
        size_t _end = 0;
    };

    class regex;

    // Every match of a pattern in a text, one after another and never
    // overlapping: a range of the library, as graphemes(s) and words(s)
    // are, decided as it is walked rather than gathered into a container
    // first. A match of no width — what (?:)|x or \b finds — moves the
    // search on by one code point, or the range would stand still.
    //
    // The iterator carries the pattern and the text itself rather than a
    // pointer back to the range, so it is safe on its own: a copy of it
    // outlives the range it came from, and the text it walks is held by
    // the slice it holds.
    class regex_matches
    : public mixin::enumerable<regex_matches> {
    public:
        using value_type = match;
        using size_type = size_t;

        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = match;
            using difference_type = ptrdiff_t;
            using reference = const match&;
            using pointer = const match*;

            iterator() noexcept = default;

            // A move is a copy, as it is of every handle of the library:
            // the iterator moved from stands where it stood and can still
            // be stepped (a moved shared_ptr would have left it without
            // its machine)
            iterator(const iterator&) noexcept = default;
            iterator& operator=(const iterator&) noexcept = default;

            const match& operator*() const noexcept {
                return _match;
            }

            const match* operator->() const noexcept {
                return &_match;
            }

            iterator& operator++() noexcept {
                _step(_next);
                return *this;
            }

            iterator operator++(int) noexcept {
                iterator t = *this;
                ++*this;
                return t;
            }

            friend bool operator==(const iterator& a, const iterator& b) noexcept {
                return a._done == b._done && (a._done || a._match.begin_at() == b._match.begin_at());
            }

        private:
            friend class regex_matches;

            iterator(tracked_ptr<const detail::regex_state> state, const slice<const char>& text) noexcept
            : _state(std::move(state))
            , _text(text) {
                if (_state) {
                    // One machine for the whole walk rather than one a
                    // match: its scratch is sized from the program and
                    // does not change, and building it was 40 ns of the
                    // 151 a match cost. Copies of an iterator share it,
                    // which is safe because a search is self-contained and
                    // a range is walked by one iterator at a time.
                    //
                    // Shared rather than managed. The machine is a plain
                    // engine that holds a reference to the program and a
                    // string_view over the characters, which are two raw
                    // pointers into managed memory, and a managed object is
                    // the one place such a pointer may not live: the
                    // collector classified the words as data and then found
                    // addresses of live objects in them, and said so on
                    // stderr in every build with assertions on. It never had
                    // to be managed to be safe — what keeps the program and
                    // the characters alive is _state and _text, which sit
                    // beside it in the iterator and are copied with it, so
                    // the machine cannot outlive what it points into however
                    // the iterator is passed about. An iterator is four words
                    // to copy either way.
                    _machine = std::make_shared<detail::matcher>(
                        _state->prog, std::string_view(_text.data(), _text.size()),
                        2 * (size_t(_state->prog.groups) + 1));
                }
                _step(0);
            }

            void _step(size_t from) noexcept {
                if (!_state || from > _text.size()) {
                    _done = true;
                    return;
                }
                size_t ncap = 2 * (size_t(_state->prog.groups) + 1);
                size_t room[detail::InlineCaps];
                std::vector<size_t> wider;
                size_t* caps = room;
                if (ncap > detail::InlineCaps) {
                    wider.resize(ncap);
                    caps = wider.data();
                }
                if (!_machine->run(from, caps)) {
                    _done = true;
                    return;
                }
                _match = match::_made(_state, _text, caps);
                _next = caps[1];
                if (caps[0] == caps[1]) {
                    // no width: the next search starts a whole code point
                    // on, or this match would be found again forever
                    _next += _next < _text.size()
                        ? utf8::decode(std::string_view(_text.data(), _text.size()), _next).second
                        : 1;
                }
                _done = false;
            }

            tracked_ptr<const detail::regex_state> _state;
            std::shared_ptr<detail::matcher> _machine;
            slice<const char> _text;
            match _match;
            size_t _next = 0;
            bool _done = true;
        };

        using const_iterator = iterator;

        regex_matches() noexcept = default;

        // A copy is the range again; a range moved from is the empty one,
        // as the one made with nothing
        regex_matches(const regex_matches&) = default;
        regex_matches& operator=(const regex_matches&) = default;

        regex_matches(regex_matches&& other) noexcept
        : _state(other._state)
        , _text(other._text) {
            other._state = nullptr;
            other._text = slice<const char>();
        }

        regex_matches& operator=(regex_matches&& other) noexcept {
            if (this != &other) {
                _state = other._state;
                _text = other._text;
                other._state = nullptr;
                other._text = slice<const char>();
            }
            return *this;
        }
        regex_matches(const regex& re, const slice<const char>& text) noexcept;

        // A C text, copied into a string the range then holds: the
        // slice of a literal's array would count its terminating zero
        template<size_t N>
        regex_matches(const regex& re, const char (&text)[N])
        : regex_matches(re, detail::c_string(text).as_slice()) {
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        regex_matches(const regex& re, P text)
        : regex_matches(re, detail::c_string(text).as_slice()) {
        }

        iterator begin() const noexcept {
            return iterator(_state, _text);
        }

        iterator end() const noexcept {
            return iterator();
        }

        bool empty() const noexcept {
            return begin() == end();
        }

        // Walked and counted, not stored
        size_type count() const noexcept {
            size_t n = 0;
            for (auto it = begin(); it != end(); ++it) {
                ++n;
            }
            return n;
        }

        const slice<const char>& text() const noexcept {
            return _text;
        }

    private:
        tracked_ptr<const detail::regex_state> _state;
        slice<const char> _text;
    };

    namespace detail {
        // The pattern of a literal, read where the program is compiled.
        // Nothing the reading builds is kept — the machine is put
        // together again where the program runs, which costs a few
        // hundred nanoseconds once — but a pattern that cannot be one is
        // an error of the compiler, with the pattern in hand, rather than
        // an empty result at the customer's. This is what format.h does
        // with its own pattern, and for the same reason.
        class regex_pattern {
        public:
            template<class S>
            requires std::is_convertible_v<const S&, std::string_view>
            consteval regex_pattern(const S& text)
            : _text(text) {
                regex_tree tree;
                auto fault = regex_parser(_text).parse(tree);
                if (!fault) {
                    program prog;
                    fault = regex_compiler(tree, prog).compile();
                }
                // The compiler's message points at one of these three
                // lines and the sentence on it is the reason; where in
                // the pattern it went wrong is what regex::compile()
                // hands back where the program runs.
                if (asks_for_backtracking(fault.fault)) {
                    throw "sgcl::txt::regex: a backreference or a lookaround — this engine matches in time linear in the length of the text and can have neither";
                }
                if (fault.fault == regex_fault::too_large || fault.fault == regex_fault::too_deep
                    || fault.fault == regex_fault::too_many_groups
                    || fault.fault == regex_fault::repeat_too_large) {
                    throw "sgcl::txt::regex: the pattern is past one of the engine's limits";
                }
                if (fault) {
                    throw "sgcl::txt::regex: the pattern is malformed — regex::compile() gives the place and the reason";
                }
            }

            constexpr std::string_view view() const noexcept {
                return _text;
            }

        private:
            std::string_view _text;
        };
    }

    // A compiled pattern: one word to copy and safe to share between
    // threads, since nothing in it changes after it is built and the
    // machine that runs it keeps its own scratch for one search.
    class regex {
    public:
        // From a literal, which the compiler has already read
        regex(const detail::regex_pattern& pattern) noexcept
        : _state(_built(string(pattern.view().data(), pattern.view().size()))) {
        }

        // From a pattern the program only has where it runs: either the
        // regex or the reason it is not one
        static expected<regex, regex_error> compile(const string& pattern) noexcept {
            return _compile(pattern);
        }

        static expected<regex, regex_error> compile(const slice<const char>& pattern) {
            return _compile(string(pattern.data(), pattern.size()));
        }

        // A C text — a literal among them, which a string and a slice
        // would both take — as detail::c_text reads it; so every member
        // below that takes a text
        template<size_t N>
        static expected<regex, regex_error> compile(const char (&pattern)[N]) {
            return _compile(detail::c_string(pattern));
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        static expected<regex, regex_error> compile(P pattern) {
            return _compile(detail::c_string(pattern));
        }

        const string& pattern() const noexcept {
            return _state->pattern;
        }

        // How many capturing groups the pattern has
        size_t group_count() const noexcept {
            return _state->prog.groups;
        }

        // The number a name stands for, or nothing
        optional<size_t> group_index(const string& name) const noexcept {
            for (const auto& g : _state->prog.names) {
                if (_state->pattern.view().substr(g.at, g.size) == name.view()) {
                    return size_t(g.group);
                }
            }
            return nullopt;
        }

        // Whether the whole text is a match, its first byte to its last.
        // Not find() with the ends compared afterwards: the machine stops
        // at the match a backtracking engine would have preferred, which
        // may be the shorter one, and the question here is whether the
        // longer one exists at all.
        bool full_match(const slice<const char>& text) const noexcept {
            size_t caps[2];
            detail::matcher machine(detail::LentBlock{}, _state->prog, _view(text), 2);
            return machine.run(0, caps, true);
        }

        bool full_match(const string& text) const noexcept {
            return full_match(text.as_slice());
        }

        template<size_t N>
        bool full_match(const char (&text)[N]) const noexcept {
            return full_match(detail::c_text(text));
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        bool full_match(P text) const noexcept {
            return full_match(detail::c_text(text));
        }

        // Whether the pattern is anywhere in the text
        bool contains(const slice<const char>& text) const noexcept {
            size_t caps[2];
            detail::matcher machine(detail::LentBlock{}, _state->prog, _view(text), 2);
            return machine.run(0, caps);
        }

        bool contains(const string& text) const noexcept {
            return contains(text.as_slice());
        }

        template<size_t N>
        bool contains(const char (&text)[N]) const noexcept {
            return contains(detail::c_text(text));
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        bool contains(P text) const noexcept {
            return contains(detail::c_text(text));
        }

        // The first match at or after `from`, leftmost and then by the
        // order the pattern puts its branches in
        optional<match> find(const slice<const char>& text, size_t from = 0) const noexcept {
            if (from > text.size()) {
                return nullopt;
            }
            if (from && from < text.size()) {
                from = _point_start(text, from);
            }
            size_t ncap = _ncap();
            detail::matcher machine(detail::LentBlock{}, _state->prog, _view(text), ncap);
            size_t room[detail::InlineCaps];
            std::vector<size_t> wider;
            size_t* caps = room;
            if (ncap > detail::InlineCaps) {
                wider.resize(ncap);
                caps = wider.data();
            }
            if (!machine.run(from, caps)) {
                return nullopt;
            }
            return match::_made(_state, text, caps);
        }

        optional<match> find(const string& text, size_t from = 0) const noexcept {
            return find(text.as_slice(), from);
        }

        // The match holds its text, so a C text is copied into one
        template<size_t N>
        optional<match> find(const char (&text)[N], size_t from = 0) const {
            return find(detail::c_string(text), from);
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        optional<match> find(P text, size_t from = 0) const {
            return find(detail::c_string(text), from);
        }

        // Every match, one after another and never overlapping
        regex_matches all(const slice<const char>& text) const noexcept {
            return regex_matches(*this, text);
        }

        regex_matches all(const string& text) const noexcept {
            return regex_matches(*this, text.as_slice());
        }

        template<size_t N>
        regex_matches all(const char (&text)[N]) const {
            return regex_matches(*this, text);
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        regex_matches all(P text) const {
            return regex_matches(*this, text);
        }

        size_t count(const slice<const char>& text) const noexcept {
            return all(text).count();
        }

        size_t count(const string& text) const noexcept {
            return count(text.as_slice());
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

        // Every match replaced. In the replacement $1 is what group one
        // matched, ${name} what a named group matched, $0 the whole match
        // and $$ a dollar of its own; anything else stands for itself.
        string replace(const slice<const char>& text, const slice<const char>& with) const {
            return _replace(text, _view(with), npos);
        }

        string replace(const string& text, const string& with) const {
            return _replace(text.as_slice(), with.view(), npos);
        }

        template<size_t N, size_t M>
        string replace(const char (&text)[N], const char (&with)[M]) const {
            return _replace(detail::c_text(text), _view(detail::c_text(with)), npos);
        }

        // Two pointers; a pointer is taken by reference here, so that an
        // array does not decay into one (it goes to the pair above)
        template<class P, class Q>
        requires (std::same_as<P, const char*> || std::same_as<P, char*>)
              && (std::same_as<Q, const char*> || std::same_as<Q, char*>)
        string replace(const P& text, const Q& with) const {
            return _replace(detail::c_text(text), _view(detail::c_text(with)), npos);
        }

        // Only the first match
        string replace_first(const string& text, const string& with) const {
            return _replace(text.as_slice(), with.view(), 1);
        }

        string replace_first(const slice<const char>& text, const slice<const char>& with) const {
            return _replace(text, _view(with), 1);
        }

        template<size_t N, size_t M>
        string replace_first(const char (&text)[N], const char (&with)[M]) const {
            return _replace(detail::c_text(text), _view(detail::c_text(with)), 1);
        }

        // Two pointers; a pointer is taken by reference here, so that an
        // array does not decay into one (it goes to the pair above)
        template<class P, class Q>
        requires (std::same_as<P, const char*> || std::same_as<P, char*>)
              && (std::same_as<Q, const char*> || std::same_as<Q, char*>)
        string replace_first(const P& text, const Q& with) const {
            return _replace(detail::c_text(text), _view(detail::c_text(with)), 1);
        }

        // The pieces of the text between the matches, as slices of it. A
        // limit of n gives at most n pieces, the last of them the whole
        // rest of the text; zero is no limit.
        vector<slice<const char>> split(const slice<const char>& text, size_t limit = 0) const noexcept {
            vector<slice<const char>> out;
            // where the next piece begins, and where the next search
            // does: a match of no width moves the second on by a whole
            // code point and leaves the first where the match was, or
            // the code point between two such matches would be lost
            size_t piece = 0;
            size_t search = 0;
            size_t caps[2];
            detail::matcher machine(detail::LentBlock{}, _state->prog, _view(text), 2);
            while (!limit || out.size() + 1 < limit) {
                if (search > text.size() || !machine.run(search, caps)) {
                    break;
                }
                bool nothing = caps[0] == caps[1];
                out.push_back(text.subslice(piece, caps[0] - piece));
                piece = caps[1];
                search = caps[1] + (nothing ? _one_point(text, caps[1]) : 0);
                if (nothing && caps[1] >= text.size()) {
                    break;
                }
            }
            out.push_back(text.subslice(piece, text.size() - piece));
            return out;
        }

        vector<slice<const char>> split(const string& text, size_t limit = 0) const noexcept {
            return split(text.as_slice(), limit);
        }

        // The pieces hold their text, so a C text is copied into one
        template<size_t N>
        vector<slice<const char>> split(const char (&text)[N], size_t limit = 0) const {
            return split(detail::c_string(text), limit);
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        vector<slice<const char>> split(P text, size_t limit = 0) const {
            return split(detail::c_string(text), limit);
        }

        // How many instructions the pattern spelled out to, for whoever
        // wants to know what a {n,m} cost
        size_t program_size() const noexcept {
            return _state->prog.insts.size();
        }

    private:
        friend class regex_matches;

        explicit regex(const tracked_ptr<const detail::regex_state>& state) noexcept
        : _state(state) {
        }

        static std::string_view _view(const slice<const char>& s) noexcept {
            return std::string_view(s.data(), s.size());
        }

        size_t _ncap() const noexcept {
            return 2 * (size_t(_state->prog.groups) + 1);
        }

        // For a pattern already known to be one: the compiler read the
        // literal, so nothing here can fail
        static tracked_ptr<const detail::regex_state> _built(const string& pattern) noexcept {
            auto state = make_tracked<detail::regex_state>();
            state->pattern = pattern;
            detail::regex_tree tree;
            detail::regex_parser(state->pattern.view()).parse(tree);
            detail::regex_compiler(tree, state->prog).compile();
            return tracked_ptr<const detail::regex_state>(std::move(state));
        }

        static expected<regex, regex_error> _compile(const string& pattern) noexcept {
            detail::regex_tree tree;
            auto fault = detail::regex_parser(pattern.view()).parse(tree);
            detail::program prog;
            if (!fault) {
                fault = detail::regex_compiler(tree, prog).compile();
            }
            if (fault) {
                return unexpected(regex_error(
                    format("sgcl::txt::regex: {} (at byte {} of the pattern)",
                           detail::text_of(fault.fault), fault.at),
                    fault.at));
            }
            auto state = make_tracked<detail::regex_state>();
            // the group names are where they stand in the pattern, so the
            // state has to hold the very text the tree was read from
            state->pattern = pattern;
            state->prog = std::move(prog);
            return regex(tracked_ptr<const detail::regex_state>(std::move(state)));
        }

        // Where a search from byte `at` starts: there, or past the code
        // point `at` stands inside. The walk is over code points and a
        // match never begins inside one; a byte that belongs to no whole
        // code point (a lone continuation byte, a cut sequence) is one of
        // its own, as the machine reads it, and stays where it is.
        static size_t _point_start(const slice<const char>& text, size_t at) noexcept {
            if ((uint8_t(text[at]) & 0xC0) != 0x80) {
                return at;
            }
            std::string_view view(text.data(), text.size());
            for (size_t back = 1; back <= 3 && back <= at; ++back) {
                if ((uint8_t(view[at - back]) & 0xC0) != 0x80) {
                    size_t n = utf8::decode(view, at - back).second;
                    return n > back ? at - back + n : at;
                }
            }
            return at;
        }

        static size_t _one_point(const slice<const char>& text, size_t at) noexcept {
            if (at >= text.size()) {
                return 1;
            }
            return utf8::decode(std::string_view(text.data(), text.size()), at).second;
        }

        // $1, ${12}, ${name}, $0 and $$ in a replacement. A group the
        // pattern does not have puts nothing in, as it does in Go: a
        // replacement is written by a person and a missing group is far
        // more often a typing mistake than a request for the text "$7".
        void _expand(std::string& out, std::string_view with, std::string_view text,
                     const size_t* caps, size_t ncap) const noexcept {
            for (size_t i = 0; i < with.size();) {
                if (with[i] != '$') {
                    size_t next = with.find('$', i);
                    out.append(with.substr(i, next - i));
                    i = next == std::string_view::npos ? with.size() : next;
                    continue;
                }
                if (i + 1 >= with.size()) {
                    out.push_back('$');
                    break;
                }
                char c = with[i + 1];
                if (c == '$') {
                    out.push_back('$');
                    i += 2;
                    continue;
                }
                size_t group = npos;
                size_t after = i;
                if (c == '{') {
                    size_t close = with.find('}', i + 2);
                    if (close == std::string_view::npos) {
                        out.push_back('$');
                        ++i;
                        continue;
                    }
                    std::string_view name = with.substr(i + 2, close - i - 2);
                    group = _group_of(name);
                    after = close + 1;
                } else if (c >= '0' && c <= '9') {
                    size_t k = i + 1;
                    size_t value = 0;
                    while (k < with.size() && with[k] >= '0' && with[k] <= '9') {
                        value = _digit(value, with[k]);
                        ++k;
                    }
                    group = value;
                    after = k;
                } else {
                    out.push_back('$');
                    ++i;
                    continue;
                }
                if (group != npos && 2 * group + 1 < ncap) {
                    size_t from = caps[2 * group];
                    size_t to = caps[2 * group + 1];
                    if (from != detail::NoPos && to != detail::NoPos) {
                        out.append(text.substr(from, to - from));
                    }
                }
                i = after;
            }
        }

        // One digit onto a group number, held down so that it cannot wrap.
        // $9223372036854775808 used to reach 2*group + 1 == 1 and put the
        // whole match in, and ${9223372036854775809} put group one in, where
        // a number the pattern has no group for is meant to put nothing in.
        // Anything past the engine's own limit is already such a number, so
        // stopping there loses no group that could have existed.
        static constexpr size_t _digit(size_t value, char c) noexcept {
            if (value > detail::MaxRegexGroups) {
                return value;
            }
            return value * 10 + size_t(c - '0');
        }

        size_t _group_of(std::string_view name) const noexcept {
            if (!name.empty() && name[0] >= '0' && name[0] <= '9') {
                size_t value = 0;
                for (char c : name) {
                    if (c < '0' || c > '9') {
                        return npos;
                    }
                    value = _digit(value, c);
                }
                return value;
            }
            for (const auto& g : _state->prog.names) {
                if (_state->pattern.view().substr(g.at, g.size) == name) {
                    return size_t(g.group);
                }
            }
            return npos;
        }

        // The text built into a std::string and copied once into a
        // managed one. Two passes would tell the size beforehand, as
        // note 158 has the rest of the module do, but here the second
        // pass is the matching itself and that costs far more than the
        // copy it would save.
        string _replace(const slice<const char>& text, std::string_view with, size_t limit) const {
            std::string out;
            out.reserve(text.size());
            std::string_view view = _view(text);
            size_t ncap = _ncap();
            detail::matcher machine(detail::LentBlock{}, _state->prog, view, ncap);
            size_t room[detail::InlineCaps];
            std::vector<size_t> wider;
            size_t* caps = room;
            if (ncap > detail::InlineCaps) {
                wider.resize(ncap);
                caps = wider.data();
            }
            size_t at = 0;
            size_t done = 0;
            while (done < limit && at <= view.size() && machine.run(at, caps)) {
                out.append(view.substr(at, caps[0] - at));
                _expand(out, with, view, caps, ncap);
                ++done;
                at = caps[1];
                if (caps[0] == caps[1]) {
                    if (caps[1] >= view.size()) {
                        break;
                    }
                    size_t width = utf8::decode(view, caps[1]).second;
                    out.append(view.substr(caps[1], width));
                    at = caps[1] + width;
                }
            }
            out.append(view.substr(std::min(at, view.size())));
            return string(out.data(), out.size());
        }

        tracked_ptr<const detail::regex_state> _state;
    };

    inline regex_matches::regex_matches(const regex& re, const slice<const char>& text) noexcept
    : _state(re._state)
    , _text(text) {
    }
}
