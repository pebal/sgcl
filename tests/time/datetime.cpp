//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// time::datetime and time::now(): the fields in a zone, a time of the clock
// into an instant (the skipped and the repeated times), the calendar's
// arithmetic, truncate and round, the zone's members made of datetimes.
// Against Go (tools/time_oracle.go datetimes) and by hand where Go's
// answer is "not guaranteed" or differs by design.
#include "sgcl/time/time.h"
#include "tests/time/datetime_cases.h"
#include "tests/types.h"

#include <chrono>
#include <climits>
#include <map>
#include <string>
#include <thread>

using namespace sgcl::async;

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

    zone warsaw() {
        return load("Europe/Warsaw");
    }

    datetime utc(int y, int m, int d, int h = 0, int mi = 0, int s = 0) {
        return date(y, m, d).at(h, mi, s, zone::utc());
    }
}

TEST(Datetime_Tests, TheFieldsAsGoReadsThem) {
    sgcl::map<sgcl::string, zone> zones;   // a zone holds a tracked_ptr: a container of the library
    for (const auto& c : oracle::GoFields) {
        auto [it, fresh] = zones.try_emplace(sgcl::string(c.zone), zone());
        if (fresh) {
            it->second = load(c.zone);
        }
        datetime t = datetime::from_unix_nano(c.ns, it->second);
        std::string where = std::string(c.zone) + " " + std::to_string(c.ns);
        ASSERT_EQ(t.year(), c.year) << where;
        ASSERT_EQ(int(t.month()), c.month) << where;
        ASSERT_EQ(t.day(), c.day) << where;
        ASSERT_EQ(t.hour(), c.hour) << where;
        ASSERT_EQ(t.minute(), c.minute) << where;
        ASSERT_EQ(t.second(), c.second) << where;
        ASSERT_EQ(t.nanosecond(), c.nanosecond) << where;
        ASSERT_EQ(int(t.weekday()), c.weekday) << where;
        ASSERT_EQ(t.year_day(), c.year_day) << where;
        ASSERT_EQ(t.iso_week(), (time::iso_week{c.iso_year, c.iso_week})) << where;
        ASSERT_EQ(t.offset(), std::chrono::seconds(c.offset)) << where;
        ASSERT_EQ(text(t.abbreviation()), c.abbreviation) << where;
        ASSERT_EQ(text(t.to_string()), c.rfc3339) << where;
        ASSERT_EQ(t.date(), date(c.year, c.month, c.day)) << where;
        ASSERT_EQ(t.unix_nano(), c.ns);
        ASSERT_EQ(t.unix(), time::detail::floor_div(c.ns, 1000000000)) << where;
        ASSERT_EQ(t.unix_milli(), time::detail::floor_div(c.ns, 1000000)) << where;
    }
}

