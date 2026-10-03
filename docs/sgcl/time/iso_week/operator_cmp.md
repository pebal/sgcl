[sgcl](../../README.md) › [time](../README.md) › [iso_week](../iso_week.md)

# sgcl::time::operator== (sgcl::time::iso_week)

```cpp
friend constexpr bool operator==(const iso_week&, const iso_week&) noexcept = default;
```

Compares two weeks field by field: the same year and the same week. `!=` is made from it by the compiler. There
is no order: a week is compared for equality only.

## Parameters

The two weeks compared, the operands, unnamed in the declaration.

## Return value

`true` when both the years and the weeks are equal.

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
    time::iso_week week = time::date(2024, 12, 30).iso_week();
    println("{} {}", week == time::iso_week{2025, 1}, week != time::date(2025, 1, 5).iso_week());
}
```

Output:

```text
true false
```

## See also

- [date::iso_week](../date/iso_week.md): the week of a date
- [sgcl::time::iso_week](../iso_week.md)
