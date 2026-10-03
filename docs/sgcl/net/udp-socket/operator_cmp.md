[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::operator==, operator!= (sgcl::net::udp::socket)

```cpp
friend bool operator==(const socket& a, const socket& b) noexcept;
```

Checks whether `a` and `b` are handles of the same socket: copies of one handle, not two sockets on one address.
Two handles that hold none are equal. `a != b` is `!(a == b)`, rewritten by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when the two hold the same socket, or both none; `false` otherwise.

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
    net::udp::socket copy = s;
    net::udp::socket other = net::udp::bind("127.0.0.1:0");
    println("{} {} {}", s == copy, s == other, s != other);
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](udp-socket.md): a copy that is the same socket
- [sgcl::net::udp::socket](README.md)
