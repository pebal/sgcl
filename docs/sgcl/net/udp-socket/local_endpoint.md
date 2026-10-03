[sgcl](../../README.md) › [net](../README.md) › [udp](../udp.md) › [socket](../udp-socket.md)

# sgcl::net::udp::socket::local_endpoint

```cpp
endpoint local_endpoint() const noexcept;
```

Returns the address and port the socket is bound to: Go's `LocalAddr`. A port 0 given to
[udp::bind](../udp/bind.md) is the port the system chose; a socket of [udp::connect](../udp/connect.md) has the
address and port the system gave it for its peer.

## Parameters

None.

## Return value

The address and port of this end.

## Complexity

Constant: the address is kept when the socket is made.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::udp::socket s = net::udp::bind("127.0.0.1:0");
    println("{} {}", s.local_endpoint().address(), s.local_endpoint().port() != 0);

    net::udp::socket c = net::udp::connect(s.local_endpoint().to_string());
    println("{}", c.local_endpoint().address());
}
```

Output:

```text
127.0.0.1 true
127.0.0.1
```

## See also

- [remote_endpoint](remote_endpoint.md): the peer of a connected socket
- [sgcl::net::udp::socket](../udp-socket.md)
