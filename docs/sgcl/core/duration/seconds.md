[sgcl](../../README.md) › [core](../README.md) › [duration](README.md)

# sgcl::duration::seconds

```cpp
constexpr double seconds() const noexcept;
```

The seconds with the fraction, a `double`: `1.5` for 1500 ms, not `1500000000`. What
`duration<double>(d).count()` is for a `std::chrono` duration; Go's `Seconds() float64`.

## Parameters

None.

## Return value

The duration in seconds, with the fraction.

## Complexity

Constant.

## Exceptions

None.

## Notes

The whole seconds and the rest are converted apart and added, so that a long duration keeps its nanoseconds as
far as a `double` can.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    duration lap = 1500 * millisecond;
    println("{}", lap.seconds());
    println("{}", (-250 * millisecond).seconds());
}
```

Output:

```text
1.5
-0.25
```

## See also

- [milliseconds](milliseconds.md): the whole milliseconds
- [minutes](minutes.md), [hours](hours.md): the larger units, with the fraction
- [sgcl::duration](README.md)
