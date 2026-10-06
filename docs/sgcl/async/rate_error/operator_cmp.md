[sgcl](../../README.md) › [async](../README.md) › [rate_error](README.md)

# sgcl::async::operator==, operator!= (sgcl::async::rate_error)

```cpp
friend bool operator==(const rate_error& a, const rate_error& b) noexcept;
```

Compares two errors: equal when their reasons are. The `!=` is the one C++ writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the errors to compare |

## Return value

`true` when `a` and `b` have the same reason.

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
    auto r = lim.acquire(2).wait();
    println("{}", r.error() == async::rate_error(async::rate_error::reason::burst));
}
```

Output:

```text
true
```

## See also

- [why](why.md): the reason itself
- [sgcl::async::rate_error](README.md)
