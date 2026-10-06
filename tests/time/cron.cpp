//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// time::cron: the syntax (every form of every field, the names, Quartz's
// ? L W #, the errors with their bytes), the next times against cases
// worked out by hand, in UTC and across the changes of the clock of
// Europe/Warsaw and America/New_York in 2026 by ISC cron's rule (fixed
// times once, at the jump of a skipped hour and the first pass of a
// repeated one; wildcards at every instant that matches), matches(),
// next(after, n), an expression that never fires, the end of the range,
// equality and the text; and time::every under a manual clock: the calls
// at the times, a time held while f runs, the stop.
#include "tests/types.h"

#include <atomic>
#include <chrono>
#include <string>

namespace {
    using namespace std::chrono_literals;
    namespace async = sgcl::async;

    time::datetime at(const char* text) {
        return time::datetime::parse(text, time::rfc3339).value();
    }

    // The next n times as "YYYY-MM-DD HH:MM:SS ZZZ", space-separated
    std::string next_of(const char* expression, const char* zone, const char* after, size_t n) {
        auto c = time::cron::parse(expression, time::zone(zone));
        if (!c) {
            return std::string("error: ") + c.error().message().data();
        }
        std::string out;
        for (auto& t : c->next(at(after), n)) {
            if (!out.empty()) {
                out += " | ";
            }
            out += t.format("%F %X %Z").data();
        }
        return out;
    }

    size_t error_offset(const char* expression) {
        auto c = time::cron::parse(expression, time::zone::utc());
        return c ? SIZE_MAX : c.error().offset();
    }
}

// The plain fields: numbers, ranges, steps, lists, names, 6 fields
TEST(Cron_Test, TheFields) {
    EXPECT_EQ(next_of("0 9 * * MON-FRI", "UTC", "2026-10-02T10:00:00Z", 3),
              "2026-10-05 09:00:00 UTC | 2026-10-06 09:00:00 UTC | 2026-10-07 09:00:00 UTC");
    EXPECT_EQ(next_of("*/20 1-2 * * *", "UTC", "2026-10-06T00:00:00Z", 4),
              "2026-10-06 01:00:00 UTC | 2026-10-06 01:20:00 UTC | 2026-10-06 01:40:00 UTC | 2026-10-06 02:00:00 UTC");
    EXPECT_EQ(next_of("5,10-12/2,50/5 * * * *", "UTC", "2026-10-06T00:00:00Z", 6),
              "2026-10-06 00:05:00 UTC | 2026-10-06 00:10:00 UTC | 2026-10-06 00:12:00 UTC | 2026-10-06 00:50:00 UTC | 2026-10-06 00:55:00 UTC | 2026-10-06 01:05:00 UTC");
    EXPECT_EQ(next_of("0 0 1 jan,Jul *", "UTC", "2026-03-01T00:00:00Z", 2),
              "2026-07-01 00:00:00 UTC | 2027-01-01 00:00:00 UTC");
    EXPECT_EQ(next_of("0 0 * * 7", "UTC", "2026-10-06T00:00:00Z", 1), "2026-10-11 00:00:00 UTC");   // 7 is Sunday
    EXPECT_EQ(next_of("0 0 * * 5-7", "UTC", "2026-10-06T00:00:00Z", 3),
              "2026-10-09 00:00:00 UTC | 2026-10-10 00:00:00 UTC | 2026-10-11 00:00:00 UTC");
    EXPECT_EQ(next_of("*/15 */2 * * * *", "UTC", "2026-10-06T00:00:00Z", 5),
              "2026-10-06 00:00:15 UTC | 2026-10-06 00:00:30 UTC | 2026-10-06 00:00:45 UTC | 2026-10-06 00:02:00 UTC | 2026-10-06 00:02:15 UTC");
    EXPECT_EQ(next_of("  0\t0  *   * *  ", "UTC", "2026-10-06T00:00:00Z", 1), "2026-10-07 00:00:00 UTC");   // white space around and between
}

