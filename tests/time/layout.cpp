//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// time's text: the formats known by name, written and read, against Go
// (tools/time_oracle.go text); the '%' patterns against std::format of
// <chrono>; txt::format of the module's values.
#include "sgcl/time/time.h"
#include "tests/time/text_cases.h"
#include "tests/types.h"

#include <chrono>
#include <format>
#include <map>
#include <random>
#include <string>

namespace {
    using namespace std::chrono_literals;
    using time::date;
    using time::datetime;
    using time::zone;

    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    zone load(const char* name) {
        auto z = zone::load(name);
        EXPECT_TRUE(z) << name;
        return z ? *z : zone::utc();
    }
}

TEST(Layout_Tests, WrittenAsGoWritesThem) {
    sgcl::map<sgcl::string, zone> zones;   // a zone holds a tracked_ptr: a container of the library
    for (const auto& c : oracle::GoWrites) {
        auto [it, fresh] = zones.try_emplace(sgcl::string(c.zone), zone());
        if (fresh) {
            it->second = load(c.zone);
        }
        datetime t = datetime::from_unix_nano(c.ns, it->second);
        ASSERT_EQ(text(t.format(time::rfc3339)), c.rfc3339) << c.zone << " " << c.ns;
        ASSERT_EQ(text(t.format(time::rfc3339_nano)), c.rfc3339_nano) << c.zone << " " << c.ns;
        ASSERT_EQ(text(t.format(time::iso8601)), c.rfc3339_nano) << c.zone << " " << c.ns;
        ASSERT_EQ(text(t.to_string()), c.rfc3339_nano) << c.zone << " " << c.ns;
        ASSERT_EQ(text(t.format(time::http)), c.http) << c.zone << " " << c.ns;
        ASSERT_EQ(text(t.format(time::email)), c.email) << c.zone << " " << c.ns;
    }
}

// Where sgcl parts from Go by design, each difference is put in a class
// (tools/time_oracle.go text lists them) and counted; everything else is
// compared
TEST(Layout_Tests, ReadAsGoReadsThem) {
    std::map<std::string, size_t> classes;
    size_t compared = 0;
    for (const auto& c : oracle::GoReads) {
        time::layout l = c.layout == 'h' ? time::http : c.layout == 'r' ? time::rfc3339 : time::email;
        std::string s = c.text;
        auto r = datetime::parse(c.text, l);
        bool ok = c.ok && !c.outside;
        if (c.outside) {
            EXPECT_FALSE(r) << s;
            EXPECT_NE(text(r.error().message()).find("1677 to 2262"), std::string::npos) << s;
            continue;
        }
        bool same = bool(r) == ok && (!ok || (r->unix_nano() == c.ns && r->offset() == std::chrono::seconds(c.offset)));
        if (same) {
            ++compared;
            continue;
        }
        std::string why;
        if (ok && !r) {
            // sgcl refuses what Go takes
            std::string m = text(r.error().message());
            if (m.find("of two digits expected") != std::string::npos) {
                why = "Go takes one digit";
            } else if (m == "GMT expected") {
                why = "Go takes any zone in RFC 850";
            } else if (m.find("an offset from") == 0 || m.find("an offset of four digits") == 0) {
                why = "Go takes an offset past 23:59 or of three digits";
            } else if (m.find("a zone expected") == 0) {
                why = "Go takes any abbreviation in e-mail";
            } else if (m == "the end of the date expected" && c.layout == 'e') {
                why = "Go takes text after an e-mail date";
            }
        } else if (!ok && r) {
            // sgcl takes what Go refuses
            if (s.find(":60") != std::string::npos) {
                why = "Go refuses a leap second";
            } else if (c.layout == 'r' && (s.find('t') != std::string::npos || s.find('z') != std::string::npos)) {
                why = "Go refuses RFC 3339's small t and z";
            } else if (c.layout == 'e') {
                why = "Go refuses RFC 5322's obsolete forms and dates with less white space";
            }
        } else if (ok && r) {
            // Both take it and read it otherwise
            if ((r->unix_nano() - c.ns) % (int64_t(86400) * 1000000000) == 0 && r->year() != datetime::from_unix_nano(c.ns, zone::utc()).year()) {
                why = "a year of two digits";
            } else if (c.layout == 'e' && c.offset == 0) {
                why = "Go reads a named zone of e-mail as UTC";
            }
        }
        EXPECT_FALSE(why.empty()) << c.layout << " [" << s << "] go " << c.ok << " " << c.ns << " " << c.offset << " sgcl "
                                  << bool(r) << " " << (r ? r->unix_nano() : 0) << (r ? "" : " " + text(r.error().message()));
        ++classes[why];
    }
    EXPECT_GT(compared, 1000u);
    for (auto& [why, n] : classes) {
        std::cout << "  parted from Go by design: " << why << ": " << n << "\n";
    }
}

namespace {
    const char* const DateSpecifiers[] = {
        "%a", "%A", "%b", "%B", "%h", "%C", "%d", "%e", "%D", "%F", "%g", "%G", "%j", "%m", "%U", "%u", "%V",
        "%w", "%W", "%x", "%y", "%Y", "%Ey", "%EY", "%EC", "%Ex", "%Od", "%Oe", "%Om", "%Ou", "%OU", "%OV",
        "%Ow", "%OW", "%Oy", "%n", "%t", "%%", "%Y-%m-%d is %A, week %V of %G"};
    const char* const TimeSpecifiers[] = {
        "%H", "%I", "%M", "%S", "%p", "%r", "%R", "%T", "%X", "%c", "%OH", "%OI", "%OM", "%OS", "%EX", "%Ec",
        "%z", "%Ez", "%Oz", "%Z", "%F %T %Z", "%d.%m.%Y %H:%M"};

    std::string by_std(const char* spec, const auto& value) {
        return std::vformat(std::string("{:") + spec + "}", std::make_format_args(value));
    }

