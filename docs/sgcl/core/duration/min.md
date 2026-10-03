[sgcl](../../README.md) › [core](../README.md) › [duration](README.md)

# sgcl::duration::min

```cpp
static constexpr duration min() noexcept;
```

The smallest duration, −2^63 nanoseconds, some 292 years back. It has no positive counterpart: `-min()` and
`min().abs()` are `max()`. A difference, a product or a conversion past the end of the range is this duration.

## Parameters

None.

## Return value

The smallest duration.

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
    println("{}", duration::min());
    println("{}", duration::min() - second == duration::min());  // saturated
    println("{}", -duration::min() == duration::max());
}
```

Output:

```text
-2562047h47m16.854775808s
true
true
```

## See also

- [max](max.md): the largest duration
- [abs](abs.md): the absolute value
- [sgcl::duration](README.md)
