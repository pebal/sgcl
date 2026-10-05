[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [revocation_cache](README.md)

# sgcl::net::tls::revocation_cache::capacity

```cpp
size_t capacity() const noexcept;
```

Returns the entries the cache holds at most, as its constructor was given: past it the oldest entry is dropped for a
new one.

## Parameters

None.

## Return value

The capacity.

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
    println("{} {}",
        net::tls::revocation_cache().capacity(), net::tls::revocation_cache(10).capacity());
}
```

Output:

```text
256 10
```

## See also

- [size](size.md): the entries held now
- [sgcl::net::tls::revocation_cache](README.md)