    // Where libc++ writes a negative year its own way and the standard's
    // text says otherwise, named: %G and %EY of -999 to -1 as "-999"
    // (three characters with the sign, where %Y writes "-0999"), %EC of a
    // negative year truncated ("-327" for -32767, where %C floors to
    // -328)
    bool libcxx_differs(const char* spec, int year) {
        std::string s(spec);
        if (year < 0 && year > -1000 && (s.find("%G") != std::string::npos || s == "%EY")) {
            return true;
        }
        return year < 0 && s == "%EC";
    }
}

TEST(Pattern_Tests, ADateAsStdFormatWritesIt) {
    namespace chr = std::chrono;
    size_t compared = 0;
    for (int y = -32767; y <= 32767; y += (y > 1500 && y < 2500) ? 1 : 97) {
        for (int m = 1; m <= 12; m += 5) {
            for (int d : {1, 2, 3, 4, 7, 28, 31}) {
                if (!date::is_valid(y, m, d)) {
                    continue;
                }
                date dt(y, m, d);
                chr::year_month_day ymd{chr::year(y), chr::month(unsigned(m)), chr::day(unsigned(d))};
                for (const char* spec : DateSpecifiers) {
                    if (libcxx_differs(spec, y)) {
                        continue;
                    }
                    ASSERT_EQ(text(dt.format(spec)), by_std(spec, ymd)) << dt << " " << spec;
                    ++compared;
                }
            }
        }
    }
    EXPECT_GT(compared, 100000u);
    // Specifiers of a time of day or of a zone are written as they stand
    EXPECT_EQ(text(date(2026, 9, 24).format("%F %H:%M %Z %Q %")), "2026-09-24 %H:%M %Z %Q %");
    EXPECT_EQ(text(date(-44, 3, 15).format("%Y|%C|%y|%G|%F")), "-0044|-1|44|-0044|-0044-03-15");
}

TEST(Pattern_Tests, ADatetimeAsStdFormatWritesIt) {
    namespace chr = std::chrono;
    std::mt19937_64 rng(20260925);
    zone w = load("Europe/Warsaw");
    size_t compared = 0;
    for (int i = 0; i < 3000; ++i) {
        int64_t ns = int64_t(rng());
        if (i % 3 == 1) {
            ns = ns / 1000000000 * 1000000000;   // whole seconds: the fraction written as zeros
        }
        auto sys = chr::sys_time<chr::nanoseconds>(chr::nanoseconds(ns));
        datetime t = datetime(sys, zone::utc());
        for (const char* spec : DateSpecifiers) {
            ASSERT_EQ(text(t.format(spec)), by_std(spec, sys)) << t << " " << spec;
        }
        for (const char* spec : TimeSpecifiers) {
            ASSERT_EQ(text(t.format(spec)), by_std(spec, sys)) << t << " " << spec;
        }
        // In a zone: the clock is std's local_time at the offset; the
        // zone's own specifiers by hand
        datetime local = t.in(w);
        auto clock = chr::local_time<chr::nanoseconds>(chr::nanoseconds(ns) + chr::nanoseconds(local.offset()));
        for (const char* spec : DateSpecifiers) {
            ASSERT_EQ(text(local.format(spec)), by_std(spec, clock)) << local << " " << spec;
        }
        for (const char* spec : {"%H", "%I", "%M", "%S", "%p", "%r", "%R", "%T", "%X", "%c"}) {
            ASSERT_EQ(text(local.format(spec)), by_std(spec, clock)) << local << " " << spec;
        }
        std::string abbreviation = text(local.abbreviation());
        int64_t off = local.offset().nanoseconds() / 1000000000;
        std::string z = std::format("{}{:02}{:02}", off < 0 ? '-' : '+', std::abs(off) / 3600, std::abs(off) / 60 % 60);
        ASSERT_EQ(text(local.format("%Z %z")), abbreviation + " " + z);
        ASSERT_EQ(text(local.format("%Ez")), z.substr(0, 3) + ":" + z.substr(3));
        compared += std::size(DateSpecifiers) * 2 + std::size(TimeSpecifiers) + 10;
    }
    EXPECT_GT(compared, 100000u);
    // A fixed zone's %Z is its name, UTC's "UTC"
    auto t = date(2026, 9, 24).at(12, 41, 15, zone::fixed(-(3h + 30min)));
    EXPECT_EQ(text(t.format("%Z|%z|%Ez|%Oz")), "-03:30|-0330|-03:30|-03:30");
    EXPECT_EQ(text(t.utc().format("%Z|%z")), "UTC|+0000");
    // What the writer does not know is written as it stands
    EXPECT_EQ(text(t.format("%Q %q %E %Ea %O %Oa 100%")), "%Q %q %E %Ea %O %Oa 100%");
    EXPECT_EQ(text(t.format("")), "");
    // A long pattern: past the room on the stack
    std::string pattern;
    std::string expected;
    for (int i = 0; i < 40; ++i) {
        pattern += "%F %T|";
        expected += "2026-09-24 12:41:15.000000000|";
    }
    EXPECT_EQ(text(t.format(sgcl::string(pattern))), expected);
}

