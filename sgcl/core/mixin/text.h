//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../unicode.h"
#include "../utf8.h"

#include <compare>
#include <cstddef>
#include <string>
#include <string_view>

namespace sgcl {
    class runes;
}

namespace sgcl::mixin {
    // text<Derived, CharT, Traits>: the read side of std::string_view
    // as members of whatever holds characters through data() and size():
    // basic_string and slice<const CharT>. A mixin (m_): a static
    // interface, no virtual method, no state; its constructor and
    // destructor are protected. Every operation runs on a
    // std::basic_string_view over the characters; the operations that
    // make a new object (substr, trim, split) stay with the class, whose
    // type they return.
    //
    // The text is Unicode: UTF-8 in a char (or char8_t) string, whose
    // size() counts bytes and whose runes() walks the code points; one
    // code point per unit in the wide ones. A char32_t is a character
    // wherever a CharT is (find(U'ż'), contains(U'😀')), encoded and
    // searched as bytes; a std::u32string_view is a set of characters
    // where a view is (find_first_of(U"«»"), trim(U"\u3000")). An int is
    // refused: 'ż' in a UTF-8 source is a multi-character literal of type
    // int, silently cut to one byte, so find(int) and the rest are
    // deleted and the call says to write U'ż'. The white space of trim()
    // and fields() and the case of to_lower() are Unicode's ([unicode]).
    template<class Derived, class CharT, class Traits = std::char_traits<CharT>>
    class text {
    public:
        using view_type = std::basic_string_view<CharT, Traits>;
        using size_type = size_t;
        static constexpr size_type npos = view_type::npos;

        // The characters as a std view: what the algorithms run on, and
        // what a std interface takes
        view_type view() const noexcept {
            return view_type(_self().data(), _self().size());
        }

        operator view_type() const noexcept {
            return view();
        }

        size_type length() const noexcept {
            return _self().size();
        }

        const CharT& at(size_type i) const {
            if (i >= _self().size()) {
                throw std::out_of_range("sgcl::at");
            }
            return _self().data()[i];
        }

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

        // The Unicode characters: the code points of a UTF-8 string
        // decoded as they are walked (sgcl::runes, a range over a slice
        // that holds the text), their count, the one at a byte position
        // with its width, and whether every sequence is valid. A wide
        // string's units are its code points already: size() counts them.
        sgcl::runes runes() const noexcept requires (sizeof(CharT) == 1);

        size_type rune_count() const noexcept {
            if constexpr (sizeof(CharT) == 1) {
                return utf8::count(_bytes());
            } else {
                return _self().size();
            }
        }

        pair<char32_t, size_type> decode(size_type pos) const noexcept requires (sizeof(CharT) == 1) {
            return utf8::decode(_bytes(), pos);
        }

        bool is_valid_utf8() const noexcept requires (sizeof(CharT) == 1) {
            return utf8::valid(_bytes());
        }

        // A character as a code point: encoded into bytes and searched as
        // text in a UTF-8 string, one unit in a wide one
        bool starts_with(char32_t c) const noexcept requires (sizeof(CharT) == 1) { return view().starts_with(_encoded(c)); }
        bool ends_with(char32_t c) const noexcept requires (sizeof(CharT) == 1) { return view().ends_with(_encoded(c)); }
        bool contains(char32_t c) const noexcept requires (sizeof(CharT) == 1) { return view().find(_encoded(c)) != npos; }
        size_type find(char32_t c, size_type pos = 0) const noexcept requires (sizeof(CharT) == 1) { return view().find(_encoded(c), pos); }
        size_type rfind(char32_t c, size_type pos = npos) const noexcept requires (sizeof(CharT) == 1) { return view().rfind(_encoded(c), pos); }

        // A set of characters as code points: the byte position of the
        // first (last) character of the text that is (is not) in the set,
        // walked by code points
        size_type find_first_of(std::u32string_view set, size_type pos = 0) const noexcept requires (sizeof(CharT) == 1) {
            return _find_forward(pos, [&](char32_t c) { return set.find(c) != std::u32string_view::npos; });
        }

        size_type find_first_not_of(std::u32string_view set, size_type pos = 0) const noexcept requires (sizeof(CharT) == 1) {
            return _find_forward(pos, [&](char32_t c) { return set.find(c) == std::u32string_view::npos; });
        }

        size_type find_last_of(std::u32string_view set, size_type pos = npos) const noexcept requires (sizeof(CharT) == 1) {
            return _find_backward(pos, [&](char32_t c) { return set.find(c) != std::u32string_view::npos; });
        }

        size_type find_last_not_of(std::u32string_view set, size_type pos = npos) const noexcept requires (sizeof(CharT) == 1) {
            return _find_backward(pos, [&](char32_t c) { return set.find(c) == std::u32string_view::npos; });
        }

