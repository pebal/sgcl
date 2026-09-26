//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::regex. Three oracles and one clock.
//
// Python's re answers the questions the two engines are defined to answer
// the same way, over 3000 random patterns for a first match, 800 for every
// match and 600 for a split (regex_tests.h, generated). What re cannot be
// asked — a backreference, a lookaround — this engine refuses, and those
// refusals are checked by name and by message. What the two would answer
// differently by design is translated in the generator and written down
// there rather than compared blind.
//
// The clock is the point of the whole header: a pattern a backtracking
// engine takes longer than a lifetime over has to come back here in
// milliseconds, and the test says so in milliseconds rather than trusting
// the argument about why it should.
#include "sgcl/txt/regex.h"
#include "tests/types.h"
#include "regex_tests.h"

#include <chrono>
#include <random>
#include <string>

namespace {
    string of(std::string_view s) {
        return string(s.data(), s.size());
    }

    txt::regex compiled(std::string_view pattern) {
        auto re = txt::regex::compile(of(pattern));
        EXPECT_TRUE(re.has_value()) << pattern
            << (re ? std::string() : std::string(re.error().message().view()));
        return re ? *re : *txt::regex::compile(of("(?:)"));
    }

    // The spans of a match, in the shape the oracle writes them:
    // begin:end for the whole of it and then one to a group, 'x' where a
    // group took no part
    std::string spans(const txt::match& m) {
        std::string out = std::to_string(m.begin_at()) + ":" + std::to_string(m.end_at());
        for (size_t i = 1; i <= m.group_count(); ++i) {
            auto g = m.group(i);
            if (!g) {
                out += ";x";
            } else {
                size_t at = size_t(g->data() - m.subject().data());
                out += ";" + std::to_string(at) + ":" + std::to_string(at + g->size());
            }
        }
        return out;
    }

    std::string piece_of(slice<const char> s) {
        return std::string(s.data(), s.size());
    }

    // The flags of a case, written the way a pattern carries them
    string with_flags(const char* flags, const char* pattern) {
        std::string p;
        if (*flags) {
            p = std::string("(?") + flags + ")";
        }
        p += pattern;
        return of(p);
    }

    double milliseconds(auto&& f) {
        auto start = std::chrono::steady_clock::now();
        f();
        auto stop = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(stop - start).count();
    }
}

//------------------------------------------------------------------------------
TEST(Regex_Tests, WhatTheSyntaxMeans) {
    auto re = compiled("a+b");
    EXPECT_TRUE(re.contains(of("xxaaab")));
    EXPECT_FALSE(re.contains(of("xxb")));
    EXPECT_TRUE(re.full_match(of("aab")));
    EXPECT_FALSE(re.full_match(of("aabc")));

    auto m = compiled("(\\d+)-(\\w+)").find(of("ab 123-kot cd"));
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->begin_at(), 3u);
    EXPECT_EQ(m->end_at(), 10u);
    EXPECT_EQ(piece_of(m->text()), "123-kot");
    EXPECT_EQ(piece_of(*m->group(1)), "123");
    EXPECT_EQ(piece_of(*m->group(2)), "kot");
    EXPECT_EQ(m->group_count(), 2u);

    // A group that took no part is not an empty one
    auto either = compiled("(a)|(b)").find(of("b"));
    ASSERT_TRUE(either.has_value());
    EXPECT_FALSE(either->group(1).has_value());
    ASSERT_TRUE(either->group(2).has_value());
    EXPECT_EQ(piece_of(*either->group(2)), "b");
    auto maybe = compiled("(a?)b").find(of("b"));
    ASSERT_TRUE(maybe->group(1).has_value());
    EXPECT_TRUE(maybe->group(1)->empty());

    // Greedy against lazy, which is the whole of the priority rule
    EXPECT_EQ(piece_of(compiled("<.*>").find(of("<a><b>"))->text()), "<a><b>");
    EXPECT_EQ(piece_of(compiled("<.*?>").find(of("<a><b>"))->text()), "<a>");

    // Counted repetition, spelled out where the program is built
    EXPECT_TRUE(compiled("a{3}").full_match(of("aaa")));
    EXPECT_FALSE(compiled("a{3}").full_match(of("aa")));
    EXPECT_TRUE(compiled("a{2,4}").full_match(of("aaa")));
    EXPECT_FALSE(compiled("a{2,4}").full_match(of("aaaaa")));
    EXPECT_TRUE(compiled("a{2,}").full_match(of("aaaaa")));
    // a brace that is not a count is a character, as it is in RE2
    EXPECT_TRUE(compiled("a{b}").full_match(of("a{b}")));

    // The anchors
    EXPECT_TRUE(compiled("^ab$").full_match(of("ab")));
    EXPECT_FALSE(compiled("^b").contains(of("ab")));
    EXPECT_TRUE(compiled("\\Aab\\z").full_match(of("ab")));
    // '$' here is the end of the text, not the place before a last
    // newline, which is where this parts company with Python's re
    EXPECT_FALSE(compiled("b$").contains(of("ab\n")));
    EXPECT_TRUE(compiled("(?m)b$").contains(of("ab\ncd")));

    // The classes
    EXPECT_TRUE(compiled("[a-c]+").full_match(of("abcba")));
    EXPECT_FALSE(compiled("[a-c]+").full_match(of("abd")));
    EXPECT_TRUE(compiled("[^a-c]+").full_match(of("xyz")));
    EXPECT_FALSE(txt::regex::compile(of("[]]")).has_value());   // ']' must be escaped
    EXPECT_TRUE(compiled("[\\]]").full_match(of("]")));
    EXPECT_TRUE(compiled("[-a]+").full_match(of("-a")));
    EXPECT_TRUE(compiled("[a-]+").full_match(of("-a")));
}

TEST(Regex_Tests, TheWholeTextIsNotTheFirstMatch) {
    // find() stops where a backtracking engine would have stopped, which
    // is the shorter match; matches() asks whether the longer one exists
    auto re = compiled("a|ab");
    EXPECT_EQ(piece_of(re.find(of("ab"))->text()), "a");
    EXPECT_TRUE(re.full_match(of("ab")));
}

