//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The conditional rules ask for the combining class, and the result is
// built as normalize builds one, so this header stands on that one
#include "normalize.h"
#include "segment.h"
#include "detail/case_tables.h"

// The case of a text, where core has the case of a code point. Three
// things happen here that cannot happen one code point at a time: a
// letter may become two ("straße" is "STRASSE"), a letter may depend on
// what stands around it (a Greek sigma at the end of a word is written
// differently), and a letter may depend on the language (in Turkish an i
// keeps its dot when it grows, and an I loses one it never had).
namespace sgcl::txt {
    // The language a mapping may depend on. Three of them change the case
    // of a letter — Turkish, Azerbaijani and Lithuanian — and the tables
    // of Unicode name no others; the type takes a BCP-47 tag all the same,
    // so that a tag out of a header or out of the system needs no table of
    // its own at the caller, and so that a collator can take the same type
    // when it comes. An unknown tag is the root locale, and nothing throws.
    class locale {
    public:
        constexpr locale() noexcept = default;

        explicit locale(const string& tag) noexcept
        : _language(_parse(tag.view())) {
        }

        static constexpr locale root() noexcept {
            return locale();
        }

        static constexpr locale turkish() noexcept {
            return locale(_packed("tr"));
        }

        static constexpr locale azerbaijani() noexcept {
            return locale(_packed("az"));
        }

        static constexpr locale lithuanian() noexcept {
            return locale(_packed("lt"));
        }

        constexpr bool operator==(const locale&) const noexcept = default;

        // Whether the language writes an i the Turkish way, which is the
        // one question the case mappings ask
        constexpr bool dotted_i() const noexcept {
            return *this == turkish() || *this == azerbaijani();
        }

        constexpr bool keeps_dot() const noexcept {
            return *this == lithuanian();
        }

        // The language subtag in four bytes, which is what a table keyed
        // by language is looked up with — the collator's tailorings are
        // the one such table in the library
        constexpr uint32_t subtag() const noexcept {
            return _language;
        }

    private:
        explicit constexpr locale(uint32_t language) noexcept
        : _language(language) {
        }

        // The letters of the language subtag, lower cased, in four bytes;
        // anything longer or stranger than a subtag is the root locale
        static constexpr uint32_t _packed(std::string_view tag) noexcept {
            uint32_t out = 0;
            size_t n = 0;
            for (char c : tag) {
                if (c == '-' || c == '_') {
                    break;
                }
                if (++n > 3) {
                    return 0;
                }
                if (c >= 'A' && c <= 'Z') {
                    c = char(c + 32);
                }
                if (c < 'a' || c > 'z') {
                    return 0;
                }
                out = (out << 8) | uint32_t(uint8_t(c));
            }
            return n >= 2 ? out : 0;
        }

        static constexpr uint32_t _parse(std::string_view tag) noexcept {
            return _packed(tag);
        }

        uint32_t _language = 0;
    };

    namespace detail {
        constexpr bool is_cased(char32_t c) noexcept {
            return in_set(c, case_tables::Cased);
        }

        constexpr bool is_case_ignorable(char32_t c) noexcept {
            return in_set(c, case_tables::CaseIgnorable);
        }

        constexpr bool is_soft_dotted(char32_t c) noexcept {
            return in_set(c, case_tables::SoftDotted);
        }

        // The full mapping of one code point, or nothing when the simple
        // one core has is the whole of it
        constexpr Decomposition full_of(char32_t c, const DecompTable& table) noexcept {
            return decomposition_of(c, table);
        }

        inline void append(vector<char32_t>& out, Decomposition d) {
            for (size_t i = 0; i < d.size; ++i) {
                char32_t x = d.units[i];
                if (x >= 0xD800 && x < 0xDC00 && i + 1 < d.size) {
                    x = 0x10000 + ((x - 0xD800) << 10) + (d.units[i + 1] - 0xDC00);
                    ++i;
                }
                out.push_back(x);
            }
        }

