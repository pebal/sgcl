//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// icalendar, recurrence, content_line: RFC 5545. The recurrence rules against
// the examples of RFC 5545 §3.8.5.3 with the instances it lists, and against a
// second implementation written for the tests (tools/rrule_oracle.py writes
// rrule_tests.h: 600 random rules from random starts in three zones). Then the
// rules' text, content lines (unfolding, folding, parameters, RFC 6868, the
// value types), calendars (components, errors and their places, VTIMEZONE,
// occurrences with RDATE and EXDATE, all-day events), the writer's round
// trip, new versions and the boundaries. vCard has its own file.
#include "common.h"
#include "rrule_tests.h"

#include <string>
#include <vector>

using namespace sgcl::encoding;
using namespace enc_test;
namespace stime = sgcl::time;

namespace {
    time::zone ny() {
        return time::zone::load("America/New_York").value();
    }

    time::datetime at(int y, int mo, int d, int h, int mi, const time::zone& z) {
        return time::date(y, mo, d).at(h, mi, z);
    }

    // "1997-09-02 09:00" of each instance in New York
    std::vector<std::string> walls(const sgcl::vector<time::datetime>& v) {
        std::vector<std::string> out;
        for (auto& t : v) {
            auto s = std::string(t.to_string().view()).substr(0, 16);
            s[10] = ' ';
            out.push_back(s);
        }
        return out;
    }

    std::vector<std::string> expand(const char* start, const char* rule, size_t limit) {
        int y, mo, d, h, mi;
        std::sscanf(start, "%4d%2d%2dT%2d%2d", &y, &mo, &d, &h, &mi);
        auto s = at(y, mo, d, h, mi, ny());
        return walls(recurrence(rule).occurrences(s, s, at(2010, 1, 1, 0, 0, ny()), limit));
    }

    std::vector<std::string> days(int y, int mo, std::initializer_list<int> ds, const char* time = "09:00") {
        std::vector<std::string> out;
        for (int d : ds) {
            char b[32];
            std::snprintf(b, sizeof b, "%04d-%02d-%02d %s", y, mo, d, time);
            out.push_back(b);
        }
        return out;
    }

    std::vector<std::string> join(std::initializer_list<std::vector<std::string>> parts) {
        std::vector<std::string> out;
        for (auto& p : parts) {
            out.insert(out.end(), p.begin(), p.end());
        }
        return out;
    }

    icalendar cal(const std::string& body) {
        auto r = icalendar::parse(sgcl::string("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//test//test//EN\r\n" + body + "END:VCALENDAR\r\n"));
        if (!r) {
            throw std::runtime_error("bad test calendar: " + std::string(r.error().message().view()));
        }
        return *r;
    }
}

