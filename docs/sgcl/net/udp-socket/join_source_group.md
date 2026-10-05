[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::join_source_group

```cpp
expected<void, io::error> join_source_group(const ip_address& group, const ip_address& source,
                                          const network_interface& ifi = {}) const noexcept;
```

Joins the multicast `group` for the datagrams of one `source` alone: a source-specific membership (RFC 4607, IGMPv3 of
RFC 3376 and MLDv2 of RFC 3810), through RFC 3678's `MCAST_JOIN_SOURCE_GROUP`. A datagram the group gets from any
other sender does not reach the socket. Called again with another source, it adds that one. The interface is named
as for [join_group](join_group.md); a group joined whole cannot also be joined by source on the same interface.

## Parameters

| Parameter | Description |
|---|---|
| `group` | a multicast address: `224.0.0.0/4`, `ff00::/8` (`232.0.0.0/8` and `ff3x::/32` are the ranges RFC 4607 sets aside) |
| `source` | the one sender of the group to receive, of the group's family |
| `ifi` | the interface to join on ([network_interface](../network_interface.md)); none by default |

## Return value

Nothing, or the [io::error](../../io/error/README.md), its operation `join` and its path the group (`"239.1.2.3"`,
`"239.1.2.3 from 10.0.0.1"` with a source):

- `net::errc::invalid_address` for a group that is not a multicast address, and for a source that is not a unicast address of the group's family, and for an IPv6
  group whose zone names no interface;
- `EAFNOSUPPORT` for a group of the other family than the socket's: an IPv4 group needs an IPv4 socket (macOS takes
  none on a socket of both families, so the module takes none on any system), an IPv6 group an IPv6 one;
- `io::errc::closed` for a closed socket;
- the system's `errno` otherwise: `EADDRINUSE` for a source joined already, `EINVAL` for a group joined whole on that interface.

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
    net::ip_address group("239.255.10.3");
    net::endpoint to(group, member.local_endpoint().port());
    net::udp::socket sender = net::udp::bind("127.0.0.1:0");
    sender.set_multicast_interface(lo);
    vector<byte> room(64);

    member.join_source_group(group, net::ip_address("192.0.2.7"), lo);  // another sender
    sender.send_to("filtered", to);
    member.set_read_deadline(clock::now() + 200ms);
    println("{}", member.receive_from(room).error().is_timeout());

    member.join_source_group(group, net::ip_address::loopback_v4(), lo);
    sender.send_to("passed", to);
    member.set_read_deadline(clock::now() + 2s);
    println("{}", member.receive_from(room)->size);
}
```

Output:

```text
true
6
```

## See also

- [leave_source_group](leave_source_group.md): the other way
- [join_group](join_group.md): the group from every sender
- [udp::socket](README.md)
