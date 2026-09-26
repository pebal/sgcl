//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/duration.h"
#include "../core/expected.h"
#include "../core/string.h"
#include "../txt/format.h"
#include "date.h"
#include "datetime.h"
#include "error.h"
#include "zone.h"

#include <charconv>
#include <chrono>
#include <cmath>
#include <limits>
#include <cstdint>
#include <string>
#include <string_view>

// The text of the module's values. Two ways to say how: a pattern of '%'
// specifiers, the language of std::format for <chrono> (and of C's
// strftime before it), which t.format("%d.%m.%Y %H:%M") and
// txt::format("{:%H:%M}", t) both speak; and a layout, one of a closed
// set of formats known by name (RFC 3339, the date of HTTP, the date of
// e-mail, ISO 8601), each written and read by its own code with no
// pattern walked. Written from the standard ([time.format]), RFC 3339,
// RFC 9110 5.6.7, RFC 5322 3.3.
namespace sgcl::time {
    namespace detail {
        struct layout_access;
    }

    // A format known by its name: time::rfc3339, time::http and the rest
    // below. One byte; nothing to build, nothing to check where it runs
    class layout {
    public:
        friend constexpr bool operator==(layout, layout) noexcept = default;

    private:
        friend struct detail::layout_access;

        constexpr explicit layout(uint8_t kind) noexcept
        : _kind(kind) {
        }

        uint8_t _kind;
    };

    namespace detail {
        enum : uint8_t { Rfc3339, Rfc3339Nano, Http, Email, Iso8601 };

        struct layout_access {
            static constexpr layout make(uint8_t kind) noexcept {
                return layout(kind);
            }

            static constexpr uint8_t kind(layout l) noexcept {
                return l._kind;
            }
        };
    }

    // RFC 3339 to the second, in the datetime's zone, Z for an offset of
    // zero (Go's time.RFC3339): 2026-09-24T12:41:15+02:00. Read, it takes
    // a fraction of a second too, of any length (the nanoseconds kept),
    // 't' and 'z' in small letters (the RFC allows them), and 23:59:60 only
    // where it is the last second of a day in UTC, a leap second, which
    // is read as the first instant of the next day as timegm reads it
    inline constexpr layout rfc3339 = detail::layout_access::make(detail::Rfc3339);

    // The same with the fraction of a second, its trailing zeros left out
    // and none where it is zero (Go's time.RFC3339Nano):
    // 2026-09-24T12:41:15.122575+02:00; read as rfc3339 is
    inline constexpr layout rfc3339_nano = detail::layout_access::make(detail::Rfc3339Nano);

    // The date of HTTP (RFC 9110 5.6.7), the IMF-fixdate, always in GMT:
    // Thu, 24 Sep 2026 10:41:15 GMT; what Date:, Expires:, Last-Modified:
    // and a cookie's Expires carry. Read, it takes the two obsolete forms
    // as well, as a recipient must: RFC 850's "Thursday, 24-Sep-26
    // 10:41:15 GMT" (a year of two digits is the latest year with those
    // digits that is not more than 50 years ahead of now) and asctime's
    // "Thu Sep 24 10:41:15 2026" (the day padded with a space or a nought).
    // The day of the week is read and not checked against the date, as Go
    // and the recipients RFC 9110 has in mind do; names in any case
    inline constexpr layout http = detail::layout_access::make(detail::Http);

    // The date of e-mail (RFC 5322 3.3), in the datetime's zone:
    // Thu, 24 Sep 2026 12:41:15 +0200. Read, it takes the obsolete forms
    // of the RFC too: no day of the week, no seconds, a year of two digits
    // (1950 to 2049) or three (1900 on), the zone as a name (UT, GMT, EST,
    // EDT, CST, CDT, MST, MDT, PST, PDT; a military letter is -0000, which
    // is UTC), comments in parentheses and folding white space between
    // the parts
    inline constexpr layout email = detail::layout_access::make(detail::Email);

    // ISO 8601: written as rfc3339_nano writes. Read, it takes the broad
    // profile: the three forms of a date (2026-09-24, 2026-W39-4,
    // 2026-267), each basic or extended (20260924, 2026W394, 2026267), a
    // date alone (its midnight), or with 'T' and a time of hours, of hours
    // and minutes or of all three, basic or extended (12, 1241, 124115,
    // 12:41, 12:41:15), a fraction on the last of them with a point or a
    // comma (12:41:15,5; 12.5 is half past twelve), 24:00 for the end of
    // a day, and an offset Z, ±hh, ±hhmm or ±hh:mm, UTC where there is none
    inline constexpr layout iso8601 = detail::layout_access::make(detail::Iso8601);

