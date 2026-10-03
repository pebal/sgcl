[sgcl](../../README.md) › [time](../README.md) › [date](../date.md)

# sgcl::time::date::day

```cpp
constexpr int day() const noexcept;
```

The day of the month, 1 to 31.

## Parameters

None.

## Return value

The day of the month of the date.

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
    println("{} {}", time::date(2026, 9, 24).day(), time::date(2026, 3, 0).day());
}
```

Output:

```text
24 28
```

## See also

- [year](year.md), [month](month.md): the other fields
- [days_in_month](days_in_month.md): the last day of the month
- [sgcl::time::date](../date.md)
