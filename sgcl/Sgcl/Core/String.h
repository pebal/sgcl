//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// String: an immutable string of one word, the
// characters in a managed object shared by every copy, and StringView, a
// view of a String that holds the object; a new string for
// every result (the same object when nothing changes: Trim, Replace,
// ToLower of a string they leave as it is). Split (a range of views,
// Pieces), Join, Trim, Replace, Repeat, ToLower, ToUpper as in C# and
// Java. BasicString<CharT> for the other character types.
#pragma once

#include "../../core/string.h"
#include "Types.h"

#include <ostream>
#include <ranges>
#include <string>

namespace Sgcl {
    template<class CharT, class Traits = std::char_traits<CharT>>
    class BasicStringView;

    template<class CharT, class Traits = std::char_traits<CharT>>
    class BasicString {
    public:
        using CharType = CharT;
        using InnerType = sgcl::basic_string<CharT, Traits>;
        using ViewType = std::basic_string_view<CharT, Traits>;
        using SizeType = size_t;

        static constexpr SizeType NoPosition = InnerType::npos;
        using Pieces = typename InnerType::pieces;             // what Split and SplitWords return: a range of views

        constexpr BasicString() noexcept = default;
        constexpr BasicString(std::nullptr_t) = delete;

        BasicString(const CharT* s)
        : _s(s) {
        }

        BasicString(const CharT* s, SizeType n)
        : _s(s, n) {
        }

        BasicString(ViewType s)
        : _s(s) {
        }

        template<class V>
        requires std::is_convertible_v<const V&, ViewType> && (!std::is_convertible_v<const V&, const CharT*>) && (!std::is_same_v<std::remove_cvref_t<V>, BasicString>) && (!std::is_same_v<std::remove_cvref_t<V>, InnerType>)
        explicit BasicString(const V& v)
        : _s(ViewType(v)) {
        }

        BasicString(SizeType n, CharT c)
        : _s(n, c) {
        }

        template<std::input_iterator It>
        BasicString(It first, It last)
        : _s(first, last) {
        }

        BasicString(std::initializer_list<CharT> il)
        : _s(il) {
        }

        BasicString(InnerType s) noexcept
        : _s(std::move(s)) {
        }

        // From a view that holds a string's object: that object when the
        // view is the whole of it (no copy), a new string of the
        // characters otherwise
        explicit BasicString(const BasicStringView<CharT, Traits>& v)
        : _s(v.Inner()) {
        }

        BasicString(const BasicString&) noexcept = default;
        BasicString(BasicString&&) noexcept = default;
        BasicString& operator=(const BasicString&) noexcept = default;
        BasicString& operator=(BasicString&&) noexcept = default;

        BasicString& operator=(const CharT* s) {
            _s = s;
            return *this;
        }

        BasicString& operator=(ViewType s) {
            _s = s;
            return *this;
        }

        // The characters, zero-terminated
        const CharT* CStr() const noexcept {
            return _s.c_str();
        }

        const CharT* Data() const noexcept {
            return _s.data();
        }

        SizeType Length() const noexcept {
            return _s.length();
        }

        bool IsEmpty() const noexcept {
            return _s.empty();
        }

        // The string as a view that holds the object (BasicStringView,
        // below): the whole of it, or the characters [pos, pos + n),
        // shared, nothing copied
        BasicStringView<CharT, Traits> View() const noexcept {
            return _s.view();
        }

        BasicStringView<CharT, Traits> View(SizeType pos, SizeType n = NoPosition) const {
            return _s.view(pos, n);
        }

        operator ViewType() const noexcept {
            return ViewType(_s);
        }

        // A std::string with a copy of the characters
        std::basic_string<CharT, Traits> ToStd() const {
            return _s.str();
        }

        const CharT& operator[](SizeType i) const noexcept {
            return _s[i];
        }

        const CharT& First() const noexcept {
            return _s.front();
        }

        const CharT& Last() const noexcept {
            return _s.back();
        }

        // A new string of the characters from `pos`, `n` of them at most
        BasicString Substring(SizeType pos = 0, SizeType n = NoPosition) const {
            return BasicString(_s.substr(pos, n));
        }