    namespace detail {
        inline constexpr const char* ShortDays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        inline constexpr const char* LongDays[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
        inline constexpr const char* ShortMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        inline constexpr const char* LongMonths[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

        // What a value hands to a pattern: the fields it has, and which of
        // the three kinds of specifier it can answer
        struct moment {
            bool has_date = false;
            bool has_time = false;
            bool has_zone = false;
            int64_t year = 1970;
            int month = 1;
            int day = 1;
            int weekday = 4;                // ISO: Monday 1
            int year_day = 1;
            int iso_year = 1970;
            int iso_week = 1;
            int64_t second_of_day = 0;
            int64_t fraction = 0;           // of a second, in `digits` digits
            int digits = 0;
            int32_t offset = 0;
            std::string_view abbreviation;
            // A span of time (std::chrono::duration): its days (%j), its
            // count and unit (%Q, %q), and a sign written once, before the
            // first specifier, as [time.format] has it
            bool is_duration = false;
            bool negative = false;
            int64_t days = 0;
            std::string_view count;
            std::string_view unit;
        };

        inline void fill_date(moment& m, const time::date& d) noexcept {
            m.has_date = true;
            m.year = d.year();
            m.month = int(d.month());
            m.day = d.day();
            m.weekday = int(d.weekday());
            m.year_day = d.year_day();
            auto w = d.iso_week();
            m.iso_year = w.year;
            m.iso_week = w.week;
        }

        // Where a specifier is at home: 'd' a date's, 't' a time of day's,
        // 'z' a zone's, 'b' a date and a time's, 'a' anybody's, 0 none (not
        // a specifier); a modifier E or O goes with the specifiers C locale
        // lets it
        constexpr char specifier_kind(char c) noexcept {
            switch (c) {
            case 'a': case 'A': case 'b': case 'B': case 'h': case 'C': case 'd': case 'e': case 'D':
            case 'F': case 'g': case 'G': case 'j': case 'm': case 'U': case 'u': case 'V': case 'w':
            case 'W': case 'x': case 'y': case 'Y':
                return 'd';
            case 'H': case 'I': case 'M': case 'S': case 'p': case 'r': case 'R': case 'T': case 'X':
                return 't';
            case 'z': case 'Z':
                return 'z';
            case 'c':
                return 'b';
            case 'n': case 't': case '%':
                return 'a';
            default:
                return 0;
            }
        }

        constexpr bool modified_ok(char modifier, char c) noexcept {
            if (modifier == 'E') {
                return c == 'c' || c == 'C' || c == 'x' || c == 'X' || c == 'y' || c == 'Y' || c == 'z';
            }
            return c == 'd' || c == 'e' || c == 'H' || c == 'I' || c == 'm' || c == 'M' || c == 'S'
                || c == 'u' || c == 'U' || c == 'V' || c == 'w' || c == 'W' || c == 'y' || c == 'z';
        }

        // Whether every specifier of a pattern is one a value with those
        // fields answers: what txt::format asks where the program is
        // compiled
        constexpr bool pattern_ok(std::string_view p, bool date, bool time, bool zone, bool span = false) noexcept {
            for (size_t i = 0; i < p.size(); ++i) {
                if (p[i] != '%') {
                    continue;
                }
                if (++i == p.size()) {
                    return false;
                }
                char c = p[i];
                if (c == 'E' || c == 'O') {
                    if (++i == p.size() || !modified_ok(c, p[i])) {
                        return false;
                    }
                    c = p[i];
                }
                if (span && (c == 'j' || c == 'Q' || c == 'q') && p[i - 1] == '%') {
                    continue;
                }
                switch (specifier_kind(c)) {
                case 'd':
                    if (!date) {
                        return false;
                    }
                    break;
                case 't':
                    if (!time) {
                        return false;
                    }
                    break;
                case 'z':
                    if (!zone) {
                        return false;
                    }
                    break;
                case 'b':
                    if (!date || !time) {
                        return false;
                    }
                    break;
                case 'a':
                    break;
                default:
                    return false;
                }
            }
            return true;
        }

        inline void put_number(txt::format_sink& out, int64_t v, int width, char pad = '0') noexcept {
            char digits[24];
            int n = 0;
            bool negative = v < 0;
            uint64_t u = negative ? uint64_t(0) - uint64_t(v) : uint64_t(v);
            do {
                digits[n++] = char('0' + u % 10);
                u /= 10;
            } while (u != 0);
            if (negative) {
                out.put('-');
            }
            for (int k = n; k < width; ++k) {
                out.put(pad);
            }
            while (n) {
                out.put(digits[--n]);
            }
        }

        // Two digits; a number past them (the hours of a long span) whole
        inline void put_two(txt::format_sink& out, int64_t v) noexcept {
            if (v < 0 || v > 99) [[unlikely]] {
                put_number(out, v, 2, '0');
                return;
            }
            char two[2] = {char('0' + v / 10), char('0' + v % 10)};
            out.put(two, 2);
        }

        inline void put_seconds(txt::format_sink& out, const moment& m) noexcept {
            put_two(out, m.second_of_day % 60);
            if (m.digits) {
                out.put('.');
                put_number(out, m.fraction, m.digits);
            }
        }

        inline void put_offset(txt::format_sink& out, int32_t offset, bool colon) noexcept {
            out.put(offset < 0 ? '-' : '+');
            int32_t a = offset < 0 ? -offset : offset;
            put_two(out, a / 3600);
            if (colon) {
                out.put(':');
            }
            put_two(out, a / 60 % 60);
        }

        // The weeks of the year counted from its first Sunday (%U) or its
        // first Monday (%W), the days before them week 0
        inline int week_of_year(const moment& m, bool monday) noexcept {
            int wday = monday ? m.weekday - 1 : m.weekday % 7;   // days since the week's first day
            return (m.year_day - 1 + 7 - wday) / 7;
        }

        // A pattern written: every specifier the value answers replaced by
        // its field, anything else as it stands (a writer does not fail:
        // "%Q" of a date is "%Q")
        inline void write_pattern(txt::format_sink& out, std::string_view p, const moment& m) noexcept {
            size_t i = 0;
            bool sign = m.negative;
            while (i < p.size()) {
                size_t from = i;
                while (i < p.size() && p[i] != '%') {
                    ++i;
                }
                if (i > from) {
                    out.put(p.data() + from, i - from);
                }
                if (i == p.size()) {
                    break;
                }
                size_t spec_at = i++;
                if (i == p.size()) {
                    out.put('%');
                    break;
                }
                char c = p[i++];
                if ((c == 'E' || c == 'O') && i < p.size() && modified_ok(c, p[i])) {
                    c = p[i++];
                }
                char kind = specifier_kind(c);
                bool span = m.is_duration && i - spec_at == 2 && (c == 'j' || c == 'Q' || c == 'q');
                bool ok = span || kind == 'a' || (kind == 'd' && m.has_date) || (kind == 't' && m.has_time)
                       || (kind == 'z' && m.has_zone) || (kind == 'b' && m.has_date && m.has_time);
                if (!ok) {
                    out.put(p.data() + spec_at, i - spec_at);
                    continue;
                }
                if (sign) {
                    out.put('-');
                    sign = false;
                }
                if (span) {
                    if (c == 'j') {
                        put_number(out, m.days, 1);
                    } else {
                        out.put(c == 'Q' ? m.count : m.unit);
                    }
                    continue;
                }
                // A name out of its table (a year_month_day of month 13)
                // is written as the specifier stands
                bool names_month = c == 'b' || c == 'h' || c == 'B';
                bool names_day = c == 'a' || c == 'A' || c == 'c';
                if ((names_month && (m.month < 1 || m.month > 12)) || (names_day && (m.weekday < 1 || m.weekday > 7))
                    || (c == 'c' && (m.month < 1 || m.month > 12))) {
                    out.put(p.data() + spec_at, i - spec_at);
                    continue;
                }
                int64_t s = m.second_of_day;
                switch (c) {
                case 'a': out.put(ShortDays[m.weekday - 1]); break;
                case 'A': out.put(LongDays[m.weekday - 1]); break;
                case 'b': case 'h': out.put(ShortMonths[m.month - 1]); break;
                case 'B': out.put(LongMonths[m.month - 1]); break;
                case 'C': {
                    // Floored, two characters at least with the sign
                    // among them (-1 is "-1"), as std::format writes it
                    int64_t c100 = floor_div(m.year, 100);
                    put_number(out, c100, c100 < 0 ? 1 : 2);
                    break;
                }
                case 'd': put_two(out, m.day); break;
                case 'e': put_number(out, m.day, 2, ' '); break;
                case 'D': case 'x':
                    put_two(out, m.month);
                    out.put('/');
                    put_two(out, m.day);
                    out.put('/');
                    put_two(out, (m.year < 0 ? -m.year : m.year) % 100);
                    break;
                case 'F':
                    put_number(out, m.year, 4);
                    out.put('-');
                    put_two(out, m.month);
                    out.put('-');
                    put_two(out, m.day);
                    break;
                case 'g': put_two(out, (m.iso_year < 0 ? -m.iso_year : m.iso_year) % 100); break;
                case 'G': put_number(out, m.iso_year, 4); break;
                case 'j': put_number(out, m.year_day, 3); break;
                case 'm': put_two(out, m.month); break;
                case 'U': put_two(out, week_of_year(m, false)); break;
                case 'W': put_two(out, week_of_year(m, true)); break;
                case 'u': out.put(char('0' + m.weekday)); break;
                case 'w': out.put(char('0' + m.weekday % 7)); break;
                case 'V': put_two(out, m.iso_week); break;
                case 'y': {
                    int64_t y = m.year < 0 ? -m.year : m.year;
                    put_two(out, y % 100);
                    break;
                }
                case 'Y': put_number(out, m.year, 4); break;
                case 'H': put_two(out, s / 3600); break;
                case 'I': put_two(out, (s / 3600 + 11) % 12 + 1); break;
                case 'M': put_two(out, s / 60 % 60); break;
                case 'S': put_seconds(out, m); break;
                case 'p': out.put(s < 12 * 3600 ? "AM" : "PM"); break;
                case 'R':
                    put_two(out, s / 3600);
                    out.put(':');
                    put_two(out, s / 60 % 60);
                    break;
                case 'T':
                    put_two(out, s / 3600);
                    out.put(':');
                    put_two(out, s / 60 % 60);
                    out.put(':');
                    put_seconds(out, m);
                    break;
                case 'X':
                    put_two(out, s / 3600);
                    out.put(':');
                    put_two(out, s / 60 % 60);
                    out.put(':');
                    put_two(out, s % 60);
                    break;
                case 'r':
                    put_two(out, (s / 3600 + 11) % 12 + 1);
                    out.put(':');
                    put_two(out, s / 60 % 60);
                    out.put(':');
                    put_two(out, s % 60);
                    out.put(s < 12 * 3600 ? " AM" : " PM");
                    break;
                case 'c':
                    out.put(ShortDays[m.weekday - 1]);
                    out.put(' ');
                    out.put(ShortMonths[m.month - 1]);
                    out.put(' ');
                    put_number(out, m.day, 2, ' ');
                    out.put(' ');
                    put_two(out, s / 3600);
                    out.put(':');
                    put_two(out, s / 60 % 60);
                    out.put(':');
                    put_two(out, s % 60);
                    out.put(' ');
                    put_number(out, m.year, 4);
                    break;
                case 'z':
                    put_offset(out, m.offset, p[spec_at + 1] == 'E' || p[spec_at + 1] == 'O');
                    break;
                case 'Z': out.put(m.abbreviation); break;
                case 'n': out.put('\n'); break;
                case 't': out.put('\t'); break;
                case '%': out.put('%'); break;
                }
            }
        }

        //----------------------------------------------------------------
        // The layouts, written
        //----------------------------------------------------------------
        inline void put_date_time(txt::format_sink& out, const time::date& d, int64_t s, char between) noexcept {
            put_number(out, d.year(), 4);
            out.put('-');
            put_two(out, int(d.month()));
            out.put('-');
            put_two(out, d.day());
            out.put(between);
            put_two(out, s / 3600);
            out.put(':');
            put_two(out, s / 60 % 60);
            out.put(':');
            put_two(out, s % 60);
        }
    }

    namespace detail {
        // How the module's values are written by a format, for the writers
        // below and for txt::format
        struct layout_writer {
            static void write(txt::format_sink& out, const datetime& t, layout format) noexcept {
                uint8_t kind = detail::layout_access::kind(format);
                if (kind == detail::Http) {
                    _write(out, t.utc(), kind);   // HTTP's dates are in UTC
                } else {
                    _write(out, t, kind);         // by reference: a datetime holds its zone's tracked_ptr
                }
            }

            static void _write(txt::format_sink& out, const datetime& u, uint8_t kind) noexcept {
                using namespace detail;
                // The zone asked once: its offset gives the date, the second of
                // the day and the offset written (three lookups before)
                const int64_t seconds = u._seconds();
                const int32_t offset = zone_access::offset_at(u._zone, seconds);
                const int64_t wall = seconds + offset;
                const int64_t days = floor_div(wall, 86400);
                time::date d = time::date(std::chrono::sys_days(std::chrono::days(days)));
                int64_t s = wall - days * 86400;
                if (kind == Http || kind == Email) {
                    out.put(ShortDays[int(d.weekday()) - 1]);
                    out.put(", ", 2);
                    put_two(out, d.day());
                    out.put(' ');
                    out.put(ShortMonths[int(d.month()) - 1]);
                    out.put(' ');
                    put_number(out, d.year(), 4);
                    out.put(' ');
                    put_two(out, s / 3600);
                    out.put(':');
                    put_two(out, s / 60 % 60);
                    out.put(':');
                    put_two(out, s % 60);
                    if (kind == Http) {
                        out.put(" GMT", 4);
                    } else {
                        out.put(' ');
                        put_offset(out, offset, false);
                    }
                    return;
                }
                put_date_time(out, d, s, 'T');
                int64_t fraction = u._fraction();
                if (kind != Rfc3339 && fraction) {
                    int digits = 9;
                    while (fraction % 10 == 0) {
                        fraction /= 10;
                        --digits;
                    }
                    out.put('.');
                    put_number(out, fraction, digits);
                }
                if (offset == 0) {
                    out.put('Z');
                } else {
                    put_offset(out, offset, true);
                }
            }

            static detail::moment moment_of(const datetime& t) noexcept {
                detail::moment m;
                detail::fill_date(m, t.date());
                m.has_time = true;
                m.has_zone = true;
                // the zone asked once: its state gives the offset, the second of
                // the day and the abbreviation (a fixed offset's and UTC's are
                // their names, which their data holds)
                const auto& z = detail::zone_access::data(t._zone);
                const int64_t seconds = t._seconds();
                auto state = detail::zone_access::state_at(z, seconds);
                const int64_t wall = seconds + state.offset;
                m.second_of_day = wall - detail::floor_div(wall, 86400) * 86400;
                m.fraction = t._fraction();
                m.digits = 9;
                m.offset = state.offset;
                m.abbreviation = std::string_view(state.abbreviation ? *state.abbreviation : z.name);
                return m;
            }

            // The moment of t handed to f (every zone's abbreviation is in its
            // data, a fixed offset's and UTC's their names)
            template<class F>
            static void with_moment(const datetime& t, F&& f) {
                f(moment_of(t));
            }

            static detail::moment moment_of(const time::date& d) noexcept {
                detail::moment m;
                detail::fill_date(m, d);
                return m;
            }
        };

        // A string written by `write` into a sink: once into room on the
        // stack, and again into a string of the size that took when it
        // did not fit
        template<class Write>
        string written(Write&& write) {
            char room[128];
            txt::format_sink first(room, sizeof room);
            write(first);
            if (first.size() <= sizeof room) {
                return string(room, first.size());
            }
            std::string big(first.size(), '\0');
            txt::format_sink second(big.data(), big.size());
            write(second);
            return string(big);
        }
    }

    inline string datetime::format(layout format) const {
        return detail::written([&](txt::format_sink& out) { detail::layout_writer::write(out, *this, format); });
    }

    inline string datetime::format(const string& pattern) const {
        std::string_view p(pattern);
        string out;
        detail::layout_writer::with_moment(*this, [&](const detail::moment& m) {
            out = detail::written([&](txt::format_sink& sink) { detail::write_pattern(sink, p, m); });
        });
        return out;
    }

    inline string date::format(const string& pattern) const {
        std::string_view p(pattern);
        detail::moment m = detail::layout_writer::moment_of(*this);
        return detail::written([&](txt::format_sink& sink) { detail::write_pattern(sink, p, m); });
    }
}

//------------------------------------------------------------------------
// txt::format of the module's values: {} is to_string(); a pattern after
// the colon, as std::format has it for <chrono> ({:%H:%M}, {:>12%F}),
// checked where the program is compiled; the field's width pads the
// whole
//------------------------------------------------------------------------
namespace sgcl::txt {
    template<>
    struct formatter<time::datetime> {
        static constexpr bool takes(char type) noexcept {
            return !type;
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_layout(std::string_view pattern) noexcept {
            return !pattern.data() || time::detail::pattern_ok(pattern, true, true, true);
        }

        static void write(format_sink& out, const time::datetime& t, const format_spec& spec, std::string_view pattern) {
            detail::put_body_in_field(out, spec, [&](format_sink& to) {
                if (!pattern.data()) {
                    time::detail::layout_writer::write(to, t, time::rfc3339_nano);
                    return;
                }
                time::detail::layout_writer::with_moment(t, [&](const time::detail::moment& m) { time::detail::write_pattern(to, pattern, m); });
            });
        }
    };

