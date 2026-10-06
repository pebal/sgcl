//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Dates and times in a locale (localized.h): CLDR's pattern letters, the four
// styles, skeletons, intervals and the names of months and days. The oracle is
// ICU 78 through the vectors of tools/cldr_vectors.py (the cases where its
// CLDR 48 and the library's CLDR 46 agree); the rest is held here by hand from
// the XML of CLDR 46.
#include "tests/types.h"
#include "tests/time/cldr_answer.h"
#include "tests/time/cldr_date_vectors.h"

#include <string>

namespace {
    std::string text(const char8_t* s) {
        return std::string(reinterpret_cast<const char*>(s));
    }

    txt::locale loc(const char* tag) {
        return txt::locale(string(tag));
    }

    time::zone warsaw() {
        return *time::zone::load(string("Europe/Warsaw"));
    }

    // 2026-09-24 14:05:09.250 in Warsaw (CEST, +02:00), a Thursday
    time::datetime when() {
        return time::date(2026, 9, 24).at(14, 5, 9, warsaw()) + duration(std::chrono::milliseconds(250));
    }
}

TEST(Localized_Tests, Vectors) {
    size_t bad = 0;
    for (const auto& v : DateVectors) {
        std::string q = text(v.question);
        std::string got = cldr_answer::date_answer(cldr_answer::fields(q));
        if (got != text(v.answer) && ++bad <= 20) {
            ADD_FAILURE() << q << "\n  got " << got << "\n  want " << text(v.answer);
        }
    }
    EXPECT_EQ(bad, 0u) << "of " << std::size(DateVectors);
}

TEST(Localized_Tests, Styles) {
    auto t = when();
    auto pl = loc("pl");
    EXPECT_EQ(t.format(pl), string("24 wrz 2026, 14:05"));
    EXPECT_EQ(t.format(pl, time::style::full, time::style::brief), string("czwartek, 24 wrze\xC5\x9Bnia 2026 14:05"));
    EXPECT_EQ(t.format(pl, time::style::brief, time::style::none), string("24.09.2026"));
    EXPECT_EQ(t.format(pl, time::style::none, time::style::medium), string("14:05:09"));
    EXPECT_EQ(t.format(pl, time::style::detailed, time::style::detailed),
              string("24 wrze\xC5\x9Bnia 2026 14:05:09 GMT+2"));   // the zone's name: a display name, GMT without one
    EXPECT_EQ(t.format(pl, time::style::none, time::style::none), string());
    EXPECT_EQ(t.format(loc("en"), time::style::full, time::style::brief),
              string("Thursday, September 24, 2026 at 2:05\xE2\x80\xAFPM"));
    EXPECT_EQ(t.format(loc("de"), time::style::medium, time::style::medium), string("24.09.2026, 14:05:09"));
    EXPECT_EQ(t.format(loc("ar-EG"), time::style::brief, time::style::none),
              string("\xD9\xA2\xD9\xA4\xE2\x80\x8F/\xD9\xA9\xE2\x80\x8F/\xD9\xA2\xD9\xA0\xD9\xA2\xD9\xA6"));
    EXPECT_EQ(t.format(txt::locale(), time::style::medium, time::style::brief), string("2026 M09 24 14:05"));   // root: CLDR's root names the months M01..M12
    // a date
    time::date d(2026, 9, 24);
    EXPECT_EQ(d.format(pl), string("24 wrz 2026"));
    EXPECT_EQ(d.format(pl, time::style::full), string("czwartek, 24 wrze\xC5\x9Bnia 2026"));
    EXPECT_EQ(d.format(pl, string("MMMMd")), string("24 wrze\xC5\x9Bnia"));
    EXPECT_EQ(time::date_format::from_pattern(pl, string("HH:mm")).format(d), string("00:00"));   // its midnight
}