TEST(Pattern_Tests, InTxtFormat) {
    zone w = load("Europe/Warsaw");
    auto t = date(2026, 9, 24).at(12, 41, 15, w) + 122575us;
    EXPECT_EQ(text(txt::format("{}", t)), "2026-09-24T12:41:15.122575+02:00");
    EXPECT_EQ(text(txt::format("{:%H:%M}", t)), "12:41");
    EXPECT_EQ(text(txt::format("{:%d.%m.%Y %H:%M:%S %Z}", t)), "24.09.2026 12:41:15.122575000 CEST");
    EXPECT_EQ(text(txt::format("[{:>8%H:%M}]", t)), "[   12:41]");
    EXPECT_EQ(text(txt::format("[{:*^9%H:%M}]", t)), "[**12:41**]");
    EXPECT_EQ(text(txt::format("[{:<40}]", t)), "[2026-09-24T12:41:15.122575+02:00        ]");
    EXPECT_EQ(text(txt::format("{} {:%A}", t.date(), t.date())), "2026-09-24 Thursday");
    EXPECT_EQ(text(txt::format("[{:>12}]", t.date())), "[  2026-09-24]");
    EXPECT_EQ(text(txt::format("{} {:%a} {:%u}", t.weekday(), t.weekday(), t.weekday())), "Thursday Thu 4");
    EXPECT_EQ(text(txt::format("{} [{:>8}]", sgcl::duration(90min), sgcl::duration(1500ms))), "1h30m0s [    1.5s]");
    EXPECT_EQ(text(txt::format("{:%H:%M}|{}", optional<datetime>(t), optional<datetime>())), "12:41|nullopt");
    sgcl::vector<date> days;
    days.push_back(date(2026, 1, 2));
    days.push_back(date(2026, 3, 4));
    EXPECT_EQ(text(txt::format("{::%d.%m}", days)), "[02.01, 04.03]");
    EXPECT_EQ(text(txt::format("{}", days)), "[2026-01-02, 2026-03-04]");
    // Into a caller's buffer, nothing allocated
    char buffer[64];
    size_t n = txt::format_to(buffer, "{:%a, %d %b %Y %H:%M:%S GMT}", t.utc());
    EXPECT_EQ(std::string(buffer, n), "Thu, 24 Sep 2026 10:41:15.122575000 GMT");
    // A pattern the value does not take is an error of the compiler: asked
    // here of the check the compiler runs
    static_assert(txt::detail::fits<datetime>("{:%H:%M}"));
    static_assert(txt::detail::fits<datetime>("{:>20%F %T %Z}"));
    static_assert(!txt::detail::fits<datetime>("{:%Q}"));
    static_assert(!txt::detail::fits<datetime>("{:%}"));
    static_assert(!txt::detail::fits<datetime>("{:d}"));
    static_assert(!txt::detail::fits<datetime>("{:+%H}"));
    static_assert(!txt::detail::fits<datetime>("{:.3%H}"));
    static_assert(txt::detail::fits<date>("{:%F}"));
    static_assert(!txt::detail::fits<date>("{:%H}"));
    static_assert(!txt::detail::fits<date>("{:%Z}"));
    static_assert(txt::detail::fits<time::weekday>("{:%a}"));
    static_assert(!txt::detail::fits<time::weekday>("{:%F}"));
    static_assert(!txt::detail::fits<time::weekday>("{:d}"));
    // And by a pattern that arrives at run time, an empty answer
    EXPECT_FALSE(txt::format(txt::runtime("{:%Q}"), t));
    EXPECT_EQ(text(*txt::format(txt::runtime("{:%R}"), t)), "12:41");
}

TEST(Layout_Tests, WrittenAndReadBack) {
    std::mt19937_64 rng(7);
    sgcl::vector<zone> zones = {zone::utc(), load("Europe/Warsaw"), load("America/St_Johns"), load("Asia/Kathmandu"), zone::fixed(-(9h + 30min))};
    for (int i = 0; i < 5000; ++i) {
        int64_t ns = int64_t(rng() % 18000000000000000000ull) - int64_t(9000000000000000000);   // 1684 to 2255
        datetime t = datetime::from_unix_nano(ns, zones[i % zones.size()]);
        if (t.offset().nanoseconds() % 60000000000 != 0) {
            // An offset with seconds (a local mean time of the past) is
            // written to the minute, as Go writes it: RFC 3339 has no
            // seconds there, and the text is not the instant
            continue;
        }
        auto back = datetime::parse(t.format(time::rfc3339_nano), time::rfc3339);
        ASSERT_TRUE(back) << t;
        ASSERT_EQ(*back, t);
        ASSERT_EQ(back->offset(), t.offset());
        ASSERT_EQ(back->format(time::rfc3339_nano), t.format(time::rfc3339_nano));
        datetime whole = t.truncate(1s);
        for (time::layout l : {time::rfc3339, time::http, time::email}) {
            auto again = datetime::parse(t.format(l), l);
            ASSERT_TRUE(again) << text(t.format(l));
            ASSERT_EQ(*again, whole) << text(t.format(l));
        }
        ASSERT_EQ(datetime::parse(t.format(time::http), time::http)->zone(), zone::utc());
    }
}

