[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [revocation_cache](README.md)

# sgcl::net::tls::revocation_cache::operator=

```cpp
revocation_cache& operator=(const revocation_cache& other) noexcept;    // (1), implicitly declared
revocation_cache& operator=(revocation_cache&& other) noexcept;         // (2), implicitly declared
```

Makes this handle one of the cache `other` holds; the two share its entries. The move is the copy: `other` keeps the
cache. The cache this handle held before is left to the collector when nothing else holds it.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose cache this one takes |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::revocation_cache mine(8);
    net::tls::revocation_cache shared(32);
    mine = shared;
    println("{}", mine.capacity());
}
```

Output:

```text
32
```

## See also

- [(constructor)](revocation_cache.md): a cache made, or a copy
- [sgcl::net::tls::revocation_cache](README.md)