// RFC 5545 §3.8.5.3, the examples whose DTSTART is in New York, as listed
TEST(Icalendar_Tests, RfcRecurrenceExamples) {
    EXPECT_EQ(expand("19970902T0900", "FREQ=DAILY;COUNT=10", 100), days(1997, 9, {2, 3, 4, 5, 6, 7, 8, 9, 10, 11}));
    auto until = expand("19970902T0900", "FREQ=DAILY;UNTIL=19971224T000000Z", 1000);
    EXPECT_EQ(until.size(), 113u);
    EXPECT_EQ(until.back(), "1997-12-23 09:00");
    EXPECT_EQ(expand("19970902T0900", "FREQ=DAILY;INTERVAL=10;COUNT=5", 100), join({days(1997, 9, {2, 12, 22}), days(1997, 10, {2, 12})}));
    auto january = expand("19980101T0900", "FREQ=YEARLY;UNTIL=20000131T140000Z;BYMONTH=1;BYDAY=SU,MO,TU,WE,TH,FR,SA", 1000);
    EXPECT_EQ(january.size(), 93u);
    EXPECT_EQ(january.back(), "2000-01-31 09:00");
    EXPECT_EQ(expand("19980101T0900", "FREQ=DAILY;UNTIL=20000131T140000Z;BYMONTH=1", 1000), january);
    EXPECT_EQ(expand("19970902T0900", "FREQ=WEEKLY;COUNT=10", 100),
              join({days(1997, 9, {2, 9, 16, 23, 30}), days(1997, 10, {7, 14, 21, 28}), days(1997, 11, {4})}));
    EXPECT_EQ(expand("19970902T0900", "FREQ=WEEKLY;UNTIL=19971007T000000Z;WKST=SU;BYDAY=TU,TH", 100),
              join({days(1997, 9, {2, 4, 9, 11, 16, 18, 23, 25, 30}), days(1997, 10, {2})}));
    EXPECT_EQ(expand("19970901T0900", "FREQ=WEEKLY;INTERVAL=2;UNTIL=19971224T000000Z;WKST=SU;BYDAY=MO,WE,FR", 100),
              join({days(1997, 9, {1, 3, 5, 15, 17, 19, 29}), days(1997, 10, {1, 3, 13, 15, 17, 27, 29, 31}), days(1997, 11, {10, 12, 14, 24, 26, 28}),
                    days(1997, 12, {8, 10, 12, 22})}));
    EXPECT_EQ(expand("19970902T0900", "FREQ=WEEKLY;INTERVAL=2;COUNT=8;WKST=SU;BYDAY=TU,TH", 100),
              join({days(1997, 9, {2, 4, 16, 18, 30}), days(1997, 10, {2, 14, 16})}));
    EXPECT_EQ(expand("19970905T0900", "FREQ=MONTHLY;COUNT=10;BYDAY=1FR", 100),
              join({days(1997, 9, {5}), days(1997, 10, {3}), days(1997, 11, {7}), days(1997, 12, {5}), days(1998, 1, {2}), days(1998, 2, {6}),
                    days(1998, 3, {6}), days(1998, 4, {3}), days(1998, 5, {1}), days(1998, 6, {5})}));
    EXPECT_EQ(expand("19970907T0900", "FREQ=MONTHLY;INTERVAL=2;COUNT=10;BYDAY=1SU,-1SU", 100),
              join({days(1997, 9, {7, 28}), days(1997, 11, {2, 30}), days(1998, 1, {4, 25}), days(1998, 3, {1, 29}), days(1998, 5, {3, 31})}));
    EXPECT_EQ(expand("19970922T0900", "FREQ=MONTHLY;COUNT=6;BYDAY=-2MO", 100),
              join({days(1997, 9, {22}), days(1997, 10, {20}), days(1997, 11, {17}), days(1997, 12, {22}), days(1998, 1, {19}), days(1998, 2, {16})}));
    EXPECT_EQ(expand("19970928T0900", "FREQ=MONTHLY;BYMONTHDAY=-3", 6),
              join({days(1997, 9, {28}), days(1997, 10, {29}), days(1997, 11, {28}), days(1997, 12, {29}), days(1998, 1, {29}), days(1998, 2, {26})}));
    EXPECT_EQ(expand("19970902T0900", "FREQ=MONTHLY;COUNT=10;BYMONTHDAY=2,15", 100),
              join({days(1997, 9, {2, 15}), days(1997, 10, {2, 15}), days(1997, 11, {2, 15}), days(1997, 12, {2, 15}), days(1998, 1, {2, 15})}));
    EXPECT_EQ(expand("19970930T0900", "FREQ=MONTHLY;COUNT=10;BYMONTHDAY=1,-1", 100),
              join({days(1997, 9, {30}), days(1997, 10, {1, 31}), days(1997, 11, {1, 30}), days(1997, 12, {1, 31}), days(1998, 1, {1, 31}), days(1998, 2, {1})}));
    EXPECT_EQ(expand("19970910T0900", "FREQ=MONTHLY;INTERVAL=18;COUNT=10;BYMONTHDAY=10,11,12,13,14,15", 100),
              join({days(1997, 9, {10, 11, 12, 13, 14, 15}), days(1999, 3, {10, 11, 12, 13})}));
    EXPECT_EQ(expand("19970902T0900", "FREQ=MONTHLY;INTERVAL=2;BYDAY=TU", 12),
              join({days(1997, 9, {2, 9, 16, 23, 30}), days(1997, 11, {4, 11, 18, 25}), days(1998, 1, {6, 13, 20})}));
    EXPECT_EQ(expand("19970610T0900", "FREQ=YEARLY;COUNT=10;BYMONTH=6,7", 100),
              join({days(1997, 6, {10}), days(1997, 7, {10}), days(1998, 6, {10}), days(1998, 7, {10}), days(1999, 6, {10}), days(1999, 7, {10}),
                    days(2000, 6, {10}), days(2000, 7, {10}), days(2001, 6, {10}), days(2001, 7, {10})}));
    EXPECT_EQ(expand("19970310T0900", "FREQ=YEARLY;INTERVAL=2;COUNT=10;BYMONTH=1,2,3", 100),
              join({days(1997, 3, {10}), days(1999, 1, {10}), days(1999, 2, {10}), days(1999, 3, {10}), days(2001, 1, {10}), days(2001, 2, {10}),
                    days(2001, 3, {10}), days(2003, 1, {10}), days(2003, 2, {10}), days(2003, 3, {10})}));
    EXPECT_EQ(expand("19970101T0900", "FREQ=YEARLY;INTERVAL=3;COUNT=10;BYYEARDAY=1,100,200", 100),
              join({days(1997, 1, {1}), days(1997, 4, {10}), days(1997, 7, {19}), days(2000, 1, {1}), days(2000, 4, {9}), days(2000, 7, {18}),
                    days(2003, 1, {1}), days(2003, 4, {10}), days(2003, 7, {19}), days(2006, 1, {1})}));
    EXPECT_EQ(expand("19970519T0900", "FREQ=YEARLY;BYDAY=20MO", 3), join({days(1997, 5, {19}), days(1998, 5, {18}), days(1999, 5, {17})}));
    EXPECT_EQ(expand("19970512T0900", "FREQ=YEARLY;BYWEEKNO=20;BYDAY=MO", 3), join({days(1997, 5, {12}), days(1998, 5, {11}), days(1999, 5, {17})}));
    EXPECT_EQ(expand("19970313T0900", "FREQ=YEARLY;BYMONTH=3;BYDAY=TH", 11),
              join({days(1997, 3, {13, 20, 27}), days(1998, 3, {5, 12, 19, 26}), days(1999, 3, {4, 11, 18, 25})}));
    EXPECT_EQ(expand("19970605T0900", "FREQ=YEARLY;BYDAY=TH;BYMONTH=6,7,8", 13),
              join({days(1997, 6, {5, 12, 19, 26}), days(1997, 7, {3, 10, 17, 24, 31}), days(1997, 8, {7, 14, 21, 28})}));
    EXPECT_EQ(expand("19980213T0900", "FREQ=MONTHLY;BYDAY=FR;BYMONTHDAY=13", 5),
              join({days(1998, 2, {13}), days(1998, 3, {13}), days(1998, 11, {13}), days(1999, 8, {13}), days(2000, 10, {13})}));
    EXPECT_EQ(expand("19970913T0900", "FREQ=MONTHLY;BYDAY=SA;BYMONTHDAY=7,8,9,10,11,12,13", 10),
              join({days(1997, 9, {13}), days(1997, 10, {11}), days(1997, 11, {8}), days(1997, 12, {13}), days(1998, 1, {10}), days(1998, 2, {7}),
                    days(1998, 3, {7}), days(1998, 4, {11}), days(1998, 5, {9}), days(1998, 6, {13})}));
    EXPECT_EQ(expand("19961105T0900", "FREQ=YEARLY;INTERVAL=4;BYMONTH=11;BYDAY=TU;BYMONTHDAY=2,3,4,5,6,7,8", 3),
              join({days(1996, 11, {5}), days(2000, 11, {7}), days(2004, 11, {2})}));
    EXPECT_EQ(expand("19970904T0900", "FREQ=MONTHLY;COUNT=3;BYDAY=TU,WE,TH;BYSETPOS=3", 100), join({days(1997, 9, {4}), days(1997, 10, {7}), days(1997, 11, {6})}));
    EXPECT_EQ(expand("19970929T0900", "FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;BYSETPOS=-2", 7),
              join({days(1997, 9, {29}), days(1997, 10, {30}), days(1997, 11, {27}), days(1997, 12, {30}), days(1998, 1, {29}), days(1998, 2, {26}),
                    days(1998, 3, {30})}));
    EXPECT_EQ(expand("19970902T0900", "FREQ=MINUTELY;INTERVAL=15;COUNT=6", 100),
              (std::vector<std::string>{"1997-09-02 09:00", "1997-09-02 09:15", "1997-09-02 09:30", "1997-09-02 09:45", "1997-09-02 10:00", "1997-09-02 10:15"}));
    EXPECT_EQ(expand("19970902T0900", "FREQ=MINUTELY;INTERVAL=90;COUNT=4", 100),
              (std::vector<std::string>{"1997-09-02 09:00", "1997-09-02 10:30", "1997-09-02 12:00", "1997-09-02 13:30"}));
    auto twenty = expand("19970902T0900", "FREQ=DAILY;BYHOUR=9,10,11,12,13,14,15,16;BYMINUTE=0,20,40", 48);
    EXPECT_EQ(twenty.size(), 48u);
    EXPECT_EQ(twenty[23], "1997-09-02 16:40");
    EXPECT_EQ(twenty[24], "1997-09-03 09:00");
    EXPECT_EQ(expand("19970902T0900", "FREQ=MINUTELY;INTERVAL=20;BYHOUR=9,10,11,12,13,14,15,16", 48), twenty);
    EXPECT_EQ(expand("19970805T0900", "FREQ=WEEKLY;INTERVAL=2;COUNT=4;BYDAY=TU,SU;WKST=MO", 100), days(1997, 8, {5, 10, 19, 24}));
    EXPECT_EQ(expand("19970805T0900", "FREQ=WEEKLY;INTERVAL=2;COUNT=4;BYDAY=TU,SU;WKST=SU", 100), days(1997, 8, {5, 17, 19, 31}));
    EXPECT_EQ(expand("20070115T0900", "FREQ=MONTHLY;BYMONTHDAY=15,30;COUNT=5", 100), join({days(2007, 1, {15, 30}), days(2007, 2, {15}), days(2007, 3, {15, 30})}));
    // an invalid date is no instance: the 29th of February only in leap years
    EXPECT_EQ(expand("19960229T0900", "FREQ=YEARLY;COUNT=3", 100), join({days(1996, 2, {29}), days(2000, 2, {29}), days(2004, 2, {29})}));
}