TEST(Regex_Tests, TheBoundariesAndTheFlags) {
    EXPECT_EQ(compiled("\\bcat\\b").count(of("cat cats concat cat.")), 2u);
    EXPECT_EQ(compiled("\\Bcat").count(of("cat concat")), 1u);

    // A boundary over code points and not bytes: no boundary falls
    // inside the two bytes of "ż"
    EXPECT_EQ(compiled("\\w+").count(of("ma\u0142e \u017c\u00f3\u0142wie")), 2u);
    EXPECT_EQ(piece_of(compiled("\\b\\w+\\b").find(of(" \u017c\u00f3\u0142w "))->text()),
              "\u017c\u00f3\u0142w");

    EXPECT_TRUE(compiled("(?i)stra\u00dfe").full_match(of("STRA\u00dfE")));
    EXPECT_TRUE(compiled("(?i)abc").full_match(of("AbC")));
    EXPECT_TRUE(compiled("(?i)[a-z]+").full_match(of("AbC")));
    EXPECT_FALSE(compiled("(?i)[^a-z]+").contains(of("A")));
    // the case folding reaches out of ASCII: the Kelvin sign folds to 'k'
    EXPECT_TRUE(compiled("(?i)k").full_match(of("\u212a")));

    // The flag holds to the end of the group it was set in
    EXPECT_TRUE(compiled("a(?i:b)c").full_match(of("aBc")));
    EXPECT_FALSE(compiled("a(?i:b)c").full_match(of("aBC")));
    EXPECT_TRUE(compiled("a(?i)bc").full_match(of("aBC")));
    EXPECT_TRUE(compiled("(?i)a(?-i:b)c").full_match(of("AbC")));
    EXPECT_FALSE(compiled("(?i)a(?-i:b)c").full_match(of("ABC")));

    EXPECT_FALSE(compiled("a.b").full_match(of("a\nb")));
    EXPECT_TRUE(compiled("(?s)a.b").full_match(of("a\nb")));
    EXPECT_EQ(compiled("(?m)^\\w+").count(of("one\ntwo\nthree")), 3u);
    EXPECT_EQ(compiled("^\\w+").count(of("one\ntwo\nthree")), 1u);
}

TEST(Regex_Tests, TheUnicodeProperties) {
    EXPECT_EQ(piece_of(compiled("\\p{Script=Cyrillic}+").find(of("abc \u041f\u0440\u0438\u0432\u0435\u0442 x"))->text()),
              "\u041f\u0440\u0438\u0432\u0435\u0442");
    EXPECT_EQ(piece_of(compiled("\\p{Lu}+").find(of("abcDEFgh"))->text()), "DEF");
    EXPECT_EQ(piece_of(compiled("\\p{L}+").find(of("12\u017c\u00f3\u014242"))->text()), "\u017c\u00f3\u0142");
    EXPECT_EQ(piece_of(compiled("\\pN+").find(of("ab123"))->text()), "123");
    EXPECT_TRUE(compiled("\\P{L}+").full_match(of("123 ")));
    // the name is matched as Unicode matches one: case and the
    // separators do not count
    EXPECT_TRUE(compiled("\\p{script=cyrillic}").full_match(of("\u0431")));
    EXPECT_TRUE(compiled("\\p{Old_Italic}").full_match(of("\U00010300")));
    EXPECT_TRUE(compiled("\\p{olditalic}").full_match(of("\U00010300")));
    EXPECT_FALSE(txt::regex::compile(of("\\p{Nosuch}")).has_value());

    // \d is the decimal digits of every script, not just the ASCII ones
    EXPECT_TRUE(compiled("\\d+").full_match(of("\u0663\u0664")));
    EXPECT_EQ(piece_of(compiled("[\u0430-\u044f]+").find(of("ab\u0431\u0432cd"))->text()),
              "\u0431\u0432");
    // '.' is one code point however many bytes it takes
    EXPECT_TRUE(compiled("^.$").full_match(of("\U0001F600")));
    EXPECT_TRUE(compiled("^.{3}$").full_match(of("\u017c\u00f3\u0142")));
}

TEST(Regex_Tests, TheNamedGroups) {
    auto re = compiled("(?<year>\\d{4})-(?<month>\\d{2})");
    auto m = re.find(of("on 2026-09-23"));
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(piece_of(*m->group("year")), "2026");
    EXPECT_EQ(piece_of(*m->group("month")), "09");
    EXPECT_FALSE(m->group("day").has_value());
    EXPECT_EQ(re.group_index(of("month")), optional<size_t>(2));
    EXPECT_FALSE(re.group_index(of("day")).has_value());
    // Python's spelling of the same thing
    EXPECT_TRUE(compiled("(?P<a>x)").find(of("x"))->group("a").has_value());
    EXPECT_FALSE(txt::regex::compile(of("(?<a>x)(?<a>y)")).has_value());
}

TEST(Regex_Tests, ReplaceAndSplit) {
    auto re = compiled("(\\d+)-(\\w+)");
    EXPECT_EQ(re.replace(of("1-a 22-bb"), of("[$2:$1]")), of("[a:1] [bb:22]"));
    EXPECT_EQ(re.replace(of("1-a 22-bb"), of("<$0>")), of("<1-a> <22-bb>"));
    EXPECT_EQ(re.replace_first(of("1-a 22-bb"), of("x")), of("x 22-bb"));
    EXPECT_EQ(re.replace(of("1-a"), of("$$")), of("$"));
    EXPECT_EQ(re.replace(of("1-a"), of("$9")), of(""));
    EXPECT_EQ(compiled("(?<k>\\w+)=(?<v>\\w+)").replace(of("a=1;b=2"), of("${v}:${k}")),
              of("1:a;2:b"));

    auto comma = compiled("\\s*,\\s*");
    auto parts = comma.split(of("a ,b,  c"));
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(piece_of(parts[0]), "a");
    EXPECT_EQ(piece_of(parts[1]), "b");
    EXPECT_EQ(piece_of(parts[2]), "c");
    EXPECT_EQ(comma.split(of("a,b,c"), 2).size(), 2u);
    EXPECT_EQ(piece_of(comma.split(of("a,b,c"), 2)[1]), "b,c");
    // a pattern of no width cuts between every code point
    auto every = compiled("x*").split(of("abc"));
    ASSERT_EQ(every.size(), 5u);
    EXPECT_EQ(piece_of(every[1]), "a");
    EXPECT_EQ(piece_of(every[3]), "c");
    EXPECT_EQ(compiled("x*").replace(of("abc"), of("-")), of("-a-b-c-"));
}

