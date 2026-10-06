[sgcl](../../README.md) › [time](../README.md)

# sgcl::time::cron

```cpp
#include "sgcl/time/cron.h"   // or "sgcl/time.h"

namespace sgcl::time {
    class cron {
    public:
        friend bool operator==(const cron& a, const cron& b) noexcept;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::time::cron` is a cron expression and the [zone](../zone/README.md) its times are read in: the times of the
zone's clock it names, which [next](next.md) finds after an instant and [every](../every.md) calls a function at. Go's
standard library has no cron; this is the syntax of Vixie cron, as Linux and macOS run it, with Quartz's additions
for the days.

The expression is five fields, the minute, the hour, the day of the month, the month and the day of the week, or six
with the second first; or a name: `@yearly` (and `@annually`), `@monthly`, `@weekly`, `@daily` (and `@midnight`),
`@hourly`. Every field takes `*`, a number, a range `a-b`, a step `*/n`, `a-b/n` or `a/n` (from `a` to the end), and
lists of them, `1,15-20,*/30`; the months take their English names (`JAN`–`DEC`) and the days of the week theirs
(`SUN`–`SAT`) or 0 to 7, 0 and 7 both Sunday, in any case. The day fields take Quartz's `?` for `*` and its specials:
`L` the last day of the month, `L-3` three days before it, `15W` the weekday nearest the 15th within the month, `LW`
the last weekday; `5L` the last Friday, `1#2` the second Monday.

## Rules

- When both day fields are restricted a day matches either: `0 0 13 * FRI` is every 13th and every Friday. A day
  field is unrestricted when it begins with `*` (so `*/2` in the day of the month leaves the day of the week alone to
  choose) or is `?`.
- The times are the ones the zone's clock shows, and a change of the clock follows ISC cron's rule. An expression of
  fixed times, whose minute and hour do not begin with `*`, fires once for each time: a time the clock skips fires at
  the instant of the jump (`30 2 * * *` at 03:00 CEST on the day Warsaw moves forward), a time the clock shows twice
  fires the first time only. An expression of wildcards (`*/15 * * * *`, `@hourly`) fires at every instant whose time
  matches: none in a skipped hour, twice in a repeated one.
- To the second. The times end with a [datetime](../datetime/README.md)'s range, in 2262; an expression that never
  matches (`0 0 30 2 *`) has no next time.
- A value: copied freely, its zone and its text held as tracked words.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](cron.md) | constructs the cron of an expression written in the program |
| `(destructor)` | destroys the cron |

#### Reading

| Function | Description |
|---|---|
| [parse](parse.md) | reads an expression into a cron, or says why it cannot |

#### Times

| Function | Description |
|---|---|
| [next](next.md) | the next time after an instant, or the next *n* |
| [matches](matches.md) | checks whether an instant is one of the times |

#### Observers

| Function | Description |
|---|---|
| [zone](zone.md) | the zone the times are read in |
| [to_string](to_string.md) | the expression as it was given |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | checks whether two crons name the same times in the same zone |
| [every](../every.md) | calls a function at every time of a cron |

## Complexity

A next time is a search of the calendar from the instant, a few steps a day of the months the expression allows, and a
look at the zone's next change; a few tens of nanoseconds for an expression that fires every day.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::cron backups("30 2 * * *", time::zone("Europe/Warsaw"));
    time::datetime from("2026-03-28T12:00:00+01:00", time::rfc3339);
    for (auto& t : backups.next(from, 3)) {
        println("{}", t.format("%F %R %Z"));  // the skipped 02:30 at the jump to 03:00
    }
}
```

Output:

```text
2026-03-29 03:00 CEST
2026-03-30 02:30 CEST
2026-03-31 02:30 CEST
```

## See also

- [every](../every.md): a function called at the times
- [zone](../zone/README.md): the clock the times are read on
- [async::every](../../async/every.md): a function called every period
