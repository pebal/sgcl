//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::properties: the general category and the predicates over it, the
// value of a digit, the script, and the columns a code point and a text
// take on a terminal. The tables themselves are checked point by point
// against Python's unicodedata by the generator; what is checked here is
// the interface over them, the ASCII paths that answer without a table,
// and the agreement of the two.
#include "tests/types.h"

#include <string>

namespace {
    // The names take a code point and refuse everything else: a call is
    // ill-formed, which a concept sees where a static_assert on a
    // requires-expression would not (the call would be a hard error)
    template<class T> concept Alphas = requires(T c) { txt::is_alpha(c); };
    template<class T> concept Digits = requires(T c) { txt::is_digit(c); };
    template<class T> concept Categorizes = requires(T c) { txt::category_of(c); };
    template<class T> concept Scripts = requires(T c) { txt::script_of(c); };
    template<class T> concept Values = requires(T c) { txt::numeric_value_of(c); };
    template<class T> concept Columns = requires(T c) { txt::columns(c); };
}

TEST(Properties_Tests, TheCategoryOfACodePoint) {
    using enum txt::category;
    static_assert(txt::category_of(U'A') == uppercase_letter && txt::category_of(U'a') == lowercase_letter);
    static_assert(txt::category_of(U'ǅ') == titlecase_letter && txt::category_of(U'ʰ') == modifier_letter);
    static_assert(txt::category_of(U'漢') == other_letter && txt::category_of(U'Ł') == uppercase_letter);
    static_assert(txt::category_of(U'\u0301') == nonspacing_mark && txt::category_of(U'\u0903') == spacing_mark);
    static_assert(txt::category_of(U'\u20DD') == enclosing_mark);                  // a combining enclosing circle
    static_assert(txt::category_of(U'7') == decimal_number && txt::category_of(U'Ⅻ') == letter_number);
    static_assert(txt::category_of(U'½') == other_number && txt::category_of(U'_') == connector_punctuation);
    static_assert(txt::category_of(U'-') == dash_punctuation && txt::category_of(U'(') == open_punctuation);
    static_assert(txt::category_of(U')') == close_punctuation && txt::category_of(U'«') == initial_punctuation);
    static_assert(txt::category_of(U'»') == final_punctuation && txt::category_of(U'!') == other_punctuation);
    static_assert(txt::category_of(U'+') == math_symbol && txt::category_of(U'$') == currency_symbol);
    static_assert(txt::category_of(U'^') == modifier_symbol && txt::category_of(U'😀') == other_symbol);
    static_assert(txt::category_of(U' ') == space_separator && txt::category_of(U'\u2028') == line_separator);
    static_assert(txt::category_of(U'\u2029') == paragraph_separator && txt::category_of(U'\n') == control);
    static_assert(txt::category_of(U'\u200D') == format && txt::category_of(U'\uE000') == private_use);
    static_assert(txt::category_of(char32_t(0xD800)) == surrogate);
    static_assert(txt::category_of(char32_t(0x0378)) == unassigned);               // a hole in the Greek block
    static_assert(txt::category_of(char32_t(0x110000)) == unassigned);             // past the last code point

    // The predicates agree with the category over the whole space
    for (char32_t c = 0; c < 0x110000; ++c) {
        auto k = txt::category_of(c);
        ASSERT_EQ(txt::is_alpha(c), k >= uppercase_letter && k <= other_letter) << std::hex << uint32_t(c);
        ASSERT_EQ(txt::is_digit(c), k == decimal_number) << std::hex << uint32_t(c);
        ASSERT_EQ(txt::is_alnum(c), txt::is_alpha(c) || txt::is_digit(c));
        ASSERT_EQ(txt::is_mark(c), k >= nonspacing_mark && k <= enclosing_mark) << std::hex << uint32_t(c);
        ASSERT_EQ(txt::is_punct(c), k >= connector_punctuation && k <= other_punctuation) << std::hex << uint32_t(c);
        ASSERT_EQ(txt::is_format(c), k == format) << std::hex << uint32_t(c);
        ASSERT_EQ(txt::is_control(c), k == control) << std::hex << uint32_t(c);
        ASSERT_EQ(txt::is_printable(c), c == U' ' || (k >= uppercase_letter && k <= other_symbol)) << std::hex << uint32_t(c);
    }
}