        // The position of the first (last) occurrence, NoPosition when none
        SizeType IndexOf(ViewType s, SizeType from = 0) const noexcept {
            return _s.find(s, from);
        }

        SizeType IndexOf(CharT c, SizeType from = 0) const noexcept {
            return _s.find(c, from);
        }

        SizeType LastIndexOf(ViewType s, SizeType from = NoPosition) const noexcept {
            return _s.rfind(s, from);
        }

        SizeType LastIndexOf(CharT c, SizeType from = NoPosition) const noexcept {
            return _s.rfind(c, from);
        }

        SizeType IndexOfAny(ViewType chars, SizeType from = 0) const noexcept {
            return _s.find_first_of(chars, from);
        }

        SizeType LastIndexOfAny(ViewType chars, SizeType from = NoPosition) const noexcept {
            return _s.find_last_of(chars, from);
        }

        bool Contains(ViewType s) const noexcept {
            return _s.contains(s);
        }

        bool Contains(CharT c) const noexcept {
            return _s.contains(c);
        }

        bool StartsWith(ViewType s) const noexcept {
            return _s.starts_with(s);
        }

        bool StartsWith(CharT c) const noexcept {
            return _s.starts_with(c);
        }

        bool EndsWith(ViewType s) const noexcept {
            return _s.ends_with(s);
        }

        bool EndsWith(CharT c) const noexcept {
            return _s.ends_with(c);
        }

        // The pieces between the occurrences of `sep`, in order: an
        // empty piece where two separators meet or one ends the string;
        // the whole string when `sep` does not occur; every character on
        // its own for an empty `sep`; no piece for an empty string. With
        // `maxParts`, at most that many, the last holding the rest; 0 is
        // no limit. A range of views into this string (Pieces), walked
        // as it goes: `for (std::string_view piece : s.Split(','))`
        // allocates nothing; a List<String> is built from it when the
        // pieces are to be kept: `List<String> parts(s.Split(','))`.
        Pieces Split(ViewType sep, SizeType maxParts = 0) const {
            return _s.split(sep, maxParts);
        }

        Pieces Split(CharT sep, SizeType maxParts = 0) const {
            return _s.split(sep, maxParts);
        }

        Pieces Split(const CharT* sep, SizeType maxParts = 0) const {
            return _s.split(sep, maxParts);
        }

        Pieces Split(const BasicString& sep, SizeType maxParts = 0) const {
            return _s.split(sep._s, maxParts);
        }

        // The words: the pieces between runs of white space, none empty
        Pieces SplitWords() const {
            return _s.fields();
        }

        // One string of the parts (strings, views, literals) with `sep`
        // between each two, built once
        template<std::ranges::input_range R>
        requires std::is_convertible_v<std::ranges::range_reference_t<R>, ViewType>
        static BasicString Join(R&& parts, ViewType sep) {
            return BasicString(InnerType::join(std::forward<R>(parts), sep));
        }

        template<std::ranges::input_range R>
        requires std::is_convertible_v<std::ranges::range_reference_t<R>, ViewType>
        static BasicString Join(R&& parts, CharT sep) {
            return BasicString(InnerType::join(std::forward<R>(parts), sep));
        }

        template<std::ranges::input_range R>
        requires std::is_convertible_v<std::ranges::range_reference_t<R>, ViewType>
        static BasicString Join(R&& parts, const CharT* sep) {
            return BasicString(InnerType::join(std::forward<R>(parts), sep));
        }

        // Without the characters of `chars` (white space by default) at
        // both ends, at the start, at the end: the same object when
        // there are none
        BasicString Trim() const {
            return BasicString(_s.trim());
        }

        BasicString Trim(ViewType chars) const {
            return BasicString(_s.trim(chars));
        }

        BasicString TrimStart() const {
            return BasicString(_s.trim_left());
        }

        BasicString TrimStart(ViewType chars) const {
            return BasicString(_s.trim_left(chars));
        }

        BasicString TrimEnd() const {
            return BasicString(_s.trim_right());
        }

        BasicString TrimEnd(ViewType chars) const {
            return BasicString(_s.trim_right(chars));
        }

