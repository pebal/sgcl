//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/string.h"
#include "../txt/locale.h"
#include "../txt/message_format.h"
#include "date.h"
#include "datetime.h"
#include "detail/cldr_dates.h"
#include "layout.h"
#include "../txt/names/detail/zone_names.h"
#include "zone.h"

#include <cstdint>
#include <string_view>

// Dates and times as a locale writes them (LDML Part 4): the names of the
// months and the days in the locale's language, its patterns of four
// lengths ("czwartek, 24 września 2026", "24.09.2026, 14:05"), a skeleton
// — the fields wanted, in any order ("yMMMd") — matched to the locale's
// own pattern for them, intervals ("24–26 września 2026"), and CLDR's
// pattern letters ("d MMMM y", "h:mm a"). The calendar is the proleptic
// Gregorian one of the time module, the data CLDR's (detail/cldr_dates.h).
//
// The time module's own patterns, those of strftime and std::format
// ("%d.%m.%Y"), stay what they are: they are written in English and read
// back; this header writes for a reader and reads nothing.
namespace sgcl::time {
    namespace detail::cldr {
        using namespace sgcl::txt::detail::cldr;

        enum : uint32_t {
            DlMonths = 0, DlDays = 6, DlQuarters = 14, DlEras = 20, DlPeriods = 21, DlDate = 27, DlTime = 31,
            DlGlue = 35, DlAtTime = 39, DlIntervalFallback = 43, DlZone = 44, DlDigits = 45, DlDecimal = 46,
            DlAppend = 47,
        };

        // What a locale's calendar is, read once
        struct Calendar {
            uint16_t index = 0;               // cldr_locales.h
            uint16_t row[DateLocalesWidth];   // the locale's DateLocales row
            uint8_t first_day = 1;            // ISO: Monday 1
            uint8_t min_days = 1;
            char hour = 'H';                  // the preferred hour letter (j)
            char allowed[2] = {'H', 0};       // the first allowed form (C)
            uint64_t key = 0;                 // the locale, packed
        };

        inline Calendar calendar_of(const txt::locale& l) noexcept {
            using A = txt::detail::cldr::LocaleAccess;
            Calendar c;
            c.index = txt::detail::cldr::locale_index(l);
            uint32_t r = DateLocale[c.index] * DateLocalesWidth;
            for (uint32_t k = 0; k < DateLocalesWidth; ++k) {
                c.row[k] = uint16_t(DateLocales[r + k]);
            }
            c.key = A::key(l);
            uint64_t max = txt::detail::cldr::maximize(c.key);
            uint32_t region = A::region(max);
            size_t w = find(WeekKeys, uint16_t(region));
            if (w == std::size(WeekKeys)) {
                w = find(WeekKeys, uint16_t(729 + 1));   // 001
            }
            if (w != std::size(WeekKeys)) {
                c.first_day = uint8_t(WeekRules[w] & 15);
                c.min_days = uint8_t(WeekRules[w] >> 4);
            }
            // the hours: a language and region, the region, the world
            size_t h = find(HourKeys, A::compose(A::language(max), 0, region));
            if (h == std::size(HourKeys)) {
                h = find(HourKeys, A::compose(0, 0, region));
            }
            if (h == std::size(HourKeys)) {
                h = find(HourKeys, A::compose(0, 0, 729 + 1));
            }
            if (h != std::size(HourKeys)) {
                c.hour = char(HourRules[h] & 0xFF);
                c.allowed[0] = char((HourRules[h] >> 8) & 0xFF);
                c.allowed[1] = char((HourRules[h] >> 16) & 0xFF);
            }
            return c;
        }

        SGCL_INLINE_HOT std::string_view date_text(uint32_t i) noexcept {
            return DateTexts[i];
        }

        // The fields of what is written: a datetime's, or a date's at its
        // midnight in UTC
        struct Fields {
            int64_t year = 1970;
            int month = 1;
            int day = 1;
            int weekday = 4;   // ISO
            int year_day = 1;
            int hour = 0;
            int minute = 0;
            int second = 0;
            int64_t nanos = 0;
            int32_t offset = 0;           // seconds east of UTC
            std::string_view zone_name;   // "Europe/Warsaw"
            bool has_zone = false;
            bool dst = false;             // daylight saving time at the instant
            bool dst_near = false;        // a change of daylight saving time within half a year of it
            bool pattern_minutes = false; // whether the pattern shows them (for noon)
            bool pattern_seconds = false;
        };

        // Where the text goes: what fits, and a count of the whole
        struct Out {
            char* at;
            char* end;
            size_t size = 0;

            SGCL_INLINE_HOT void put(std::string_view s) noexcept {
                size_t room = size_t(end - at);
                size_t n = s.size() < room ? s.size() : room;
                for (size_t i = 0; i < n; ++i) {
                    at[i] = s[i];
                }
                at += n;
                size += s.size();
            }

            SGCL_INLINE_HOT void put(char c) noexcept {
                if (at < end) {
                    *at++ = c;
                }
                ++size;
            }
        };

        // A number in the locale's digits, padded with zeros to width
        inline void put_number(Out& out, const Calendar& c, int64_t v, int width) noexcept {
            std::string_view digits = date_text(c.row[DlDigits]);
            if (v < 0) {
                out.put('-');
                v = -v;
            }
            char tmp[24];
            int n = 0;
            do {
                tmp[n++] = char('0' + v % 10);
                v /= 10;
            } while (v);
            size_t w = digits.size() / 10;
            auto put_digit = [&](char d) {
                if (w <= 1) {
                    out.put(d);
                } else {
                    out.put(digits.substr(size_t(d - '0') * w, w));
                }
            };
            // the padding of a field of more letters than digits: "yyyyyyyyy"
            for (int k = n; k < width; ++k) {
                put_digit('0');
            }
            for (int k = n - 1; k >= 0; --k) {
                put_digit(tmp[k]);
            }
        }

        SGCL_INLINE_HOT std::string_view name(const Packed& table, uint32_t width, uint32_t row, int k) noexcept {
            return date_text(table[row * width + uint32_t(k)]);
        }

        // The day of the week as the locale counts it: 1 for its first day
        SGCL_INLINE_HOT int local_day(const Calendar& c, int weekday) noexcept {
            return (weekday - c.first_day + 7) % 7 + 1;
        }

        constexpr int64_t days_from_civil(int64_t y, int m, int d) noexcept {
            y -= m <= 2;
            int64_t era = (y >= 0 ? y : y - 399) / 400;
            int64_t yoe = y - era * 400;
            int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
            int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
            return era * 146097 + doe - 719468;
        }

        constexpr int weekday_of_days(int64_t days) noexcept {
            // 1970-01-01 was a Thursday (ISO 4)
            return int(((days % 7) + 7 + 3) % 7) + 1;
        }

        // The week of the year and the year it belongs to, by the
        // locale's first day and the least days of a first week
        inline void week_of_year(const Calendar& c, int64_t year, int year_day, int weekday, int64_t& week_year,
                                 int& week) noexcept {
            auto first_week_start = [&](int64_t y) {
                // the day number (since 1970) of the start of week 1 of y
                int64_t jan1 = days_from_civil(y, 1, 1);
                int k = local_day(c, weekday_of_days(jan1)) - 1;   // days of its week before Jan 1
                int64_t start = jan1 - k;
                if (7 - k < c.min_days) {
                    start += 7;
                }
                return start;
            };
            int64_t day = days_from_civil(year, 1, 1) + year_day - 1;
            (void)weekday;
            int64_t s = first_week_start(year);
            if (day < s) {
                week_year = year - 1;
                s = first_week_start(year - 1);
            } else {
                int64_t next = first_week_start(year + 1);
                if (day >= next) {
                    week_year = year + 1;
                    s = next;
                } else {
                    week_year = year;
                }
            }
            week = int((day - s) / 7) + 1;
        }

