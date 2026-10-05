[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::multicast_ttl

```cpp
expected<int, io::error> multicast_ttl() const noexcept;
```

Returns the TTL of the socket's datagrams to multicast groups, the hop limit of an IPv6 socket's: what
[set_multicast_ttl](set_multicast_ttl.md) set, 1 before.

## Parameters

None.

## Return value

The TTL, 0 to 255; or the [io::error](../../io/error/README.md), its operation `multicast ttl`: `io::errc::closed` for a
closed socket, the system's `errno` otherwise.

## Complexity

One system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::udp::socket v4 = net::udp::bind("127.0.0.1:0");
    net::udp::socket v6 = net::udp::bind("[::1]:0");
    v6.set_multicast_ttl(5);
    println("{} {}", v4.multicast_ttl().value(), v6.multicast_ttl().value());
    v4.close();
    println("{}", v4.multicast_ttl().error().is_closed());
}
```

Output:

```text
1 5
true
```

## See also

- [set_multicast_ttl](set_multicast_ttl.md): the other way
- [udp::socket](README.md)