        // Without `prefix` at the start (`suffix` at the end) when it is
        // there; the same object when it is not
        BasicString TrimPrefix(ViewType prefix) const {
            return BasicString(_s.trim_prefix(prefix));
        }

        BasicString TrimSuffix(ViewType suffix) const {
            return BasicString(_s.trim_suffix(suffix));
        }

        // With every occurrence of `from` (the first `count` of them,
        // when given) replaced by `to`; the same object when `from` is
        // empty or does not occur
        BasicString Replace(ViewType from, ViewType to, SizeType count = 0) const {
            return BasicString(_s.replace(from, to, count));
        }

        BasicString Replace(CharT from, CharT to, SizeType count = 0) const {
            return BasicString(_s.replace(from, to, count));
        }

        // The string `count` times over: empty for 0, the same object for 1
        BasicString Repeat(SizeType count) const {
            return BasicString(_s.repeat(count));
        }

        // With the ASCII letters in lower (upper) case; the same object
        // when no letter changes
        BasicString ToLower() const {
            return BasicString(_s.to_lower());
        }

        BasicString ToUpper() const {
            return BasicString(_s.to_upper());
        }

        // The same object, or the same characters (the hashes first)
        bool Equals(const BasicString& o) const noexcept {
            return _s.equals(o._s);
        }

        // Negative, zero, positive: this before, the same as, after `s`
        int Compare(ViewType s) const noexcept {
            return _s.compare(s);
        }

        // The hash of the characters, computed once and kept with them
        size_t Hash() const noexcept {
            return _s.hash();
        }

        // The object's address: the identity; null when empty
        const void* Object() const noexcept {
            return _s.object();
        }

        // The characters copied out, n of them at most from pos: how many
        SizeType CopyTo(CharT* dest, SizeType n, SizeType pos = 0) const {
            return _s.copy(dest, n, pos);
        }

        void Swap(BasicString& o) noexcept {
            _s.swap(o._s);
        }

        InnerType& Inner() noexcept {
            return _s;
        }

        const InnerType& Inner() const noexcept {
            return _s;
        }

    private:
        InnerType _s;
    };

    using String = BasicString<char>;
    using WString = BasicString<wchar_t>;
    using U8String = BasicString<char8_t>;
    using U16String = BasicString<char16_t>;
    using U32String = BasicString<char32_t>;

    template<class C, class T>
    bool operator==(const BasicString<C, T>& a, const BasicString<C, T>& b) noexcept {
        return a.Equals(b);
    }

    template<class C, class T>
    bool operator==(const BasicString<C, T>& a, std::type_identity_t<std::basic_string_view<C, T>> b) noexcept {
        return a.View() == b;
    }

    template<class C, class T>
    bool operator==(const BasicString<C, T>& a, const C* b) noexcept {
        return a.View() == b;
    }

    template<class C, class T>
    std::strong_ordering operator<=>(const BasicString<C, T>& a, const BasicString<C, T>& b) noexcept {
        return a.View() <=> b.View();
    }

    template<class C, class T>
    std::strong_ordering operator<=>(const BasicString<C, T>& a, std::type_identity_t<std::basic_string_view<C, T>> b) noexcept {
        return a.View() <=> b;
    }

    template<class C, class T>
    std::strong_ordering operator<=>(const BasicString<C, T>& a, const C* b) noexcept {
        return a.View() <=> std::basic_string_view<C, T>(b);
    }

    template<class C, class T>
    BasicString<C, T> operator+(const BasicString<C, T>& a, const BasicString<C, T>& b) {
        return BasicString<C, T>(a.Inner() + b.Inner());
    }

    template<class C, class T>
    BasicString<C, T> operator+(const BasicString<C, T>& a, std::type_identity_t<std::basic_string_view<C, T>> b) {
        return BasicString<C, T>(a.Inner() + b);
    }

    template<class C, class T>
    BasicString<C, T> operator+(std::type_identity_t<std::basic_string_view<C, T>> a, const BasicString<C, T>& b) {
        return BasicString<C, T>(a + b.Inner());
    }

    template<class C, class T>
    BasicString<C, T> operator+(const BasicString<C, T>& a, const C* b) {
        return BasicString<C, T>(a.Inner() + b);
    }