        inline int week_of_month(const Calendar& c, const Fields& f) noexcept {
            int64_t first = days_from_civil(f.year, f.month, 1);
            int k = local_day(c, weekday_of_days(first)) - 1;
            int w = (f.day - 1 + k) / 7;
            return 7 - k >= c.min_days ? w + 1 : w;
        }

        // The localized GMT format: "GMT+2" short, "GMT+02:00" long
        inline void put_gmt(Out& out, const Calendar& c, int32_t offset, bool long_form) noexcept {
            uint32_t zrow = c.row[DlZone] * ZoneFormatsWidth;
            if (offset == 0) {
                out.put(date_text(ZoneFormats[zrow + 2]));
                return;
            }
            std::string_view gmt = date_text(ZoneFormats[zrow + 1]);
            std::string_view hour = date_text(ZoneFormats[zrow + 0]);
            size_t semi = hour.find(';');
            std::string_view h = offset >= 0 ? hour.substr(0, semi) : hour.substr(semi + 1);
            int32_t a = offset < 0 ? -offset : offset;
            int hh = a / 3600, mm = a / 60 % 60, ss = a % 60;
            size_t at = gmt.find("{0}");
            out.put(gmt.substr(0, at));
            // the hour format, "+HH:mm": short drops the zero of the hour
            // and the minutes when they are zero
            char sep = ':';
            for (size_t i = 0; i < h.size();) {
                char ch = h[i];
                size_t n = 1;
                while (i + n < h.size() && h[i + n] == ch) {
                    ++n;
                }
                if (ch == 'H') {
                    put_number(out, c, hh, long_form ? int(n) : 1);
                    if (!long_form && !mm && !ss) {
                        break;   // the short form of a whole hour ends at it, as ICU truncates it
                    }
                } else if (ch == 'm') {
                    if (long_form || mm || ss) {
                        put_number(out, c, mm, 2);
                        if (ss) {
                            // an offset of the old local mean times: +05:53:28
                            out.put(sep);
                            put_number(out, c, ss, 2);
                        }
                    }
                } else if (ch == ':' || ch == '.') {
                    // the separator stands before the minutes only where they are written
                    sep = ch;
                    if (long_form || mm || ss) {
                        out.put(ch);
                    }
                } else {
                    out.put(h.substr(i, n));
                }
                i += n;
            }
            if (at != std::string_view::npos) {
                out.put(gmt.substr(at + 3));
            }
        }

        // ISO 8601 offsets: X (Z for zero) and x, of 1 to 5 letters
        inline void put_iso_offset(Out& out, int32_t offset, int n, bool z) noexcept {
            if (offset == 0 && z) {
                out.put('Z');
                return;
            }
            int32_t a = offset < 0 ? -offset : offset;
            int hh = a / 3600, mm = a / 60 % 60, ss = a % 60;
            out.put(offset < 0 ? '-' : '+');
            out.put(char('0' + hh / 10));
            out.put(char('0' + hh % 10));
            bool extended = n == 3 || n == 5;
            if (n == 1 && mm == 0) {
                return;
            }
            if (extended) {
                out.put(':');
            }
            out.put(char('0' + mm / 10));
            out.put(char('0' + mm % 10));
            if (n >= 4 && ss) {
                if (extended) {
                    out.put(':');
                }
                out.put(char('0' + ss / 10));
                out.put(char('0' + ss % 10));
            }
        }

        // The flexible period of the day (B) of a time, as an index of
        // the periods (midnight, noon, morning1...), or -1
        inline int day_period(const Calendar& c, int hour, int minute, int second) noexcept {
            using A = txt::detail::cldr::LocaleAccess;
            uint64_t language = A::compose(A::language(c.key), 0, 0);
            size_t i = find(PeriodKeys, language);
            if (i == std::size(PeriodKeys)) {
                i = find(PeriodKeys, uint64_t(0));
            }
            if (i == std::size(PeriodKeys)) {
                return -1;
            }
            uint32_t set = PeriodSets[i];
            int m = hour * 60 + minute;
            int found = -1;
            for (uint32_t k = PeriodStart[set]; k < PeriodStart[set + 1]; ++k) {
                uint32_t r = PeriodRules[k];
                int period = int(r >> 24);
                int from = int((r >> 12) & 0x7FF), before = int(r & 0xFFF);
                if (r & (1u << 23)) {
                    if (m == from && second == 0) {
                        return period;
                    }
                    continue;
                }
                bool in = from <= before ? (m >= from && m < before) : (m >= from || m < before);
                if (in && found < 0) {
                    found = period;
                }
            }
            return found;
        }

        // The zones' display names, from the registered names of the
        // locale (sgcl/txt/names/<locale>.h): a metazone's or a zone's name,
        // the location format with the exemplar city, else the localized
        // GMT format, as TR35 Part 4 §7 falls back
        inline txt::detail::names::Chain zone_chain(const Calendar& c) noexcept {
            namespace N = txt::detail::names;
            N::Chain chain;
            for (uint64_t k = c.key ? txt::detail::cldr::normalized_key(
                     txt::detail::cldr::LocaleAccess::make(c.key)) : 0; k; k = txt::detail::cldr::parent(k)) {
                if (const N::Table* t = N::table_of(k)) {
                    N::follow(chain, t);
                    break;
                }
            }
            return chain;
        }

        inline std::string_view zone_city(const txt::detail::names::Chain& chain, uint32_t zone) noexcept {
            namespace N = txt::detail::names;
            return zone == UINT32_MAX ? std::string_view()
                 : N::first_of(chain, [&](const N::Table& t) { return N::zone_name(t, zone, 0); });
        }

        inline void put_city(Out& out, const Calendar& c, const Fields& f) noexcept {
            uint32_t zone = txt::detail::names::zone_index(f.zone_name);
            auto chain = zone_chain(c);
            std::string_view city = zone_city(chain, zone);
            if (city.empty() && zone != UINT32_MAX && !txt::detail::cldr::ZoneRegions[zone]) {
                // a zone of no country (Etc/UTC): the city of Etc/Unknown, as ICU
                city = zone_city(chain, txt::detail::names::zone_index("Etc/Unknown"));
            }
            if (!city.empty()) {
                out.put(city);
                return;
            }
            // the last part of the identifier, its underscores spaces
            std::string_view id = f.zone_name;
            size_t slash = id.rfind('/');
            for (char x : slash == std::string_view::npos ? id : id.substr(slash + 1)) {
                out.put(x == '_' ? ' ' : x);
            }
        }

        // the location format: "czas: Warszawa", "{0} (czas letni)";
        // which: 0 generic, 1 standard, 2 daylight
        inline void put_location(Out& out, const Calendar& c, const Fields& f, uint32_t which) noexcept {
            namespace N = txt::detail::names;
            auto chain = zone_chain(c);
            std::string_view pattern = N::first_of(chain, [&](const N::Table& t) {
                return N::format(t, N::FmtRegion + which);
            });
            if (pattern.empty()) {
                pattern = "{0}";
            }
            size_t at = pattern.find("{0}");
            out.put(pattern.substr(0, at));
            if (at != std::string_view::npos) {
                // the country where it has one zone (or this is its primary
                // one), else the city
                uint32_t zone = N::zone_index(f.zone_name);
                uint32_t region = zone < std::size(txt::detail::cldr::ZoneRegions) ? txt::detail::cldr::ZoneRegions[zone] : 0;
                std::string_view country;
                if (region & 0x8000) {
                    country = N::first_of(chain, [&](const N::Table& t) { return N::region_name(t, region & 0x7FFF); });
                }
                if (!country.empty()) {
                    out.put(country);
                } else {
                    put_city(out, c, f);
                }
                out.put(pattern.substr(at + 3));
            }
        }

