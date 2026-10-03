[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a socket: `false` for one made by the default constructor, `true` for any made by
the module, closed or not. A closed socket is told by [is_closed](is_closed.md).

## Parameters

None.

## Return value

`true` when the handle holds a socket.

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
    net::udp::socket none;
    net::udp::socket s = net::udp::bind("127.0.0.1:0");
    s.close();
    println("{} {}", static_cast<bool>(none), static_cast<bool>(s));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](udp-socket.md): a handle that holds none
- [is_closed](is_closed.md): whether the socket was closed
- [sgcl::net::udp::socket](README.md)
