[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](README.md)

# sgcl::async::rate_limiter::set_burst

```cpp
void set_burst(size_t burst);
```

Changes the most tokens the bucket holds: Go's `SetBurst`. The tokens there now are kept up to the new burst; a
larger burst adds none, the refill fills it. A burst of zero allows nothing at a finite limit.

## Parameters

| Parameter | Description |
|---|---|
| `burst` | the most tokens the bucket holds from now on |

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
    async::rate_limiter lim(10, 8);
    lim.set_burst(3);
    println("{} {}", lim.burst(), lim.tokens());  // the 8 cut to 3
    lim.set_burst(5);
    println("{}", lim.tokens());  // not filled
    clock.advance(1s);
    println("{}", lim.tokens());
}
```

Output:

```text
3 3
3
5
```

## See also

- [set_limit](set_limit.md): the other setting
- [burst](burst.md): the burst now
- [sgcl::async::rate_limiter](README.md)
