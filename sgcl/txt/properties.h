//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/slice.h"
#include "../core/string.h"
#include "../core/unicode.h"
#include "detail/property_enums.h"
#include "detail/property_tables.h"

// The properties of a code point: what it is (its general category and the
// predicates over it), what it is worth (a decimal digit), what it is
// written in (its script) and how wide it is on a terminal. The names are
// constexpr objects, not functions (core's detail::code_point_fn): each
// takes a char32_t and refuses everything else — a char is a byte of
// UTF-8, an int is a multi-character literal — and each is passable where
// a predicate is asked for, so that s.runes().count_of(txt::is_alpha)
// works.
namespace sgcl::txt {
    using detail::category;
    using detail::script;

    namespace detail {
        constexpr category category_of_fn(char32_t c) noexcept {
            return category(value_of(c, property_tables::Category));
        }

        constexpr bool is_category(char32_t c, category lo, category hi) noexcept {
            auto v = category_of_fn(c);
            return v >= lo && v <= hi;
        }

        // The letters: Lu, Ll, Lt, Lm, Lo — Go's unicode.IsLetter. Not the
        // Alphabetic property, which also holds Nl and the marks that
        // spell a vowel, and would make is_alpha true of a combining sign
        constexpr bool is_alpha_fn(char32_t c) noexcept {
            if (c < 0x80) {
                return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
            }
            return is_category(c, category::uppercase_letter, category::other_letter);
        }

        constexpr bool is_digit_fn(char32_t c) noexcept {
            if (c < 0x80) {
                return c >= U'0' && c <= U'9';
            }
            return category_of_fn(c) == category::decimal_number;
        }

        constexpr bool is_alnum_fn(char32_t c) noexcept {
            return is_alpha_fn(c) || is_digit_fn(c);
        }

        constexpr bool is_punct_fn(char32_t c) noexcept {
            return is_category(c, category::connector_punctuation, category::other_punctuation);
        }

        constexpr bool is_mark_fn(char32_t c) noexcept {
            return is_category(c, category::nonspacing_mark, category::enclosing_mark);
        }

        constexpr bool is_control_fn(char32_t c) noexcept {
            return c < 0x20 || (c >= 0x7F && c <= 0x9F);
        }

        constexpr bool is_format_fn(char32_t c) noexcept {
            return category_of_fn(c) == category::format;
        }

        // A letter, a mark, a number, a punctuation mark or a symbol, and
        // the space: what Go's unicode.IsPrint holds, so that a printable
        // code point is one a terminal can show without a surprise. The
        // other separators, the controls and the unassigned are not
        constexpr bool is_printable_fn(char32_t c) noexcept {
            if (c == U' ') {
                return true;
            }
            return is_category(c, category::uppercase_letter, category::other_symbol);
        }

        constexpr bool is_emoji_fn(char32_t c) noexcept {
            return c >= 0xA9 && in_set(c, property_tables::ExtendedPictographic);
        }

        constexpr int numeric_value_fn(char32_t c) noexcept {
            if (c < 0x80) {
                return c >= U'0' && c <= U'9' ? int(c - U'0') : -1;
            }
            auto r = find(c, property_tables::DecimalDigit);
            return r.ok ? int(r.value + (c - r.lo)) : -1;
        }

        constexpr script script_of_fn(char32_t c) noexcept {
            return script(value_of(c, property_tables::Script));
        }

        // The columns a code point takes on a terminal: East_Asian_Width W
        // and F are two, a combining or formatting code point is none, and
        // the rest is one. Class A (ambiguous: the Greek and Cyrillic
        // letters of the East Asian fonts) is one, as it is everywhere
        // outside a CJK locale
        constexpr size_t columns_fn(char32_t c) noexcept {
            if (c < 0x300) {
                return c < 0x20 || (c >= 0x7F && c <= 0x9F) ? 0 : 1;
            }
            if (is_mark_fn(c) && category_of_fn(c) != category::spacing_mark) {
                return 0;
            }
            if (is_format_fn(c)) {
                return 0;
            }
            return in_set(c, property_tables::Wide) ? 2 : 1;
        }

        // The columns a whole text takes. A run of ASCII takes a column a
        // character but for the controls, which take none: one comparison
        // a byte and no branch, where the walk below decodes a code point
        // and asks two tables about it
        constexpr size_t columns_of_text(std::string_view text) noexcept {
            size_t n = 0;
            for (size_t i = 0; i < text.size();) {
                size_t run = utf8::ascii_run(text, i);
                for (size_t k = 0; k < run; ++k) {
                    n += uint8_t(uint8_t(text[i + k]) - 0x20) < 0x5F ? 1 : 0;
                }
                i += run;
                if (i >= text.size()) {
                    break;
                }
                auto [c, width] = utf8::decode(text, i);
                n += columns_fn(c);
                i += width;
            }
            return n;
        }