TEST(Layout_Tests, TheDateOfHttp) {
    // The three forms of RFC 9110, one instant
    auto imf = datetime::parse("Sun, 06 Nov 1994 08:49:37 GMT", time::http);
    ASSERT_TRUE(imf);
    EXPECT_EQ(imf->unix(), 784111777);
    EXPECT_EQ(imf->zone(), zone::utc());
    EXPECT_EQ(datetime::parse("Sunday, 06-Nov-94 08:49:37 GMT", time::http), imf);
    EXPECT_EQ(datetime::parse("Sun Nov  6 08:49:37 1994", time::http), imf);
    EXPECT_EQ(datetime::parse("Sun Nov 06 08:49:37 1994", time::http), imf);
    EXPECT_EQ(text(imf->format(time::http)), "Sun, 06 Nov 1994 08:49:37 GMT");
    // Written in GMT whatever the zone
    EXPECT_EQ(text(imf->in(load("Asia/Tokyo")).format(time::http)), "Sun, 06 Nov 1994 08:49:37 GMT");
    // The day of the week is not checked against the date
    EXPECT_EQ(datetime::parse("Fri, 06 Nov 1994 08:49:37 GMT", time::http), imf);
    // Names in any case, GMT as it is
    EXPECT_EQ(datetime::parse("SUN, 06 NOV 1994 08:49:37 GMT", time::http), imf);
    EXPECT_FALSE(datetime::parse("Sun, 06 Nov 1994 08:49:37 gmt", time::http));
    // A year of two digits: the latest with those digits not more than
    // 50 years ahead
    int now = time::now().year();
    auto year_of = [](int yy) {
        auto r = datetime::parse(sgcl::string(std::format("Monday, 01-Jan-{:02} 00:00:00 GMT", yy)), time::http);
        return r ? r->year() : 0;
    };
    EXPECT_EQ(year_of((now + 50) % 100), now + 50);
    EXPECT_EQ(year_of((now + 51) % 100), now + 51 - 100);
    EXPECT_EQ(year_of(now % 100), now);
    // A leap second, only at the end of a day
    EXPECT_EQ(text(datetime::parse("Thu, 31 Dec 1998 23:59:60 GMT", time::http)->format(time::http)), "Fri, 01 Jan 1999 00:00:00 GMT");
    EXPECT_FALSE(datetime::parse("Thu, 31 Dec 1998 12:59:60 GMT", time::http));
    struct Refused {
        const char* text;
        size_t at;
        const char* message;
    };
    for (auto c : std::initializer_list<Refused>{
             {"", 0, "a date of HTTP expected"},
             {"Sun, 06 Nov 1994 08:49:37 UTC", 26, "GMT expected"},
             {"Sun, 06 Nov 1994 08:49:37 GMT ", 29, "the end of the date expected"},
             {"Sun, 6 Nov 1994 08:49:37 GMT", 5, "a day of two digits expected"},
             {"Sun, 31 Feb 1994 08:49:37 GMT", 5, "a day that the month has expected"},
             {"Sun, 06 Nov 1994 24:00:00 GMT", 17, "a time of day from 00:00:00 to 23:59:59 expected"},
             {"Sun, 06 Foo 1994 08:49:37 GMT", 8, "a month expected"},
             {"Sun, 06 Nov 94 08:49:37 GMT", 12, "a year of four digits expected"},
             {"Sundae, 06-Nov-94 08:49:37 GMT", 0, "a date of HTTP expected"},
             {"Sun Nov  6 08:49:37 94", 20, "a year of four digits expected"},
             {"Fri, 11 Apr 2262 23:47:17 GMT", 5, "an instant within the years 1677 to 2262 expected"},
         }) {
        auto r = datetime::parse(c.text, time::http);
        ASSERT_FALSE(r) << c.text;
        EXPECT_EQ(r.error().offset(), c.at) << c.text;
        EXPECT_EQ(text(r.error().message()).find(c.message), 0u) << c.text << ": " << text(r.error().message());
    }
}

TEST(Layout_Tests, Rfc3339ByItsExamples) {
    // RFC 3339 5.8
    auto a = datetime::parse("1985-04-12T23:20:50.52Z", time::rfc3339);
    ASSERT_TRUE(a);
    EXPECT_EQ(a->unix_nano(), 482196050520000000);
    EXPECT_EQ(a->zone(), zone::utc());
    auto b = datetime::parse("1996-12-19T16:39:57-08:00", time::rfc3339);
    ASSERT_TRUE(b);
    EXPECT_EQ(b->unix(), 851042397);
    EXPECT_EQ(b->zone(), zone::fixed(-8h));
    EXPECT_EQ(text(b->to_string()), "1996-12-19T16:39:57-08:00");
    EXPECT_EQ(datetime::parse("1990-12-31T23:59:60Z", time::rfc3339)->unix(), 662688000);
    EXPECT_EQ(datetime::parse("1990-12-31T15:59:60-08:00", time::rfc3339)->unix(), 662688000);
    auto c = datetime::parse("1937-01-01T12:00:27.87+00:20", time::rfc3339);
    ASSERT_TRUE(c);
    EXPECT_EQ(c->unix_nano(), -1041337172130000000);
    EXPECT_EQ(c->offset(), 20min);
    EXPECT_EQ(datetime::parse("2026-09-24t12:41:15z", time::rfc3339), datetime::parse("2026-09-24T12:41:15Z", time::rfc3339));
    EXPECT_EQ(datetime::parse("2026-09-24T12:41:15-00:00", time::rfc3339)->zone(), zone::utc());
    EXPECT_EQ(datetime::parse("2026-09-24T12:41:15.123456789999Z", time::rfc3339)->nanosecond(), 123456789);
    EXPECT_EQ(datetime::parse("2262-04-11T23:47:16.854775807Z", time::rfc3339)->unix_nano(), INT64_MAX);
    EXPECT_EQ(datetime::parse("1677-09-21T00:12:43.145224192Z", time::rfc3339)->unix_nano(), INT64_MIN);
    EXPECT_FALSE(datetime::parse("2262-04-11T23:47:16.854775808Z", time::rfc3339));
    EXPECT_FALSE(datetime::parse("1677-09-21T00:12:43.145224191Z", time::rfc3339));
    for (const char* bad : {"2026-09-24 12:41:15Z", "2026-09-24T12:41:15", "2026-09-24T12:41:15+0200", "2026-09-24T12:41:15+24:00",
                            "2026-09-24T12:41:15.Z", "2026-02-30T00:00:00Z", "1990-12-31T23:59:60+01:00", "10000-01-01T00:00:00Z"}) {
        auto r = datetime::parse(bad, time::rfc3339);
        EXPECT_FALSE(r) << bad;
        EXPECT_LE(r.error().offset(), std::strlen(bad)) << bad;
    }
}