    template<class C, class T>
    BasicString<C, T> operator+(const C* a, const BasicString<C, T>& b) {
        return BasicString<C, T>(a + b.Inner());
    }

    template<class C, class T>
    BasicString<C, T> operator+(const BasicString<C, T>& a, C b) {
        return BasicString<C, T>(a.Inner() + b);
    }

    template<class C, class T>
    BasicString<C, T> operator+(C a, const BasicString<C, T>& b) {
        return BasicString<C, T>(a + b.Inner());
    }

    // A number as a String: ToString(42), ToString(2.5)
    template<class T>
    requires std::is_arithmetic_v<T>
    String ToString(T v) {
        return String(sgcl::to_string(v));
    }

    // A number from its text, the reverse of ToString: Parse<int>("42"),
    // Parse<double>("2.5"), Parse<bool>("true"), None when the text is
    // not exactly one number that fits the type; a base other than 10
    // for the integers: Parse<int>("ff", 16). C#'s TryParse as an
    // Optional.
    template<class T>
    requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    Optional<T> Parse(std::string_view text, int base = 10) noexcept {
        return sgcl::parse<T>(text, base);
    }

    template<class T>
    requires std::is_floating_point_v<T> || std::is_same_v<T, bool>
    Optional<T> Parse(std::string_view text) noexcept {
        return sgcl::parse<T>(text);
    }

