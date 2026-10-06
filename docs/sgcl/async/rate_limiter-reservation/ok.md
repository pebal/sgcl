[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](../rate_limiter/README.md) › [reservation](README.md)

# sgcl::async::rate_limiter::reservation::ok

```cpp
bool ok() const noexcept;
```

Checks whether the reservation holds tokens: `true` when [reserve](../rate_limiter/reserve.md) took them; `false`
when it took nothing (more than the burst at a finite limit, more than is left at a limit of zero, tokens further
than the bucket counts), and for a reservation default-constructed, moved from or cancelled.

## Parameters

None.

## Return value

`true` when the reservation holds tokens.

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
    async::rate_limiter lim(10, 3);
    auto fits = lim.reserve(3);
    auto too_many = lim.reserve(4);
    println("{} {}", fits.ok(), too_many.ok());
    fits.cancel();
    println("{}", fits.ok());
}
```

Output:

```text
true false
false
```

## See also

- [time](time.md), [delay](delay.md): when the caller may act
- [sgcl::async::rate_limiter::reservation](README.md)
