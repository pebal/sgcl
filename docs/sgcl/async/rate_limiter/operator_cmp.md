[sgcl](../../README.md) › [async](../README.md) › [rate_limiter](README.md)

# sgcl::async::operator==, operator!= (sgcl::async::rate_limiter)

```cpp
friend bool operator==(const rate_limiter& a, const rate_limiter& b) noexcept;
```

Checks whether two handles stand for the same bucket: `true` when one is a copy of the other or both are copies of
one. Two limiters made apart are never equal, whatever their limits. The `!=` is the one C++ writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` are the same bucket, `false` otherwise.

## Complexity

Constant: two words compared.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::rate_limiter a(10, 1);
    async::rate_limiter b = a;
    async::rate_limiter c(10, 1);
    println("{} {} {}", a == b, a == c, a != c);
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](rate_limiter.md): a handle of the same bucket
- [sgcl::async::rate_limiter](README.md)