        inline void put_zone_name(Out& out, const Calendar& c, const Fields& f, char ch, bool long_form) noexcept {
            namespace N = txt::detail::names;
            uint32_t zone = f.zone_name.empty() ? UINT32_MAX : N::zone_index(f.zone_name);
            if (zone != UINT32_MAX) {
                auto chain = zone_chain(c);
                uint32_t meta = N::metazone_of(zone);
                // z: the metazone's or the zone's standard or daylight name;
                // v: the metazone's generic name, its standard one where no
                // daylight saving time is near (Japan Standard Time), as ICU
                uint32_t base = long_form ? 0 : 3;
                std::string_view name;
                if (ch == 'z') {
                    uint32_t kind = f.dst ? 2 : 1;
                    if (meta != UINT32_MAX) {
                        name = N::first_of(chain, [&](const N::Table& t) { return N::meta_name(t, meta, base + kind); });
                    }
                    if (name.empty()) {
                        name = N::first_of(chain, [&](const N::Table& t) { return N::zone_name(t, zone, 1 + base + kind); });
                    }
                } else if (meta != UINT32_MAX) {
                    for (uint32_t kind : {f.dst_near ? 0u : 1u, f.dst_near ? 1u : 0u}) {
                        if (kind == 1 && f.dst_near) {
                            break;
                        }
                        name = N::first_of(chain, [&](const N::Table& t) { return N::meta_name(t, meta, base + kind); });
                        if (!name.empty()) {
                            break;
                        }
                    }
                }
                if (!name.empty()) {
                    out.put(name);
                    return;
                }
                bool has_country = zone < std::size(txt::detail::cldr::ZoneRegions) && txt::detail::cldr::ZoneRegions[zone];
                if (ch == 'v' && !chain.empty() && has_country) {
                    put_location(out, c, f, 0);   // a generic name falls back on the location format
                    return;
                }
            }
            put_gmt(out, c, f.offset, long_form);
        }

        // One field of a pattern: letter ch repeated n times
        inline void put_field(Out& out, const Calendar& c, const Fields& f, char ch, int n) noexcept {
            auto width_index = [](int count) {
                return count == 4 ? 0 : count == 5 ? 2 : 1;   // wide, abbreviated, narrow
            };
            switch (ch) {
                case 'G': {
                    int era = f.year > 0 ? 1 : 0;
                    int w = n == 4 ? 0 : n == 5 ? 2 : 1;
                    out.put(name(EraNames, EraNamesWidth, c.row[DlEras], w * 2 + era));
                    return;
                }
                case 'y': {
                    int64_t y = f.year > 0 ? f.year : 1 - f.year;
                    if (n == 2) {
                        put_number(out, c, y % 100, 2);
                    } else {
                        put_number(out, c, y, n);
                    }
                    return;
                }
                case 'u':
                    put_number(out, c, f.year, n);
                    return;
                case 'Y': {
                    int64_t wy;
                    int week;
                    week_of_year(c, f.year, f.year_day, f.weekday, wy, week);
                    if (n == 2) {
                        put_number(out, c, (wy > 0 ? wy : 1 - wy) % 100, 2);
                    } else {
                        put_number(out, c, wy > 0 ? wy : 1 - wy, n);
                    }
                    return;
                }
                case 'Q':
                case 'q': {
                    int q = (f.month - 1) / 3;
                    if (n <= 2) {
                        put_number(out, c, q + 1, n);
                    } else {
                        uint32_t base = ch == 'Q' ? DlQuarters : DlQuarters + 3;
                        out.put(name(QuarterNames, QuarterNamesWidth, c.row[base + uint32_t(width_index(n))], q));
                    }
                    return;
                }
                case 'M':
                case 'L': {
                    if (n <= 2) {
                        put_number(out, c, f.month, n);
                    } else {
                        uint32_t base = ch == 'M' ? DlMonths : DlMonths + 3;
                        out.put(name(MonthNames, MonthNamesWidth, c.row[base + uint32_t(width_index(n))], f.month - 1));
                    }
                    return;
                }
                case 'w': {
                    int64_t wy;
                    int week;
                    week_of_year(c, f.year, f.year_day, f.weekday, wy, week);
                    put_number(out, c, week, n);
                    return;
                }
                case 'W':
                    put_number(out, c, week_of_month(c, f), n);
                    return;
                case 'd':
                    put_number(out, c, f.day, n);
                    return;
                case 'D':
                    put_number(out, c, f.year_day, n);
                    return;
                case 'F':
                    put_number(out, c, (f.day - 1) / 7 + 1, n);
                    return;
                case 'g':
                    // the Julian day of the local date, as ICU counts it (2440588 on 1970-01-01)
                    put_number(out, c, days_from_civil(f.year, f.month, f.day) + 2440588, n);
                    return;
                case 'E':
                case 'e':
                case 'c': {
                    if ((ch == 'e' && n <= 2) || (ch == 'c' && n <= 2)) {
                        put_number(out, c, local_day(c, f.weekday), ch == 'c' ? 1 : n);
                        return;
                    }
                    // E 1-3 abbreviated, 4 wide, 5 narrow, 6 short
                    uint32_t base = ch == 'c' ? DlDays + 4 : DlDays;
                    int w = n == 4 ? 0 : n == 5 ? 3 : n == 6 ? 2 : 1;
                    out.put(name(DayNames, DayNamesWidth, c.row[base + uint32_t(w)], f.weekday - 1));
                    return;
                }
                case 'a':
                case 'b':
                case 'B': {
                    // as ICU writes them: noon at 12:00 (the minutes and the
                    // seconds asked only where the pattern shows them),
                    // midnight never ("midnight" is the start of a day or
                    // its end, and ICU holds it back since 57), a flexible
                    // period by the hour, am or pm where a name is missing
                    uint32_t row = c.row[DlPeriods + uint32_t(width_index(n))];
                    int period = f.hour < 12 ? 0 : 1;
                    if (ch != 'a') {
                        bool noon = f.hour == 12 && (!f.pattern_minutes || f.minute == 0)
                                 && (!f.pattern_seconds || f.second == 0);
                        int p = noon ? 3 : ch == 'B' ? day_period(c, f.hour, 0, 1) : -1;
                        if (p == 2) {
                            p = -1;
                        }
                        if (p >= 0 && name(PeriodNames, PeriodNamesWidth, row, p).empty() && noon && ch == 'B') {
                            p = day_period(c, f.hour, 0, 1);
                        }
                        if (p >= 0 && !name(PeriodNames, PeriodNamesWidth, row, p).empty()) {
                            period = p;
                        }
                    }
                    out.put(name(PeriodNames, PeriodNamesWidth, row, period));
                    return;
                }
                case 'h':
                    put_number(out, c, f.hour % 12 == 0 ? 12 : f.hour % 12, n);
                    return;
                case 'H':
                    put_number(out, c, f.hour, n);
                    return;
                case 'K':
                    put_number(out, c, f.hour % 12, n);
                    return;
                case 'k':
                    put_number(out, c, f.hour == 0 ? 24 : f.hour, n);
                    return;
                case 'm':
                    put_number(out, c, f.minute, n);
                    return;
                case 's':
                    put_number(out, c, f.second, n);
                    return;
                case 'S': {
                    // the fraction truncated to n digits
                    int64_t v = f.nanos;
                    char digits[9];
                    for (int k = 8; k >= 0; --k) {
                        digits[k] = char('0' + v % 10);
                        v /= 10;
                    }
                    std::string_view d = date_text(c.row[DlDigits]);
                    size_t w = d.size() / 10;
                    for (int k = 0; k < n; ++k) {
                        char x = k < 9 ? digits[k] : '0';
                        if (w <= 1) {
                            out.put(x);
                        } else {
                            out.put(d.substr(size_t(x - '0') * w, w));
                        }
                    }
                    return;
                }
                case 'A':
                    put_number(out, c, ((int64_t(f.hour) * 60 + f.minute) * 60 + f.second) * 1000 + f.nanos / 1000000, n);
                    return;
                case 'z':
                case 'v':
                    put_zone_name(out, c, f, ch, n == 4);
                    return;
                case 'O':
                    put_gmt(out, c, f.offset, n == 4);
                    return;
                case 'Z':
                    if (n <= 3) {
                        put_iso_offset(out, f.offset, 4, false);
                    } else if (n == 4) {
                        put_gmt(out, c, f.offset, true);
                    } else {
                        put_iso_offset(out, f.offset, 5, true);
                    }
                    return;
                case 'X':
                case 'x':
                    put_iso_offset(out, f.offset, n, ch == 'X');
                    return;
                case 'V':
                    if (n == 2 && !f.zone_name.empty()) {
                        out.put(f.zone_name);
                    } else if (n == 3 && !f.zone_name.empty()) {
                        put_city(out, c, f);
                    } else if (n == 4 && !f.zone_name.empty() && txt::detail::names::zone_index(f.zone_name) != UINT32_MAX
                               && txt::detail::cldr::ZoneRegions[txt::detail::names::zone_index(f.zone_name)]) {
                        put_location(out, c, f, 0);   // a zone of a country; Etc/UTC and the like: GMT
                    } else {
                        put_gmt(out, c, f.offset, true);
                    }
                    return;
                default:
                    for (int k = 0; k < n; ++k) {
                        out.put(ch);
                    }
                    return;
            }
        }

