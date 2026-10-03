[sgcl](../../README.md) › [core](../README.md) › [duration](README.md)

# sgcl::duration::microseconds

```cpp
constexpr int64_t microseconds() const noexcept;
```

The whole microseconds, truncated toward zero: what `duration_cast<microseconds>(d).count()` is for a
`std::chrono` duration; Go's `Microseconds() int64`.

## Parameters

None.

## Return value

The nanoseconds divided by 1000, toward zero.

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
    duration d = 1999 * nanosecond;
    println("{} {}", d.microseconds(), (-d).microseconds());
    println("{}", (2 * millisecond).microseconds());
}
```

Output:

```text
1 -1
2000
```

## See also

- [nanoseconds](nanoseconds.md): the whole nanoseconds
- [milliseconds](milliseconds.md): the whole milliseconds
- [sgcl::duration](README.md)
