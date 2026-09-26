//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/clock.h"
#include "../core/duration.h"
#include "../core/string.h"
#include "date.h"
#include "zone.h"

#include <chrono>
#include <compare>
#include <cstdint>
#include <iosfwd>
#include <limits>

namespace sgcl::time {
    namespace detail {
        inline constexpr int64_t NanosPerSecond = 1000000000;


        // A time of the clock in a zone, as the instants that show it: one,
        // two (a time shown twice, the earlier first) or none (a time the
        // clock skipped; then the change that skipped it and the offsets on
        // either side). In seconds; the part of a second rides beside.
        struct wall_instants {
            int count = 0;
            int64_t first = 0;
            int64_t second = 0;
            bool skipped = false;           // a skip was found:
            int64_t change = 0;             // its change
            int32_t before = 0;             // the offset before it
        };

        // Every instant showing `wall` (seconds of the clock since
        // 1970-01-01T00:00 as if it were UTC) lies within a day and two
        // hours of it, an offset being less than that: the zone's periods
        // over that window are walked, each asked whether it shows the time
        inline wall_instants instants_of(const zone_data& z, int64_t wall) noexcept {
            wall_instants r;
            constexpr int64_t Window = 26 * 3600 + 3600;
            int64_t start = wall - Window;
            int32_t offset = zone_access::offset_at(z, start);
            for (int guard = 0; guard < 64; ++guard) {
                optional<int64_t> next = zone_access::next_change(z, start);
                int64_t end = next ? *next : INT64_MAX;
                int64_t t = wall - offset;
                if (t >= start && t < end) {
                    if (r.count == 0) {
                        r.first = t;
                    } else {
                        r.second = t;
                    }
                    r.count = r.count < 2 ? r.count + 1 : 2;
                }
                if (!next || *next > wall + Window) {
                    break;
                }
                int32_t after = zone_access::offset_at(z, *next);
                if (after > offset && wall >= *next + offset && wall < *next + after) {
                    r.skipped = true;
                    r.change = *next;
                    r.before = offset;
                }
                start = *next;
                offset = after;
            }
            return r;
        }

        // The rule that settles the two cases, as a number: 0 compatible,
        // 1 earlier, 2 later
        inline int64_t instant_of(const zone_data& z, int64_t wall, int rule) noexcept {
            wall_instants r = instants_of(z, wall);
            if (r.count == 1) {
                return r.first;
            }
            if (r.count == 2) {
                return rule == 2 ? r.second : r.first;
            }
            // Skipped: the change itself, or the time moved on by the skip.
            // Neither found (a zone of more changes in two days than any
            // real one has): the time at the offset in force at it
            if (!r.skipped) {
                return wall - zone_access::offset_at(z, wall - zone_access::offset_at(z, wall));
            }
            return rule == 1 ? r.change : wall - r.before;
        }

        // Seconds and a part of a second into nanoseconds, saturated at the
        // ends of a datetime's range
        inline int64_t to_nanos(int64_t seconds, int64_t nanos) noexcept {
            __int128 v = (__int128)seconds * NanosPerSecond + nanos;
            return v < INT64_MIN ? INT64_MIN : v > INT64_MAX ? INT64_MAX : int64_t(v);
        }

        // The seconds of the clock since 1970 of a date and a time of day,
        // carried
        constexpr int64_t wall_seconds(time::date d, int64_t hour, int64_t minute, int64_t second) noexcept {
            return int64_t(-d.days_until(time::date(1970, 1, 1))) * 86400 + hour * 3600 + minute * 60 + second;
        }
    }

    // An instant and the zone it is seen in (Go's time.Time): the
    // nanoseconds since 1970-01-01T00:00:00Z in 64 bits, which is the
    // years 1677 to 2262, and a zone. Sixteen bytes: the count and the
    // zone's pointer, so a value that lives where a tracked_ptr may (on
    // a stack, in a managed object or a container of the library), as a
    // string does (DESIGN 219).
    //
    // The fields — year to nanosecond, the day of the week, the date —
    // are the ones the zone's clock shows at that instant. Two datetimes
    // are equal when they are the same instant, whatever their zones: the
    // same moment in Warsaw and in UTC is equal (Go's == compares the zone
    // too, a known trap there); a.zone() == b.zone() asks the other
    // question. Arithmetic past either end of the range stops at the end.
    //
    // The exact arithmetic is t + d, t - d and t2 - t1, a duration of
    // nanoseconds; the calendar's is add_days, add_months, add_years,
    // which keep the time of the clock: a day later across a change of
    // the clock is 23 or 25 hours later.
    class datetime {
    public:
        // 1970-01-01T00:00:00Z, in UTC
        datetime() noexcept = default;