TEST(Regex_Tests, EveryMatchAsARange) {
    auto re = compiled("\\d+");
    vector<string> found;
    for (const auto& m : re.all(of("1 22 333"))) {
        found.push_back(of(std::string(m.text().data(), m.text().size())));
    }
    ASSERT_EQ(found.size(), 3u);
    EXPECT_EQ(found[0], of("1"));
    EXPECT_EQ(found[2], of("333"));
    EXPECT_EQ(re.count(of("1 22 333")), 3u);
    EXPECT_EQ(compiled("\\b").count(of("ab cd")), 4u);
    EXPECT_TRUE(re.all(of("no digits")).empty());
}

TEST(Regex_Tests, AMatchHoldsTheTextItWasFoundIn) {
    // The slice of a match holds the object the characters live in, so
    // the string it was found in may be gone
    optional<txt::match> kept;
    off_frame([&] {
        kept = compiled("\\w+").find(of("znaleziony tekst"));
    });
    collector::clear_stack();
    collector::force_collect();
    ASSERT_TRUE(kept.has_value());
    EXPECT_EQ(piece_of(kept->text()), "znaleziony");
}

//------------------------------------------------------------------------------
// Where this engine and a backtracking one answer differently, and why.
// Both places are the same place: a repetition whose body can match
// nothing. Perl and Python stop such a loop the moment a turn of it
// consumes nothing, because that is how they keep from looping forever;
// a machine that carries every alternative at once has no such danger and
// instead drops the turn that consumed nothing, since another thread has
// already been at that instruction at this position. So the empty turn is
// not taken, and a wider one behind it is.
//
// The consequence is written down here rather than argued about: these are
// the answers this engine gives, they are stable, and they are the reason
// the generated oracle puts no quantifier on a body that can match
// nothing. RE2 and Go part company with Perl in the same place.
TEST(Regex_Tests, WhereThisPartsCompanyWithABacktrackingEngine) {
    // The whole match differs. (?:a*|b)* over "aab": the loop's first
    // branch matches nothing at position 2, so Perl stops there and
    // answers "aa"; here that turn is dropped and the second branch
    // takes the "b"
    EXPECT_EQ(piece_of(compiled("(?:a*|b)*").find(of("aab"))->text()), "aab");
    EXPECT_EQ(piece_of(compiled("(?:|a)*").find(of("aab"))->text()), "aa");

    // The group differs. Both engines match "aa" with (a*)*, but Perl
    // takes one more turn of the loop, matching nothing at 2, and the
    // group is left holding that; here the last turn that happened is
    // the one that matched
    auto m = compiled("(a*)*").find(of("aab"));
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->begin_at(), 0u);
    EXPECT_EQ(m->end_at(), 2u);
    EXPECT_EQ(m->group(1)->size(), 2u);

    // And what does not differ, which is the point of saying where it
    // does: a body that must consume something behaves everywhere alike
    EXPECT_EQ(piece_of(compiled("(?:a|b)*").find(of("aab"))->text()), "aab");
    EXPECT_EQ(piece_of(compiled("(a+)+").find(of("aab"))->text()), "aa");
}

//------------------------------------------------------------------------------
TEST(Regex_Tests, WhatIsRefusedAndWhy) {
    struct Case { const char* pattern; const char* says; };
    const Case refused[] = {
        {"(a)\\1",        "backreference"},
        {"(?<a>x)\\k<a>", "backreference"},
        {"(?P=a)",        "backreference"},
        {"foo(?=bar)",    "lookahead"},
        {"foo(?!bar)",    "lookahead"},
        {"(?<=foo)bar",   "lookahead"},
        {"(?<!foo)bar",   "lookahead"},
        {"(?>a+)b",       "atomic"},
        {"a++",           "possessive"},
        {"(?(1)a|b)",     "conditional"},
    };
    for (const auto& c : refused) {
        auto re = txt::regex::compile(of(c.pattern));
        ASSERT_FALSE(re.has_value()) << c.pattern;
        EXPECT_LT(re.error().offset(), std::string_view(c.pattern).size()) << c.pattern;   // a byte of the pattern
        std::string message(re.error().message().view());
        EXPECT_NE(message.find(c.says), std::string::npos) << c.pattern << " -> " << message;
        // and every one of them says why, in the same words
        EXPECT_TRUE(message.find("backtracking") != std::string::npos
                    || message.find("linear") != std::string::npos) << message;
    }
}

TEST(Regex_Tests, WhatElseIsRefused) {
    const char* bad[] = {
        "(", ")", "a)", "[a", "[]", "a\\", "\\q", "*a", "+a", "?a", "{2}", "a**",
        "a{2}{3}", "[z-a]", "[\\b]", "\\p", "\\p{Nope}", "\\x", "\\xZZ", "\\x{110000}",
        "\\x{D800}", "(?<1a>x)", "(?<a>x)(?<a>y)", "(?z)", "a{1001}", "a{3,2}",
    };
    for (const char* p : bad) {
        auto re = txt::regex::compile(of(p));
        EXPECT_FALSE(re.has_value()) << p;
        if (!re) {
            EXPECT_FALSE(re.error().message().empty()) << p;
        }
    }
    // and the ones that only look bad
    const char* good[] = {"a{b}", "a{", "}", "\\{", "\\.", "[a-]", "[-a]", "()", "(?:)", "|", "a|"};
    for (const char* p : good) {
        EXPECT_TRUE(txt::regex::compile(of(p)).has_value()) << p;
    }
}

