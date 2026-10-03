[sgcl](../../README.md) › [time](../README.md) › [datetime](../datetime.md)

# sgcl::time::datetime::month

```cpp
time::month month() const noexcept;
```

The month the zone's clock shows at the instant, Go's `t.Month()`: a [month](../month.md), `month::january` to
`month::december`; `int(t.month())` is 1 to 12. It is written as its English name.

## Parameters

None.

## Return value

The month.

## Complexity

Logarithmic in the number of the zone's changes; constant in UTC and in a fixed zone.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto t = time::date(2026, 9, 24).at(12, 41, time::zone("Europe/Warsaw"));
    println("{} {} {}", t.month(), int(t.month()), t.month() == time::month::september);
}
```

Output:

```text
September 9 true
```

## See also

- [year](year.md), [day](day.md): the rest of the date
- [month](../month.md): the enumeration
- [sgcl::time::datetime](../datetime.md)