        // For the library (detail): an instant in a zone given by its data
        // (null: UTC), made where it goes — the zone's pointer made once
        datetime(detail::made_in_place, int64_t ns, const detail::zone_data& z) noexcept
        : _ns(ns), _zone(detail::made_in_place(), z) {
        }

        // An instant of the system clock, and what io::file_info::modified
        // is; in the local zone unless another is given
        explicit datetime(std::chrono::sys_time<std::chrono::nanoseconds> t, const time::zone& z = time::zone::local()) noexcept
        : _ns(t.time_since_epoch().count())
        , _zone(z) {
        }

        // From the seconds, milliseconds, microseconds or nanoseconds since
        // 1970 (Unix time, negative before 1970), in the local zone unless
        // another is given; beyond the range, the end of it. The units
        // written out, as Go's UnixMilli and duration's methods have them
        static datetime from_unix(int64_t seconds, const time::zone& z = time::zone::local()) noexcept {
            return _of(detail::to_nanos(seconds, 0), z);
        }

        static datetime from_unix_milli(int64_t milliseconds, const time::zone& z = time::zone::local()) noexcept {
            int64_t s = sgcl::time::detail::floor_div(milliseconds, 1000);
            return _of(detail::to_nanos(s, (milliseconds - s * 1000) * 1000000), z);
        }

        static datetime from_unix_micro(int64_t microseconds, const time::zone& z = time::zone::local()) noexcept {
            int64_t s = sgcl::time::detail::floor_div(microseconds, 1000000);
            return _of(detail::to_nanos(s, (microseconds - s * 1000000) * 1000), z);
        }

        static datetime from_unix_nano(int64_t nanoseconds, const time::zone& z = time::zone::local()) noexcept {
            return _of(nanoseconds, z);
        }

        // A text in a format known by name: rfc3339, http (with its two
        // obsolete forms), email, iso8601 (layout.h says what each takes).
        // The datetime is in UTC where the text says UTC or GMT or says
        // nothing, else in a fixed zone of the offset the text gives; a
        // text that is not one, or an instant outside the years 1677 to
        // 2262, is an error saying why and at which byte
        static expected<datetime, error> parse(const string& text, layout format);

        // A text in a pattern of std::format's specifiers for <chrono>,
        // read as std::chrono::parse reads one: "%d.%m.%Y %H:%M". A date is
        // needed (the time of day is midnight where the pattern has none).
        // An offset in the text (%z) makes a fixed zone; with none, the
        // time is one of the zone given (UTC unless another is), read as
        // date::at reads it — and a %Z naming an abbreviation that zone
        // has there settles a time shown twice ("02:30 CET")
        static expected<datetime, error> parse(const string& text, const string& pattern, const time::zone& z = time::zone::utc());

        // The seconds, milliseconds, microseconds and nanoseconds since
        // 1970, rounded down (so -0.5 s is -1 s: 1969-12-31T23:59:59)
        int64_t unix() const noexcept {
            return _seconds();
        }

        int64_t unix_milli() const noexcept {
            return sgcl::time::detail::floor_div(_ns, 1000000);
        }

        int64_t unix_micro() const noexcept {
            return sgcl::time::detail::floor_div(_ns, 1000);
        }

        int64_t unix_nano() const noexcept {
            return _ns;
        }

        std::chrono::sys_time<std::chrono::nanoseconds> to_sys() const noexcept {
            return std::chrono::sys_time<std::chrono::nanoseconds>(std::chrono::nanoseconds(_ns));
        }

        // The zone, and the same instant in another
        time::zone zone() const noexcept {
            return _zone;
        }

        datetime in(const time::zone& z) const noexcept {
            return _of(_ns, z);
        }

        datetime utc() const noexcept {
            return _of(_ns, time::zone::utc());
        }

        datetime local() const {
            return _of(_ns, time::zone::local());
        }

        // The zone at this instant: +2h in Warsaw in summer, "CEST", true
        duration offset() const noexcept {
            return std::chrono::seconds(_offset());
        }

        string abbreviation() const {
            const auto& z = detail::zone_access::data(_zone);
            return detail::zone_access::abbreviation(z, detail::zone_access::state_at(z, _seconds()));
        }

        bool is_dst() const noexcept {
            return detail::zone_access::state_at(detail::zone_access::data(_zone), _seconds()).dst;
        }

