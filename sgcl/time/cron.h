//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../async/coroutine.h"
#include "../async/select.h"
#include "../async/stop_token.h"
#include "../async/timer.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/string.h"
#include "../core/vector.h"
#include "date.h"
#include "datetime.h"
#include "error.h"
#include "zone.h"

#include <bit>
#include <concepts>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

namespace sgcl::time {
    // A cron expression and the zone its times are read in. Five fields
    // (minute, hour, day of the month, month, day of the week) or six (the
    // second first), or a name (@yearly, @annually, @monthly, @weekly,
    // @daily, @midnight, @hourly); in every field `*`, numbers, ranges
    // `a-b`, steps `*/n`, `a-b/n` and `a/n`, lists; the months and the
    // days of the week by their English names too (JAN, MON), the days of
    // the week 0 to 7 (0 and 7 Sunday); Quartz's `?` for `*` in the day
    // fields, and its L, W and #: the last day of the month (`L`), n days
    // before it (`L-n`), the weekday nearest a day (`15W`), the last
    // weekday (`LW`), the last Friday (`5L`), the second Monday (`1#2`).
    // Both day fields restricted, a day matches either (Vixie cron's OR);
    // a day field is unrestricted when it begins with `*` or is `?`.
    //
    // The times are those the zone's clock shows. A time the clock skips
    // or shows twice (a change for daylight saving time) follows ISC
    // cron's rule: an expression of fixed times (neither the minute nor
    // the hour a wildcard) fires once at the instant the clock jumps over
    // a skipped time and once, the first time, for a time shown twice; an
    // expression of wildcards fires at every instant whose time matches,
    // none in a skipped hour, twice in a repeated one. The search walks
    // the zone's periods of one offset each, inside which the clock and
    // the instant go together, from the instant asked to the next change
    // of the zone and on. To the second; a datetime's range ends in 2262,
    // and so does the search.
    class cron;

    namespace detail {
        struct CronFields {
            uint64_t seconds = 0;           // bits 0..59
            uint64_t minutes = 0;           // bits 0..59
            uint32_t hours = 0;             // bits 0..23
            uint32_t days = 0;              // bits 1..31
            uint32_t before_last = 0;       // L-n: bit n, 0..30 (L is bit 0)
            uint32_t nearest = 0;           // nW: bit n, 1..31
            uint16_t months = 0;            // bits 1..12
            uint8_t weekdays = 0;           // bits 0..6, 0 Sunday
            uint8_t last_of = 0;            // nL: bit of the weekday
            uint64_t nth = 0;               // n#k: bit weekday * 8 + k, k 1..5
            bool last_weekday = false;      // LW
            bool day_star = false;          // the day of the month unrestricted
            bool weekday_star = false;      // the day of the week unrestricted
            bool fixed = false;             // neither the minute nor the hour a wildcard (ISC's rule)

            friend bool operator==(const CronFields&, const CronFields&) noexcept = default;

            // Whether a day of a month matches the two day fields
            SGCL_INLINE_HOT bool day_matches(int day, int length, int weekday) const noexcept {
                if (day_star && weekday_star) {
                    return true;
                }
                if (day_star) {
                    return weekday_matches(day, length, weekday);
                }
                if (weekday_star) {
                    return month_day_matches(day, length, weekday);
                }
                return month_day_matches(day, length, weekday) || weekday_matches(day, length, weekday);
            }

            bool month_day_matches(int day, int length, int weekday) const noexcept {
                if (days >> day & 1) {
                    return true;
                }
                if (before_last != 0 && length - day >= 0 && length - day <= 30 && (before_last >> (length - day) & 1)) {
                    return true;
                }
                if (nearest != 0) {
                    for (uint32_t bits = nearest; bits != 0; bits &= bits - 1) {
                        int n = std::countr_zero(bits);
                        if (n <= length && nearest_weekday(n, length, weekday, day) == day) {
                            return true;
                        }
                    }
                }
                if (last_weekday && nearest_weekday(length, length, weekday, day) == day) {
                    return true;
                }
                return false;
            }

            bool weekday_matches(int day, int length, int weekday) const noexcept {
                if (weekdays >> weekday & 1) {
                    return true;
                }
                if ((last_of >> weekday & 1) && day + 7 > length) {
                    return true;
                }
                return (nth >> (weekday * 8 + (day - 1) / 7 + 1)) & 1;
            }