    template<>
    struct formatter<time::date> {
        static constexpr bool takes(char type) noexcept {
            return !type;
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_layout(std::string_view pattern) noexcept {
            return !pattern.data() || time::detail::pattern_ok(pattern, true, false, false);
        }

        static void write(format_sink& out, const time::date& d, const format_spec& spec, std::string_view pattern) {
            time::detail::moment m = time::detail::layout_writer::moment_of(d);
            detail::put_body_in_field(out, spec, [&](format_sink& to) {
                time::detail::write_pattern(to, pattern.data() ? pattern : std::string_view("%F"), m);
            });
        }
    };

    // A day of the week: {} its English name, as to_string writes it
    // ("Monday"); {:%a} "Mon", {:%A} "Monday", {:%u} 1, {:%w} 1
    template<>
    struct formatter<time::weekday> {
        static constexpr bool takes(char type) noexcept {
            return !type;
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_layout(std::string_view pattern) noexcept {
            if (!pattern.data()) {
                return true;
            }
            for (size_t i = 0; i < pattern.size(); ++i) {
                if (pattern[i] != '%') {
                    continue;
                }
                if (++i == pattern.size()) {
                    return false;
                }
                char c = pattern[i];
                if (c == 'O' && i + 1 < pattern.size() && (pattern[i + 1] == 'u' || pattern[i + 1] == 'w')) {
                    c = pattern[++i];
                }
                if (c != 'a' && c != 'A' && c != 'u' && c != 'w' && c != 'n' && c != 't' && c != '%') {
                    return false;
                }
            }
            return true;
        }

        static void write(format_sink& out, time::weekday d, const format_spec& spec, std::string_view pattern) {
            unsigned n = static_cast<unsigned>(d);
            if (n < 1 || n > 7) {
                string text = time::to_string(d);
                detail::put_padded(out, std::string_view(text), spec);
                return;
            }
            time::detail::moment m;
            m.has_date = true;
            m.weekday = int(n);
            detail::put_body_in_field(out, spec, [&](format_sink& to) {
                time::detail::write_pattern(to, pattern.data() ? pattern : std::string_view("%A"), m);
            });
        }
    };

    // A month: {} its English name, as to_string writes it ("September"),
    // as a weekday's; {:%b} "Sep", {:%B} "September", {:%m} 09
    template<>
    struct formatter<time::month> {
        static constexpr bool takes(char type) noexcept {
            return !type;
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_layout(std::string_view pattern) noexcept {
            if (!pattern.data()) {
                return true;
            }
            for (size_t i = 0; i < pattern.size(); ++i) {
                if (pattern[i] != '%') {
                    continue;
                }
                if (++i == pattern.size()) {
                    return false;
                }
                char c = pattern[i];
                if (c == 'O' && i + 1 < pattern.size() && pattern[i + 1] == 'm') {
                    c = pattern[++i];
                }
                if (c != 'b' && c != 'B' && c != 'h' && c != 'm' && c != 'n' && c != 't' && c != '%') {
                    return false;
                }
            }
            return true;
        }

        static void write(format_sink& out, time::month mo, const format_spec& spec, std::string_view pattern) {
            unsigned n = static_cast<unsigned>(mo);
            if (n < 1 || n > 12) {
                string text = time::to_string(mo);
                detail::put_padded(out, std::string_view(text), spec);
                return;
            }
            time::detail::moment m;
            m.has_date = true;
            m.month = int(n);
            detail::put_body_in_field(out, spec, [&](format_sink& to) {
                time::detail::write_pattern(to, pattern.data() ? pattern : std::string_view("%B"), m);
            });
        }
    };
}

//------------------------------------------------------------------------
// The layouts, read
//------------------------------------------------------------------------
namespace sgcl::time {
    namespace detail {
        // A cursor over the text with the reading every layout needs; each
        // failure is a sentence and the byte it stopped at
        struct text_reader {
            std::string_view s;
            size_t i = 0;
            const char* why = nullptr;
            size_t at = 0;

            bool fail(const char* message, size_t where) noexcept {
                if (!why) {
                    why = message;
                    at = where;
                }
                return false;
            }

            bool fail(const char* message) noexcept {
                return fail(message, i);
            }

            bool more() const noexcept {
                return i < s.size();
            }

            char peek() const noexcept {
                return i < s.size() ? s[i] : '\0';
            }

            bool eat(char c) noexcept {
                if (i < s.size() && s[i] == c) {
                    ++i;
                    return true;
                }
                return false;
            }

            static bool digit(char c) noexcept {
                return c >= '0' && c <= '9';
            }

            static char lower(char c) noexcept {
                return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
            }

            static bool letter(char c) noexcept {
                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            }

            // Exactly n digits; a failure is where they start
            bool number(int n, int& out, const char* message) noexcept {
                int v = 0;
                for (int k = 0; k < n; ++k) {
                    if (i + k >= s.size() || !digit(s[i + k])) {
                        return fail(message, i);
                    }
                    v = v * 10 + (s[i + k] - '0');
                }
                i += n;
                out = v;
                return true;
            }

            // From `least` to `most` digits
            bool number_between(int least, int most, int& out, int& count, const char* message) noexcept {
                int v = 0;
                int n = 0;
                while (n < most && i < s.size() && digit(s[i])) {
                    v = v * 10 + (s[i] - '0');
                    ++i;
                    ++n;
                }
                if (n < least) {
                    return fail(message, i);
                }
                out = v;
                count = n;
                return true;
            }

            // One of the names of a table here, in any case: its index, the
            // cursor past it; or -1, the cursor where it was. The first
            // name of the table the text starts with, so that "SunSep" is
            // Sun and then Sep, as a stream reads it
            int name(const char* const* table, int count, size_t& length) noexcept {
                for (int k = 0; k < count; ++k) {
                    std::string_view n(table[k]);
                    if (s.size() - i < n.size()) {
                        continue;
                    }
                    bool same = true;
                    for (size_t c = 0; c < n.size() && same; ++c) {
                        same = lower(s[i + c]) == lower(n[c]);
                    }
                    if (same) {
                        i += n.size();
                        length = n.size();
                        return k;
                    }
                }
                length = 0;
                return -1;
            }

            bool word(std::string_view w, const char* message) noexcept {
                if (s.size() - i < w.size()) {
                    return fail(message);
                }
                for (size_t c = 0; c < w.size(); ++c) {
                    if (lower(s[i + c]) != lower(w[c])) {
                        return fail(message);
                    }
                }
                i += w.size();
                return true;
            }

            // These characters exactly, as they are
            bool exact(std::string_view w, const char* message) noexcept {
                if (s.substr(i, w.size()) != w) {
                    return fail(message);
                }
                i += w.size();
                return true;
            }

            bool expect(char c, const char* message) noexcept {
                return eat(c) || fail(message);
            }
        };

        // The fields read, made an instant: the date checked (the 30th of
        // February is refused, not carried), the time of day checked, a
        // second of 60 taken only where it is 23:59:60 in UTC, a leap
        // second, and read as the first instant after it (as timegm reads
        // it); the instant within the range of a datetime
        struct read_fields {
            int year = 1970, month = 1, day = 1;
            int hour = 0, minute = 0, second = 0;
            int64_t nanos = 0;
            int32_t offset = 0;
            bool utc = true;            // Z, GMT, UT, +00:00: UTC; any other offset: a fixed zone
        };

