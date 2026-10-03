[sgcl](../../README.md) › [core](../README.md) › [duration](README.md)

# sgcl::duration::hours

```cpp
constexpr double hours() const noexcept;
```

The hours with the fraction, a `double`: `1.5` for 90 minutes. Go's `Hours() float64`. A day of the calendar is
not a number of hours (23, 24 or 25): that is a [date's](../../time/date/README.md) `add_days`.

## Parameters

None.

## Return value

The duration in hours, with the fraction.

## Complexity

Constant.

## Exceptions

None.

## Notes

The whole hours and the rest are converted apart and added, so that a long duration keeps its nanoseconds as far
as a `double` can.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    duration d = 90 * minute;
    println("{}", d.hours());
    println("{}", duration::max().hours());  // some 292 years
}
```

Output:

```text
1.5
2562047.7880152157
```

## See also

- [minutes](minutes.md): the minutes, with the fraction
- [max](max.md): the largest duration
- [sgcl::duration](README.md)