        // Whether the two texts are the same letters in either case: by
        // the simple case folding of each code point, Go's EqualFold
        bool equal_fold(view_type s) const noexcept {
            if constexpr (sizeof(CharT) == 1) {
                auto a = _bytes();
                auto b = std::string_view(reinterpret_cast<const char*>(s.data()), s.size());
                size_type i = 0, j = 0;
                while (i < a.size() && j < b.size()) {
                    // While both sides are ASCII the fold is one mask a
                    // byte: no decoding, no table, and the letters are
                    // the only bytes it touches
                    while (i < a.size() && j < b.size()
                           && uint8_t(a[i]) < 0x80 && uint8_t(b[j]) < 0x80) {
                        uint8_t x = uint8_t(a[i]), y = uint8_t(b[j]);
                        x = uint8_t(x + (uint8_t(x - 'A') < 26 ? 32 : 0));
                        y = uint8_t(y + (uint8_t(y - 'A') < 26 ? 32 : 0));
                        if (x != y) {
                            return false;
                        }
                        ++i;
                        ++j;
                    }
                    if (i >= a.size() || j >= b.size()) {
                        break;
                    }
                    auto [ca, na] = utf8::decode(a, i);
                    auto [cb, nb] = utf8::decode(b, j);
                    if (!unicode::equal_fold(ca, cb)) {
                        return false;
                    }
                    i += na;
                    j += nb;
                }
                return i == a.size() && j == b.size();
            } else {
                auto a = view();
                if (a.size() != s.size()) {
                    return false;
                }
                for (size_type i = 0; i < a.size(); ++i) {
                    if (!unicode::equal_fold(char32_t(a[i]), char32_t(s[i]))) {
                        return false;
                    }
                }
                return true;
            }
        }

        bool equal_fold(const CharT* s) const noexcept {
            return equal_fold(view_type(s));
        }

        // An int is no character: 'ż' in a UTF-8 source is a multi-character
        // literal of type int, which would be cut to its last byte. Write
        // U'ż' (a char32_t), or the character as text, "ż".
        bool starts_with(int) const = delete;
        bool ends_with(int) const = delete;
        bool contains(int) const = delete;
        size_type find(int, size_type = 0) const = delete;
        size_type rfind(int, size_type = npos) const = delete;
        size_type find_first_of(int, size_type = 0) const = delete;
        size_type find_last_of(int, size_type = npos) const = delete;
        size_type find_first_not_of(int, size_type = 0) const = delete;
        size_type find_last_not_of(int, size_type = npos) const = delete;

        // A std::string with the same characters: for the interfaces that
        // want one, and for building a new string
        std::basic_string<CharT, Traits> str() const {
            return std::basic_string<CharT, Traits>(_self().data(), _self().size());
        }

        // Comparisons with a std view or a literal, by the characters
        // (friends on Derived: an exact match on the object, so that a
        // literal does not also convert to Derived and tie)
        friend bool operator==(const Derived& a, view_type s) noexcept { return a.view() == s; }
        friend bool operator==(const Derived& a, const CharT* s) noexcept { return a.view() == view_type(s); }
        friend std::strong_ordering operator<=>(const Derived& a, view_type s) noexcept { return a.view() <=> s; }
        friend std::strong_ordering operator<=>(const Derived& a, const CharT* s) noexcept { return a.view() <=> view_type(s); }

    protected:
        text() = default;
        ~text() = default;

        // The characters as bytes, for the UTF-8 primitives (a char8_t
        // string is bytes too)
        std::string_view _bytes() const noexcept requires (sizeof(CharT) == 1) {
            return std::string_view(reinterpret_cast<const char*>(_self().data()), _self().size());
        }

        // The white space of trim() and fields(): the first position at or
        // after pos that is (is not) a Unicode white-space character, or
        // npos; a code point at a time in a UTF-8 string, a unit in a wide one
        size_type _find_space(size_type pos, bool space) const noexcept {
            if constexpr (sizeof(CharT) == 1) {
                return _find_forward(pos, [&](char32_t c) { return unicode::is_space(c) == space; });
            } else {
                auto v = view();
                for (size_type i = pos; i < v.size(); ++i) {
                    if (unicode::is_space(char32_t(v[i])) == space) {
                        return i;
                    }
                }
                return npos;
            }
        }

        // The end of the text without its trailing white space: one past
        // the last character that is not white space, 0 for none
        size_type _end_without_spaces() const noexcept {
            if constexpr (sizeof(CharT) == 1) {
                auto at = _find_backward(npos, [](char32_t c) { return !unicode::is_space(c); });
                return at == npos ? 0 : at + utf8::decode(_bytes(), at).second;
            } else {
                auto v = view();
                size_type i = v.size();
                while (i > 0 && unicode::is_space(char32_t(v[i - 1]))) {
                    --i;
                }
                return i;
            }
        }

        // The first byte position at or after pos whose code point satisfies
        // pred, walking forward; the last one at or before pos, backward
        template<class Pred>
        size_type _find_forward(size_type pos, Pred pred) const noexcept requires (sizeof(CharT) == 1) {
            auto b = _bytes();
            for (size_type i = pos; i < b.size();) {
                auto [c, n] = utf8::decode(b, i);
                if (pred(c)) {
                    return i;
                }
                i += n;
            }
            return npos;
        }

        template<class Pred>
        size_type _find_backward(size_type pos, Pred pred) const noexcept requires (sizeof(CharT) == 1) {
            auto b = _bytes();
            if (b.empty()) {
                return npos;
            }
            size_type end = pos == npos || pos >= b.size() ? b.size() : pos + 1;
            while (end > 0) {
                auto [c, n] = utf8::decode_last(b, end);
                end -= n;
                if (pred(c)) {
                    return end;
                }
            }
            return npos;
        }

        // The text a code point is searched as: the bytes of its encoding,
        // a view of them for the length of the expression that made it
        struct _encoded {
            utf8::encoded e;
            constexpr _encoded(char32_t c) noexcept : e(c) {}
            operator view_type() const noexcept { return view_type(reinterpret_cast<const CharT*>(e.bytes), e.size); }
        };

    private:
        const Derived& _self() const noexcept {
            return static_cast<const Derived&>(*this);
        }
    };
}
