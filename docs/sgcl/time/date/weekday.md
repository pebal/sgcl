[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::weekday

```cpp
constexpr time::weekday weekday() const noexcept;
```

The day of the week of the date, a [weekday](../weekday.md) numbered as ISO 8601 numbers it: Monday 1 to Sunday 7,
not C's and Go's Sunday 0.

## Parameters

None.

## Return value

The day of the week.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    time::weekday day = time::date(2026, 9, 27).weekday();
    println("{} {}", day, int(day));
    println("{}", day == time::weekday::sunday);
}
```

Output:

```text
Sunday 7
true
```

## See also

- [iso_week](iso_week.md): the week of ISO 8601
- [weekday](../weekday.md): the enumeration
- [sgcl::time::date](../date.md)
