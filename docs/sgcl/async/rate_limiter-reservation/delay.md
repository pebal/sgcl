[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](../rate_limiter/README.md) › [reservation](README.md)

# sgcl::async::rate_limiter::reservation::delay

```cpp
duration delay() const noexcept;
```

Returns how long the caller has to wait from now: Go's `Delay`. Zero once the [time](time.md) has come, and at once
for tokens that were there.

## Parameters

None.

## Return value

The time until the reservation's time, zero when it has come; `duration::max()` for a reservation that is not
[ok](ok.md).

## Complexity

Constant: a read of the clock.

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
    async::rate_limiter lim(4, 1);
    auto first = lim.reserve();
    auto second = lim.reserve();
    println("{} {}", first.delay().to_string(), second.delay().to_string());
    clock.advance(100ms);
    println("{}", second.delay().to_string());
}
```

Output:

```text
0s 250ms
150ms
```

## See also

- [time](time.md): the point itself
- [sgcl::async::rate_limiter::reservation](README.md)
