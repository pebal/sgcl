[sgcl](../../README.md) › [core](../README.md) › [duration](../duration.md)

# sgcl::duration::max

```cpp
static constexpr duration max() noexcept;
```

The largest duration, 2^63 − 1 nanoseconds, some 292 years: "never" for a timer. A point moved by it saturates at
its clock's `max()` (`now() + duration::max()` is `time_point::max()`), which the [timers](../../async/README.md#time) of
async never fire, so `async::sleep(duration::max())`, `async::after(duration::max())` and a `timeout` of it mean
never. A sum, a product or a conversion past the end of the range is this duration.

## Parameters

None.

## Return value

The largest duration.

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
    println("{}", duration::max());
    println("{}", duration::max() + second == duration::max());  // saturated

    time_point never = sgcl::clock::now() + duration::max();
    println("{}", never == time_point::max());
}
```

Output:

```text
2562047h47m16.854775807s
true
true
```

## See also

- [min](min.md): the smallest duration
- [operator+](operator_arith.md): the saturated arithmetic
- [sgcl::duration](../duration.md)
