[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::infinity

```cpp
static big_float infinity(bool negative = false) noexcept;
```

+∞, or −∞ for `negative`, at precision 0. Above (below) every number; a finite value plus it, times it (but zero) or
over it gives an infinity or a zero by the signs; ∞ − ∞, 0·∞ and ∞/∞ are `domain_error`, as Go panics there.

## Parameters

| Parameter | Description |
|---|---|
| `negative` | the sign |

## Return value

The infinity.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_float inf = math::big_float::infinity();
    println("{} {} {}", inf, -inf, inf + 1);
    println("{} {}", math::big_float(1) / inf, math::big_float(-1) / math::big_float());
}
```

Output:

```text
+Inf -Inf +Inf
0 -Inf
```

## See also

- [is_infinite](is_infinite.md): asks for one
- [sgcl::math::big_float](README.md)
