# sgcl::duration

```cpp
#include "sgcl/core/duration.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class duration;                                   // a span of time: nanoseconds in 64 bits, Go's text
    class duration_error;                             // why a text is not a duration: message(), offset()

    inline constexpr duration nanosecond, microsecond, millisecond, second, minute, hour;
}
```

A span of time, what Go's `time.Duration` is: a signed count of nanoseconds in 64 bits, some 292 years either way. It reads in the units a person thinks in (`d.seconds()` is `1.5`, not `1500000000`), it is written and read as Go writes and reads one (`"1h30m"`, `"1.5µs"`, `"-2m0.5s"`), and the constants `nanosecond` to `hour` make one: `90 * second`, `2 * hour + 30 * minute`.

It is the library's one duration: what a [sleep, a timer, a timeout](../async/timer.md) and a [manual clock](../async/manual_clock.md) take, what a [stopwatch](../time/README.md#stopwatch) gives, and what the [time](../time/README.md) module measures with. It stands next to the types of `<chrono>` without being one of them. Every `std::chrono::duration` of an integral count in a whole number of nanoseconds converts into it implicitly and exactly (`1h`, `30min`, `500ms`, a `std::chrono::seconds` from elsewhere), and it converts implicitly into `std::chrono::nanoseconds`, so a function whose parameter is exactly that type takes a duration as it is. A point of any clock plus or minus a duration is that clock's point (`steady_clock::now() + d`), and a duration compared with, added to or subtracted from a `std::chrono` one is a `duration` (`d + 500ms`, `d < 5s`). A duration of a floating count converts only explicitly, truncated toward zero as `duration_cast` does (`duration(std::chrono::duration<double>(1.5))`); one of a unit finer than a nanosecond does not convert at all, and takes a `duration_cast<std::chrono::nanoseconds>` first. A function template of the standard that deduces its duration from the argument — `duration_cast`, `floor`, `round`, `async::condition_variable::wait_for`, `this_thread::sleep_for`, `hh_mm_ss`, `std::format` — does not look through a class: it takes `std::chrono::nanoseconds(d)`.

The text is Go's, both ways. `to_string()` writes the hours, minutes and seconds with the seconds' fraction, the leading units that are zero left out (`"1h0m0s"`, `"2m3.5s"`), and a span under a second in the largest unit that keeps its first digit above zero (`"300ms"`, `"1.5µs"`, `"7ns"`); zero is `"0s"`. `parse` reads a sign, then one or more numbers, each with a unit (`ns`, `us` or `µs` in either spelling, `ms`, `s`, `m`, `h`) and a fraction or none (`"1.5h"`, `".5s"`), in any order and repeated: whatever Go's `time.ParseDuration` reads, and nothing else — no spaces, no days (a day of the calendar is 23, 24 or 25 hours: that is a [date's](../time/date.md) `add_days`). A fraction is taken exactly, however many digits it has, and cut to the nanosecond; Go multiplies it in `double` and may land a nanosecond off. A text that is not a duration is a `duration_error` with a sentence and the byte the reading stopped on.

Arithmetic saturates at the ends of the range instead of wrapping: a sum, a difference, a product, a negation, a conversion that would not fit is the largest or the smallest duration, `duration::max()` or `duration::min()` (Go wraps silently; `std::chrono` overflows as its integer does, which is undefined). So does a point moved by a duration, at its type's `max()` and `min()`: `now() + duration::max()` is `time_point::max()`, and the [timers](../async/timer.md) never fire at that point, so `async::sleep(duration::max())`, `async::after(duration::max())` and a `timeout` of it mean never, as a Go timer's when() cuts the same way. A duration is a plain value of eight bytes, trivially copyable, `constexpr` throughout but for `to_string` and `parse`: it lives anywhere.

## Rules