            // The weekday nearest day n of the month, not leaving it
            // (Quartz's W), given the weekday of `day` (0 Sunday)
            static int nearest_weekday(int n, int length, int weekday, int day) noexcept {
                int wd = ((weekday + (n - day)) % 7 + 7) % 7;   // the weekday of day n
                if (wd == 6) {                                  // Saturday: the Friday before, or the Monday after the 1st
                    return n == 1 ? n + 2 : n - 1;
                }
                if (wd == 0) {                                  // Sunday: the Monday after, or the Friday before the last
                    return n == length ? n - 2 : n + 1;
                }
                return n;
            }

            // The first time of day >= second (seconds from midnight) that
            // matches, or -1
            int time_of_day_from(int second) const noexcept {
                int h = second / 3600, m = second / 60 % 60, s = second % 60;
                for (; h < 24; ++h, m = 0, s = 0) {
                    if (!(hours >> h & 1)) {
                        continue;
                    }
                    for (; m < 60; ++m, s = 0) {
                        if (!(minutes >> m & 1)) {
                            continue;
                        }
                        uint64_t left = seconds & (~uint64_t(0) << s);
                        if (left != 0) {
                            return h * 3600 + m * 60 + std::countr_zero(left);
                        }
                    }
                }
                return -1;
            }

            // The first time of the clock >= wall (seconds since 1970 of
            // the clock read as if UTC) that matches, or nothing before the
            // end of a datetime's range (2262)
            optional<int64_t> next_wall(int64_t wall) const noexcept {
                constexpr int64_t End = 9223372036LL;   // INT64_MAX ns in seconds
                int64_t days = floor_div(wall, int64_t(86400));
                int second = int(wall - days * 86400);
                while (days * 86400 <= End) {
                    Civil c = civil_from_days(int32_t(days));
                    if (!(months >> c.month & 1)) {   // to the first of the next month of the field
                        int y = c.year;
                        unsigned m = c.month;
                        do {
                            if (++m == 13) {
                                m = 1;
                                ++y;
                            }
                        } while (!(months >> m & 1));
                        days = days_from_civil(y, m);
                        second = 0;
                        continue;
                    }
                    int length = int(days_from_civil(c.month == 12 ? c.year + 1 : c.year, c.month == 12 ? 1 : c.month + 1) - days_from_civil(c.year, c.month));
                    int weekday = int(((days + 4) % 7 + 7) % 7);
                    if (day_matches(int(c.day), length, weekday)) {
                        int t = time_of_day_from(second);
                        if (t >= 0) {
                            int64_t w = days * 86400 + t;
                            return w <= End ? optional<int64_t>(w) : nullopt;
                        }
                    }
                    ++days;
                    second = 0;
                }
                return nullopt;
            }

            // The first instant > after (seconds since 1970) whose time of
            // the zone's clock matches, by ISC's rule at the zone's changes
            optional<int64_t> next_instant(const zone_data& z, int64_t after) const noexcept {
                int64_t s = after + 1;
                int64_t floor_wall = INT64_MIN;   // fixed times: the times of a repeated hour shown once already
                if (fixed) {
                    if (auto p = zone_access::previous_change(z, s + 1)) {
                        int32_t before = zone_access::offset_at(z, *p - 1), now = zone_access::offset_at(z, *p);
                        if (before > now && s < *p + (before - now)) {   // in the second pass of a repeated hour: its times fired in the first
                            floor_wall = *p + before;
                        }
                        if (before < now && s == *p) {   // at the jump itself: the times it skipped fire now
                            if (auto w = next_wall(*p + before); w && *w < *p + now) {
                                return s;
                            }
                        }
                    }
                }
                for (int guard = 0; guard < 4096; ++guard) {
                    int32_t offset = zone_access::offset_at(z, s);
                    optional<int64_t> change = zone_access::next_change(z, s);
                    int64_t wall_from = s + offset > floor_wall ? s + offset : floor_wall;
                    optional<int64_t> w = next_wall(wall_from);
                    if (!w) {
                        return nullopt;
                    }
                    int64_t at = *w - offset;
                    if (!change || at < *change) {
                        return at;
                    }
                    int32_t next_offset = zone_access::offset_at(z, *change);
                    if (next_offset > offset && *w < *change + next_offset) {   // a time the clock skips
                        if (fixed) {
                            return *change;   // fired at the jump
                        }
                    }
                    floor_wall = fixed && next_offset < offset ? *change + offset : INT64_MIN;   // a repeated hour: fixed times not again
                    s = *change;
                }
                return nullopt;
            }
        };

