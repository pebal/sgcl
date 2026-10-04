//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../aliases.h"
#include "../detail/os.h"
#include "../unicode.h"
#include "../utf8.h"

#include <compare>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>

namespace sgcl {
    class runes;
}

namespace sgcl::detail {
    // A code point as the units of a wide text: one unit of a 32-bit
    // text, one or a surrogate pair of a UTF-16 one; a value that is not
    // a scalar value (a surrogate, past U+10FFFF) as the replacement
    // character, as utf8::encoded writes it
    template<class CharT>
    struct WideEncoded {
        CharT units[sizeof(CharT) == 2 ? 2 : 1] = {};
        size_t size = 1;

        constexpr WideEncoded(char32_t c) noexcept {
            if (!utf8::valid(c)) {
                c = utf8::replacement;
            }
            if constexpr (sizeof(CharT) == 2) {
                if (c >= 0x10000) {
                    units[0] = CharT(0xD800 + ((c - 0x10000) >> 10));
                    units[1] = CharT(0xDC00 + ((c - 0x10000) & 0x3FF));
                    size = 2;
                    return;
                }
            }
            units[0] = CharT(c);
        }
    };

    // An array of characters as text: up to its first NUL or its end,
    // whichever comes first — a literal without its terminator, and an
    // array filled to the brim, which has no NUL, not read past its end
    // (a pointer's strlen would go on into whatever follows it)
    template<class CharT, class Traits, size_t N>
    SGCL_INLINE_HOT constexpr std::basic_string_view<CharT, Traits> array_text(const CharT (&text)[N]) noexcept {
        const CharT* nul = Traits::find(text, N, CharT());
        return std::basic_string_view<CharT, Traits>(text, nul ? size_t(nul - text) : N);
    }
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
    // size() counts bytes and whose runes() walks the code points; UTF-16
    // in a char16_t one (a wchar_t one where wchar_t has 16 bits), whose
    // size() counts units and where a code point past U+FFFF is a
    // surrogate pair; one code point per unit in a char32_t one (and a
    // wchar_t one of 32 bits). A char32_t is a character wherever a CharT
    // is (find(U'ż'), contains(U'😀')), encoded into the text's units and
    // searched as text; a std::u32string_view is a set of characters
    // where a view is (find_first_of(U"«»"), trim(U"\u3000")), the text
    // walked by code points. An int is
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
        SGCL_INLINE_HOT view_type view() const noexcept {
            return view_type(_self().data(), _self().size());
        }

        SGCL_INLINE_HOT operator view_type() const noexcept {
            return view();
        }

        SGCL_INLINE_HOT size_type length() const noexcept {
            return _self().size();
        }

        SGCL_INLINE_HOT const CharT& at(size_type i) const {
            if (i >= _self().size()) {
                throw out_of_range("sgcl::text::at");   // a string's or a text slice's
            }
            return _self().data()[i];
        }