        inline expected<datetime, error> instant_of_fields(const read_fields& f, size_t date_at, size_t time_at) {
            if (!time::date::is_valid(f.year, f.month, f.day)) {
                return unexpected(error("a day that the month has expected", date_at));
            }
            if (f.hour > 23 || f.minute > 59 || f.second > 60) {
                return unexpected(error("a time of day from 00:00:00 to 23:59:59 expected", time_at));
            }
            int64_t wall = wall_seconds(time::date(f.year, f.month, f.day), f.hour, f.minute, f.second == 60 ? 59 : f.second);
            int64_t t = wall - f.offset;
            if (f.second == 60) {
                if (floor_div(t, 86400) * 86400 + 86399 != t) {
                    return unexpected(error("a second of 60 is a leap second, the last of a day in UTC", time_at));
                }
                t += 1;
            }
            // Within 1677-09-21T00:12:43.145224192Z to 2262-04-11T23:47:16.854775807Z
            __int128 v = (__int128)t * NanosPerSecond + f.nanos;
            if (v < INT64_MIN || v > INT64_MAX) {
                return unexpected(error("an instant within the years 1677 to 2262 expected", date_at));
            }
            int64_t ns = int64_t(v);
            return expected<datetime, error>(std::in_place, made_in_place(), ns, f.utc ? utc_data() : fixed_zone(f.offset));
        }

        // 1*DIGIT after a point: the nanoseconds, digits past the ninth
        // read and dropped
        inline bool read_fraction(text_reader& r, int64_t& nanos, int most = INT32_MAX) noexcept {
            size_t from = r.i;
            int64_t v = 0;
            int n = 0;
            while (n < most && r.more() && text_reader::digit(r.peek())) {
                if (n < 9) {
                    v = v * 10 + (r.peek() - '0');
                }
                ++n;
                ++r.i;
            }
            if (n == 0) {
                return r.fail("a digit of the fraction of a second expected", from);
            }
            for (int k = n; k < 9; ++k) {
                v *= 10;
            }
            nanos = v;
            return true;
        }

        // RFC 3339 5.6: date-time
        inline expected<datetime, error> parse_rfc3339(std::string_view text) {
            text_reader r{text};
            read_fields f;
            size_t date_at = 0;
            bool ok = r.number(4, f.year, "a year of four digits expected")
                   && r.expect('-', "'-' expected") && r.number(2, f.month, "a month of two digits expected")
                   && r.expect('-', "'-' expected") && r.number(2, f.day, "a day of two digits expected");
            if (ok && !(r.eat('T') || r.eat('t'))) {
                ok = r.fail("'T' between the date and the time expected");
            }
            size_t time_at = r.i;
            ok = ok && r.number(2, f.hour, "an hour of two digits expected")
                    && r.expect(':', "':' expected") && r.number(2, f.minute, "a minute of two digits expected")
                    && r.expect(':', "':' expected") && r.number(2, f.second, "a second of two digits expected");
            if (ok && r.eat('.')) {
                ok = read_fraction(r, f.nanos);
            }
            if (ok) {
                if (r.eat('Z') || r.eat('z')) {
                    f.offset = 0;
                } else if (r.peek() == '+' || r.peek() == '-') {
                    int sign = r.peek() == '-' ? -1 : 1;
                    ++r.i;
                    size_t offset_at = r.i;
                    int h = 0, m = 0;
                    ok = r.number(2, h, "the hours of the offset expected") && r.expect(':', "':' expected")
                      && r.number(2, m, "the minutes of the offset expected");
                    if (ok && (h > 23 || m > 59)) {
                        ok = r.fail("an offset from -23:59 to +23:59 expected", offset_at);
                    }
                    f.offset = sign * (h * 3600 + m * 60);
                    f.utc = f.offset == 0;
                } else {
                    ok = r.fail("'Z' or an offset (+hh:mm, -hh:mm) expected");
                }
            }
            if (ok && r.more()) {
                ok = r.fail("the end of the text expected");
            }
            if (!ok) {
                return unexpected(error(r.why, r.at));
            }
            return instant_of_fields(f, date_at, time_at);
        }

        // hh:mm:ss, the time of day of HTTP
        inline bool read_time_of_day(text_reader& r, read_fields& f) noexcept {
            return r.number(2, f.hour, "an hour of two digits expected") && r.expect(':', "':' expected")
                && r.number(2, f.minute, "a minute of two digits expected") && r.expect(':', "':' expected")
                && r.number(2, f.second, "a second of two digits expected");
        }

        // A year of two digits as RFC 9110 reads one: the latest year
        // with those digits that is not more than 50 years ahead of now
        inline int year_of_two_digits(int yy) {
            int now = datetime::from_unix_nano(now_nanos(), time::zone::utc()).year();   // the year in UTC: no zone to read for it
            int y = now - now % 100 + yy;
            if (y > now + 50) {
                y -= 100;
            }
            return y;
        }

        // RFC 9110 5.6.7: IMF-fixdate, rfc850-date, asctime-date
        inline expected<datetime, error> parse_http(std::string_view text) {
            text_reader r{text};
            read_fields f;
            size_t length = 0;
            size_t date_at = 0, time_at = 0;
            bool ok = true;
            int short_day = r.name(ShortDays, 7, length);
            if (short_day >= 0 && r.peek() != ',' && r.peek() != ' ') {
                r.i = 0;          // a long name, as RFC 850 writes it
                short_day = -1;
            }
            if (short_day >= 0 && r.eat(',')) {
                // IMF-fixdate: Sun, 06 Nov 1994 08:49:37 GMT
                date_at = r.i + 1;
                ok = r.expect(' ', "' ' expected") && r.number(2, f.day, "a day of two digits expected")
                  && r.expect(' ', "' ' expected");
                if (ok) {
                    int m = r.name(ShortMonths, 12, length);
                    ok = m >= 0 || r.fail("a month expected: Jan, Feb … Dec");
                    f.month = m + 1;
                }
                ok = ok && r.expect(' ', "' ' expected") && r.number(4, f.year, "a year of four digits expected")
                        && r.expect(' ', "' ' expected");
                time_at = r.i;
                ok = ok && read_time_of_day(r, f) && r.expect(' ', "' ' expected") && r.exact("GMT", "GMT expected");
            } else if (short_day >= 0 && r.eat(' ')) {
                // asctime-date: Sun Nov  6 08:49:37 1994
                date_at = r.i;
                int m = r.name(ShortMonths, 12, length);
                ok = m >= 0 || r.fail("a month expected: Jan, Feb … Dec");
                f.month = m + 1;
                // The day: two digits, or a space and one (RFC 9110), or
                // one digit alone, which Go takes and so does this
                int digits = 0;
                ok = ok && r.expect(' ', "' ' expected");
                if (ok) {
                    r.eat(' ');
                    ok = r.number_between(1, 2, f.day, digits, "a day expected");
                }
                ok = ok && r.expect(' ', "' ' expected");
                time_at = r.i;
                ok = ok && read_time_of_day(r, f) && r.expect(' ', "' ' expected")
                        && r.number(4, f.year, "a year of four digits expected");
            } else {
                // rfc850-date: Sunday, 06-Nov-94 08:49:37 GMT
                int long_day = r.name(LongDays, 7, length);
                ok = (long_day >= 0 && r.eat(',')) || r.fail("a date of HTTP expected: \"Sun, 06 Nov 1994 08:49:37 GMT\"", 0);
                date_at = r.i + 1;
                int yy = 0;
                ok = ok && r.expect(' ', "' ' expected") && r.number(2, f.day, "a day of two digits expected")
                        && r.expect('-', "'-' expected");
                if (ok) {
                    int m = r.name(ShortMonths, 12, length);
                    ok = m >= 0 || r.fail("a month expected: Jan, Feb … Dec");
                    f.month = m + 1;
                }
                ok = ok && r.expect('-', "'-' expected") && r.number(2, yy, "a year of two digits expected")
                        && r.expect(' ', "' ' expected");
                time_at = r.i;
                ok = ok && read_time_of_day(r, f) && r.expect(' ', "' ' expected") && r.exact("GMT", "GMT expected");
                if (ok) {
                    f.year = year_of_two_digits(yy);
                }
            }
            if (ok && r.more()) {
                ok = r.fail("the end of the date expected");
            }
            if (!ok) {
                return unexpected(error(r.why, r.at));
            }
            return instant_of_fields(f, date_at, time_at);
        }

        // RFC 5322 3.3, with the obsolete forms of 4.3. CFWS: white space,
        // folding (CR LF before white space) and comments, which nest and
        // may quote a character with a backslash
        inline bool skip_cfws(text_reader& r) noexcept {
            for (;;) {
                char c = r.peek();
                if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                    ++r.i;
                    continue;
                }
                if (c != '(') {
                    return true;
                }
                size_t from = r.i;
                int depth = 0;
                do {
                    char d = r.peek();
                    if (!r.more()) {
                        return r.fail("a comment that does not end", from);
                    }
                    if (d == '\\') {
                        r.i += 2;
                        continue;
                    }
                    depth += d == '(' ? 1 : d == ')' ? -1 : 0;
                    ++r.i;
                } while (depth > 0);
                if (r.i > r.s.size()) {
                    return r.fail("a comment that does not end", from);
                }
            }
        }