        // Final_Sigma: a cased letter before, none after, and what may
        // stand between is what the case mappings ignore
        inline bool final_sigma(std::string_view text, size_t at, size_t after) noexcept {
            bool before = false;
            for (size_t i = at; i > 0;) {
                auto [c, n] = utf8::decode_last(text, i);
                i -= n;
                if (is_case_ignorable(c)) {
                    continue;
                }
                before = is_cased(c);
                break;
            }
            if (!before) {
                return false;
            }
            for (size_t i = after; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                i += n;
                if (is_case_ignorable(c)) {
                    continue;
                }
                return !is_cased(c);
            }
            return true;
        }

        // More_Above: a mark above follows, before any starter
        inline bool more_above(std::string_view text, size_t after) noexcept {
            for (size_t i = after; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                i += n;
                auto cc = ccc_fn(c);
                if (cc == 230) {
                    return true;
                }
                if (cc == 0) {
                    return false;
                }
            }
            return false;
        }

        // After_Soft_Dotted: the last letter with a dot of its own had
        // nothing above it since
        inline bool after_soft_dotted(std::string_view text, size_t at) noexcept {
            for (size_t i = at; i > 0;) {
                auto [c, n] = utf8::decode_last(text, i);
                i -= n;
                auto cc = ccc_fn(c);
                if (cc == 230) {
                    return false;
                }
                if (cc == 0) {
                    return is_soft_dotted(c);
                }
            }
            return false;
        }

        // After_I: the last letter before, marks aside, is an I —
        // the condition the Turkish rules are written with, and not the
        // same as a letter with a dot of its own
        inline bool after_I(std::string_view text, size_t at) noexcept {
            for (size_t i = at; i > 0;) {
                auto [c, n] = utf8::decode_last(text, i);
                i -= n;
                auto cc = ccc_fn(c);
                if (cc == 230) {
                    return false;
                }
                if (cc == 0) {
                    return c == U'I';
                }
            }
            return false;
        }

        // Before_Dot: a combining dot above follows, before any other
        // mark at that height
        inline bool before_dot(std::string_view text, size_t after) noexcept {
            for (size_t i = after; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                i += n;
                if (c == 0x0307) {
                    return true;
                }
                auto cc = ccc_fn(c);
                if (cc == 0 || cc == 230) {
                    return false;
                }
            }
            return false;
        }

        enum class casing { lower, upper, title };

        // One code point into the buffer, with the rules that depend on
        // the language and on what stands around it
        inline void case_one(vector<char32_t>& out, std::string_view text, size_t at, size_t after,
                             char32_t c, casing to, locale where) {
            if (to == casing::lower) {
                if (c == 0x03A3 && final_sigma(text, at, after)) {
                    out.push_back(0x03C2);                     // the sigma that ends a word
                    return;
                }
                if (where.dotted_i()) {
                    if (c == 0x0130) {
                        out.push_back(U'i');                   // in Turkish the dot is the letter
                        return;
                    }
                    if (c == 0x0307 && after_I(text, at)) {
                        return;                                // a dot written after an I is the letter's own
                    }
                    if (c == U'I') {
                        out.push_back(before_dot(text, after) ? U'i' : 0x0131);
                        return;
                    }
                }
                if (where.keeps_dot() && (c == U'I' || c == U'J' || c == 0x012E) && more_above(text, after)) {
                    out.push_back(c == U'I' ? U'i' : c == U'J' ? U'j' : 0x012F);
                    out.push_back(0x0307);                     // Lithuanian keeps the dot under the accent
                    return;
                }
                if (auto d = full_of(c, case_tables::FullLower)) {
                    append(out, d);
                    return;
                }
                out.push_back(unicode::to_lower(c));
                return;
            }
            if (where.dotted_i() && c == U'i' && to != casing::lower) {
                out.push_back(0x0130);                         // and back again
                return;
            }
            if (where.keeps_dot() && c == 0x0307 && after_soft_dotted(text, at)) {
                return;
            }
            if (auto d = full_of(c, to == casing::title ? case_tables::FullTitle : case_tables::FullUpper)) {
                append(out, d);
                return;
            }
            out.push_back(unicode::to_upper(c));
        }

