[sgcl](../../README.md) › [time](../README.md) › [date](README.md)

# sgcl::time::date::month

```cpp
constexpr time::month month() const noexcept;
```

The month of the date, January to December, a [month](../month.md) as Go's `time.Month` is: a name for the
number, `int(d.month())` the number 1 to 12.

## Parameters

None.

## Return value

The month of the date.

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
    time::date d(2026, 9, 24);
    println("{} {}", d.month(), int(d.month()));
    println("{}", d.month() == time::month::september);
}
```

Output:

```text
September 9
true
```

## See also

- [year](year.md), [day](day.md): the other fields
- [month](../month.md): the enumeration
- [sgcl::time::date](README.md)