        struct CronAccess;
    }

    class cron {
    public:
        // The expression read in the zone; an error with the byte it
        // stopped on
        static expected<cron, error> parse(const string& expression, const time::zone& z = time::zone::local()) noexcept;

        // The cron an expression written in the program names, or
        // bad_expected_access<time::error> with parse's message (DESIGN
        // 234): a literal is constructed, a text from outside parsed
        SGCL_INLINE_HOT explicit cron(const string& expression, const time::zone& z = time::zone::local())
        : cron(parse(expression, z).value()) {
        }

        // The first time strictly after `after`, in the cron's zone;
        // nothing when there is none before 2262 (February 30th)
        optional<datetime> next(const datetime& after) const noexcept {
            const detail::zone_data& z = detail::zone_access::data(_zone);
            int64_t s = detail::floor_div(after.unix_nano(), detail::NanosPerSecond);
            if (auto t = _f.next_instant(z, s)) {
                return datetime::from_unix(*t, _zone);
            }
            return nullopt;
        }

        // The next count times after `after`, fewer when the times end
        vector<datetime> next(const datetime& after, size_t count) const {
            vector<datetime> out;
            out.reserve(count);
            const detail::zone_data& z = detail::zone_access::data(_zone);
            int64_t s = detail::floor_div(after.unix_nano(), detail::NanosPerSecond);
            for (size_t i = 0; i < count; ++i) {
                auto t = _f.next_instant(z, s);
                if (!t) {
                    break;
                }
                out.push_back(datetime::from_unix(*t, _zone));
                s = *t;
            }
            return out;
        }

        // Whether t, to the second, is one of the cron's times
        bool matches(const datetime& t) const noexcept {
            int64_t s = detail::floor_div(t.unix_nano(), detail::NanosPerSecond);
            const detail::zone_data& z = detail::zone_access::data(_zone);
            auto n = _f.next_instant(z, s - 1);
            return n && *n == s;
        }

        SGCL_INLINE_HOT time::zone zone() const noexcept {
            return _zone;
        }

        // The expression as it was given
        SGCL_INLINE_HOT string to_string() const noexcept {
            return _text;
        }

        // The same times: the same fields and the same zone, however
        // written (`0 0 * * *` and `@daily`)
        SGCL_INLINE_HOT friend bool operator==(const cron& a, const cron& b) noexcept {
            return a._f == b._f && a._zone == b._zone;
        }

    private:
        friend struct detail::CronAccess;

        SGCL_INLINE_HOT cron(const detail::CronFields& f, const string& text, const time::zone& z) noexcept
        : _f(f)
        , _text(text)
        , _zone(z) {
        }

        detail::CronFields _f;
        string _text;
        time::zone _zone;
    };

    namespace detail {
        // The reader of an expression: fields split at white space, items
        // at commas, each a number, a name, a range, a step or a special
        class CronParser {
        public:
            SGCL_INLINE_HOT explicit CronParser(const string& text) noexcept
            : _s(text.data())
            , _n(text.size()) {
            }

            optional<error> read(CronFields& f) noexcept {
                size_t i = _skip(0);
                if (i < _n && _s[i] == '@') {
                    return _name(i, f);
                }
                size_t starts[7];
                size_t ends[7];
                int count = 0;
                while (i < _n) {
                    if (count == 6) {
                        return error("a cron expression has 5 or 6 fields", i);
                    }
                    starts[count] = i;
                    while (i < _n && !_space(_s[i])) {
                        ++i;
                    }
                    ends[count++] = i;
                    i = _skip(i);
                }
                if (count < 5) {
                    return error("a cron expression has 5 or 6 fields", _n);
                }
                int k = 0;
                if (count == 6) {
                    if (auto e = _field(starts[0], ends[0], 0, 59, Plain, f.seconds)) {
                        return e;
                    }
                    k = 1;
                } else {
                    f.seconds = 1;
                }
                if (auto e = _field(starts[k], ends[k], 0, 59, Plain, f.minutes)) {
                    return e;
                }
                uint64_t bits = 0;
                if (auto e = _field(starts[k + 1], ends[k + 1], 0, 23, Plain, bits)) {
                    return e;
                }
                f.hours = uint32_t(bits);
                f.fixed = _s[starts[k]] != '*' && _s[starts[k + 1]] != '*';
                bits = 0;
                if (auto e = _day_field(starts[k + 2], ends[k + 2], f)) {
                    return e;
                }
                if (auto e = _field(starts[k + 3], ends[k + 3], 1, 12, Months, bits)) {
                    return e;
                }
                f.months = uint16_t(bits);
                if (auto e = _weekday_field(starts[k + 4], ends[k + 4], f)) {
                    return e;
                }
                return nullopt;
            }