TEST(Datetime_Tests, ATimeOfTheClockAsGoAndByTheRule) {
    sgcl::map<sgcl::string, zone> zones;   // a zone holds a tracked_ptr: a container of the library
    size_t once = 0, twice = 0, never = 0, go_later = 0, go_forward = 0;
    for (const auto& c : oracle::GoWalls) {
        auto [it, fresh] = zones.try_emplace(sgcl::string(c.zone), zone());
        if (fresh) {
            it->second = load(c.zone);
        }
        zone z = it->second;
        date d(c.year, c.month, c.day);
        std::string where = std::string(c.zone) + " " + std::to_string(c.year) + "-" + std::to_string(c.month) + "-" + std::to_string(c.day) + " "
                          + std::to_string(c.hour) + ":" + std::to_string(c.minute) + ":" + std::to_string(c.second);
        datetime compatible = d.at(c.hour, c.minute, c.second, z);
        datetime first = d.at(c.hour, c.minute, c.second, z, time::earlier);
        datetime last = d.at(c.hour, c.minute, c.second, z, time::later);
        auto exact = d.try_at(c.hour, c.minute, c.second, z);
        ASSERT_EQ(compatible.zone(), z);
        if (c.count == 1) {
            ++once;
            ASSERT_EQ(compatible.unix(), c.first) << where;
            ASSERT_EQ(compatible.unix(), c.go) << where;
            ASSERT_EQ(first, compatible) << where;
            ASSERT_EQ(last, compatible) << where;
            ASSERT_EQ(exact, optional<datetime>(compatible)) << where;
        } else if (c.count == 2) {
            // Shown twice: the first by default and by earlier, the
            // second by later; Go takes one of the two
            ++twice;
            ASSERT_EQ(compatible.unix(), c.first) << where;
            ASSERT_EQ(first.unix(), c.first) << where;
            ASSERT_EQ(last.unix(), c.last) << where;
            ASSERT_FALSE(exact) << where;
            ASSERT_TRUE(c.go == c.first || c.go == c.last) << where;
            go_later += c.go == c.last;
        } else {
            // Skipped: moved on by the skip (compatible, later), or the
            // change itself (earlier), which shows the first time of the
            // clock after the skip
            ++never;
            ASSERT_FALSE(exact) << where;
            ASSERT_EQ(last, compatible) << where;
            auto change = z.previous_transition(compatible + 1ns);
            ASSERT_TRUE(change) << where;
            ASSERT_EQ(first, *change) << where;
            ASSERT_LE(first, compatible) << where;
            // Moved on by the skip: the time of the clock read at the
            // offset before the change
            ASSERT_GT(first.offset(), (first - 1s).offset()) << where;
            ASSERT_EQ(time::detail::wall_seconds(d, c.hour, c.minute, c.second) - (first - 1s).offset().nanoseconds() / 1000000000, compatible.unix()) << where;
            // Go moves it on or back by the skip, by where the time falls
            // against the change read as UTC
            int64_t back = time::detail::wall_seconds(d, c.hour, c.minute, c.second) - first.offset().nanoseconds() / 1000000000;
            ASSERT_TRUE(c.go == compatible.unix() || c.go == back) << where;
            go_forward += c.go == compatible.unix();
        }
    }
    EXPECT_GT(once, 5000u);
    EXPECT_GT(twice, 500u);
    EXPECT_GT(never, 500u);
    // What Go does where it does not promise anything, noted: the later
    // of a time shown twice most of the time (3755 of 5232 of these
    // cases), a skipped time moved on most of the time and back by the
    // skip the rest (618 of 830): by where the time falls against the
    // change read as UTC. sgcl's rule does not depend on that
    EXPECT_GT(go_later, twice / 2);
    EXPECT_LT(go_later, twice);
    EXPECT_GT(go_forward, never / 2);
    EXPECT_LT(go_forward, never);
}

TEST(Datetime_Tests, TheCalendarsArithmeticAsGo) {
    sgcl::map<sgcl::string, zone> zones;   // a zone holds a tracked_ptr: a container of the library
    size_t n = 0;
    for (const auto& c : oracle::GoAddDates) {
        auto [it, fresh] = zones.try_emplace(sgcl::string(c.zone), zone());
        if (fresh) {
            it->second = load(c.zone);
        }
        datetime t = datetime::from_unix_nano(c.ns, it->second);
        datetime u = c.years ? t.add_years(c.years) : c.months ? t.add_months(c.months) : t.add_days(c.days);
        ASSERT_EQ(u.unix_nano(), c.result) << c.zone << " " << c.ns << " " << c.years << " " << c.months << " " << c.days;
        ASSERT_EQ(u.zone(), t.zone());
        ++n;
    }
    EXPECT_GT(n, 1500u);
}