    template<class C, class T>
    std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& os, const BasicString<C, T>& s) {
        return os << s.View();
    }

    template<class C, class T>
    void swap(BasicString<C, T>& a, BasicString<C, T>& b) noexcept {
        a.Swap(b);
    }

    template<class C, class T>
    const C* begin(const BasicString<C, T>& s) noexcept {
        return s.Data();
    }

    template<class C, class T>
    const C* end(const BasicString<C, T>& s) noexcept {
        return s.Data() + s.Length();
    }

    // BasicStringView: a view of a String that holds the string's object,
    // two words (the string's word, a tracked pointer, and the range in
    // it), so a piece of a String lives on its own for as long as the view
    // does, wherever a Ptr may live; what a substring is in Go and a
    // ReadOnlyMemory<char> in C#. The read interface of a String over the
    // range, Substring as another view of the same object, Trim and the
    // like as views, ToString for a String (the same object when the view
    // is the whole of it). Made by String::View, by Substring, by the
    // pieces of Split; never from a literal, which holds nothing.
    template<class CharT, class Traits>
    class BasicStringView {
    public:
        using CharType = CharT;
        using InnerType = sgcl::basic_string_view<CharT, Traits>;
        using StringType = BasicString<CharT, Traits>;
        using ViewType = std::basic_string_view<CharT, Traits>;
        using SizeType = size_t;

        static constexpr SizeType NoPosition = InnerType::npos;

        constexpr BasicStringView() noexcept = default;

        BasicStringView(const StringType& s) noexcept
        : _v(s.Inner()) {
        }

        BasicStringView(InnerType v) noexcept
        : _v(std::move(v)) {
        }

        BasicStringView(const BasicStringView&) noexcept = default;
        BasicStringView(BasicStringView&&) noexcept = default;
        BasicStringView& operator=(const BasicStringView&) noexcept = default;
        BasicStringView& operator=(BasicStringView&&) noexcept = default;

        // The characters, not terminated
        const CharT* Data() const noexcept {
            return _v.data();
        }

        SizeType Length() const noexcept {
            return _v.length();
        }

        bool IsEmpty() const noexcept {
            return _v.empty();
        }

        ViewType View() const noexcept {
            return _v.view();
        }

        operator ViewType() const noexcept {
            return _v.view();
        }

        // A String of the characters: the string's own object when the
        // view is the whole of it, a copy otherwise
        StringType ToString() const {
            return StringType(_v.str());
        }

        // A std::string with a copy of the characters
        std::basic_string<CharT, Traits> ToStd() const {
            return std::basic_string<CharT, Traits>(_v.view());
        }

        const CharT& operator[](SizeType i) const noexcept {
            return _v[i];
        }

        const CharT& First() const noexcept {
            return _v.front();
        }

        const CharT& Last() const noexcept {
            return _v.back();
        }

        // The characters [pos, pos + n) as a view of the same object
        BasicStringView Substring(SizeType pos = 0, SizeType n = NoPosition) const {
            return _v.substr(pos, n);
        }

        // The view narrowed: n characters off the start, off the end
        void RemovePrefix(SizeType n) noexcept {
            _v.remove_prefix(n);
        }

        void RemoveSuffix(SizeType n) noexcept {
            _v.remove_suffix(n);
        }

        // The position of the first (last) occurrence, NoPosition when none
        SizeType IndexOf(ViewType s, SizeType from = 0) const noexcept {
            return _v.find(s, from);
        }

        SizeType IndexOf(CharT c, SizeType from = 0) const noexcept {
            return _v.find(c, from);
        }

        SizeType LastIndexOf(ViewType s, SizeType from = NoPosition) const noexcept {
            return _v.rfind(s, from);
        }

        SizeType LastIndexOf(CharT c, SizeType from = NoPosition) const noexcept {
            return _v.rfind(c, from);
        }

        SizeType IndexOfAny(ViewType chars, SizeType from = 0) const noexcept {
            return _v.find_first_of(chars, from);
        }

        SizeType LastIndexOfAny(ViewType chars, SizeType from = NoPosition) const noexcept {
            return _v.find_last_of(chars, from);
        }

        bool Contains(ViewType s) const noexcept {
            return _v.contains(s);
        }

        bool Contains(CharT c) const noexcept {
            return _v.contains(c);
        }

        bool StartsWith(ViewType s) const noexcept {
            return _v.starts_with(s);
        }

        bool StartsWith(CharT c) const noexcept {
            return _v.starts_with(c);
        }

        bool EndsWith(ViewType s) const noexcept {
            return _v.ends_with(s);
        }

        bool EndsWith(CharT c) const noexcept {
            return _v.ends_with(c);
        }

        // Without white space (the characters of `chars`) at both ends,
        // at the start, at the end; without a prefix (a suffix) when it
        // is there: views of the same object, nothing copied
        BasicStringView Trim() const noexcept {
            return _v.trim();
        }

        BasicStringView Trim(ViewType chars) const noexcept {
            return _v.trim(chars);
        }

        BasicStringView TrimStart() const noexcept {
            return _v.trim_left();
        }

        BasicStringView TrimStart(ViewType chars) const noexcept {
            return _v.trim_left(chars);
        }

        BasicStringView TrimEnd() const noexcept {
            return _v.trim_right();
        }

        BasicStringView TrimEnd(ViewType chars) const noexcept {
            return _v.trim_right(chars);
        }

        BasicStringView TrimPrefix(ViewType prefix) const noexcept {
            return _v.trim_prefix(prefix);
        }

        BasicStringView TrimSuffix(ViewType suffix) const noexcept {
            return _v.trim_suffix(suffix);
        }

        // Negative, zero, positive: this before, the same as, after `s`
        int Compare(ViewType s) const noexcept {
            return _v.compare(s);
        }

        // The hash of the characters: the one a String of them has
        size_t Hash() const noexcept {
            return _v.hash();
        }

        // The address of the string's object the view is of; null when empty
        const void* Object() const noexcept {
            return _v.object();
        }

        // The characters copied out, n of them at most from pos: how many
        SizeType CopyTo(CharT* dest, SizeType n, SizeType pos = 0) const {
            return _v.copy(dest, n, pos);
        }

        void Swap(BasicStringView& o) noexcept {
            _v.swap(o._v);
        }

        InnerType& Inner() noexcept {
            return _v;
        }

        const InnerType& Inner() const noexcept {
            return _v;
        }

    private:
        InnerType _v;
    };

    using StringView = BasicStringView<char>;
    using WStringView = BasicStringView<wchar_t>;
    using U8StringView = BasicStringView<char8_t>;
    using U16StringView = BasicStringView<char16_t>;
    using U32StringView = BasicStringView<char32_t>;

    template<class C, class T>
    bool operator==(const BasicStringView<C, T>& a, const BasicStringView<C, T>& b) noexcept {
        return a.Inner() == b.Inner();
    }

    template<class C, class T>
    bool operator==(const BasicStringView<C, T>& a, const BasicString<C, T>& b) noexcept {
        return a.Inner() == b.Inner();
    }

    template<class C, class T>
    bool operator==(const BasicStringView<C, T>& a, std::type_identity_t<std::basic_string_view<C, T>> b) noexcept {
        return a.View() == b;
    }

    template<class C, class T>
    bool operator==(const BasicStringView<C, T>& a, const C* b) noexcept {
        return a.View() == b;
    }

    template<class C, class T>
    std::strong_ordering operator<=>(const BasicStringView<C, T>& a, const BasicStringView<C, T>& b) noexcept {
        return a.Inner() <=> b.Inner();
    }

    template<class C, class T>
    std::strong_ordering operator<=>(const BasicStringView<C, T>& a, const BasicString<C, T>& b) noexcept {
        return a.Inner() <=> b.Inner();
    }

    template<class C, class T>
    std::strong_ordering operator<=>(const BasicStringView<C, T>& a, std::type_identity_t<std::basic_string_view<C, T>> b) noexcept {
        return a.View() <=> b;
    }

    template<class C, class T>
    std::strong_ordering operator<=>(const BasicStringView<C, T>& a, const C* b) noexcept {
        return a.View() <=> std::basic_string_view<C, T>(b);
    }

    template<class C, class T>
    std::basic_ostream<C, T>& operator<<(std::basic_ostream<C, T>& os, const BasicStringView<C, T>& v) {
        return os << v.View();
    }

    template<class C, class T>
    void swap(BasicStringView<C, T>& a, BasicStringView<C, T>& b) noexcept {
        a.Swap(b);
    }

    template<class C, class T>
    const C* begin(const BasicStringView<C, T>& v) noexcept {
        return v.Data();
    }

    template<class C, class T>
    const C* end(const BasicStringView<C, T>& v) noexcept {
        return v.Data() + v.Length();
    }
}

