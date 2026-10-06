[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](README.md)

# sgcl::async::rate_limiter::reserve

```cpp
reservation reserve(size_t n = 1) noexcept;
```

Takes `n` tokens for the moment they come and returns a [reservation](../rate_limiter-reservation/README.md) that
says when that is: Go's `Reserve` and `ReserveN`. The tokens there now are taken at once, the rest from the refill,
the bucket going into debt, so the calls after it come after it. The caller acts at the reservation's
[time](../rate_limiter-reservation/time.md), or gives the tokens back with
[cancel](../rate_limiter-reservation/cancel.md) when it will not act.

The reservation is not [ok](../rate_limiter-reservation/ok.md), and nothing is taken, for more than the burst at a
finite limit, for more than is left at a limit of zero, and for tokens further away than the bucket counts (about 52
days at a fine limit).

## Parameters

| Parameter | Description |
|---|---|
| `n` | the tokens to take |

## Return value

The reservation: its time and delay, ok or not.

## Complexity

Constant: a read of the clock and one compare-exchange, retried after a pause when another thread changed the word
first.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::manual_clock clock;
    clock.install();
    async::rate_limiter lim(10, 1);
    auto now = lim.reserve();
    auto next = lim.reserve();
    auto after = lim.reserve(1);
    println("{} {}", now.delay().to_string(), next.delay().to_string());
    println("{}", after.delay().to_string());
    println("{}", lim.reserve(2).ok());  // more than the burst
}
```

Output:

```text
0s 100ms
200ms
false
```

## See also

- [allow](allow.md): the tokens now or nothing
- [acquire](acquire.md): a reservation and the wait for its time
- [reservation](../rate_limiter-reservation/README.md)
- [sgcl::async::rate_limiter](README.md)
