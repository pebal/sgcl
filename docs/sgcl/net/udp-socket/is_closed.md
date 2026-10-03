[sgcl](../../README.md) › [net](../README.md) › [udp](../udp.md) › [socket](../udp-socket.md)

# sgcl::net::udp::socket::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the socket was closed, by [close](close.md) on this handle or on any copy.

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
    net::udp::socket s = net::udp::bind("127.0.0.1:0");
    println("{}", s.is_closed());
    s.close();
    println("{}", s.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close](close.md): closes the socket
- [sgcl::net::udp::socket](../udp-socket.md)