        private:
            enum Names { Plain, Months, Weekdays };

            static bool _space(char c) noexcept {
                return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
            }

            size_t _skip(size_t i) const noexcept {
                while (i < _n && _space(_s[i])) {
                    ++i;
                }
                return i;
            }

            static char _lower(char c) noexcept {
                return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
            }

            optional<error> _name(size_t i, CronFields& f) const noexcept {
                size_t e = i;
                while (e < _n && !_space(_s[e])) {
                    ++e;
                }
                if (_skip(e) != _n) {
                    return error("a named cron expression is one word", _skip(e));
                }
                char w[12] = {};
                size_t len = e - i;
                if (len < sizeof w) {
                    for (size_t k = 0; k < len; ++k) {
                        w[k] = _lower(_s[i + k]);
                    }
                }
                auto is = [&](const char* name) {
                    return len < sizeof w && std::string_view(w, len) == name;
                };
                f = CronFields{};
                f.seconds = 1;
                f.minutes = 1;
                f.hours = 1;
                f.days = ~uint32_t(1);
                f.months = uint16_t(0x1FFE);
                f.weekdays = 0x7F;
                f.day_star = true;
                f.weekday_star = true;
                f.fixed = true;
                if (is("@yearly") || is("@annually")) {
                    f.days = 1u << 1;
                    f.months = 1u << 1;
                    f.day_star = false;
                } else if (is("@monthly")) {
                    f.days = 1u << 1;
                    f.day_star = false;
                } else if (is("@weekly")) {
                    f.weekdays = 1;
                    f.weekday_star = false;
                } else if (is("@daily") || is("@midnight")) {
                } else if (is("@hourly")) {
                    f.hours = 0xFFFFFF;
                    f.fixed = false;
                } else {
                    return error("an unknown cron name", i);
                }
                return nullopt;
            }

