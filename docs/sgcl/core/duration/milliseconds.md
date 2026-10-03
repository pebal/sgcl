[sgcl](../../README.md) › [core](../README.md) › [duration](../duration.md)

# sgcl::duration::milliseconds

```cpp
constexpr int64_t milliseconds() const noexcept;
```

The whole milliseconds, truncated toward zero: what `duration_cast<milliseconds>(d).count()` is for a
`std::chrono` duration; Go's `Milliseconds() int64`.

## Parameters

None.

## Return value

The nanoseconds divided by 1000000, toward zero.

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
    duration d = 1999 * microsecond;
    println("{} {}", d.milliseconds(), (-d).milliseconds());
    println("{}", (90 * second).milliseconds());
}
```

Output:

```text
1 -1
90000
```

## See also

- [microseconds](microseconds.md): the whole microseconds
- [seconds](seconds.md): the seconds, with the fraction
- [sgcl::duration](../duration.md)