TEST(Datetime_Tests, TruncateAndRoundAsGo) {
    for (const auto& c : oracle::GoSteps) {
        datetime t = datetime::from_unix_nano(c.ns, zone::utc());
        duration step = std::chrono::nanoseconds(c.step);
        ASSERT_EQ(t.truncate(step).unix_nano(), c.truncated) << c.ns << " " << c.step;
        ASSERT_EQ(t.round(step).unix_nano(), c.rounded) << c.ns << " " << c.step;
        // In any zone the same: steps are counted from Go's zero time, not
        // from the zone's midnight
        ASSERT_EQ(t.in(zone::fixed(5h + 45min)).truncate(step).unix_nano(), c.truncated);
    }
}

TEST(Datetime_Tests, TheDifferenceAsGo) {
    for (const auto& c : oracle::GoSub) {
        auto a = datetime::from_unix_nano(c.a, zone::utc());
        auto b = datetime::from_unix_nano(c.b, zone::utc());
        ASSERT_EQ((a - b).nanoseconds(), c.difference) << c.a << " " << c.b;   // saturated, as Go's Sub is
    }
}

TEST(Datetime_Tests, SkippedAndRepeatedByHand) {
    zone w = warsaw();
    // Spring: 02:00 CET becomes 03:00 CEST on 2026-03-29
    EXPECT_EQ(text(date(2026, 3, 29).at(2, 30, w).to_string()), "2026-03-29T03:30:00+02:00");
    EXPECT_EQ(text(date(2026, 3, 29).at(2, 30, w, time::later).to_string()), "2026-03-29T03:30:00+02:00");
    EXPECT_EQ(text(date(2026, 3, 29).at(2, 30, w, time::earlier).to_string()), "2026-03-29T03:00:00+02:00");
    EXPECT_EQ(text(date(2026, 3, 29).at(2, 0, w).to_string()), "2026-03-29T03:00:00+02:00");
    EXPECT_EQ(text(date(2026, 3, 29).at(1, 59, 59, w).to_string()), "2026-03-29T01:59:59+01:00");
    EXPECT_EQ(text(date(2026, 3, 29).at(3, 0, w).to_string()), "2026-03-29T03:00:00+02:00");
    EXPECT_FALSE(date(2026, 3, 29).try_at(2, 30, 0, w));
    // Autumn: 03:00 CEST becomes 02:00 CET on 2026-10-25
    EXPECT_EQ(text(date(2026, 10, 25).at(2, 30, w).to_string()), "2026-10-25T02:30:00+02:00");
    EXPECT_EQ(text(date(2026, 10, 25).at(2, 30, w, time::earlier).to_string()), "2026-10-25T02:30:00+02:00");
    EXPECT_EQ(text(date(2026, 10, 25).at(2, 30, w, time::later).to_string()), "2026-10-25T02:30:00+01:00");
    EXPECT_EQ(text(date(2026, 10, 25).at(2, 0, w, time::later).to_string()), "2026-10-25T02:00:00+01:00");
    EXPECT_EQ(text(date(2026, 10, 25).at(3, 0, w).to_string()), "2026-10-25T03:00:00+01:00");
    EXPECT_FALSE(date(2026, 10, 25).try_at(2, 30, 0, w));
    EXPECT_TRUE(date(2026, 10, 25).try_at(3, 0, 0, w));
    // Half an hour of DST (Lord Howe), two hours (Troll)
    zone lh = load("Australia/Lord_Howe");
    EXPECT_EQ(text(date(2026, 10, 4).at(2, 15, lh).to_string()), "2026-10-04T02:45:00+11:00");
    EXPECT_EQ(text(date(2026, 4, 5).at(1, 45, lh, time::later).to_string()), "2026-04-05T01:45:00+10:30");
    zone troll = load("Antarctica/Troll");
    EXPECT_EQ(text(date(2026, 3, 29).at(1, 30, troll).to_string()), "2026-03-29T03:30:00+02:00");
    // A skipped midnight: the day starts at the change
    zone santiago = load("America/Santiago");
    auto sd = date(2026, 9, 6).start_of_day(santiago);
    EXPECT_EQ(text(sd.to_string()), "2026-09-06T01:00:00-03:00");
    EXPECT_EQ(sd.date(), date(2026, 9, 6));
    EXPECT_EQ(text(date(2026, 9, 6).at(0, 0, santiago).to_string()), "2026-09-06T01:00:00-03:00");
    EXPECT_EQ(text(date(2026, 4, 5).start_of_day(santiago).to_string()), "2026-04-05T00:00:00-04:00");   // midnight at -03 went back to 23:00
    EXPECT_EQ(text(date(2026, 4, 4).at(23, 30, santiago).to_string()), "2026-04-04T23:30:00-03:00");    // shown twice: the first
    EXPECT_EQ(text(date(2026, 4, 4).at(23, 30, santiago, time::later).to_string()), "2026-04-04T23:30:00-04:00");
    // A day skipped whole: Samoa went from 2011-12-29 to 2011-12-31
    zone apia = load("Pacific/Apia");
    EXPECT_EQ(text(date(2011, 12, 30).at(12, 0, apia).to_string()), "2011-12-31T12:00:00+14:00");
    EXPECT_EQ(text(date(2011, 12, 30).start_of_day(apia).to_string()), "2011-12-31T00:00:00+14:00");
    EXPECT_EQ(text(date(2026, 9, 25).start_of_day(w).to_string()), "2026-09-25T00:00:00+02:00");
    EXPECT_EQ(text(date(2026, 9, 25).at(12, 0, w).start_of_day().to_string()), "2026-09-25T00:00:00+02:00");
    // Out of range carries
    EXPECT_EQ(date(2026, 9, 25).at(24, 0, zone::utc()), utc(2026, 9, 26));
    EXPECT_EQ(date(2026, 9, 25).at(-1, 0, zone::utc()), utc(2026, 9, 24, 23));
    EXPECT_EQ(date(2026, 9, 25).at(0, 0, 86400 * 3, zone::utc()), utc(2026, 9, 28));
}

