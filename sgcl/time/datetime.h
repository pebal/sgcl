//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
        SGCL_INLINE_HOT int64_t instant_of(const zone_data& z, int64_t wall, int rule) noexcept {
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
        SGCL_INLINE_HOT int64_t to_nanos(int64_t seconds, int64_t nanos) noexcept {
            __int128 v = (__int128)seconds * NanosPerSecond + nanos;
            return v < INT64_MIN ? INT64_MIN : v > INT64_MAX ? INT64_MAX : int64_t(v);
        }

        // The seconds of the clock since 1970 of a date and a time of day,
        // carried
        SGCL_INLINE_HOT constexpr int64_t wall_seconds(time::date d, int64_t hour, int64_t minute, int64_t second) noexcept {
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

        // An instant of the system clock, and what io::file_info::modified
        // is; in the local zone unless another is given
        SGCL_INLINE_HOT explicit datetime(std::chrono::sys_time<std::chrono::nanoseconds> t, const time::zone& z = time::zone::local()) noexcept
        : _ns(t.time_since_epoch().count())
        , _zone(z) {
        }

        // From the seconds, milliseconds, microseconds or nanoseconds since
        // 1970 (Unix time, negative before 1970), in the local zone unless
        // another is given; beyond the range, the end of it. The units
        // written out, as Go's UnixMilli and duration's methods have them
        SGCL_INLINE_HOT static datetime from_unix(int64_t seconds, const time::zone& z = time::zone::local()) noexcept {
            return _of(detail::to_nanos(seconds, 0), z);
        }

        // The part below a second is the count less s · 1000, worked
        // modulo 2^64: for the least counts the product does not fit 64
        // bits, but the difference is 0 to 999 and comes out exact, so
        // nothing is checked and the code is the multiply it always was
        SGCL_INLINE_HOT static datetime from_unix_milli(int64_t milliseconds, const time::zone& z = time::zone::local()) noexcept {
            int64_t s = sgcl::time::detail::floor_div(milliseconds, 1000);
            int64_t rest = int64_t(uint64_t(milliseconds) - uint64_t(s) * 1000);
            return _of(detail::to_nanos(s, rest * 1000000), z);
        }

        SGCL_INLINE_HOT static datetime from_unix_micro(int64_t microseconds, const time::zone& z = time::zone::local()) noexcept {
            int64_t s = sgcl::time::detail::floor_div(microseconds, 1000000);
            int64_t rest = int64_t(uint64_t(microseconds) - uint64_t(s) * 1000000);
            return _of(detail::to_nanos(s, rest * 1000), z);
        }

        SGCL_INLINE_HOT static datetime from_unix_nano(int64_t nanoseconds, const time::zone& z = time::zone::local()) noexcept {
            return _of(nanoseconds, z);
        }

        // A text in a format known by name: rfc3339, http (with its two
        // obsolete forms), email, iso8601 (layout.h says what each takes).
        // The datetime is in UTC where the text says UTC or GMT or says
        // nothing, else in a fixed zone of the offset the text gives; a
        // text that is not one, or an instant outside the years 1677 to
        // 2262, is an error saying why and at which byte
        static expected<datetime, error> parse(const string& text, layout format) noexcept;

        // A text in a pattern of std::format's specifiers for <chrono>,
        // read as std::chrono::parse reads one: "%d.%m.%Y %H:%M". A date is
        // needed (the time of day is midnight where the pattern has none).
        // An offset in the text (%z) makes a fixed zone; with none, the
        // time is one of the zone given (UTC unless another is), read as
        // date::at reads it — and a %Z naming an abbreviation that zone
        // has there settles a time shown twice ("02:30 CET")
        static expected<datetime, error> parse(const string& text, const string& pattern, const time::zone& z = time::zone::utc()) noexcept;

        // The datetime a literal in the program spells, in either of
        // parse's forms: parse's value, or bad_expected_access<time::error>
        // with parse's message. Input is parsed; a text the program itself
        // wrote is constructed (DESIGN 234). The first is defined in
        // layout.h, where a layout is complete
        explicit datetime(const string& text, layout format);

        SGCL_INLINE_HOT explicit datetime(const string& text, const string& pattern, const time::zone& z = time::zone::utc())
        : datetime(parse(text, pattern, z).value()) {
        }

        // The seconds, milliseconds, microseconds and nanoseconds since
        // 1970, rounded down (so -0.5 s is -1 s: 1969-12-31T23:59:59)
        SGCL_INLINE_HOT int64_t unix() const noexcept {
            return _seconds();
        }

        SGCL_INLINE_HOT int64_t unix_milli() const noexcept {
            return sgcl::time::detail::floor_div(_ns, 1000000);
        }

        SGCL_INLINE_HOT int64_t unix_micro() const noexcept {
            return sgcl::time::detail::floor_div(_ns, 1000);
        }

        SGCL_INLINE_HOT int64_t unix_nano() const noexcept {
            return _ns;
        }

        SGCL_INLINE_HOT std::chrono::sys_time<std::chrono::nanoseconds> to_sys() const noexcept {
            return std::chrono::sys_time<std::chrono::nanoseconds>(std::chrono::nanoseconds(_ns));
        }

        // The zone, and the same instant in another
        SGCL_INLINE_HOT time::zone zone() const noexcept {
            return _zone;
        }

        SGCL_INLINE_HOT datetime in(const time::zone& z) const noexcept {
            return _of(_ns, z);
        }

        SGCL_INLINE_HOT datetime utc() const noexcept {
            return _of(_ns, time::zone::utc());
        }

        SGCL_INLINE_HOT datetime local() const noexcept {
            return _of(_ns, time::zone::local());
        }

        // The zone at this instant: +2h in Warsaw in summer, "CEST", true
        SGCL_INLINE_HOT duration offset() const noexcept {
            return std::chrono::seconds(_offset());
        }

        SGCL_INLINE_HOT string abbreviation() const noexcept {
            const auto& z = detail::zone_access::data(_zone);
            return detail::zone_access::abbreviation(z, detail::zone_access::state_at(z, _seconds()));
        }

        SGCL_INLINE_HOT bool is_dst() const noexcept {
            return detail::zone_access::state_at(detail::zone_access::data(_zone), _seconds()).dst;
        }

        // The date of the zone's clock
        SGCL_INLINE_HOT time::date date() const noexcept {
            return time::date(std::chrono::sys_days(std::chrono::days(sgcl::time::detail::floor_div(_wall(), 86400))));
        }

        SGCL_INLINE_HOT int year() const noexcept {
            return date().year();
        }

        // January to December (int(t.month()) is 1 to 12)
        SGCL_INLINE_HOT time::month month() const noexcept {
            return date().month();
        }

        // 1 to 31
        SGCL_INLINE_HOT int day() const noexcept {
            return date().day();
        }

        // 0 to 23
        SGCL_INLINE_HOT int hour() const noexcept {
            return int(_second_of_day() / 3600);
        }

        // 0 to 59
        SGCL_INLINE_HOT int minute() const noexcept {
            return int(_second_of_day() / 60 % 60);
        }

        // 0 to 59
        SGCL_INLINE_HOT int second() const noexcept {
            return int(_second_of_day() % 60);
        }

        // 0 to 999999999
        SGCL_INLINE_HOT int nanosecond() const noexcept {
            return int(_fraction());
        }

        SGCL_INLINE_HOT time::weekday weekday() const noexcept {
            return date().weekday();
        }

        // 1 to 366
        SGCL_INLINE_HOT int year_day() const noexcept {
            return date().year_day();
        }

        SGCL_INLINE_HOT time::iso_week iso_week() const noexcept {
            return date().iso_week();
        }

        // The same time of the clock n days, months or years on, in the same
        // zone: across a change of the clock a day is 23 or 25 hours; a
        // month on from the 31st is the month's last day (2026-01-31 plus
        // one month is 2026-02-28); a time of the clock that the new day
        // does not have is read as date::at reads it
        SGCL_INLINE_HOT datetime add_days(int n) const noexcept {
            return _at_wall(detail::wall_seconds(date().add_days(n), 0, 0, 0) + _second_of_day());
        }

        SGCL_INLINE_HOT datetime add_months(int n) const noexcept {
            return _at_wall(detail::wall_seconds(date().add_months(n), 0, 0, 0) + _second_of_day());
        }

        SGCL_INLINE_HOT datetime add_years(int n) const noexcept {
            return _at_wall(detail::wall_seconds(date().add_years(n), 0, 0, 0) + _second_of_day());
        }

        // Down to a whole number of steps, and to the nearest one (a half
        // up), counted as Go counts them: from 0001-01-01T00:00:00Z, which
        // for a step that divides a day is the same as counting from
        // midnight UTC; not from midnight of the zone. A step of zero or
        // less leaves the time as it is
        SGCL_INLINE_HOT datetime truncate(duration step) const noexcept {
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
        datetime start_of_day() const noexcept;

        // The text of the datetime by a format known by name,
        // t.format(time::http), or by a pattern of std::format's
        // specifiers for <chrono>, t.format("%d.%m.%Y %H:%M") (layout.h);
        // a pattern's specifier this does not know is written as it stands
        string format(layout format) const noexcept;
        string format(const string& pattern) const noexcept;

        // RFC 3339 with the fraction only where there is one:
        // "2026-09-24T12:41:15+02:00", "2026-09-24T10:41:15.5Z"
        string to_string() const noexcept;

        template<class CharT, class Traits>
        SGCL_INLINE_HOT friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const datetime& t) {
            return os << t.to_string();
        }

        // The instant later or earlier by d, in the same zone; the
        // duration between two instants
        SGCL_INLINE_HOT friend datetime operator+(const datetime& t, duration d) noexcept {
            int64_t out;
            if (__builtin_add_overflow(t._ns, d.nanoseconds(), &out)) {
                out = d.nanoseconds() < 0 ? INT64_MIN : INT64_MAX;
            }
            return _of(out, t._zone);
        }

        SGCL_INLINE_HOT friend datetime operator+(duration d, const datetime& t) noexcept {
            return t + d;
        }

        // Its own subtraction, not t + (-d): the negation of
        // duration::min() is duration::max(), a nanosecond short
        SGCL_INLINE_HOT friend datetime operator-(const datetime& t, duration d) noexcept {
            int64_t out;
            if (__builtin_sub_overflow(t._ns, d.nanoseconds(), &out)) {
                out = d.nanoseconds() < 0 ? INT64_MAX : INT64_MIN;
            }
            return _of(out, t._zone);
        }

        SGCL_INLINE_HOT friend duration operator-(const datetime& a, const datetime& b) noexcept {
            int64_t out;
            if (__builtin_sub_overflow(a._ns, b._ns, &out)) {
                return a._ns > b._ns ? duration::max() : duration::min();
            }
            return std::chrono::nanoseconds(out);
        }

        SGCL_INLINE_HOT datetime& operator+=(duration d) noexcept {
            return *this = *this + d;
        }

        SGCL_INLINE_HOT datetime& operator-=(duration d) noexcept {
            return *this = *this - d;
        }

        SGCL_INLINE_HOT friend bool operator==(const datetime& a, const datetime& b) noexcept {
            return a._ns == b._ns;
        }

        SGCL_INLINE_HOT friend std::strong_ordering operator<=>(const datetime& a, const datetime& b) noexcept {
            return a._ns <=> b._ns;
        }

    private:
        friend class time::date;
        friend class time::zone;
        friend struct time::detail::layout_writer;
        friend struct time::detail::zone_access;

        // Made with its zone at once: one copy of the zone's pointer, not a
        // null one first and an assignment after
        SGCL_INLINE_HOT datetime(int64_t ns, const time::zone& z) noexcept
        : _ns(ns), _zone(z) {
        }

        // The library's own (detail::zone_access::make_datetime): an
        // instant in a zone given by its data, the zone's pointer made once
        SGCL_INLINE_HOT datetime(detail::made_in_place, int64_t ns, const detail::zone_data& z) noexcept
        : _ns(ns), _zone(detail::zone_access::make(z)) {
        }

        SGCL_INLINE_HOT static datetime _of(int64_t ns, const time::zone& z) noexcept {
            return datetime(ns, z);
        }

        SGCL_INLINE_HOT int64_t _seconds() const noexcept {
            return sgcl::time::detail::floor_div(_ns, detail::NanosPerSecond);
        }

        // The part of a second, 0 to 999999999: the remainder taken as it
        // is, since the product of the seconds and 10^9 does not fit in the
        // first second of the range
        SGCL_INLINE_HOT int64_t _fraction() const noexcept {
            int64_t r = _ns % detail::NanosPerSecond;
            return r < 0 ? r + detail::NanosPerSecond : r;
        }

        SGCL_INLINE_HOT int32_t _offset() const noexcept {
            return detail::zone_access::offset_at(_zone, _seconds());
        }

        // The seconds of the zone's clock since 1970-01-01T00:00
        SGCL_INLINE_HOT int64_t _wall() const noexcept {
            return _seconds() + _offset();
        }

        SGCL_INLINE_HOT int64_t _second_of_day() const noexcept {
            int64_t w = _wall();
            return w - sgcl::time::detail::floor_div(w, 86400) * 86400;
        }

        // The instant a time of this zone's clock is, with this instant's
        // part of a second, read as date::at reads it
        SGCL_INLINE_HOT datetime _at_wall(int64_t wall, int rule = 0) const noexcept {
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
            // Both below d, which is below 2^63: the sum below 2^64, which an
            // unsigned word holds and a signed one does not
            uint64_t r = uint64_t(own) + uint64_t(shift);
            return int64_t(r >= uint64_t(d) ? r - uint64_t(d) : r);
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
        SGCL_INLINE_HOT int64_t now_nanos() noexcept {
            if (sgcl::detail::manual_clock_installed.load(std::memory_order_acquire)) [[unlikely]] {
                auto moved = time_point::duration(sgcl::detail::manual_clock_now.load(std::memory_order_acquire) - sgcl::detail::manual_clock_origin.load(std::memory_order_relaxed));
                return sgcl::detail::manual_clock_wall_origin.load(std::memory_order_relaxed)
                     + std::chrono::duration_cast<std::chrono::nanoseconds>(moved).count();
            }
            return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
    }

    SGCL_INLINE_HOT datetime now() noexcept {
        return detail::zone_access::make_datetime<datetime>(detail::now_nanos(), detail::local_data());
    }

    //--------------------------------------------------------------------
    // The members of date and zone that are made of datetimes
    //--------------------------------------------------------------------
    SGCL_INLINE_HOT datetime date::at(int hour, int minute, const zone& z) const noexcept {
        return at(hour, minute, 0, z);
    }

    SGCL_INLINE_HOT datetime date::at(int hour, int minute, int second, const zone& z) const noexcept {
        return datetime::_of(detail::to_nanos(detail::instant_of(detail::zone_access::data(z), detail::wall_seconds(*this, hour, minute, second), 0), 0), z);
    }

    SGCL_INLINE_HOT datetime date::at(int hour, int minute, const zone& z, earlier_t) const noexcept {
        return at(hour, minute, 0, z, earlier);
    }

    SGCL_INLINE_HOT datetime date::at(int hour, int minute, int second, const zone& z, earlier_t) const noexcept {
        return datetime::_of(detail::to_nanos(detail::instant_of(detail::zone_access::data(z), detail::wall_seconds(*this, hour, minute, second), 1), 0), z);
    }

    SGCL_INLINE_HOT datetime date::at(int hour, int minute, const zone& z, later_t) const noexcept {
        return at(hour, minute, 0, z, later);
    }

    SGCL_INLINE_HOT datetime date::at(int hour, int minute, int second, const zone& z, later_t) const noexcept {
        return datetime::_of(detail::to_nanos(detail::instant_of(detail::zone_access::data(z), detail::wall_seconds(*this, hour, minute, second), 2), 0), z);
    }

    SGCL_INLINE_HOT optional<datetime> date::try_at(int hour, int minute, int second, const zone& z) const noexcept {
        detail::wall_instants r = detail::instants_of(detail::zone_access::data(z), detail::wall_seconds(*this, hour, minute, second));
        if (r.count != 1) {
            return nullopt;
        }
        return datetime::_of(detail::to_nanos(r.first, 0), z);
    }

    SGCL_INLINE_HOT datetime date::start_of_day(const zone& z) const noexcept {
        return at(0, 0, 0, z, earlier);
    }

    SGCL_INLINE_HOT datetime datetime::start_of_day() const noexcept {
        return date().start_of_day(_zone);
    }

    SGCL_INLINE_HOT duration zone::offset_at(const datetime& t) const noexcept {
        return std::chrono::seconds(detail::zone_access::offset_at(*this, t._seconds()));
    }

    SGCL_INLINE_HOT string zone::abbreviation_at(const datetime& t) const noexcept {
        const auto& z = detail::zone_access::data(*this);
        return detail::zone_access::abbreviation(z, detail::zone_access::state_at(z, t._seconds()));
    }

    SGCL_INLINE_HOT bool zone::is_dst_at(const datetime& t) const noexcept {
        return detail::zone_access::state_at(detail::zone_access::data(*this), t._seconds()).dst;
    }

    SGCL_INLINE_HOT optional<datetime> zone::next_transition(const datetime& t) const noexcept {
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

    inline optional<datetime> zone::previous_transition(const datetime& t) const noexcept {
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

    inline string datetime::to_string() const noexcept {
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
