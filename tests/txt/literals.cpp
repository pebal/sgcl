//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A literal where a function takes both a string and a text slice. Each
// converts from a literal by a conversion of its own, so the call was
// ambiguous and did not compile; the overloads of a C text take it — an
// array of char up to its first NUL or its end, a char* or const char* up
// to its NUL (the slice of the literal's array would count the zero).
// Every call here is one that did not compile before them, and each answer
// is checked against the same call over a string; an array filled to the
// brim is read to its end and not past it.
#include "sgcl/txt/txt.h"
#include "tests/types.h"

TEST(TxtLiteral_Tests, RegexTakesALiteral) {
    auto bad = txt::regex::compile("(a+)+\\1");
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().offset(), txt::regex::compile(string("(a+)+\\1")).error().offset());

    auto re = txt::regex::compile("(\\d+)-(\\d+)");
    ASSERT_TRUE(re.has_value());
    EXPECT_EQ(re->pattern(), string("(\\d+)-(\\d+)"));

    EXPECT_TRUE(re->full_match("12-34"));               // no zero after the text
    EXPECT_FALSE(re->full_match("12-34x"));
    EXPECT_TRUE(re->contains("a 1-2 b"));
    EXPECT_FALSE(re->contains("a b"));

    auto m = re->find("x 12-34 y");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->begin_at(), 2u);
    EXPECT_EQ(m->end_at(), 7u);
    EXPECT_EQ((*m)[2], "34");
    auto later = re->find("1-2 3-4", 1);
    ASSERT_TRUE(later.has_value());
    EXPECT_EQ(later->begin_at(), 4u);

    EXPECT_EQ(re->all("1-2 3-4 5-6").count(), 3u);
    EXPECT_EQ(txt::regex_matches(*re, "1-2 3-4").count(), 2u);
    EXPECT_EQ(re->count("1-2 3-4"), 2u);
    EXPECT_EQ(re->replace("1-2 3-4", "$2-$1"), string("2-1 4-3"));
    EXPECT_EQ(re->replace_first("1-2 3-4", "$2-$1"), string("2-1 3-4"));

    auto comma = txt::regex::compile(",");
    ASSERT_TRUE(comma.has_value());
    auto pieces = comma->split("a,b,c");
    ASSERT_EQ(pieces.size(), 3u);
    EXPECT_EQ(pieces[2], "c");                          // not "c" and its zero
    EXPECT_EQ(comma->split("a,b,c", 2).size(), 2u);
}

TEST(TxtLiteral_Tests, AMatchOfACStringHoldsACopy) {
    // what keeps pieces of the text copies a C string into a string it
    // holds: the buffer the text came from may change or go
    auto re = txt::regex::compile("b+");
    ASSERT_TRUE(re.has_value());
    char buffer[] = "abbc";
    auto m = re->find(buffer);
    auto pieces = re->split(buffer);
    auto lines = txt::wrap(buffer, 10);
    buffer[1] = 'x';
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->text(), "bb");
    ASSERT_EQ(pieces.size(), 2u);
    EXPECT_EQ(pieces[0], "a");
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "abbc");
}

TEST(TxtLiteral_Tests, SearcherTakesALiteral) {
    txt::searcher needle(string("lo"));
    EXPECT_EQ(needle.find("hello, lo"), 3u);
    EXPECT_EQ(needle.find("hello, lo", 4), 7u);
    EXPECT_TRUE(needle.contains("hello"));
    EXPECT_FALSE(needle.contains("help"));
}

TEST(TxtLiteral_Tests, FoldedAndNormalizedTextTakeALiteral) {
    txt::folded_text folded("Straße, STRASSE");
    EXPECT_EQ(folded.text(), "Straße, STRASSE");         // not the zero after it
    EXPECT_EQ(folded.count(string("strasse")), 2u);
    EXPECT_EQ(folded.find(string("STRASSE"), 1)->pos, 9u);

    txt::normalized_text normalized("café café");
    EXPECT_EQ(normalized.count(string("café")), 2u);

    txt::fold_searcher pattern(string("strasse"));
    std::vector<size_t> at;
    for (auto m : txt::fold_matches("Straße, STRASSE", pattern)) {
        at.push_back(size_t(m.size()));
    }
    ASSERT_EQ(at.size(), 2u);
    EXPECT_EQ(at[0], 7u);                               // "Straße" is seven bytes
    EXPECT_EQ(at[1], 7u);
    EXPECT_EQ(txt::normalized_matches("café café", txt::normalized_searcher(string("café"))).count(), 2u);

    // a buffer that changes after: the text holds a copy
    char buffer[] = "abc ABC";
    txt::folded_text copy(buffer);
    buffer[0] = 'x';
    EXPECT_EQ(copy.count(string("abc")), 2u);
}