        // The date of the zone's clock
        time::date date() const noexcept {
            return time::date(std::chrono::sys_days(std::chrono::days(sgcl::time::detail::floor_div(_wall(), 86400))));
        }

        int year() const noexcept {
            return date().year();
        }

        // January to December (int(t.month()) is 1 to 12)
        time::month month() const noexcept {
            return date().month();
        }

        // 1 to 31
        int day() const noexcept {
            return date().day();
        }

        // 0 to 23
        int hour() const noexcept {
            return int(_second_of_day() / 3600);
        }

        // 0 to 59
        int minute() const noexcept {
            return int(_second_of_day() / 60 % 60);
        }

        // 0 to 59
        int second() const noexcept {
            return int(_second_of_day() % 60);
        }

        // 0 to 999999999
        int nanosecond() const noexcept {
            return int(_fraction());
        }

        time::weekday weekday() const noexcept {
            return date().weekday();
        }

        // 1 to 366
        int year_day() const noexcept {
            return date().year_day();
        }

        time::iso_week iso_week() const noexcept {
            return date().iso_week();
        }

        // The same time of the clock n days, months or years on, in the same
        // zone: across a change of the clock a day is 23 or 25 hours; a
        // month on from the 31st is the month's last day (2026-01-31 plus
        // one month is 2026-02-28); a time of the clock that the new day
        // does not have is read as date::at reads it
        datetime add_days(int n) const {
            return _at_wall(detail::wall_seconds(date().add_days(n), 0, 0, 0) + _second_of_day());
        }

        datetime add_months(int n) const {
            return _at_wall(detail::wall_seconds(date().add_months(n), 0, 0, 0) + _second_of_day());
        }

        datetime add_years(int n) const {
            return _at_wall(detail::wall_seconds(date().add_years(n), 0, 0, 0) + _second_of_day());
        }

        // Down to a whole number of steps, and to the nearest one (a half
        // up), counted as Go counts them: from 0001-01-01T00:00:00Z, which
        // for a step that divides a day is the same as counting from
        // midnight UTC; not from midnight of the zone. A step of zero or
        // less leaves the time as it is
        datetime truncate(duration step) const noexcept {
            int64_t d = step.nanoseconds();
            if (d <= 0) {
                return *this;
            }
            int64_t down;
            if (__builtin_sub_overflow(_ns, _remainder(d), &down)) {
                down = INT64_MIN;   // the step below is before the range: its start
            }
            return _of(down, _zone);
        }

        datetime round(duration step) const noexcept {
            int64_t d = step.nanoseconds();
            if (d <= 0) {
                return *this;
            }
            int64_t r = _remainder(d);
            if (uint64_t(r) + uint64_t(r) < uint64_t(d)) {
                int64_t down;
                if (__builtin_sub_overflow(_ns, r, &down)) {
                    down = INT64_MIN;
                }
                return _of(down, _zone);
            }
            int64_t up;
            if (__builtin_add_overflow(_ns, d - r, &up)) {
                up = INT64_MAX;   // the next step is past the range: its end
            }
            return _of(up, _zone);
        }

        // The first instant of this datetime's date in its zone
        datetime start_of_day() const;

        // The text of the datetime by a format known by name,
        // t.format(time::http), or by a pattern of std::format's
        // specifiers for <chrono>, t.format("%d.%m.%Y %H:%M") (layout.h);
        // a pattern's specifier this does not know is written as it stands
        string format(layout format) const;
        string format(const string& pattern) const;

        // RFC 3339 with the fraction only where there is one:
        // "2026-09-24T12:41:15+02:00", "2026-09-24T10:41:15.5Z"
        string to_string() const;

        template<class CharT, class Traits>
        friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const datetime& t) {
            return os << t.to_string();
        }

        // The instant later or earlier by d, in the same zone; the
        // duration between two instants
        friend datetime operator+(const datetime& t, duration d) noexcept {
            int64_t out;
            if (__builtin_add_overflow(t._ns, d.nanoseconds(), &out)) {
                out = d.nanoseconds() < 0 ? INT64_MIN : INT64_MAX;
            }
            return _of(out, t._zone);
        }

        friend datetime operator+(duration d, const datetime& t) noexcept {
            return t + d;
        }

        friend datetime operator-(const datetime& t, duration d) noexcept {
            return t + (-d);
        }

        friend duration operator-(const datetime& a, const datetime& b) noexcept {
            int64_t out;
            if (__builtin_sub_overflow(a._ns, b._ns, &out)) {
                return a._ns > b._ns ? duration::max() : duration::min();
            }
            return std::chrono::nanoseconds(out);
        }

