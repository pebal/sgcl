[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a connection: `false` for one made by the default constructor, `true` for any made
by the module, closed or not. A closed connection is told by [is_closed](is_closed.md).

## Parameters

None.

## Return value

`true` when the handle holds a connection.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::connection none;
    auto [a, b] = net::connection::in_memory();
    a.close();
    println("{} {}", static_cast<bool>(none), static_cast<bool>(a));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](connection.md): a handle that holds none
- [is_closed](is_closed.md): whether the connection was closed
- [sgcl::net::connection](../connection.md)
