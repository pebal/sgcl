[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](README.md)

# sgcl::async::rate_limiter::burst

```cpp
size_t burst() const noexcept;
```

Returns the most tokens the bucket holds: the burst of the constructor or of the last [set_burst](set_burst.md).
More tokens than this are never taken at once at a finite limit.

## Parameters

None.

## Return value

The burst.

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
    async::rate_limiter lim(100, 20);
    println("{}", lim.burst());
    lim.set_burst(50);
    println("{}", lim.burst());
}
```

Output:

```text
20
50
```

## See also

- [set_burst](set_burst.md): changes it
- [limit](limit.md), [tokens](tokens.md): the other observers
- [sgcl::async::rate_limiter](README.md)