// The names of a whole expression
TEST(Cron_Test, TheNames) {
    EXPECT_EQ(next_of("@yearly", "UTC", "2026-10-06T00:00:00Z", 1), "2027-01-01 00:00:00 UTC");
    EXPECT_EQ(next_of("@annually", "UTC", "2026-10-06T00:00:00Z", 1), "2027-01-01 00:00:00 UTC");
    EXPECT_EQ(next_of("@monthly", "UTC", "2026-10-06T00:00:00Z", 1), "2026-11-01 00:00:00 UTC");
    EXPECT_EQ(next_of("@weekly", "UTC", "2026-10-06T00:00:00Z", 1), "2026-10-11 00:00:00 UTC");
    EXPECT_EQ(next_of("@daily", "UTC", "2026-10-06T00:00:00Z", 1), "2026-10-07 00:00:00 UTC");
    EXPECT_EQ(next_of("@MIDNIGHT", "UTC", "2026-10-06T00:00:00Z", 1), "2026-10-07 00:00:00 UTC");
    EXPECT_EQ(next_of("@hourly", "UTC", "2026-10-06T00:00:00Z", 2), "2026-10-06 01:00:00 UTC | 2026-10-06 02:00:00 UTC");
    EXPECT_EQ(time::cron("@daily", time::zone::utc()), time::cron("0 0 * * *", time::zone::utc()));
    EXPECT_EQ(time::cron("@daily", time::zone::utc()).to_string(), "@daily");
}

// Quartz's ? L W #
TEST(Cron_Test, QuartzDays) {
    EXPECT_EQ(next_of("0 0 L * ?", "UTC", "2026-01-15T00:00:00Z", 3),
              "2026-01-31 00:00:00 UTC | 2026-02-28 00:00:00 UTC | 2026-03-31 00:00:00 UTC");
    EXPECT_EQ(next_of("0 0 L-2 * *", "UTC", "2026-01-15T00:00:00Z", 2), "2026-01-29 00:00:00 UTC | 2026-02-26 00:00:00 UTC");
    EXPECT_EQ(next_of("0 0 LW * *", "UTC", "2026-01-15T00:00:00Z", 3),   // Jan 31 and Feb 28 2026 are Saturdays
              "2026-01-30 00:00:00 UTC | 2026-02-27 00:00:00 UTC | 2026-03-31 00:00:00 UTC");
    EXPECT_EQ(next_of("0 0 15W * *", "UTC", "2026-01-01T00:00:00Z", 4),   // Feb 15 and Mar 15 are Sundays
              "2026-01-15 00:00:00 UTC | 2026-02-16 00:00:00 UTC | 2026-03-16 00:00:00 UTC | 2026-04-15 00:00:00 UTC");
    EXPECT_EQ(next_of("0 0 1W * *", "UTC", "2026-08-01T00:00:00Z", 1), "2026-08-03 00:00:00 UTC");   // Aug 1 2026 a Saturday: the Monday, not July
    EXPECT_EQ(next_of("0 0 31W * *", "UTC", "2026-04-01T00:00:00Z", 1), "2026-05-29 00:00:00 UTC");   // no 31st in April; May 31 a Sunday
    EXPECT_EQ(next_of("0 0 * * 5L", "UTC", "2026-01-01T00:00:00Z", 3),
              "2026-01-30 00:00:00 UTC | 2026-02-27 00:00:00 UTC | 2026-03-27 00:00:00 UTC");
    EXPECT_EQ(next_of("0 0 * * FRIL", "UTC", "2026-01-01T00:00:00Z", 1), "2026-01-30 00:00:00 UTC");
    EXPECT_EQ(next_of("0 0 ? * MON#2", "UTC", "2026-01-01T00:00:00Z", 3),
              "2026-01-12 00:00:00 UTC | 2026-02-09 00:00:00 UTC | 2026-03-09 00:00:00 UTC");
    EXPECT_EQ(next_of("0 0 * * 0#5", "UTC", "2026-01-01T00:00:00Z", 2),   // the months of five Sundays
              "2026-03-29 00:00:00 UTC | 2026-05-31 00:00:00 UTC");
    EXPECT_EQ(next_of("0 0 1,L * *", "UTC", "2026-02-01T00:00:01Z", 2), "2026-02-28 00:00:00 UTC | 2026-03-01 00:00:00 UTC");
}