TEST(Regex_Tests, APatternThatIsALiteralIsReadByTheCompiler) {
    // no compile(), no optional: the compiler has already read it
    txt::regex re("(?<n>\\d+)\\s*(?i:kg)");
    auto m = re.find(of("waga 75 KG"));
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(piece_of(*m->group("n")), "75");
    // txt::regex bad("(a)\\1"); would be an error of the compiler, which
    // is the point; there is no way to write that here and still build
}

//------------------------------------------------------------------------------
// The reason the engine is what it is. Each of these is a pattern a
// backtracking engine goes exponential on; here every one of them comes
// back in milliseconds, and the test measures rather than argues.
TEST(Regex_Tests, WhatABacktrackingEngineCannotFinish) {
    struct Case { const char* pattern; size_t n; char fill; const char* tail; };
    const Case cases[] = {
        {"(a+)+b",        30, 'a', "!"},
        {"(a|a)*b",       30, 'a', "!"},
        {"(a*)*b",        30, 'a', "!"},
        {"(x+x+)+y",      30, 'x', "!"},
        {"(a|ab)*c",      30, 'a', "!"},
        {"a?a?a?a{3}b",   30, 'a', "!"},
        {"(.*)*b",        40, 'a', "!"},
        {"((a)*)*b",      40, 'a', "!"},
    };
    for (const auto& c : cases) {
        std::string text(c.n, c.fill);
        text += c.tail;
        auto re = compiled(c.pattern);
        bool found = true;
        double ms = milliseconds([&] { found = re.contains(of(text)); });
        EXPECT_FALSE(found) << c.pattern;
        EXPECT_LT(ms, 50.0) << c.pattern << " took " << ms << " ms";
    }

    // And the same pattern over a text a hundred times longer costs about
    // a hundred times as much, not a hundredth of the age of the universe
    auto re = compiled("(a+)+b");
    std::string small(100, 'a');
    std::string large(10000, 'a');
    double one = milliseconds([&] { re.contains(of(small)); });
    double many = milliseconds([&] { re.contains(of(large)); });
    EXPECT_LT(many, 400.0) << many << " ms over ten thousand characters";
    EXPECT_LT(many, one * 100 * 20 + 20.0);

    // A counted repetition is spelled out, so the program is bounded and
    // so is the work
    auto counted = compiled("(?:a?){200}a{200}");
    EXPECT_LT(milliseconds([&] { counted.full_match(of(std::string(200, 'a'))); }), 200.0);
}

// The same clock, held to the other half of the engine. Everything above
// asks what a text costs; this asks what a PATTERN costs, because the brake
// on spelling {n,m} out stood in the wrong place. MaxRegexInsts is raised
// where an instruction is written, so a body that writes none — (?:), (?i:),
// a group holding nothing but a flag — never reached it, and the turns
// multiplied through the nesting with nothing counting them:
// (?:(?:(?:(?:){1000}){1000}){1000}){1000} is forty bytes and 10^12 turns,
// and compile() did not come back. Three levels took 1.9 s, four took longer
// than anyone waited, and the depth limit of 200 allows 1000^200. That is
// the very denial of service this header refuses backreferences to prevent,
// moved from the match to the compile, and a pattern that arrives from
// outside the program goes through compile().
TEST(Regex_Tests, APatternThatCannotBeSpelledOut) {
    auto nest = [](int levels, const char* inner, const char* count) {
        std::string p = inner;
        for (int i = 0; i < levels; ++i) {
            p = "(?:" + p + count + ")";
        }
        return p + count;
    };
    // a body that emits nothing, which is what walks past the ceiling
    for (int levels : {2, 3, 4, 6, 10, 20}) {
        for (const char* inner : {"(?:)", "(?i:)", "(?:(?i))"}) {
            std::string p = nest(levels, inner, "{1000}");
            SCOPED_TRACE(p);
            expected<txt::regex, txt::regex_error> re = txt::regex::compile(of("(?:)"));
            double ms = milliseconds([&] { re = txt::regex::compile(of(p)); });
            EXPECT_LT(ms, 100.0) << "compile took " << ms << " ms";
            if (levels >= 3) {
                EXPECT_FALSE(re.has_value()) << "a pattern of 1000^" << (levels + 1) << " turns compiled";
            }
        }
    }
    // the exact-and-small road _flatten takes when it reads the required
    // run off the tree, which multiplies the same way and by itself
    for (int levels : {4, 8, 12}) {
        std::string p = nest(levels, "(?:)", "{8}");
        SCOPED_TRACE(p);
        double ms = milliseconds([&] { (void)txt::regex::compile(of(p)); });
        EXPECT_LT(ms, 100.0) << "compile took " << ms << " ms";
    }
    // and what must still compile, so the brake is not a new refusal: every
    // one of these spells out to exactly the same program it did before the
    // counter existed, which is the point — nothing that fits inside the
    // instruction ceiling can come near the turn budget
    struct Good { const char* pattern; size_t insts; };
    const Good good[] = {
        {"a{1000}", 1003},
        {"(?:ab){1000}", 2003},
        {"(?:abcde){1000}", 5003},
        {"a{0,1000}", 2003},
        {"(?:a?){200}a{200}", 603},
        {"(?:(?:(?:(?:(?:a){8}){8}){8}){2}){2}", 2051},    // 8*8*8*2*2 copies of 'a'
        {"(?:(?:(?:(?:(?:){8}){8}){8}){8}){8}", 3},
    };
    for (const auto& g : good) {
        SCOPED_TRACE(g.pattern);
        auto re = txt::regex::compile(of(g.pattern));
        ASSERT_TRUE(re.has_value()) << "the brake refused a pattern that fits";
        EXPECT_EQ(re->program_size(), g.insts);
    }
}

//------------------------------------------------------------------------------
TEST(Regex_Tests, TheOracleOnTheFirstMatch) {
    size_t cases = 0;
    size_t differ = 0;
    for (const auto& c : ucd::RegexFindCases) {
        auto re = txt::regex::compile(with_flags(c.flags, c.pattern));
        ASSERT_TRUE(re.has_value()) << c.pattern << " / " << c.flags
            << " -> " << std::string(re ? "" : std::string(re.error().message().view()));
        auto m = re->find(of(c.text));
        std::string got = m ? spans(*m) : "-";
        if (got != std::string(c.expect)) {
            if (++differ < 12) {
                ADD_FAILURE() << "pattern " << c.pattern << " flags '" << c.flags
                              << "' text '" << c.text << "' got " << got
                              << " want " << c.expect;
            }
        }
        ++cases;
    }
    EXPECT_EQ(cases, std::size(ucd::RegexFindCases));
    EXPECT_EQ(differ, 0u);
}

