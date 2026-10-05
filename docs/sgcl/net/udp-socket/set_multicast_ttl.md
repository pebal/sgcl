[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::set_multicast_ttl

```cpp
expected<void, io::error> set_multicast_ttl(int ttl) const noexcept;
```

Sets the TTL of the socket's datagrams to multicast groups: how many routers they may cross, `IP_MULTICAST_TTL` on an
IPv4 socket, the hop limit `IPV6_MULTICAST_HOPS` on an IPv6 one (a socket of both families included). 1, the
default (RFC 1112 §6.1, RFC 3493 §5.2), keeps them on the link; 0 keeps them on this machine. The datagrams to a
unicast address keep their own TTL.

## Parameters

| Parameter | Description |
|---|---|
| `ttl` | 0 to 255 |

## Return value

Nothing, or the [io::error](../../io/error/README.md), its operation `multicast ttl` and its path the socket:
`EINVAL` for a `ttl` outside 0 to 255, the setting unchanged; `io::errc::closed` for a closed socket; the system's
`errno` otherwise.

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
    net::udp::socket sender = net::udp::bind("127.0.0.1:0");
    println("{}", sender.multicast_ttl().value());
    sender.set_multicast_ttl(32);
    println("{}", sender.multicast_ttl().value());
    println("{}", sender.set_multicast_ttl(300).error().code() == std::errc::invalid_argument);
}
```

Output:

```text
1
32
true
```

## See also

- [multicast_ttl](multicast_ttl.md): the TTL set
- [set_multicast_interface](set_multicast_interface.md): the interface they leave by
- [udp::socket](README.md)
