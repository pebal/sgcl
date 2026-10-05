[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [revocation_cache](README.md)

# sgcl::net::tls::revocation_cache::size

```cpp
size_t size() const noexcept;
```

Returns the entries the cache holds now: OCSP answers and CRLs fetched by the checks of connections whose config
names it, their time not yet past when they were last looked up.

## Parameters

None.

## Return value

The number of entries, at most [capacity](capacity.md).

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
    net::tls::revocation_cache answers;
    println("{}", answers.size());
}
```

Output:

```text
0
```

## See also

- [capacity](capacity.md): the most it holds
- [clear](clear.md): every entry dropped
- [sgcl::net::tls::revocation_cache](README.md)
