[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](../rate_limiter/README.md) › [reservation](README.md)

# sgcl::async::rate_limiter::reservation::operator=

```cpp
reservation& operator=(reservation&& other) noexcept;    // (1)
reservation& operator=(const reservation&) = delete;     // (2)
```

1. Takes `other` over, as the move constructor does: its tokens, its time and its right to cancel them. The
   reservation assigned over lets go of its own, whose tokens stay taken. `other` is left not ok.
2. A reservation is not copyable: its tokens are given back once.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the reservation to take over |

## Return value

`*this`.

## Complexity

Constant.

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
    async::rate_limiter lim(10, 2);
    async::rate_limiter::reservation r;
    r = lim.reserve(2);
    println("{} {}", r.ok(), lim.tokens());
}
```

Output:

```text
true 0
```

## See also

- [(constructor)](rate_limiter-reservation.md)
- [sgcl::async::rate_limiter::reservation](README.md)