        // Latin text has no mapping that changes its length, none that
        // depends on what stands around it, and none that the root
        // locale spells its own way, so the whole apparatus above is a
        // shift of one bit — and the code points never have to become a
        // vector and go back to bytes again. Turkish, Azerbaijani and
        // Lithuanian are not the root locale and do not come this way:
        // they spell an i of their own.
        template<char Lo, char Hi, int By>
        string ascii_cased(const string& text) {
            auto v = text.view();
            std::string out(v);
            bool changed = false;
            // Without a branch: see string::_ascii_cased — one here
            // costs twelve times what the work does
            for (auto& c : out) {
                bool letter = uint8_t(uint8_t(c) - uint8_t(Lo)) <= uint8_t(Hi - Lo);
                c = char(int(c) + (letter ? By : 0));
                changed |= letter;
            }
            return changed ? string(out.data(), out.size()) : text;
        }

        template<class F>
        string cased_text(const string& text, F&& each) {
            auto v = text.view();
            vector<char32_t> out;
            out.reserve(v.size());
            each(v, out);
            return encoded(out);
        }
    }

    // The text in lower case, by the full mappings: a letter may become
    // two, the Greek sigma at the end of a word is written its own way,
    // and the three languages that spell an i differently are told by the
    // locale. A text that changes in no letter comes back as itself.
    inline string to_lower_full(const string& text, locale where = {}) {
        if (where == locale() && utf8::all_ascii(text.view())) {
            return detail::ascii_cased<'A', 'Z', 32>(text);
        }
        return detail::cased_text(text, [&](std::string_view v, vector<char32_t>& out) {
            for (size_t i = 0; i < v.size();) {
                auto [c, n] = utf8::decode(v, i);
                detail::case_one(out, v, i, i + n, c, detail::casing::lower, where);
                i += n;
            }
        });
    }

    inline string to_upper_full(const string& text, locale where = {}) {
        if (where == locale() && utf8::all_ascii(text.view())) {
            return detail::ascii_cased<'a', 'z', -32>(text);
        }
        return detail::cased_text(text, [&](std::string_view v, vector<char32_t>& out) {
            for (size_t i = 0; i < v.size();) {
                auto [c, n] = utf8::decode(v, i);
                detail::case_one(out, v, i, i + n, c, detail::casing::upper, where);
                i += n;
            }
        });
    }

    // The first letter of every word in title case and the rest of it in
    // lower case. The words are the ones UAX #29 finds ([segment]), so
    // "don't" is one word and its apostrophe does not start a new letter
    inline string to_title(const string& text, locale where = {}) {
        return detail::cased_text(text, [&](std::string_view v, vector<char32_t>& out) {
            for (auto word : words(text)) {
                size_t base = size_t(word.data() - text.data());
                bool first = true;
                for (size_t i = 0; i < word.size();) {
                    auto [c, n] = utf8::decode(v, base + i);
                    bool cased = detail::is_cased(c);
                    detail::case_one(out, v, base + i, base + i + n, c,
                                     first && cased ? detail::casing::title : detail::casing::lower, where);
                    first = first && !cased;
                    i += n;
                }
            }
        });
    }

    // The text folded for comparison: not a case of its own and not for
    // showing, only for asking whether two texts are the same word. The
    // full folding, so "straße" and "STRASSE" fold alike
    inline string fold_case(const string& text) {
        if (utf8::all_ascii(text.view())) {
            return detail::ascii_cased<'A', 'Z', 32>(text);
        }
        return detail::cased_text(text, [&](std::string_view v, vector<char32_t>& out) {
            for (size_t i = 0; i < v.size();) {
                auto [c, n] = utf8::decode(v, i);
                i += n;
                if (auto d = detail::full_of(c, detail::case_tables::FullFold)) {
                    detail::append(out, d);
                } else {
                    out.push_back(unicode::to_lower(c));
                }
            }
        });
    }

    // Whether two texts are the same word but for their case. core's
    // equal_fold compares one code point to one, which is enough for most
    // texts and wrong for ß against SS
    inline bool equal_fold_full(const string& a, const string& b) {
        return a == b || fold_case(a) == fold_case(b);
    }
}