TEST(Icalendar_Tests, AsTheOracleExpandsThem) {
    size_t compared = 0;
    for (auto& e : rrule_oracle::expansions) {
        auto z = time::zone::load(sgcl::string(std::string(e.zone))).value();
        int64_t w = e.start_wall;
        auto start = time::date(std::chrono::sys_days(std::chrono::days(w / 86400))).at(int(w % 86400 / 3600), int(w % 3600 / 60), int(w % 60), z);
        auto r = recurrence::parse(sgcl::string(std::string(e.rule)));
        ASSERT_TRUE(r) << e.rule << ": " << r.error().message();
        EXPECT_EQ(*recurrence::parse(r->to_string()), *r) << e.rule;
        std::string_view inst = e.instants;
        int64_t horizon = std::stoll(std::string(inst.substr(0, inst.find(';'))));
        std::string want(inst.substr(inst.find(';') + 1)), got;
        for (auto& t : r->occurrences(start, start, time::datetime::from_unix(horizon + 1, z), 40)) {
            got += (got.empty() ? "" : ",") + std::to_string(t.unix());
        }
        EXPECT_EQ(got, want) << e.zone << " " << e.rule << " from " << start.to_string();
        ++compared;
    }
    EXPECT_EQ(compared, 600u);
}

TEST(Icalendar_Tests, RecurrenceText) {
    recurrence r("freq=monthly;byday=-1fr,2MO;count=5;wkst=su;interval=2;X-NAME=x");
    EXPECT_EQ(r.freq(), recurrence::frequency::monthly);
    EXPECT_EQ(r.interval(), 2);
    EXPECT_EQ(r.count(), 5);
    EXPECT_FALSE(r.until());
    ASSERT_EQ(r.by_day().size(), 2u);
    EXPECT_EQ(r.by_day()[0].day, time::weekday::friday);
    EXPECT_EQ(r.by_day()[0].ordinal, -1);
    EXPECT_EQ(r.week_start(), time::weekday::sunday);
    EXPECT_EQ(r.to_string(), "FREQ=MONTHLY;COUNT=5;INTERVAL=2;BYDAY=-1FR,2MO;WKST=SU");
    recurrence u("FREQ=YEARLY;UNTIL=20301231;BYMONTH=1,12;BYMONTHDAY=-1;BYSETPOS=1;BYHOUR=1;BYMINUTE=2;BYSECOND=60");
    EXPECT_EQ(u.until(), "20301231");
    EXPECT_EQ(u.by_month().size(), 2u);
    EXPECT_EQ(u.by_month_day()[0], -1);
    EXPECT_EQ(u.by_second()[0], 60);
    EXPECT_EQ(u.to_string(), "FREQ=YEARLY;UNTIL=20301231;BYSECOND=60;BYMINUTE=2;BYHOUR=1;BYMONTHDAY=-1;BYMONTH=1,12;BYSETPOS=1");
    EXPECT_EQ(recurrence().to_string(), "FREQ=DAILY");
    EXPECT_EQ(recurrence(), recurrence("FREQ=DAILY"));
    EXPECT_NE(recurrence("FREQ=DAILY;COUNT=1"), recurrence("FREQ=DAILY"));
    for (const char* bad : {"", "COUNT=1", "FREQ=SOMETIMES", "FREQ=DAILY;COUNT=1;UNTIL=20200101", "FREQ=DAILY;FREQ=DAILY", "FREQ=DAILY;INTERVAL=0",
                            "FREQ=DAILY;BYHOUR=24", "FREQ=DAILY;BYMONTHDAY=0", "FREQ=DAILY;BYMONTH=13", "FREQ=DAILY;BYWEEKNO=1",
                            "FREQ=MONTHLY;BYYEARDAY=1", "FREQ=WEEKLY;BYMONTHDAY=1", "FREQ=WEEKLY;BYDAY=1MO", "FREQ=YEARLY;BYWEEKNO=1;BYDAY=1MO",
                            "FREQ=DAILY;BYSETPOS=1", "FREQ=DAILY;BYDAY=XX", "FREQ=DAILY;UNTIL=2020", "FREQ=DAILY;COUNT=x", "FREQ=DAILY;;", "FREQ=DAILY;NAME=1",
                            "FREQ=DAILY;BYDAY=0MO", "FREQ=DAILY;BYDAY=,MO"}) {
        auto p = recurrence::parse(bad);
        EXPECT_FALSE(p) << bad;
        if (!p) {
            EXPECT_EQ(p.error().code(), errc::syntax) << bad;
        }
    }
    EXPECT_THROW(recurrence("FREQ=NEVER"), sgcl::bad_expected_access<error>);
    // limits and bounds of occurrences
    auto s = at(2026, 1, 5, 9, 0, ny());
    recurrence daily("FREQ=DAILY");
    EXPECT_EQ(daily.occurrences(s, s, at(2026, 1, 10, 9, 0, ny())).size(), 5u);   // to is excluded
    EXPECT_EQ(daily.occurrences(s, at(2026, 1, 7, 9, 0, ny()), at(2026, 1, 10, 9, 0, ny())).size(), 3u);
    EXPECT_EQ(daily.occurrences(s, s, at(2027, 1, 1, 0, 0, ny()), 2).size(), 2u);
    EXPECT_EQ(daily.occurrences(s, s, s).size(), 0u);
    EXPECT_EQ(daily.occurrences(s, s, at(2027, 1, 1, 0, 0, ny()), 0).size(), 0u);
    EXPECT_EQ(daily.occurrences(s, at(2400, 1, 1, 0, 0, time::zone::utc()), at(2401, 1, 1, 0, 0, time::zone::utc())).size(), 0u);
    // COUNT counts from the start, whatever from is
    EXPECT_EQ(recurrence("FREQ=DAILY;COUNT=3").occurrences(s, at(2026, 1, 6, 0, 0, ny()), at(2027, 1, 1, 0, 0, ny())).size(), 2u);
    // a start the rule does not make is the first instance all the same
    EXPECT_EQ(walls(recurrence("FREQ=MONTHLY;BYMONTHDAY=20;COUNT=2").occurrences(s, s, at(2027, 1, 1, 0, 0, ny()))),
              (std::vector<std::string>{"2026-01-05 09:00", "2026-01-20 09:00"}));
    // a rule that makes nothing ends
    EXPECT_EQ(recurrence("FREQ=YEARLY;BYMONTH=2;BYMONTHDAY=30").occurrences(s, s, at(2200, 1, 1, 0, 0, ny())).size(), 1u);
    EXPECT_EQ(recurrence("FREQ=SECONDLY;BYMONTH=2;BYMONTHDAY=30").occurrences(s, s, at(2030, 1, 1, 0, 0, ny())).size(), 1u);
    // a time the zone skips moves on, one it shows twice is the first
    auto skip = at(2026, 3, 7, 2, 30, ny());
    auto both = recurrence("FREQ=DAILY;COUNT=3").occurrences(skip, skip, at(2027, 1, 1, 0, 0, ny()));
    EXPECT_EQ(walls(both), (std::vector<std::string>{"2026-03-07 02:30", "2026-03-08 03:30", "2026-03-09 02:30"}));
    auto twice = at(2026, 10, 31, 1, 30, ny());
    auto fall = recurrence("FREQ=DAILY;COUNT=2").occurrences(twice, twice, at(2027, 1, 1, 0, 0, ny()));
    EXPECT_EQ(fall[1].offset(), sgcl::duration(std::chrono::hours(-4)));
    // a start at the second of a time shown twice is the first instance as it is
    auto second = stime::datetime::from_unix(at(2026, 11, 1, 1, 30, ny()).unix() + 3600, ny());
    auto from_second = recurrence("FREQ=DAILY;COUNT=2").occurrences(second, second, at(2027, 1, 1, 0, 0, ny()));
    ASSERT_EQ(from_second.size(), 2u);
    EXPECT_EQ(from_second[0], second);
    EXPECT_EQ(from_second[0].offset(), sgcl::duration(std::chrono::hours(-5)));
    // fractions of a second carried, the bounds exact
    auto frac = time::datetime::from_unix_nano(1767600000500000000, time::zone::utc());
    auto fr = recurrence("FREQ=SECONDLY;COUNT=3").occurrences(frac, frac, time::datetime::from_unix(1767600002, time::zone::utc()));
    ASSERT_EQ(fr.size(), 2u);
    EXPECT_EQ(fr[1].nanosecond(), 500000000);
}

