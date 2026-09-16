//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/string_data.h"
#include "tracked_ptr.h"

#include <compare>
#include <functional>
#include <iosfwd>
#include <iterator>
#include <string>
#include <string_view>

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
    // std::string, a range, (n, ch); no mutation, no capacity. The word
    // is a tracked_ptr, so a string lives where one may: on a stack or in
    // a managed object.
    // The length is kept in 32 bits: a string holds up to 4 G characters.
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
            return view();
        }

        view_type view() const noexcept {
            return view_type(data(), size());
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
        size_type copy(CharT* dest, size_type n, size_type pos = 0) const { return view().copy(dest, n, pos); }
        int compare(view_type s) const noexcept { return view().compare(s); }
        int compare(size_type pos, size_type n, view_type s) const { return view().compare(pos, n, s); }
        int compare(size_type pos, size_type n, view_type s, size_type pos2, size_type n2) const { return view().compare(pos, n, s, pos2, n2); }
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
        size_type find(const CharT* s, size_type pos, size_type n) const noexcept { return view().find(s, pos, n); }
        size_type find(const CharT* s, size_type pos = 0) const noexcept { return view().find(s, pos); }
        size_type rfind(view_type s, size_type pos = npos) const noexcept { return view().rfind(s, pos); }
        size_type rfind(CharT c, size_type pos = npos) const noexcept { return view().rfind(c, pos); }
        size_type rfind(const CharT* s, size_type pos, size_type n) const noexcept { return view().rfind(s, pos, n); }
        size_type rfind(const CharT* s, size_type pos = npos) const noexcept { return view().rfind(s, pos); }
        size_type find_first_of(view_type s, size_type pos = 0) const noexcept { return view().find_first_of(s, pos); }
        size_type find_first_of(CharT c, size_type pos = 0) const noexcept { return view().find_first_of(c, pos); }
        size_type find_first_of(const CharT* s, size_type pos = 0) const noexcept { return view().find_first_of(s, pos); }
        size_type find_last_of(view_type s, size_type pos = npos) const noexcept { return view().find_last_of(s, pos); }
        size_type find_last_of(CharT c, size_type pos = npos) const noexcept { return view().find_last_of(c, pos); }
        size_type find_last_of(const CharT* s, size_type pos = npos) const noexcept { return view().find_last_of(s, pos); }
        size_type find_first_not_of(view_type s, size_type pos = 0) const noexcept { return view().find_first_not_of(s, pos); }
        size_type find_first_not_of(CharT c, size_type pos = 0) const noexcept { return view().find_first_not_of(c, pos); }
        size_type find_first_not_of(const CharT* s, size_type pos = 0) const noexcept { return view().find_first_not_of(s, pos); }
        size_type find_last_not_of(view_type s, size_type pos = npos) const noexcept { return view().find_last_not_of(s, pos); }
        size_type find_last_not_of(CharT c, size_type pos = npos) const noexcept { return view().find_last_not_of(c, pos); }
        size_type find_last_not_of(const CharT* s, size_type pos = npos) const noexcept { return view().find_last_not_of(s, pos); }

        // A new string of the characters [pos, pos + n): a copy, the
        // whole string when the range is the whole string
        basic_string substr(size_type pos = 0, size_type n = npos) const {
            if (pos > size()) {
                throw std::out_of_range("sgcl::basic_string::substr");
            }
            if (pos == 0 && n >= size()) {
                return *this;
            }
            return basic_string(view().substr(pos, n));
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
            return view() == o.view();
        }

        // The hash of the characters, computed the first time and kept in
        // the string's object (0 is "not yet"); the empty string's is a constant
        size_t hash() const noexcept {
            if (!_word) {
                return 0x9E3779B97F4A7C15ull;
            }
            auto header = _header();
            auto h = header->hash.load(std::memory_order_relaxed);
            if (!h) {
                h = _compute_hash();
                header->hash.store(h, std::memory_order_relaxed);
            }
            return (size_t)h * 0x9E3779B97F4A7C15ull;
        }

        // The address of the string's object: its identity (two strings
        // made from the same characters are two objects); null when empty
        const void* object() const noexcept {
            return _word.get();
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
        detail::StringHeader* _header() const noexcept {
            return const_cast<detail::StringHeader*>(static_cast<const detail::StringHeader*>(_word.get()));
        }

        // The standard library's hash of the characters, folded to the 32
        // bits the header keeps; never 0
        uint32_t _compute_hash() const noexcept {
            auto h = std::hash<view_type>()(view());
            auto r = (uint32_t)(h ^ (h >> 32));
            return r ? r : 1;
        }

        const CharT* _chars() const noexcept {
            return reinterpret_cast<const CharT*>(static_cast<const unsigned char*>(_word.get()) + sizeof(detail::StringHeader));
        }

        inline static constexpr CharT _empty = CharT();

        // A string over a word loaded from an atomic (atomic.h)
        explicit basic_string(tracked_ptr<const void> w) noexcept
        : _word(w) {
        }

        Word _word;

        template<class>
        friend class atomic;
    };

    using string = basic_string<char>;
    using wstring = basic_string<wchar_t>;
    using u8string = basic_string<char8_t>;
    using u16string = basic_string<char16_t>;
    using u32string = basic_string<char32_t>;

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
        return a.view() <=> b.view();
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
        return detail::string_concat<basic_string<CharT, Traits>>(a.view(), b.view());
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, std::type_identity_t<std::basic_string_view<CharT, Traits>> b) {
        return detail::string_concat<basic_string<CharT, Traits>>(a.view(), b);
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(std::type_identity_t<std::basic_string_view<CharT, Traits>> a, const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(a, b.view());
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, const CharT* b) {
        return detail::string_concat<basic_string<CharT, Traits>>(a.view(), std::basic_string_view<CharT, Traits>(b));
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const CharT* a, const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), b.view());
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, CharT b) {
        return detail::string_concat<basic_string<CharT, Traits>>(a.view(), std::basic_string_view<CharT, Traits>(&b, 1));
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(CharT a, const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(&a, 1), b.view());
    }

    template<class CharT, class Traits>
    std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const basic_string<CharT, Traits>& s) {
        return os << s.view();
    }
}

namespace std {
    template<class CharT, class Traits>
    struct hash<sgcl::basic_string<CharT, Traits>> {
        size_t operator()(const sgcl::basic_string<CharT, Traits>& s) const noexcept {
            return s.hash();
        }
    };
}
