[sgcl](../../README.md) › [encoding](../README.md) › [recurrence](README.md)

# sgcl::encoding::recurrence::by_second, by_minute, by_hour, by_day, by_month_day, by_year_day, by_week_no, by_month, by_set_pos, week_start

```cpp
slice<const int> by_second() const noexcept;          // (1)
slice<const int> by_minute() const noexcept;
slice<const int> by_hour() const noexcept;
slice<const weekday_rule> by_day() const noexcept;    // (2)
slice<const int> by_month_day() const noexcept;       // (3)
slice<const int> by_year_day() const noexcept;
slice<const int> by_week_no() const noexcept;
slice<const int> by_month() const noexcept;           // (4)
slice<const int> by_set_pos() const noexcept;
time::weekday week_start() const noexcept;            // (5)
```

The BY parts as written, in their order; empty when the rule does not give one.

1. BYSECOND (0 to 60), BYMINUTE (0 to 59), BYHOUR (0 to 23).
2. BYDAY: [weekday_rule](../recurrence-weekday_rule.md)s, a weekday and which of them.
3. BYMONTHDAY (1 to 31, -31 to -1 from the end), BYYEARDAY (to 366), BYWEEKNO (to 53).
4. BYMONTH (1 to 12), BYSETPOS (to 366, negative from the end).
5. WKST: the first day of a week; Monday without it.

## Parameters

None.

## Return value

The part.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::recurrence r("FREQ=MONTHLY;BYDAY=MO,-1FR;BYMONTHDAY=-1;BYHOUR=9,17;WKST=SU");
    println("{} {} {} {}", r.by_day().size(), r.by_month_day()[0], r.by_hour().size(), r.week_start());
}
```

Output:

```text
2 -1 2 Sunday
```

## See also

- [freq](freq.md)
- [sgcl::encoding::recurrence](README.md)