TEST(Localized_Tests, Skeletons) {
    auto t = when();
    auto pl = loc("pl");
    EXPECT_EQ(t.format(pl, string("MMMd")), string("24 wrz"));
    EXPECT_EQ(t.format(pl, string("yMMMMd")), string("24 wrze\xC5\x9Bnia 2026"));
    EXPECT_EQ(t.format(pl, string("LLLL")), string("wrzesie\xC5\x84"));      // stand-alone
    EXPECT_EQ(t.format(pl, string("jm")), string("14:05"));
    EXPECT_EQ(t.format(loc("en"), string("jm")), string("2:05\xE2\x80\xAFPM"));
    EXPECT_EQ(t.format(loc("en"), string("yMMMdjm")), string("Sep 24, 2026, 2:05\xE2\x80\xAFPM"));
    EXPECT_EQ(t.format(loc("en"), string("Hmsv")), string("14:05:09 GMT+2"));
    EXPECT_EQ(t.format(loc("en"), string("jmsSSS")), string("2:05:09.250\xE2\x80\xAFPM"));
    EXPECT_EQ(time::date_format::from_skeleton(loc("en"), string("yMMMd")).pattern(), string("MMM d, y"));
    EXPECT_EQ(time::date_format::from_skeleton(loc("de"), string("yMMMMEEEEd")).pattern(), string("EEEE, d. MMMM y"));
    EXPECT_EQ(time::date_format::from_skeleton(loc("en"), string("")).pattern(), string(""));
    EXPECT_EQ(time::date_format::from_skeleton(loc("en"), string("?!")).pattern(), string(""));
    // the same fields in any order
    EXPECT_EQ(time::date_format::from_skeleton(loc("en"), string("dMMMy")).pattern(), string("MMM d, y"));
}

TEST(Localized_Tests, Patterns) {
    auto t = when();
    auto f = [&](const char* tag, const char* pattern) {
        return time::date_format::from_pattern(loc(tag), string(pattern)).format(t);
    };
    EXPECT_EQ(f("pl", "EEEE, d MMMM y"), string("czwartek, 24 wrze\xC5\x9Bnia 2026"));
    EXPECT_EQ(f("pl", "d LLLL"), string("24 wrzesie\xC5\x84"));
    EXPECT_EQ(f("en", "G GGGG GGGGG"), string("AD Anno Domini A"));
    EXPECT_EQ(f("en", "Q QQQ QQQQ"), string("3 Q3 3rd quarter"));
    EXPECT_EQ(f("en", "h:mm a"), string("2:05 PM"));   // the pattern's own space
    EXPECT_EQ(f("en", "K k"), string("2 14"));
    EXPECT_EQ(f("en", "B"), string("in the afternoon"));
    EXPECT_EQ(f("en", "S SS SSSSSS"), string("2 25 250000"));
    EXPECT_EQ(f("en", "Z ZZZZ ZZZZZ X XXX x O OOOO"), string("+0200 GMT+02:00 +02:00 +02 +02:00 +02 GMT+2 GMT+02:00"));
    EXPECT_EQ(f("en", "VV VVV"), string("Europe/Warsaw Warsaw"));
    EXPECT_EQ(f("en", "w W D F e c g"), string("39 4 267 4 5 5 2461308"));
    EXPECT_EQ(f("en", "'o''clock' h"), string("o'clock 2"));
    EXPECT_EQ(f("en", "''"), string("'"));
    EXPECT_EQ(f("en", "'unterminated"), string("unterminated"));
    EXPECT_EQ(f("en", "j R"), string("j R"));         // letters CLDR does not define here stand as they are
    EXPECT_EQ(f("en", ""), string(""));
    EXPECT_EQ(f("en", "yyyyyyyyy"), string("000002026"));
    // noon, and midnight held back as ICU holds it
    auto noon = time::date(2026, 9, 24).at(12, 0, warsaw());
    auto midnight = time::date(2026, 9, 24).at(0, 0, warsaw());
    EXPECT_EQ(time::date_format::from_pattern(loc("en"), string("h:mm b")).format(noon), string("12:00 noon"));
    EXPECT_EQ(time::date_format::from_pattern(loc("en"), string("h:mm b")).format(midnight), string("12:00 AM"));
    // the year before 1, the era and the year of the era; a negative offset
    auto ny = *time::zone::load(string("America/New_York"));
    auto winter = time::date(2026, 1, 5).at(9, 30, ny);
    EXPECT_EQ(time::date_format::from_pattern(loc("en"), string("ZZZZZ O")).format(winter), string("-05:00 GMT-5"));
    EXPECT_EQ(time::date_format::from_pattern(loc("en"), string("ZZZZZ")).format(winter.utc()), string("Z"));
    // a long text past the room on the stack
    string many(std::string(300, 'd').c_str());
    EXPECT_EQ(time::date_format::from_pattern(loc("en"), many).format(t).size(), 300u);
}

