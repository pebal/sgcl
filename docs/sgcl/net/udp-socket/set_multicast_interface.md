[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::set_multicast_interface

```cpp
expected<void, io::error> set_multicast_interface(const network_interface& ifi) const noexcept;
```

Sets the interface the socket's datagrams to a multicast group leave by: `IP_MULTICAST_IF` on an IPv4 socket, which
names it by an IPv4 address of its (the first of `ifi.addresses`, the form every system takes), `IPV6_MULTICAST_IF`
on an IPv6 one, by its index. Without it the system picks the interface by its routes; a group on the loopback
interface, or on one of several networks, needs it. An interface of index 0 is the system's choice again on IPv4; on
IPv6 the system decides (macOS refuses it with `EINVAL`, though RFC 3493 has 0 as the default). An IPv6 destination
with a zone (`"ff02::1%en0"`) leaves by the zone's interface whatever this says.

## Parameters

| Parameter | Description |
|---|---|
| `ifi` | the interface ([network_interface](../network_interface.md)), as [interfaces](../interfaces.md) gives it |

## Return value

Nothing, or the [io::error](../../io/error/README.md), its operation `multicast interface` and its path the interface's
name: `EADDRNOTAVAIL` for an interface without an IPv4 address on an IPv4 socket, `io::errc::closed` for a closed
socket, the system's `errno` otherwise.

## Complexity

One system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

net::network_interface loopback() {
    vector<net::network_interface> all = net::interfaces().value();
    for (auto& i : all) {
        if (i.loopback && i.multicast) {
            return i;
        }
    }
    return {};
}

int main() {
    net::network_interface lo = loopback();
    net::udp::socket sender = net::udp::bind("127.0.0.1:0");
    println("{}", sender.set_multicast_interface(lo).has_value());

    net::network_interface none_v4;
    none_v4.name = "v6only0";
    none_v4.index = 4242;
    println("{}", sender.set_multicast_interface(none_v4).error().code() == std::errc::address_not_available);
}
```

Output:

```text
true
true
```

## See also

- [interfaces](../interfaces.md): the interfaces to choose from
- [set_multicast_ttl](set_multicast_ttl.md): how far the datagrams go
- [udp::socket](README.md)