TEST(Layout_Tests, TheDateOfEmail) {
    auto a = datetime::parse("Thu, 24 Sep 2026 12:41:15 +0200", time::email);
    ASSERT_TRUE(a);
    EXPECT_EQ(text(a->to_string()), "2026-09-24T12:41:15+02:00");
    EXPECT_EQ(text(a->format(time::email)), "Thu, 24 Sep 2026 12:41:15 +0200");
    EXPECT_EQ(text(datetime::parse("24 Sep 2026 12:41 EDT", time::email)->to_string()), "2026-09-24T12:41:00-04:00");
    EXPECT_EQ(text(datetime::parse("Thu, 24 Sep 26 12:41:15 (a (nested) comment) GMT", time::email)->to_string()), "2026-09-24T12:41:15Z");
    EXPECT_EQ(text(datetime::parse(" Thu , 4 Sep 2026 12 : 41 : 15 -0000 (x)", time::email)->to_string()), "2026-09-04T12:41:15Z");
    EXPECT_EQ(text(datetime::parse("Thu, 24 Sep 2026 12:41:15\r\n +0200", time::email)->to_string()), "2026-09-24T12:41:15+02:00");
    EXPECT_EQ(datetime::parse("Thu, 24 Sep 99 12:41:15 +0000", time::email)->year(), 1999);
    EXPECT_EQ(datetime::parse("Thu, 24 Sep 49 12:41:15 +0000", time::email)->year(), 2049);
    EXPECT_EQ(datetime::parse("Thu, 24 Sep 126 12:41:15 +0000", time::email)->year(), 2026);
    EXPECT_EQ(datetime::parse("Thu, 24 Sep 2026 12:41:15 A", time::email)->zone(), zone::utc());
    for (const char* bad : {"Thu, 24 Sep 2026 12:41:15 J", "Thu, 24 Sep 2026 12:41:15 (unclosed", "Thu, 24 Sep 2026 12:41:15 +2400",
                            "Thu, 24 Sep 2026 12:41:15", "Thu, 31 Sep 2026 12:41:15 +0200", "Thu, 24 Sep 20266 12:41:15 +0200", ""}) {
        auto r = datetime::parse(bad, time::email);
        EXPECT_FALSE(r) << bad;
        EXPECT_LE(r.error().offset(), std::strlen(bad)) << bad;
    }
}

TEST(Layout_Tests, RandomTextIsReadOrRefusedAndNothingElse) {
    std::mt19937 rng(11);
    const char alphabet[] = "0123456789 :-,.+TtZzGMTSunNovDecThuESTPD()\\\r\n";
    size_t read_ok = 0;
    for (int i = 0; i < 200000; ++i) {
        std::string s;
        int n = int(rng() % 40);
        std::string seed = std::vector<std::string>{"Sun, 06 Nov 1994 08:49:37 GMT", "1985-04-12T23:20:50.52Z", "Thu, 24 Sep 2026 12:41:15 +0200"}[i % 3];
        s = seed.substr(0, rng() % (seed.size() + 1));
        for (int k = 0; k < n % 5; ++k) {
            s += alphabet[rng() % (sizeof alphabet - 1)];
        }
        for (time::layout l : {time::http, time::rfc3339, time::email}) {
            auto r = datetime::parse(sgcl::string(s), l);
            if (r) {
                ++read_ok;
            } else {
                ASSERT_LE(r.error().offset(), s.size()) << s;
                ASSERT_FALSE(r.error().message().empty());
            }
        }
    }
    EXPECT_GT(read_ok, 1000u);
}

// The types of <chrono> in txt::format, against std::format. Two places
// where libc++ is left: a negative duration with a pattern of more than
// one specifier (libc++ writes a '-' before each; the standard and this,
// before the first), and hh_mm_ss of 24 hours or more (libc++ throws)
TEST(Pattern_Tests, TheTypesOfChronoAsStdFormat) {
    namespace chr = std::chrono;
    std::mt19937_64 rng(3);
    auto same = [](const sgcl::string& a, const std::string& b) {
        return std::string(a.data(), a.size()) == b;
    };
    for (int i = 0; i < 2000; ++i) {
        int64_t ns = int64_t(rng());
        auto t = chr::sys_time<chr::nanoseconds>(chr::nanoseconds(ns));
        auto us = chr::floor<chr::microseconds>(t);
        auto ms = chr::floor<chr::milliseconds>(t);
        auto s = chr::floor<chr::seconds>(t);
        auto mi = chr::floor<chr::minutes>(t);
        auto d = chr::floor<chr::days>(t);
        auto l = chr::local_time<chr::seconds>(s.time_since_epoch());
        ASSERT_TRUE(same(txt::format("{}|{}|{}|{}|{}|{}|{}", t, us, ms, s, mi, d, l), std::format("{}|{}|{}|{}|{}|{}|{}", t, us, ms, s, mi, d, l))) << ns;
        ASSERT_TRUE(same(txt::format("{:%c %Z %z %j %U}|{:%T}|{:%F %R}|{:>30%T}", t, ms, l, us), std::format("{:%c %Z %z %j %U}|{:%T}|{:%F %R}|{:>30%T}", t, ms, l, us))) << ns;
        chr::year_month_day ymd(d);
        ASSERT_TRUE(same(txt::format("{} {:%A %d %B %Y} {}", ymd, ymd, chr::weekday(d)), std::format("{} {:%A %d %B %Y} {}", ymd, ymd, chr::weekday(d)))) << ns;
        // Spans of every unit, both signs
        int64_t v = int64_t(rng() % 4000000000) - 2000000000;
        auto check = [&](auto span) {
            ASSERT_EQ(text(txt::format("{}", span)), std::format("{}", span)) << v;
            for (const char* spec : {"%T", "%H", "%M", "%S", "%j", "%Q", "%q", "%R", "%X", "%I", "%p"}) {
                ASSERT_EQ(text(txt::format(txt::runtime(sgcl::string(std::string("{:") + spec + "}")), span).value()), std::vformat(std::string("{:") + spec + "}", std::make_format_args(span))) << v << " " << spec;
            }
            if (span >= decltype(span)::zero()) {
                ASSERT_TRUE(same(txt::format("{:%j days %T, %Q%q}", span), std::format("{:%j days %T, %Q%q}", span))) << v;
            }
        };
        check(chr::nanoseconds(v));
        check(chr::microseconds(v));
        check(chr::milliseconds(v));
        check(chr::seconds(v / 1000));
        check(chr::minutes(v / 100000));
        check(chr::hours(v / 10000000));
        check(chr::duration<int64_t, std::ratio<1, 3>>(v));
        check(chr::duration<int64_t, std::ratio<7>>(v / 1000));
        check(chr::duration<double>(double(v) / 1024));
        check(chr::duration<float, std::milli>(float(v) / 4));
        auto h = chr::hh_mm_ss<chr::nanoseconds>(chr::nanoseconds(v * 40));
        ASSERT_TRUE(same(txt::format("{}|{:%H:%M}", h, h), std::format("{}|{:%H:%M}", h, h))) << v;
    }
    for (unsigned w = 0; w <= 8; ++w) {
        ASSERT_TRUE(same(txt::format("{}", chr::weekday(w)), std::format("{}", chr::weekday(w))));
    }
    chr::year_month_day bad = chr::year(2026) / 2 / 30;
    EXPECT_TRUE(same(txt::format("{}", bad), std::format("{}", bad)));
    // The standard's sign, once
    EXPECT_EQ(text(txt::format("{:%H:%M:%S}", -(90min + 5s))), "-01:30:05");
    static_assert(!txt::detail::fits<chr::seconds>("{:%F}"));
    static_assert(!txt::detail::fits<chr::seconds>("{:.3}"));
    static_assert(!txt::detail::fits<chr::local_seconds>("{:%Z}"));
    static_assert(txt::detail::fits<chr::sys_seconds>("{:%Z}"));
    static_assert(!txt::detail::fits<chr::hh_mm_ss<chr::seconds>>("{:%j}"));
}