            // A number, or a name of the field's: its value and the end
            optional<error> _value(size_t& i, size_t end, Names names, int& v) const noexcept {
                if (i < end && _s[i] >= '0' && _s[i] <= '9') {
                    v = 0;
                    size_t start = i;
                    while (i < end && _s[i] >= '0' && _s[i] <= '9') {
                        if (i - start >= 4) {
                            return error("a number too large for a cron field", start);
                        }
                        v = v * 10 + (_s[i++] - '0');
                    }
                    return nullopt;
                }
                if (names != Plain && i + 3 <= end) {
                    static constexpr const char* MonthNames[] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
                    static constexpr const char* DayNames[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
                    char w[3] = {_lower(_s[i]), _lower(_s[i + 1]), _lower(_s[i + 2])};
                    if (names == Months) {
                        for (int k = 0; k < 12; ++k) {
                            if (std::string_view(w, 3) == MonthNames[k]) {
                                v = k + 1;
                                i += 3;
                                return nullopt;
                            }
                        }
                    } else {
                        for (int k = 0; k < 7; ++k) {
                            if (std::string_view(w, 3) == DayNames[k]) {
                                v = k;
                                i += 3;
                                return nullopt;
                            }
                        }
                    }
                }
                return error(names == Plain ? "a number expected in a cron field" : "a number or a name expected in a cron field", i);
            }

            // An item `*`, `a`, `a-b`, with `/n`, into bits
            optional<error> _range(size_t i, size_t end, int lo, int hi, Names names, uint64_t& bits) const noexcept {
                int a = lo, b = hi, step = 1;
                size_t at = i;
                bool single = false;
                if (i < end && _s[i] == '*') {
                    ++i;
                } else {
                    if (auto e = _value(i, end, names, a)) {
                        return e;
                    }
                    b = a;
                    single = true;
                    if (i < end && _s[i] == '-') {
                        single = false;
                        ++i;
                        if (auto e = _value(i, end, names, b)) {
                            return e;
                        }
                    }
                    if (a < lo || a > hi || b < lo || b > hi) {
                        return error("a value out of the cron field's range", at);
                    }
                    if (b < a) {
                        return error("a cron range backwards", at);
                    }
                }
                if (i < end && _s[i] == '/') {
                    size_t s = ++i;
                    if (auto e = _value(i, end, Plain, step)) {
                        return e;
                    }
                    if (step == 0) {
                        return error("a cron step of zero", s);
                    }
                    if (single) {   // a/n: from a to the end
                        b = hi;
                    }
                }
                if (i != end) {
                    return error("an unexpected character in a cron field", i);
                }
                for (int v = a; v <= b; v += step) {
                    bits |= uint64_t(1) << v;
                }
                return nullopt;
            }

            optional<error> _field(size_t i, size_t end, int lo, int hi, Names names, uint64_t& bits) const noexcept {
                bits = 0;
                while (i <= end) {
                    size_t e = i;
                    while (e < end && _s[e] != ',') {
                        ++e;
                    }
                    if (e == i) {
                        return error("an empty item in a cron field", i);
                    }
                    if (auto err = _range(i, e, lo, hi, names, bits)) {
                        return err;
                    }
                    i = e + 1;
                }
                return nullopt;
            }

            optional<error> _day_field(size_t i, size_t end, CronFields& f) const noexcept {
                f.day_star = _s[i] == '*' || (end - i == 1 && _s[i] == '?');
                if (end - i == 1 && _s[i] == '?') {
                    f.days = ~uint32_t(1);
                    return nullopt;
                }
                uint64_t bits = 0;
                while (i <= end) {
                    size_t e = i;
                    while (e < end && _s[e] != ',') {
                        ++e;
                    }
                    if (e == i) {
                        return error("an empty item in a cron field", i);
                    }
                    char first = _lower(_s[i]);
                    char last = _lower(_s[e - 1]);
                    if (first == 'l') {   // L, L-n, LW
                        if (e - i == 1) {
                            f.before_last |= 1;
                        } else if (e - i == 2 && last == 'w') {
                            f.last_weekday = true;
                        } else if (_s[i + 1] == '-') {
                            size_t k = i + 2;
                            int n = 0;
                            if (auto err = _value(k, e, Plain, n)) {
                                return err;
                            }
                            if (k != e || n > 30) {
                                return error("L-n takes n from 0 to 30", i);
                            }
                            f.before_last |= 1u << n;
                        } else {
                            return error("an unexpected character in a cron field", i + 1);
                        }
                    } else if (last == 'w') {   // nW
                        size_t k = i;
                        int n = 0;
                        if (auto err = _value(k, e, Plain, n)) {
                            return err;
                        }
                        if (k != e - 1 || n < 1 || n > 31) {
                            return error("nW takes a day from 1 to 31", i);
                        }
                        f.nearest |= 1u << n;
                    } else if (auto err = _range(i, e, 1, 31, Plain, bits)) {
                        return err;
                    }
                    i = e + 1;
                }
                f.days = uint32_t(bits);
                return nullopt;
            }

            optional<error> _weekday_field(size_t i, size_t end, CronFields& f) const noexcept {
                f.weekday_star = _s[i] == '*' || (end - i == 1 && _s[i] == '?');
                if (end - i == 1 && _s[i] == '?') {
                    f.weekdays = 0x7F;
                    return nullopt;
                }
                uint64_t bits = 0;
                while (i <= end) {
                    size_t e = i;
                    while (e < end && _s[e] != ',') {
                        ++e;
                    }
                    if (e == i) {
                        return error("an empty item in a cron field", i);
                    }
                    size_t hash = i;
                    while (hash < e && _s[hash] != '#') {
                        ++hash;
                    }
                    if (hash < e) {   // n#k
                        size_t k = i;
                        int d = 0, nth = 0;
                        if (auto err = _value(k, hash, Weekdays, d)) {
                            return err;
                        }
                        size_t j = hash + 1;
                        if (auto err = _value(j, e, Plain, nth)) {
                            return err;
                        }
                        if (k != hash || j != e || d > 7 || nth < 1 || nth > 5) {
                            return error("n#k takes a day of the week and k from 1 to 5", i);
                        }
                        f.nth |= uint64_t(1) << ((d % 7) * 8 + nth);
                    } else if (e - i >= 2 && _lower(_s[e - 1]) == 'l') {   // nL
                        size_t k = i;
                        int d = 0;
                        if (auto err = _value(k, e - 1, Weekdays, d)) {
                            return err;
                        }
                        if (k != e - 1 || d > 7) {
                            return error("nL takes a day of the week", i);
                        }
                        f.last_of |= uint8_t(1u << (d % 7));
                    } else if (auto err = _range(i, e, 0, 7, Weekdays, bits)) {
                        return err;
                    }
                    i = e + 1;
                }
                if (bits >> 7 & 1) {   // 7 is Sunday
                    bits |= 1;
                }
                f.weekdays = uint8_t(bits & 0x7F);
                return nullopt;
            }

            const char* _s;
            size_t _n;
        };