        // The bytes of the longest prefix of the text that takes at most
        // `limit` columns, ending on a code point and never inside one: a
        // field cut by its bytes leaves a lead byte with nothing after it,
        // which is not text any more
        constexpr size_t columns_prefix(std::string_view text, size_t limit) noexcept {
            size_t bytes = 0;
            size_t taken = 0;
            while (bytes < text.size()) {
                auto [c, width] = utf8::decode(text, bytes);
                size_t w = columns_fn(c);
                if (taken + w > limit) {
                    break;
                }
                bytes += width;
                taken += w;
            }
            return bytes;
        }

        // columns of a text as well as of a code point: the sum over the
        // runes, which a code_point_fn could not carry, since it refuses
        // everything that is not a char32_t
        struct columns_of {
            constexpr size_t operator()(char32_t c) const noexcept {
                return columns_fn(c);
            }

            // Not constexpr, these two: a slice and a string hold a
            // tracked pointer, so neither is a literal type
            size_t operator()(const slice<const char>& text) const noexcept {
                return _sum({text.data(), text.size()});
            }

            size_t operator()(const string& text) const noexcept {
                return _sum(text.view());
            }

            // A C string, as core's text interface takes one; a literal
            // comes here rather than through the slice of its array, and
            // does not count its terminating zero
            constexpr size_t operator()(const char* text) const noexcept {
                return text ? _sum(text) : 0;
            }

            // A std view is refused rather than left ambiguous (a string
            // and a slice both take one): the module's texts are the
            // library's, which hold what they point at
            size_t operator()(std::string_view) const = delete;

            // A char is a byte of UTF-8 and an int is a multi-character
            // literal: neither is a code point, and neither is a text
            template<class T> requires (!std::same_as<std::remove_cvref_t<T>, char32_t>
                                        && !std::convertible_to<T, slice<const char>>
                                        && !std::convertible_to<T, const string&>
                                        && !std::convertible_to<T, const char*>)
            size_t operator()(T) const = delete;

        private:
            static constexpr size_t _sum(std::string_view text) noexcept {
                return columns_of_text(text);
            }
        };
    }

    // The general category of a code point (category::unassigned when it
    // has none) and the script it is written in (script::unknown)
    inline constexpr sgcl::detail::code_point_fn<detail::category_of_fn> category_of {};
    inline constexpr sgcl::detail::code_point_fn<detail::script_of_fn> script_of {};

    // What a code point is: a letter, a decimal digit, either of them, a
    // punctuation mark, a mark of the categories Mn, Mc and Me, a control
    // (C0 and C1), a formatting code point, something a terminal can show
    // (Go's IsPrint), an emoji (Extended_Pictographic, not the Emoji
    // property, whose members are also '#', '*' and the ten digits)
    inline constexpr sgcl::detail::code_point_fn<detail::is_alpha_fn> is_alpha {};
    inline constexpr sgcl::detail::code_point_fn<detail::is_digit_fn> is_digit {};
    inline constexpr sgcl::detail::code_point_fn<detail::is_alnum_fn> is_alnum {};
    inline constexpr sgcl::detail::code_point_fn<detail::is_punct_fn> is_punct {};
    inline constexpr sgcl::detail::code_point_fn<detail::is_mark_fn> is_mark {};
    inline constexpr sgcl::detail::code_point_fn<detail::is_control_fn> is_control {};
    inline constexpr sgcl::detail::code_point_fn<detail::is_format_fn> is_format {};
    inline constexpr sgcl::detail::code_point_fn<detail::is_printable_fn> is_printable {};
    inline constexpr sgcl::detail::code_point_fn<detail::is_emoji_fn> is_emoji {};

    // The three core answers (core/unicode.h) under this module's names,
    // so that txt::is_space stands beside txt::is_alpha: the White_Space
    // property, and whether a code point has a case form other than
    // itself
    inline constexpr auto is_space = unicode::is_space;
    inline constexpr auto is_upper = unicode::is_upper;
    inline constexpr auto is_lower = unicode::is_lower;

    // The value of a decimal digit, -1 when the code point is not one:
    // numeric_value_of(U'٣') is 3 (the Arabic-Indic three)
    inline constexpr sgcl::detail::code_point_fn<detail::numeric_value_fn> numeric_value_of {};

    // The columns of a code point or of a whole text on a terminal, for a
    // table of columns and for wrap() and truncate() of the segmentation.
    // Monospaced cells, not the width of a glyph: a proportional font is
    // measured by the one that draws it
    inline constexpr detail::columns_of columns {};

    // The Unicode version of the tables, the one core's unicode answers
    inline constexpr const char* version = unicode::version;
}
