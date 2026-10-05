[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::join_group

```cpp
expected<void, io::error> join_group(const ip_address& group, const network_interface& ifi = {}) const noexcept;
```

Joins the multicast `group` on the interface `ifi`, so that the socket receives the datagrams sent to the group at
its port: RFC 1112's host membership for IPv4 (IGMP), RFC 3810's for IPv6 (MLD), through RFC 3678's
`MCAST_JOIN_GROUP`, which names the interface by its index for both families. An interface of index 0 (the default)
is the system's choice, or, for an IPv6 group with a zone (`"ff02::1%lo0"`), the zone's. An IPv4-mapped group is the
IPv4 one. The membership is the socket's, and ends with [leave_group](leave_group.md) or the socket's close; a
socket may be a member of many groups, on many interfaces. [udp::listen_multicast](../udp/listen_multicast.md) binds
a socket and joins in one call.

## Parameters

| Parameter | Description |
|---|---|
| `group` | a multicast address: `224.0.0.0/4`, `ff00::/8` |
| `ifi` | the interface to join on ([network_interface](../network_interface.md)); none by default |

## Return value

Nothing, or the [io::error](../../io/error/README.md), its operation `join` and its path the group (`"239.1.2.3"`,
`"239.1.2.3 from 10.0.0.1"` with a source):

- `net::errc::invalid_address` for a group that is not a multicast address, and for an IPv6
  group whose zone names no interface;
- `EAFNOSUPPORT` for a group of the other family than the socket's: an IPv4 group needs an IPv4 socket (macOS takes
  none on a socket of both families, so the module takes none on any system), an IPv6 group an IPv6 one;
- `io::errc::closed` for a closed socket;
- the system's `errno` otherwise: `EADDRINUSE` for a group the socket joined already on that interface, `EADDRNOTAVAIL`
  for an interface that cannot take it.

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
using namespace std::chrono_literals;

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
    net::udp::socket member = net::udp::bind("0.0.0.0:0");
    net::ip_address group("239.255.10.1");
    member.join_group(group, lo);

    net::udp::socket sender = net::udp::bind("127.0.0.1:0");
    sender.set_multicast_interface(lo);
    sender.send_to("to the group", net::endpoint(group, member.local_endpoint().port()));

    vector<byte> room(64);
    member.set_read_deadline(clock::now() + 2s);
    net::udp::datagram d = member.receive_from(room).value();
    println("{} bytes from {}", d.size, d.from.address());

    println("{}", member.join_group(group, lo).error().code() == std::errc::address_in_use);
    println("{}", member.join_group(net::ip_address("10.0.0.1")).error().message());
}
```

Output:

```text
12 bytes from 127.0.0.1
true
join 10.0.0.1: invalid address
```

## See also

- [leave_group](leave_group.md): the other way
- [join_source_group](join_source_group.md): the group from one source alone
- [udp::listen_multicast](../udp/listen_multicast.md): bind and join in one call
- [interfaces](../interfaces.md): the interfaces to join on
- [udp::socket](README.md)