TEST(Datetime_Tests, ADayIsNotAlways24Hours) {
    zone w = warsaw();
    auto before = date(2026, 10, 24).at(12, 0, w);
    EXPECT_EQ(before.add_days(1) - before, 25h);
    EXPECT_EQ(date(2026, 3, 28).at(12, 0, w).add_days(1) - date(2026, 3, 28).at(12, 0, w), 23h);
    EXPECT_EQ(before + 24h, date(2026, 10, 25).at(11, 0, w));
    // The time of the clock kept, with the part of a second
    auto t = date(2026, 1, 31).at(10, 30, 15, w) + 123456789ns;
    auto m = t.add_months(1);
    EXPECT_EQ(text(m.to_string()), "2026-02-28T10:30:15.123456789+01:00");
    EXPECT_EQ(text(t.add_years(-2).add_months(1).to_string()), "2024-02-29T10:30:15.123456789+01:00");
    EXPECT_EQ(text(date(2024, 2, 29).at(0, 0, w).add_years(1).to_string()), "2025-02-28T00:00:00+01:00");
    // Into a skipped time: moved on
    EXPECT_EQ(text(date(2026, 3, 28).at(2, 30, w).add_days(1).to_string()), "2026-03-29T03:30:00+02:00");
    // Into a time shown twice: the first
    EXPECT_EQ(text(date(2026, 10, 24).at(2, 30, w).add_days(1).to_string()), "2026-10-25T02:30:00+02:00");
}