// Both day fields restricted: either; one of them * or ?: the other alone
TEST(Cron_Test, TheDayFieldsTogether) {
    EXPECT_EQ(next_of("0 0 13 * FRI", "UTC", "2026-10-06T00:00:00Z", 3),   // the 13th or a Friday
              "2026-10-09 00:00:00 UTC | 2026-10-13 00:00:00 UTC | 2026-10-16 00:00:00 UTC");
    EXPECT_EQ(next_of("0 0 */10 * MON", "UTC", "2026-10-06T00:00:00Z", 2),   // */10 begins with *: the weekday alone
              "2026-10-12 00:00:00 UTC | 2026-10-19 00:00:00 UTC");
    EXPECT_EQ(next_of("0 0 13 * ?", "UTC", "2026-10-06T00:00:00Z", 1), "2026-10-13 00:00:00 UTC");
}

// The errors and the bytes they stop on
TEST(Cron_Test, TheErrors) {
    EXPECT_EQ(error_offset("* * * *"), 7u);
    EXPECT_EQ(error_offset(""), 0u);
    EXPECT_EQ(error_offset("* * * * * * *"), 12u);
    EXPECT_EQ(error_offset("60 * * * *"), 0u);
    EXPECT_EQ(error_offset("* 24 * * *"), 2u);
    EXPECT_EQ(error_offset("* * 0 * *"), 4u);
    EXPECT_EQ(error_offset("* * 32 * *"), 4u);
    EXPECT_EQ(error_offset("* * * 13 *"), 6u);
    EXPECT_EQ(error_offset("* * * * 8"), 8u);
    EXPECT_EQ(error_offset("5-3 * * * *"), 0u);
    EXPECT_EQ(error_offset("*/0 * * * *"), 2u);
    EXPECT_EQ(error_offset("1,,2 * * * *"), 2u);
    EXPECT_EQ(error_offset("1, * * * *"), 2u);
    EXPECT_EQ(error_offset("x * * * *"), 0u);
    EXPECT_EQ(error_offset("* * * FOO *"), 6u);
    EXPECT_EQ(error_offset("* * * * FOO"), 8u);
    EXPECT_EQ(error_offset("1x * * * *"), 1u);
    EXPECT_EQ(error_offset("12345 * * * *"), 0u);
    EXPECT_EQ(error_offset("* * L-31 * *"), 4u);
    EXPECT_EQ(error_offset("* * 0W * *"), 4u);
    EXPECT_EQ(error_offset("* * LX * *"), 5u);
    EXPECT_EQ(error_offset("* * * * 1#6"), 8u);
    EXPECT_EQ(error_offset("* * * * 8L"), 8u);
    EXPECT_EQ(error_offset("L * * * *"), 0u);    // L is a day's
    EXPECT_EQ(error_offset("@sometimes"), 0u);
    EXPECT_EQ(error_offset("@daily now"), 7u);
    EXPECT_EQ(error_offset("? * * * *"), 0u);    // ? is a day field's
    auto e = time::cron::parse("61 * * * *", time::zone::utc());
    ASSERT_FALSE(e);
    EXPECT_EQ(e.error().message(), "a value out of the cron field's range");
    EXPECT_THROW(time::cron("61 * * * *"), bad_expected_access<time::error>);
}