TEST(Icalendar_Tests, ContentLines) {
    auto l = content_line::parse("DTSTART;TZID=\"America/New_York\";X-LIST=a,\"b,c\",d^nx^^^':19970902T090000");
    ASSERT_TRUE(l);
    EXPECT_EQ(l->name(), "DTSTART");
    EXPECT_EQ(l->param("tzid"), "America/New_York");
    ASSERT_EQ(l->params().size(), 2u);
    ASSERT_EQ(l->params()[1].values.size(), 3u);
    EXPECT_EQ(l->params()[1].values[1], "b,c");
    EXPECT_EQ(l->params()[1].values[2], "d\nx^\"");
    EXPECT_EQ(l->value(), "19970902T090000");
    EXPECT_EQ(l->as_datetime()->to_string(), "1997-09-02T09:00:00-04:00");
    EXPECT_EQ(l->as_date(), time::date(1997, 9, 2));
    EXPECT_EQ(std::string(l->to_string().view()), "DTSTART;TZID=America/New_York;X-LIST=a,\"b,c\",d^nx^^^':19970902T090000\r\n");
    EXPECT_EQ(*content_line::parse(sgcl::string(std::string(l->to_string().view()).substr(0, l->to_string().size() - 2))), *l);
    // groups, lower case, TEXT
    auto g = content_line::parse("item1.email;type=work:a@b.c");
    EXPECT_EQ(g->group(), "item1");
    EXPECT_EQ(g->name(), "EMAIL");
    EXPECT_EQ(g->param("TYPE"), "work");
    content_line t = content_line::text("SUMMARY", "a, b; c\\d\nnext");
    EXPECT_EQ(t.value(), "a\\, b\\; c\\\\d\\nnext");
    EXPECT_EQ(t.text(), "a, b; c\\d\nnext");
    EXPECT_EQ(content_line("X", "a\\,b,c\\nd,").list().size(), 3u);
    EXPECT_EQ(content_line("X", "a\\,b,c\\nd,").list()[0], "a,b");
    auto n = content_line("N", "Doe;John;;Dr.;Jr.\\, MD").components();
    ASSERT_EQ(n.size(), 5u);
    EXPECT_EQ(n[4], "Jr., MD");
    EXPECT_EQ(content_line("X", "\\N\\x").text(), "\nx");
    // the value types
    EXPECT_EQ(content_line("X", "P1W").as_duration(), sgcl::duration(std::chrono::hours(168)));
    EXPECT_EQ(content_line("X", "-PT15M").as_duration(), sgcl::duration(std::chrono::minutes(-15)));
    EXPECT_EQ(content_line("X", "P1DT2H3M4S").as_duration(), sgcl::duration(std::chrono::seconds(86400 + 7384)));
    EXPECT_EQ(content_line("X", "PT1H0M").as_duration(), sgcl::duration(std::chrono::hours(1)));
    for (const char* bad : {"P", "PT", "P1H", "PT1D", "P1W2D", "PT1S2M", "PT1H2S", "1D", "P-1D", "PT1M2M"}) {
        EXPECT_FALSE(content_line("X", bad).as_duration()) << bad;
    }
    EXPECT_EQ(content_line("X", "+0530").as_utc_offset(), sgcl::duration(std::chrono::minutes(330)));
    EXPECT_EQ(content_line("X", "-080030").as_utc_offset(), sgcl::duration(std::chrono::seconds(-28830)));
    EXPECT_FALSE(content_line("X", "-0000").as_utc_offset());
    EXPECT_FALSE(content_line("X", "0530").as_utc_offset());
    EXPECT_EQ(content_line("X", "-12").as_int(), -12);
    EXPECT_FALSE(content_line("X", "1x").as_int());
    EXPECT_EQ(content_line("X", "true").as_bool(), true);
    EXPECT_FALSE(content_line("X", "yes").as_bool());
    EXPECT_EQ(content_line("X", "19970714T173000Z").as_datetime()->unix(), 868901400);
    EXPECT_FALSE(content_line("X", "19970714T173000").as_datetime());   // floating
    EXPECT_EQ(content_line("X", "19970714T173000").as_datetime(time::zone::utc())->unix(), 868901400);
    EXPECT_FALSE(content_line("X", "19970714").as_datetime(time::zone::utc()));
    EXPECT_FALSE(content_line("X", "19970230").as_date());
    EXPECT_FALSE(content_line("X", "19970714T250000Z").as_datetime());
    EXPECT_FALSE(content_line("X", {{"TZID", {"No/Such_Zone"}}}, "19970714T173000").as_datetime());
    EXPECT_EQ(content_line("RRULE", "FREQ=DAILY").as_recurrence(), recurrence());
    EXPECT_FALSE(content_line("RRULE", "FREQ=x").as_recurrence());
    // new lines
    EXPECT_EQ(l->with_param("tzid", "UTC").param("TZID"), "UTC");
    EXPECT_EQ(l->with_param("NEW", "x").params().size(), 3u);
    EXPECT_EQ(content_line("EMAIL", "x").with_group("home").to_string(), "home.EMAIL:x\r\n");
    // folding at 75 octets, never inside a UTF-8 sequence
    std::string long_text = "DESCRIPTION:" + std::string(70, 'a') + "ąęśćżźółń" + std::string(100, 'b');
    auto folded = std::string(content_line::parse(sgcl::string(long_text))->to_string().view());
    size_t from = 0;
    while (from < folded.size()) {
        size_t end = folded.find("\r\n", from);
        EXPECT_LE(end - from, 75u);
        EXPECT_NE(uint8_t(folded[from + (from ? 1 : 0)]) & 0xC0, 0x80u);
        from = end + 2;
    }
    // errors
    for (const char* bad : {":x", "A B:x", "NAME", "NAME;P=\"open:x", "NAME;=x:y", "NAME;P=a\"b:c", "NAME;P=x\x01:y", "NAME:a\x01"}) {
        EXPECT_FALSE(content_line::parse(bad)) << bad;
    }
    EXPECT_EQ(content_line::parse("TEL;HOME;VOICE:1")->param("TYPE"), "HOME");   // vCard 3.0's bare TYPE
    EXPECT_EQ(content_line::parse("TEL;HOME;VOICE:1")->params().size(), 2u);
    EXPECT_THROW(content_line("", "x").to_string(), sgcl::invalid_argument);
    EXPECT_THROW(content_line("A B", "x").to_string(), sgcl::invalid_argument);
    EXPECT_EQ(content_line(), content_line());
}