        datetime& operator+=(duration d) noexcept {
            return *this = *this + d;
        }

        datetime& operator-=(duration d) noexcept {
            return *this = *this - d;
        }

        friend bool operator==(const datetime& a, const datetime& b) noexcept {
            return a._ns == b._ns;
        }

        friend std::strong_ordering operator<=>(const datetime& a, const datetime& b) noexcept {
            return a._ns <=> b._ns;
        }

    private:
        friend class time::date;
        friend class time::zone;
        friend struct time::detail::layout_writer;

        // Made with its zone at once: one copy of the zone's pointer, not a
        // null one first and an assignment after
        datetime(int64_t ns, const time::zone& z) noexcept
        : _ns(ns), _zone(z) {
        }

        static datetime _of(int64_t ns, const time::zone& z) noexcept {
            return datetime(ns, z);
        }

        int64_t _seconds() const noexcept {
            return sgcl::time::detail::floor_div(_ns, detail::NanosPerSecond);
        }

        // The part of a second, 0 to 999999999: the remainder taken as it
        // is, since the product of the seconds and 10^9 does not fit in the
        // first second of the range
        int64_t _fraction() const noexcept {
            int64_t r = _ns % detail::NanosPerSecond;
            return r < 0 ? r + detail::NanosPerSecond : r;
        }

        int32_t _offset() const noexcept {
            return detail::zone_access::offset_at(_zone, _seconds());
        }

        // The seconds of the zone's clock since 1970-01-01T00:00
        int64_t _wall() const noexcept {
            return _seconds() + _offset();
        }

        int64_t _second_of_day() const noexcept {
            int64_t w = _wall();
            return w - sgcl::time::detail::floor_div(w, 86400) * 86400;
        }

        // The instant a time of this zone's clock is, with this instant's
        // part of a second, read as date::at reads it
        datetime _at_wall(int64_t wall, int rule = 0) const noexcept {
            return _of(detail::to_nanos(detail::instant_of(detail::zone_access::data(_zone), wall, rule), _fraction()), _zone);
        }

        // Where this instant is within a step, counted from Go's zero time
        // (0001-01-01T00:00:00Z, 62135596800 seconds before 1970), which
        // does not fit 64 bits of nanoseconds: the two remainders added
        int64_t _remainder(int64_t d) const noexcept {
            constexpr unsigned __int128 ZeroToEpoch = (unsigned __int128)62135596800ull * 1000000000ull;
            int64_t own = _ns % d;
            if (own < 0) {
                own += d;
            }
            int64_t shift = int64_t(ZeroToEpoch % (unsigned __int128)d);
            int64_t r = own + shift;   // both below d, so no overflow of 2^63
            return r >= d ? r - d : r;
        }