// A Dictionary or a SortedDictionary keyed by strings is searched with a
// string_view or a literal as with a String, and no String is made for
// the search: the hash, the equality and the order are transparent, as
// for sgcl::string.
template<class C, class T>
struct std::hash<Sgcl::BasicString<C, T>> {
    using is_transparent = void;

    size_t operator()(const Sgcl::BasicString<C, T>& s) const noexcept {
        return s.Hash();
    }

    size_t operator()(std::basic_string_view<C, T> s) const noexcept {
        return sgcl::basic_string<C, T>::hash_of(s);
    }

    size_t operator()(const C* s) const noexcept {
        return sgcl::basic_string<C, T>::hash_of(s);
    }
};

template<class C, class T>
struct std::hash<Sgcl::BasicStringView<C, T>> {
    using is_transparent = void;

    size_t operator()(const Sgcl::BasicStringView<C, T>& v) const noexcept {
        return v.Hash();
    }

    size_t operator()(const Sgcl::BasicString<C, T>& s) const noexcept {
        return s.Hash();
    }

    size_t operator()(std::basic_string_view<C, T> s) const noexcept {
        return sgcl::basic_string<C, T>::hash_of(s);
    }

    size_t operator()(const C* s) const noexcept {
        return sgcl::basic_string<C, T>::hash_of(s);
    }
};

template<class C, class T>
struct std::equal_to<Sgcl::BasicStringView<C, T>> {
    using is_transparent = void;

    template<class A, class B>
    bool operator()(const A& a, const B& b) const noexcept {
        return a == b;
    }
};

template<class C, class T>
struct std::less<Sgcl::BasicStringView<C, T>> {
    using is_transparent = void;

    template<class A, class B>
    bool operator()(const A& a, const B& b) const noexcept {
        return a < b;
    }
};

template<class C, class T>
struct std::equal_to<Sgcl::BasicString<C, T>> {
    using is_transparent = void;

    template<class A, class B>
    bool operator()(const A& a, const B& b) const noexcept {
        return a == b;
    }
};

template<class C, class T>
struct std::less<Sgcl::BasicString<C, T>> {
    using is_transparent = void;

    template<class A, class B>
    bool operator()(const A& a, const B& b) const noexcept {
        return a < b;
    }
};

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