TEST(Icalendar_Tests, Calendars) {
    icalendar c = cal("BEGIN:VEVENT\r\nUID:1@example.com\r\nDTSTAMP:19970610T172345Z\r\nDTSTART;VALUE=DATE:19970714\r\n"
                      "SUMMARY:Bastille Day Party\r\nCATEGORIES:A,B\r\nATTENDEE;CN=\"Doe, J\":mailto:j@example.com\r\nATTENDEE:mailto:k@example.com\r\n"
                      "BEGIN:VALARM\r\nACTION:DISPLAY\r\nTRIGGER:-PT15M\r\nEND:VALARM\r\nEND:VEVENT\r\n"
                      "BEGIN:VTODO\r\nUID:2\r\nDUE:19970415T235959Z\r\nEND:VTODO\r\n");
    EXPECT_EQ(c.name(), "VCALENDAR");
    EXPECT_EQ(c.text("VERSION", "?"), "2.0");
    EXPECT_EQ(c.components().size(), 2u);
    auto events = c.components_of("vevent");
    ASSERT_EQ(events.size(), 1u);
    icalendar e = events[0];
    EXPECT_EQ(e.text("SUMMARY", "?"), "Bastille Day Party");
    EXPECT_EQ(e.text("LOCATION", "none"), "none");
    EXPECT_EQ(e.properties_of("ATTENDEE").size(), 2u);
    EXPECT_EQ(e.properties_of("ATTENDEE")[0].param("CN"), "Doe, J");
    EXPECT_EQ(e.property_of("CATEGORIES")->list().size(), 2u);
    EXPECT_EQ(e.property_of("DTSTART")->as_date(), time::date(1997, 7, 14));
    EXPECT_FALSE(e.property_of("NOPE"));
    EXPECT_EQ(e.components_of("VALARM")[0].property_of("TRIGGER")->as_duration(), sgcl::duration(std::chrono::minutes(-15)));
    // an all-day event's day at midnight in the zone given
    auto d = c.datetime_of(*e.property_of("DTSTART"), time::zone::utc());
    EXPECT_EQ(d->unix(), 868838400);
    EXPECT_EQ(c.components_of("VTODO")[0].property_of("DUE")->as_datetime()->unix(), 861148799);
    // writing reads back the same
    auto back = icalendar::parse(c.to_string());
    ASSERT_TRUE(back);
    EXPECT_EQ(*back, c);
    EXPECT_EQ(back->to_string(), c.to_string());
    // made in the program
    icalendar made = icalendar().add(icalendar("VEVENT")
                                         .add(content_line("UID", "x"))
                                         .add(content_line::text("SUMMARY", "Lunch, with friends"))
                                         .add(content_line("DTSTART", {{"TZID", {"Europe/Warsaw"}}}, "20261006T120000")));
    EXPECT_EQ(std::string(made.to_string().view()),
              "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//sgcl//sgcl//EN\r\nBEGIN:VEVENT\r\nUID:x\r\nSUMMARY:Lunch\\, with friends\r\n"
              "DTSTART;TZID=Europe/Warsaw:20261006T120000\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n");
    EXPECT_EQ(made.components()[0].property_of("DTSTART")->as_datetime()->hour(), 12);
    EXPECT_EQ(made.set(content_line("VERSION", "3.0")).text("VERSION", "?"), "3.0");
    EXPECT_EQ(made.set(content_line("VERSION", "3.0")).properties().size(), 2u);
    EXPECT_EQ(made.erase("prodid").properties().size(), 1u);
    EXPECT_EQ(made.set(content_line("X-NEW", "1")).properties().size(), 3u);
    EXPECT_NE(made, made.erase("PRODID"));
    EXPECT_THROW(icalendar("A B").to_string(), sgcl::invalid_argument);
}

