[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::at

```cpp
/*(1)*/ datetime at(int hour, int minute, const zone& z) const noexcept;
/*(2)*/ datetime at(int hour, int minute, int second, const zone& z) const noexcept;
/*(3)*/ datetime at(int hour, int minute, const zone& z, earlier_t) const noexcept;
/*(4)*/ datetime at(int hour, int minute, int second, const zone& z, earlier_t) const noexcept;
/*(5)*/ datetime at(int hour, int minute, const zone& z, later_t) const noexcept;
/*(6)*/ datetime at(int hour, int minute, int second, const zone& z, later_t) const noexcept;
```

The instant at which the clock of zone `z` shows this date and that time: Go's `time.Date`. A change of the clock
makes some times of it two instants or none, and the overloads differ in which they take.

- (1–2) By the rule Java, JavaScript's Temporal and iCalendar (RFC 5545) call compatible: a time the clock skipped
  (the hour lost to a change in spring) is moved on by the length of the skip — 02:30 on the night Warsaw goes from
  02:00 to 03:00 is 03:30 — and a time it showed twice (the hour repeated in autumn) is the first of the two.
- (3–4) With the tag [earlier](../earlier_t.md): the first of a time shown twice and, for a time skipped, the
  instant of the change, 03:00 in the example above, the first time of the clock after the skip.
- (5–6) With the tag [later](../earlier_t.md): the second of a time shown twice, and a skipped one moved on as in
  (1–2).

The overloads without `second` are at second 0. Hours, minutes and seconds out of their ranges carry, as the
date's own fields do: `at(24, 0, z)` is midnight of the next day, and so is `at(0, 0, 86400, z)`.
[try_at](try_at.md) is nothing for a time skipped or shown twice.

## Parameters

| Parameter | Description |
|---|---|
| `hour` | the hour of the clock, 0 to 23; carried when outside |
| `minute` | the minute, 0 to 59; carried when outside |
| `second` | the second, 0 to 59; carried when outside |
| `z` | the zone whose clock shows the time |

## Return value

The [datetime](../datetime.md) of that instant, in the zone `z`. An instant past either end of a datetime's range,
the years 1677 to 2262 (`date(3000, 1, 1).at(0, 0, z)`), is that end.

## Complexity

Logarithmic in the number of the zone's changes of the clock; constant for a fixed offset.

## Exceptions

None.

## Notes

Go does not say which instant `time.Date` takes for such a time ("not guaranteed"), and takes either by where the
time falls against the change read as UTC.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::zone warsaw("Europe/Warsaw");
    time::date spring(2026, 3, 29);  // 02:00 skipped to 03:00
    time::date autumn(2026, 10, 25);  // 03:00 back to 02:00
    println("{} {} {}", spring.at(2, 30, warsaw), spring.at(2, 30, warsaw, time::earlier),
            spring.at(2, 30, warsaw, time::later));
    println("{} {} {}", autumn.at(2, 30, warsaw), autumn.at(2, 30, warsaw, time::earlier),
            autumn.at(2, 30, warsaw, time::later));

    time::date d(2026, 9, 24);
    println("{} {}", d.at(9, 30, 15, warsaw), d.at(24, 0, warsaw));
    println("{}", time::date(3000, 1, 1).at(0, 0, time::zone::utc()));
}
```

Output:

```text
2026-03-29T03:30:00+02:00 2026-03-29T03:00:00+02:00 2026-03-29T03:30:00+02:00
2026-10-25T02:30:00+02:00 2026-10-25T02:30:00+02:00 2026-10-25T02:30:00+01:00
2026-09-24T09:30:15+02:00 2026-09-25T00:00:00+02:00
2262-04-11T23:47:16.854775807Z
```

## See also

- [try_at](try_at.md): the instant only when there is exactly one
- [start_of_day](start_of_day.md): the first instant of the date
- [earlier_t, later_t](../earlier_t.md): the tags
- [datetime](../datetime.md), [zone](../zone.md)
- [sgcl::time::date](../date.md)