TEST(TxtLiteral_Tests, SegmentsTakeALiteral) {
    EXPECT_EQ(txt::graphemes("éa").count(), 2u);
    EXPECT_EQ(txt::words("two words.").count(), 2u);
    EXPECT_EQ(txt::word_breaks("two words").count(), 3u);
    EXPECT_EQ(txt::sentences("One. Two.").count(), 2u);
    EXPECT_EQ(txt::line_breaks("one two").count(), 2u);

    auto lines = txt::wrap("one two three", 7);
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "one two");
    EXPECT_EQ(lines[1], "three");

    EXPECT_EQ(txt::grapheme_count("ab"), 2u);           // not three with the zero
    EXPECT_EQ(txt::grapheme_count("é"), 1u);
    EXPECT_EQ(txt::grapheme_start("aé", 2), 1u);
    EXPECT_EQ(txt::grapheme_next("aé", 1), 4u);
    EXPECT_EQ(txt::grapheme_prev("aé", 4), 1u);
    EXPECT_EQ(txt::grapheme_count(static_cast<const char*>(nullptr)), 0u);
}

TEST(TxtLiteral_Tests, BidiRunsTakeALiteral) {
    auto runs = txt::bidi_runs("abc אב");
    EXPECT_EQ(runs.count(), txt::bidi_runs(string("abc אב")).count());
    EXPECT_EQ(runs.paragraph(), txt::direction::left_to_right);
    EXPECT_EQ(txt::bidi_runs("אב", txt::direction::automatic).paragraph(), txt::direction::right_to_left);
}

TEST(TxtLiteral_Tests, CollatedTextTakesALiteral) {
    txt::collator search{txt::collator::options{.strength = txt::strength::primary}};
    txt::collated_text weighed(search, "Résumé, resume");
    EXPECT_EQ(weighed.text().size(), string("Résumé, resume").size());
    EXPECT_EQ(weighed.count(string("RESUME")), 2u);

    auto pattern = weighed.searcher(string("resume"));
    EXPECT_EQ(txt::collated_matches(search, "Résumé, resume", pattern).count(), 2u);
}

namespace {
    // Whether grapheme_count takes an argument of type A: nullptr has to
    // be refused where a pointer and an array are taken
    template<class A>
    concept TakesText = requires(A a) { txt::grapheme_count(a); };

    static_assert(TakesText<const char*> && TakesText<char*> && TakesText<const char (&)[3]> && TakesText<char (&)[3]>);
    static_assert(!TakesText<std::nullptr_t>);

    // An array filled to the brim, with no NUL of its own, and characters
    // right after it: whoever reads past its end reads them too
    struct Brim {
        char text[4];
        char after[4];
    };
}

TEST(TxtLiteral_Tests, AnArrayIsReadToItsEndAndNoFurther) {
    Brim b = {{'a', 'b', 'c', 'd'}, {'x', 'y', 'z', '\0'}};

    EXPECT_EQ(txt::grapheme_count(b.text), 4u);         // not 7, with "xyz"
    EXPECT_EQ(txt::grapheme_start(b.text, 9), 4u);
    EXPECT_EQ(txt::graphemes(b.text).count(), 4u);
    EXPECT_EQ(txt::line_breaks(b.text).text(), "abcd");
    auto lines = txt::wrap(b.text, 10);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0], "abcd");
    EXPECT_EQ(txt::bidi_runs(b.text).text(), "abcd");
    EXPECT_EQ(txt::folded_text(b.text).text(), "abcd");

    auto re = txt::regex::compile(b.text);
    ASSERT_TRUE(re.has_value());
    EXPECT_EQ(re->pattern(), string("abcd"));
    EXPECT_TRUE(re->full_match(b.text));
    EXPECT_EQ(re->count(b.text), 1u);
    EXPECT_EQ(re->all(b.text).text(), "abcd");
    EXPECT_EQ(re->find(b.text)->subject(), "abcd");
    EXPECT_EQ(re->replace(b.text, b.after), string("xyz"));
    EXPECT_EQ(txt::searcher(string("d")).find(b.text), 3u);
    EXPECT_FALSE(txt::searcher(string("x")).contains(b.text));

    // an array with a NUL inside stops at it, as a literal does
    char early[8] = {'a', 'b', '\0', 'c', 'd', 'e', 'f', 'g'};
    EXPECT_EQ(txt::grapheme_count(early), 2u);
}

