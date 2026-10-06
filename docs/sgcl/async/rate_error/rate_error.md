[sgcl](../../README.md) › [async](../README.md) › [rate_error](README.md)

# sgcl::async::rate_error::rate_error

```cpp
constexpr explicit rate_error(reason r) noexcept;
```

Constructs the error of the reason `r`: what the library returns, and what a test or a function of the program's own
that passes the limiter's failures on compares with.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the [reason](../rate_error-reason.md) |

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
    async::rate_error e(async::rate_error::reason::stopped);
    println("{}", e.message());
}
```

Output:

```text
stopped
```

## See also

- [why](why.md): the reason given back
- [sgcl::async::rate_error](README.md)