TEST(Localized_Tests, Intervals) {
    auto t = when();
    auto pl = loc("pl");
    EXPECT_EQ(t.format_interval(t + duration(std::chrono::hours(48)), pl, string("yMMMd")), string("24\xE2\x80\x93" "26 wrz 2026"));
    EXPECT_EQ(t.format_interval(t + duration(std::chrono::hours(24 * 40)), pl, string("yMMMd")),
              string("24 wrz\xE2\x80\x93" "3 lis 2026"));
    EXPECT_EQ(t.format_interval(t + duration(std::chrono::hours(2)), pl, string("Hm")), string("14:05\xE2\x80\x93" "16:05"));
    EXPECT_EQ(t.format_interval(t + duration(std::chrono::minutes(1)), pl, string("yMMMd")), string("24 wrz 2026"));
    EXPECT_EQ(t.format_interval(t, pl, string("Hm")), string("14:05"));
    EXPECT_EQ(t.format_interval(t + duration(std::chrono::hours(3)), loc("en"), string("hm")),
              string("2:05\xE2\x80\x89\xE2\x80\x93\xE2\x80\x89" "5:05\xE2\x80\xAFPM"));
    EXPECT_EQ(t.format_interval(t + duration(std::chrono::hours(24 * 400)), loc("en"), string("MMMd")),
              string("Sep 24, 2026\xE2\x80\x89\xE2\x80\x93\xE2\x80\x89Oct 29, 2027"));   // the year added
    EXPECT_EQ(t.format_interval(t + duration(std::chrono::hours(48)), loc("en"), string("Hm")),
              string("9/24/2026, 14:05\xE2\x80\x89\xE2\x80\x93\xE2\x80\x89" "9/26/2026, 14:05"));   // the date added
    // the second time in the first's zone
    auto ny = t.in(*time::zone::load(string("America/New_York")));
    EXPECT_EQ(t.format_interval(ny + duration(std::chrono::hours(1)), pl, string("Hm")), string("14:05\xE2\x80\x93" "15:05"));
}

TEST(Localized_Tests, Names) {
    EXPECT_EQ(time::to_string(time::month::september, loc("pl")), string("wrze\xC5\x9Bnia"));
    EXPECT_EQ(time::to_string(time::month::september, loc("pl"), txt::width::wide, txt::name_context::standalone), string("wrzesie\xC5\x84"));
    EXPECT_EQ(time::to_string(time::month::september, loc("pl"), txt::width::abbreviated), string("wrz"));
    EXPECT_EQ(time::to_string(time::month::september, loc("pl"), txt::width::narrow), string("w"));
    EXPECT_EQ(time::to_string(time::month::may, loc("ru")), string("\xD0\xBC\xD0\xB0\xD1\x8F"));
    EXPECT_EQ(time::to_string(time::month(13), loc("pl")), string());
    EXPECT_EQ(time::to_string(time::month(0), loc("pl")), string());
    EXPECT_EQ(time::to_string(time::weekday::thursday, loc("pl")), string("czwartek"));
    EXPECT_EQ(time::to_string(time::weekday::thursday, loc("pl"), txt::width::abbreviated), string("czw."));
    EXPECT_EQ(time::to_string(time::weekday::sunday, loc("en"), txt::width::narrow), string("S"));
    EXPECT_EQ(time::to_string(time::weekday(0), loc("en")), string());
    EXPECT_EQ(time::to_string(time::month::september), string("September"));    // the English one stays
}
