[sgcl](../../README.md) › [core](../README.md) › [duration](../duration.md)

# sgcl::duration::minutes

```cpp
constexpr double minutes() const noexcept;
```

The minutes with the fraction, a `double`: `1.5` for 90 seconds. Go's `Minutes() float64`.

## Parameters

None.

## Return value

The duration in minutes, with the fraction.

## Complexity

Constant.

## Exceptions

None.

## Notes

The whole minutes and the rest are converted apart and added, so that a long duration keeps its nanoseconds as
far as a `double` can.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    duration d = 90 * second;
    println("{}", d.minutes());
    println("{}", (2 * hour).minutes());
}
```

Output:

```text
1.5
120
```

## See also

- [seconds](seconds.md), [hours](hours.md): the neighbouring units, with the fraction
- [sgcl::duration](../duration.md)