TEST(Regex_Tests, TheOracleOnEveryMatch) {
    for (const auto& c : ucd::RegexAllCases) {
        auto re = txt::regex::compile(with_flags(c.flags, c.pattern));
        ASSERT_TRUE(re.has_value()) << c.pattern;
        std::string got;
        for (const auto& m : re->all(of(c.text))) {
            if (!got.empty()) {
                got += ",";
            }
            got += std::to_string(m.begin_at()) + ":" + std::to_string(m.end_at());
        }
        ASSERT_EQ(got, std::string(c.expect))
            << "pattern " << c.pattern << " flags '" << c.flags << "' text '" << c.text << "'";
    }
}

TEST(Regex_Tests, TheOracleOnSplit) {
    for (const auto& c : ucd::RegexSplitCases) {
        auto re = txt::regex::compile(with_flags(c.flags, c.pattern));
        ASSERT_TRUE(re.has_value()) << c.pattern;
        std::string got;
        bool first = true;
        for (auto p : re->split(of(c.text))) {
            if (!first) {
                got += '\x01';
            }
            first = false;
            got += piece_of(p);
        }
        ASSERT_EQ(got, std::string(c.expect))
            << "pattern " << c.pattern << " flags '" << c.flags << "' text '" << c.text << "'";
    }
}

//------------------------------------------------------------------------------
// Random patterns and random texts through every entry point, to be run
// under the address and undefined-behaviour sanitizers: the cheap way to
// cover a parser, a compiler and a machine at once, which is what note 170
// found for the rest of the module. Nothing is compared here — the oracle
// above does that — only that nothing crashes and nothing is undefined.
TEST(Regex_Tests, Fuzz) {
    std::mt19937 rng(20260923);
    const char* atoms[] = {
        "a", "b", ".", "[ab]", "[^a]", "[a-c]", "\\d", "\\w", "\\s", "\\D", "\\W", "\\S",
        "\\b", "\\B", "^", "$", "\\A", "\\z", "\\p{L}", "\\P{Lu}", "\\p{Script=Greek}",
        "\u017c", "\\x41", "\\x{1F600}", "(", ")", "(?:", "(?<n>", "[", "]", "|", "*", "+",
        "?", "{2}", "{1,3}", "{2,}", "*?", "??", "\\", "(?i)", "(?-s:", "\\1", "(?=", "(?<=",
        "-", "\\n", "\\Q", "{", "}",
    };
    const char* points[] = {
        "a", "Z", "0", " ", "\n", "\u017c", "\u0414", "\u03c2", "\u00df", "\U0001F600",
        "\u0915\u093f", "\xC3", "\x80", "\xF0\x9F", "\u0000x", "\t", ".", "[", "\\",
    };
    size_t compiled_ok = 0;
    for (int round = 0; round < 4000; ++round) {
        std::string pattern;
        for (size_t i = 0, n = rng() % 12; i < n; ++i) {
            pattern += atoms[rng() % std::size(atoms)];
        }
        std::string text;
        for (size_t i = 0, n = rng() % 20; i < n; ++i) {
            text += points[rng() % std::size(points)];
        }
        auto re = txt::regex::compile(of(pattern));
        if (!re) {
            EXPECT_FALSE(re.error().message().empty());
            continue;
        }
        ++compiled_ok;
        string subject = of(text);
        re->contains(subject);
        re->full_match(subject);
        auto m = re->find(subject);
        if (m) {
            for (size_t g = 0; g <= m->group_count() + 1; ++g) {
                (void)m->group(g);
                (void)(*m)[g];
            }
            (void)m->group(of("n"));
        }
        size_t seen = 0;
        for (const auto& one : re->all(subject)) {
            (void)one.text();
            if (++seen > 200) {
                break;
            }
        }
        re->replace(subject, of("<$0|$1|${n}|$$>"));
        re->split(subject);
        re->split(subject, 3);
        if (!text.empty()) {
            re->find(subject, rng() % text.size());
        }
        re->find(subject, text.size() + 5);
    }
    EXPECT_GT(compiled_ok, 400u);
}

// The machine of a walk is a plain engine: it holds a reference to the
// program and a string_view over the characters, which are two raw pointers
// into managed memory, and a managed object is the one place such a pointer
// may not live. The collector said so — it classified the words as data and
// then found the addresses of live objects in them, and printed that on
// stderr in every build with assertions on, which is to say at the user.
//
// It never had to be managed to be safe. What keeps the program and the
// characters alive is the state and the text the iterator already carries
// beside it and copies with it, so the machine cannot outlive what it points
// into however the iterator is passed about. Shared rather than managed, and
// the collector has nothing of it to look at.
TEST(Regex_Tests, TheMachineOfAWalkIsNotAManagedObject) {
    auto re = compiled("l+");
    string text = of("hello hello hello hello");
    auto matches = re.all(text);
    // held open, so that a managed machine would be a LIVE managed object
    // and not merely one that had been collected by the time we looked
    auto it = matches.begin();
    ASSERT_NE(it, matches.end());
    for (const auto& s : collector::get_type_statistics()) {
        if (s.type && *s.type == typeid(txt::detail::matcher)) {
            ADD_FAILURE() << "the machine is a managed object again: "
                          << s.live_objects << " live";
        }
    }
    // and the lifetime the change rests on: a copy of an iterator walks on
    // after the range it came from is gone, because it holds the text and
    // the state itself
    size_t walked = 0;
    {
        auto copy = matches.begin();
        auto last = matches.end();
        for (auto one = copy; one != last; ++one) {
            ++walked;
        }
    }
    EXPECT_EQ(walked, 4u);
    // a walk over a text that is a temporary, which is what the slice is for
    size_t over_temporary = 0;
    for (const auto& one : re.all(of(std::string("hello hello")))) {
        EXPECT_EQ(piece_of(one.text()), "ll");
        ++over_temporary;
    }
    EXPECT_EQ(over_temporary, 2u);
    collector::clear_stack();
    collector::force_collect();
}