        inline expected<datetime, error> parse_email(std::string_view text) {
            static constexpr const char* Zones[] = {"UT", "GMT", "EST", "EDT", "CST", "CDT", "MST", "MDT", "PST", "PDT"};
            static constexpr int ZoneHours[] = {0, 0, -5, -4, -6, -5, -7, -6, -8, -7};
            text_reader r{text};
            read_fields f;
            size_t length = 0;
            bool ok = skip_cfws(r);
            if (ok && text_reader::letter(r.peek())) {
                int day = r.name(ShortDays, 7, length);
                ok = day >= 0 || r.fail("a day of the week expected: Mon, Tue … Sun");
                ok = ok && skip_cfws(r) && r.expect(',', "',' after the day of the week expected");
            }
            ok = ok && skip_cfws(r);
            size_t date_at = r.i;
            int n = 0;
            ok = ok && r.number_between(1, 2, f.day, n, "a day of one or two digits expected") && skip_cfws(r);
            if (ok) {
                int m = r.name(ShortMonths, 12, length);
                ok = m >= 0 || r.fail("a month expected: Jan, Feb … Dec");
                f.month = m + 1;
            }
            ok = ok && skip_cfws(r);
            if (ok) {
                size_t year_at = r.i;
                int digits = 0;
                ok = r.number_between(2, 9, f.year, digits, "a year expected");
                if (ok && digits == 2) {
                    f.year += f.year < 50 ? 2000 : 1900;   // 4.3: obs-year
                } else if (ok && digits == 3) {
                    f.year += 1900;
                } else if (ok && digits > 4) {
                    ok = r.fail("a year of four digits expected", year_at);
                }
            }
            ok = ok && skip_cfws(r);
            size_t time_at = r.i;
            ok = ok && r.number(2, f.hour, "an hour of two digits expected") && skip_cfws(r) && r.expect(':', "':' expected")
                    && skip_cfws(r) && r.number(2, f.minute, "a minute of two digits expected") && skip_cfws(r);
            if (ok && r.eat(':')) {
                ok = skip_cfws(r) && r.number(2, f.second, "a second of two digits expected") && skip_cfws(r);
            }
            if (ok) {
                if (r.peek() == '+' || r.peek() == '-') {
                    int sign = r.peek() == '-' ? -1 : 1;
                    size_t offset_at = ++r.i;
                    int hhmm = 0;
                    ok = r.number(4, hhmm, "an offset of four digits expected: +hhmm");
                    if (ok && (hhmm / 100 > 23 || hhmm % 100 > 59)) {
                        ok = r.fail("an offset from -2359 to +2359 expected", offset_at);
                    }
                    f.offset = sign * (hhmm / 100 * 3600 + hhmm % 100 * 60);
                    f.utc = f.offset == 0;
                } else if (text_reader::letter(r.peek())) {
                    size_t zone_at = r.i;
                    size_t run = 0;
                    while (zone_at + run < r.s.size() && text_reader::letter(r.s[zone_at + run])) {
                        ++run;
                    }
                    int z = r.name(Zones, 10, length);
                    if (z >= 0 && length == run) {
                        f.offset = ZoneHours[z] * 3600;
                        f.utc = f.offset == 0;
                    } else if (run == 1 && r.peek() != 'J' && r.peek() != 'j') {
                        r.i = zone_at + 1;   // a military zone: -0000, UTC with nothing known (4.3)
                    } else {
                        r.i = zone_at;
                        ok = r.fail("a zone expected: +hhmm, -hhmm, UT, GMT, EST … PDT", zone_at);
                    }
                } else {
                    ok = r.fail("a zone expected: +hhmm, -hhmm, UT, GMT, EST … PDT");
                }
            }
            ok = ok && skip_cfws(r);
            if (ok && r.more()) {
                ok = r.fail("the end of the date expected");
            }
            if (!ok) {
                return unexpected(error(r.why, r.at));
            }
            return instant_of_fields(f, date_at, time_at);
        }
        //----------------------------------------------------------------
        // ISO 8601
        //----------------------------------------------------------------
        inline expected<datetime, error> parse_iso8601(std::string_view text) {
            // The date up to 'T', read by date::parse, which knows its three
            // forms
            size_t t_at = text.find_first_of("Tt");
            std::string_view date_text = text.substr(0, t_at);
            auto d = time::date::parse(string(date_text));
            if (!d) {
                return unexpected(d.error());
            }
            read_fields f;
            f.year = d->year();
            f.month = int(d->month());
            f.day = d->day();
            if (t_at == std::string_view::npos) {
                return instant_of_fields(f, 0, 0);
            }
            text_reader r{text};
            r.i = t_at + 1;
            size_t time_at = r.i;
            // hh, then mm and ss, each with a colon or none; a fraction on
            // the last one read, which carries into the ones below it
            int parts[3] = {0, 0, 0};
            int read = 0;
            bool extended = false;
            bool ok = r.number(2, parts[0], "an hour of two digits expected");
            read = ok ? 1 : 0;
            while (ok && read < 3) {
                size_t at = r.i;
                if (r.eat(':')) {
                    if (read == 1) {
                        extended = true;
                    } else if (!extended) {
                        ok = r.fail("a time written one way: 12:41:15 or 124115", at);
                        break;
                    }
                } else if (extended || !text_reader::digit(r.peek())) {
                    break;
                }
                ok = r.number(2, parts[read], read == 1 ? "a minute of two digits expected" : "a second of two digits expected");
                read += ok ? 1 : 0;
            }
            int64_t fraction_ns = 0;   // of the last part read, in nanoseconds of it
            if (ok && (r.peek() == '.' || r.peek() == ',')) {
                ++r.i;
                ok = read_fraction(r, fraction_ns);
            }
            if (ok) {
                if (r.eat('Z') || r.eat('z')) {
                    f.offset = 0;
                } else if (r.peek() == '+' || r.peek() == '-') {
                    int sign = r.peek() == '-' ? -1 : 1;
                    size_t offset_at = ++r.i;
                    int h = 0, m = 0;
                    ok = r.number(2, h, "the hours of the offset expected");
                    if (ok && r.eat(':')) {
                        ok = r.number(2, m, "the minutes of the offset expected");
                    } else if (ok && text_reader::digit(r.peek())) {
                        ok = r.number(2, m, "the minutes of the offset expected");
                    }
                    if (ok && (h > 23 || m > 59)) {
                        ok = r.fail("an offset from -23:59 to +23:59 expected", offset_at);
                    }
                    f.offset = sign * (h * 3600 + m * 60);
                    f.utc = f.offset == 0;
                }
            }
            if (ok && r.more()) {
                ok = r.fail("the end of the time expected");
            }
            if (!ok) {
                return unexpected(error(r.why, r.at));
            }
            f.hour = parts[0];
            f.minute = parts[1];
            f.second = parts[2];
            // 24:00 is the end of the day: the next one's midnight
            bool end_of_day = f.hour == 24 && f.minute == 0 && f.second == 0 && fraction_ns == 0;
            if (end_of_day) {
                f.hour = 0;
            }
            if (f.hour > 23 || f.minute > 59 || f.second > 60) {
                return unexpected(error("a time of day from 00:00:00 to 23:59:59 expected", time_at));
            }
            // The fraction of an hour or a minute, in whole nanoseconds of
            // the seconds below it
            int64_t unit = read == 1 ? 3600 : read == 2 ? 60 : 1;
            __int128 extra = (__int128)fraction_ns * unit;   // nanoseconds
            f.nanos = int64_t(extra % NanosPerSecond);
            int64_t carried = int64_t(extra / NanosPerSecond);
            auto made = instant_of_fields(f, 0, time_at);
            if (!made) {
                return made;
            }
            int64_t shift = (end_of_day ? 86400 : 0) + carried;
            if (!shift) {
                return made;
            }
            __int128 ns = (__int128)made->unix_nano() + (__int128)shift * NanosPerSecond;
            if (ns < INT64_MIN || ns > INT64_MAX) {
                return unexpected(error("an instant within the years 1677 to 2262 expected", 0));
            }
            return datetime::from_unix_nano(int64_t(ns), made->zone());
        }

        //----------------------------------------------------------------
        // Patterns of '%', read (std::chrono::parse, [time.parse])
        //----------------------------------------------------------------
        struct pattern_fields {
            optional<int64_t> year;
            optional<int> century, short_year, month, day, year_day;
            optional<int> hour, hour12, minute, second;
            int64_t nanos = 0;
            optional<bool> pm;
            optional<int> weekday;                   // ISO: Monday 1
            optional<int> iso_year, iso_short_year, iso_week, week_sunday, week_monday;
            optional<int32_t> offset;
            std::string_view abbreviation;
            bool has_abbreviation = false;
            size_t date_at = size_t(-1);
            size_t time_at = size_t(-1);
        };

        // A number of at most `width` digits (at least one), a sign first
        // where `sign`; nothing else skipped
        inline bool read_number(text_reader& r, int width, bool sign, int64_t& out, const char* message) noexcept {
            size_t from = r.i;
            bool negative = false;
            if (sign && (r.peek() == '-' || r.peek() == '+')) {
                negative = r.peek() == '-';
                ++r.i;
            }
            int64_t v = 0;
            int n = 0;
            while (n < width && r.more() && text_reader::digit(r.peek())) {
                // A number too long for 64 bits stays the largest, which no
                // field takes
                v = v < 100000000000000000 ? v * 10 + (r.peek() - '0') : INT64_MAX;
                ++r.i;
                ++n;
            }
            if (n == 0) {
                r.i = from;
                return r.fail(message, from);
            }
            out = negative ? -v : v;
            return true;
        }

        // A field the pattern names twice must be read the same both times
        // (%m and %b, %a and %u, a second %Y): a text that says two things
        // is refused, as std::chrono::parse refuses it
        template<class T>
        bool set_once(text_reader& r, optional<T>& slot, T v, size_t at) noexcept {
            if (slot && *slot != v) {
                return r.fail("a field read twice with two values", at);
            }
            slot = v;
            return true;
        }

        template<class T>
        bool read_field(text_reader& r, int width, int least, int most, optional<T>& out, const char* message, bool sign = false) noexcept {
            size_t from = r.i;
            int64_t v = 0;
            if (!read_number(r, width, sign, v, message)) {
                return false;
            }
            if (v < least || v > most) {
                return r.fail(message, from);
            }
            return set_once(r, out, T(v), from);
        }

        inline bool is_space(char c) noexcept {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
        }

