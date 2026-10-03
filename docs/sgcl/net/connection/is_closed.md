[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the program closed the connection, by [close](close.md) on this handle or on any copy. A peer that
closed its end is not seen here: that is a read of 0, or a write that fails.

## Parameters

None.

## Return value

`true` after `close`, `false` before it.

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
    auto [a, b] = net::connection::in_memory();
    net::connection copy = a;
    println("{} {}", a.is_closed(), b.is_closed());
    copy.close();
    println("{} {}", a.is_closed(), b.is_closed());
}
```

Output:

```text
false false
true false
```

## See also

- [close, async_close](close.md): ends the connection
- [sgcl::net::connection](../connection.md)