TEST(Icalendar_Tests, Errors) {
    struct {
        const char* text;
        errc code;
        uint32_t line;
        uint32_t column;
    } cases[] = {
        {"BEGIN:VCALENDAR\r\nEND:VEVENT\r\n", errc::mismatched_tag, 2, 1},
        {"BEGIN:VCALENDAR\r\nX:1\r\n", errc::unexpected_end, 3, 1},
        {"END:VCALENDAR\r\n", errc::mismatched_tag, 1, 1},
        {"X:1\r\n", errc::syntax, 1, 1},
        {"BEGIN:VCALENDAR\r\nNO COLON\r\nEND:VCALENDAR\r\n", errc::syntax, 2, 3},
        {"BEGIN:VCALENDAR\r\nA;B=\"x:y\r\nEND:VCALENDAR\r\n", errc::unexpected_end, 2, 5},
        {"BEGIN:VCALENDAR\r\nA:\x01\r\nEND:VCALENDAR\r\n", errc::invalid_character, 2, 3},
        {"BEGIN:VCALENDAR\r\nG.A:x\r\nEND:VCALENDAR\r\n", errc::syntax, 2, 2},
        {"BEGIN:VCALENDAR\r\nA:\xff\r\nEND:VCALENDAR\r\n", errc::invalid_utf8, 2, 1},
        {"BEGIN:VEVENT\r\nEND:VEVENT\r\n", errc::syntax, 1, 1},
        {"", errc::syntax, 1, 1},
        {"BEGIN:VCALENDAR\r\nEND:VCALENDAR\r\nBEGIN:VCALENDAR\r\nEND:VCALENDAR\r\n", errc::syntax, 1, 1},
    };
    for (auto& c : cases) {
        auto r = icalendar::parse(c.text);
        ASSERT_FALSE(r) << c.text;
        EXPECT_EQ(r.error().code(), c.code) << c.text << ": " << r.error().message();
        EXPECT_EQ(r.error().line(), c.line) << c.text;
        EXPECT_EQ(r.error().column(), c.column) << c.text;
    }
    EXPECT_EQ(icalendar::parse_all("BEGIN:VCALENDAR\r\nEND:VCALENDAR\r\nBEGIN:VCALENDAR\r\nEND:VCALENDAR\r\n")->size(), 2u);
    EXPECT_EQ(icalendar::parse_all("")->size(), 0u);
    icalendar::options o;
    o.max_depth = 2;
    EXPECT_TRUE(icalendar::parse("BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n", o));
    EXPECT_EQ(icalendar::parse("BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nBEGIN:VALARM\r\nEND:VALARM\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n", o).error().code(),
              errc::depth_limit);
    o.max_size = 10;
    EXPECT_EQ(icalendar::parse("BEGIN:VCALENDAR\r\nEND:VCALENDAR\r\n", o).error().code(), errc::limit_exceeded);
}

