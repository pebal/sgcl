[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](README.md)

# sgcl::async::rate_limiter::tokens

```cpp
double tokens() const noexcept;
```

Returns the tokens in the bucket now, with the part of a token the refill has made so far: Go's `Tokens`. It is
negative while reservations are ahead of the refill, never more than the [burst](burst.md). For a bucket that allows
everything it is the burst; for one never refilled, the tokens left. A reading, not a promise: another thread may
take the tokens the moment after.

## Parameters

None.

## Return value

The tokens now.

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
    async::rate_limiter lim(4, 2);
    auto r = lim.reserve(2);
    auto debt = lim.reserve(1);
    println("{}", lim.tokens());
    clock.advance(375ms);
    println("{}", lim.tokens());
}
```

Output:

```text
-1
0.5
```

## See also

- [allow](allow.md), [reserve](reserve.md): what takes them
- [sgcl::async::rate_limiter](README.md)
