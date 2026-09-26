//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/expected.h"
#include "../date.h"
#include "../error.h"

#include <cstdint>
#include <string>
#include <string_view>

// The TZ string of POSIX (XBD 8.3), with the two extensions of RFC 9636
// 3.3.1 (the footer of a TZif file of version 3): the hours of a rule's
// time from -167 to 167, and daylight saving time all year round when
// it starts on January 1 at 0:00 and ends at 24:00 plus the difference on
// December 31 ("EST5EDT,0/0,J365/25"). Written from the two texts.
//
//     std offset [dst [offset] [,start[/time],end[/time]]]
//
// A name is three or more letters, or three or more letters, digits,
// '+' and '-' between '<' and '>'; an offset is [+|-]hh[:mm[:ss]] and is
// the one POSIX means, what is added to the local time to give UTC (west
// positive: "CET-1" is one hour east). A date is Jn (1 to 365, February
// 29 never counted), n (0 to 365, counted) or Mm.w.d (day d, Sunday 0, of
// week w of month m, week 5 the last). A time defaults to 02:00:00, a
// DST offset to one hour ahead of the standard one; a DST name with no
// rule takes the rule of the United States, as tzcode does
// (",M3.2.0,M11.1.0").
namespace sgcl::time::detail {
    struct posix_date {
        char kind = 'M';            // 'J': Jn, 'n': n, 'M': Mm.w.d
        int16_t day = 0;            // of Jn and n
        uint8_t month = 1;          // of Mm.w.d
        uint8_t week = 1;
        uint8_t weekday = 0;        // Sunday 0
    };

    struct posix_rule {
        std::string std_name;
        int32_t std_offset = 0;     // seconds east of UTC (the sign of POSIX turned round)
        bool has_dst = false;
        std::string dst_name;
        int32_t dst_offset = 0;
        posix_date start, end;
        int32_t start_time = 7200;  // seconds of the local time then in force, -167h to 167h
        int32_t end_time = 7200;
    };

    class posix_reader {
    public:
        explicit posix_reader(std::string_view text) noexcept
        : _s(text) {
        }

        expected<posix_rule, error> read() {
            posix_rule r;
            if (_s.empty()) {
                return _fail("a POSIX TZ string expected", 0);
            }
            if (!_name(r.std_name)) {
                return _fail("a name of three or more letters, or one in <>, expected", _i);
            }
            int32_t west;
            size_t at = _i;
            if (!_offset(west, 24)) {
                return _fail("an offset expected: [+|-]hh[:mm[:ss]], hours 0 to 24", at);
            }
            r.std_offset = -west;
            if (_i == _s.size()) {
                return r;
            }
            if (!_name(r.dst_name)) {
                return _fail("a name of daylight saving time expected", _i);
            }
            r.has_dst = true;
            r.dst_offset = r.std_offset + 3600;
            if (_i < _s.size() && _s[_i] != ',') {
                at = _i;
                if (!_offset(west, 24)) {
                    return _fail("an offset expected: [+|-]hh[:mm[:ss]], hours 0 to 24", at);
                }
                r.dst_offset = -west;
            }
            if (_i == _s.size()) {
                // tzcode's default: the rule of the United States since 2007
                r.start = {'M', 0, 3, 2, 0};
                r.end = {'M', 0, 11, 1, 0};
                return r;
            }
            if (_s[_i] != ',') {
                return _fail("a comma and the rule expected", _i);
            }
            ++_i;
            at = _i;
            if (!_date(r.start) || !_time(r.start_time)) {
                return _fail("the date the daylight saving time starts expected: Jn, n or Mm.w.d, then /time", at);
            }
            if (_i >= _s.size() || _s[_i] != ',') {
                return _fail("a comma and the date it ends expected", _i);
            }
            ++_i;
            at = _i;
            if (!_date(r.end) || !_time(r.end_time)) {
                return _fail("the date the daylight saving time ends expected: Jn, n or Mm.w.d, then /time", at);
            }
            if (_i != _s.size()) {
                return _fail("the end of the TZ string expected", _i);
            }
            return r;
        }

    private:
        static expected<posix_rule, error> _fail(const char* message, size_t at) {
            return unexpected(error(message, at));
        }

        static bool _alpha(char c) noexcept {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        }

        static bool _digit(char c) noexcept {
            return c >= '0' && c <= '9';
        }

        bool _name(std::string& out) {
            size_t from = _i;
            if (_i < _s.size() && _s[_i] == '<') {
                ++_i;
                size_t start = _i;
                while (_i < _s.size() && (_alpha(_s[_i]) || _digit(_s[_i]) || _s[_i] == '+' || _s[_i] == '-')) {
                    ++_i;
                }
                if (_i >= _s.size() || _s[_i] != '>' || _i - start < 3) {
                    _i = from;
                    return false;
                }
                out.assign(_s.substr(start, _i - start));
                ++_i;
                return true;
            }
            while (_i < _s.size() && _alpha(_s[_i])) {
                ++_i;
            }
            if (_i - from < 3) {
                _i = from;
                return false;
            }
            out.assign(_s.substr(from, _i - from));
            return true;
        }