        inline bool read_pattern(text_reader& r, std::string_view p, pattern_fields& f, bool date_only) noexcept {
            size_t i = 0;
            while (i < p.size()) {
                char c = p[i];
                if (c != '%') {
                    if (is_space(c)) {
                        // White space matches none or more of it
                        while (r.more() && is_space(r.peek())) {
                            ++r.i;
                        }
                    } else if (!r.eat(c)) {
                        return r.fail("the text does not follow the pattern");
                    }
                    ++i;
                    continue;
                }
                size_t spec_at = i++;
                int width = -1;
                while (i < p.size() && text_reader::digit(p[i])) {
                    width = (width < 0 ? 0 : width * 10) + (p[i++] - '0');
                    if (width > 100) {
                        width = 100;
                    }
                }
                if (i < p.size() && (p[i] == 'E' || p[i] == 'O')) {
                    ++i;
                }
                if (i == p.size()) {
                    return r.fail("a pattern that ends with an unfinished specifier", r.i);
                }
                char s = p[i++];
                auto w = [&](int by_default) { return width > 0 ? width : by_default; };
                char kind = specifier_kind(s);
                if (date_only && (kind == 't' || kind == 'z' || kind == 'b')) {
                    return r.fail("a specifier of a time of day or of a zone in the pattern of a date", r.i);
                }
                bool date_spec = kind == 'd' || kind == 'b';
                if (date_spec && f.date_at == size_t(-1)) {
                    f.date_at = r.i;
                }
                if (kind == 't' && f.time_at == size_t(-1)) {
                    f.time_at = r.i;
                }
                size_t length = 0;
                switch (s) {
                case 'a': case 'A': {
                    int k = r.name(LongDays, 7, length);
                    if (k < 0) {
                        k = r.name(ShortDays, 7, length);
                    }
                    if (k < 0) {
                        return r.fail("a day of the week expected: Mon … Sun or Monday … Sunday");
                    }
                    if (!set_once(r, f.weekday, k + 1, r.i - length)) {
                        return false;
                    }
                    break;
                }
                case 'b': case 'B': case 'h': {
                    int k = r.name(LongMonths, 12, length);
                    if (k < 0) {
                        k = r.name(ShortMonths, 12, length);
                    }
                    if (k < 0) {
                        return r.fail("a month expected: Jan … Dec or January … December");
                    }
                    if (!set_once(r, f.month, k + 1, r.i - length)) {
                        return false;
                    }
                    break;
                }
                case 'C':
                    if (!read_field(r, w(2), -99, 99, f.century, "a century of up to two digits expected", true)) {
                        return false;
                    }
                    break;
                case 'd': case 'e':
                    if (s == 'e' && r.peek() == ' ') {
                        ++r.i;
                    }
                    if (!read_field(r, w(2), 1, 31, f.day, "a day of the month from 1 to 31 expected")) {
                        return false;
                    }
                    break;
                case 'D': case 'x':
                    if (!read_pattern(r, "%m/%d/%y", f, date_only)) {
                        return false;
                    }
                    break;
                case 'F': {
                    // The width is the year's
                    char year[8] = "%4Y";
                    if (width > 0 && width < 10) {
                        year[1] = char('0' + width);
                    }
                    if (!read_pattern(r, year, f, date_only) || !read_pattern(r, "-%m-%d", f, date_only)) {
                        return false;
                    }
                    break;
                }
                case 'g':
                    if (!read_field(r, w(2), 0, 99, f.iso_short_year, "a year of the week of up to two digits expected")) {
                        return false;
                    }
                    break;
                case 'G': {
                    optional<int64_t> y;
                    size_t at = r.i;
                    if (!read_field(r, w(4), -32767, 32767, y, "a year of the week expected", true) || !set_once(r, f.iso_year, int(*y), at)) {
                        return false;
                    }
                    break;
                }
                case 'j':
                    if (!read_field(r, w(3), 1, 366, f.year_day, "a day of the year from 1 to 366 expected")) {
                        return false;
                    }
                    break;
                case 'm':
                    if (!read_field(r, w(2), 1, 12, f.month, "a month from 1 to 12 expected")) {
                        return false;
                    }
                    break;
                case 'U':
                    if (!read_field(r, w(2), 0, 53, f.week_sunday, "a week from 0 to 53 expected")) {
                        return false;
                    }
                    break;
                case 'W':
                    if (!read_field(r, w(2), 0, 53, f.week_monday, "a week from 0 to 53 expected")) {
                        return false;
                    }
                    break;
                case 'V':
                    if (!read_field(r, w(2), 1, 53, f.iso_week, "a week from 1 to 53 expected")) {
                        return false;
                    }
                    break;
                case 'u': {
                    optional<int> v;
                    size_t at = r.i;
                    if (!read_field(r, w(1), 1, 7, v, "a day of the week from 1 to 7 expected") || !set_once(r, f.weekday, *v, at)) {
                        return false;
                    }
                    break;
                }
                case 'w': {
                    optional<int> v;
                    size_t at = r.i;
                    if (!read_field(r, w(1), 0, 6, v, "a day of the week from 0 to 6 expected") || !set_once(r, f.weekday, *v == 0 ? 7 : *v, at)) {
                        return false;
                    }
                    break;
                }
                case 'y':
                    if (!read_field(r, w(2), 0, 99, f.short_year, "a year of up to two digits expected")) {
                        return false;
                    }
                    break;
                case 'Y':
                    if (!read_field(r, w(4), -32767, 32767, f.year, "a year expected", true)) {
                        return false;
                    }
                    break;
                case 'H':
                    if (!read_field(r, w(2), 0, 23, f.hour, "an hour from 0 to 23 expected")) {
                        return false;
                    }
                    break;
                case 'I':
                    if (!read_field(r, w(2), 1, 12, f.hour12, "an hour from 1 to 12 expected")) {
                        return false;
                    }
                    break;
                case 'M':
                    if (!read_field(r, w(2), 0, 59, f.minute, "a minute from 0 to 59 expected")) {
                        return false;
                    }
                    break;
                case 'S': {
                    if (!read_field(r, w(2), 0, 60, f.second, "a second from 0 to 60 expected")) {
                        return false;
                    }
                    // A fraction, if the text has one
                    // A fraction, if the text has one: nine digits at most,
                    // the nanoseconds a datetime holds, as std::chrono::parse
                    // reads as many as the precision it parses into
                    if ((r.peek() == '.' || r.peek() == ',') && r.i + 1 < r.s.size() && text_reader::digit(r.s[r.i + 1])) {
                        ++r.i;
                        if (!read_fraction(r, f.nanos, 9)) {
                            return false;
                        }
                    }
                    break;
                }
                case 'p': {
                    size_t at = r.i;
                    bool pm = false;
                    if (!r.word("AM", "AM or PM expected")) {
                        r.why = nullptr;
                        if (!r.word("PM", "AM or PM expected")) {
                            return false;
                        }
                        pm = true;
                    }
                    if (!set_once(r, f.pm, pm, at)) {
                        return false;
                    }
                    break;
                }
                case 'R':
                    if (!read_pattern(r, "%H:%M", f, date_only)) {
                        return false;
                    }
                    break;
                case 'T': case 'X':
                    if (!read_pattern(r, "%H:%M:%S", f, date_only)) {
                        return false;
                    }
                    break;
                case 'r':
                    if (!read_pattern(r, "%I:%M:%S %p", f, date_only)) {
                        return false;
                    }
                    break;
                case 'c':
                    if (!read_pattern(r, "%a %b %d %H:%M:%S %Y", f, date_only)) {
                        return false;
                    }
                    break;
                case 'z': {
                    // [+|-]hh[mm]; with E or O, [+|-]h[h][:mm]
                    bool colon = p[spec_at + 1] == 'E' || p[spec_at + 1] == 'O';
                    // [time.parse]: [+|-]hh[mm], the sign optional
                    size_t at = r.i;
                    int sign = r.peek() == '-' ? -1 : 1;
                    if (r.peek() == '+' || r.peek() == '-') {
                        ++r.i;
                    } else if (!text_reader::digit(r.peek())) {
                        return r.fail("an offset expected: +hh, +hhmm or -hhmm");
                    }
                    int h = 0, m = 0, n = 0;
                    if (colon) {
                        if (!r.number_between(1, 2, h, n, "the hours of the offset expected")) {
                            return false;
                        }
                        if (r.eat(':') && !r.number(2, m, "the minutes of the offset expected")) {
                            return false;
                        }
                    } else {
                        if (!r.number(2, h, "the hours of the offset expected")) {
                            return false;
                        }
                        if (r.more() && text_reader::digit(r.peek()) && !r.number(2, m, "the minutes of the offset expected")) {
                            return false;
                        }
                    }
                    if (h > 23 || m > 59) {
                        return r.fail("an offset from -23:59 to +23:59 expected", at);
                    }
                    if (!set_once(r, f.offset, int32_t(sign * (h * 3600 + m * 60)), at)) {
                        return false;
                    }
                    break;
                }
                case 'Z': {
                    size_t from = r.i;
                    while (r.more() && (text_reader::letter(r.peek()) || text_reader::digit(r.peek()) || r.peek() == '+' || r.peek() == '-' || r.peek() == '_' || r.peek() == '/')) {
                        ++r.i;
                    }
                    if (r.i == from) {
                        return r.fail("an abbreviation of a zone expected");
                    }
                    f.abbreviation = r.s.substr(from, r.i - from);
                    f.has_abbreviation = true;
                    break;
                }
                case 'n':
                    if (!r.more() || !is_space(r.peek())) {
                        return r.fail("a white space expected");
                    }
                    ++r.i;
                    break;
                case 't':
                    if (r.more() && is_space(r.peek())) {
                        ++r.i;
                    }
                    break;
                case '%':
                    if (!r.eat('%')) {
                        return r.fail("'%' expected");
                    }
                    break;
                default:
                    return r.fail("a specifier this reader does not know", r.i);
                }
            }
            return true;
        }

