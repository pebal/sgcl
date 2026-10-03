[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::operator+=, operator-=, sgcl::time::operator+, operator- (sgcl::time::datetime)

```cpp
friend datetime operator+(const datetime& t, duration d) noexcept;           // (1)
friend datetime operator+(duration d, const datetime& t) noexcept;           // (2)
friend datetime operator-(const datetime& t, duration d) noexcept;           // (3)
friend duration operator-(const datetime& a, const datetime& b) noexcept;    // (4)
datetime& operator+=(duration d) noexcept;                                   // (5)
datetime& operator-=(duration d) noexcept;                                   // (6)
```

The exact arithmetic of instants, Go's `t.Add(d)` and `t.Sub(u)`: an hour later is 3600 seconds later, whatever the
zone's clock does meanwhile (the calendar's arithmetic, which keeps the time of the clock, is
[add_days](add_days.md) and the others). The operators other than the compound assignments are hidden friends,
found through a `datetime` argument; a `std::chrono` duration of whole nanoseconds converts to a
[duration](../../core/duration.md), so `t + 90min` is a datetime. A result past either end of the range is the end:
the arithmetic saturates, where Go's wraps.

1. The instant `d` later, in the zone of `t`.
2. The same, the duration first.
3. The instant `d` earlier, in the zone of `t`.
4. The duration from `b` to `a`, whatever their zones: negative when `a` is earlier.
5. `*this = *this + d`.
6. `*this = *this - d`.

## Parameters

| Parameter | Description |
|---|---|
| `t`, `a`, `b` | the datetimes |
| `d` | the duration added or subtracted |

## Return value

- (1–3) The datetime moved, saturated at the ends of the range.
- (4) The duration between the instants, saturated at `duration::max()` and `duration::min()`.
- (5–6) `*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include <chrono>

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    time::zone warsaw("Europe/Warsaw");
    auto before = time::date(2026, 10, 24).at(12, 0, warsaw);
    println("{} {}", before + 24 * hour, before - 90min);

    auto meeting = time::date(2026, 10, 30).at(9, 30, time::zone("America/New_York"));
    println(meeting - before);

    auto t = before;
    t += 30 * second;
    t -= 2 * hour;
    println(t);
    println(t + duration::max());  // saturated
}
```

Output:

```text
2026-10-25T11:00:00+01:00 2026-10-24T10:30:00+02:00
147h30m0s
2026-10-24T10:00:30+02:00
2262-04-12T01:47:16.854775807+02:00
```

## See also

- [add_days](add_days.md), [add_months](add_months.md), [add_years](add_years.md): the calendar's arithmetic
- [operator==, operator\<=\>](operator_cmp.md): the comparisons
- [duration](../../core/duration.md): what `t2 - t1` is
- [sgcl::time::datetime](../datetime.md)
