[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](../rate_limiter/README.md) › [reservation](README.md)

# sgcl::async::rate_limiter::reservation::time

```cpp
time_point time() const noexcept;
```

Returns the point of the module's [clock](../../core/clock/README.md) at which the caller may act: the moment of the
reservation for tokens that were there, the moment the refill gives the last of them otherwise, rounded up to the
nanosecond so that it is never early. What a [sleep_until](../sleep_until.md) or a [timeout](../timeout.md) case
takes.

## Parameters

None.

## Return value

The point; `time_point::max()` for a reservation that is not [ok](ok.md).

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::manual_clock clock;
    clock.install();
    async::rate_limiter lim(10, 1);
    time_point start = clock.now();
    (void)lim.allow();
    auto r = lim.reserve();
    println("{}", r.time() == start + 100ms);
}
```

Output:

```text
true
```

## See also

- [delay](delay.md): the same from now
- [sgcl::async::rate_limiter::reservation](README.md)
