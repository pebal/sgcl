[sgcl](../README.md) › [core](README.md)

# sgcl::duration

```cpp
#include "sgcl/core/duration.h"   // or "sgcl/core.h"

namespace sgcl {
    class duration;
    class duration_error;

    inline constexpr duration nanosecond = std::chrono::nanoseconds(1);
    inline constexpr duration microsecond = std::chrono::microseconds(1);
    inline constexpr duration millisecond = std::chrono::milliseconds(1);
    inline constexpr duration second = std::chrono::seconds(1);
    inline constexpr duration minute = std::chrono::minutes(1);
    inline constexpr duration hour = std::chrono::hours(1);
}
```

`sgcl::duration` is a span of time, what Go's `time.Duration` is: a signed count of nanoseconds in 64 bits, some
292 years either way. It reads in the units a person thinks in (`d.seconds()` is `1.5`, not `1500000000`), it is
written and read as Go writes and reads one (`"1h30m"`, `"1.5µs"`, `"-2m0.5s"`), and the constants `nanosecond` to
`hour` make one: `90 * second`, `2 * hour + 30 * minute`.

It is the library's one duration: what a [sleep, a timer, a timeout](../async/README.md#time) and a
[manual clock](../async/manual_clock.md) take, what a [stopwatch](../time/stopwatch.md) gives, and what the
[time](../time/README.md) module measures with. It stands next to the types of `<chrono>` without being one of
them. Every `std::chrono::duration` of an integral count in a whole number of nanoseconds converts into it
implicitly and exactly (`1h`, `30min`, `500ms`, a `std::chrono::seconds` from elsewhere), and it converts
implicitly into `std::chrono::nanoseconds`, so a function whose parameter is exactly that type takes a duration as
it is. A point of any clock plus or minus a duration is that clock's point (`steady_clock::now() + d`), and a
duration compared with, added to or subtracted from a `std::chrono` one is a `duration` (`d + 500ms`, `d < 5s`). A
duration of a floating count converts only explicitly, truncated toward zero as `duration_cast` does
(`duration(std::chrono::duration<double>(1.5))`); one of a unit finer than a nanosecond does not convert at all,
and takes a `duration_cast<std::chrono::nanoseconds>` first. A function template of the standard that deduces its
duration from the argument — `duration_cast`, `floor`, `round`, `async::condition_variable::wait_for`,
`this_thread::sleep_for`, `hh_mm_ss`, `std::format` — does not look through a class: it takes
`std::chrono::nanoseconds(d)`.

