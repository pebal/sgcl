# sgcl::time::datetime, sgcl::time::now

```cpp
#include "sgcl/time/datetime.h"   // or "sgcl/time/time.h"

namespace sgcl::time {
    class datetime;                                // an instant and the zone it is seen in
    datetime now();                                // the system's clock, the local zone
    inline constexpr earlier_t earlier;            // tags: which of a time shown twice
    inline constexpr later_t later;
}
```

An instant on the time line and the zone it is seen in: Go's `time.Time`. The instant is the nanoseconds since 1970-01-01T00:00:00Z in 64 bits — the years 1677 to 2262, the range and the unit of `std::chrono::system_clock` and of [`io::file_info::modified`](../io/fs.md), which a datetime is made from as it is — and the zone is a [zone](zone.md). Sixteen bytes, the zone a `tracked_ptr` to its data, so a datetime lives where a [string](../core/string.md) does — on a stack, in a managed object, in a container of the library — and is taken as `const datetime&`; Go's is 24, with a monotonic reading inside that this module keeps in a [stopwatch](README.md#stopwatch) of its own.

The fields — `year()` to `nanosecond()`, `weekday()`, `year_day()`, `iso_week()`, `date()` — are what the zone's clock shows at that instant. `t.in(z)` is the same instant in another zone, `t.utc()` and `t.local()` in UTC and the local one; `t.offset()`, `t.abbreviation()` and `t.is_dst()` are what the zone is then.

Two datetimes are equal when they are the same instant, whatever their zones: the same moment in Warsaw and in UTC is equal, and ordered by the instant. Go's `==` compares the zone too, which is a known trap there (`t.Equal(u)` is what Go code means); here `a.zone() == b.zone()` asks the other question.

Two kinds of arithmetic. The exact one is `t + d`, `t - d` and `t2 - t1`, a [duration](../core/duration.md) of nanoseconds: an hour later is 3600 seconds later. The calendar's is `add_days`, `add_months` and `add_years`, which keep the time of the clock: a day later across a change of the clock is 23 or 25 hours later, and a month later from the 31st is the month's last day, as [date](date.md)'s `add_months` cuts it. Both saturate at the ends of the range; Go wraps.

A time of the clock becomes an instant by `date.at(hour, minute, zone)` (Go's `time.Date`), and a change of the clock makes some times of it two instants or none. `at` reads them by the rule Java, JavaScript's Temporal and iCalendar (RFC 5545) call compatible: a time the clock skipped (the hour lost in spring) moves on by the length of the skip, 02:30 on the night Warsaw goes from 02:00 to 03:00 being 03:30; a time it showed twice (the hour repeated in autumn) is the first of the two. The tag `time::earlier` takes the first of a time shown twice and, for a skipped time, the change itself (03:00, the first time of the clock after the skip); `time::later` takes the second and moves a skipped time on; `try_at` is nothing for both. Go does not say which it takes ("not guaranteed"), and takes either by where the time falls against the change read as UTC.

`time::now()` reads the system's clock, in the local zone. While a test has a [manual_clock](../async/manual_clock.md) installed it is the wall time of the install moved on by as much as the manual time has been advanced, so that code which asks for the time — an expiry, a header of HTTP, a log's rotation — is tested with no real waiting, as the timers are.

## Rules

- The range is 1677-09-21T00:12:43.145224192Z to 2262-04-11T23:47:16.854775807Z. A date beyond it (`date(3000, 1, 1).at(0, 0, z)`), a sum or a difference past it, and `from_unix` of more seconds than it holds give its end; a text of an instant beyond it is refused by `parse`.
- `unix()`, `unix_milli()`, `unix_micro()` and `nanosecond()` round down: half a second before 1970 is `unix() == -1` and `nanosecond() == 500000000`, 1969-12-31T23:59:59.5Z.
- A datetime made without a zone (`from_unix(s)`, `datetime(sys_time)`) is in the local zone, as in Go; `datetime()` is 1970-01-01T00:00:00Z in UTC.
- `truncate(d)` and `round(d)` count steps from Go's zero time, 0001-01-01T00:00:00Z, as Go does — for a step that divides a day the same as counting from midnight UTC, not from the zone's midnight. Half a step rounds up. A step of zero or less leaves the time as it is.
- `start_of_day()` is the first instant of the datetime's date in its zone: midnight, or where midnight was skipped (America/Santiago in September) the change that skipped it; a date skipped whole (Pacific/Apia's 2011-12-30) starts where the next one does.
- Hours, minutes and seconds out of their ranges carry, as a date's fields do: `d.at(24, 0, z)` is midnight of the next day, `d.at(0, 0, 86400, z)` too.
- `to_string()` is RFC 3339 with the fraction of a second only where there is one and `Z` for an offset of zero, Go's `RFC3339Nano`: `2026-09-24T12:41:15.122575+02:00`. An offset with seconds (the local mean times of the 19th century) is written to the minute, as Go writes it; RFC 3339 has no seconds there. The other texts are on the [text](layout.md) page.

## Members

```cpp
constexpr datetime() noexcept;                                          // 1970-01-01T00:00:00Z, UTC
explicit datetime(std::chrono::sys_time<std::chrono::nanoseconds> t, const time::zone& z = time::zone::local()) noexcept;
static datetime from_unix(int64_t seconds, const time::zone& z = time::zone::local()) noexcept;
static datetime from_unix_milli(int64_t milliseconds, const time::zone& z = time::zone::local()) noexcept;
static datetime from_unix_micro(int64_t microseconds, const time::zone& z = time::zone::local()) noexcept;
static datetime from_unix_nano(int64_t nanoseconds, const time::zone& z = time::zone::local()) noexcept;
static expected<datetime, error> parse(const string& text, layout format);                   // layout.md
static expected<datetime, error> parse(const string& text, const string& pattern, const time::zone& z = time::zone::utc());

int64_t unix() const noexcept;                   // seconds since 1970, rounded down
int64_t unix_milli() const noexcept;
int64_t unix_micro() const noexcept;
int64_t unix_nano() const noexcept;
std::chrono::sys_time<std::chrono::nanoseconds> to_sys() const noexcept;

time::zone zone() const noexcept;
datetime in(const time::zone& z) const noexcept;        // the same instant in another zone
datetime utc() const noexcept;
datetime local() const;
duration offset() const noexcept;                // +2h
string abbreviation() const;                     // "CEST"
bool is_dst() const noexcept;

time::date date() const noexcept;                // of the zone's clock
int year() const noexcept;
time::month month() const noexcept;              // january to december; int(...) 1 to 12
int day() const noexcept;                        // 1 to 31
int hour() const noexcept;                       // 0 to 23
int minute() const noexcept;
int second() const noexcept;
int nanosecond() const noexcept;                 // 0 to 999999999
time::weekday weekday() const noexcept;          // Monday 1 to Sunday 7
int year_day() const noexcept;                   // 1 to 366
time::iso_week iso_week() const noexcept;

datetime add_days(int n) const;                  // the same time of the clock n days on
datetime add_months(int n) const;                // cut to the month's end
datetime add_years(int n) const;
datetime truncate(duration step) const noexcept; // from Go's zero time
datetime round(duration step) const noexcept;
datetime start_of_day() const;

string format(layout format) const;              // t.format(time::http)
string format(const string& pattern) const;      // t.format("%d.%m.%Y %H:%M")
string to_string() const;                        // RFC 3339, the fraction where there is one

// t + d, d + t, t - d, t += d, t -= d; t2 - t1 -> duration; == and <=> by the instant; operator<<

// On a date (date.md):
datetime at(int hour, int minute, const zone& z) const;                       // compatible
datetime at(int hour, int minute, int second, const zone& z) const;
datetime at(int hour, int minute, const zone& z, earlier_t) const;            // and with seconds
datetime at(int hour, int minute, const zone& z, later_t) const;              // and with seconds
optional<datetime> try_at(int hour, int minute, int second, const zone& z) const;
datetime start_of_day(const zone& z) const;

datetime now();
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include "sgcl/time/time.h"
#include <iostream>

using namespace sgcl;

int main() {
    auto warsaw = time::zone::load("Europe/Warsaw").value();
    auto new_york = time::zone::load("America/New_York").value();

    // A meeting in New York, seen in Warsaw
    auto meeting = time::date(2026, 10, 30).at(9, 30, new_york);
    std::cout << meeting << " is " << meeting.in(warsaw).format("%A %H:%M") << " in Warsaw\n";
    std::cout << (meeting == meeting.utc()) << "\n";                 // the same instant

    // A day later is not always 24 hours later
    auto before = time::date(2026, 10, 24).at(12, 0, warsaw);
    std::cout << (before.add_days(1) - before) << " " << (before + 24 * hour) << "\n";

    // The hour skipped in spring and the one repeated in autumn
    std::cout << time::date(2026, 3, 29).at(2, 30, warsaw) << " "
              << time::date(2026, 3, 29).at(2, 30, warsaw, time::earlier) << "\n";
    std::cout << time::date(2026, 10, 25).at(2, 30, warsaw) << " "
              << time::date(2026, 10, 25).at(2, 30, warsaw, time::later) << " "
              << time::date(2026, 10, 25).try_at(2, 30, 0, warsaw).has_value() << "\n";

    // A month from the 31st, the start of a day, a step of time
    auto invoice = time::date(2026, 1, 31).at(10, 0, warsaw);
    std::cout << invoice.add_months(1) << " " << invoice.start_of_day() << "\n";
    auto t = time::datetime::from_unix_nano(1790246475122575000, warsaw);
    std::cout << t << " " << t.truncate(15 * minute) << " " << t.round(second) << "\n";

    // Before 1970 the division goes down
    auto half = time::datetime::from_unix_milli(-500, time::zone::utc());
    std::cout << half << " " << half.unix() << "\n";

    // The time now follows a test's clock
    async::manual_clock clock;
    clock.install();
    auto start = time::now();
    clock.advance(90 * minute);
    std::cout << (time::now() - start) << "\n";
    return 0;
}
```

Output:

```text
2026-10-30T09:30:00-04:00 is Friday 14:30 in Warsaw
1
25h0m0s 2026-10-25T11:00:00+01:00
2026-03-29T03:30:00+02:00 2026-03-29T03:00:00+02:00
2026-10-25T02:30:00+02:00 2026-10-25T02:30:00+01:00 0
2026-02-28T10:00:00+01:00 2026-01-31T00:00:00+01:00
2026-09-24T12:41:15.122575+02:00 2026-09-24T12:30:00+02:00 2026-09-24T12:41:15+02:00
1969-12-31T23:59:59.5Z -1
1h30m0s
```

## See also

- [zone](zone.md): the zones a datetime is seen in
- [text](layout.md): the formats known by name, patterns of `%`, `txt::format` and reading
- [date](date.md): a date with no time of day, and `at`
- [time](README.md): the module and the table of Go's names