        SGCL_INLINE_HOT bool pattern_letter(char ch) noexcept {
            return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
        }

        // A pattern of CLDR's letters: a run of one letter is a field,
        // text between apostrophes is literal ('' an apostrophe), and the
        // rest stands as it is
        inline void put_pattern(Out& out, const Calendar& c, const Fields& fields, std::string_view p) noexcept {
            Fields f = fields;
            bool quoted = false;
            for (char ch : p) {
                if (ch == '\'') {
                    quoted = !quoted;
                } else if (!quoted) {
                    f.pattern_minutes = f.pattern_minutes || ch == 'm';
                    f.pattern_seconds = f.pattern_seconds || ch == 's';
                }
            }
            for (size_t i = 0; i < p.size();) {
                char ch = p[i];
                if (ch == '\'') {
                    if (i + 1 < p.size() && p[i + 1] == '\'') {
                        out.put('\'');
                        i += 2;
                        continue;
                    }
                    size_t j = i + 1;
                    while (j < p.size()) {
                        if (p[j] == '\'') {
                            if (j + 1 < p.size() && p[j + 1] == '\'') {
                                out.put('\'');
                                j += 2;
                                continue;
                            }
                            break;
                        }
                        out.put(p[j]);
                        ++j;
                    }
                    i = j + 1;
                    continue;
                }
                if (pattern_letter(ch)) {
                    size_t j = i;
                    while (j < p.size() && p[j] == ch) {
                        ++j;
                    }
                    put_field(out, c, f, ch, int(j - i));
                    i = j;
                    continue;
                }
                out.put(ch);
                ++i;
            }
        }

        //----------------------------------------------------------------
        // skeletons (TR35 Part 4 §2.6.2)
        //----------------------------------------------------------------
        // The field a letter belongs to; its kind: numeric (1..2 letters
        // where the letter allows text) or text
        enum Field : int {
            FEra, FYear, FQuarter, FMonth, FWeekOfYear, FWeekOfMonth, FWeekday, FDay, FDayOfYear, FDayOfWeekInMonth,
            FPeriod, FHour, FMinute, FSecond, FFraction, FZone, FCount,
        };

        inline int field_of(char ch) noexcept {
            switch (ch) {
                case 'G': return FEra;
                case 'y': case 'Y': case 'u': case 'U': case 'r': return FYear;
                case 'Q': case 'q': return FQuarter;
                case 'M': case 'L': return FMonth;
                case 'w': return FWeekOfYear;
                case 'W': return FWeekOfMonth;
                case 'E': case 'e': case 'c': return FWeekday;
                case 'd': return FDay;
                case 'D': return FDayOfYear;
                case 'F': return FDayOfWeekInMonth;
                case 'a': case 'b': case 'B': return FPeriod;
                case 'h': case 'H': case 'K': case 'k': case 'j': case 'J': case 'C': return FHour;
                case 'm': return FMinute;
                case 's': return FSecond;
                case 'S': case 'A': return FFraction;
                case 'z': case 'Z': case 'O': case 'v': case 'V': case 'X': case 'x': return FZone;
                default: return -1;
            }
        }

        // A skeleton read: for each field its letter and count
        struct Skeleton {
            char letter[FCount] = {};
            uint8_t count[FCount] = {};

            bool has(int f) const noexcept {
                return letter[f] != 0;
            }

            bool any_date() const noexcept {
                for (int f = FEra; f <= FDayOfWeekInMonth; ++f) {
                    if (has(f)) {
                        return true;
                    }
                }
                return false;
            }

            bool any_time() const noexcept {
                for (int f = FPeriod; f < FCount; ++f) {
                    if (has(f)) {
                        return true;
                    }
                }
                return false;
            }
        };

        // A text's kind for a field's count: numeric fields' numbers are
        // their counts, text forms count from 0x100
        inline int field_value(char letter, int count) noexcept {
            switch (letter) {
                case 'M': case 'L': case 'Q': case 'q': case 'e': case 'c':
                    return count <= 2 ? count : 0x100 + count;
                case 'E':
                    return 0x100 + count;
                case 'G':
                    return 0x100 + count;
                case 'a': case 'b': case 'B':
                    return 0x100 + count + (letter == 'B' ? 0x40 : letter == 'b' ? 0x20 : 0);
                case 'z': case 'v':
                    return 0x100 + count + (letter == 'v' ? 0x20 : 0);
                case 'h': return 0x10 + count;
                case 'H': return 0x20 + count;
                case 'K': return 0x30 + count;
                case 'k': return 0x40 + count;
                default:
                    return count;
            }
        }

        inline Skeleton read_skeleton(std::string_view s, const Calendar& c) noexcept {
            Skeleton k;
            for (size_t i = 0; i < s.size();) {
                char ch = s[i];
                size_t j = i;
                while (j < s.size() && s[j] == ch) {
                    ++j;
                }
                int n = int(j - i);
                i = j;
                // the locale's own hours for j, J and C
                if (ch == 'j' || ch == 'J' || ch == 'C') {
                    char hour = ch == 'C' ? c.allowed[0] : c.hour;
                    if (ch != 'J' && (hour == 'h' || hour == 'K')) {
                        char period = ch == 'C' && c.allowed[1] ? c.allowed[1] : 'a';
                        k.letter[FPeriod] = period;
                        k.count[FPeriod] = uint8_t(n <= 2 ? 1 : n <= 4 ? 4 : 5);
                    }
                    ch = hour;
                    n = n <= 2 ? n : 2;
                }
                int f = field_of(ch);
                if (f < 0) {
                    continue;
                }
                if (f == FHour && (ch == 'h' || ch == 'K') && !k.has(FPeriod)) {
                    k.letter[FPeriod] = 'a';
                    k.count[FPeriod] = 1;
                }
                k.letter[f] = ch;
                k.count[f] = uint8_t(n > 255 ? 255 : n);
            }
            return k;
        }

        constexpr int MissingField = 0x1000;
        constexpr int ExtraField = 0x10000;