TEST(TxtLiteral_Tests, APointerIsReadToItsNul) {
    Brim b = {{'a', 'b', 'c', 'd'}, {'x', 'y', 'z', '\0'}};
    const char* p = b.after;
    char* q = b.after;
    EXPECT_EQ(txt::grapheme_count(p), 3u);
    EXPECT_EQ(txt::grapheme_count(q), 3u);
    EXPECT_EQ(txt::grapheme_count(static_cast<const char*>(nullptr)), 0u);

    auto re = txt::regex::compile("y");
    ASSERT_TRUE(re.has_value());
    EXPECT_EQ(re->replace(p, q), string("xxyzz"));      // two pointers
    EXPECT_EQ(re->replace(p, "Y"), string("xYz"));     // a pointer and a literal: through string
    EXPECT_EQ(re->replace_first("yy", q), string("xyzy"));
    EXPECT_EQ(re->find(q)->begin_at(), 1u);
    EXPECT_EQ(txt::folded_text(p).text(), "xyz");
}

TEST(TxtLiteral_Tests, ReplaceTakesTwoArrays) {
    auto re = txt::regex::compile("b+");
    ASSERT_TRUE(re.has_value());
    EXPECT_EQ(re->replace("abba cb", "-"), string("a-a c-"));     // two literals of two sizes
    EXPECT_EQ(re->replace_first("abba cb", "<$0>"), string("a<bb>a cb"));

    // arrays filled to the brim, the text and the replacement both
    Brim b = {{'a', 'b', 'b', 'c'}, {'x', 'y', 'z', '\0'}};
    Brim with = {{'1', '2', '3', '4'}, {'5', '6', '7', '\0'}};
    EXPECT_EQ(re->replace(b.text, with.text), string("a1234c"));   // not "a1234567c"
    EXPECT_EQ(re->replace_first(b.text, with.text), string("a1234c"));
}

TEST(TxtLiteral_Tests, GroupValueAndColumnsReadAnArrayToItsEnd) {
    auto re = txt::regex::compile("(?<abcd>b)");
    ASSERT_TRUE(re.has_value());
    auto m = re->find("abc");
    ASSERT_TRUE(m.has_value());
    Brim name = {{'a', 'b', 'c', 'd'}, {'x', 'y', 'z', '\0'}};
    ASSERT_TRUE(m->group(name.text).has_value());          // "abcd", not "abcdxyz"
    EXPECT_EQ(*m->group(name.text), "b");
    EXPECT_TRUE(m->group("abcd").has_value());
    const char* p = "abcd";
    EXPECT_TRUE(m->group(p).has_value());

    txt::value v(name.text);
    ASSERT_NE(v.text(), nullptr);
    EXPECT_EQ(*v.text(), string("abcd"));
    EXPECT_EQ(*txt::value("ab").text(), string("ab"));
    EXPECT_EQ(*txt::value(p).text(), string("abcd"));

    EXPECT_EQ(txt::columns(name.text), 4u);
    EXPECT_EQ(txt::columns("ab"), 2u);
    EXPECT_EQ(txt::columns(p), 4u);
    static_assert(txt::columns("abc") == 3);                // still constexpr
}

TEST(TxtLiteral_Tests, AMixedReplaceReadsTheArrayToItsEnd) {
    // an array beside a pointer goes through string, whose array
    // constructor stops at the array's end as well
    auto re = txt::regex::compile("b+");
    ASSERT_TRUE(re.has_value());
    Brim b = {{'a', 'b', 'b', 'c'}, {'x', 'y', 'z', '\0'}};
    const char* with = "-";
    EXPECT_EQ(re->replace(b.text, with), string("a-c"));        // not "a-cxyz"
    EXPECT_EQ(re->replace_first(b.text, with), string("a-c"));
    const char* text = "abbc";
    Brim to = {{'1', '2', '3', '4'}, {'5', '6', '7', '\0'}};
    EXPECT_EQ(re->replace(text, to.text), string("a1234c"));      // not "a1234567c"
}
