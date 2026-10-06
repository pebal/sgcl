[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](README.md)

# sgcl::async::rate_limiter::limit

```cpp
double limit() const noexcept;
```

Returns the tokens the bucket gains a second: the limit of the constructor or of the last
[set_limit](set_limit.md), `rate_limiter::inf` for a bucket that allows everything, and `0` for one never refilled
(a limit given as zero, as a negative number or as NaN).

## Parameters

None.

## Return value

The limit, in tokens a second.

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
    async::rate_limiter a(250ms, 1);
    async::rate_limiter b(-1, 1);
    async::rate_limiter c(async::rate_limiter::inf, 1);
    println("{} {} {}", a.limit(), b.limit(), c.limit());
}
```

Output:

```text
4 0 inf
```

## See also

- [set_limit](set_limit.md): changes it
- [burst](burst.md), [tokens](tokens.md): the other observers
- [sgcl::async::rate_limiter](README.md)