// Patterns of '%' read, against Howard Hinnant's date::parse, the
// reference implementation of std::chrono::parse
// (tools/time_parse_oracle.cpp); each difference by design is named and
// counted, everything else compared
#include "tests/time/parse_cases.h"

TEST(Pattern_Tests, ReadAsStdChronoParseReadsThem) {
    std::map<std::string, size_t> classes;
    size_t compared = 0;
    for (const auto& c : oracle::HinnantParse) {
        auto r = datetime::parse(c.text, c.pattern);
        std::string s = c.text;
        if (c.outside) {
            EXPECT_FALSE(r) << c.pattern << " [" << s << "]";
            continue;
        }
        bool same = bool(r) == c.ok && (!c.ok || r->unix_nano() == c.ns);
        if (same) {
            ++compared;
            continue;
        }
        std::string why;
        std::string m = r ? "" : text(r.error().message());
        std::string p = c.pattern;
        if (c.ok && !r && m.find("an instant within the years 1677 to 2262") == 0) {
            why = "a year outside a datetime (Hinnant's nanoseconds wrap round)";
        } else if (c.ok && !r && m == "a week from 1 to 53 expected") {
            why = "Hinnant takes week 0 of a year of weeks";
        } else if (c.ok && !r && m == "a day of the year from 1 to 366 expected") {
            why = "Hinnant takes day 0 of a year";
        } else if (c.ok && !r && m == "an hour from 1 to 12 expected") {
            why = "Hinnant takes hour 0 for %I";
        } else if (c.ok && !r && m == "an offset from -23:59 to +23:59 expected") {
            why = "Hinnant takes an offset past 23:59";
        } else if (!c.ok && r && p.find("%e") != std::string::npos && s.find(' ') != std::string::npos) {
            why = "sgcl reads the space %e writes before one digit";
        } else if (!c.ok && r && p == "%c") {
            why = "sgcl's %c matches no white space where the pattern has one, as strptime";
        } else if (c.ok && !r && m == "the end of the text expected" && s.back() == '.') {
            why = "Hinnant takes a point with no digit after the seconds";
        } else if (c.ok && !r && m.find("a day that the year has") == 0) {
            why = "Hinnant carries day 366 of a common year into the next";
        } else if (!c.ok && r && (s.find(',') != std::string::npos)) {
            why = "sgcl takes a comma before the fraction (ISO 8601's)";
        } else if (!c.ok && r && (p.find("%n") != std::string::npos || p.find("%t") != std::string::npos)) {
            why = "Hinnant refuses %n and %t";
        } else if (!c.ok && r && s.find('.') != std::string::npos && s.size() - s.rfind('.') > 10) {
            why = "sgcl drops the digits of a fraction past the ninth";
        }
        EXPECT_FALSE(why.empty()) << c.pattern << " [" << s << "] hinnant " << c.ok << " " << c.ns << " sgcl " << bool(r) << " "
                                  << (r ? r->unix_nano() : 0) << " " << m;
        ++classes[why];
    }
    EXPECT_GT(compared, 5000u);
    for (auto& [why, n] : classes) {
        std::cout << "  parted from Hinnant's parse by design: " << why << ": " << n << "\n";
    }
}

TEST(Pattern_Tests, ReadInAZone) {
    zone w = load("Europe/Warsaw");
    auto r = datetime::parse("24.09.2026 12:41", "%d.%m.%Y %H:%M", w);
    ASSERT_TRUE(r);
    EXPECT_EQ(text(r->to_string()), "2026-09-24T12:41:00+02:00");
    EXPECT_EQ(r->zone(), w);
    // No zone given: UTC
    EXPECT_EQ(text(datetime::parse("24.09.2026 12:41", "%d.%m.%Y %H:%M")->to_string()), "2026-09-24T12:41:00Z");
    // A skipped time moved on, a time shown twice the first, unless %Z says
    EXPECT_EQ(text(datetime::parse("2026-03-29 02:30", "%F %R", w)->to_string()), "2026-03-29T03:30:00+02:00");
    EXPECT_EQ(text(datetime::parse("2026-10-25 02:30", "%F %R", w)->to_string()), "2026-10-25T02:30:00+02:00");
    EXPECT_EQ(text(datetime::parse("2026-10-25 02:30 CEST", "%F %R %Z", w)->to_string()), "2026-10-25T02:30:00+02:00");
    EXPECT_EQ(text(datetime::parse("2026-10-25 02:30 CET", "%F %R %Z", w)->to_string()), "2026-10-25T02:30:00+01:00");
    EXPECT_FALSE(datetime::parse("2026-07-01 12:00 CET", "%F %R %Z", w));   // not Warsaw's in July
    EXPECT_EQ(datetime::parse("2026-07-01 12:00 UTC", "%F %R %Z", w)->zone(), zone::utc());
    // An offset wins over the zone
    auto o = datetime::parse("2026-09-24 12:41 -0330", "%F %R %z", w);
    ASSERT_TRUE(o);
    EXPECT_EQ(o->zone(), zone::fixed(-(3h + 30min)));
    EXPECT_EQ(o->unix(), date(2026, 9, 24).at(16, 11, zone::utc()).unix());
    // A leap second only in UTC
    EXPECT_EQ(datetime::parse("1998-12-31 23:59:60", "%F %T")->unix(), 915148800);
    EXPECT_FALSE(datetime::parse("1998-12-31 23:59:60", "%F %T", w));
    EXPECT_FALSE(datetime::parse("1998-12-31 12:59:60", "%F %T"));
    // Twelve hours
    EXPECT_EQ(datetime::parse("2026-09-24 12:05 AM", "%F %I:%M %p")->hour(), 0);
    EXPECT_EQ(datetime::parse("2026-09-24 12:05 PM", "%F %I:%M %p")->hour(), 12);
    EXPECT_EQ(datetime::parse("2026-09-24 01:05 pm", "%F %I:%M %p")->hour(), 13);
    EXPECT_FALSE(datetime::parse("2026-09-24 01:05", "%F %I:%M"));
}