        inline int skeleton_distance(const Skeleton& want, const Skeleton& have, int& missing) noexcept {
            int d = 0;
            missing = 0;
            for (int f = 0; f < FCount; ++f) {
                if (want.has(f) && have.has(f)) {
                    int a = field_value(want.letter[f], want.count[f]);
                    int b = field_value(have.letter[f], have.count[f]);
                    d += a > b ? a - b : b - a;
                } else if (want.has(f)) {
                    d += MissingField;
                    ++missing;
                } else if (have.has(f)) {
                    d += ExtraField;
                }
            }
            return d;
        }

        // The pattern adjusted to the skeleton asked for: a field whose
        // count in the pattern is the one of the matched skeleton takes
        // the asked count, numeric to numeric and text to text (a pattern
        // that pads, dd.MM.y for yMd, keeps its padding)
        inline void adjust(string& out, std::string_view pattern, const Skeleton& want, const Skeleton& matched) {
            char buf[512];
            size_t n = 0;
            auto put = [&](char ch) {
                if (n < sizeof buf) {
                    buf[n++] = ch;
                }
            };
            for (size_t i = 0; i < pattern.size();) {
                char ch = pattern[i];
                if (ch == '\'') {
                    size_t j = i + 1;
                    put(ch);
                    while (j < pattern.size()) {
                        put(pattern[j]);
                        if (pattern[j] == '\'') {
                            if (j + 1 < pattern.size() && pattern[j + 1] == '\'') {
                                put(pattern[j + 1]);
                                j += 2;
                                continue;
                            }
                            break;
                        }
                        ++j;
                    }
                    i = j + 1;
                    continue;
                }
                if (!pattern_letter(ch)) {
                    put(ch);
                    ++i;
                    continue;
                }
                size_t j = i;
                while (j < pattern.size() && pattern[j] == ch) {
                    ++j;
                }
                int count = int(j - i);
                int f = field_of(ch);
                if (f >= 0 && want.has(f) && matched.has(f) && f != FHour && f != FFraction) {
                    int asked = want.count[f];
                    int had = matched.count[f];
                    bool numeric_pattern = field_value(ch, count) < 0x100;
                    bool numeric_asked = field_value(want.letter[f], asked) < 0x100;
                    if (asked != had && count == had && numeric_pattern == numeric_asked) {
                        count = asked;
                    } else if (asked != had && !numeric_pattern && !numeric_asked && count >= 3) {
                        count = asked;
                    }
                    if (f == FWeekday && want.letter[f] == 'c' && ch == 'E') {
                        ch = 'c';
                    }
                }
                if (f == FZone && want.has(f)) {
                    // the zone as asked: z for v, and its count
                    ch = want.letter[f];
                    count = want.count[f];
                }
                if (ch == 'E' && count < 3) {
                    count = 3;   // E and EE are EEE, as ICU writes them
                }
                for (int k = 0; k < count; ++k) {
                    put(ch);
                }
                i = j;
            }
            out = string(std::string_view(buf, n));
        }

        // The locale's patterns of skeletons, its own run and then its
        // base's, the best of them for a skeleton
        struct Match {
            std::string_view pattern;
            Skeleton matched;
            int distance = 1 << 30;
            int missing = 0;
        };

        inline void best_in_run(Match& best, const Skeleton& want, const Calendar& c, uint32_t run) noexcept {
            uint32_t start = run >> 12, n = run & 0xFFF;
            for (uint32_t k = 0; k < n; ++k) {
                uint32_t e = DateItems[start + k];
                Skeleton have = read_skeleton(SkeletonTexts[e >> 16], c);
                int missing;
                int d = skeleton_distance(want, have, missing);
                if (d < best.distance) {
                    best.distance = d;
                    best.missing = missing;
                    best.pattern = date_text(e & 0xFFFF);
                    best.matched = have;
                }
            }
        }

        // The skeleton of a pattern: its fields as they stand
        inline Skeleton skeleton_of_pattern(std::string_view p) noexcept {
            Skeleton k;
            bool quoted = false;
            for (size_t i = 0; i < p.size();) {
                char ch = p[i];
                if (ch == '\'') {
                    quoted = !quoted;
                    ++i;
                    continue;
                }
                size_t j = i + 1;
                while (j < p.size() && p[j] == ch) {
                    ++j;
                }
                int f = quoted ? -1 : field_of(ch);
                if (f >= 0) {
                    k.letter[f] = ch;
                    k.count[f] = uint8_t(j - i > 255 ? 255 : j - i);
                }
                i = j;
            }
            return k;
        }

        // The best of the locale's patterns for a skeleton: its four date
        // and four time patterns, each under the skeleton of its own
        // fields, and its availableFormats, which win a tie (ICU's
        // DateTimePatternGenerator takes both, the second over the first)
        inline Match best_pattern(const Skeleton& want, const Calendar& c) noexcept {
            Match best;
            for (uint32_t k = 0; k < 8; ++k) {
                std::string_view p = date_text(c.row[DlDate + k]);
                Skeleton have = skeleton_of_pattern(p);
                int missing;
                int d = skeleton_distance(want, have, missing);
                if (d < best.distance) {
                    best.distance = d;
                    best.missing = missing;
                    best.pattern = p;
                    best.matched = have;
                }
            }
            Match items;
            best_in_run(items, want, c, DateItemRuns[c.index]);
            if (DateBase[c.index]) {
                best_in_run(items, want, c, DateItemRuns[DateBase[c.index]]);
            }
            return items.distance <= best.distance ? items : best;
        }

        // The pattern joining a date and a time of a date's length (full
        // 0 .. short 3): the atTime form for full and long where the
        // locale has one ("… 'at' …"), the plain one otherwise, as ICU
        inline std::string_view glue_of(const Calendar& c, uint32_t length) noexcept {
            if (length <= 1) {
                std::string_view at = date_text(c.row[DlAtTime + length]);
                if (!at.empty()) {
                    return at;
                }
            }
            return date_text(c.row[DlGlue + length]);
        }

        // The skeleton's fields of the date alone, or of the time alone
        inline Skeleton part(const Skeleton& s, bool date) noexcept {
            Skeleton out;
            for (int f = 0; f < FCount; ++f) {
                bool is_date = f <= FDayOfWeekInMonth;
                if (is_date == date) {
                    out.letter[f] = s.letter[f];
                    out.count[f] = s.count[f];
                }
            }
            return out;
        }

        inline void join(string& out, std::string_view glue, const string& date, const string& time) {
            // {1} the date, {0} the time; apostrophes quote text
            char buf[1024];
            size_t n = 0;
            auto put = [&](std::string_view s) {
                for (char ch : s) {
                    if (n < sizeof buf) {
                        buf[n++] = ch;
                    }
                }
            };
            for (size_t i = 0; i < glue.size();) {
                if (glue.substr(i, 3) == "{0}") {
                    put(time.view());
                    i += 3;
                } else if (glue.substr(i, 3) == "{1}") {
                    put(date.view());
                    i += 3;
                } else {
                    put(glue.substr(i, 1));
                    ++i;
                }
            }
            out = string(std::string_view(buf, n));
        }

