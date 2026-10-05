[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [session_cache](README.md)

# sgcl::net::tls::session_cache::capacity

```cpp
size_t capacity() const noexcept;
```

Returns the sessions the cache holds at most, the capacity it was made with: a session past it drops the oldest of
all. Each server holds at most four besides.

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
    println("{}", net::tls::session_cache().capacity());
    println("{}", net::tls::session_cache(128).capacity());
}
```

Output:

```text
64
128
```

## See also

- [size](size.md): the sessions held now
- [sgcl::net::tls::session_cache](README.md)
