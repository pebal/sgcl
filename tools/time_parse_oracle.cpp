// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for reading patterns of '%' (datetime::parse(text, pattern),
// date::parse(text, pattern)): Howard Hinnant's date library, the
// reference implementation of std::chrono::parse (libc++ has none), asked
// the same questions and the answers written out as a C++ header. Only in
// this tool, never in the library. From the root of the tree, with the
// library checked out in ~/Programming/oracles/date:
//
//	clang++ -std=c++20 -O1 -DONLY_C_LOCALE=1 -I ~/Programming/oracles/date/include \
//	    tools/time_parse_oracle.cpp -o /tmp/time_parse_oracle && /tmp/time_parse_oracle > tests/time/parse_cases.h
//
// What is asked: date::parse(pattern, sys_time<nanoseconds>) of each text
// (and with the abbreviation and offset read), the text read to its end
// (sgcl refuses what is left over; a stream leaves it). The texts: every
// pattern of Patterns written by std::format at instants drawn from 1700
// to 2200 (what a writer writes a reader reads back), then each with one
// byte changed, dropped or its case turned, and the hand-made texts of
// HandCases, each named for what it tries. Not asked, because sgcl reads
// them otherwise by design: %Z (sgcl maps UTC and GMT to UTC and an
// abbreviation to the zone given; Hinnant's parse keeps it as text), a
// second of 60 (sgcl takes 23:59:60 in UTC).
#include "date/date.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <format>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {
    const char* const Patterns[] = {
        "%Y-%m-%d", "%F %T", "%d.%m.%Y %H:%M", "%FT%T%z", "%FT%T%Ez", "%a, %d %b %Y %H:%M:%S", "%A %e %B %Y",
        "%D %r", "%c", "%x %X", "%j/%Y", "%G-W%V-%u", "%Y%m%d%H%M%S", "%m/%d/%y %I:%M %p", "%C%y-%m-%d",
        "%Y week %U day %w", "%Y week %W day %u", "%F%n%T", "%F%t%R", "%%%F%%",
    };

    struct Hand {
        const char* pattern;
        const char* text;
        const char* why;
    };

    const Hand HandCases[] = {
        {"%Y-%m-%d", "2026-9-4", "single digits"},
        {"%Y-%m-%d", "26-09-04", "a short year"},
        {"%Y-%m-%d", "-044-03-15", "a negative year"},
        {"%Y-%m-%d", "2026-02-30", "a day the month has not"},
        {"%Y-%m-%d", "2026-13-01", "a month 13"},
        {"%Y-%m-%d", "2026-00-01", "a month 0"},
        {"%Y-%m-%d", "02026-09-24", "five digits to a year of four"},
        {"%5Y-%m-%d", "02026-09-24", "a width of five"},
        {"%Y%m%d", "20260924", "digits with nothing between"},
        {"%Y %m %d", "2026    09\t24", "white space matching any"},
        {"%Y %m %d", "20260924", "white space matching none"},
        {"%Y%n%m", "2026 09", "%n one white space"},
        {"%Y%n%m", "202609", "%n and none"},
        {"%Y%t%m", "202609", "%t and none"},
        {"%Y%t%m", "2026  09", "%t and two"},
        {"%d %b %Y", "24 sep 2026", "a month in small letters"},
        {"%d %b %Y", "24 SEPTEMBER 2026", "a long month for %b"},
        {"%d %B %Y", "24 Sep 2026", "a short month for %B"},
        {"%a %F", "Thu 2026-09-24", "the day of the week"},
        {"%a %F", "Fri 2026-09-24", "the wrong day of the week"},
        {"%A %F", "thursday 2026-09-24", "a long day in small letters"},
        {"%F %I:%M %p", "2026-09-24 12:00 AM", "midnight"},
        {"%F %I:%M %p", "2026-09-24 12:00 PM", "noon"},
        {"%F %I:%M %p", "2026-09-24 12:00 pm", "pm in small letters"},
        {"%F %I:%M", "2026-09-24 11:00", "%I with no %p"},
        {"%F %H:%M", "2026-09-24 24:00", "hour 24"},
        {"%F %H:%M:%S", "2026-09-24 12:41:15.123456789", "a fraction"},
        {"%F %H:%M:%S", "2026-09-24 12:41:15,5", "a fraction after a comma"},
        {"%F %H:%M:%S", "2026-09-24 12:41:15.1234567891", "ten digits of fraction"},
        {"%F %H:%M:%S", "2026-09-24 12:41:15.", "a point and no digit"},
        {"%F %T %z", "2026-09-24 12:41:15 +0200", "an offset"},
        {"%F %T %z", "2026-09-24 12:41:15 +02", "an offset of hours"},
        {"%F %T %z", "2026-09-24 12:41:15 +02:00", "a colon for %z"},
        {"%F %T %Ez", "2026-09-24 12:41:15 +02:00", "a colon for %Ez"},
        {"%F %T %Ez", "2026-09-24 12:41:15 +2", "one digit of hours for %Ez"},
        {"%F %T %z", "2026-09-24 12:41:15 -1130", "a negative offset"},
        {"%F %T %z", "2026-09-24 12:41:15 Z", "Z for %z"},
        {"%j %Y", "366 2024", "day 366 of a leap year"},
        {"%j %Y", "366 2026", "day 366 of a common year"},
        {"%G-W%V-%u", "2026-W53-1", "week 53 of a year of 52"},
        {"%G-W%V-%u", "2020-W53-7", "week 53 of a year of 53"},
        {"%G-W%V-%u", "2024-W01-1", "week 1 in the year before"},
        {"%y", "26", "a year alone"},
        {"%y-%m-%d", "69-01-01", "the pivot: 69"},
        {"%y-%m-%d", "68-01-01", "the pivot: 68"},
        {"%C%y-%m-%d", "1968-01-01", "a century and a short year"},
        {"%m/%d", "09/24", "no year"},
        {"%H:%M", "12:41", "no date"},
        {"%F", "2026-09-24 ", "a space after"},
        {"%F", " 2026-09-24", "a space before"},
        {"%F%%", "2026-09-24%", "a percent"},
        {"%e.%m.%Y", " 4.09.2026", "a space for %e"},
        {"%d.%m.%Y", " 4.09.2026", "a space for %d"},
        {"%Y week %U day %w", "2026 week 00 day 4", "week 0 by Sundays"},
        {"%Y week %W day %u", "2026 week 53 day 7", "a week past the year"},
        {"%Q", "12", "a specifier no reader knows"},
    };

    std::string quote(const std::string& s) {
        std::string out = "\"";
        for (char c : s) {
            if (c == '"' || c == '\\') {
                out += '\\';
                out += c;
            } else if (c == '\n') {
                out += "\\n";
            } else if (c == '\t') {
                out += "\\t";
            } else {
                out += c;
            }
        }
        return out + "\"";
    }

    void ask(const std::string& pattern, const std::string& text) {
        std::istringstream in(text);
        date::sys_time<std::chrono::nanoseconds> t{};
        in >> date::parse(pattern, t);
        bool ok = !in.fail() && in.peek() == std::char_traits<char>::eof();
        // A date read outside 1678 to 2261 does not fit the nanoseconds of
        // sys_time (nor a datetime): read again as a date, and marked
        bool outside = false;
        if (ok) {
            std::istringstream again(text);
            date::year_month_day ymd{};
            again >> date::parse(pattern, ymd);
            if (!again.fail() && ymd.ok() && (int(ymd.year()) < 1678 || int(ymd.year()) > 2261)) {
                outside = true;
            }
        }
        std::printf("        {%s, %s, %s, %lld, %s},\n", quote(pattern).c_str(), quote(text).c_str(), ok ? "true" : "false",
                    ok && !outside ? (long long)t.time_since_epoch().count() : 0LL, outside ? "true" : "false");
    }
}