        // The pattern for a skeleton: the best of the locale's, adjusted;
        // a date and a time matched apart and joined when no one pattern
        // has both; a field still missing appended as appendItems does
        inline string pattern_of_skeleton(std::string_view skeleton, const Calendar& c) {
            Skeleton want = read_skeleton(skeleton, c);
            string out;
            if (!want.any_date() && !want.any_time()) {
                return out;   // no field asked for: nothing to write
            }
            Match best = best_pattern(want, c);
            if (best.missing > 0 && want.any_date() && want.any_time()) {
                Skeleton d = part(want, true), t = part(want, false);
                Match bd = best_pattern(d, c), bt = best_pattern(t, c);
                string ds, ts;
                adjust(ds, bd.pattern, d, bd.matched);
                adjust(ts, bt.pattern, t, bt.matched);
                // the length of the date part chooses the glue
                int length = 3;   // short
                if (d.has(FMonth) && field_value(d.letter[FMonth], d.count[FMonth]) >= 0x100) {
                    int mc = d.count[FMonth];
                    length = mc == 4 ? (d.has(FWeekday) ? 0 : 1) : 2;
                }
                join(out, glue_of(c, uint32_t(length)), ds, ts);
                return out;
            }
            if (best.pattern.empty() && best.distance >= ExtraField) {
                best.pattern = date_text(c.row[DlDate + 3]);
            }
            adjust(out, best.pattern, want, best.matched);
            // fractions of a second after the seconds, with the decimal separator
            if (want.has(FFraction) && want.letter[FFraction] == 'S' && !best.matched.has(FFraction)) {
                std::string_view p = out.view();
                size_t at = p.rfind('s');
                if (at != std::string_view::npos) {
                    char buf[512];
                    size_t n = 0;
                    for (size_t i = 0; i <= at && n < sizeof buf; ++i) {
                        buf[n++] = p[i];
                    }
                    std::string_view dec = date_text(c.row[DlDecimal]);
                    for (char ch : dec) {
                        if (n < sizeof buf) {
                            buf[n++] = ch;
                        }
                    }
                    for (int k = 0; k < want.count[FFraction] && n < sizeof buf; ++k) {
                        buf[n++] = 'S';
                    }
                    for (size_t i = at + 1; i < p.size() && n < sizeof buf; ++i) {
                        buf[n++] = p[i];
                    }
                    out = string(std::string_view(buf, n));
                }
            }
            // a field still missing: the locale's appendItems, "{0} {1}"
            // or "{0} ({2}: {1})" with {2} the field's name, quoted
            static constexpr int8_t Append[FCount] = {0, 1, 2, 3, 4, 4, 5, 6, 6, 6, -1, 7, 8, 9, -1, 10};
            for (int f = 0; f < FCount; ++f) {
                if (!want.has(f) || best.matched.has(f) || Append[f] < 0) {
                    continue;
                }
                uint32_t arow = c.row[DlAppend] * AppendItemsWidth;
                std::string_view item = date_text(AppendItems[arow + uint32_t(Append[f])]);
                std::string_view field_name = date_text(AppendItems[arow + 11 + uint32_t(Append[f])]);
                if (item.empty()) {
                    item = "{0} {1}";
                }
                char buf[512];
                size_t n = 0;
                auto put = [&](std::string_view s) {
                    for (char ch : s) {
                        if (n < sizeof buf) {
                            buf[n++] = ch;
                        }
                    }
                };
                for (size_t i = 0; i < item.size();) {
                    if (item.substr(i, 3) == "{0}") {
                        put(out.view());
                        i += 3;
                    } else if (item.substr(i, 3) == "{1}") {
                        for (int k = 0; k < want.count[f]; ++k) {
                            put(std::string_view(&want.letter[f], 1));
                        }
                        i += 3;
                    } else if (item.substr(i, 3) == "{2}") {
                        put("'");
                        for (char ch : field_name) {
                            put(ch == '\'' ? std::string_view("''") : std::string_view(&ch, 1));
                        }
                        put("'");
                        i += 3;
                    } else if (pattern_letter(item[i]) || item[i] == '\'') {
                        // text of the item stands quoted in the pattern
                        put("'");
                        put(item[i] == '\'' ? std::string_view("''") : item.substr(i, 1));
                        put("'");
                        ++i;
                    } else {
                        put(item.substr(i, 1));
                        ++i;
                    }
                }
                out = string(std::string_view(buf, n));
            }
            return out;
        }

        inline Fields fields_of(const datetime& t, const string& zone_name) noexcept {
            Fields f;
            time::date d = t.date();
            f.year = d.year();
            f.month = int(d.month());
            f.day = d.day();
            f.weekday = int(d.weekday());
            f.year_day = d.year_day();
            f.hour = t.hour();
            f.minute = t.minute();
            f.second = t.second();
            f.nanos = t.nanosecond();
            f.offset = int32_t(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(t.offset())).count());
            f.zone_name = zone_name.view();
            f.has_zone = true;
            f.dst = t.is_dst();
            // a zone that keeps daylight saving time near the instant has a
            // generic name of its own; one that does not is named by its
            // standard name (TR35 Part 4 §7)
            time::zone z = t.zone();
            constexpr int64_t half_year = int64_t(184) * 86400;
            for (auto change : {z.next_transition(t), z.previous_transition(t)}) {
                if (change && (change->unix() - t.unix() < half_year && t.unix() - change->unix() < half_year)) {
                    f.dst_near = f.dst_near || change->is_dst() != f.dst || f.dst;
                }
            }
            f.dst_near = f.dst_near || f.dst;
            return f;
        }

        inline Fields fields_of(const time::date& d) noexcept {
            Fields f;
            f.year = d.year();
            f.month = int(d.month());
            f.day = d.day();
            f.weekday = int(d.weekday());
            f.year_day = d.year_day();
            return f;
        }

        inline string render(const Calendar& c, const Fields& f, std::string_view pattern) {
            char room[256];
            Out out{room, room + sizeof room};
            put_pattern(out, c, f, pattern);
            if (out.size <= sizeof room) {
                return string(std::string_view(room, out.size));
            }
            size_t n = out.size;
            return sgcl::detail::StringAccess::bounded<string>(n, [&](char* chars) {
                Out again{chars, chars + n};
                put_pattern(again, c, f, pattern);
                return again.size < n ? again.size : n;
            });
        }