// A group number in a replacement is read a digit at a time into a size_t,
// and a size_t wraps. $9223372036854775808 doubled to nothing and the test
// `2 * group + 1 < ncap` then read 1 < ncap and put the whole match in;
// ${9223372036854775809} landed on 3 and put group one in. Neither is out of
// bounds — if 2*group + 1 is below ncap then so is 2*group — so nothing
// crashed and nothing ever would have: it simply substituted a group nobody
// named, where the documented answer is that a group the pattern does not
// have puts nothing in.
TEST(Regex_Tests, AGroupNumberInAReplacementCannotWrap) {
    auto re = compiled("(b)");
    string text = of("abc");
    struct Case { const char* with; const char* want; };
    const Case cases[] = {
        {"[$0]", "a[b]c"},
        {"[$1]", "a[b]c"},
        {"[$2]", "a[]c"},
        {"[${1}]", "a[b]c"},
        // 2^63: 2*group wrapped to 0, so this used to give the whole match
        {"[$9223372036854775808]", "a[]c"},
        // 2^64: wrapped to 0 outright, the whole match again
        {"[$18446744073709551616]", "a[]c"},
        // 2^63 + 1: 2*group wrapped to 2, which is group one
        {"[${9223372036854775809}]", "a[]c"},
        {"[$99999999999999999999]", "a[]c"},
        {"[$251]", "a[]c"},                       // one past the engine's limit
        {"[${18446744073709551617}]", "a[]c"},
        {"[$000000000000000000001]", "a[b]c"},    // leading zeros are still group one
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.with);
        EXPECT_EQ(piece_of(re.replace(text, of(c.with)).as_slice()), std::string(c.want));
    }
    // and the same through a pattern that really does have many groups, so
    // that holding the number down does not lose one that exists
    std::string many;
    for (int i = 0; i < 250; ++i) many += "(a)";
    auto wide = compiled(many.c_str());
    string as = of(std::string(250, 'a'));
    EXPECT_EQ(piece_of(wide.replace(as, of("$1$250")).as_slice()), "aa");
    EXPECT_EQ(piece_of(wide.replace(as, of("$251")).as_slice()), "");
}

// The matcher carved its two thread lists out of one instruction per row,
// and a row is 2*(groups+1) words of group slots. But _add only ever appends
// an instruction that reads a code point — jump, split, save and check it
// walks through without storing — so the list is as long as program::listed
// and no longer, and the rows for the rest were allocated, zeroed and never
// touched. A pattern of 250 groups spelled out to 19003 instructions asked
// for 145.6 MB and one find() over a five-byte text took 10.3 ms, nearly all
// of it the memset inside make_unique.
//
// The bound has to be exactly right: a list grows no further than it, and
// the first rows of both lists sit inside the matcher's one block, where a
// list run over its end is not seen by any sanitizer. It holds because list.size
// rises only in that one branch and because a pc already carrying the list's
// stamp is skipped, and the stamp is one per list however many times _add is
// called into it — so each such instruction is counted at most once.
TEST(Regex_Tests, AThreadListIsAsLongAsWhatCanStandInIt) {
    auto program_of = [](const char* pattern) {
        txt::detail::regex_tree tree;
        EXPECT_FALSE(bool(txt::detail::regex_parser(pattern).parse(tree)));
        txt::detail::program prog;
        EXPECT_FALSE(bool(txt::detail::regex_compiler(tree, prog).compile()));
        return prog;
    };
    const char* patterns[] = {
        "zyzykot", "\\bzyzykot\\b", "[0-9]+", "kot|pies|ryba",
        "(\\d{4})-(\\d{2})-(\\d{2})", "(\\w+)@(\\w+)\\.(com|pl)",
        "zyz.*kot", "(a+)+b", "((((((a))))))+x", "(?:a|b|c|d|e|f|g|h){20}",
        "(?:)", "a", "(?:a|b)*",
    };
    for (const char* p : patterns) {
        SCOPED_TRACE(p);
        auto prog = program_of(p);
        // counted again here rather than trusted: only these four opcodes
        // ever reach the branch that lengthens a list
        uint32_t counted = 0;
        for (const auto& i : prog.insts) {
            if (i.op == txt::detail::opcode::literal || i.op == txt::detail::opcode::klass
                || i.op == txt::detail::opcode::any || i.op == txt::detail::opcode::match) {
                ++counted;
            }
        }
        EXPECT_EQ(prog.listed, counted ? counted : 1u);
        EXPECT_LE(prog.listed, prog.insts.size());
        EXPECT_GE(prog.listed, 1u);
    }

    // and a pattern built to fill the list: twenty branches all alive at the
    // same position, over a text that keeps every one of them alive, asked
    // through the anchored road and the unanchored one so that a list run
    // over its end would show as a wrong answer and not only as a crash
    auto re = compiled("(?:a|aa|aaa|aaaa|aaaaa|aaaaaa|aaaaaaa|aaaaaaaa)+b");
    EXPECT_TRUE(re.full_match(of(std::string(64, 'a') + "b")));
    EXPECT_TRUE(re.contains(of("xxx" + std::string(64, 'a') + "byyy")));
    EXPECT_FALSE(re.contains(of(std::string(64, 'a'))));
    auto groups = compiled("(a)(a)(a)(a)(a)(a)(a)(a)(a)(a)|(b)");
    auto m = groups.find(of("aaaaaaaaaa"));
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->group_count(), 11u);
    EXPECT_TRUE(m->group(1).has_value());
    EXPECT_FALSE(m->group(11).has_value());
}