TEST(Pattern_Tests, ReadBackWhatIsWritten) {
    std::mt19937_64 rng(5);
    sgcl::vector<zone> zones = {zone::utc(), load("Europe/Warsaw"), load("Asia/Kathmandu"), zone::fixed(-(9h + 30min))};
    const char* const full[] = {"%FT%T%z", "%FT%T%Ez", "%Y%m%d%H%M%S%z", "%a, %d %b %Y %H:%M:%S %z", "%A %e %B %Y %I:%M:%S %p %z",
                                "%G-W%V-%u %T %z", "%j/%Y %T %Oz", "%c %z", "%D %X %Ez"};
    for (int i = 0; i < 3000; ++i) {
        int64_t ns = int64_t(rng() % 7000000000000000000ull) - 2500000000000000000;   // 1890 to 2111
        zone z = zones[i % zones.size()];
        datetime t = datetime::from_unix_nano(ns, z);
        if (t.offset().nanoseconds() % 60000000000 != 0) {
            continue;   // an offset with seconds: %z writes minutes
        }
        for (const char* p : full) {
            sgcl::string written = t.format(p);
            auto back = datetime::parse(written, p);
            ASSERT_TRUE(back) << p << " [" << text(written) << "] " << text(back.error().message());
            // What the pattern carries: %T has the nanoseconds, %c, %X, %r
            // and %S-less patterns the second
            std::string sp(p);
            bool nanos = (sp.find("%T") != std::string::npos || sp.find("%S") != std::string::npos) && sp.find("%X") == std::string::npos;
            datetime expected = nanos ? t : t.truncate(1s);
            if (sp.find("%j") != std::string::npos || sp.find("%G") != std::string::npos || sp.find("%y") != std::string::npos || sp.find("%D") != std::string::npos) {
                // fields that carry the date in other ways; %D's year of two
                // digits by POSIX's pivot
                if (sp.find("%D") != std::string::npos && (t.year() < 1969 || t.year() > 2068)) {
                    continue;
                }
            }
            ASSERT_EQ(*back, expected) << p << " [" << text(written) << "]";
            ASSERT_EQ(back->offset(), t.offset()) << p;
        }
        // In the zone itself, without an offset: the same instant unless the
        // time was shown twice
        sgcl::string plain = t.format("%F %T");
        auto again = datetime::parse(plain, "%F %T", z);
        ASSERT_TRUE(again);
        if (date(t.year(), t.month(), t.day()).try_at(t.hour(), t.minute(), t.second(), z)) {
            ASSERT_EQ(*again, t) << text(plain);
        }
    }
}

TEST(Pattern_Tests, ADateByAPattern) {
    EXPECT_EQ(date::parse("24.09.2026", "%d.%m.%Y"), date(2026, 9, 24));
    EXPECT_EQ(date::parse("September 24, 2026", "%B %d, %Y"), date(2026, 9, 24));
    EXPECT_EQ(date::parse("2026-W39-4", "%G-W%V-%u"), date(2026, 9, 24));
    EXPECT_EQ(date::parse("267/2026", "%j/%Y"), date(2026, 9, 24));
    EXPECT_EQ(date::parse("-0044-03-15", "%Y-%m-%d"), date(-44, 3, 15));
    EXPECT_EQ(date::parse("10000-01-01", "%5Y-%m-%d"), date(10000, 1, 1));
    EXPECT_EQ(date::parse("2026 week 38 day 4", "%Y week %U day %w"), date(2026, 9, 24));
    EXPECT_EQ(date::parse("2026 week 38 day 4", "%Y week %W day %u"), date(2026, 9, 24));
    // %Y reads four digits after a sign, as std::chrono::parse does: a
    // year of five is read by %5Y
    for (int y : {-9999, -1, 0, 1582, 1970, 2026, 9999}) {
        for (int m = 1; m <= 12; ++m) {
            date d(y, m, 13);
            for (const char* p : {"%F", "%Y%m%d", "%d %B %Y", "%A %j %Y", "%G-W%V-%u", "%Y week %U day %w", "%Y week %W day %u"}) {
                auto back = date::parse(d.format(p), p);
                ASSERT_EQ(back, d) << p << " " << d << " " << text(d.format(p));
            }
        }
    }
    struct Refused {
        const char* text;
        const char* pattern;
        const char* message;
        size_t at;
    };
    for (auto c : std::initializer_list<Refused>{
             {"2026-02-30", "%F", "a day that the month has expected", 0},
             {"2026-13-01", "%F", "a month from 1 to 12 expected", 5},
             {"24.09.2026 12", "%d.%m.%Y %H", "a specifier of a time of day or of a zone in the pattern of a date", 11},
             {"09/24", "%m/%d", "a date expected", 0},
             {"Fri 2026-09-24", "%a %F", "a day of the week that is not the date's", 0},
             {"2026-09-24x", "%F", "the end of the text expected", 10},
             {"2026/09/24", "%F", "the text does not follow the pattern", 4},
             {"2026-W53-1", "%G-W%V-%u", "", 0},
             {"2027-W53-1", "%G-W%V-%u", "a week that the year of weeks has expected", 0},
             {"366/2026", "%j/%Y", "a day that the year has expected", 0},
             {"2026", "%Q", "a specifier this reader does not know", 0},
             {"2026", "%Y%", "a pattern that ends with an unfinished specifier", 4},
         }) {
        auto r = date::parse(c.text, c.pattern);
        if (!*c.message) {
            EXPECT_TRUE(r) << c.text;
            continue;
        }
        ASSERT_FALSE(r) << c.text;
        EXPECT_EQ(text(r.error().message()).find(c.message), 0u) << c.text << ": " << text(r.error().message());
        EXPECT_EQ(r.error().offset(), c.at) << c.text;
    }
}