int main() {
    std::mt19937_64 rng(20260925);
    std::puts("//------------------------------------------------------------------------------");
    std::puts("// SGCL: a C++20 application framework");
    std::puts("// Copyright (c) 2022-2026 Sebastian Nibisz");
    std::puts("// SPDX-License-Identifier: Apache-2.0");
    std::puts("//------------------------------------------------------------------------------");
    std::puts("#pragma once");
    std::puts("");
    std::puts("// Generated by tools/time_parse_oracle.cpp: do not edit.");
    std::puts("//");
    std::puts("// What Howard Hinnant's date::parse reads of patterns of '%'; the");
    std::puts("// program says what is asked.");
    std::puts("");
    std::puts("namespace oracle {");
    std::puts("    struct ParseCase {");
    std::puts("        const char* pattern;");
    std::puts("        const char* text;");
    std::puts("        bool ok;");
    std::puts("        long long ns;            // the instant, UTC, since 1970");
    std::puts("        bool outside;            // read, but outside the years 1678 to 2261");
    std::puts("    };");
    std::puts("");
    std::puts("    inline constexpr ParseCase HinnantParse[] = {");
    for (auto& h : HandCases) {
        ask(h.pattern, h.text);
    }
    const char* alphabet = "0123456789 :-/.,+TWZAPMaeimnpsuy%";
    for (int i = 0; i < 300; ++i) {
        int64_t s = int64_t(rng() % (7258118400ull + 8520336000ull)) - 8520336000ll;   // 1700 to 2200
        auto t = std::chrono::sys_time<std::chrono::nanoseconds>(std::chrono::seconds(s) + std::chrono::nanoseconds(rng() % 1000000000));
        for (const char* p : Patterns) {
            std::string text = std::vformat(std::string("{:") + p + "}", std::make_format_args(t));
            ask(p, text);
            if (i % 3 == 0 && !text.empty()) {
                std::string m = text;
                size_t k = rng() % m.size();
                switch (rng() % 3) {
                case 0:
                    m[k] = alphabet[rng() % 33];
                    break;
                case 1:
                    m.erase(k, 1);
                    break;
                default:
                    m[k] = char(std::isupper((unsigned char)m[k]) ? std::tolower((unsigned char)m[k]) : std::toupper((unsigned char)m[k]));
                }
                ask(p, m);
            }
        }
    }
    std::puts("    };");
    std::puts("}");
}
