[sgcl](../../README.md) › [net](../README.md) › [connection](README.md)

# sgcl::net::connection::remote_endpoint

```cpp
endpoint remote_endpoint() const noexcept;
```

Returns the address and port of the peer: Go's `Conn.RemoteAddr`. A connection accepted on a socket of both
families from an IPv4 peer reports the IPv4 address, not the IPv4-mapped IPv6 one, and so does a connection dialed to
an IPv4-mapped address; a zone is the interface's name, as the system gives it, also when the address dialed named
the interface by its number (`fe80::1%1` is `fe80::1%lo0`). A unix socket and a pair in
memory have none: the [endpoint](../endpoint/README.md) is empty (`!is_valid()`).

## Parameters

None.

## Return value

The address of the peer, or an empty endpoint.

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
    net::listener l = net::tcp::listen(":0");  // both families
    net::endpoint ipv4(net::ip_address::loopback_v4(), l.local_endpoint().port());
    net::connection client = net::tcp::connect(ipv4);
    net::connection server = l.accept();
    println("{}", client.remote_endpoint() == ipv4);
    println("{}", server.remote_endpoint().address());
}
```

Output:

```text
true
127.0.0.1
```

## See also

- [local_endpoint](local_endpoint.md): the address of this end
- [endpoint](../endpoint/README.md): an address and a port
- [sgcl::net::connection](README.md)