TEST(Properties_Tests, TheDigitsAndTheEmoji) {
    static_assert(txt::is_digit(U'7') && txt::numeric_value_of(U'7') == 7);
    static_assert(txt::numeric_value_of(U'٣') == 3);          // Arabic-Indic three
    static_assert(txt::numeric_value_of(U'१') == 1);          // Devanagari one
    static_assert(txt::numeric_value_of(U'９') == 9);          // fullwidth nine
    static_assert(txt::numeric_value_of(U'x') == -1 && txt::numeric_value_of(U'Ⅻ') == -1);   // a Roman numeral is no decimal digit
    static_assert(txt::numeric_value_of(U'½') == -1);

    // Every decimal digit has a value in [0, 9] and every value belongs to
    // a run of ten; nothing else has one
    size_t digits = 0;
    for (char32_t c = 0; c < 0x110000; ++c) {
        int v = txt::numeric_value_of(c);
        ASSERT_EQ(v >= 0, txt::is_digit(c)) << std::hex << uint32_t(c);
        if (v >= 0) {
            ASSERT_LE(v, 9);
            ASSERT_EQ(txt::numeric_value_of(char32_t(c - v)), 0) << std::hex << uint32_t(c);   // the zero of its block
            ++digits;
        }
    }
    EXPECT_EQ(digits % 10, 0u);
    EXPECT_GT(digits, 600u);

    // Extended_Pictographic, not the Emoji property: '1' and '#' are emoji
    // by that one, and a reader would not call them so
    static_assert(txt::is_emoji(U'😀') && txt::is_emoji(U'❤') && txt::is_emoji(U'\U0001F6E0'));
    static_assert(!txt::is_emoji(U'1') && !txt::is_emoji(U'#') && !txt::is_emoji(U'*') && !txt::is_emoji(U'a'));
    static_assert(!txt::is_emoji(U'\u200D'));                   // the joiner itself is not one
}

TEST(Properties_Tests, TheScriptOfACodePoint) {
    using enum txt::script;
    static_assert(txt::script_of(U'a') == latin && txt::script_of(U'Ł') == latin);
    static_assert(txt::script_of(U'Я') == cyrillic && txt::script_of(U'Σ') == greek);
    static_assert(txt::script_of(U'漢') == han && txt::script_of(U'あ') == hiragana);
    static_assert(txt::script_of(U'ア') == katakana && txt::script_of(U'한') == hangul);
    static_assert(txt::script_of(U'ع') == arabic && txt::script_of(U'ש') == hebrew);
    static_assert(txt::script_of(U'क') == devanagari && txt::script_of(U'ก') == thai);
    static_assert(txt::script_of(U'1') == common && txt::script_of(U' ') == common);
    static_assert(txt::script_of(U'\u0301') == inherited);      // a combining acute takes the script it sits on
    static_assert(txt::script_of(char32_t(0x0378)) == unknown && txt::script_of(char32_t(0x110000)) == unknown);

    // A text in one script, the joiners and the punctuation aside
    string s = "Łódź nad Wisłą";
    EXPECT_TRUE(s.runes().all([](char32_t c) { return txt::script_of(c) == latin || txt::script_of(c) == common; }));
    EXPECT_FALSE(s.runes().exists([](char32_t c) { return txt::script_of(c) == cyrillic; }));
}

