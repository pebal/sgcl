[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::recurrence

```cpp
#include "sgcl/encoding/recurrence.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class recurrence;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::recurrence` is a recurrence rule of iCalendar ([RFC 5545](https://www.rfc-editor.org/rfc/rfc5545)
§3.3.10), an RRULE's value: `FREQ=MONTHLY;BYDAY=-1FR;COUNT=12` is the last Friday of each month, twelve times.
Immutable, one word shared by copying. [parse](parse.md) reads one, [to_string](to_string.md) writes one, and
[occurrences](occurrences.md) expands it from a start, in the start's zone — the times of a meeting, of a reminder, of
a backup schedule.

## Rules

- **The instances** are the start, then the times the rule makes after it; COUNT counts from the start, UNTIL ends
  them (in UTC with a `Z`, else the start zone's clock).
- **The BY parts**: BYMONTH, BYWEEKNO, BYYEARDAY, BYMONTHDAY and BYDAY add days to a period larger than a day and
  keep those of a smaller one; BYHOUR, BYMINUTE and BYSECOND add times or keep them likewise. A part the rule does
  not give comes from the start (a MONTHLY rule without days falls on the start's day of the month). BYDAY's number
  counts in the month for MONTHLY and for YEARLY with BYMONTH, in the year otherwise; the period of a YEARLY rule
  with BYWEEKNO is its week-year (weeks starting on WKST, week 1 the first with four days of the year). BYSETPOS
  picks from a period's times in order. A date the calendar has not (February 30) is no instance.
- **A time the zone skips** moves on by the skip, one it shows twice is the first (RFC 5545 §3.3.5).
- **What is refused**: the parts RFC 5545 forbids for a frequency (BYWEEKNO but in YEARLY, BYYEARDAY in DAILY,
  WEEKLY and MONTHLY, BYMONTHDAY in WEEKLY, a numbered BYDAY but in MONTHLY and YEARLY or with BYWEEKNO), COUNT with
  UNTIL, BYSETPOS without another BY part, a part twice, a value out of its range.
- **The oracle**: the examples of RFC 5545 §3.8.5.3 with the instances it lists, and a second implementation written
  for the tests in another way (every day of a period tried against every part, Python's zoneinfo for the zones) on
  600 random rules from random starts in three zones.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `frequency` | FREQ: [recurrence::frequency](../recurrence-frequency.md) |
| `weekday_rule` | a day of BYDAY: [recurrence::weekday_rule](../recurrence-weekday_rule.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](recurrence.md) | FREQ=DAILY, or the rule of a text written in the program |
| [parse](parse.md) | a rule's text (static) |
| [to_string](to_string.md) | the rule's text |
| [occurrences](occurrences.md) | the instances from a start |

#### Observers

| Function | Description |
|---|---|
| [freq, interval, count, until](freq.md) | how often, how many, how long |
| [by_month, by_day, ...](by_month.md) | the BY parts and WKST |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | the same parts with the same values |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/time.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto warsaw = time::zone::load("Europe/Warsaw").value();
    auto start = time::date(2026, 1, 30).at(18, 0, warsaw);
    encoding::recurrence last_friday("FREQ=MONTHLY;BYDAY=-1FR;COUNT=4");
    for (const auto& t : last_friday.occurrences(start, start, time::date(2027, 1, 1).at(0, 0, warsaw))) {
        println(t.to_string());
    }
}
```

Output:

```text
2026-01-30T18:00:00+01:00
2026-02-27T18:00:00+01:00
2026-03-27T18:00:00+01:00
2026-04-24T18:00:00+02:00
```

## See also

- [icalendar::occurrences](../icalendar/occurrences.md): a calendar's events with their RDATEs and EXDATEs
- [content_line::as_recurrence](../content_line/as_recurrence.md)
- [sgcl::encoding](../README.md)