        // The date the fields name, with every field read agreeing
        inline expected<time::date, error> date_of_fields(const pattern_fields& f, size_t at) {
            optional<int64_t> year = f.year;
            if (f.year && f.short_year && (*f.year < 0 ? -*f.year : *f.year) % 100 != *f.short_year) {
                return unexpected(error("a year of two digits that is not the year's", at));
            }
            if (f.year && f.century && floor_div(*f.year, 100) != *f.century) {
                return unexpected(error("a century that is not the year's", at));
            }
            if (!year && f.short_year) {
                // POSIX: 69 to 99 in the 1900s, 00 to 68 in the 2000s; a
                // century read makes the two digits its
                year = f.century ? int64_t(*f.century) * 100 + *f.short_year : int64_t(*f.short_year < 69 ? 2000 + *f.short_year : 1900 + *f.short_year);
            } else if (!year && f.century) {
                year = int64_t(*f.century) * 100;
            }
            optional<time::date> d;
            if (year && f.month && f.day) {
                if (!time::date::is_valid(int(*year), *f.month, *f.day)) {
                    return unexpected(error("a day that the month has expected", at));
                }
                d = time::date(int(*year), *f.month, *f.day);
            } else if (year && f.year_day) {
                bool leap = time::date(int(*year), 1, 1).is_leap_year();
                if (*f.year_day > (leap ? 366 : 365)) {
                    return unexpected(error("a day that the year has expected", at));
                }
                d = time::date(int(*year), 1, *f.year_day);
            } else if (year && f.weekday && (f.week_sunday || f.week_monday)) {
                // Week 1 starts on the year's first Sunday (%U) or Monday
                // (%W); the days before it are week 0
                time::date jan1(int(*year), 1, 1);
                int jan1_weekday = int(jan1.weekday());   // Monday 1
                int first = f.week_sunday ? (7 - jan1_weekday % 7) % 7 : (8 - jan1_weekday) % 7;   // days to the first such day
                int week = f.week_sunday ? *f.week_sunday : *f.week_monday;
                int offset_in_week = f.week_sunday ? *f.weekday % 7 : *f.weekday - 1;
                d = jan1.add_days(first + (week - 1) * 7 + offset_in_week);
                if (d->year() != *year) {
                    return unexpected(error("a week and a day that the year has expected", at));
                }
            } else if ((f.iso_year || f.iso_short_year) && f.iso_week && f.weekday) {
                int iy = f.iso_year ? *f.iso_year : (*f.iso_short_year < 69 ? 2000 : 1900) + *f.iso_short_year;
                if (*f.iso_week > iso_weeks_in(iy)) {
                    return unexpected(error("a week that the year of weeks has expected", at));
                }
                time::date jan4(iy, 1, 4);
                d = jan4.add_days(-(int(jan4.weekday()) - 1) + (*f.iso_week - 1) * 7 + (*f.weekday - 1));
            } else {
                return unexpected(error("a date expected: the pattern names no year, month and day, no day of a year and no week", at));
            }
            if (f.weekday && int(d->weekday()) != *f.weekday) {
                return unexpected(error("a day of the week that is not the date's", at));
            }
            if (f.month && int(d->month()) != *f.month) {
                return unexpected(error("a month that is not the date's", at));
            }
            if (f.year_day && d->year_day() != *f.year_day) {
                return unexpected(error("a day of the year that is not the date's", at));
            }
            return *d;
        }
    }

    inline expected<datetime, error> datetime::parse(const string& text, layout format) {
        std::string_view s(text);
        switch (detail::layout_access::kind(format)) {
        case detail::Rfc3339:
        case detail::Rfc3339Nano:
            return detail::parse_rfc3339(s);
        case detail::Http:
            return detail::parse_http(s);
        case detail::Email:
            return detail::parse_email(s);
        default:
            return detail::parse_iso8601(s);
        }
    }
}

//------------------------------------------------------------------------
// txt::format of the types of <chrono>, written as std::format writes
// them: sys_time and local_time of an integral duration, year_month_day,
// weekday, hh_mm_ss and duration. Code written with <chrono> passes its
// values to txt::format as they are
//------------------------------------------------------------------------
namespace sgcl::time::detail {
    // The digits of a fraction of a second a unit asks for, as
    // [time.hms.members] counts them: n where 10^n of the unit is a whole
    // number of seconds' fractions, 6 where none up to 18 is
    template<class Period>
    constexpr int fraction_digits() noexcept {
        if constexpr (Period::den == 1) {
            return 0;
        } else {
            unsigned long long p = 1;
            for (int n = 0; n <= 18; ++n) {
                if ((p * (unsigned long long)Period::num) % (unsigned long long)Period::den == 0) {
                    return n;
                }
                p *= 10;
            }
            return 6;
        }
    }

    constexpr int64_t power_of_ten(int n) noexcept {
        int64_t p = 1;
        while (n-- > 0) {
            p *= 10;
        }
        return p;
    }

    // An instant of <chrono> of an integral duration since 1970, into the
    // fields of a pattern
    template<class Rep, class Period>
    moment moment_of_since(std::chrono::duration<Rep, Period> d, bool date, bool clock) noexcept {
        using namespace std::chrono;
        moment m;
        auto whole = floor<seconds>(d);
        int64_t secs = int64_t(whole.count());
        int64_t days = floor_div(secs, 86400);
        if (date) {
            fill_date(m, time::date(sys_days(std::chrono::days(days))));
        }
        m.has_time = clock;
        m.second_of_day = secs - days * 86400;
        constexpr int digits = fraction_digits<Period>();
        m.digits = digits;
        if constexpr (digits > 0) {
            using Fraction = std::chrono::duration<int64_t, std::ratio<1, power_of_ten(digits)>>;
            m.fraction = duration_cast<Fraction>(d - whole).count();
        }
        return m;
    }

    // The unit of a duration as std::format's %q writes it
    template<class Period>
    std::string unit_of() {
        using namespace std;
        if constexpr (is_same_v<Period, atto>) return "as";
        else if constexpr (is_same_v<Period, femto>) return "fs";
        else if constexpr (is_same_v<Period, pico>) return "ps";
        else if constexpr (is_same_v<Period, nano>) return "ns";
        else if constexpr (is_same_v<Period, micro>) return "µs";
        else if constexpr (is_same_v<Period, milli>) return "ms";
        else if constexpr (is_same_v<Period, centi>) return "cs";
        else if constexpr (is_same_v<Period, deci>) return "ds";
        else if constexpr (is_same_v<Period, ratio<1>>) return "s";
        else if constexpr (is_same_v<Period, deca>) return "das";
        else if constexpr (is_same_v<Period, hecto>) return "hs";
        else if constexpr (is_same_v<Period, kilo>) return "ks";
        else if constexpr (is_same_v<Period, mega>) return "Ms";
        else if constexpr (is_same_v<Period, giga>) return "Gs";
        else if constexpr (is_same_v<Period, tera>) return "Ts";
        else if constexpr (is_same_v<Period, peta>) return "Ps";
        else if constexpr (is_same_v<Period, exa>) return "Es";
        else if constexpr (is_same_v<Period, ratio<60>>) return "min";
        else if constexpr (is_same_v<Period, ratio<3600>>) return "h";
        else if constexpr (is_same_v<Period, ratio<86400>>) return "d";
        else if constexpr (Period::den == 1) return "[" + std::to_string(Period::num) + "]s";
        else return "[" + std::to_string(Period::num) + "/" + std::to_string(Period::den) + "]s";
    }

    // The count of a duration: as %Q writes it (the count's own {}: the
    // shortest decimal that reads back), or as the duration's {} writes
    // it, which is as a stream does (six significant digits, %g:
    // 2.46898e+08)
    template<class Rep>
    std::string count_of(Rep r, bool streamed = false) {
        char text[64];
        if constexpr (std::is_floating_point_v<Rep>) {
            if (!streamed) {
                auto res = std::to_chars(text, text + sizeof text, r);
                return std::string(text, res.ptr);
            }
            auto res = std::to_chars(text, text + sizeof text, r, std::chars_format::general, 6);
            return std::string(text, res.ptr);
        } else {
            auto res = std::to_chars(text, text + sizeof text, r);
            return std::string(text, res.ptr);
        }
    }

    template<class Rep, class Period>
    moment moment_of_duration(std::chrono::duration<Rep, Period> d, std::string& count, std::string& unit) {
        using namespace std::chrono;
        moment m;
        m.is_duration = true;
        m.negative = d < std::chrono::duration<Rep, Period>::zero();
        unit = unit_of<Period>();
        m.unit = unit;
        // The count's magnitude: the smallest integral one has none of its
        // own type, so it is written from the unsigned one
        if constexpr (std::is_integral_v<Rep> && std::is_signed_v<Rep>) {
            if (d.count() == std::numeric_limits<Rep>::min()) {
                count = std::to_string(uint64_t(0) - uint64_t(int64_t(d.count())));
                m.count = count;
                return m;   // no time of day: written as the specifiers stand
            }
        }
        auto a = m.negative ? -d : d;
        count = count_of(a.count());
        m.count = count;
        // The time of day of the span, where it has one a day and seconds
        // of 64 bits can hold: a NaN, an infinity, or 10^300 seconds has
        // none, and its specifiers are written as they stand
        long double total = duration_cast<std::chrono::duration<long double>>(a).count();
        if (!(total < 9.0e15L)) {
            return m;
        }
        m.has_time = true;
        if constexpr (std::is_floating_point_v<Rep>) {
            auto whole = floor<seconds>(a);
            int64_t secs = int64_t(whole.count());
            constexpr int digits = fraction_digits<Period>();
            m.digits = digits;
            if constexpr (digits > 0) {
                // The part of a second in the unit's digits, rounded to the
                // nearest as std writes it ("%.0f"); a rounding up to a
                // whole second carries into it
                using Fraction = std::chrono::duration<double, std::ratio<1, power_of_ten(digits)>>;
                int64_t f = int64_t(std::nearbyint(duration_cast<Fraction>(a - whole).count()));
                if (f >= power_of_ten(digits)) {
                    f -= power_of_ten(digits);
                    secs += 1;
                }
                m.fraction = f;
            }
            m.days = secs / 86400;
            m.second_of_day = secs - m.days * 86400;
        } else {
            m.days = int64_t(floor<std::chrono::days>(a).count());
            moment clock = moment_of_since(a - floor<std::chrono::days>(a), false, true);
            m.second_of_day = clock.second_of_day;
            m.digits = clock.digits;
            m.fraction = clock.fraction;
        }
        return m;
    }
}

namespace sgcl::txt {
    namespace detail {
        // The field of a value of <chrono>: its pattern written, or its
        // default one
        inline void write_chrono(format_sink& out, const format_spec& spec, std::string_view pattern,
                                 std::string_view fallback, const time::detail::moment& m, std::string_view tail = {}) {
            put_body_in_field(out, spec, [&](format_sink& to) {
                time::detail::write_pattern(to, pattern.data() ? pattern : fallback, m);
                if (!pattern.data() && tail.size()) {
                    to.put(tail);
                }
            });
        }
    }