// program::listed is what a thread list COULD reach, and the lists were
// sized by it: rows of 2*(groups+1) words each, so the pattern of the limits —
// 250 groups spelled out to 19003 instructions — asked for 146 MB with every
// find(), five bytes of text or not. A list holds the instructions reachable
// at one position without reading a code point, and for nearly every pattern
// that is a handful, so the lists start at sixteen rows inside the matcher's
// block and move to a block of listed rows the one time they run out.
//
// Three things are held here. That the pattern of the limits takes its
// sixteen rows and no more over a text that keeps nothing alive. That a
// search which does keep hundreds of threads alive gets the rows it needs,
// and no more than listed. And that the group positions come through the
// move whole: a match whose groups were written while the list was sixteen
// rows long and read after it had moved.
TEST(Regex_Tests, AThreadListHoldsWhatASearchReaches) {
    auto program_of = [](const std::string& pattern) {
        txt::detail::regex_tree tree;
        EXPECT_FALSE(bool(txt::detail::regex_parser(pattern).parse(tree)));
        txt::detail::program prog;
        EXPECT_FALSE(bool(txt::detail::regex_compiler(tree, prog).compile()));
        return prog;
    };
    std::string widest;
    for (int i = 0; i < 250; ++i) widest += "(a?)";
    for (int i = 0; i < 18; ++i) widest += "b{1000}";
    auto prog = program_of(widest);
    ASSERT_GT(prog.listed, 18000u);
    const size_t ncap = 2 * 251;
    std::vector<size_t> caps(ncap);

    std::string hello = "hello";
    txt::detail::matcher idle(prog, std::string_view(hello), ncap);
    EXPECT_FALSE(idle.run(0, caps.data()));
    EXPECT_LE(idle.list_rows(), 16u) << "the lists are sized by the program again";

    // 250 optional a's and one b stand in the list at every position, and
    // every b read keeps one more thread alive: some five hundred rows, far
    // from the eighteen thousand the program could fill
    std::string bs = "hello" + std::string(300, 'b');
    txt::detail::matcher busy(prog, std::string_view(bs), ncap);
    EXPECT_FALSE(busy.run(0, caps.data()));
    EXPECT_GT(busy.list_rows(), 300u);
    EXPECT_LE(busy.list_rows(), size_t(prog.listed));
    EXPECT_FALSE(compiled(widest).find(of(bs)).has_value());

    // thirty groups written at position 2, a counted repetition that keeps
    // one thread for every b it has read, and a match read out at the end
    std::string grouped;
    for (int i = 0; i < 30; ++i) grouped += "(a?)";
    grouped += "(b{1,200})c";
    std::string text = "zz" + std::string(150, 'b') + "c";
    std::string want = "2:153";
    for (int i = 0; i < 30; ++i) want += ";2:2";
    want += ";2:152";
    auto m = compiled(grouped).find(of(text));
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(spans(*m), want);
    auto small = program_of(grouped);
    txt::detail::matcher grown(small, std::string_view(text), 2 * 32);
    std::vector<size_t> held(2 * 32);
    ASSERT_TRUE(grown.run(0, held.data()));
    EXPECT_GT(grown.list_rows(), 64u) << "the test no longer moves the lists";
    EXPECT_LE(grown.list_rows(), size_t(small.listed));
}

// The visit stamps are a 32-bit counter and a counter runs out.
//
// _gen holds zero for an instruction nobody has stood on yet, and _add drops
// a thread whose instruction already carries this list's stamp. So the moment
// the counter came back round to zero, every instruction NOT yet visited read
// as "already here at this position" and was dropped — and an instruction
// behind a literal the text only reaches late is exactly such an instruction.
// The matcher is kept for a whole walk and the counter climbs once per run()
// and once per position, so all(), replace(), split() and count() over a text
// of a couple of gigabytes reached it, and matches simply stopped being found
// with nothing said.
//
// Two billion positions is not a test, so the matcher takes the stamp it
// starts from and the test starts it sixteen below the wrap. The pattern is
// the one that shows it: `literal b` is first walked two positions after the
// first 'q', so sweeping where the 'q' stands sweeps the wrap across that
// first visit. The walk is the one all() makes — one matcher, run() called
// from one position after another.
TEST(Regex_Tests, TheVisitStampsRunOut) {
    txt::detail::regex_tree tree;
    ASSERT_FALSE(bool(txt::detail::regex_parser("(?s).q.b").parse(tree)));
    txt::detail::program prog;
    ASSERT_FALSE(bool(txt::detail::regex_compiler(tree, prog).compile()));

    auto walk = [&prog](const std::string& text, uint32_t seed) {
        txt::detail::matcher machine(prog, std::string_view(text), 2, seed);
        size_t caps[2];
        size_t found = 0;
        size_t from = 0;
        while (from <= text.size() && machine.run(from, caps)) {
            ++found;
            from = caps[1] > caps[0] ? caps[1] : caps[1] + 1;
        }
        return found;
    };

    for (int lead = 0; lead < 80; ++lead) {
        std::string text(size_t(lead), 'a');
        text += "aqxb";
        SCOPED_TRACE("lead=" + std::to_string(lead));
        // the counter sixteen below the wrap, so the sweep crosses it
        EXPECT_EQ(walk(text, 0xFFFFFFF0u), 1u) << "the match went missing as the stamps wrapped";
        // and the ordinary counter agrees, so the seed is not doing the work
        EXPECT_EQ(walk(text, 0u), 1u);
    }

    // the wrap itself, asked directly: a counter that comes back to zero
    // must not read as the mark of an instruction nobody has visited
    std::string text = "aaaaaaaaaaaaaaaaaaaaaqxb";
    for (uint32_t seed : {0xFFFFFF00u, 0xFFFFFFF0u, 0xFFFFFFFEu, 0xFFFFFFFFu}) {
        SCOPED_TRACE(seed);
        EXPECT_EQ(walk(text, seed), 1u);
    }
}

