[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](README.md)

# sgcl::async::rate_limiter::rate_limiter

```cpp
rate_limiter(double per_second, size_t burst);           // (1)
rate_limiter(duration interval, size_t burst);           // (2)
rate_limiter(const rate_limiter&) noexcept = default;    // (3)
rate_limiter(rate_limiter&&) noexcept = default;         // (4)
```

1. A new bucket of `burst` tokens, full, refilled at `per_second` tokens a second. `rate_limiter::inf` allows
   everything, whatever the burst; zero, a negative limit or NaN gives the burst once and never refills it.
2. The same with one token every `interval`, Go's `rate.Every`: `rate_limiter(100ms, 1)` is ten a second. An
   interval of zero or less is `inf`.
3. A handle of the same bucket: the copy shares it, and tokens taken through either are taken from both.
4. The same, taken from the other handle, which stands for the same bucket still.

The bucket counts its time in ticks of a 1024th of a nanosecond, or of whole nanoseconds for a slow limit with a
large burst; a burst whose time does not fit in 2^58 ticks is held to what fits (a limit of one a day keeps a burst
of three thousand).

## Parameters

| Parameter | Description |
|---|---|
| `per_second` | the tokens added a second |
| `interval` | the time of one token |
| `burst` | the most tokens the bucket holds, and the tokens it starts with |

## Complexity

- (1–2) Constant: two allocations, the bucket and its constants.
- (3–4) Constant: one tracked word copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::rate_limiter requests(5, 10);  // 5 a second, 10 at once
    async::rate_limiter pings(200ms, 1);  // the same rate, one at a time
    async::rate_limiter same = requests;
    async::rate_limiter open(async::rate_limiter::inf, 0);
    println("{} {} {}", requests.limit(), pings.limit(), requests.burst());
    println("{} {}", same == requests, open.allow(1000));
}
```

Output:

```text
5 5 10
true true
```

## See also

- [set_limit](set_limit.md), [set_burst](set_burst.md): the settings changed later
- [sgcl::async::rate_limiter](README.md)
