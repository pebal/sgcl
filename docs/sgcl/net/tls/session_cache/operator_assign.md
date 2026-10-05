[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [session_cache](README.md)

# sgcl::net::tls::session_cache::operator=

```cpp
session_cache& operator=(const session_cache& other) noexcept;    // (1), implicitly declared
session_cache& operator=(session_cache&& other) noexcept;         // (2), implicitly declared
```

Makes this handle one of the cache `other` holds; the two share its sessions. The move is the copy: `other` keeps
the cache.

The cache this handle held before is left to the collector when nothing else holds it; the keys of its sessions are
zeroed then.

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
    net::tls::session_cache mine(8);
    net::tls::session_cache shared(32);
    mine = shared;
    println("{}", mine.capacity());
}
```

Output:

```text
32
```

## See also

- [(constructor)](session_cache.md): a cache made, or a copy
- [sgcl::net::tls::session_cache](README.md)