TEST(Datetime_Tests, TheZonesMembers) {
    zone w = warsaw();
    auto summer = date(2026, 9, 24).at(12, 41, w);
    EXPECT_EQ(w.offset_at(summer), 2h);
    EXPECT_EQ(text(w.abbreviation_at(summer)), "CEST");
    EXPECT_TRUE(w.is_dst_at(summer));
    EXPECT_EQ(summer.offset(), 2h);
    EXPECT_TRUE(summer.is_dst());
    auto next = w.next_transition(summer);
    ASSERT_TRUE(next);
    EXPECT_EQ(text(next->to_string()), "2026-10-25T02:00:00+01:00");
    EXPECT_EQ(next->zone(), w);
    // Strictly after and strictly before, to the nanosecond
    EXPECT_EQ(w.next_transition(*next - 1ns), next);
    EXPECT_NE(w.next_transition(*next), next);
    EXPECT_EQ(w.previous_transition(*next + 1ns), next);
    EXPECT_NE(w.previous_transition(*next), next);
    EXPECT_EQ(text(w.previous_transition(*next)->to_string()), "2026-03-29T03:00:00+02:00");
    // Past 2100 the rule
    auto far = date(2200, 7, 1).at(12, 0, w);
    EXPECT_EQ(text(far.abbreviation()), "CEST");
    EXPECT_EQ(text(w.next_transition(far)->to_string()), "2200-10-26T02:00:00+01:00");
    EXPECT_EQ(text(w.previous_transition(far)->to_string()), "2200-03-30T03:00:00+02:00");
    // Before the first: Warsaw's local mean time, +01:24
    auto old = datetime::from_unix(-5364662400, w);
    EXPECT_EQ(text(old.abbreviation()), "LMT");
    EXPECT_EQ(old.offset(), 1h + 24min);
    EXPECT_FALSE(w.previous_transition(old));
    EXPECT_FALSE(zone::utc().next_transition(summer));
    EXPECT_FALSE(zone::fixed(3h).previous_transition(summer));
    EXPECT_EQ(zone::fixed(-(3h + 30min)).offset_at(summer), -(3h + 30min));
    EXPECT_EQ(text(zone::fixed(-(3h + 30min)).abbreviation_at(summer)), "-03:30");
    EXPECT_EQ(text(summer.in(zone::fixed(5h + 45min)).to_string()), "2026-09-24T16:26:00+05:45");
    // A zone that stopped changing
    zone tokyo = load("Asia/Tokyo");
    EXPECT_FALSE(tokyo.next_transition(summer));
    EXPECT_EQ(text(tokyo.previous_transition(summer)->to_string()), "1951-09-09T00:00:00+09:00");
}

TEST(Datetime_Tests, TheInstant) {
    zone w = warsaw();
    auto a = date(2026, 9, 24).at(12, 41, 15, w);
    auto b = a.utc();
    EXPECT_EQ(a, b);
    EXPECT_NE(a.zone(), b.zone());
    EXPECT_EQ(b.hour(), 10);
    EXPECT_EQ(a.in(w), a);
    EXPECT_LT(a, a + 1ns);
    EXPECT_GT(a, a - 1ns);
    EXPECT_EQ((a + 90min) - a, 90min);
    EXPECT_EQ(1h + a, a + 1h);
    auto c = a;
    c += 1h;
    c -= 30min;
    EXPECT_EQ(c - a, 30min);
    EXPECT_EQ(a.local().zone(), zone::local());
    EXPECT_EQ(datetime(), datetime::from_unix(0, zone::utc()));
    EXPECT_EQ(datetime().zone(), zone::utc());
    EXPECT_EQ(text(datetime().to_string()), "1970-01-01T00:00:00Z");
    // Before 1970 the division goes down
    auto half = datetime::from_unix_nano(-500000000, zone::utc());
    EXPECT_EQ(text(half.to_string()), "1969-12-31T23:59:59.5Z");
    EXPECT_EQ(half.unix(), -1);
    EXPECT_EQ(half.unix_milli(), -500);
    EXPECT_EQ(half.second(), 59);
    EXPECT_EQ(half.nanosecond(), 500000000);
    EXPECT_EQ(datetime::from_unix_milli(-1, zone::utc()).unix_nano(), -1000000);
    EXPECT_EQ(half.unix_micro(), -500000);
    EXPECT_EQ(datetime::from_unix_micro(-1, zone::utc()).unix_nano(), -1000);
    EXPECT_EQ(datetime::from_unix_micro(1'500'000, zone::utc()).unix_milli(), 1500);
    EXPECT_EQ(datetime::from_unix_milli(-1500, zone::utc()).unix_milli(), -1500);
    // From the system clock and io's file times, which are the same type
    auto sys = std::chrono::sys_time<std::chrono::nanoseconds>(1790000000123456789ns);
    EXPECT_EQ(datetime(sys, zone::utc()).unix_nano(), 1790000000123456789);
    EXPECT_EQ(datetime(sys).zone(), zone::local());
    EXPECT_EQ(datetime(sys, w).to_sys(), sys);
    io::file_time ft = sys;
    EXPECT_EQ(datetime(ft, w).unix_nano(), 1790000000123456789);
    datetime from_seconds(std::chrono::sys_seconds(1790000000s), w);
    EXPECT_EQ(from_seconds.unix(), 1790000000);
    static_assert(sizeof(datetime) == 16);
    // not trivially copyable: it holds its zone's tracked_ptr (DESIGN 219)
}

