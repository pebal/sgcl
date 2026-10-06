//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "../core/aliases.h"
#include "../core/duration.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../time/date.h"
#include "../time/datetime.h"
#include "../time/zone.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace sgcl::encoding {
    namespace detail {
        struct RecurrenceData;
        struct RecurrenceAccess;
    }

    // A recurrence rule of iCalendar (RFC 5545 §3.3.10, an RRULE's value):
    // FREQ=WEEKLY;INTERVAL=2;BYDAY=MO,WE;COUNT=10. Immutable, one word
    // shared by copying. Read by parse, written by to_string; occurrences
    // expands it from a start, in the start's zone.
    class recurrence {
    public:
        using error = encoding::error;

        // FREQ: how far one period of the rule spans
        enum class frequency : uint8_t {
            secondly,
            minutely,
            hourly,
            daily,
            weekly,
            monthly,
            yearly
        };

        // A day of BYDAY: the weekday, and which of them in the month or the
        // year (1 the first, -1 the last), 0 for every one
        struct weekday_rule {
            time::weekday day = time::weekday::monday;
            int ordinal = 0;

            friend bool operator==(const weekday_rule&, const weekday_rule&) noexcept = default;
        };

        // FREQ=DAILY, nothing else
        recurrence() noexcept;

        // The rule of the text; bad_expected_access<encoding::error> for one
        // that is not one (a rule written in the program)
        explicit recurrence(const string& text)
        : recurrence(parse(text).value()) {
        }

        // A rule's text: FREQ first or not, its parts in any order, each
        // once; the parts RFC 5545 forbids for a frequency (BYWEEKNO but in
        // YEARLY, BYYEARDAY in DAILY, WEEKLY and MONTHLY, BYMONTHDAY in
        // WEEKLY, a numbered BYDAY but in MONTHLY and YEARLY or with
        // BYWEEKNO) refused, COUNT and UNTIL together refused
        static expected<recurrence, error> parse(const string& text) noexcept;

        // The parts in RFC 5545's order: FREQ, UNTIL or COUNT, INTERVAL when
        // not 1, the BYs, WKST when not MO
        string to_string() const;

        frequency freq() const noexcept;
        int interval() const noexcept;
        optional<int64_t> count() const noexcept;
        optional<string> until() const noexcept;     // as written: a DATE or a DATE-TIME
        slice<const int> by_second() const noexcept;
        slice<const int> by_minute() const noexcept;
        slice<const int> by_hour() const noexcept;
        slice<const weekday_rule> by_day() const noexcept;
        slice<const int> by_month_day() const noexcept;
        slice<const int> by_year_day() const noexcept;
        slice<const int> by_week_no() const noexcept;
        slice<const int> by_month() const noexcept;
        slice<const int> by_set_pos() const noexcept;
        time::weekday week_start() const noexcept;

        // The instances from start on, in start's zone: start itself first
        // (the first instance, as DTSTART is, matching the rule or not),
        // then the times the rule makes after it; those at or after from
        // and before to, at most limit (100 000 without it). COUNT counts
        // from start, whatever from is; UNTIL in UTC (a Z) is an instant, a
        // local one the start zone's clock. A time the zone skips moves on
        // by the skip, one it shows twice is the first (RFC 5545 §3.3.5)
        vector<time::datetime> occurrences(const time::datetime& start, const time::datetime& from, const time::datetime& to) const;
        vector<time::datetime> occurrences(const time::datetime& start, const time::datetime& from, const time::datetime& to, size_t limit) const;

        // The same parts with the same values (UNTIL by its text)
        friend bool operator==(const recurrence& a, const recurrence& b) noexcept;

    private:
        friend struct detail::RecurrenceAccess;

        tracked_ptr<const detail::RecurrenceData> _data;

        const detail::RecurrenceData& _d() const noexcept;
    };

    namespace detail {
        struct RecurrenceData {
            recurrence::frequency freq = recurrence::frequency::daily;
            int interval = 1;
            int64_t count = -1;
            string until;
            vector<int> by_second, by_minute, by_hour, by_month_day, by_year_day, by_week_no, by_month, by_set_pos;
            vector<recurrence::weekday_rule> by_day;
            time::weekday wkst = time::weekday::monday;
        };

        // Days since 1970-01-01 and the civil fields of one
        SGCL_INLINE_HOT int64_t rec_days(int y, int m, int d) noexcept {
            return std::chrono::sys_days(std::chrono::year_month_day(std::chrono::year(y), std::chrono::month(unsigned(m)), std::chrono::day(unsigned(d))))
                .time_since_epoch()
                .count();
        }

        struct RecCivil {
            int y, m, d;
        };

        SGCL_INLINE_HOT RecCivil rec_civil(int64_t days) noexcept {
            std::chrono::year_month_day ymd{std::chrono::sys_days(std::chrono::days(days))};
            return {int(ymd.year()), int(unsigned(ymd.month())), int(unsigned(ymd.day()))};
        }

        SGCL_INLINE_HOT bool rec_leap(int y) noexcept {
            return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        }

        SGCL_INLINE_HOT int rec_month_days(int y, int m) noexcept {
            static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            return days[m - 1] + (m == 2 && rec_leap(y));
        }

        // ISO weekday of a day number: 1 Monday .. 7 Sunday
        SGCL_INLINE_HOT int rec_weekday(int64_t days) noexcept {
            int64_t w = (days + 3) % 7;   // 1970-01-01 was a Thursday
            if (w < 0) {
                w += 7;
            }
            return int(w) + 1;
        }

        SGCL_INLINE_HOT int64_t rec_floor_div(int64_t a, int64_t b) noexcept {
            int64_t q = a / b;
            return q - ((a % b != 0) && ((a < 0) != (b < 0)));
        }

        // The first day of week 1 of a year: the week (starting on wkst)
        // with at least four days of the year (RFC 5545's BYWEEKNO)
        inline int64_t rec_week1(int y, int wkst) noexcept {
            int64_t jan4 = rec_days(y, 1, 4);
            return jan4 - ((rec_weekday(jan4) - wkst + 7) % 7);
        }

        // A wall clock's instant: the clock's seconds since 1970 to Unix seconds
        using RecResolve = std::function<int64_t(int64_t)>;

        class RecurrenceEngine {
        public:
            RecurrenceEngine(const RecurrenceData& r, int64_t start_wall, bool date_only) noexcept
            : _r(r), _start(start_wall), _date_only(date_only) {
                RecCivil c = rec_civil(rec_floor_div(start_wall, 86400));
                int64_t sod = start_wall - rec_floor_div(start_wall, 86400) * 86400;
                _sy = c.y;
                _sm = c.m;
                _sd = c.d;
                _sh = int(sod / 3600);
                _smi = int(sod / 60 % 60);
                _ss = int(sod % 60);
                _wkst = int(r.wkst);
                // the defaults of the day (RFC 5545 §3.3.10: what DTSTART gives
                // when no BY part says)
                bool day_parts = !r.by_week_no.empty() || !r.by_year_day.empty() || !r.by_month_day.empty() || !r.by_day.empty();
                if (!day_parts) {
                    if (r.freq == recurrence::frequency::yearly) {
                        if (r.by_month.empty()) {
                            _months.push_back(_sm);
                        }
                        _month_days.push_back(_sd);
                    } else if (r.freq == recurrence::frequency::monthly) {
                        _month_days.push_back(_sd);
                    } else if (r.freq == recurrence::frequency::weekly) {
                        _weekdays.push_back({rec_weekday(rec_days(_sy, _sm, _sd)), 0});
                    }
                }
                for (int m : r.by_month) {
                    _months.push_back(m);
                }
                for (int d : r.by_month_day) {
                    _month_days.push_back(d);
                }
                for (auto& w : r.by_day) {
                    _weekdays.push_back({int(w.day), w.ordinal});
                }
                _hours = r.by_hour.empty() ? std::vector<int>{_sh} : std::vector<int>(r.by_hour.begin(), r.by_hour.end());
                _minutes = r.by_minute.empty() ? std::vector<int>{_smi} : std::vector<int>(r.by_minute.begin(), r.by_minute.end());
                _seconds = r.by_second.empty() ? std::vector<int>{_ss} : std::vector<int>(r.by_second.begin(), r.by_second.end());
                std::sort(_hours.begin(), _hours.end());
                std::sort(_minutes.begin(), _minutes.end());
                std::sort(_seconds.begin(), _seconds.end());
                _hours.erase(std::unique(_hours.begin(), _hours.end()), _hours.end());
                _minutes.erase(std::unique(_minutes.begin(), _minutes.end()), _minutes.end());
                _seconds.erase(std::unique(_seconds.begin(), _seconds.end()), _seconds.end());
            }

            // The rule's wall times after start (not start itself), in order:
            // f(wall) false stops. Periods with no candidate in a row past
            // 10^7 stop it too (a rule that makes nothing, FEB 30)
            template<class F>
            void run(int64_t skip_before_wall, int64_t stop_after_wall, F&& f) {
                using fr = recurrence::frequency;
                int64_t interval = _r.interval;
                int64_t start_day = rec_floor_div(_start, 86400);
                // the period index of skip_before_wall: periods ending before it passed over
                int64_t k = 0;
                if (skip_before_wall > _start) {
                    int64_t target = 0, base = 0;
                    switch (_r.freq) {
                        case fr::yearly: base = _sy; target = rec_civil(rec_floor_div(skip_before_wall, 86400)).y; break;
                        case fr::monthly: {
                            RecCivil t = rec_civil(rec_floor_div(skip_before_wall, 86400));
                            base = int64_t(_sy) * 12 + _sm - 1;
                            target = int64_t(t.y) * 12 + t.m - 1;
                            break;
                        }
                        case fr::weekly: base = _week_start(start_day); target = rec_floor_div(skip_before_wall, 86400); break;
                        case fr::daily: base = start_day; target = rec_floor_div(skip_before_wall, 86400); break;
                        default: base = _period_floor(_start); target = skip_before_wall; break;
                    }
                    int64_t unit = _r.freq == fr::weekly ? 7 : _r.freq == fr::hourly ? 3600 : _r.freq == fr::minutely ? 60 : 1;
                    k = std::max<int64_t>(0, rec_floor_div(target - base, unit * interval) - 1);
                }
                int64_t empty = 0;
                std::vector<int64_t> cand;
                for (;; ++k) {
                    cand.clear();
                    bool past = false;
                    switch (_r.freq) {
                        case fr::yearly: {
                            int64_t y = _sy + k * interval;
                            if (y > 9999 || (y - 1 > -32767 && rec_days(int(y - 1), 12, 20) * 86400 > stop_after_wall)) {
                                return;
                            }
                            _year(int(y), cand);
                            break;
                        }
                        case fr::monthly: {
                            int64_t mi = int64_t(_sy) * 12 + (_sm - 1) + k * interval;
                            int64_t y = mi / 12;
                            if (y > 9999 || rec_days(int(y), int(mi % 12) + 1, 1) * 86400 > stop_after_wall) {
                                return;
                            }
                            _month(int(y), int(mi % 12) + 1, cand);
                            break;
                        }
                        case fr::weekly: {
                            int64_t w = _week_start(start_day) + k * 7 * interval;
                            if (w > rec_days(9999, 12, 31) || w * 86400 > stop_after_wall) {
                                return;
                            }
                            for (int64_t d = w; d < w + 7; ++d) {
                                if (_day_ok(d)) {
                                    _times(d, cand);
                                }
                            }
                            break;
                        }
                        case fr::daily: {
                            int64_t d = start_day + k * interval;
                            if (d > rec_days(9999, 12, 31) || d * 86400 > stop_after_wall) {
                                return;
                            }
                            if (_day_ok(d)) {
                                _times(d, cand);
                            }
                            break;
                        }
                        default: {
                            int64_t unit = _r.freq == fr::hourly ? 3600 : _r.freq == fr::minutely ? 60 : 1;
                            int64_t p = _period_floor(_start) + k * unit * interval;
                            int64_t d = rec_floor_div(p, 86400);
                            if (d > rec_days(9999, 12, 31) || p > stop_after_wall) {
                                return;
                            }
                            if (!_day_ok(d)) {
                                // to the first period of the next day
                                int64_t next = (d + 1) * 86400;
                                int64_t base = _period_floor(_start);
                                int64_t nk = rec_floor_div(next - base + unit * interval - 1, unit * interval);
                                k = std::max(k, nk - 1);
                                past = true;
                                break;
                            }
                            _sub_daily(p, cand);
                            break;
                        }
                    }
                    if (!past) {
                        _set_pos(cand);
                    }
                    bool any = false;
                    for (int64_t c : cand) {
                        if (c <= _start) {
                            continue;
                        }
                        any = true;
                        if (!f(c)) {
                            return;
                        }
                    }
                    if (any) {
                        empty = 0;
                    } else if (++empty > 10000000) {
                        return;
                    }
                }
            }

        private:
            struct Weekday {
                int day;
                int ordinal;
            };

            const RecurrenceData& _r;
            int64_t _start;
            bool _date_only;
            int _sy, _sm, _sd, _sh, _smi, _ss, _wkst;
            std::vector<int> _months, _month_days, _hours, _minutes, _seconds;
            std::vector<Weekday> _weekdays;

            int64_t _week_start(int64_t day) const noexcept {
                return day - ((rec_weekday(day) - _wkst + 7) % 7);
            }

            int64_t _period_floor(int64_t wall) const noexcept {
                using fr = recurrence::frequency;
                int64_t unit = _r.freq == fr::hourly ? 3600 : _r.freq == fr::minutely ? 60 : 1;
                return rec_floor_div(wall, unit) * unit;
            }

            static bool _has(const std::vector<int>& v, int x) noexcept {
                return std::find(v.begin(), v.end(), x) != v.end();
            }

            static bool _has(const vector<int>& v, int x) noexcept {
                return std::find(v.begin(), v.end(), x) != v.end();
            }

            // Whether the day passes every BY part of a day (with the
            // defaults); the numbered BYDAY counted in the month (MONTHLY,
            // YEARLY with BYMONTH) or the year
            bool _day_ok(int64_t day) const noexcept {
                RecCivil c = rec_civil(day);
                if (!_months.empty() && !_has(_months, c.m)) {
                    return false;
                }
                if (!_month_days.empty()) {
                    int md = rec_month_days(c.y, c.m);
                    bool ok = false;
                    for (int x : _month_days) {
                        ok = ok || x == c.d || x == c.d - md - 1;
                    }
                    if (!ok) {
                        return false;
                    }
                }
                if (!_r.by_year_day.empty()) {
                    int yd = int(day - rec_days(c.y, 1, 1)) + 1;
                    int yl = rec_leap(c.y) ? 366 : 365;
                    bool ok = false;
                    for (int x : _r.by_year_day) {
                        ok = ok || x == yd || x == yd - yl - 1;
                    }
                    if (!ok) {
                        return false;
                    }
                }
                if (!_r.by_week_no.empty()) {
                    int wy = c.y;
                    int64_t w1 = rec_week1(wy, _wkst);
                    if (day < w1) {
                        w1 = rec_week1(--wy, _wkst);
                    } else if (day >= rec_week1(wy + 1, _wkst)) {
                        w1 = rec_week1(++wy, _wkst);
                    }
                    int wn = int((day - w1) / 7) + 1;
                    int weeks = int((rec_week1(wy + 1, _wkst) - w1) / 7);
                    // the period of a YEARLY rule is the week-year
                    bool ok = false;
                    for (int x : _r.by_week_no) {
                        ok = ok || x == wn || x == wn - weeks - 1;
                    }
                    if (!ok) {
                        return false;
                    }
                }
                if (!_weekdays.empty()) {
                    int wd = rec_weekday(day);
                    bool in_month = _r.freq == recurrence::frequency::monthly || (_r.freq == recurrence::frequency::yearly && !_r.by_month.empty());
                    bool ok = false;
                    for (const Weekday& w : _weekdays) {
                        if (w.day != wd) {
                            continue;
                        }
                        if (w.ordinal == 0) {
                            ok = true;
                            break;
                        }
                        int n, last;
                        if (in_month) {
                            n = (c.d - 1) / 7 + 1;
                            last = (rec_month_days(c.y, c.m) - c.d) / 7 + 1;
                        } else {
                            int yd = int(day - rec_days(c.y, 1, 1)) + 1;
                            int yl = rec_leap(c.y) ? 366 : 365;
                            n = (yd - 1) / 7 + 1;
                            last = (yl - yd) / 7 + 1;
                        }
                        if (w.ordinal == n || w.ordinal == -last) {
                            ok = true;
                            break;
                        }
                    }
                    if (!ok) {
                        return false;
                    }
                }
                return true;
            }

            void _times(int64_t day, std::vector<int64_t>& out) const {
                if (_date_only) {
                    out.push_back(day * 86400);
                    return;
                }
                for (int h : _hours) {
                    for (int m : _minutes) {
                        for (int s : _seconds) {
                            if (s < 60) {
                                out.push_back(day * 86400 + h * 3600 + m * 60 + s);
                            }
                        }
                    }
                }
            }

            void _year(int y, std::vector<int64_t>& out) const {
                if (!_months.empty() && _r.by_week_no.empty() && _r.by_year_day.empty()) {
                    // only the months the rule names: their days in order
                    std::vector<int> months(_months.begin(), _months.end());
                    std::sort(months.begin(), months.end());
                    months.erase(std::unique(months.begin(), months.end()), months.end());
                    for (int m : months) {
                        _month(y, m, out);
                    }
                    return;
                }
                int64_t first = rec_days(y, 1, 1), last = rec_days(y, 12, 31);
                if (!_r.by_week_no.empty()) {
                    // the week-year y: from its week 1 to the day before the next's
                    first = rec_week1(y, _wkst);
                    last = rec_week1(y + 1, _wkst) - 1;
                }
                for (int64_t d = first; d <= last; ++d) {
                    if (_day_ok(d)) {
                        _times(d, out);
                    }
                }
            }

            void _month(int y, int m, std::vector<int64_t>& out) const {
                int64_t first = rec_days(y, m, 1);
                int n = rec_month_days(y, m);
                for (int64_t d = first; d < first + n; ++d) {
                    if (_day_ok(d)) {
                        _times(d, out);
                    }
                }
            }

            void _sub_daily(int64_t p, std::vector<int64_t>& out) const {
                using fr = recurrence::frequency;
                int64_t day = rec_floor_div(p, 86400);
                int64_t sod = p - day * 86400;
                int h = int(sod / 3600), mi = int(sod / 60 % 60), s = int(sod % 60);
                if (!_r.by_hour.empty() && !_has(_r.by_hour, h)) {
                    return;
                }
                if (_r.freq == fr::hourly) {
                    for (int m : _minutes) {
                        for (int x : _seconds) {
                            if (x < 60) {
                                out.push_back(day * 86400 + h * 3600 + m * 60 + x);
                            }
                        }
                    }
                    return;
                }
                if (!_r.by_minute.empty() && !_has(_r.by_minute, mi)) {
                    return;
                }
                if (_r.freq == fr::minutely) {
                    for (int x : _seconds) {
                        if (x < 60) {
                            out.push_back(day * 86400 + h * 3600 + mi * 60 + x);
                        }
                    }
                    return;
                }
                if (!_r.by_second.empty() && !_has(_r.by_second, s)) {
                    return;
                }
                out.push_back(p);
            }

            void _set_pos(std::vector<int64_t>& cand) const {
                if (_r.by_set_pos.empty() || cand.empty()) {
                    return;
                }
                std::vector<int64_t> kept;
                int64_t n = int64_t(cand.size());
                for (int pos : _r.by_set_pos) {
                    int64_t i = pos > 0 ? pos - 1 : n + pos;
                    if (i >= 0 && i < n) {
                        kept.push_back(cand[size_t(i)]);
                    }
                }
                std::sort(kept.begin(), kept.end());
                kept.erase(std::unique(kept.begin(), kept.end()), kept.end());
                cand.swap(kept);
            }
        };

        // A DATE or DATE-TIME of RFC 5545: the clock's seconds since 1970,
        // whether a Z ended it, whether it is a DATE
        struct RecMoment {
            int64_t wall = 0;
            bool utc = false;
            bool date = false;
        };

        inline bool rec_moment(std::string_view s, RecMoment& m) noexcept {
            auto digits = [&](size_t at, size_t n) {
                if (at + n > s.size()) {
                    return -1;
                }
                int v = 0;
                for (size_t i = 0; i < n; ++i) {
                    char c = s[at + i];
                    if (c < '0' || c > '9') {
                        return -1;
                    }
                    v = v * 10 + (c - '0');
                }
                return v;
            };
            int y = digits(0, 4), mo = digits(4, 2), d = digits(6, 2);
            if (y < 0 || mo < 1 || mo > 12 || d < 1 || d > rec_month_days(y, mo)) {
                return false;
            }
            int64_t days = rec_days(y, mo, d);
            if (s.size() == 8) {
                m = {days * 86400, false, true};
                return true;
            }
            if (s.size() < 15 || s[8] != 'T') {
                return false;
            }
            int h = digits(9, 2), mi = digits(11, 2), sec = digits(13, 2);
            if (h < 0 || h > 23 || mi < 0 || mi > 59 || sec < 0 || sec > 60) {
                return false;
            }
            bool z = s.size() == 16 && s[15] == 'Z';
            if (s.size() != 15 && !z) {
                return false;
            }
            m = {days * 86400 + h * 3600 + mi * 60 + std::min(sec, 59), z, false};
            return true;
        }

        // The rule over a start's wall clock: the walls of the instances,
        // start first, from <= instant < to, at most limit
        inline std::vector<int64_t> rec_expand(const RecurrenceData& r, int64_t start_wall, bool date_only, const RecResolve& resolve, int64_t from,
                                               int64_t to, size_t limit) {
            std::vector<int64_t> out;
            if (limit == 0) {
                return out;
            }
            RecMoment until;
            bool has_until = !r.until.empty() && rec_moment(r.until.view(), until);
            int64_t n = 0;
            auto take = [&](int64_t wall) {
                if (has_until) {
                    if (until.utc && !date_only) {
                        if (resolve(wall) > until.wall) {
                            return false;
                        }
                    } else if (until.date) {
                        if (rec_floor_div(wall, 86400) > rec_floor_div(until.wall, 86400)) {
                            return false;
                        }
                    } else if (wall > until.wall) {
                        return false;
                    }
                }
                ++n;
                if (r.count >= 0 && n > r.count) {
                    return false;
                }
                int64_t at = resolve(wall);
                if (at >= to) {
                    return false;
                }
                if (at >= from) {
                    out.push_back(wall);
                    if (out.size() >= limit) {
                        return false;
                    }
                }
                return true;
            };
            if (!take(start_wall)) {
                return out;
            }
            RecurrenceEngine e(r, start_wall, date_only);
            // with no COUNT, the periods before from need not be made: the
            // wall a day and more before the instant (no offset is a day)
            int64_t skip = r.count < 0 && from > INT64_MIN / 2 ? from - 2 * 86400 : INT64_MIN;
            // the periods past to make nothing in it: the wall a day and more after the instant
            e.run(skip, to < INT64_MAX / 2 ? to + 2 * 86400 : INT64_MAX, take);
            return out;
        }

        struct RecurrenceAccess {
            static const RecurrenceData& data(const recurrence& r) noexcept {
                return r._d();
            }
        };
    }

    inline recurrence::recurrence() noexcept
    : _data(make_tracked<detail::RecurrenceData>()) {
    }

    inline const detail::RecurrenceData& recurrence::_d() const noexcept {
        return *_data;
    }

    inline recurrence::frequency recurrence::freq() const noexcept {
        return _d().freq;
    }

    inline int recurrence::interval() const noexcept {
        return _d().interval;
    }

    inline optional<int64_t> recurrence::count() const noexcept {
        return _d().count < 0 ? nullopt : optional<int64_t>(_d().count);
    }

    inline optional<string> recurrence::until() const noexcept {
        return _d().until.empty() ? nullopt : optional<string>(_d().until);
    }

    inline slice<const int> recurrence::by_second() const noexcept {
        return _d().by_second.as_slice();
    }

    inline slice<const int> recurrence::by_minute() const noexcept {
        return _d().by_minute.as_slice();
    }

    inline slice<const int> recurrence::by_hour() const noexcept {
        return _d().by_hour.as_slice();
    }

    inline slice<const recurrence::weekday_rule> recurrence::by_day() const noexcept {
        return _d().by_day.as_slice();
    }

    inline slice<const int> recurrence::by_month_day() const noexcept {
        return _d().by_month_day.as_slice();
    }

    inline slice<const int> recurrence::by_year_day() const noexcept {
        return _d().by_year_day.as_slice();
    }

    inline slice<const int> recurrence::by_week_no() const noexcept {
        return _d().by_week_no.as_slice();
    }

    inline slice<const int> recurrence::by_month() const noexcept {
        return _d().by_month.as_slice();
    }

    inline slice<const int> recurrence::by_set_pos() const noexcept {
        return _d().by_set_pos.as_slice();
    }

    inline time::weekday recurrence::week_start() const noexcept {
        return _d().wkst;
    }

    inline bool operator==(const recurrence& a, const recurrence& b) noexcept {
        const auto& x = a._d();
        const auto& y = b._d();
        auto same = [](const vector<int>& p, const vector<int>& q) { return p.size() == q.size() && std::equal(p.begin(), p.end(), q.begin()); };
        return x.freq == y.freq && x.interval == y.interval && x.count == y.count && x.until == y.until && same(x.by_second, y.by_second) &&
               same(x.by_minute, y.by_minute) && same(x.by_hour, y.by_hour) && same(x.by_month_day, y.by_month_day) &&
               same(x.by_year_day, y.by_year_day) && same(x.by_week_no, y.by_week_no) && same(x.by_month, y.by_month) &&
               same(x.by_set_pos, y.by_set_pos) && x.by_day.size() == y.by_day.size() && std::equal(x.by_day.begin(), x.by_day.end(), y.by_day.begin()) &&
               x.wkst == y.wkst;
    }

    namespace detail {
        inline const char* const RecWeekdays[] = {"MO", "TU", "WE", "TH", "FR", "SA", "SU"};
        inline const char* const RecFreqs[] = {"SECONDLY", "MINUTELY", "HOURLY", "DAILY", "WEEKLY", "MONTHLY", "YEARLY"};

        inline int rec_weekday_of(std::string_view s) noexcept {
            for (int i = 0; i < 7; ++i) {
                if (s == RecWeekdays[i]) {
                    return i + 1;
                }
            }
            return 0;
        }

        // A signed integer of 1 to digits digits; the sign only where allowed
        inline bool rec_int(std::string_view s, bool sign, int& out) noexcept {
            size_t i = 0;
            bool neg = false;
            if (sign && !s.empty() && (s[0] == '+' || s[0] == '-')) {
                neg = s[0] == '-';
                i = 1;
            }
            if (i == s.size() || s.size() - i > 9) {
                return false;
            }
            int v = 0;
            for (; i < s.size(); ++i) {
                if (s[i] < '0' || s[i] > '9') {
                    return false;
                }
                v = v * 10 + (s[i] - '0');
            }
            out = neg ? -v : v;
            return true;
        }
    }

    inline expected<recurrence, recurrence::error> recurrence::parse(const string& text) noexcept {
        using detail::rec_int;
        auto fail = [&](size_t at, const std::string& why) {
            error e(errc::syntax, at, string(why));
            return expected<recurrence, error>(unexpected<error>(std::move(e.locate(text))));
        };
        auto d = make_tracked<detail::RecurrenceData>();
        std::string_view s = text.view();
        bool seen[14] = {};
        bool freq_seen = false;
        size_t at = 0;
        while (at <= s.size()) {
            size_t end = s.find(';', at);
            if (end == std::string_view::npos) {
                end = s.size();
            }
            std::string_view part = s.substr(at, end - at);
            size_t eq = part.find('=');
            if (eq == std::string_view::npos || eq == 0) {
                return fail(at, "a rule part that is no NAME=value");
            }
            std::string key(part.substr(0, eq));
            for (char& c : key) {
                c = c >= 'a' && c <= 'z' ? char(c - 32) : c;
            }
            std::string_view value = part.substr(eq + 1);
            std::string up(value);
            for (char& c : up) {
                c = c >= 'a' && c <= 'z' ? char(c - 32) : c;
            }
            static const char* const names[] = {"FREQ", "UNTIL", "COUNT", "INTERVAL", "BYSECOND", "BYMINUTE", "BYHOUR", "BYDAY",
                                                "BYMONTHDAY", "BYYEARDAY", "BYWEEKNO", "BYMONTH", "BYSETPOS", "WKST"};
            int which = -1;
            for (int i = 0; i < 14; ++i) {
                if (key == names[i]) {
                    which = i;
                }
            }
            size_t vat = at + eq + 1;
            if (which < 0) {
                if (key.size() > 2 && key[0] == 'X' && key[1] == '-') {
                    at = end + 1;   // an extension: passed over
                    continue;
                }
                return fail(at, "an unknown rule part " + key);
            }
            if (seen[which]) {
                return fail(at, "the rule part " + key + " given twice");
            }
            seen[which] = true;
            auto list = [&](vector<int>& out, int lo, int hi, bool sign) -> bool {
                size_t p = 0;
                while (p <= value.size()) {
                    size_t c = value.find(',', p);
                    if (c == std::string_view::npos) {
                        c = value.size();
                    }
                    int v;
                    if (!rec_int(value.substr(p, c - p), sign, v) || (v < 0 ? -v : v) < lo || (v < 0 ? -v : v) > hi || (!sign && v < 0)) {
                        return false;
                    }
                    out.push_back(v);
                    p = c + 1;
                }
                return true;
            };
            switch (which) {
                case 0: {
                    int f = -1;
                    for (int i = 0; i < 7; ++i) {
                        if (up == detail::RecFreqs[i]) {
                            f = i;
                        }
                    }
                    if (f < 0) {
                        return fail(vat, "an unknown FREQ");
                    }
                    d->freq = frequency(f);
                    freq_seen = true;
                    break;
                }
                case 1: {
                    detail::RecMoment m;
                    if (!detail::rec_moment(up, m)) {
                        return fail(vat, "an UNTIL that is no DATE or DATE-TIME");
                    }
                    d->until = string(up);
                    break;
                }
                case 2: {
                    int v;
                    if (!rec_int(value, false, v) || v < 0) {
                        return fail(vat, "a COUNT that is no number");
                    }
                    d->count = v;
                    break;
                }
                case 3: {
                    int v;
                    if (!rec_int(value, false, v) || v < 1) {
                        return fail(vat, "an INTERVAL that is no positive number");
                    }
                    d->interval = v;
                    break;
                }
                case 4:
                    if (!list(d->by_second, 0, 60, false)) {
                        return fail(vat, "a BYSECOND past 0 to 60");
                    }
                    break;
                case 5:
                    if (!list(d->by_minute, 0, 59, false)) {
                        return fail(vat, "a BYMINUTE past 0 to 59");
                    }
                    break;
                case 6:
                    if (!list(d->by_hour, 0, 23, false)) {
                        return fail(vat, "a BYHOUR past 0 to 23");
                    }
                    break;
                case 7: {
                    size_t p = 0;
                    std::string_view u(up);
                    while (p <= u.size()) {
                        size_t c = u.find(',', p);
                        if (c == std::string_view::npos) {
                            c = u.size();
                        }
                        std::string_view item = u.substr(p, c - p);
                        if (item.size() < 2) {
                            return fail(vat + p, "a BYDAY that is no weekday");
                        }
                        int wd = detail::rec_weekday_of(item.substr(item.size() - 2));
                        int ord = 0;
                        if (!wd || (item.size() > 2 && (!rec_int(item.substr(0, item.size() - 2), true, ord) || ord == 0 || ord > 53 || ord < -53))) {
                            return fail(vat + p, "a BYDAY that is no [+-n]weekday");
                        }
                        d->by_day.push_back(weekday_rule{time::weekday(wd), ord});
                        p = c + 1;
                    }
                    break;
                }
                case 8:
                    if (!list(d->by_month_day, 1, 31, true)) {
                        return fail(vat, "a BYMONTHDAY past -31 to 31 or 0");
                    }
                    break;
                case 9:
                    if (!list(d->by_year_day, 1, 366, true)) {
                        return fail(vat, "a BYYEARDAY past -366 to 366 or 0");
                    }
                    break;
                case 10:
                    if (!list(d->by_week_no, 1, 53, true)) {
                        return fail(vat, "a BYWEEKNO past -53 to 53 or 0");
                    }
                    break;
                case 11:
                    if (!list(d->by_month, 1, 12, false)) {
                        return fail(vat, "a BYMONTH past 1 to 12");
                    }
                    break;
                case 12:
                    if (!list(d->by_set_pos, 1, 366, true)) {
                        return fail(vat, "a BYSETPOS past -366 to 366 or 0");
                    }
                    break;
                default: {
                    int wd = detail::rec_weekday_of(up);
                    if (!wd) {
                        return fail(vat, "a WKST that is no weekday");
                    }
                    d->wkst = time::weekday(wd);
                    break;
                }
            }
            at = end + 1;
        }
        if (!freq_seen) {
            return fail(0, "a rule without FREQ");
        }
        if (seen[1] && seen[2]) {
            return fail(0, "a rule of both UNTIL and COUNT");
        }
        using fr = frequency;
        if (!d->by_week_no.empty() && d->freq != fr::yearly) {
            return fail(0, "BYWEEKNO in a rule but YEARLY");
        }
        if (!d->by_year_day.empty() && (d->freq == fr::daily || d->freq == fr::weekly || d->freq == fr::monthly)) {
            return fail(0, "BYYEARDAY in a DAILY, WEEKLY or MONTHLY rule");
        }
        if (!d->by_month_day.empty() && d->freq == fr::weekly) {
            return fail(0, "BYMONTHDAY in a WEEKLY rule");
        }
        for (auto& w : d->by_day) {
            if (w.ordinal != 0 && ((d->freq != fr::monthly && d->freq != fr::yearly) || !d->by_week_no.empty())) {
                return fail(0, "a numbered BYDAY in a rule but MONTHLY and YEARLY, or with BYWEEKNO");
            }
        }
        if (!d->by_set_pos.empty() && !(seen[4] || seen[5] || seen[6] || seen[7] || seen[8] || seen[9] || seen[10] || seen[11])) {
            return fail(0, "BYSETPOS without another BY part");
        }
        recurrence r;
        r._data = std::move(d);
        return r;
    }

    inline string recurrence::to_string() const {
        const auto& d = _d();
        std::string out = "FREQ=";
        out += detail::RecFreqs[int(d.freq)];
        if (!d.until.empty()) {
            out += ";UNTIL=";
            out.append(d.until.view());
        }
        if (d.count >= 0) {
            out += ";COUNT=" + std::to_string(d.count);
        }
        if (d.interval != 1) {
            out += ";INTERVAL=" + std::to_string(d.interval);
        }
        auto list = [&](const char* name, const vector<int>& v) {
            if (v.empty()) {
                return;
            }
            out += ';';
            out += name;
            out += '=';
            for (size_t i = 0; i < v.size(); ++i) {
                out += (i ? "," : "") + std::to_string(v[i]);
            }
        };
        list("BYSECOND", d.by_second);
        list("BYMINUTE", d.by_minute);
        list("BYHOUR", d.by_hour);
        if (!d.by_day.empty()) {
            out += ";BYDAY=";
            for (size_t i = 0; i < d.by_day.size(); ++i) {
                if (i) {
                    out += ',';
                }
                if (d.by_day[i].ordinal) {
                    out += std::to_string(d.by_day[i].ordinal);
                }
                out += detail::RecWeekdays[int(d.by_day[i].day) - 1];
            }
        }
        list("BYMONTHDAY", d.by_month_day);
        list("BYYEARDAY", d.by_year_day);
        list("BYWEEKNO", d.by_week_no);
        list("BYMONTH", d.by_month);
        list("BYSETPOS", d.by_set_pos);
        if (d.wkst != time::weekday::monday) {
            out += ";WKST=";
            out += detail::RecWeekdays[int(d.wkst) - 1];
        }
        return string(out);
    }

    inline vector<time::datetime> recurrence::occurrences(const time::datetime& start, const time::datetime& from, const time::datetime& to) const {
        return occurrences(start, from, to, 100000);
    }

    inline vector<time::datetime> recurrence::occurrences(const time::datetime& start, const time::datetime& from, const time::datetime& to, size_t limit) const {
        time::zone z = start.zone();
        int64_t wall = start.unix() + start.offset().nanoseconds() / 1000000000;
        int64_t frac = start.nanosecond();
        int64_t start_unix = start.unix();
        detail::RecResolve resolve = [&z, wall, start_unix](int64_t w) {
            if (w == wall) {
                return start_unix;   // the start as it is: the second of a time shown twice too
            }
            int64_t day = detail::rec_floor_div(w, 86400);
            int64_t sod = w - day * 86400;
            return time::date(std::chrono::sys_days(std::chrono::days(day))).at(int(sod / 3600), int(sod / 60 % 60), int(sod % 60), z).unix();
        };
        // whole seconds a second wider on each side, the exact bounds after
        auto walls = detail::rec_expand(_d(), wall, false, resolve, from.unix() - 1, to.unix() + 1, limit > SIZE_MAX - 4 ? limit : limit + 4);
        vector<time::datetime> out;
        for (int64_t w : walls) {
            time::datetime t = time::datetime::from_unix_nano(resolve(w) * 1000000000 + frac, z);
            if (t >= from && t < to && out.size() < limit) {
                out.push_back(t);
            }
        }
        return out;
    }
}