        // A text given as an array of characters (a literal) is read up to
        // its first NUL or its end, never past it; as a pointer (CharT* or
        // const CharT*, no other) up to its NUL: two overloads, so that an
        // array does not decay into the pointer's strlen
        SGCL_INLINE_HOT size_type copy(CharT* dest, size_type n, size_type pos = 0) const { return view().copy(dest, n, pos); }
        SGCL_INLINE_HOT int compare(view_type s) const noexcept { return view().compare(s); }
        SGCL_INLINE_HOT int compare(size_type pos, size_type n, view_type s) const { return view().compare(pos, n, s); }
        SGCL_INLINE_HOT int compare(size_type pos, size_type n, view_type s, size_type pos2, size_type n2) const { return view().compare(pos, n, s, pos2, n2); }
        template<size_t N> SGCL_INLINE_HOT int compare(size_type pos, size_type n, const CharT (&s)[N]) const { return view().compare(pos, n, _array(s)); }
        template<size_t N> SGCL_INLINE_HOT int compare(size_type pos, size_type n, const CharT (&s)[N], size_type pos2, size_type n2) const { return view().compare(pos, n, _array(s), pos2, n2); }
        template<size_t N> SGCL_INLINE_HOT int compare(const CharT (&s)[N]) const noexcept { return compare(_array(s)); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT int compare(P s) const noexcept { return view().compare(s); }
        SGCL_INLINE_HOT bool starts_with(view_type s) const noexcept { return view().starts_with(s); }
        SGCL_INLINE_HOT bool starts_with(CharT c) const noexcept { return view().starts_with(c); }
        template<size_t N> SGCL_INLINE_HOT bool starts_with(const CharT (&s)[N]) const noexcept { return starts_with(_array(s)); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT bool starts_with(P s) const noexcept { return view().starts_with(s); }
        SGCL_INLINE_HOT bool ends_with(view_type s) const noexcept { return view().ends_with(s); }
        SGCL_INLINE_HOT bool ends_with(CharT c) const noexcept { return view().ends_with(c); }
        template<size_t N> SGCL_INLINE_HOT bool ends_with(const CharT (&s)[N]) const noexcept { return ends_with(_array(s)); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT bool ends_with(P s) const noexcept { return view().ends_with(s); }
        SGCL_INLINE_HOT bool contains(view_type s) const noexcept { return view().find(s) != npos; }
        SGCL_INLINE_HOT bool contains(CharT c) const noexcept { return view().find(c) != npos; }
        template<size_t N> SGCL_INLINE_HOT bool contains(const CharT (&s)[N]) const noexcept { return contains(_array(s)); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT bool contains(P s) const noexcept { return view().find(s) != npos; }
        SGCL_INLINE_HOT size_type find(view_type s, size_type pos = 0) const noexcept { return view().find(s, pos); }
        SGCL_INLINE_HOT size_type find(CharT c, size_type pos = 0) const noexcept { return view().find(c, pos); }
        SGCL_INLINE_HOT size_type find(const CharT* s, size_type pos, size_type n) const noexcept { return view().find(s, pos, n); }
        template<size_t N> SGCL_INLINE_HOT size_type find(const CharT (&s)[N], size_type pos = 0) const noexcept { return find(_array(s), pos); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT size_type find(P s, size_type pos = 0) const noexcept { return view().find(s, pos); }
        SGCL_INLINE_HOT size_type rfind(view_type s, size_type pos = npos) const noexcept { return view().rfind(s, pos); }
        SGCL_INLINE_HOT size_type rfind(CharT c, size_type pos = npos) const noexcept { return view().rfind(c, pos); }
        SGCL_INLINE_HOT size_type rfind(const CharT* s, size_type pos, size_type n) const noexcept { return view().rfind(s, pos, n); }
        template<size_t N> SGCL_INLINE_HOT size_type rfind(const CharT (&s)[N], size_type pos = npos) const noexcept { return rfind(_array(s), pos); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT size_type rfind(P s, size_type pos = npos) const noexcept { return view().rfind(s, pos); }
        SGCL_INLINE_HOT size_type find_first_of(view_type s, size_type pos = 0) const noexcept { return view().find_first_of(s, pos); }
        SGCL_INLINE_HOT size_type find_first_of(CharT c, size_type pos = 0) const noexcept { return view().find_first_of(c, pos); }
        template<size_t N> SGCL_INLINE_HOT size_type find_first_of(const CharT (&s)[N], size_type pos = 0) const noexcept { return find_first_of(_array(s), pos); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT size_type find_first_of(P s, size_type pos = 0) const noexcept { return view().find_first_of(s, pos); }
        SGCL_INLINE_HOT size_type find_last_of(view_type s, size_type pos = npos) const noexcept { return view().find_last_of(s, pos); }
        SGCL_INLINE_HOT size_type find_last_of(CharT c, size_type pos = npos) const noexcept { return view().find_last_of(c, pos); }
        template<size_t N> SGCL_INLINE_HOT size_type find_last_of(const CharT (&s)[N], size_type pos = npos) const noexcept { return find_last_of(_array(s), pos); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT size_type find_last_of(P s, size_type pos = npos) const noexcept { return view().find_last_of(s, pos); }
        SGCL_INLINE_HOT size_type find_first_not_of(view_type s, size_type pos = 0) const noexcept { return view().find_first_not_of(s, pos); }
        SGCL_INLINE_HOT size_type find_first_not_of(CharT c, size_type pos = 0) const noexcept { return view().find_first_not_of(c, pos); }
        template<size_t N> SGCL_INLINE_HOT size_type find_first_not_of(const CharT (&s)[N], size_type pos = 0) const noexcept { return find_first_not_of(_array(s), pos); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT size_type find_first_not_of(P s, size_type pos = 0) const noexcept { return view().find_first_not_of(s, pos); }
        SGCL_INLINE_HOT size_type find_last_not_of(view_type s, size_type pos = npos) const noexcept { return view().find_last_not_of(s, pos); }
        SGCL_INLINE_HOT size_type find_last_not_of(CharT c, size_type pos = npos) const noexcept { return view().find_last_not_of(c, pos); }
        template<size_t N> SGCL_INLINE_HOT size_type find_last_not_of(const CharT (&s)[N], size_type pos = npos) const noexcept { return find_last_not_of(_array(s), pos); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT size_type find_last_not_of(P s, size_type pos = npos) const noexcept { return view().find_last_not_of(s, pos); }

        // The Unicode characters: the code points of a UTF-8 string
        // decoded as they are walked (sgcl::runes, a range over a slice
        // that holds the text), their count, the one at a byte position
        // with its width, and whether every sequence is valid. The count
        // of a UTF-16 text is its units less one for every surrogate
        // pair; a 32-bit text's units are its code points already.
        sgcl::runes runes() const noexcept requires (sizeof(CharT) == 1);

        size_type rune_count() const noexcept {
            if constexpr (sizeof(CharT) == 1) {
                return utf8::count(_bytes());
            } else if constexpr (sizeof(CharT) == 2) {
                auto v = view();
                size_type pairs = 0;
                for (size_type i = 0; i + 1 < v.size(); ++i) {
                    bool pair = _high(v[i]) && _low(v[i + 1]);
                    pairs += pair;
                    i += pair;
                }
                return v.size() - pairs;
            } else {
                return _self().size();
            }
        }

        SGCL_INLINE_HOT pair<char32_t, size_type> decode(size_type pos) const noexcept requires (sizeof(CharT) == 1) {
            return utf8::decode(_bytes(), pos);
        }

        SGCL_INLINE_HOT bool is_valid_utf8() const noexcept requires (sizeof(CharT) == 1) {
            return utf8::valid(_bytes());
        }

        // A character as a code point: encoded into the text's units (the
        // bytes of UTF-8, one unit or a surrogate pair of UTF-16) and
        // searched as text. In a char32_t text the CharT overloads above
        // are these already. A value that is no code point (a surrogate,
        // past U+10FFFF) has no encoding and is in no such text: not the
        // U+FFFD it would be written as.
        SGCL_INLINE_HOT bool starts_with(char32_t c) const noexcept requires (!std::same_as<CharT, char32_t>) { return utf8::valid(c) && view().starts_with(_encoded(c)); }
        SGCL_INLINE_HOT bool ends_with(char32_t c) const noexcept requires (!std::same_as<CharT, char32_t>) { return utf8::valid(c) && view().ends_with(_encoded(c)); }
        SGCL_INLINE_HOT bool contains(char32_t c) const noexcept requires (!std::same_as<CharT, char32_t>) { return utf8::valid(c) && view().find(_encoded(c)) != npos; }
        SGCL_INLINE_HOT size_type find(char32_t c, size_type pos = 0) const noexcept requires (!std::same_as<CharT, char32_t>) { return utf8::valid(c) ? view().find(_encoded(c), pos) : npos; }
        SGCL_INLINE_HOT size_type rfind(char32_t c, size_type pos = npos) const noexcept requires (!std::same_as<CharT, char32_t>) { return utf8::valid(c) ? view().rfind(_encoded(c), pos) : npos; }

        // A set of characters as code points: the position of the first
        // (last) character of the text that is (is not) in the set, walked
        // by code points; a backward search from `pos` starts with the
        // code point that begins at or before it, a forward one with the
        // first that begins at or after it. A char32_t is a set of one.
        SGCL_INLINE_HOT size_type find_first_of(std::u32string_view set, size_type pos = 0) const noexcept requires (!std::same_as<CharT, char32_t>) {
            return _find_forward(_forward_start(pos), [&](char32_t c) { return set.find(c) != std::u32string_view::npos; });
        }

        SGCL_INLINE_HOT size_type find_first_not_of(std::u32string_view set, size_type pos = 0) const noexcept requires (!std::same_as<CharT, char32_t>) {
            return _find_forward(_forward_start(pos), [&](char32_t c) { return set.find(c) == std::u32string_view::npos; });
        }

        SGCL_INLINE_HOT size_type find_last_of(std::u32string_view set, size_type pos = npos) const noexcept requires (!std::same_as<CharT, char32_t>) {
            return _find_backward(pos, [&](char32_t c) { return set.find(c) != std::u32string_view::npos; });
        }

        SGCL_INLINE_HOT size_type find_last_not_of(std::u32string_view set, size_type pos = npos) const noexcept requires (!std::same_as<CharT, char32_t>) {
            return _find_backward(pos, [&](char32_t c) { return set.find(c) == std::u32string_view::npos; });
        }

        SGCL_INLINE_HOT size_type find_first_of(char32_t c, size_type pos = 0) const noexcept requires (!std::same_as<CharT, char32_t>) { return find_first_of(std::u32string_view(&c, 1), pos); }
        SGCL_INLINE_HOT size_type find_first_not_of(char32_t c, size_type pos = 0) const noexcept requires (!std::same_as<CharT, char32_t>) { return find_first_not_of(std::u32string_view(&c, 1), pos); }
        SGCL_INLINE_HOT size_type find_last_of(char32_t c, size_type pos = npos) const noexcept requires (!std::same_as<CharT, char32_t>) { return find_last_of(std::u32string_view(&c, 1), pos); }
        SGCL_INLINE_HOT size_type find_last_not_of(char32_t c, size_type pos = npos) const noexcept requires (!std::same_as<CharT, char32_t>) { return find_last_not_of(std::u32string_view(&c, 1), pos); }

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
                    // An ill-formed byte equals only itself: the decoder
                    // marks it as a value past every code point, which
                    // folds to nothing, so no test of U+FFFD is in the
                    // loop and valid text runs the loop it always did
                    auto [ca, na] = utf8::_decode<true>(a, i);
                    auto [cb, nb] = utf8::_decode<true>(b, j);
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
                    // An equal unit needs no fold (the fold is a call)
                    if (a[i] != s[i] && !unicode::equal_fold(char32_t(a[i]), char32_t(s[i]))) {
                        if constexpr (sizeof(CharT) == 2) {
                            // Units that differ may be halves of surrogate
                            // pairs: asked out of the loop, which stays the
                            // loop of units it was before pairs were mapped
                            i = _folded_pair_end(a, s, i);
                            if (i != npos) {
                                continue;
                            }
                        }
                        return false;
                    }
                }
                return true;
            }
        }

        template<size_t N>
        SGCL_INLINE_HOT bool equal_fold(const CharT (&s)[N]) const noexcept {
            return equal_fold(_array(s));
        }

        template<class P>
        requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT bool equal_fold(P s) const noexcept {
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
        SGCL_INLINE_HOT std::basic_string<CharT, Traits> str() const noexcept {
            return std::basic_string<CharT, Traits>(_self().data(), _self().size());
        }

        // Comparisons with a std view or a literal, by the characters
        // (friends on Derived: an exact match on the object, so that a
        // literal does not also convert to Derived and tie)
        SGCL_INLINE_HOT friend bool operator==(const Derived& a, view_type s) noexcept { return a.view() == s; }
        template<size_t N>
        SGCL_INLINE_HOT friend bool operator==(const Derived& a, const CharT (&s)[N]) noexcept { return a.view() == _array(s); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT friend bool operator==(const Derived& a, P s) noexcept { return a.view() == view_type(s); }
        SGCL_INLINE_HOT friend std::strong_ordering operator<=>(const Derived& a, view_type s) noexcept { return a.view() <=> s; }
        template<size_t N>
        SGCL_INLINE_HOT friend std::strong_ordering operator<=>(const Derived& a, const CharT (&s)[N]) noexcept { return a.view() <=> _array(s); }
        template<class P> requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        SGCL_INLINE_HOT friend std::strong_ordering operator<=>(const Derived& a, P s) noexcept { return a.view() <=> view_type(s); }

    protected:
        text() = default;
        ~text() = default;

        // An array of characters as a view: to its first NUL or its end
        template<size_t N>
        SGCL_INLINE_HOT static constexpr view_type _array(const CharT (&s)[N]) noexcept {
            return detail::array_text<CharT, Traits>(s);
        }

        // The characters as bytes, for the UTF-8 primitives (a char8_t
        // string is bytes too)
        SGCL_INLINE_HOT std::string_view _bytes() const noexcept requires (sizeof(CharT) == 1) {
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

        // The first position at or after pos whose code point satisfies
        // pred, walking forward; the last one at or before pos, backward,
        // from the code point that begins at or before pos and covers it
        template<class Pred>
        size_type _find_forward(size_type pos, Pred pred) const noexcept requires (!std::same_as<CharT, char32_t>) {
            if constexpr (sizeof(CharT) == 1) {
                auto b = _bytes();
                for (size_type i = pos; i < b.size();) {
                    auto [c, n] = utf8::decode(b, i);
                    if (pred(c)) {
                        return i;
                    }
                    i += n;
                }
            } else {
                auto size = _self().size();
                for (size_type i = pos; i < size;) {
                    auto [c, n] = _decode(i);
                    if (pred(c)) {
                        return i;
                    }
                    i += n;
                }
            }
            return npos;
        }

        template<class Pred>
        size_type _find_backward(size_type pos, Pred pred) const noexcept requires (!std::same_as<CharT, char32_t>) {
            auto size = _self().size();
            if (size == 0) {
                return npos;
            }
            size_type end = size;
            if (pos < size) {
                auto start = _start_of(pos);
                auto n = _decode(start).second;
                end = start + n > pos ? start + n : pos + 1;
            }
            if constexpr (sizeof(CharT) == 1) {
                auto b = _bytes();
                while (end > 0) {
                    auto [c, n] = utf8::decode_last(b, end);
                    end -= n;
                    if (pred(c)) {
                        return end;
                    }
                }
            } else {
                while (end > 0) {
                    auto [c, n] = _decode_last(end);
                    end -= n;
                    if (pred(c)) {
                        return end;
                    }
                }
            }
            return npos;
        }

        // The code point at i and the units it takes: a UTF-8 sequence, a
        // UTF-16 unit or surrogate pair, a unit of a 32-bit text;
        // {replacement, 1} for a unit that does not begin a valid one
        SGCL_INLINE_HOT pair<char32_t, size_type> _decode(size_type i) const noexcept requires (!std::same_as<CharT, char32_t>) {
            if constexpr (sizeof(CharT) == 1) {
                return utf8::decode(_bytes(), i);
            } else if constexpr (sizeof(CharT) == 2) {
                auto v = view();
                if (_high(v[i]) && i + 1 < v.size() && _low(v[i + 1])) {
                    return {_pair(v[i], v[i + 1]), 2};
                }
                return {_low(v[i]) || _high(v[i]) ? utf8::replacement : char32_t(char16_t(v[i])), 1};
            } else {
                return {char32_t(view()[i]), 1};
            }
        }

        // The code point of UTF-16 units that covers i, where it starts and
        // its units: a surrogate pair whose either half is at i, else the
        // unit at i as its own value (a lone surrogate too, not U+FFFD)
        struct _covering_point {
            char32_t c;
            size_type start;
            size_type width;
        };

        // Where units a[i] and s[i] that differ end as halves of the same
        // letter in either case: the last unit of the pair (U+10400 and
        // U+10428 differ in the low half), else npos. The code points that
        // cover i on each side must start and end together. A surrogate
        // maps to nothing, so the units before i are equal where they are
        // surrogates and the two texts pair them alike; a lone surrogate
        // is its own value, equal only to itself. Not inlined: the loop of
        // equal_fold keeps its registers for the units
        SGCL_NOINLINE static size_type _folded_pair_end(view_type a, view_type s, size_type i) noexcept requires (sizeof(CharT) == 2) {
            auto [ca, at, wa] = _covering(a, i);
            auto [cb, bt, wb] = _covering(s, i);
            if (at == bt && wa == wb && unicode::equal_fold(ca, cb)) {
                return at + wa - 1;
            }
            return npos;
        }

        SGCL_INLINE_HOT static _covering_point _covering(view_type v, size_type i) noexcept requires (sizeof(CharT) == 2) {
            if (_low(v[i]) && i > 0 && _high(v[i - 1])) {
                return {_pair(v[i - 1], v[i]), i - 1, 2};
            }
            if (_high(v[i]) && i + 1 < v.size() && _low(v[i + 1])) {
                return {_pair(v[i], v[i + 1]), i, 2};
            }
            return {char32_t(v[i]), i, 1};
        }

        // The last code point of [0, end) and the units it takes
        SGCL_INLINE_HOT pair<char32_t, size_type> _decode_last(size_type end) const noexcept requires (!std::same_as<CharT, char32_t>) {
            if constexpr (sizeof(CharT) == 1) {
                return utf8::decode_last(_bytes(), end);
            } else if constexpr (sizeof(CharT) == 2) {
                auto v = view();
                if (end >= 2 && _low(v[end - 1]) && _high(v[end - 2])) {
                    return {_pair(v[end - 2], v[end - 1]), 2};
                }
                return {_low(v[end - 1]) || _high(v[end - 1]) ? utf8::replacement : char32_t(char16_t(v[end - 1])), 1};
            } else {
                return {char32_t(view()[end - 1]), 1};
            }
        }

        // Where a forward search from pos starts: pos, or the end of the
        // code point that covers it when pos cuts one (a continuation byte,
        // the low half of a surrogate pair), so that no position inside a
        // code point is an answer
        SGCL_INLINE_HOT size_type _forward_start(size_type pos) const noexcept requires (!std::same_as<CharT, char32_t>) {
            if (pos == 0 || pos >= _self().size()) {
                return pos;
            }
            auto start = _start_of(pos);
            if (start == pos) {
                return pos;
            }
            auto end = start + _decode(start).second;
            return end > pos ? end : pos;
        }

        // The first unit of the code point the unit at pos is part of: the
        // lead byte before continuation bytes, the high surrogate before a
        // low one
        size_type _start_of(size_type pos) const noexcept requires (!std::same_as<CharT, char32_t>) {
            if constexpr (sizeof(CharT) == 1) {
                auto b = _bytes();
                for (size_type k = 1; k < utf8::max_width && pos > 0 && !utf8::starts_rune(b[pos]); ++k) {
                    --pos;
                }
                return pos;
            } else if constexpr (sizeof(CharT) == 2) {
                auto v = view();
                return pos > 0 && _low(v[pos]) && _high(v[pos - 1]) ? pos - 1 : pos;
            } else {
                return pos;
            }
        }

        // The units of the code point at i: what split("") hands out
        SGCL_INLINE_HOT size_type _width_at(size_type i) const noexcept {
            if constexpr (std::same_as<CharT, char32_t>) {
                return 1;
            } else {
                return _decode(i).second;
            }
        }

        SGCL_INLINE_HOT static constexpr bool _high(CharT u) noexcept {
            return (char32_t(u) & 0xFFFFFC00u) == 0xD800u && sizeof(CharT) == 2;
        }

        SGCL_INLINE_HOT static constexpr bool _low(CharT u) noexcept {
            return (char32_t(u) & 0xFFFFFC00u) == 0xDC00u && sizeof(CharT) == 2;
        }

        SGCL_INLINE_HOT static constexpr char32_t _pair(CharT high, CharT low) noexcept {
            return 0x10000 + ((char32_t(high) - 0xD800) << 10) + (char32_t(low) - 0xDC00);
        }

        // The text a code point is searched as: the units of its encoding
        // (UTF-8 bytes, UTF-16 units), a view of them for the length of
        // the expression that made it
        struct _encoded {
            std::conditional_t<sizeof(CharT) == 1, utf8::encoded, detail::WideEncoded<CharT>> e;
            SGCL_INLINE_HOT constexpr _encoded(char32_t c) noexcept : e(c) {}
            SGCL_INLINE_HOT operator view_type() const noexcept {
                if constexpr (sizeof(CharT) == 1) {
                    return view_type(reinterpret_cast<const CharT*>(e.bytes), e.size);
                } else {
                    return view_type(e.units, e.size);
                }
            }
        };

    private:
        SGCL_INLINE_HOT const Derived& _self() const noexcept {
            return static_cast<const Derived&>(*this);
        }
    };
}
