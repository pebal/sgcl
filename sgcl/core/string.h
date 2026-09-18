//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "detail/string_data.h"
#include "tracked_ptr.h"

#include <algorithm>
#include <charconv>
#include <compare>
#include <functional>
#include <iosfwd>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace sgcl {
    // An immutable string on the managed heap, one word: a pointer to an
    // object holding the length, the characters and a terminator, and the
    // hash once asked for, of exactly that size (detail/string_data.h).
    // What a string is in Java and Go rather than in C++: made once,
    // never modified, shared by copying the word, compared and hashed by
    // its contents, reclaimed by the collector, no destructor (the sweep
    // frees the slot and nothing runs), no reference count. Copying one
    // between managed objects costs a word and the barrier, wherever it
    // is long; a std::string past its small buffer costs an allocation
    // per copy and a free per destruction, in the sweep. Creating one
    // costs a managed allocation (README: Allocation), where a
    // std::string of a few characters costs none: a string is for text
    // that is kept, shared and compared, not for a scratch buffer, which
    // std::string remains. No small-string optimization: the word is the
    // whole of it, and the empty string is null.
    // The interface is the read side of std::string (and all of
    // std::string_view): size, data, c_str, [], at, front, back, the
    // iterators, compare, starts_with, ends_with, contains, the finds,
    // substr (a new string), the comparisons and <=> with a string, a
    // string_view or a literal, operator+ (a new string), std::hash
    // (computed once, kept in the object), operator<<, conversions to string_view and to
    // std::string; the constructors from a literal, a string_view, a
    // std::string, a range, (n, ch); no mutation, no capacity. Past
    // std::string, what the strings of Go and Java have, each a new
    // string (or the same object when nothing changes): split and fields
    // (a range of views, the class pieces), join, trim, trim_left,
    // trim_right, trim_prefix, trim_suffix, replace, repeat, to_lower,
    // to_upper. The word
    // is a tracked_ptr, so a string lives where one may: on a stack or in
    // a managed object.
    // The length is kept in 32 bits: a string holds up to 4 G characters.
    template<class CharT, class Traits = std::char_traits<CharT>>
    class basic_string_view;

    namespace detail {
        // A string over the word of its object, for the library's own
        // structures that keep a string's object by its address (an
        // atomic<string> loads one, an intern pool finds one through a
        // weak pointer): the word is the object's start, as the maker
        // returned it, nothing else
        struct StringAccess {
            template<class S>
            static S over(tracked_ptr<const void> word) noexcept {
                return S(std::move(word));
            }
        };
    }

    template<class CharT, class Traits = std::char_traits<CharT>>
    class basic_string {
        static_assert(sizeof(detail::StringHeader) % sizeof(CharT) == 0, "the header is a whole number of characters");

        using Maker = detail::StringMaker;
        using Word = typename Maker::Word;
        static constexpr size_t HeaderChars = sizeof(detail::StringHeader) / sizeof(CharT);

    public:
        using traits_type = Traits;
        using value_type = CharT;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = const CharT&;
        using const_reference = const CharT&;
        using pointer = const CharT*;
        using const_pointer = const CharT*;
        using iterator = const CharT*;
        using const_iterator = const CharT*;
        using reverse_iterator = std::reverse_iterator<const_iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using view_type = std::basic_string_view<CharT, Traits>;

        static constexpr size_type npos = view_type::npos;

        class pieces;   // what split and fields return: a range of views, below

        // The constructors: from what a std::string is made of, the
        // characters copied once into the string's object. The empty
        // string allocates nothing.
        constexpr basic_string() noexcept = default;

        constexpr basic_string(std::nullptr_t) = delete;

        basic_string(const CharT* s)
        : basic_string(view_type(s)) {
        }

        basic_string(const CharT* s, size_type n)
        : basic_string(view_type(s, n)) {
        }

        basic_string(view_type s)
        : _word(s.empty() ? Word() : Maker::make(s)) {
        }

        // From any type a string_view is made of (a std::string, a
        // std::string_view), as std::string does
        template<class V>
        requires std::is_convertible_v<const V&, view_type> && (!std::is_convertible_v<const V&, const CharT*>) && (!std::is_same_v<std::remove_cvref_t<V>, basic_string>)
        explicit basic_string(const V& v)
        : basic_string(view_type(v)) {
        }

        basic_string(size_type n, CharT c)
        : basic_string(std::basic_string<CharT, Traits>(n, c)) {
        }

        template<std::input_iterator It>
        basic_string(It first, It last)
        : basic_string(std::basic_string<CharT, Traits>(first, last)) {
        }

        basic_string(std::initializer_list<CharT> il)
        : basic_string(view_type(il.begin(), il.size())) {
        }

        // From a view that holds a string's object: that object when the
        // view is the whole of it (no copy), a new string of the
        // characters otherwise
        explicit basic_string(const basic_string_view<CharT, Traits>& v)
        : _word(v._whole() ? v._word : (v.empty() ? Word() : Maker::make(view_type(v)))) {
        }

        basic_string(const basic_string&) noexcept = default;
        basic_string(basic_string&&) noexcept = default;

        basic_string& operator=(const basic_string&) noexcept = default;
        basic_string& operator=(basic_string&&) noexcept = default;

        basic_string& operator=(const CharT* s) {
            return *this = basic_string(s);
        }

        basic_string& operator=(view_type s) {
            return *this = basic_string(s);
        }

        template<class V>
        requires std::is_convertible_v<const V&, view_type> && (!std::is_convertible_v<const V&, const CharT*>) && (!std::is_same_v<std::remove_cvref_t<V>, basic_string>)
        basic_string& operator=(const V& v) {
            return *this = basic_string(v);
        }

        // The characters, terminated; the empty string's are a terminator
        const CharT* data() const noexcept {
            return _word ? _chars() : &_empty;
        }

        const CharT* c_str() const noexcept {
            return data();
        }

        size_type size() const noexcept {
            return _word ? _header()->size : 0;
        }

        size_type length() const noexcept {
            return size();
        }

        bool empty() const noexcept {
            return !_word;
        }

        static constexpr size_type max_size() noexcept {
            return UINT32_MAX;
        }

        operator view_type() const noexcept {
            return _view();
        }

        // The string as a view that holds the object (basic_string_view,
        // below): the whole of it, or the characters [pos, pos + n),
        // shared, nothing copied
        basic_string_view<CharT, Traits> view() const noexcept {
            return basic_string_view<CharT, Traits>(_word, 0, (uint32_t)size());
        }

        basic_string_view<CharT, Traits> view(size_type pos, size_type n = npos) const {
            if (pos > size()) {
                throw std::out_of_range("sgcl::basic_string::view");
            }
            return basic_string_view<CharT, Traits>(_word, (uint32_t)pos, (uint32_t)std::min(n, size() - pos));
        }

        // A std::string with the same characters: for the interfaces that
        // want one, and for building a new string
        std::basic_string<CharT, Traits> str() const {
            return std::basic_string<CharT, Traits>(data(), size());
        }

        const CharT& operator[](size_type i) const noexcept {
            assert(i <= size());
            return data()[i];
        }

        const CharT& at(size_type i) const {
            if (i >= size()) {
                throw std::out_of_range("sgcl::basic_string::at");
            }
            return data()[i];
        }

        const CharT& front() const noexcept {
            assert(!empty());
            return data()[0];
        }

        const CharT& back() const noexcept {
            assert(!empty());
            return data()[size() - 1];
        }

        const_iterator begin() const noexcept { return data(); }
        const_iterator end() const noexcept { return data() + size(); }
        const_iterator cbegin() const noexcept { return begin(); }
        const_iterator cend() const noexcept { return end(); }
        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
        const_reverse_iterator crbegin() const noexcept { return rbegin(); }
        const_reverse_iterator crend() const noexcept { return rend(); }

        // The searches and comparisons of std::string_view, over the view
        size_type copy(CharT* dest, size_type n, size_type pos = 0) const { return _view().copy(dest, n, pos); }
        int compare(view_type s) const noexcept { return _view().compare(s); }
        int compare(size_type pos, size_type n, view_type s) const { return _view().compare(pos, n, s); }
        int compare(size_type pos, size_type n, view_type s, size_type pos2, size_type n2) const { return _view().compare(pos, n, s, pos2, n2); }
        int compare(const CharT* s) const noexcept { return _view().compare(s); }
        bool starts_with(view_type s) const noexcept { return _view().starts_with(s); }
        bool starts_with(CharT c) const noexcept { return _view().starts_with(c); }
        bool starts_with(const CharT* s) const noexcept { return _view().starts_with(s); }
        bool ends_with(view_type s) const noexcept { return _view().ends_with(s); }
        bool ends_with(CharT c) const noexcept { return _view().ends_with(c); }
        bool ends_with(const CharT* s) const noexcept { return _view().ends_with(s); }
        bool contains(view_type s) const noexcept { return _view().find(s) != npos; }
        bool contains(CharT c) const noexcept { return _view().find(c) != npos; }
        bool contains(const CharT* s) const noexcept { return _view().find(s) != npos; }
        size_type find(view_type s, size_type pos = 0) const noexcept { return _view().find(s, pos); }
        size_type find(CharT c, size_type pos = 0) const noexcept { return _view().find(c, pos); }
        size_type find(const CharT* s, size_type pos, size_type n) const noexcept { return _view().find(s, pos, n); }
        size_type find(const CharT* s, size_type pos = 0) const noexcept { return _view().find(s, pos); }
        size_type rfind(view_type s, size_type pos = npos) const noexcept { return _view().rfind(s, pos); }
        size_type rfind(CharT c, size_type pos = npos) const noexcept { return _view().rfind(c, pos); }
        size_type rfind(const CharT* s, size_type pos, size_type n) const noexcept { return _view().rfind(s, pos, n); }
        size_type rfind(const CharT* s, size_type pos = npos) const noexcept { return _view().rfind(s, pos); }
        size_type find_first_of(view_type s, size_type pos = 0) const noexcept { return _view().find_first_of(s, pos); }
        size_type find_first_of(CharT c, size_type pos = 0) const noexcept { return _view().find_first_of(c, pos); }
        size_type find_first_of(const CharT* s, size_type pos = 0) const noexcept { return _view().find_first_of(s, pos); }
        size_type find_last_of(view_type s, size_type pos = npos) const noexcept { return _view().find_last_of(s, pos); }
        size_type find_last_of(CharT c, size_type pos = npos) const noexcept { return _view().find_last_of(c, pos); }
        size_type find_last_of(const CharT* s, size_type pos = npos) const noexcept { return _view().find_last_of(s, pos); }
        size_type find_first_not_of(view_type s, size_type pos = 0) const noexcept { return _view().find_first_not_of(s, pos); }
        size_type find_first_not_of(CharT c, size_type pos = 0) const noexcept { return _view().find_first_not_of(c, pos); }
        size_type find_first_not_of(const CharT* s, size_type pos = 0) const noexcept { return _view().find_first_not_of(s, pos); }
        size_type find_last_not_of(view_type s, size_type pos = npos) const noexcept { return _view().find_last_not_of(s, pos); }
        size_type find_last_not_of(CharT c, size_type pos = npos) const noexcept { return _view().find_last_not_of(c, pos); }
        size_type find_last_not_of(const CharT* s, size_type pos = npos) const noexcept { return _view().find_last_not_of(s, pos); }

        // A new string of the characters [pos, pos + n): a copy, the
        // whole string when the range is the whole string
        basic_string substr(size_type pos = 0, size_type n = npos) const {
            if (pos > size()) {
                throw std::out_of_range("sgcl::basic_string::substr");
            }
            if (pos == 0 && n >= size()) {
                return *this;
            }
            return basic_string(_view().substr(pos, n));
        }

        // The pieces between the occurrences of `sep`, in order: an empty
        // piece where two separators meet or one ends the string; the
        // whole string when `sep` does not occur; every character on its
        // own for an empty `sep`; no piece for an empty string. With
        // `max_parts`, at most that many, the last holding the rest of the
        // string; 0 is no limit. A range of views into this string (the
        // class `pieces`, below), computed as it is walked: nothing is
        // copied, `for (std::string_view piece : s.split(','))` allocates
        // nothing, and a container of strings is built from it when the
        // pieces are to be kept: `vector<string> parts(s.split(','))`.
        pieces split(view_type sep, size_type max_parts = 0) const {
            return pieces(*this, basic_string(sep), max_parts, sep.empty() ? pieces::Characters : pieces::Separator);
        }

        pieces split(CharT sep, size_type max_parts = 0) const {
            return pieces(*this, sep, max_parts);
        }

        pieces split(const CharT* sep, size_type max_parts = 0) const {
            return split(view_type(sep), max_parts);
        }

        pieces split(const basic_string& sep, size_type max_parts = 0) const {
            return pieces(*this, sep, max_parts, sep.empty() ? pieces::Characters : pieces::Separator);
        }

        // The words: the pieces between runs of white space (space, tab,
        // newline, vertical tab, form feed, carriage return), none empty
        pieces fields() const {
            return pieces(*this, basic_string(), 0, pieces::Fields);
        }

        // One string of the parts (strings, views, literals: anything a
        // view is made of) with `sep` between each two, built once
        template<std::ranges::input_range R>
        requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
        static basic_string join(R&& parts, view_type sep) {
            std::basic_string<CharT, Traits> s;
            bool first = true;
            for (auto&& part : parts) {
                if (!first) {
                    s.append(sep);
                }
                s.append(view_type(part));
                first = false;
            }
            return basic_string(view_type(s));
        }

        template<std::ranges::input_range R>
        requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
        static basic_string join(R&& parts, CharT sep) {
            return join(std::forward<R>(parts), view_type(&sep, 1));
        }

        template<std::ranges::input_range R>
        requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
        static basic_string join(R&& parts, const CharT* sep) {
            return join(std::forward<R>(parts), view_type(sep));
        }

        // Without the characters of `chars` (white space by default) at
        // both ends, at the start, at the end: the same object when there
        // are none
        basic_string trim() const {
            return trim(_spaces());
        }

        basic_string trim(view_type chars) const {
            auto v = _view();
            auto from = v.find_first_not_of(chars);
            if (from == npos) {
                return basic_string();
            }
            auto to = v.find_last_not_of(chars) + 1;
            return _part(from, to);
        }

        basic_string trim_left() const {
            return trim_left(_spaces());
        }

        basic_string trim_left(view_type chars) const {
            auto from = _view().find_first_not_of(chars);
            return from == npos ? basic_string() : _part(from, size());
        }

        basic_string trim_right() const {
            return trim_right(_spaces());
        }

        basic_string trim_right(view_type chars) const {
            auto to = _view().find_last_not_of(chars);
            return to == npos ? basic_string() : _part(0, to + 1);
        }

        // Without `prefix` at the start (`suffix` at the end) when it is
        // there; the same object when it is not
        basic_string trim_prefix(view_type prefix) const {
            return starts_with(prefix) ? _part(prefix.size(), size()) : *this;
        }

        basic_string trim_suffix(view_type suffix) const {
            return ends_with(suffix) ? _part(0, size() - suffix.size()) : *this;
        }

        // With every occurrence of `from` (the first `count` of them, when
        // given; 0 is every one) replaced by `to`, left to right without
        // overlapping; the same object when `from` is empty or does not
        // occur
        basic_string replace(view_type from, view_type to, size_type count = 0) const {
            auto v = _view();
            auto at = from.empty() ? npos : v.find(from);
            if (at == npos) {
                return *this;
            }
            std::basic_string<CharT, Traits> s;
            size_type pos = 0;
            size_type n = 0;
            while (at != npos) {
                s.append(v, pos, at - pos).append(to);
                pos = at + from.size();
                at = (count && ++n == count) ? npos : v.find(from, pos);
            }
            s.append(v, pos);
            return basic_string(view_type(s));
        }

        basic_string replace(CharT from, CharT to, size_type count = 0) const {
            return replace(view_type(&from, 1), view_type(&to, 1), count);
        }

        // The string `count` times over: empty for 0, the same object for 1
        basic_string repeat(size_type count) const {
            if (count == 0 || empty()) {
                return basic_string();
            }
            if (count == 1) {
                return *this;
            }
            auto v = _view();
            if (v.size() > max_size() / count) {
                throw std::length_error("sgcl::basic_string::repeat");
            }
            std::basic_string<CharT, Traits> s;
            s.reserve(v.size() * count);
            for (size_type i = 0; i < count; ++i) {
                s.append(v);
            }
            return basic_string(view_type(s));
        }

        // With the ASCII letters in lower (upper) case, the other
        // characters as they are; the same object when no letter changes
        basic_string to_lower() const {
            return _mapped(CharT('A'), CharT('Z'), CharT('a') - CharT('A'));
        }

        basic_string to_upper() const {
            return _mapped(CharT('a'), CharT('z'), CharT('A') - CharT('a'));
        }

        void swap(basic_string& o) noexcept {
            std::swap(_word, o._word);
        }

        // The same object, or the same characters: the lengths first,
        // the hashes when both are known, the characters last (the free
        // operators below).
        bool equals(const basic_string& o) const noexcept {
            if (object() == o.object()) {
                return true;
            }
            if (size() != o.size()) {
                return false;
            }
            if (object() && o.object()) {
                auto h = _header()->hash.load(std::memory_order_relaxed);
                auto oh = o._header()->hash.load(std::memory_order_relaxed);
                if (h && oh && h != oh) {
                    return false;
                }
            }
            return _view() == o._view();
        }

        // The hash of the characters, computed the first time and kept in
        // the string's object (0 is "not yet"); the empty string's is a constant
        size_t hash() const noexcept {
            if (!_word) {
                return HashMultiplier;
            }
            auto header = _header();
            auto h = header->hash.load(std::memory_order_relaxed);
            if (!h) {
                h = _fold(std::hash<view_type>()(_view()));
                header->hash.store(h, std::memory_order_relaxed);
            }
            return (size_t)h * HashMultiplier;
        }

        // The hash a string of these characters has: what std::hash of the
        // string is for a string_view or a literal, so that a map keyed by
        // strings is searched with either and no string is made for the
        // search (std::hash<basic_string> is transparent, below)
        static size_t hash_of(view_type s) noexcept {
            return s.empty() ? HashMultiplier : (size_t)_fold(std::hash<view_type>()(s)) * HashMultiplier;
        }

        // The address of the string's object: its identity (two strings
        // made from the same characters are two objects); null when empty
        const void* object() const noexcept {
            return _word.get();
        }

        bool operator==(view_type s) const noexcept {
            return _view() == s;
        }

        bool operator==(const CharT* s) const noexcept {
            return _view() == s;
        }

        std::strong_ordering operator<=>(view_type s) const noexcept {
            return _view() <=> s;
        }

        std::strong_ordering operator<=>(const CharT* s) const noexcept {
            return _view() <=> view_type(s);
        }

    private:
        detail::StringHeader* _header() const noexcept {
            return const_cast<detail::StringHeader*>(static_cast<const detail::StringHeader*>(_word.get()));
        }

        static constexpr size_t HashMultiplier = 0x9E3779B97F4A7C15ull;

        // The standard library's hash of the characters, folded to the 32
        // bits the header keeps; never 0
        static uint32_t _fold(size_t h) noexcept {
            auto r = (uint32_t)(h ^ (h >> 32));
            return r ? r : 1;
        }

        const CharT* _chars() const noexcept {
            return reinterpret_cast<const CharT*>(static_cast<const unsigned char*>(_word.get()) + sizeof(detail::StringHeader));
        }

        // The characters as a std view: what the algorithms run on
        view_type _view() const noexcept {
            return view_type(data(), size());
        }

        // The characters [from, to): the same object for the whole string
        basic_string _part(size_type from, size_type to) const {
            return (from == 0 && to == size()) ? *this : basic_string(_view().substr(from, to - from));
        }

        // The characters in [lo, hi] shifted by `by`: the same object when
        // there is none
        basic_string _mapped(CharT lo, CharT hi, int by) const {
            auto v = _view();
            auto changes = [&](CharT c) { return c >= lo && c <= hi; };
            if (std::find_if(v.begin(), v.end(), changes) == v.end()) {
                return *this;
            }
            std::basic_string<CharT, Traits> s(v);
            for (auto& c : s) {
                if (changes(c)) {
                    c = CharT(int(c) + by);
                }
            }
            return basic_string(view_type(s));
        }

        // The white space of trim, fields: the six of isspace in the C locale
        static view_type _spaces() noexcept {
            static constexpr CharT chars[] = {CharT(' '), CharT('\t'), CharT('\n'), CharT('\v'), CharT('\f'), CharT('\r')};
            return view_type(chars, 6);
        }

        inline static constexpr CharT _empty = CharT();

        // A string over the word of its object (detail::StringAccess:
        // atomic.h, intern.h)
        explicit basic_string(tracked_ptr<const void> w) noexcept
        : _word(w) {
        }

        Word _word;

        template<class>
        friend class atomic;
        friend class basic_string_view<CharT, Traits>;
        friend struct detail::StringAccess;
    };

    // A view of a string that holds the string's object: two words, the
    // word of the string (a tracked_ptr: the object stays alive while the
    // view does, and the view lives where a tracked_ptr may) and the
    // range within it, an offset and a length of 32 bits each. What a
    // substring is in Go (a slice of the same array) and a
    // ReadOnlyMemory<char> in C#: a piece of a string with no copy and no
    // duty to keep the string around, which std::string_view leaves to
    // the caller. The read interface of std::string_view over the range,
    // substr as another view of the same object, the conversion to
    // std::string_view for everything that takes one, the comparisons and
    // the hash by the characters (the hash a string of them has: a map
    // keyed by strings is searched with a view). Made from a string
    // (`s.view()`, `s.view(pos, n)`), from another view (`substr`), by the
    // pieces of split; never from a literal or a std::string_view, which
    // hold nothing: make a string of those first. A string is made of a
    // view by `basic_string(v)`: the same object when the view is the
    // whole of it, a copy otherwise.
    template<class CharT, class Traits>
    class basic_string_view {
        using String = basic_string<CharT, Traits>;
        using Word = typename String::Word;

    public:
        using traits_type = Traits;
        using value_type = CharT;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = const CharT&;
        using const_reference = const CharT&;
        using pointer = const CharT*;
        using const_pointer = const CharT*;
        using iterator = const CharT*;
        using const_iterator = const CharT*;
        using reverse_iterator = std::reverse_iterator<const_iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using view_type = std::basic_string_view<CharT, Traits>;

        static constexpr size_type npos = view_type::npos;

        // Empty: holds nothing
        constexpr basic_string_view() noexcept = default;

        // The whole of a string
        basic_string_view(const String& s) noexcept
        : _word(s._word)
        , _length((uint32_t)s.size()) {
        }

        basic_string_view(const basic_string_view&) noexcept = default;
        basic_string_view(basic_string_view&&) noexcept = default;
        basic_string_view& operator=(const basic_string_view&) noexcept = default;
        basic_string_view& operator=(basic_string_view&&) noexcept = default;

        basic_string_view& operator=(const String& s) noexcept {
            return *this = basic_string_view(s);
        }

        // The characters; not terminated, as a view's are not
        const CharT* data() const noexcept {
            return _word ? _chars() : &String::_empty;
        }

        size_type size() const noexcept {
            return _length;
        }

        size_type length() const noexcept {
            return _length;
        }

        bool empty() const noexcept {
            return _length == 0;
        }

        static constexpr size_type max_size() noexcept {
            return UINT32_MAX;
        }

        operator view_type() const noexcept {
            return view_type(data(), _length);
        }

        view_type view() const noexcept {
            return view_type(data(), _length);
        }

        // A string of the characters: the string's own object when the
        // view is the whole of it, a copy otherwise
        String str() const {
            return String(*this);
        }

        const CharT& operator[](size_type i) const noexcept {
            return data()[i];
        }

        const CharT& at(size_type i) const {
            if (i >= _length) {
                throw std::out_of_range("sgcl::basic_string_view::at");
            }
            return data()[i];
        }

        const CharT& front() const noexcept {
            return data()[0];
        }

        const CharT& back() const noexcept {
            return data()[_length - 1];
        }

        const_iterator begin() const noexcept { return data(); }
        const_iterator end() const noexcept { return data() + _length; }
        const_iterator cbegin() const noexcept { return begin(); }
        const_iterator cend() const noexcept { return end(); }
        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
        const_reverse_iterator crbegin() const noexcept { return rbegin(); }
        const_reverse_iterator crend() const noexcept { return rend(); }

        // The read side of std::string_view, each over the range
        size_type copy(CharT* dest, size_type n, size_type pos = 0) const { return view().copy(dest, n, pos); }
        int compare(view_type s) const noexcept { return view().compare(s); }
        int compare(size_type pos, size_type n, view_type s) const { return view().compare(pos, n, s); }
        int compare(const CharT* s) const noexcept { return view().compare(s); }
        bool starts_with(view_type s) const noexcept { return view().starts_with(s); }
        bool starts_with(CharT c) const noexcept { return view().starts_with(c); }
        bool starts_with(const CharT* s) const noexcept { return view().starts_with(s); }
        bool ends_with(view_type s) const noexcept { return view().ends_with(s); }
        bool ends_with(CharT c) const noexcept { return view().ends_with(c); }
        bool ends_with(const CharT* s) const noexcept { return view().ends_with(s); }
        bool contains(view_type s) const noexcept { return view().find(s) != npos; }
        bool contains(CharT c) const noexcept { return view().find(c) != npos; }
        bool contains(const CharT* s) const noexcept { return view().find(s) != npos; }
        size_type find(view_type s, size_type pos = 0) const noexcept { return view().find(s, pos); }
        size_type find(CharT c, size_type pos = 0) const noexcept { return view().find(c, pos); }
        size_type find(const CharT* s, size_type pos = 0) const noexcept { return view().find(s, pos); }
        size_type rfind(view_type s, size_type pos = npos) const noexcept { return view().rfind(s, pos); }
        size_type rfind(CharT c, size_type pos = npos) const noexcept { return view().rfind(c, pos); }
        size_type rfind(const CharT* s, size_type pos = npos) const noexcept { return view().rfind(s, pos); }
        size_type find_first_of(view_type s, size_type pos = 0) const noexcept { return view().find_first_of(s, pos); }
        size_type find_first_of(CharT c, size_type pos = 0) const noexcept { return view().find_first_of(c, pos); }
        size_type find_last_of(view_type s, size_type pos = npos) const noexcept { return view().find_last_of(s, pos); }
        size_type find_last_of(CharT c, size_type pos = npos) const noexcept { return view().find_last_of(c, pos); }
        size_type find_first_not_of(view_type s, size_type pos = 0) const noexcept { return view().find_first_not_of(s, pos); }
        size_type find_first_not_of(CharT c, size_type pos = 0) const noexcept { return view().find_first_not_of(c, pos); }
        size_type find_last_not_of(view_type s, size_type pos = npos) const noexcept { return view().find_last_not_of(s, pos); }
        size_type find_last_not_of(CharT c, size_type pos = npos) const noexcept { return view().find_last_not_of(c, pos); }

        // The characters [pos, pos + n) as a view of the same object
        basic_string_view substr(size_type pos = 0, size_type n = npos) const {
            if (pos > _length) {
                throw std::out_of_range("sgcl::basic_string_view::substr");
            }
            return basic_string_view(_word, _offset + (uint32_t)pos, (uint32_t)std::min(n, _length - pos));
        }

        // The view narrowed, as std::string_view's: n characters off the
        // start, off the end (n at most the length)
        void remove_prefix(size_type n) noexcept {
            _offset += (uint32_t)n;
            _length -= (uint32_t)n;
        }

        void remove_suffix(size_type n) noexcept {
            _length -= (uint32_t)n;
        }

        // Without white space (the characters of `chars`) at both ends,
        // at the start, at the end: views, nothing copied
        basic_string_view trim() const noexcept {
            return trim(String::_spaces());
        }

        basic_string_view trim(view_type chars) const noexcept {
            auto v = view();
            auto from = v.find_first_not_of(chars);
            if (from == npos) {
                return basic_string_view();
            }
            return substr(from, v.find_last_not_of(chars) + 1 - from);
        }

        basic_string_view trim_left() const noexcept {
            return trim_left(String::_spaces());
        }

        basic_string_view trim_left(view_type chars) const noexcept {
            auto from = view().find_first_not_of(chars);
            return from == npos ? basic_string_view() : substr(from);
        }

        basic_string_view trim_right() const noexcept {
            return trim_right(String::_spaces());
        }

        basic_string_view trim_right(view_type chars) const noexcept {
            auto to = view().find_last_not_of(chars);
            return to == npos ? basic_string_view() : substr(0, to + 1);
        }

        basic_string_view trim_prefix(view_type prefix) const noexcept {
            return starts_with(prefix) ? substr(prefix.size()) : *this;
        }

        basic_string_view trim_suffix(view_type suffix) const noexcept {
            return ends_with(suffix) ? substr(0, _length - suffix.size()) : *this;
        }

        void swap(basic_string_view& o) noexcept {
            std::swap(_word, o._word);
            std::swap(_offset, o._offset);
            std::swap(_length, o._length);
        }

        // The hash of the characters: the one a string of them has
        size_t hash() const noexcept {
            return String::hash_of(view());
        }

        // The address of the string's object the view is of; null when the view is empty
        const void* object() const noexcept {
            return _word.get();
        }

        // The comparisons: with a view of this kind, a string, a
        // std::string_view, a literal; by the characters (two views of the
        // same range compare equal at once)
        friend bool operator==(const basic_string_view& a, const basic_string_view& b) noexcept {
            return (a._word == b._word && a._offset == b._offset && a._length == b._length) || a.view() == b.view();
        }

        friend std::strong_ordering operator<=>(const basic_string_view& a, const basic_string_view& b) noexcept {
            return a.view() <=> b.view();
        }

        friend bool operator==(const basic_string_view& a, const String& b) noexcept {
            return a.view() == view_type(b);
        }

        friend std::strong_ordering operator<=>(const basic_string_view& a, const String& b) noexcept {
            return a.view() <=> view_type(b);
        }

        bool operator==(view_type s) const noexcept {
            return view() == s;
        }

        bool operator==(const CharT* s) const noexcept {
            return view() == s;
        }

        std::strong_ordering operator<=>(view_type s) const noexcept {
            return view() <=> s;
        }

        std::strong_ordering operator<=>(const CharT* s) const noexcept {
            return view() <=> view_type(s);
        }

    private:
        Word _word;
        uint32_t _offset = 0;
        uint32_t _length = 0;

        basic_string_view(const Word& word, uint32_t offset, uint32_t length) noexcept
        : _word(word)
        , _offset(offset)
        , _length(length) {
        }

        const CharT* _chars() const noexcept {
            return reinterpret_cast<const CharT*>(static_cast<const unsigned char*>(_word.get()) + sizeof(detail::StringHeader)) + _offset;
        }

        // Whether the view is the whole of its string
        bool _whole() const noexcept {
            return _word && _offset == 0 && _length == static_cast<const detail::StringHeader*>(_word.get())->size;
        }

        friend class basic_string<CharT, Traits>;
    };

    // The pieces of a string between the occurrences of a separator, the
    // words between white space, or the characters: what split and fields
    // hand back. A forward range of basic_string_views into the string,
    // one piece per step, found as the walk goes (one find per step);
    // each view holds the string's object, so a piece kept anywhere a
    // tracked_ptr may live stays valid on its own. The object holds the
    // string and the separator (a string of its own, a copy of the view
    // given, so that a temporary separator in the head of a range-for
    // cannot dangle; a single character is kept inline). A container of
    // views or of strings is built from the range when the pieces are to
    // be kept.
    template<class CharT, class Traits>
    class basic_string<CharT, Traits>::pieces {
    public:
        using value_type = basic_string_view<CharT, Traits>;
        using size_type = size_t;

        enum Mode { Separator, Characters, Fields };

        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = basic_string_view<CharT, Traits>;
            using difference_type = ptrdiff_t;
            using pointer = const value_type*;
            using reference = const value_type&;

            iterator() noexcept = default;

            reference operator*() const noexcept {
                return _piece;
            }

            pointer operator->() const noexcept {
                return &_piece;
            }

            iterator& operator++() {
                _advance();
                return *this;
            }

            iterator operator++(int) {
                iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            friend bool operator==(const iterator& a, const iterator& b) noexcept {
                return a._done == b._done && (a._done || a._next == b._next);
            }

        private:
            const pieces* _owner = nullptr;
            size_type _next = npos;    // where the next piece starts; npos when the last one is out
            size_type _count = 0;      // pieces given out so far
            bool _done = true;         // past the last piece: the end
            value_type _piece;

            iterator(const pieces* owner, size_type start)
            : _owner(owner)
            , _next(start)
            , _done(false) {
                _advance();
            }

            // The piece from _next, and _next moved past it and its
            // separator (npos once the last piece is out); the end when
            // there is no piece to give
            void _advance() {
                if (_next == npos) {
                    _done = true;
                    return;
                }
                auto text = _owner->_text._view();
                switch (_owner->_mode) {
                case Fields: {
                    auto spaces = _spaces();
                    auto from = text.find_first_not_of(spaces, _next);
                    if (from == npos) {
                        _done = true;
                        return;
                    }
                    auto to = text.find_first_of(spaces, from);
                    _piece = _owner->_text.view(from, to == npos ? npos : to - from);
                    _next = to;
                    return;
                }
                case Characters:
                    if (_next >= text.size()) {
                        _done = true;
                        return;
                    }
                    if (_last()) {
                        _piece = _owner->_text.view(_next);
                        _next = npos;
                    } else {
                        _piece = _owner->_text.view(_next, 1);
                        ++_next;
                    }
                    ++_count;
                    return;
                case Separator: {
                    auto sep = _owner->_separator();
                    auto at = _last() ? npos : text.find(sep, _next);
                    if (at == npos) {
                        _piece = _owner->_text.view(_next);
                        _next = npos;
                    } else {
                        _piece = _owner->_text.view(_next, at - _next);
                        _next = at + sep.size();
                    }
                    ++_count;
                    return;
                }
                }
            }

            // Whether the piece to give out is the last one allowed
            bool _last() const noexcept {
                return _owner->_max_parts && _count + 1 == _owner->_max_parts;
            }

            friend class pieces;
        };

        using const_iterator = iterator;

        iterator begin() const {
            return _text.empty() ? end() : iterator(this, 0);
        }

        iterator end() const noexcept {
            return iterator();
        }

        bool empty() const {
            return begin() == end();
        }

        // The string the pieces are of
        const basic_string& text() const noexcept {
            return _text;
        }

    private:
        basic_string _text;
        basic_string _sep;
        CharT _sep_char = CharT();
        size_type _max_parts;
        Mode _mode;

        pieces(const basic_string& text, basic_string sep, size_type max_parts, Mode mode)
        : _text(text)
        , _sep(std::move(sep))
        , _max_parts(max_parts)
        , _mode(mode) {
        }

        pieces(const basic_string& text, CharT sep, size_type max_parts)
        : _text(text)
        , _sep_char(sep)
        , _max_parts(max_parts)
        , _mode(Separator) {
        }

        // The separator as a view: the string, or the one character
        view_type _separator() const noexcept {
            return _sep.empty() ? view_type(&_sep_char, 1) : _sep._view();
        }

        friend class basic_string;
    };

    using string = basic_string<char>;
    using wstring = basic_string<wchar_t>;
    using u8string = basic_string<char8_t>;
    using u16string = basic_string<char16_t>;
    using u32string = basic_string<char32_t>;
    using string_view = basic_string_view<char>;
    using wstring_view = basic_string_view<wchar_t>;
    using u8string_view = basic_string_view<char8_t>;
    using u16string_view = basic_string_view<char16_t>;
    using u32string_view = basic_string_view<char32_t>;

    template<class CharT, class Traits>
    void swap(basic_string<CharT, Traits>& l, basic_string<CharT, Traits>& r) noexcept {
        l.swap(r);
    }

    template<class CharT, class Traits>
    bool operator==(const basic_string<CharT, Traits>& a, const basic_string<CharT, Traits>& b) noexcept {
        return a.equals(b);
    }

    template<class CharT, class Traits>
    std::strong_ordering operator<=>(const basic_string<CharT, Traits>& a, const basic_string<CharT, Traits>& b) noexcept {
        return std::basic_string_view<CharT, Traits>(a) <=> std::basic_string_view<CharT, Traits>(b);
    }

    // Concatenation: a new string of the two, built once
    namespace detail {
        template<class S, class CharT, class Traits>
        S string_concat(std::basic_string_view<CharT, Traits> a, std::basic_string_view<CharT, Traits> b) {
            std::basic_string<CharT, Traits> s;
            s.reserve(a.size() + b.size());
            s.append(a).append(b);
            return S(std::basic_string_view<CharT, Traits>(s));
        }
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), std::basic_string_view<CharT, Traits>(b));
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, std::type_identity_t<std::basic_string_view<CharT, Traits>> b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), b);
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(std::type_identity_t<std::basic_string_view<CharT, Traits>> a, const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(a, std::basic_string_view<CharT, Traits>(b));
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, const CharT* b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), std::basic_string_view<CharT, Traits>(b));
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const CharT* a, const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), std::basic_string_view<CharT, Traits>(b));
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, CharT b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), std::basic_string_view<CharT, Traits>(&b, 1));
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(CharT a, const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(&a, 1), std::basic_string_view<CharT, Traits>(b));
    }

    template<class CharT, class Traits>
    std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const basic_string<CharT, Traits>& s) {
        return os << std::basic_string_view<CharT, Traits>(s);
    }

    template<class CharT, class Traits>
    std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const basic_string_view<CharT, Traits>& v) {
        return os << v.view();
    }

    template<class CharT, class Traits>
    void swap(basic_string_view<CharT, Traits>& l, basic_string_view<CharT, Traits>& r) noexcept {
        l.swap(r);
    }

    // A number as a string: to_string(42), to_string(2.5); "true"/"false"
    // for a bool; a char as a string of one
    template<class T>
    requires std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>) && (!std::is_same_v<T, char>)
    string to_string(T v) {
        return string(std::to_string(v));
    }

    inline string to_string(bool v) {
        return v ? "true" : "false";
    }

    inline string to_string(char c) {
        return string(1, c);
    }

    // A number from its text, the reverse of to_string: parse<int>("42"),
    // parse<double>("2.5"), parse<bool>("true"); nothing when the text is
    // not exactly one number (no white space, no sign for an unsigned
    // type, nothing after the digits) or it does not fit the type. What
    // C#'s TryParse, Go's strconv and Java's parseInt do, as an optional:
    // std::from_chars under it, so no locale and no allocation. A base
    // other than 10 for the integers: parse<int>("ff", 16).
    template<class T>
    requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    optional<T> parse(std::string_view text, int base = 10) noexcept {
        T value;
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value, base);
        if (ec != std::errc() || end != text.data() + text.size() || text.empty()) {
            return nullopt;
        }
        return value;
    }

    template<class T>
    requires std::is_floating_point_v<T>
    optional<T> parse(std::string_view text) noexcept {
        T value;
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc() || end != text.data() + text.size() || text.empty()) {
            return nullopt;
        }
        return value;
    }

    template<class T>
    requires std::is_same_v<T, bool>
    optional<T> parse(std::string_view text) noexcept {
        if (text == "true") {
            return true;
        }
        if (text == "false") {
            return false;
        }
        return nullopt;
    }
}