        int64_t _ns = 0;
        time::zone _zone;
    };

    // The time now, on the system's clock, in the local zone. While a
    // test has a manual_clock installed (async/timer.h), the wall time of
    // the install moved on by as much as the manual time has been
    // advanced: code that asks for the time is tested with no real waiting
    namespace detail {
        // The nanoseconds since 1970 now() reads
        inline int64_t now_nanos() noexcept {
            if (sgcl::detail::manual_clock_installed.load(std::memory_order_acquire)) [[unlikely]] {
                auto moved = time_point::duration(sgcl::detail::manual_clock_now.load(std::memory_order_acquire) - sgcl::detail::manual_clock_origin.load(std::memory_order_relaxed));
                return sgcl::detail::manual_clock_wall_origin.load(std::memory_order_relaxed)
                     + std::chrono::duration_cast<std::chrono::nanoseconds>(moved).count();
            }
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
    }

    inline datetime now() {
        return datetime(detail::made_in_place(), detail::now_nanos(), detail::local_data());
    }

    //--------------------------------------------------------------------
    // The members of date and zone that are made of datetimes
    //--------------------------------------------------------------------
    inline datetime date::at(int hour, int minute, const zone& z) const {
        return at(hour, minute, 0, z);
    }

    inline datetime date::at(int hour, int minute, int second, const zone& z) const {
        return datetime::_of(detail::to_nanos(detail::instant_of(detail::zone_access::data(z), detail::wall_seconds(*this, hour, minute, second), 0), 0), z);
    }

    inline datetime date::at(int hour, int minute, const zone& z, earlier_t) const {
        return at(hour, minute, 0, z, earlier);
    }

    inline datetime date::at(int hour, int minute, int second, const zone& z, earlier_t) const {
        return datetime::_of(detail::to_nanos(detail::instant_of(detail::zone_access::data(z), detail::wall_seconds(*this, hour, minute, second), 1), 0), z);
    }

    inline datetime date::at(int hour, int minute, const zone& z, later_t) const {
        return at(hour, minute, 0, z, later);
    }

    inline datetime date::at(int hour, int minute, int second, const zone& z, later_t) const {
        return datetime::_of(detail::to_nanos(detail::instant_of(detail::zone_access::data(z), detail::wall_seconds(*this, hour, minute, second), 2), 0), z);
    }

    inline optional<datetime> date::try_at(int hour, int minute, int second, const zone& z) const {
        detail::wall_instants r = detail::instants_of(detail::zone_access::data(z), detail::wall_seconds(*this, hour, minute, second));
        if (r.count != 1) {
            return nullopt;
        }
        return datetime::_of(detail::to_nanos(r.first, 0), z);
    }

    inline datetime date::start_of_day(const zone& z) const {
        return at(0, 0, 0, z, earlier);
    }

    inline datetime datetime::start_of_day() const {
        return date().start_of_day(_zone);
    }

    inline duration zone::offset_at(const datetime& t) const {
        return std::chrono::seconds(detail::zone_access::offset_at(*this, t._seconds()));
    }

    inline string zone::abbreviation_at(const datetime& t) const {
        const auto& z = detail::zone_access::data(*this);
        return detail::zone_access::abbreviation(z, detail::zone_access::state_at(z, t._seconds()));
    }

    inline bool zone::is_dst_at(const datetime& t) const {
        return detail::zone_access::state_at(detail::zone_access::data(*this), t._seconds()).dst;
    }

    inline optional<datetime> zone::next_transition(const datetime& t) const {
        // A change is on a whole second; t within that second is before it
        auto c = detail::zone_access::next_change(detail::zone_access::data(*this), t._seconds());
        if (!c) {
            return nullopt;
        }
        int64_t ns = detail::to_nanos(*c, 0);
        if (ns == INT64_MAX || ns == INT64_MIN) {
            return nullopt;   // past the range of a datetime
        }
        return datetime::_of(ns, *this);
    }

    inline optional<datetime> zone::previous_transition(const datetime& t) const {
        // Strictly before t: a change at t's own second counts when t is
        // past its start
        int64_t s = t._seconds();
        int64_t from = t._ns == detail::to_nanos(s, 0) ? s : s + 1;
        auto c = detail::zone_access::previous_change(detail::zone_access::data(*this), from);
        if (!c) {
            return nullopt;
        }
        int64_t ns = detail::to_nanos(*c, 0);
        if (ns == INT64_MAX || ns == INT64_MIN) {
            return nullopt;
        }
        return datetime::_of(ns, *this);
    }

    //--------------------------------------------------------------------
    // RFC 3339
    //--------------------------------------------------------------------
    namespace detail {
        inline char* put_digits(char* out, int64_t v, int width) noexcept {
            for (int i = width - 1; i >= 0; --i) {
                out[i] = char('0' + v % 10);
                v /= 10;
            }
            return out + width;
        }
    }

    inline string datetime::to_string() const {
        char text[40];
        char* out = text;
        time::date d = date();
        int y = d.year();
        if (y < 0) {
            *out++ = '-';
            y = -y;
        }
        out = detail::put_digits(out, y, 4);
        *out++ = '-';
        out = detail::put_digits(out, int(d.month()), 2);
        *out++ = '-';
        out = detail::put_digits(out, d.day(), 2);
        *out++ = 'T';
        int64_t s = _second_of_day();
        out = detail::put_digits(out, s / 3600, 2);
        *out++ = ':';
        out = detail::put_digits(out, s / 60 % 60, 2);
        *out++ = ':';
        out = detail::put_digits(out, s % 60, 2);
        int64_t fraction = _fraction();
        if (fraction) {
            int digits = 9;
            while (fraction % 10 == 0) {
                fraction /= 10;
                --digits;
            }
            *out++ = '.';
            out = detail::put_digits(out, fraction, digits);
        }
        int32_t offset = _offset();
        if (offset == 0) {
            *out++ = 'Z';
        } else {
            *out++ = offset < 0 ? '-' : '+';
            int32_t a = offset < 0 ? -offset : offset;
            out = detail::put_digits(out, a / 3600, 2);
            *out++ = ':';
            out = detail::put_digits(out, a / 60 % 60, 2);
        }
        return string(text, size_t(out - text));
    }
}
