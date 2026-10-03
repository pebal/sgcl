[sgcl](../../README.md) › [net](../README.md) › [connection](../connection.md)

# sgcl::net::connection::local_endpoint

```cpp
endpoint local_endpoint() const noexcept;
```

Returns the address and port of this end: Go's `Conn.LocalAddr`. A dialed TCP connection's port is the one the
system chose for it. A unix socket and a pair in memory have none: the [endpoint](../endpoint.md) is empty
(`!is_valid()`); a unix socket is named by its [path](path.md).

## Parameters

None.

## Return value

The address of this end, or an empty endpoint.

## Complexity

Constant: the address is kept when the connection is made.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::listener l = net::tcp::listen("127.0.0.1:0");
    net::connection client = net::tcp::connect(l.local_endpoint());
    net::connection server = l.accept();
    println("{}", client.local_endpoint().address());
    println("{}", client.local_endpoint() == server.remote_endpoint());

    auto [a, b] = net::connection::in_memory();
    println("{}", a.local_endpoint().is_valid());
}
```

Output:

```text
127.0.0.1
true
false
```

## See also

- [remote_endpoint](remote_endpoint.md): the address of the peer
- [endpoint](../endpoint.md): an address and a port
- [sgcl::net::connection](../connection.md)
