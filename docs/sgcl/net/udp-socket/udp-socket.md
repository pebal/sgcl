[sgcl](../../README.md) › [net](../README.md) › [udp](../udp.md) › [socket](../udp-socket.md)

# sgcl::net::udp::socket::socket

```cpp
/*(1)*/ socket() noexcept = default;
/*(2)*/ socket(const socket& other) noexcept;   // implicitly declared
/*(3)*/ socket(socket&& other) noexcept;        // implicitly declared
```

1. A handle that holds no socket: `!s`. An operation on it is a contract violation (debug builds assert); it is
   given a socket by an assignment.
2. A handle of the same socket as `other`: one descriptor, one deadline per direction, shared.
3. The same, `other` left holding no socket.

A socket with a descriptor is made by [udp::bind](../udp/bind.md) and [udp::connect](../udp/connect.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose socket this one shares |

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
    println("{}", static_cast<bool>(none));

    net::udp::socket s = net::udp::bind("127.0.0.1:0");
    net::udp::socket same = s;
    same.close();
    println("{} {}", s.is_closed(), same == s);
}
```

Output:

```text
false
true true
```

## See also

- [operator bool](operator_bool.md): whether the handle holds a socket
- [operator==](operator_cmp.md): whether two handles are the same socket
- [sgcl::net::udp::socket](../udp-socket.md)
