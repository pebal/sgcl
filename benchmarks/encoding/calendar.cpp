//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// iCalendar and vCard of the encoding module. Go's and Python's standard
// libraries have neither and no library of them is on the machine (no
// libical, no Python icalendar or dateutil): the numbers stand alone.
//   calendar sgcl [op=ical_parse] [seconds=2]
//
//   ical_parse    icalendar::parse of 2000 events (a VTIMEZONE, summaries
//                 folded and escaped, attendees, RRULEs), the tree made each time
//   ical_write    to_string of it parsed once
//   expand        occurrences of 100 recurring events over ten years, in the
//                 calendar's VTIMEZONE
//   rrule         recurrence::occurrences of FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;
//                 BYSETPOS=-1 over a hundred years in New York
//   vcard_parse   vcard::parse_all of 2000 cards
//
// Prints nanoseconds per operation and, for the texts, megabytes per second.
#include "benchmarks/common.h"
#include "sgcl/encoding/encoding.h"

#include <cstdlib>
#include <cstring>
#include <string>

using namespace sgcl;
using encoding::icalendar;
using encoding::recurrence;
using encoding::vcard;

namespace {
    volatile size_t sink = 0;

    std::string ical_text() {
        std::string t = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//bench//bench//EN\r\n"
                        "BEGIN:VTIMEZONE\r\nTZID:Eastern\r\n"
                        "BEGIN:STANDARD\r\nDTSTART:19671029T020000\r\nRRULE:FREQ=YEARLY;BYDAY=1SU;BYMONTH=11\r\nTZOFFSETFROM:-0400\r\nTZOFFSETTO:-0500\r\nEND:STANDARD\r\n"
                        "BEGIN:DAYLIGHT\r\nDTSTART:19870405T020000\r\nRRULE:FREQ=YEARLY;BYDAY=2SU;BYMONTH=3\r\nTZOFFSETFROM:-0500\r\nTZOFFSETTO:-0400\r\nEND:DAYLIGHT\r\n"
                        "END:VTIMEZONE\r\n";
        for (int i = 0; i < 2000; ++i) {
            std::string n = std::to_string(i);
            t += "BEGIN:VEVENT\r\nUID:" + n + "@bench.example.com\r\nDTSTAMP:20260101T000000Z\r\n";
            t += "DTSTART;TZID=Eastern:2026" + std::string(i % 12 < 9 ? "0" : "") + std::to_string(i % 12 + 1) + "1" + std::to_string(i % 9) + "T090000\r\n";
            t += "DTEND;TZID=Eastern:20260101T100000\r\n";
            t += "SUMMARY:Meeting number " + n + "\\, with the team\; agenda attached\r\n";
            t += "DESCRIPTION:A longer description of the meeting that runs past the seventy-five\r\n  octets a line may hold\\, so that it is folded\\nand has a second line.\r\n";
            t += "ATTENDEE;CN=\"Doe, Jane\";ROLE=REQ-PARTICIPANT;PARTSTAT=ACCEPTED:mailto:jane@example.com\r\n";
            t += "ATTENDEE;CN=John Smith;RSVP=TRUE:mailto:john@example.com\r\n";
            if (i % 4 == 0) {
                t += "RRULE:FREQ=WEEKLY;BYDAY=MO,WE;UNTIL=20361231T000000Z\r\n";
            }
            t += "BEGIN:VALARM\r\nACTION:DISPLAY\r\nTRIGGER:-PT15M\r\nDESCRIPTION:Reminder\r\nEND:VALARM\r\nEND:VEVENT\r\n";
        }
        return t + "END:VCALENDAR\r\n";
    }

    std::string vcard_text() {
        std::string t;
        for (int i = 0; i < 2000; ++i) {
            std::string n = std::to_string(i);
            t += "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:Person " + n + "\r\nN:Person;" + n + ";;;\r\n";
            t += "EMAIL;TYPE=work:person" + n + "@example.com\r\nTEL;VALUE=uri;TYPE=\"work,voice\":tel:+1-555-" + n + "\r\n";
            t += "ADR;TYPE=home:;;" + n + " Main Street;Springfield;IL;62701;USA\r\nNOTE:A note\\, with an escape\r\nEND:VCARD\r\n";
        }
        return t;
    }

    template<class F>
    double timed(double seconds, long& count, F&& f) {
        for (int i = 0; i < 3; ++i) {
            f();
        }
        auto t0 = bench::Clock::now();
        count = 0;
        while (bench::seconds_since(t0) < seconds) {
            f();
            ++count;
        }
        return bench::seconds_since(t0) / double(count) * 1e9;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    const char* op = argc > 2 ? argv[2] : "ical_parse";
    double seconds = argc > 3 ? std::atof(argv[3]) : 2.0;
    if (std::strcmp(variant, "sgcl")) {
        std::fprintf(stderr, "usage: calendar sgcl [ical_parse|ical_write|expand|rrule|vcard_parse] [seconds]\n");
        return 2;
    }
    long count = 0;
    double ns = 0;
    size_t bytes = 0;
    if (!std::strcmp(op, "ical_parse") || !std::strcmp(op, "ical_write") || !std::strcmp(op, "expand")) {
        string text(ical_text());
        bytes = text.size();
        if (!std::strcmp(op, "ical_parse")) {
            ns = timed(seconds, count, [&] { sink += icalendar::parse(text)->components().size(); });
        } else {
            icalendar c = icalendar::parse(text).value();
            if (!std::strcmp(op, "ical_write")) {
                ns = timed(seconds, count, [&] { sink += c.to_string().size(); });
            } else {
                bytes = 0;
                auto events = c.components_of("VEVENT");
                auto from = time::datetime::from_unix(1767225600, time::zone::utc());   // 2026
                auto to = time::datetime::from_unix(2082758400, time::zone::utc());     // 2036
                ns = timed(seconds, count, [&] {
                    size_t n = 0;
                    for (size_t i = 0; i < 400; i += 4) {
                        n += c.occurrences(events[i], from, to).size();
                    }
                    sink += n;
                });
            }
        }
    } else if (!std::strcmp(op, "rrule")) {
        auto ny = time::zone::load("America/New_York").value();
        auto start = time::date(1950, 1, 31).at(9, 0, ny);
        recurrence r("FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;BYSETPOS=-1");
        auto to = time::date(2050, 1, 1).at(0, 0, ny);
        ns = timed(seconds, count, [&] { sink += r.occurrences(start, start, to).size(); });
    } else if (!std::strcmp(op, "vcard_parse")) {
        string text(vcard_text());
        bytes = text.size();
        ns = timed(seconds, count, [&] { sink += vcard::parse_all(text)->size(); });
    } else {
        std::fprintf(stderr, "calendar: no op called %s\n", op);
        return 2;
    }
    if (bytes) {
        std::printf("%s op=%s bytes=%zu count=%ld ns/op=%.0f MB/s=%.0f\n", variant, op, bytes, count, ns, double(bytes) / ns * 1e3);
    } else {
        std::printf("%s op=%s count=%ld ns/op=%.0f\n", variant, op, count, ns);
    }
}
