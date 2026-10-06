[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](../rate_limiter/README.md) › [reservation](README.md)

# sgcl::async::rate_limiter::reservation::reservation

```cpp
reservation() noexcept;                       // (1)
reservation(reservation&& other) noexcept;    // (2)
reservation(const reservation&) = delete;     // (3)
```

1. A reservation of nothing, not ok: one to assign a reservation to later. A reservation of tokens is made by
   [reserve](../rate_limiter/reserve.md).
2. Takes `other` over: its tokens, its time, its right to cancel them. `other` is left not ok.
3. A reservation is not copyable: its tokens are given back once.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the reservation to take over |

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
    async::rate_limiter lim(10, 1);
    async::rate_limiter::reservation later;
    auto r = lim.reserve();
    async::rate_limiter::reservation mine(std::move(r));
    println("{} {} {}", later.ok(), r.ok(), mine.ok());
}
```

Output:

```text
false false true
```

## See also

- [operator=](operator_assign.md): the same into a reservation that exists
- [sgcl::async::rate_limiter::reservation](README.md)
