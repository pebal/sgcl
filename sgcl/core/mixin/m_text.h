//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <compare>
#include <cstddef>
#include <string>
#include <string_view>

namespace sgcl {
    // m_text<Derived, CharT, Traits>: the read side of std::string_view
    // as members of whatever holds characters through data() and size():
    // basic_string and slice<const CharT>. A mixin (m_): a static
    // interface, no virtual method, no state; its constructor and
    // destructor are protected. Every operation runs on a
    // std::basic_string_view over the characters; the operations that
    // make a new object (substr, trim, split) stay with the class, whose
    // type they return.
    template<class Derived, class CharT, class Traits = std::char_traits<CharT>>
    class m_text {
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

        // The white space of trim and fields: the six of isspace in the C locale
        static view_type spaces() noexcept {
            static constexpr CharT chars[] = {CharT(' '), CharT('\t'), CharT('\n'), CharT('\v'), CharT('\f'), CharT('\r')};
            return view_type(chars, 6);
        }

    protected:
        m_text() = default;
        ~m_text() = default;

    private:
        const Derived& _self() const noexcept {
            return static_cast<const Derived&>(*this);
        }
    };
}