// A map keyed by strings is searched with a string_view or a literal as
// with a string, and no string is made for the search: the hash, the
// equality and the order of a string are transparent (the containers
// take a key of any type they accept, given that), and the hash of a
// view is the hash the string keeps.
namespace std {
    template<class CharT, class Traits>
    struct hash<sgcl::basic_string<CharT, Traits>> {
        using is_transparent = void;

        size_t operator()(const sgcl::basic_string<CharT, Traits>& s) const noexcept {
            return s.hash();
        }

        size_t operator()(std::basic_string_view<CharT, Traits> s) const noexcept {
            return sgcl::basic_string<CharT, Traits>::hash_of(s);
        }

        size_t operator()(const CharT* s) const noexcept {
            return sgcl::basic_string<CharT, Traits>::hash_of(s);
        }
    };

    template<class CharT, class Traits>
    struct hash<sgcl::basic_string_view<CharT, Traits>> {
        using is_transparent = void;

        size_t operator()(const sgcl::basic_string_view<CharT, Traits>& v) const noexcept {
            return v.hash();
        }

        size_t operator()(const sgcl::basic_string<CharT, Traits>& s) const noexcept {
            return s.hash();
        }

        size_t operator()(std::basic_string_view<CharT, Traits> s) const noexcept {
            return sgcl::basic_string<CharT, Traits>::hash_of(s);
        }

        size_t operator()(const CharT* s) const noexcept {
            return sgcl::basic_string<CharT, Traits>::hash_of(s);
        }
    };

    template<class CharT, class Traits>
    struct equal_to<sgcl::basic_string_view<CharT, Traits>> {
        using is_transparent = void;

        template<class A, class B>
        bool operator()(const A& a, const B& b) const noexcept {
            return a == b;
        }
    };

    template<class CharT, class Traits>
    struct less<sgcl::basic_string_view<CharT, Traits>> {
        using is_transparent = void;

        template<class A, class B>
        bool operator()(const A& a, const B& b) const noexcept {
            return a < b;
        }
    };

    template<class CharT, class Traits>
    struct equal_to<sgcl::basic_string<CharT, Traits>> {
        using is_transparent = void;

        template<class A, class B>
        bool operator()(const A& a, const B& b) const noexcept {
            return a == b;
        }
    };

    template<class CharT, class Traits>
    struct less<sgcl::basic_string<CharT, Traits>> {
        using is_transparent = void;

        template<class A, class B>
        bool operator()(const A& a, const B& b) const noexcept {
            return a < b;
        }
    };
}