TEST(Pattern_Tests, Iso8601) {
    struct Case {
        const char* text;
        const char* read;
    };
    for (auto c : std::initializer_list<Case>{
             {"2026-09-24", "2026-09-24T00:00:00Z"},
             {"20260924", "2026-09-24T00:00:00Z"},
             {"2026-W39-4", "2026-09-24T00:00:00Z"},
             {"2026W394", "2026-09-24T00:00:00Z"},
             {"2026-267", "2026-09-24T00:00:00Z"},
             {"2026267T12Z", "2026-09-24T12:00:00Z"},
             {"2026-09-24T12", "2026-09-24T12:00:00Z"},
             {"2026-09-24T12:41", "2026-09-24T12:41:00Z"},
             {"2026-09-24T12:41:15", "2026-09-24T12:41:15Z"},
             {"20260924T1241", "2026-09-24T12:41:00Z"},
             {"20260924T124115Z", "2026-09-24T12:41:15Z"},
             {"2026-09-24t12:41:15z", "2026-09-24T12:41:15Z"},
             {"2026-09-24T12:41:15.5", "2026-09-24T12:41:15.5Z"},
             {"2026-09-24T12:41:15,25+02", "2026-09-24T12:41:15.25+02:00"},
             {"2026-09-24T12:41:15+0530", "2026-09-24T12:41:15+05:30"},
             {"2026-09-24T12:41:15-03:30", "2026-09-24T12:41:15-03:30"},
             {"2026-09-24T12.5", "2026-09-24T12:30:00Z"},
             {"2026-09-24T12:41.25", "2026-09-24T12:41:15Z"},
             {"2026-09-24T12,000000000001", "2026-09-24T12:00:00Z"},
             {"2026-09-24T24:00", "2026-09-25T00:00:00Z"},
             {"2026-09-24T24:00:00Z", "2026-09-25T00:00:00Z"},
             {"1990-12-31T23:59:60Z", "1991-01-01T00:00:00Z"},
             {"+2026-09-24T12:00Z", "2026-09-24T12:00:00Z"},
         }) {
        auto r = datetime::parse(c.text, time::iso8601);
        ASSERT_TRUE(r) << c.text << ": " << text(r.error().message());
        EXPECT_EQ(text(r->to_string()), c.read) << c.text;
    }
    for (const char* bad : {"", "2026", "2026-09-24T", "2026-09-24T1", "2026-09-24T12:4", "2026-09-24T1241:15", "2026-09-24T12:41:15+2",
                            "2026-09-24T24:30", "2026-09-24T12:41:15 ", "2026-09-24 12:41", "2026-02-30T00:00", "3000-01-01T00:00Z",
                            "2026-09-24T12:41:15+24:00", "2026-09-24T12:41:15.Z"}) {
        auto r = datetime::parse(bad, time::iso8601);
        EXPECT_FALSE(r) << bad;
        if (!r) {
            EXPECT_LE(r.error().offset(), std::strlen(bad)) << bad;
        }
    }
    // What is written is read
    std::mt19937_64 rng(9);
    for (int i = 0; i < 2000; ++i) {
        datetime t = datetime::from_unix_nano(int64_t(rng() % 7000000000000000000ull) - 2500000000000000000, zone::fixed(std::chrono::minutes(int(rng() % 1400) - 700)));
        auto back = datetime::parse(t.format(time::iso8601), time::iso8601);
        ASSERT_TRUE(back) << t;
        ASSERT_EQ(*back, t);
    }
}

TEST(Pattern_Tests, RandomTextIsReadOrRefusedAndNothingElse) {
    std::mt19937 rng(13);
    const char alphabet[] = "0123456789 :-/.,+TWZAPMaeimnpsuy%GMTSepThu";
    const char* const patterns[] = {"%F %T %z", "%a, %d %b %Y %H:%M:%S %Z", "%G-W%V-%u", "%j/%Y %I:%M %p", "%c", "%D %r", "%Y%m%d"};
    size_t read_ok = 0;
    for (int i = 0; i < 200000; ++i) {
        std::string s;
        int n = int(rng() % 30);
        for (int k = 0; k < n; ++k) {
            s += alphabet[rng() % (sizeof alphabet - 1)];
        }
        if (i % 2) {
            s = std::string("2026-09-24T12:41:15+02:00").substr(0, rng() % 26) + s.substr(0, 3);
        }
        auto r = datetime::parse(sgcl::string(s), sgcl::string(patterns[i % std::size(patterns)]));
        auto iso = datetime::parse(sgcl::string(s), time::iso8601);
        read_ok += bool(r) + bool(iso);
        if (!r) {
            ASSERT_LE(r.error().offset(), s.size()) << s;
        }
        if (!iso) {
            ASSERT_LE(iso.error().offset(), s.size()) << s;
        }
        // A pattern of random text is read or refused as well
        auto any = datetime::parse(sgcl::string(s.substr(0, 8)), sgcl::string(s));
        (void)any;
    }
    EXPECT_GT(read_ok, 100u);
}