The text is Go's, both ways ([to_string](duration/to_string.md), [parse](duration/parse.md)); a text that is not a
duration is a [duration_error](duration_error.md) with a sentence and the byte the reading stopped on. Arithmetic
saturates at the ends of the range instead of wrapping: a sum, a difference, a product, a negation, a conversion
that would not fit is the largest or the smallest duration, `duration::max()` or `duration::min()` (Go wraps
silently; `std::chrono` overflows as its integer does, which is undefined). So does a point moved by a duration,
at its type's `max()` and `min()`: `now() + duration::max()` is `time_point::max()`, and the
[timers](../async/README.md#time) never fire at that point, so `async::sleep(duration::max())`,
`async::after(duration::max())` and a `timeout` of it mean never, as a Go timer's `when()` cuts the same way.

## Rules

- A duration is a plain value of eight bytes, trivially copyable, `constexpr` throughout but for `to_string` and
  `parse`: it lives anywhere.
- `seconds()`, `minutes()` and `hours()` are `double`, with the fraction; `nanoseconds()`, `microseconds()` and
  `milliseconds()` are whole `int64_t`, the last two truncated toward zero. The names are Go's
  (`Seconds() float64`, `Milliseconds() int64`), and so are the types.
- A duration times or divided by an integer is a duration; a duration divided by a duration is an `int64_t`, how
  many whole times the second fits in the first; `%` leaves the rest with the sign of the first, as for an `int`. A
  division by zero is a division by zero.
- A duration never becomes a bare number implicitly and never comes from one: `duration(5)` does not compile,
  `5 * second` does.
- A text the program itself writes is constructed, `duration d("1h30m")`, and a wrong one throws
  `bad_expected_access<duration_error>` with `parse`'s message; a text from outside (a setting, the user) is
  parsed, and its error is a value.
- `truncate(step)` goes toward zero to a multiple of `step`, `round(step)` to the nearest, a half away from zero; a
  step of zero or less leaves the duration as it is (Go's `Truncate` and `Round`).
- `abs()` of the smallest duration, which has no positive counterpart, is the largest.
- `operator<<` writes `to_string()`.

### From code written for `<chrono>`

| With a `std::chrono` duration | With `sgcl::duration` |
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

## Member functions

| Function | Description |
|---|---|
| [(constructor)](duration/duration.md) | constructs a duration: zero, from a `std::chrono` duration, from Go's text |
| [operator std::chrono::nanoseconds](duration/operator_conv.md) | converts to the standard's nanoseconds |

#### Units

| Function | Description |
|---|---|
| [nanoseconds](duration/nanoseconds.md) | the whole nanoseconds |
| [microseconds](duration/microseconds.md) | the whole microseconds, toward zero |
| [milliseconds](duration/milliseconds.md) | the whole milliseconds, toward zero |
| [seconds](duration/seconds.md) | the seconds, with the fraction |
| [minutes](duration/minutes.md) | the minutes, with the fraction |
| [hours](duration/hours.md) | the hours, with the fraction |

#### Rounding

| Function | Description |
|---|---|
| [abs](duration/abs.md) | the absolute value |
| [truncate](duration/truncate.md) | toward zero to a multiple of a step |
| [round](duration/round.md) | to the nearest multiple of a step |

#### Text

| Function | Description |
|---|---|
| [to_string](duration/to_string.md) | Go's text of the duration |
| [parse](duration/parse.md) | reads Go's text (static) |

#### Special values

| Function | Description |
|---|---|
| [zero](duration/zero.md) | the zero duration (static) |
| [max](duration/max.md) | the largest duration, some 292 years (static) |
| [min](duration/min.md) | the smallest duration, some 292 years back (static) |

#### Arithmetic

| Function | Description |
|---|---|
| [operator+=, operator-=, operator\*=, operator/=, operator%=](duration/operator_arith.md) | the compound assignments, saturated |

## Non-member functions

| Function | Description |
|---|---|
| [operator+, operator-, operator\*, operator/, operator%](duration/operator_arith.md) | the arithmetic of durations and of a point moved by a duration, saturated |
| [operator==, operator\<=\>](duration/operator_cmp.md) | compare two durations |
| [operator\<\<](duration/to_string.md) | writes `to_string()` to a stream |

#### Constants

| Constant | Value | Description |
|---|---|---|
| `nanosecond` | `std::chrono::nanoseconds(1)` | one nanosecond, `inline constexpr duration` |
| `microsecond` | `std::chrono::microseconds(1)` | one microsecond |
| `millisecond` | `std::chrono::milliseconds(1)` | one millisecond |
| `second` | `std::chrono::seconds(1)` | one second |
| `minute` | `std::chrono::minutes(1)` | one minute |
| `hour` | `std::chrono::hours(1)` | one hour |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <chrono>

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    duration d("1h30m");  // a literal: constructed
    println("{} {}", d, d.minutes());

    duration lap = 1500ms;  // a literal of <chrono>
    println("{} {} {}", lap, lap.seconds(), lap.milliseconds());

    println("{} {} {}", d + 90 * second, d / 4, d / lap);
    duration small = 1234567ns;
    println("{} {} {}", small, small.round(millisecond), small.truncate(millisecond));
    duration back = -1500us;
    println("{} {}", back, back.abs());

    auto bad = duration::parse("1d");  // a text that may be wrong: parsed
    println("{} (byte {})", bad.error().message(), bad.error().offset());

    duration total;
    for (int i : range(1, 5)) {
        total += i * 250 * millisecond;
    }
    println(total);

    auto deadline = std::chrono::steady_clock::now() + total;  // a point of any clock
    std::chrono::nanoseconds n = total;  // into the standard's type
    println("{} {}", deadline > std::chrono::steady_clock::now(), n.count());
}
```

Output:

```text
1h30m0s 90
1.5s 1.5 1500
1h31m30s 22m30s 3600
1.234567ms 1ms 1ms
-1.5ms 1.5ms
an unknown unit: ns, us, ms, s, m or h expected (byte 1)
2.5s
true 2500000000
```

## See also

- [duration_error](duration_error.md): why a text is not a duration
- [sleep, after, tick, timeout](../async/README.md#time), [clock](clock.md), [manual_clock](../async/manual_clock.md):
  what takes a duration
- [time](../time/README.md): dates, zones, instants in a zone, the stopwatch
- [txt::format](../txt/format.md): `txt::format("{}", d)` writes a duration as `to_string()` does, in a field of
  any width (`{:>12}`), with nothing but txt included
- [expected](expected.md): what `parse` returns
