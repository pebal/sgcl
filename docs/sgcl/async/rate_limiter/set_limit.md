[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](README.md)

# sgcl::async::rate_limiter::set_limit

```cpp
void set_limit(double per_second);    // (1)
void set_limit(duration interval);    // (2)
```

1. Changes the limit to `per_second` tokens a second: Go's `SetLimit`. The tokens there now, counted at the old limit,
   are kept, and the refill goes on at the new one. `rate_limiter::inf` allows everything from now on; zero, or less,
   or NaN keeps the tokens left and refills nothing. A bucket that comes back from `inf` is full.
2. The same with one token every `interval`; zero or less is `inf`.

A reservation made before keeps its time, and its [cancel](../rate_limiter-reservation/cancel.md) gives nothing back
after the change. The calls on other threads go on without a lock: the change publishes a new set of constants, and a
call that meets the moment of the change waits the few instructions until it is published.

## Parameters

| Parameter | Description |
|---|---|
| `per_second` | the tokens added a second from now on |
| `interval` | the time of one token from now on |

## Return value

None.

## Complexity

Constant: one allocation, the new constants.

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
    async::rate_limiter lim(1, 10);
    (void)lim.allow(10);
    clock.advance(2s);  // 2 tokens at one a second
    lim.set_limit(100);
    clock.advance(30ms);  // 3 more at a hundred
    println("{} {:.1f}", lim.limit(), lim.tokens());
    lim.set_limit(10ms);
    println("{}", lim.limit());
}
```

Output:

```text
100 5.0
100
```

## See also

- [set_burst](set_burst.md): the other setting
- [limit](limit.md): the limit now
- [sgcl::async::rate_limiter](README.md)