TEST(Properties_Tests, TheColumnsOfACodePointAndOfAText) {
    static_assert(txt::columns(U'a') == 1 && txt::columns(U'ż') == 1 && txt::columns(U'\u00A0') == 1);
    static_assert(txt::columns(U'漢') == 2 && txt::columns(U'ア') == 2 && txt::columns(U'한') == 2);
    static_assert(txt::columns(U'😀') == 2 && txt::columns(U'Ａ') == 2);       // an emoji and a fullwidth A
    static_assert(txt::columns(U'\u0301') == 0 && txt::columns(U'\u200D') == 0);   // a combining acute, the joiner
    static_assert(txt::columns(U'\u0903') == 1);                                    // a spacing mark takes its cell
    static_assert(txt::columns(U'±') == 1);                                    // class A: one outside a CJK locale
    static_assert(txt::columns(U'\n') == 0 && txt::columns(U'\u0007') == 0);

    // A whole text: a string, a slice of one, a std view
    string s = "漢字 ab";
    EXPECT_EQ(txt::columns(s), 7u);                             // 2 + 2 + 1 + 1 + 1
    EXPECT_EQ(txt::columns(s.as_slice()), 7u);
    EXPECT_EQ(txt::columns("漢字 ab"), 7u);                      // a literal, without its zero
    EXPECT_EQ(txt::columns(string("")), 0u);
    EXPECT_EQ(txt::columns(string("że\u0301")), 2u);            // e plus a combining acute is one column
    EXPECT_EQ(txt::columns(string("a\xFF" "b")), 3u);           // an invalid byte is one replacement, one column

    // The sum over the runes is what the text answers
    string mixed = "a漢\u0301ż😀 ";
    size_t sum = 0;
    for (char32_t c : mixed.runes()) {
        sum += txt::columns(c);
    }
    EXPECT_EQ(txt::columns(mixed), sum);
    EXPECT_EQ(sum, 7u);                                          // 1 + 2 + 0 + 1 + 2 + 1
}

TEST(Properties_Tests, ACodePointAndNothingElse) {
    // A char is a byte of UTF-8, an int is a multi-character literal
    static_assert(Alphas<char32_t> && !Alphas<char> && !Alphas<int> && !Alphas<char8_t> && !Alphas<wchar_t>);
    static_assert(Digits<char32_t> && !Digits<char> && !Digits<int>);
    static_assert(Categorizes<char32_t> && !Categorizes<char> && !Categorizes<int>);
    static_assert(Scripts<char32_t> && !Scripts<char> && !Scripts<int>);
    static_assert(Values<char32_t> && !Values<char> && !Values<int>);
    static_assert(Columns<char32_t> && !Columns<char> && !Columns<int>);

    // columns takes a text as well, which the predicates do not
    static_assert(Columns<string> && Columns<slice<const char>> && Columns<const char*>);
    static_assert(!Columns<std::string_view>);                   // refused, not left ambiguous
    static_assert(!Alphas<string> && !Alphas<std::string_view>);

    // And every predicate is passable by name
    string s = "ŁÓDŹ 7 😀\u3000";
    EXPECT_EQ(s.runes().count_of(txt::is_alpha), 4u);
    EXPECT_EQ(s.runes().count_of(txt::is_digit), 1u);
    EXPECT_EQ(s.runes().count_of(txt::is_emoji), 1u);
    EXPECT_TRUE(s.runes().exists(txt::is_printable));
    EXPECT_EQ(s.runes().find_index(txt::is_digit), 5u);
    EXPECT_EQ(s.runes().count_of(unicode::is_space), 3u);        // core's, over the same range: two spaces and the ideographic one

    EXPECT_STREQ(txt::version, unicode::version);
}

// The core answers under the module's names, beside is_alpha
TEST(Properties_Tests, TheCoreAnswersUnderTheModulesNames) {
    EXPECT_TRUE(txt::is_space(U'　'));
    EXPECT_FALSE(txt::is_space(U'a'));
    EXPECT_TRUE(txt::is_upper(U'Ł'));
    EXPECT_TRUE(txt::is_lower(U'ł'));
    EXPECT_EQ(txt::is_space(U'\u0085'), unicode::is_space(U'\u0085'));
    EXPECT_EQ(sgcl::string("Ab c").runes().count_of(txt::is_upper), 1u);
}
