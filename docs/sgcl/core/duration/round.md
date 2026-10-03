[sgcl](../../README.md) › [core](../README.md) › [duration](../duration.md)

# sgcl::duration::round

```cpp
constexpr duration round(duration step) const noexcept;
```

The duration to the nearest multiple of `step`, a half away from zero: Go's `Round`. `std::chrono::round` rounds a
half to even instead. The result is saturated at the ends of the range; a step of zero or less leaves the duration
as it is.

## Parameters

| Parameter | Description |
|---|---|
| `step` | the multiple to round to |

## Return value

The multiple of `step` nearest to the duration, the one further from zero for a half; `max()` or `min()` when that
multiple does not fit; the duration itself when `step` is zero or less.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    duration d = 1234567 * nanosecond;
    println("{} {}", d.round(millisecond), d.round(microsecond));
    println("{} {}", (1500 * millisecond).round(second), (-1500 * millisecond).round(second));
    println("{}", duration::max().round(hour) == duration::max());  // saturated
}
```

Output:

```text
1ms 1.235ms
2s -2s
true
```

## See also

- [truncate](truncate.md): toward zero to a multiple
- [sgcl::duration](../duration.md)
