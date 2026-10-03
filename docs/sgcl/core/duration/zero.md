[sgcl](../../README.md) › [core](../README.md) › [duration](README.md)

# sgcl::duration::zero

```cpp
static constexpr duration zero() noexcept;
```

The zero duration, as `std::chrono` has it: the same as `duration()`.

## Parameters

None.

## Return value

A duration of zero nanoseconds.

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
    duration left = 3 * second;
    while (left > duration::zero()) {
        left -= 1500 * millisecond;
    }
    println("{} {}", left, duration::zero() == duration());
}
```

Output:

```text
0s true
```

## See also

- [max](max.md), [min](min.md): the ends of the range
- [sgcl::duration](README.md)