        struct CronAccess {
            SGCL_INLINE_HOT static cron make(const CronFields& f, const string& text, const time::zone& z) noexcept {
                return cron(f, text, z);
            }

            // The fields read, for the tests and the fuzz harness
            SGCL_INLINE_HOT static const CronFields& fields(const cron& c) noexcept {
                return c._f;
            }
        };
    }

    inline expected<cron, error> cron::parse(const string& expression, const time::zone& z) noexcept {
        detail::CronFields f;
        if (auto e = detail::CronParser(expression).read(f)) {
            return unexpected(*e);
        }
        return detail::CronAccess::make(f, expression, z);
    }

    namespace detail {
        // The wait until an instant of the wall clock: the timers sleep
        // on the steady clock, so the wall clock is read again on waking,
        // a clock set back meaning another sleep. False: the stop came
        inline async::task<bool> cron_wait_until(datetime when, async::stop_token stop) {
            for (;;) {
                duration d = when - now();
                if (d <= duration::zero()) {
                    co_return true;
                }
                if (stop.stop_possible()) {
                    if (co_await async::select(async::timeout(d, [] {}), stop.on_stop([] {})) == 1) {
                        co_return false;
                    }
                } else {
                    co_await async::sleep(d);
                }
            }
        }

        // The loop of every: the next time, its wait, f; a time that passed
        // while f ran is held, one, and the others dropped (as a tick's)
        template<class F>
        async::task<> cron_loop(cron c, F f, async::stop_token stop) {
            datetime prev = now();
            bool catching_up = false;
            for (;;) {
                optional<datetime> t = c.next(prev);
                if (!t || stop.stop_requested()) {
                    co_return;
                }
                if (!co_await cron_wait_until(*t, stop) || stop.stop_requested()) {
                    co_return;
                }
                if constexpr (async::detail::TaskFactory<F>) {
                    co_await f();
                } else {
                    f();
                }
                datetime after = now();
                if (catching_up) {
                    prev = after;
                    catching_up = false;
                    continue;
                }
                optional<datetime> again = c.next(*t);
                catching_up = again && *again <= after;
                prev = *t;
            }
        }
    }

    // f called at every time of the cron until the source returned is
    // stopped: `auto s = time::every(time::cron("0 9 * * MON-FRI"), f);`.
    // async::every's shape: f a function called on a worker, or a
    // coroutine function whose task is awaited; a time that comes while
    // f still runs is held (one), the others dropped. The wall clock is
    // read through the core's clock, so a test's manual_clock moves it
    template<class F>
    requires std::invocable<F&>
    SGCL_INLINE_HOT async::stop_source every(const cron& c, F f) {
        async::stop_source s;
        async::go(detail::cron_loop(c, std::move(f), s.token()));
        return s;
    }

    // The same, stopped also when parent is
    template<class F>
    requires std::invocable<F&>
    SGCL_INLINE_HOT async::stop_source every(const cron& c, F f, const async::stop_token& parent) {
        async::stop_source s(parent);
        async::go(detail::cron_loop(c, std::move(f), s.token()));
        return s;
    }
}