- `seconds()`, `minutes()` and `hours()` are `double`, with the fraction; `nanoseconds()`, `microseconds()` and `milliseconds()` are whole `int64_t`, the last two truncated toward zero. The names are Go's (`Seconds() float64`, `Milliseconds() int64`), and so are the types.
- A duration times or divided by an integer is a duration; a duration divided by a duration is an `int64_t`, how many whole times the second fits in the first; `%` leaves the rest with the sign of the first, as for an `int`. A division by zero is a division by zero.
- A duration never becomes a bare number implicitly and never comes from one: `duration(5)` does not compile, `5 * second` does.
- `truncate(step)` goes toward zero to a multiple of `step`, `round(step)` to the nearest, a half away from zero; a step of zero or less leaves the duration as it is (Go's `Truncate` and `Round`).
- `abs()` of the smallest duration, which has no positive counterpart, is the largest.
- `operator<<` writes `to_string()`.

## From code written for `<chrono>`

| with a `std::chrono` duration | with `sgcl::duration` |
|---|---|
| `d.count()` | `d.nanoseconds()` |
| `duration_cast<milliseconds>(d).count()` | `d.milliseconds()` (and `microseconds()`), toward zero |
| `duration<double>(d).count()` | `d.seconds()` (and `minutes()`, `hours()`) |
| `duration_cast<seconds>(d)`, `floor<seconds>(d)` | `d.truncate(second)` for a duration; `duration_cast<seconds>(std::chrono::nanoseconds(d))` for a `std::chrono::seconds` |
| `round<seconds>(d)` | `d.round(second)` (a half away from zero, where chrono rounds a half to even) |
| `abs(d)` | `d.abs()` |
| `d * 2`, `d / 2`, `d / d2`, `d % d2`, `d + 500ms`, `d < 5s` | the same |
| `d * 1.5` | does not compile: `duration(std::chrono::duration<double>(d.seconds() * 1.5))` |
| `nanoseconds::max()`, `zero()` | `duration::max()`, `duration::min()`, `duration::zero()` |
| `cv.wait_for(lock, d)`, `this_thread::sleep_for(d)`, `std::format("{}", d)` | pass `std::chrono::nanoseconds(d)`; the library's own `async::sleep(d)` takes `d` |
| `std::cout << d` (`1500000ns`) | `std::cout << d` writes Go's text (`1.5ms`) |

## Members

```cpp
constexpr duration() noexcept;                              // zero
template<class Rep, class Period>
constexpr duration(std::chrono::duration<Rep, Period> d) noexcept;            // implicit: an integral count of a whole number of nanoseconds, saturated
template<class Rep, class Period>
explicit constexpr duration(std::chrono::duration<Rep, Period> d) noexcept;   // a floating count: truncated toward zero, a NaN zero
constexpr operator std::chrono::nanoseconds() const noexcept;
static constexpr duration zero() noexcept;
static constexpr duration max() noexcept;                  // some 292 years: never, for a timer
static constexpr duration min() noexcept;

static expected<duration, duration_error> parse(const string& text);   // Go's text
string to_string() const;                                   // Go's text

constexpr int64_t nanoseconds() const noexcept;
constexpr int64_t microseconds() const noexcept;           // toward zero
constexpr int64_t milliseconds() const noexcept;           // toward zero
constexpr double seconds() const noexcept;                 // with the fraction
constexpr double minutes() const noexcept;
constexpr double hours() const noexcept;

constexpr duration abs() const noexcept;
constexpr duration truncate(duration step) const noexcept;
constexpr duration round(duration step) const noexcept;

// + - between durations (and std::chrono ones), unary + -, * and / by an
// integer, duration / duration -> int64_t, %, the compound assignments,
// == and <=>; a time_point of any clock + - a duration, saturated at the
// time_point's max() and min(); all saturated
```

`duration_error`:

```cpp
string message() const;                                     // "an unknown unit: ns, us, ms, s, m or h expected"
size_t offset() const noexcept;                             // the byte of the text the reading stopped on
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <chrono>
#include <iostream>

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    auto d = duration::parse("1h30m").value();
    std::cout << d << " " << d.minutes() << "\n";                   // 1h30m0s 90

    duration lap = 1500ms;                                          // a literal of <chrono>
    std::cout << lap << " " << lap.seconds() << " " << lap.milliseconds() << "\n";

    std::cout << d + 90 * second << " " << d / 4 << " " << d / lap << "\n";
    duration small = 1234567ns;
    std::cout << small << " " << small.round(millisecond) << " " << small.truncate(millisecond) << "\n";
    duration back = -1500us;
    std::cout << back << " " << back.abs() << "\n";

    auto bad = duration::parse("1d");
    std::cout << bad.error().message() << " (byte " << bad.error().offset() << ")\n";

    duration total;
    for (int i : range(1, 5)) {
        total += i * 250 * millisecond;
    }
    std::cout << total << "\n";                                     // 2.5s

    auto deadline = std::chrono::steady_clock::now() + total;      // a point of any clock
    std::chrono::nanoseconds n = total;                             // into the standard's type
    std::cout << (deadline > std::chrono::steady_clock::now()) << " " << n.count() << "\n";
    return 0;
}
```

The output:

```
1h30m0s 90
1.5s 1.5 1500
1h31m30s 22m30s 3600
1.234567ms 1ms 1ms
-1.5ms 1.5ms
an unknown unit: ns, us, ms, s, m or h expected (byte 1)
2.5s
1 2500000000
```

## See also

- [sleep, after, tick, timeout](../async/timer.md), [clock](clock.md), [manual_clock](../async/manual_clock.md): what takes a duration
- [time](../time/README.md): dates, zones, instants in a zone, the stopwatch
- [txt::format](../txt/format.md): `txt::format("{}", d)` writes a duration as `to_string()` does, in a field of any width (`{:>12}`), with nothing but txt included
- [expected](expected.md): what `parse` returns