// Warsaw in 2026: forward on March 29 at 02:00 CET, back on October 25 at
// 03:00 CEST
TEST(Cron_Test, WarsawAcrossTheChanges) {
    const char* w = "Europe/Warsaw";
    // a fixed time in the skipped hour: at the jump, once
    EXPECT_EQ(next_of("30 2 * * *", w, "2026-03-28T12:00:00+01:00", 3),
              "2026-03-29 03:00:00 CEST | 2026-03-30 02:30:00 CEST | 2026-03-31 02:30:00 CEST");
    EXPECT_EQ(next_of("0,15,30,45 2 * * *", w, "2026-03-29T01:00:00+01:00", 2),   // four times in the gap, one firing
              "2026-03-29 03:00:00 CEST | 2026-03-30 02:00:00 CEST");
    // a fixed time in the repeated hour: the first pass only
    EXPECT_EQ(next_of("30 2 * * *", w, "2026-10-24T12:00:00+02:00", 3),
              "2026-10-25 02:30:00 CEST | 2026-10-26 02:30:00 CET | 2026-10-27 02:30:00 CET");
    EXPECT_EQ(next_of("30 2 * * *", w, "2026-10-25T02:10:00+01:00", 1), "2026-10-26 02:30:00 CET");   // asked in the second pass
    // wildcards: every instant that matches, none in the gap, both passes
    EXPECT_EQ(next_of("*/30 * * * *", w, "2026-10-25T01:50:00+02:00", 6),
              "2026-10-25 02:00:00 CEST | 2026-10-25 02:30:00 CEST | 2026-10-25 02:00:00 CET | 2026-10-25 02:30:00 CET | 2026-10-25 03:00:00 CET | 2026-10-25 03:30:00 CET");
    EXPECT_EQ(next_of("*/30 * * * *", w, "2026-03-29T01:40:00+01:00", 3),
              "2026-03-29 03:00:00 CEST | 2026-03-29 03:30:00 CEST | 2026-03-29 04:00:00 CEST");
    EXPECT_EQ(next_of("0 * * * *", w, "2026-10-25T02:30:00+01:00", 2), "2026-10-25 03:00:00 CET | 2026-10-25 04:00:00 CET");
    EXPECT_EQ(next_of("0 2 * * *", w, "2026-03-29T00:00:00+01:00", 1), "2026-03-29 03:00:00 CEST");   // 02:00 is the gap's start
    EXPECT_EQ(next_of("0 3 * * *", w, "2026-03-29T00:00:00+01:00", 1), "2026-03-29 03:00:00 CEST");
}

// New York in 2026: forward on March 8 at 02:00 EST, back on November 1
// at 02:00 EDT
TEST(Cron_Test, NewYorkAcrossTheChanges) {
    const char* ny = "America/New_York";
    EXPECT_EQ(next_of("0 2 * * *", ny, "2026-03-07T12:00:00-05:00", 2), "2026-03-08 03:00:00 EDT | 2026-03-09 02:00:00 EDT");
    EXPECT_EQ(next_of("30 1 * * *", ny, "2026-10-31T12:00:00-04:00", 2), "2026-11-01 01:30:00 EDT | 2026-11-02 01:30:00 EST");
    EXPECT_EQ(next_of("0 * * * *", ny, "2026-11-01T00:30:00-04:00", 4),
              "2026-11-01 01:00:00 EDT | 2026-11-01 01:00:00 EST | 2026-11-01 02:00:00 EST | 2026-11-01 03:00:00 EST");
    EXPECT_EQ(next_of("15 * * * *", ny, "2026-03-08T01:30:00-05:00", 2), "2026-03-08 03:15:00 EDT | 2026-03-08 04:15:00 EDT");
    EXPECT_EQ(next_of("0 0 * * *", ny, "2026-11-01T00:00:00-04:00", 1), "2026-11-02 00:00:00 EST");
}