TEST(Icalendar_Tests, Unfolding) {
    // CRLF and LF, a space or a tab after the break, a fold inside a UTF-8 sequence and inside a parameter
    auto c = icalendar::parse("BEGIN:VCALENDAR\nSUMMARY:one \r\n two\n\tthree\r\nX-P;CN=\"Dó\r\n e\":z\r\nDESCRIPTION:\xc5\r\n \x82\r\n\r\nEND:VCALENDAR");
    ASSERT_TRUE(c) << c.error().message();
    EXPECT_EQ(c->text("SUMMARY", "?"), "one twothree");
    EXPECT_EQ(c->property_of("X-P")->param("CN"), "Dóe");
    EXPECT_EQ(c->text("DESCRIPTION", "?"), "\xc5\x82");
    EXPECT_TRUE(icalendar::parse("\xEF\xBB\xBF" "BEGIN:VCALENDAR\r\nEND:VCALENDAR\r\n"));
}

TEST(Icalendar_Tests, TimeZonesAndOccurrences) {
    std::string vtimezone = "BEGIN:VTIMEZONE\r\nTZID:Eastern (custom)\r\n"
                            "BEGIN:STANDARD\r\nDTSTART:19671029T020000\r\nRRULE:FREQ=YEARLY;BYDAY=-1SU;BYMONTH=10;UNTIL=20061029T060000Z\r\n"
                            "TZOFFSETFROM:-0400\r\nTZOFFSETTO:-0500\r\nTZNAME:EST\r\nEND:STANDARD\r\n"
                            "BEGIN:STANDARD\r\nDTSTART:20071104T020000\r\nRRULE:FREQ=YEARLY;BYDAY=1SU;BYMONTH=11\r\nTZOFFSETFROM:-0400\r\nTZOFFSETTO:-0500\r\nEND:STANDARD\r\n"
                            "BEGIN:DAYLIGHT\r\nDTSTART:19870405T020000\r\nRRULE:FREQ=YEARLY;BYDAY=1SU;BYMONTH=4;UNTIL=20060402T070000Z\r\n"
                            "TZOFFSETFROM:-0500\r\nTZOFFSETTO:-0400\r\nEND:DAYLIGHT\r\n"
                            "BEGIN:DAYLIGHT\r\nDTSTART:20070311T020000\r\nRRULE:FREQ=YEARLY;BYDAY=2SU;BYMONTH=3\r\nTZOFFSETFROM:-0500\r\nTZOFFSETTO:-0400\r\nEND:DAYLIGHT\r\n"
                            "END:VTIMEZONE\r\n";
    icalendar c = cal(vtimezone + "BEGIN:VEVENT\r\nUID:a\r\nDTSTART;TZID=Eastern (custom):19970902T090000\r\n"
                                  "RRULE:FREQ=WEEKLY;BYDAY=TU;COUNT=6\r\nEXDATE;TZID=Eastern (custom):19970916T090000,19970923T090000\r\n"
                                  "RDATE;TZID=Eastern (custom):19970910T100000,19971231T120000\r\nRDATE;VALUE=PERIOD:19971105T140000Z/PT1H\r\nEND:VEVENT\r\n"
                                  "BEGIN:VEVENT\r\nUID:b\r\nDTSTART;TZID=America/New_York:20260302T090000\r\nRRULE:FREQ=WEEKLY;COUNT=3\r\nEND:VEVENT\r\n"
                                  "BEGIN:VEVENT\r\nUID:c\r\nDTSTART;VALUE=DATE:20260101\r\nRRULE:FREQ=YEARLY;COUNT=3\r\nEXDATE;VALUE=DATE:20270101\r\nEND:VEVENT\r\n"
                                  "BEGIN:VEVENT\r\nUID:d\r\nDTSTART:20260101T080000\r\nRRULE:FREQ=DAILY;COUNT=2\r\nEND:VEVENT\r\n"
                                  "BEGIN:VEVENT\r\nUID:e\r\nDTSTART;TZID=Nowhere/Unknown:20260101T080000\r\nEND:VEVENT\r\n"
                                  "BEGIN:VEVENT\r\nUID:f\r\nSUMMARY:no start\r\nEND:VEVENT\r\n");
    auto events = c.components_of("VEVENT");
    auto from = time::datetime::from_unix(0, time::zone::utc());
    auto to = time::datetime::from_unix(4000000000, time::zone::utc());
    auto a = c.occurrences(events[0], from, to);
    std::vector<std::string> got;
    for (auto& t : a) {
        got.push_back(std::string(t.to_string().view()));
    }
    EXPECT_EQ(got, (std::vector<std::string>{"1997-09-02T09:00:00-04:00", "1997-09-09T09:00:00-04:00", "1997-09-10T10:00:00-04:00",
                                             "1997-09-30T09:00:00-04:00", "1997-10-07T09:00:00-04:00", "1997-11-05T09:00:00-05:00",
                                             "1997-12-31T12:00:00-05:00"}));
    // the VTIMEZONE's offsets on both sides of its changes, a time it skips and one it shows twice
    auto start = *events[0].property_of("DTSTART");
    EXPECT_EQ(c.datetime_of(start)->unix(), 873205200);
    auto at_rule = [&](const char* value) { return c.datetime_of(content_line("DTSTART", {{"TZID", {"Eastern (custom)"}}}, value)); };
    EXPECT_EQ(at_rule("20260308T013000")->to_string(), "2026-03-08T01:30:00-05:00");
    EXPECT_EQ(at_rule("20260308T023000")->to_string(), "2026-03-08T03:30:00-04:00");
    EXPECT_EQ(at_rule("20261101T013000")->to_string(), "2026-11-01T01:30:00-04:00");
    EXPECT_EQ(at_rule("20261101T023000")->to_string(), "2026-11-01T02:30:00-05:00");
    EXPECT_EQ(at_rule("20000402T030000")->to_string(), "2000-04-02T03:00:00-04:00");
    EXPECT_EQ(at_rule("19500101T000000")->to_string(), "1950-01-01T00:00:00-04:00");   // before the first onset: its offset from
    // a system zone's TZID
    auto b = c.occurrences(events[1], from, to);
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[1].to_string(), "2026-03-09T09:00:00-04:00");
    // all-day: dates at midnight in the zone given, EXDATE of a DATE
    auto d = c.occurrences(events[2], from, to, 10, time::zone::utc());
    ASSERT_EQ(d.size(), 2u);
    EXPECT_EQ(d[1].to_string(), "2028-01-01T00:00:00Z");
    // floating in the zone given
    auto warsaw = time::zone::load("Europe/Warsaw").value();
    EXPECT_EQ(c.occurrences(events[3], from, to, 10, warsaw)[0].to_string(), "2026-01-01T08:00:00+01:00");
    EXPECT_EQ(c.occurrences(events[3], from, to, 1, warsaw).size(), 1u);
    EXPECT_TRUE(c.occurrences(events[4], from, to).empty());   // an unknown TZID
    EXPECT_FALSE(c.datetime_of(*events[4].property_of("DTSTART")));
    EXPECT_TRUE(c.occurrences(events[5], from, to).empty());
    EXPECT_FALSE(c.datetime_of(content_line("X", "nope")));
}

