//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// time::cron: any text read as an expression (an error has its byte within
// the text), and every expression read searched from instants taken from
// the input, against a scan of the clock in the harness: in UTC the first
// matching second (by minutes for five fields over three days, by seconds
// for six over two hours) is next(), and nothing within the scan means
// next() lies past it; in Europe/Warsaw, whose clock is skipped and
// repeated, the same scan of instants for an expression of wildcards
// (ISC's rule: every instant whose time matches), and for one of fixed
// times a result that matches or is a change of the zone (a jump), never
// in the second pass of a repeated hour. next() is strictly after its
// instant and matches() agrees with it; next(after, n) is next() n times.
// The matcher here is the harness's own: the fields of the zone's clock
// (datetime's) against the bits read, the day fields by the rules written
// out again.
//
// The input: a byte of which zone and how many instants, eight bytes per
// instant, then the text.
//
// Built with libFuzzer (tests/fuzz/run.sh tests/time/fuzz/cron_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/time.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <source_location>

namespace {
    using namespace sgcl;

    void check(bool ok, std::source_location at = std::source_location::current()) {
        if (!ok) {
            std::fprintf(stderr, "cron_fuzz: check failed at line %u\n", (unsigned)at.line());
            __builtin_trap();
        }
    }

    using Fields = time::detail::CronFields;

    // The harness's own matcher of a datetime's fields
    bool day_ok(const Fields& f, int day, int length, int wd) {
        auto nearest = [&](int n) {
            int w = ((wd + (n - day)) % 7 + 7) % 7;
            if (w == 6) {
                return n == 1 ? 3 : n - 1;
            }
            if (w == 0) {
                return n == length ? n - 2 : n + 1;
            }
            return n;
        };
        bool dom = (f.days >> day & 1) != 0;
        for (int n = 0; n <= 30; ++n) {
            dom = dom || ((f.before_last >> n & 1) && day == length - n);
        }
        for (int n = 1; n <= 31; ++n) {
            dom = dom || ((f.nearest >> n & 1) && n <= length && nearest(n) == day);
        }
        dom = dom || (f.last_weekday && nearest(length) == day);
        bool dow = (f.weekdays >> wd & 1) || ((f.last_of >> wd & 1) && day > length - 7) || (f.nth >> (wd * 8 + (day + 6) / 7) & 1);
        if (f.day_star && f.weekday_star) {
            return true;
        }
        if (f.day_star) {
            return dow;
        }
        if (f.weekday_star) {
            return dom;
        }
        return dom || dow;
    }

    bool fields_ok(const Fields& f, const time::datetime& t) {
        int wd = int(t.weekday()) % 7;   // Monday 1 .. Sunday 7 -> 0 Sunday
        int length = t.date().days_in_month();
        return (f.seconds >> t.second() & 1) && (f.minutes >> t.minute() & 1) && (f.hours >> t.hour() & 1) && (f.months >> int(t.month()) & 1) &&
               day_ok(f, t.day(), length, wd);
    }

    bool is_change(const time::zone& z, int64_t s) {
        auto n = z.next_transition(time::datetime::from_unix(s - 1, z));
        return n && n->unix() == s;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const bool in_warsaw = data[0] & 1;
    const int instants = 1 + (data[0] >> 1 & 3);
    if (size < 1 + 8 * size_t(instants)) {
        return 0;
    }
    int64_t afters[4];
    for (int i = 0; i < instants; ++i) {
        uint64_t v;
        std::memcpy(&v, data + 1 + 8 * i, 8);
        afters[i] = 1735689600 + int64_t(v % (40ull * 366 * 86400));   // 2025 .. 2065
    }
    size_t text_at = 1 + 8 * size_t(instants);
    string text(reinterpret_cast<const char*>(data + text_at), size - text_at);
    time::zone z = in_warsaw ? time::zone::load("Europe/Warsaw").value_or(time::zone::utc()) : time::zone::utc();   // read once, from memory after
    auto c = time::cron::parse(text, z);
    if (!c) {
        check(c.error().offset() <= text.size());
        return 0;
    }
    const Fields& f = time::detail::CronAccess::fields(*c);
    const bool six = f.seconds != 1;
    for (int i = 0; i < instants; ++i) {
        int64_t after = afters[i];
        auto n = c->next(time::datetime::from_unix(after, z));
        if (n) {
            check(n->unix() > after);
            check(c->matches(*n));
        }
        auto many = c->next(time::datetime::from_unix(after, z), 3);
        check(many.size() <= 3);
        if (n) {
            check(!many.empty() && many[0] == *n);
            for (size_t k = 1; k < many.size(); ++k) {
                auto again = c->next(many[k - 1]);
                check(again && *again == many[k]);
            }
        } else {
            check(many.empty());
        }
        if (in_warsaw && f.fixed) {
            if (n) {
                check(fields_ok(f, *n) || is_change(z, n->unix()));
            }
            continue;
        }
        // the scan: every instant whose time matches
        int64_t step = six ? 1 : 60;
        int64_t start = six ? after + 1 : (after / 60 + 1) * 60;
        int64_t horizon = six ? 7200 : 3 * 86400;
        optional<int64_t> found;
        for (int64_t s = start; s <= after + horizon; s += step) {
            if (fields_ok(f, time::datetime::from_unix(s, z))) {
                found = s;
                break;
            }
        }
        if (found) {
            check(n && n->unix() == *found);
        } else {
            check(!n || n->unix() > after + horizon - step);
        }
    }
    return 0;
}