// matches(), the end of the times, the end of the range
TEST(Cron_Test, MatchesAndTheEnds) {
    time::cron c("0 12 * * *", time::zone::utc());
    EXPECT_TRUE(c.matches(at("2026-10-06T12:00:00Z")));
    EXPECT_TRUE(c.matches(at("2026-10-06T12:00:00.5Z")));   // to the second
    EXPECT_FALSE(c.matches(at("2026-10-06T12:00:01Z")));
    time::cron gap("30 2 * * *", time::zone("Europe/Warsaw"));
    EXPECT_TRUE(gap.matches(at("2026-03-29T03:00:00+02:00")));   // fires at the jump
    EXPECT_FALSE(gap.matches(at("2026-10-25T02:30:00+01:00")));   // not in the second pass
    time::cron never("0 0 30 2 *", time::zone::utc());
    EXPECT_FALSE(never.next(at("2026-01-01T00:00:00Z")));
    EXPECT_TRUE(never.next(at("2026-01-01T00:00:00Z"), 3).empty());
    time::cron leap("0 0 29 2 *", time::zone::utc());
    auto times = leap.next(at("2096-03-01T00:00:00Z"), 2);   // 2100 is no leap year
    ASSERT_EQ(times.size(), 2u);
    EXPECT_EQ(times[0].year(), 2104);
    EXPECT_EQ(times[1].year(), 2108);
    auto last = time::cron("@yearly", time::zone::utc()).next(at("2261-06-01T00:00:00Z"), 5);
    ASSERT_EQ(last.size(), 1u);   // 2262-01-01; 2263 is past a datetime's range
    EXPECT_EQ(last[0].year(), 2262);
    EXPECT_TRUE(time::cron("* * * * *", time::zone::utc()).next(at("2026-01-01T00:00:00Z"), 0).empty());
    auto next = c.next(at("2026-10-06T12:00:00Z"));   // strictly after
    ASSERT_TRUE(next);
    EXPECT_EQ(*next, at("2026-10-07T12:00:00Z"));
    EXPECT_EQ(next->zone(), time::zone::utc());
}

// Equality: the same fields and the same zone
TEST(Cron_Test, Equality) {
    auto utc = time::zone::utc();
    EXPECT_EQ(time::cron("0 0 * * 0", utc), time::cron("0 0 * * 7", utc));
    EXPECT_EQ(time::cron("0 0 * * SUN", utc), time::cron("@weekly", utc));
    EXPECT_FALSE(time::cron("0 0 * * *", utc) == time::cron("0 0 * * *", time::zone("Europe/Warsaw")));
    EXPECT_FALSE(time::cron("0 0 * * *", utc) == time::cron("0 1 * * *", utc));
    EXPECT_EQ(time::cron("0 0 * * *", utc).zone(), utc);
}

// every: f at the times under a manual clock, and the stop
TEST(Cron_Test, EveryCallsAtTheTimes) {
    async::manual_clock clock;
    clock.install();
    std::atomic<int> calls{0};
    auto now = time::now();
    int64_t s = now.unix();
    int64_t to_minute = 60 - s % 60;   // the next whole minute
    auto job = time::every(time::cron("* * * * *", time::zone::utc()), [&] { calls.fetch_add(1); });
    clock.advance(std::chrono::seconds(to_minute) - 1s);
    EXPECT_EQ(calls.load(), 0);
    clock.advance(1s);
    EXPECT_EQ(calls.load(), 1);
    clock.advance(60s);
    clock.advance(60s);
    EXPECT_EQ(calls.load(), 3);
    job.request_stop();
    clock.advance(120s);
    EXPECT_EQ(calls.load(), 3);
    clock.uninstall();
    async::scheduler::stop();
}

namespace {
    async::task<> slow_job(std::atomic<int>* calls) {
        calls->fetch_add(1);
        co_await async::sleep(150s);   // two and a half minutes of work: two times pass, one is held
    }
}

// A time that comes while f runs is held, one, the others dropped
TEST(Cron_Test, EveryHoldsOne) {
    async::manual_clock clock;
    clock.install();
    std::atomic<int> calls{0};
    int64_t s = time::now().unix();
    async::stop_source parent;
    auto job = time::every(time::cron("* * * * *", time::zone::utc()), [&] { return slow_job(&calls); }, parent.token());
    clock.advance(std::chrono::seconds(60 - s % 60));   // the first: f runs until +150 s
    EXPECT_EQ(calls.load(), 1);
    for (int i : range(15)) {
        (void)i;
        clock.advance(10s);
    }
    EXPECT_EQ(calls.load(), 2);   // at +150 s, the held one, at once
    parent.request_stop();        // the parent stops the child
    clock.advance(600s);
    EXPECT_LE(calls.load(), 3);
    clock.uninstall();
    async::scheduler::stop();
}