TEST(Datetime_Tests, TheEndsOfTheRange) {
    auto lo = datetime::from_unix_nano(INT64_MIN, zone::utc());
    auto hi = datetime::from_unix_nano(INT64_MAX, zone::utc());
    EXPECT_EQ(text(lo.to_string()), "1677-09-21T00:12:43.145224192Z");
    EXPECT_EQ(text(hi.to_string()), "2262-04-11T23:47:16.854775807Z");
    EXPECT_EQ(hi + 1ns, hi);
    EXPECT_EQ(lo - 1ns, lo);
    EXPECT_EQ(hi + duration::max(), hi);
    EXPECT_EQ(lo + duration::min(), lo);
    EXPECT_EQ(hi - lo, duration::max());
    EXPECT_EQ(lo - hi, duration::min());
    EXPECT_EQ(datetime::from_unix(INT64_MAX / 1000, zone::utc()), hi);
    EXPECT_EQ(datetime::from_unix(-10000000000000, zone::utc()), lo);
    EXPECT_EQ(datetime::from_unix_milli(INT64_MIN, zone::utc()), lo);
    EXPECT_EQ(hi.round(1h), hi);
    EXPECT_EQ(date(3000, 1, 1).at(0, 0, zone::utc()), hi);
    EXPECT_EQ(date(1000, 1, 1).at(0, 0, zone::utc()), lo);
    EXPECT_EQ(hi.add_years(1), hi);
    EXPECT_EQ(hi.in(warsaw()).year(), 2262);
    EXPECT_EQ(lo.in(warsaw()).year(), 1677);
    EXPECT_EQ(text(lo.in(warsaw()).abbreviation()), "LMT");
    EXPECT_EQ(text(datetime::from_unix(2147483647, zone::utc()).to_string()), "2038-01-19T03:14:07Z");
    EXPECT_EQ(text(datetime::from_unix(2147483648, zone::utc()).to_string()), "2038-01-19T03:14:08Z");
    EXPECT_EQ(text(datetime::from_unix(4294967296, zone::utc()).to_string()), "2106-02-07T06:28:16Z");
}

TEST(Datetime_Tests, NowFollowsTheManualClock) {
    auto sys = [] { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); };
    int64_t before = sys();
    auto t = time::now();
    int64_t after = sys();
    EXPECT_LE(before, t.unix_nano());
    EXPECT_LE(t.unix_nano(), after);
    EXPECT_EQ(t.zone(), zone::local());
    {
        manual_clock clock;
        clock.install();
        auto a = time::now();
        EXPECT_LE(before, a.unix_nano());
        std::this_thread::sleep_for(2ms);
        EXPECT_EQ(time::now(), a);   // stands still
        clock.advance(24h + 1s);
        EXPECT_EQ(time::now() - a, 24h + 1s);
        clock.advance(500ms);
        EXPECT_EQ(time::now() - a, 24h + 1500ms);
    }
    int64_t s = sys();
    EXPECT_LE(time::now().unix_nano() - s, int64_t(1000000000));   // the system's again
}