    // sys_time of an integral duration: {} is "%F %T" ("%F" for a duration
    // of a day or more), %Z "UTC", %z "+0000"
    template<class Rep, class Period>
    requires std::is_integral_v<Rep>
    struct formatter<std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<Rep, Period>>> {
        static constexpr bool takes(char type) noexcept {
            return !type;
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_layout(std::string_view pattern) noexcept {
            return !pattern.data() || time::detail::pattern_ok(pattern, true, true, true);
        }

        static void write(format_sink& out, const std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<Rep, Period>>& t,
                          const format_spec& spec, std::string_view pattern) {
            time::detail::moment m = time::detail::moment_of_since(t.time_since_epoch(), true, true);
            m.has_zone = true;
            m.abbreviation = "UTC";
            detail::write_chrono(out, spec, pattern, std::ratio_greater_equal_v<Period, std::ratio<86400>> ? "%F" : "%F %T", m);
        }
    };

    // local_time of an integral duration: the same with no zone
    template<class Rep, class Period>
    requires std::is_integral_v<Rep>
    struct formatter<std::chrono::time_point<std::chrono::local_t, std::chrono::duration<Rep, Period>>> {
        static constexpr bool takes(char type) noexcept {
            return !type;
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_layout(std::string_view pattern) noexcept {
            return !pattern.data() || time::detail::pattern_ok(pattern, true, true, false);
        }

        static void write(format_sink& out, const std::chrono::time_point<std::chrono::local_t, std::chrono::duration<Rep, Period>>& t,
                          const format_spec& spec, std::string_view pattern) {
            time::detail::moment m = time::detail::moment_of_since(t.time_since_epoch(), true, true);
            detail::write_chrono(out, spec, pattern, std::ratio_greater_equal_v<Period, std::ratio<86400>> ? "%F" : "%F %T", m);
        }
    };

    // year_month_day: {} is %F, and " is not a valid date" after one that
    // is not
    template<>
    struct formatter<std::chrono::year_month_day> {
        static constexpr bool takes(char type) noexcept {
            return !type;
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_layout(std::string_view pattern) noexcept {
            return !pattern.data() || time::detail::pattern_ok(pattern, true, false, false);
        }

        static void write(format_sink& out, const std::chrono::year_month_day& d, const format_spec& spec, std::string_view pattern) {
            time::detail::moment m;
            if (d.ok()) {
                time::detail::fill_date(m, time::date(d));
                detail::write_chrono(out, spec, pattern, "%F", m);
                return;
            }
            // Not a date. A pattern asks for what it has not (its day of
            // the week, its day of the year): refused, as std::format
            // refuses it; {} is the fields as they are and the sentence
            // std writes after them
            if (pattern.data()) {
                throw invalid_argument("sgcl::txt::format: a pattern of a year_month_day that is not a valid date");
            }
            m.has_date = true;
            m.year = int(d.year());
            m.month = int(unsigned(d.month()));
            m.day = int(unsigned(d.day()));
            detail::write_chrono(out, spec, pattern, "%F", m, " is not a valid date");
        }
    };

    // weekday of <chrono>: {} is %a ("Mon")
    template<>
    struct formatter<std::chrono::weekday> {
        static constexpr bool takes(char type) noexcept {
            return !type;
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_layout(std::string_view pattern) noexcept {
            return formatter<time::weekday>::takes_layout(pattern);
        }

        static void write(format_sink& out, const std::chrono::weekday& d, const format_spec& spec, std::string_view pattern) {
            if (!d.ok()) {
                string text(std::to_string(d.c_encoding()) + " is not a valid weekday");
                detail::put_padded(out, std::string_view(text), spec);
                return;
            }
            time::detail::moment m;
            m.has_date = true;
            m.weekday = int(d.iso_encoding());
            detail::write_chrono(out, spec, pattern, "%a", m);
        }
    };

    // hh_mm_ss: {} is %T, a '-' before a negative one
    template<class D>
    struct formatter<std::chrono::hh_mm_ss<D>> {
        static constexpr bool takes(char type) noexcept {
            return !type;
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_layout(std::string_view pattern) noexcept {
            return !pattern.data() || time::detail::pattern_ok(pattern, false, true, false);
        }

        static void write(format_sink& out, const std::chrono::hh_mm_ss<D>& h, const format_spec& spec, std::string_view pattern) {
            time::detail::moment m = time::detail::moment_of_since(h.to_duration() < D::zero() ? -h.to_duration() : h.to_duration(), false, true);
            m.second_of_day = int64_t(h.hours().count()) * 3600 + int64_t(h.minutes().count()) * 60 + int64_t(h.seconds().count());
            m.negative = h.is_negative();
            detail::write_chrono(out, spec, pattern, "%T", m);
        }
    };

    // duration of <chrono>: {} is the count and the unit ("90min",
    // "1500ms", "2[1/3]s"); a pattern has the time of day of the span and
    // %j (its days), %Q (the count), %q (the unit). A negative span's '-'
    // is written once, before the first specifier, as [time.format] says
    // (libc++ writes one before every specifier). No precision: the
    // standard's text makes it a cut of the characters ({:.3} of 1.23456s
    // is "1.2"), which libc++ does and nobody wants
    template<class Rep, class Period>
    struct formatter<std::chrono::duration<Rep, Period>> {
        static constexpr bool takes(char type) noexcept {
            return !type;
        }

        static constexpr bool takes_precision() noexcept {
            return false;
        }

        static constexpr bool takes_layout(std::string_view pattern) noexcept {
            return !pattern.data() || time::detail::pattern_ok(pattern, false, true, false, true);
        }

        static void write(format_sink& out, const std::chrono::duration<Rep, Period>& d, const format_spec& spec, std::string_view pattern) {
            std::string count;
            std::string unit;
            time::detail::moment m = time::detail::moment_of_duration(d, count, unit);
            if (!pattern.data()) {
                std::string streamed = std::is_floating_point_v<Rep> ? time::detail::count_of(m.negative ? -d.count() : d.count(), true) : count;
                detail::put_body_in_field(out, spec, [&](format_sink& to) {
                    if (m.negative) {
                        to.put('-');
                    }
                    to.put(streamed);
                    to.put(m.unit);
                });
                return;
            }
            detail::write_chrono(out, spec, pattern, {}, m);
        }
    };
}

namespace sgcl::time {
    inline expected<datetime, error> datetime::parse(const string& text, const string& pattern, const time::zone& z) {
        detail::text_reader r{std::string_view(text)};
        detail::pattern_fields f;
        if (!detail::read_pattern(r, std::string_view(pattern), f, false)) {
            return unexpected(error(r.why, r.at));
        }
        if (r.more()) {
            return unexpected(error("the end of the text expected", r.i));
        }
        if (f.date_at == size_t(-1)) {
            f.date_at = 0;
        }
        auto d = detail::date_of_fields(f, f.date_at);
        if (!d) {
            return unexpected(d.error());
        }
        size_t time_at = f.time_at == size_t(-1) ? 0 : f.time_at;
        int hour = f.hour ? *f.hour : 0;
        if (f.hour12) {
            if (!f.pm) {
                return unexpected(error("%p with %I expected: 12 hours need AM or PM", time_at));
            }
            hour = *f.hour12 % 12 + (*f.pm ? 12 : 0);
            if (f.hour && *f.hour != hour) {
                return unexpected(error("an hour of 12 that is not the hour of 24", time_at));
            }
        }
        if (f.pm && f.hour && !f.hour12 && (*f.hour >= 12) != *f.pm) {
            return unexpected(error("AM or PM that is not the hour's", time_at));
        }
        int minute = f.minute ? *f.minute : 0;
        int second = f.second ? *f.second : 0;
        // An offset in the text: an instant, in a fixed zone
        if (f.offset) {
            detail::read_fields rf{d->year(), int(d->month()), d->day(), hour, minute, second, f.nanos, *f.offset, *f.offset == 0};
            return detail::instant_of_fields(rf, f.date_at, time_at);
        }
        std::string_view abbreviation = f.abbreviation;
        if (f.has_abbreviation && (abbreviation == "UTC" || abbreviation == "GMT" || abbreviation == "Z" || abbreviation == "UT")) {
            detail::read_fields rf{d->year(), int(d->month()), d->day(), hour, minute, second, f.nanos, 0, true};
            return detail::instant_of_fields(rf, f.date_at, time_at);
        }
        // A time of the zone's clock; a second of 60 is a leap second only
        // where the zone's clock is UTC's
        if (second == 60) {
            if (z != time::zone::utc()) {
                return unexpected(error("a second of 60 is a leap second, the last of a day in UTC", time_at));
            }
            detail::read_fields rf{d->year(), int(d->month()), d->day(), hour, minute, second, f.nanos, 0, true};
            return detail::instant_of_fields(rf, f.date_at, time_at);
        }
        int64_t wall = detail::wall_seconds(*d, hour, minute, second);
        const auto& zd = detail::zone_access::data(z);
        detail::wall_instants w = detail::instants_of(zd, wall);
        int64_t t;
        if (f.has_abbreviation) {
            // The instant whose abbreviation it is
            auto named = [&](int64_t at) {
                auto state = detail::zone_access::state_at(zd, at);
                return std::string_view(state.abbreviation ? *state.abbreviation : zd.name) == abbreviation;
            };
            if (w.count >= 1 && named(w.first)) {
                t = w.first;
            } else if (w.count == 2 && named(w.second)) {
                t = w.second;
            } else {
                return unexpected(error("an abbreviation the zone does not have at that time", time_at));
            }
        } else {
            t = detail::instant_of(zd, wall, 0);
        }
        __int128 ns = (__int128)t * detail::NanosPerSecond + f.nanos;
        if (ns < INT64_MIN || ns > INT64_MAX) {
            return unexpected(error("an instant within the years 1677 to 2262 expected", f.date_at));
        }
        return datetime::from_unix_nano(int64_t(ns), z);
    }

    inline expected<date, error> date::parse(const string& text, const string& pattern) {
        detail::text_reader r{std::string_view(text)};
        detail::pattern_fields f;
        if (!detail::read_pattern(r, std::string_view(pattern), f, true)) {
            return unexpected(error(r.why, r.at));
        }
        if (r.more()) {
            return unexpected(error("the end of the text expected", r.i));
        }
        return detail::date_of_fields(f, f.date_at == size_t(-1) ? 0 : f.date_at);
    }
}