        // A number of one or more digits, at most `max`
        bool _number(int& out, int max) {
            if (_i >= _s.size() || !_digit(_s[_i])) {
                return false;
            }
            int v = 0;
            while (_i < _s.size() && _digit(_s[_i])) {
                v = v * 10 + (_s[_i] - '0');
                if (v > max) {
                    return false;
                }
                ++_i;
            }
            out = v;
            return true;
        }

        // [+|-]hh[:mm[:ss]] in seconds, hours up to max_hours
        bool _offset(int32_t& out, int max_hours) {
            int sign = 1;
            if (_i < _s.size() && (_s[_i] == '+' || _s[_i] == '-')) {
                sign = _s[_i] == '-' ? -1 : 1;
                ++_i;
            }
            int h = 0, m = 0, sec = 0;
            if (!_number(h, max_hours)) {
                return false;
            }
            if (_i < _s.size() && _s[_i] == ':') {
                ++_i;
                if (!_number(m, 59)) {
                    return false;
                }
                if (_i < _s.size() && _s[_i] == ':') {
                    ++_i;
                    if (!_number(sec, 59)) {
                        return false;
                    }
                }
            }
            out = sign * (h * 3600 + m * 60 + sec);
            return true;
        }

        bool _date(posix_date& d) {
            int v = 0;
            if (_i < _s.size() && _s[_i] == 'J') {
                ++_i;
                if (!_number(v, 365) || v < 1) {
                    return false;
                }
                d.kind = 'J';
                d.day = int16_t(v);
                return true;
            }
            if (_i < _s.size() && _s[_i] == 'M') {
                ++_i;
                int m, w, wd;
                if (!_number(m, 12) || m < 1 || _i >= _s.size() || _s[_i] != '.') {
                    return false;
                }
                ++_i;
                if (!_number(w, 5) || w < 1 || _i >= _s.size() || _s[_i] != '.') {
                    return false;
                }
                ++_i;
                if (!_number(wd, 6)) {
                    return false;
                }
                d.kind = 'M';
                d.month = uint8_t(m);
                d.week = uint8_t(w);
                d.weekday = uint8_t(wd);
                return true;
            }
            if (!_number(v, 365)) {
                return false;
            }
            d.kind = 'n';
            d.day = int16_t(v);
            return true;
        }

        // /time, hours -167 to 167 (RFC 9636's extension); none: 02:00
        bool _time(int32_t& out) {
            if (_i < _s.size() && _s[_i] == '/') {
                ++_i;
                return _offset(out, 167);
            }
            return true;
        }

        std::string_view _s;
        size_t _i = 0;
    };

    inline expected<posix_rule, error> read_posix(std::string_view text) {
        return posix_reader(text).read();
    }

    // The day (days since 1970-01-01) a date of the rule falls on in a year
    inline int64_t posix_day(const posix_date& d, int64_t year) noexcept {
        int64_t jan1 = days_from_civil(year, 1);
        if (d.kind == 'J') {
            bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
            return jan1 + d.day - 1 + (leap && d.day >= 60 ? 1 : 0);
        }
        if (d.kind == 'n') {
            return jan1 + d.day;
        }
        int64_t first = days_from_civil(year, d.month);
        int64_t first_weekday = first + 4 - floor_div(first + 4, 7) * 7;   // 1970-01-01 was a Thursday; Sunday 0
        int64_t day = first + ((d.weekday - first_weekday + 7) % 7) + int64_t(d.week - 1) * 7;
        if (d.week == 5) {
            int64_t next = d.month == 12 ? days_from_civil(year + 1, 1) : days_from_civil(year, d.month + 1u);
            while (day >= next) {
                day -= 7;
            }
        }
        return day;
    }

    // The two changes of a year, as seconds since 1970 UTC: to daylight
    // saving time at the start (a time of the standard offset) and back
    // at the end (a time of the daylight one)
    struct posix_year {
        int64_t start;
        int64_t end;
    };

    // (of a posix_rule, or of anything with its fields of numbers)
    template<class Rule>
    posix_year posix_changes(const Rule& r, int64_t year) noexcept {
        return {posix_day(r.start, year) * 86400 + r.start_time - r.std_offset,
                posix_day(r.end, year) * 86400 + r.end_time - r.dst_offset};
    }

    // The year of the civil calendar a time falls in at an offset: the
    // days brought into the first 400 years from 1970 by whole cycles,
    // the calendar of <chrono> asked there
    inline int64_t year_of(int64_t seconds, int32_t offset) noexcept {
        int64_t days = floor_div(seconds + offset, 86400);
        int64_t cycles = floor_div(days, 146097);
        std::chrono::year_month_day ymd(std::chrono::sys_days(std::chrono::days(days - cycles * 146097)));
        return int64_t(static_cast<int>(ymd.year())) + cycles * 400;
    }
}