TEST(Icalendar_Tests, Boundaries) {
    icalendar none;
    EXPECT_EQ(none.name(), "VCALENDAR");
    EXPECT_EQ(none.properties().size(), 2u);
    EXPECT_TRUE(none.components().empty());
    EXPECT_TRUE(icalendar("VEVENT").properties().empty());
    EXPECT_EQ(icalendar("vevent").name(), "VEVENT");
    EXPECT_EQ(*icalendar::parse(none.to_string()), none);
    icalendar a = none;
    icalendar b = std::move(a);
    EXPECT_EQ(b, none);
    std::string text = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nDTSTART;TZID=\"x\":20260101T000000\r\nSUMMARY:a\\, b\r\n c\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";
    for (size_t n = 0; n <= text.size(); ++n) {
        (void)icalendar::parse(sgcl::string(text.substr(0, n)));
    }
    for (size_t piece : {1, 3, 4096}) {
        sgcl::io::reader in(make_tracked<dribble>(text, piece));
        auto r = icalendar::parse(in);
        ASSERT_TRUE(r);
        EXPECT_EQ(r->components().size(), 1u);
    }
    sgcl::io::reader bad(make_tracked<failing>("BEGIN:VCALENDAR"));
    EXPECT_EQ(icalendar::parse(bad).error().code(), errc::io);
    auto task = sgcl::async::spawn([](std::string doc) -> sgcl::async::task<int> {
        sgcl::io::reader in(make_tracked<dribble>(doc, 2));
        auto r = co_await icalendar::async_parse(in);
        co_return r && r->components().size() == 1 ? 1 : -1;
    }(text));
    EXPECT_EQ(task.wait(), 1);
    sgcl::async::scheduler::stop();
}