// A fold class is not always the code point, its lower and its upper.
// equal_fold asks whether the two lowers agree or the two uppers do, so 'k'
// holds U+212A the Kelvin sign, 's' holds U+017F the long s, and 'i' holds
// U+0130 — and the set of bytes a match may begin with was built from three
// members of the class instead of all of them. The symptom was the sharpest
// one a search engine can have: matches() said yes and contains() said no
// about the same text, because only the unanchored road consults that set.
// Every pair goes both ways — the one code point in the pattern and the
// other in the text, then the other way about — and through every entry
// point, since find, count, replace, split and all take the road contains
// takes and matches does not.
TEST(Regex_Tests, AFoldClassLargerThanThreeCodePoints) {
    struct Pair {
        const char* one;
        const char* other;
        const char* name;
    };
    const Pair Pairs[] = {
        {"k", "\xE2\x84\xAA", "k / U+212A KELVIN SIGN"},
        {"s", "\xC5\xBF",     "s / U+017F LONG S"},
        {"i", "\xC4\xB0",     "i / U+0130 I WITH DOT ABOVE"},
        {"I", "\xC4\xB1",     "I / U+0131 DOTLESS I"},
        {"\xC3\x9F", "\xE1\xBA\x9E", "U+00DF / U+1E9E SHARP S"},
        {"\xC3\x85", "\xE2\x84\xAB", "U+00C5 / U+212B ANGSTROM SIGN"},
        {"\xCE\xA9", "\xE2\x84\xA6", "U+03A9 / U+2126 OHM SIGN"},
        {"\xCF\x83", "\xCF\x82",     "U+03C3 / U+03C2 FINAL SIGMA"},
    };
    for (const auto& pair : Pairs) {
        for (int turn = 0; turn < 2; ++turn) {
            const char* in_pattern = turn ? pair.other : pair.one;
            const char* in_text = turn ? pair.one : pair.other;
            SCOPED_TRACE(std::string(pair.name) + (turn ? " (reversed)" : ""));
            auto re = txt::regex::compile(with_flags("i", in_pattern));
            ASSERT_TRUE(re.has_value());
            // the anchored road: this always worked, and is what the rest
            // of the class has to agree with
            ASSERT_TRUE(re->full_match(of(in_text))) << "the machine does not fold these at all";
            string subject = of(std::string("a ") + in_text + " b");
            EXPECT_TRUE(re->contains(subject));
            auto m = re->find(subject);
            ASSERT_TRUE(m.has_value());
            EXPECT_EQ(piece_of(m->text()), std::string(in_text));
            EXPECT_EQ(re->count(subject), 1u);
            EXPECT_EQ(piece_of(re->replace(subject, of("#")).as_slice()), "a # b");
            EXPECT_EQ(re->split(subject).size(), 2u);
            size_t walked = 0;
            for (const auto& one : re->all(subject)) {
                EXPECT_EQ(piece_of(one.text()), std::string(in_text));
                ++walked;
            }
            EXPECT_EQ(walked, 1u);
        }
    }
}

// The sweep that makes the ASCII half of a fold class exact stops at
// AsciiFoldLimit rather than walking the million code points of Unicode at
// every translation unit. Nothing above it may fold into ASCII, or the sweep
// would miss a member and the fault above would be back for that code point.
// This is the test that does walk all of them.
TEST(Regex_Tests, NothingAboveTheFoldLimitReachesIntoAscii) {
    size_t below = 0;
    char32_t highest = 0;
    for (char32_t c = 0x80; c <= 0x10FFFF; ++c) {
        if (c >= 0xD800 && c <= 0xDFFF) {
            continue;
        }
        if (unicode::to_lower(c) < 0x80 || unicode::to_upper(c) < 0x80) {
            highest = c;
            EXPECT_LT(c, txt::detail::AsciiFoldLimit)
                << "U+" << std::hex << uint32_t(c) << " folds into ASCII above the limit";
            if (c < txt::detail::AsciiFoldLimit) {
                ++below;
            }
        }
    }
    // U+0130, U+0131, U+017F and U+212A, the highest of them the Kelvin sign
    EXPECT_EQ(below, 4u);
    EXPECT_EQ(uint32_t(highest), 0x212Au);
    // and the six ASCII code points they reach, and no others
    size_t marked = 0;
    for (char32_t a = 0; a < 128; ++a) {
        bool bit = (txt::detail::AsciiFoldsOutside[a >> 6] >> (a & 63)) & 1;
        bool real = false;
        for (char32_t c = 0x80; c < txt::detail::AsciiFoldLimit; ++c) {
            if (unicode::equal_fold(a, c)) {
                real = true;
                break;
            }
        }
        EXPECT_EQ(bit, real) << "U+" << std::hex << uint32_t(a);
        marked += bit ? 1 : 0;
    }
    EXPECT_EQ(marked, 6u);   // i I k K s S
}

// The narrow set is the whole reason an unanchored search is quick — a
// pattern whose first byte is rare skips whole runs of the text — so the fix
// must not widen it for a pattern that names none of the six. (?i) over
// plain letters still says two bytes, not every byte above U+007F.
TEST(Regex_Tests, TheFoldFixDoesNotBluntTheOrdinaryPattern) {
    auto program_of = [](const char* pattern) {
        txt::detail::regex_tree tree;
        EXPECT_FALSE(bool(txt::detail::regex_parser(pattern).parse(tree)));
        txt::detail::program prog;
        EXPECT_FALSE(bool(txt::detail::regex_compiler(tree, prog).compile()));
        return prog;
    };
    auto lead_bytes = [](const txt::detail::program& p) {
        size_t n = 0;
        for (size_t b = 0; b < 256; ++b) {
            n += p.may_begin_with(uint8_t(b)) ? 1 : 0;
        }
        return n;
    };
    auto plain = program_of("(?i)zyzykot");
    EXPECT_TRUE(plain.first_known);
    EXPECT_EQ(lead_bytes(plain), 2u);            // 'z' and 'Z', as before the fix
    EXPECT_TRUE(plain.may_begin_with('z'));
    EXPECT_TRUE(plain.may_begin_with('Z'));
    // one of the six, and the set has to open up to every lead byte
    auto wide = program_of("(?i)kot");
    EXPECT_TRUE(wide.first_known);
    EXPECT_TRUE(wide.may_begin_with('k'));
    EXPECT_TRUE(wide.may_begin_with('K'));
    EXPECT_TRUE(wide.may_begin_with(0xE2));      // U+212A begins with this
    // and a pattern with no folding at all is untouched
    EXPECT_EQ(lead_bytes(program_of("zyzykot")), 1u);
}
