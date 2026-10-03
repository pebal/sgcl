[sgcl](../../README.md) › [core](../README.md) › [duration](../duration.md)

# sgcl::duration::nanoseconds

```cpp
constexpr int64_t nanoseconds() const noexcept;
```

The whole nanoseconds: the count the duration holds, exact. What `count()` is for a
`std::chrono::nanoseconds`; Go's `Nanoseconds() int64`.

## Parameters

None.

## Return value

The count of nanoseconds, from `min().nanoseconds()` to `max().nanoseconds()`.

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
    duration d = 1500 * microsecond;
    println("{}", d.nanoseconds());
    println("{}", (-7 * nanosecond).nanoseconds());
}
```

Output:

```text
1500000
-7
```

## See also

- [microseconds](microseconds.md), [milliseconds](milliseconds.md): the whole larger units
- [operator std::chrono::nanoseconds](operator_conv.md): the same count as the standard's type
- [sgcl::duration](../duration.md)