        // The pattern of the styles: the date's, the time's, or both
        // joined by the date length's pattern (its atTime form when there
        // is one, as ICU writes "… at 2:05 PM")
        inline string pattern_of_styles(const Calendar& c, style date, style time) {
            auto length = [](style s) {
                return uint32_t(4 - int(s));   // full 0, long 1, medium 2, short 3
            };
            if (date == style::none && time == style::none) {
                return string();
            }
            if (time == style::none) {
                return string(date_text(c.row[DlDate + length(date)]));
            }
            if (date == style::none) {
                return string(date_text(c.row[DlTime + length(time)]));
            }
            string out;
            join(out, glue_of(c, length(date)), string(date_text(c.row[DlDate + length(date)])),
                 string(date_text(c.row[DlTime + length(time)])));
            return out;
        }
    }

    namespace detail::cldr {
        //----------------------------------------------------------------
        // intervals (TR35 Part 4 §2.6.3)
        //----------------------------------------------------------------
        // The greatest field two times differ in: G y M d a h m s, or -1
        // when they are the same to the second
        enum : int { GEra, GYear, GMonth, GDay, GPeriod, GHour, GMinute, GSecond };

        inline int greatest_difference(const Fields& a, const Fields& b) noexcept {
            if ((a.year > 0) != (b.year > 0)) {
                return GEra;
            }
            if (a.year != b.year) {
                return GYear;
            }
            if (a.month != b.month) {
                return GMonth;
            }
            if (a.day != b.day) {
                return GDay;
            }
            if ((a.hour < 12) != (b.hour < 12)) {
                return GPeriod;
            }
            if (a.hour != b.hour) {
                return GHour;
            }
            if (a.minute != b.minute) {
                return GMinute;
            }
            if (a.second != b.second || a.nanos != b.nanos) {
                return GSecond;
            }
            return -1;
        }

        // The pattern written with the first time up to the first field
        // that repeats one already written, and the rest with the second
        inline size_t interval_split(std::string_view p) noexcept {
            bool seen[FCount] = {};
            bool quoted = false;
            for (size_t i = 0; i < p.size();) {
                char ch = p[i];
                if (ch == '\'') {
                    quoted = !quoted;
                    ++i;
                    continue;
                }
                size_t j = i + 1;
                while (j < p.size() && p[j] == ch) {
                    ++j;
                }
                int f = quoted ? -1 : field_of(ch);
                if (f >= 0) {
                    if (seen[f]) {
                        return i;
                    }
                    seen[f] = true;
                }
                i = j;
            }
            return p.size();
        }

        struct IntervalMatch {
            Skeleton matched;
            int distance = 1 << 30;
            uint32_t skeleton = UINT32_MAX;
        };

        inline void best_interval_in_run(IntervalMatch& best, const Skeleton& want, const Calendar& c, uint32_t run) noexcept {
            uint32_t start = run >> 12, n = run & 0xFFF;
            for (uint32_t k = 0; k < n; ++k) {
                uint32_t e = DateIntervals[start + k];
                uint32_t sk = e >> 20;
                if (sk == best.skeleton) {
                    continue;
                }
                Skeleton have = read_skeleton(SkeletonTexts[sk], c);
                int missing;
                int d = skeleton_distance(want, have, missing);
                if (missing == 0 && d < ExtraField && d < best.distance) {
                    best.distance = d;
                    best.matched = have;
                    best.skeleton = sk;
                }
            }
        }

        // The interval pattern of a skeleton for a greatest difference:
        // the locale's own run first, then its base's
        inline std::string_view interval_pattern(const Calendar& c, uint32_t skeleton, int greatest) noexcept {
            for (uint32_t run : {DateIntervalRuns[c.index], DateBase[c.index] ? DateIntervalRuns[DateBase[c.index]] : 0u}) {
                uint32_t start = run >> 12, n = run & 0xFFF;
                for (uint32_t k = 0; k < n; ++k) {
                    uint32_t e = DateIntervals[start + k];
                    if ((e >> 20) == skeleton && int((e >> 16) & 15) == greatest) {
                        return date_text(e & 0xFFFF);
                    }
                }
            }
            return {};
        }

        inline string interval_fallback(const Calendar& c, const string& a, const string& b) {
            std::string_view fb = date_text(c.row[DlIntervalFallback]);
            char buf[1024];
            size_t n = 0;
            auto put = [&](std::string_view s) {
                for (char ch : s) {
                    if (n < sizeof buf) {
                        buf[n++] = ch;
                    }
                }
            };
            for (size_t i = 0; i < fb.size();) {
                if (fb.substr(i, 3) == "{0}") {
                    put(a.view());
                    i += 3;
                } else if (fb.substr(i, 3) == "{1}") {
                    put(b.view());
                    i += 3;
                } else {
                    put(fb.substr(i, 1));
                    ++i;
                }
            }
            return string(std::string_view(buf, n));
        }

        inline string interval(const Calendar& c, const Fields& a, const Fields& b, std::string_view skeleton_text) {
            Skeleton want = read_skeleton(skeleton_text, c);
            int g = greatest_difference(a, b);
            // the fields the skeleton shows, smallest last
            static constexpr int FieldOf[] = {FEra, FYear, FMonth, FDay, FPeriod, FHour, FMinute, FSecond};
            bool shown = false;
            for (int k = g; k >= 0 && k <= GSecond && !shown; ++k) {
                shown = want.has(FieldOf[k]) || (k == GPeriod && want.has(FHour));
            }
            if (g < 0 || !shown) {
                string p = pattern_of_skeleton(skeleton_text, c);
                return render(c, a, p.view());
            }
            bool date = want.any_date(), time = want.any_time();
            // a difference in a date field the skeleton lacks: the fields
            // from the greatest down added (MMMd over a change of the year
            // is yMMMd, d over one of the month Md, a time over a change of
            // the day the short date and the time), as ICU adds them
            char grown[64];
            size_t gn = 0;
            if (g <= GDay && !want.has(FieldOf[g]) && !want.has(FEra)) {
                for (char ch : skeleton_text) {
                    if (gn < sizeof grown - 8) {
                        grown[gn++] = ch;
                    }
                }
                // a date skeleton: the fields from the greatest difference
                // down to its largest one; a time skeleton: a whole date
                int largest = !date ? GDay + 1 : want.has(FYear) ? GYear : want.has(FMonth) ? GMonth : GDay;
                if (!want.has(FYear) && g <= GYear && GYear < largest) {
                    grown[gn++] = 'y';
                }
                if (!want.has(FMonth) && g <= GMonth && GMonth < largest) {
                    grown[gn++] = 'M';
                }
                if (!date) {
                    grown[gn++] = 'y';
                    grown[gn++] = 'M';
                    grown[gn++] = 'd';
                }
                skeleton_text = std::string_view(grown, gn);
                want = read_skeleton(skeleton_text, c);
                date = want.any_date();
            }
            if (date && time && g <= GDay) {
                string p = pattern_of_skeleton(skeleton_text, c);
                return interval_fallback(c, render(c, a, p.view()), render(c, b, p.view()));
            }
            Skeleton part_want = date && time ? part(want, false) : want;
            IntervalMatch best;
            best_interval_in_run(best, part_want, c, DateIntervalRuns[c.index]);
            if (DateBase[c.index]) {
                best_interval_in_run(best, part_want, c, DateIntervalRuns[DateBase[c.index]]);
            }
            std::string_view found;
            if (best.skeleton != UINT32_MAX) {
                int key = g;
                if ((g == GHour || g == GPeriod) && best.matched.letter[FPeriod] == 'B') {
                    // the flexible periods: B where noon is crossed (ICU
                    // compares the am/pm field and writes B for it)
                    found = g == GPeriod ? interval_pattern(c, best.skeleton, 5) : std::string_view();
                    if (found.empty()) {
                        found = interval_pattern(c, best.skeleton, 6);
                    }
                    key = -1;
                } else if (g == GHour || g == GPeriod) {
                    char hour = best.matched.letter[FHour];
                    // h and H patterns: 'a' where the period changes, 'h' or 'H' for the hour
                    int hk = hour == 'H' || hour == 'k' ? 7 : 6;
                    found = g == GPeriod ? interval_pattern(c, best.skeleton, 4) : std::string_view();
                    if (found.empty()) {
                        found = interval_pattern(c, best.skeleton, hk);
                    }
                    key = -1;
                } else if (g == GMinute) {
                    key = 8;
                } else if (g == GSecond) {
                    key = -2;
                } else {
                    key = g;   // G y M d: their places in the table (0..3)
                }
                if (key >= 0) {
                    found = interval_pattern(c, best.skeleton, key);
                }
            }
            if (found.empty()) {
                string p = pattern_of_skeleton(skeleton_text, c);
                return interval_fallback(c, render(c, a, p.view()), render(c, b, p.view()));
            }
            string adjusted;
            adjust(adjusted, found, part_want, best.matched);
            size_t split = interval_split(adjusted.view());
            string first = render(c, a, adjusted.view().substr(0, split));
            string second = render(c, b, adjusted.view().substr(split));
            char buf[1024];
            size_t n = 0;
            for (char ch : first.view()) {
                if (n < sizeof buf) {
                    buf[n++] = ch;
                }
            }
            for (char ch : second.view()) {
                if (n < sizeof buf) {
                    buf[n++] = ch;
                }
            }
            string times(std::string_view(buf, n));
            if (!(date && time)) {
                return times;
            }
            // the same day, a time interval: the date once, joined to it
            Skeleton d = part(want, true);
            Match bd = best_pattern(d, c);
            string ds;
            adjust(ds, bd.pattern, d, bd.matched);
            int length = 3;
            if (d.has(FMonth) && field_value(d.letter[FMonth], d.count[FMonth]) >= 0x100) {
                length = d.count[FMonth] == 4 ? (d.has(FWeekday) ? 0 : 1) : 2;
            }
            std::string_view glue = glue_of(c, uint32_t(length));
            string date_text_ = render(c, a, ds.view());
            n = 0;
            for (size_t i = 0; i < glue.size();) {
                std::string_view piece;
                if (glue.substr(i, 3) == "{0}") {
                    piece = times.view();
                    i += 3;
                } else if (glue.substr(i, 3) == "{1}") {
                    piece = date_text_.view();
                    i += 3;
                } else if (glue[i] == '\'') {
                    size_t j = glue.find('\'', i + 1);
                    piece = glue.substr(i + 1, (j == std::string_view::npos ? glue.size() : j) - i - 1);
                    i = j == std::string_view::npos ? glue.size() : j + 1;
                } else {
                    piece = glue.substr(i, 1);
                    ++i;
                }
                for (char ch : piece) {
                    if (n < sizeof buf) {
                        buf[n++] = ch;
                    }
                }
            }
            return string(std::string_view(buf, n));
        }
    }

    // A locale's way of writing dates and times, resolved once: a pattern
    // of CLDR's letters ("d MMMM y, HH:mm") and the locale's names and
    // digits. It holds the pattern as a string.
    class date_format {
    public:
        explicit date_format(const txt::locale& l, style date = style::medium, style time = style::none)
        : _calendar(detail::cldr::calendar_of(l))
        , _pattern(detail::cldr::pattern_of_styles(_calendar, date, time)) {
        }

        // The locale's pattern for the fields of a skeleton, in any order:
        // "yMMMd" is "d MMM y" in Polish and "MMM d, y" in English; "j" is
        // the locale's hour, 12 or 24
        static date_format from_skeleton(const txt::locale& l, const string& skeleton) {
            date_format f(l, style::none, style::none);
            f._pattern = detail::cldr::pattern_of_skeleton(skeleton.view(), f._calendar);
            return f;
        }

        // A pattern of CLDR's letters, as it is: "EEEE, d MMMM y"
        static date_format from_pattern(const txt::locale& l, const string& pattern) {
            date_format f(l, style::none, style::none);
            f._pattern = pattern;
            return f;
        }

        string format(const datetime& t) const {
            string zone_name = t.zone().name();
            return detail::cldr::render(_calendar, detail::cldr::fields_of(t, zone_name), _pattern.view());
        }

        // A date, at its midnight in UTC where the pattern asks for a time
        string format(const date& d) const {
            return detail::cldr::render(_calendar, detail::cldr::fields_of(d), _pattern.view());
        }

        // The pattern resolved: "d MMM y, HH:mm"
        const string& pattern() const noexcept {
            return _pattern;
        }

    private:
        detail::cldr::Calendar _calendar;
        string _pattern;
    };

    inline string datetime::format(const txt::locale& l, style date, style time) const {
        return date_format(l, date, time).format(*this);
    }

    inline string datetime::format(const txt::locale& l, const string& skeleton) const {
        return date_format::from_skeleton(l, skeleton).format(*this);
    }

    inline string date::format(const txt::locale& l, style date) const {
        return date_format(l, date, style::none).format(*this);
    }

    inline string date::format(const txt::locale& l, const string& skeleton) const {
        return date_format::from_skeleton(l, skeleton).format(*this);
    }

    // Two times as an interval in the locale's patterns for a skeleton:
    // "24–26 wrz 2026" for yMMMd, "14:05–16:30" for Hm; the second time is
    // seen in the first's zone. Times the skeleton cannot tell apart are
    // written once; a difference in a field the skeleton lacks (a change of
    // the year under MMMd) writes both in full with that field.
    inline string datetime::format_interval(const datetime& to, const txt::locale& l, const string& skeleton) const {
        using namespace detail::cldr;
        Calendar c = calendar_of(l);
        string zone_name = zone().name();
        datetime second = to.in(zone());
        return detail::cldr::interval(c, fields_of(*this, zone_name), fields_of(second, zone_name), skeleton.view());
    }

    // The name of a month in a locale: "września" (the form inside a
    // date) or "wrzesień" (stand-alone) in Polish; beside to_string(m),
    // the English name
    inline string to_string(month m, const txt::locale& l, txt::width w = txt::width::wide,
                            txt::name_context context = txt::name_context::format) {
        using namespace detail::cldr;
        Calendar c = calendar_of(l);
        int k = int(m) - 1;
        if (k < 0 || k > 11) {
            return string();
        }
        uint32_t row = c.row[DlMonths + (context == txt::name_context::standalone ? 3u : 0u) + uint32_t(w)];
        return string(name(MonthNames, MonthNamesWidth, row, k));
    }

    // The name of a day of the week in a locale: "czwartek", "czw.", "C"
    inline string to_string(weekday d, const txt::locale& l, txt::width w = txt::width::wide,
                            txt::name_context context = txt::name_context::format) {
        using namespace detail::cldr;
        Calendar c = calendar_of(l);
        int k = int(d) - 1;
        if (k < 0 || k > 6) {
            return string();
        }
        uint32_t width = w == txt::width::wide ? 0 : w == txt::width::abbreviated ? 1 : 3;
        uint32_t row = c.row[DlDays + (context == txt::name_context::standalone ? 4u : 0u) + width];
        return string(name(DayNames, DayNamesWidth, row, k));
    }

    namespace detail {
        // The date and time arguments of txt::message_format: a pattern
        // resolved once from ICU's style names (short, medium, long, full),
        // a skeleton after "::" or a pattern of the message's own, then a
        // value — milliseconds since 1970, in UTC, or RFC 3339 text with
        // its offset — written in it. Registered by inclusion, as the
        // display names are: txt does not depend on time.
        inline std::string message_date_resolve(const txt::locale& l, bool time, int k, std::string_view text) {
            if (k >= 0 && k <= 3) {
                constexpr style styles[] = {style::brief, style::medium, style::detailed, style::full};
                date_format f = time ? date_format(l, style::none, styles[k]) : date_format(l, styles[k]);
                return std::string(f.pattern().view());
            }
            if (text.size() > 2 && text.substr(0, 2) == "::") {
                return std::string(date_format::from_skeleton(l, string(text.substr(2))).pattern().view());
            }
            return std::string(text);
        }

        inline bool message_date_write(std::string& out, const txt::value& v, const txt::locale& l,
                                       std::string_view pattern) {
            datetime t;
            if (v.kind() == txt::value_kind::integer || v.kind() == txt::value_kind::real) {
                double ms = v.kind() == txt::value_kind::integer ? double(*txt::detail::value_reach::integer_of(v))
                                                                 : *txt::detail::value_reach::real_of(v);
                if (!(ms > -9.2e15 && ms < 9.2e15)) {
                    return false;
                }
                int64_t whole = int64_t(ms);
                if (double(whole) > ms) {
                    --whole;   // the millisecond the instant is in
                }
                t = datetime::from_unix_milli(whole, zone::utc());
            } else if (const string* text = v.text()) {
                auto parsed = datetime::parse(*text, rfc3339);
                if (!parsed) {
                    return false;
                }
                t = *parsed;
            } else {
                return false;
            }
            out.append(date_format::from_pattern(l, string(pattern)).format(t).view());
            return true;
        }

        inline constexpr txt::detail::MessageDates MessageDateHooks{&message_date_resolve, &message_date_write};

        struct MessageDateRegistration {
            MessageDateRegistration() noexcept {
                txt::detail::message_dates().store(&MessageDateHooks, std::memory_order_release);
            }
        };

        inline MessageDateRegistration message_date_registration;
    }
}
