[sgcl](../../README.md) › [core](../README.md) › [duration](README.md)

# sgcl::duration::abs

```cpp
constexpr duration abs() const noexcept;
```

The absolute value. The smallest duration, which has no positive counterpart in 64 bits, gives the largest.

## Parameters

None.

## Return value

The duration with its sign dropped; `max()` for `min()`.

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
    duration back = -1500 * microsecond;
    println("{} {}", back, back.abs());
    println("{}", duration::min().abs() == duration::max());
}
```

Output:

```text
-1.5ms 1.5ms
true
```

## See also

- [truncate](truncate.md), [round](round.md): to a multiple of a step
- [sgcl::duration](README.md)
