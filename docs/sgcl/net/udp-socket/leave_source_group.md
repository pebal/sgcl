[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::leave_source_group

```cpp
expected<void, io::error> leave_source_group(const ip_address& group, const ip_address& source,
                                           const network_interface& ifi = {}) const noexcept;
```

Leaves the source-specific membership of `group` for `source` that [join_source_group](join_source_group.md) made
(`MCAST_LEAVE_SOURCE_GROUP`): the datagrams of that sender no longer reach the socket; the other sources joined stay.

## Parameters

| Parameter | Description |
|---|---|
| `group` | a multicast address: `224.0.0.0/4`, `ff00::/8` (`232.0.0.0/8` and `ff3x::/32` are the ranges RFC 4607 sets aside) |
| `source` | the source joined, of the group's family |
| `ifi` | the interface it was joined on ([network_interface](../network_interface.md)); none by default |

## Return value

Nothing, or the [io::error](../../io/error/README.md), its operation `leave` and its path the group (`"239.1.2.3"`,
`"239.1.2.3 from 10.0.0.1"` with a source):

- `net::errc::invalid_address` for a group that is not a multicast address, and for a source that is not a unicast address of the group's family, and for an IPv6
  group whose zone names no interface;
- `EAFNOSUPPORT` for a group of the other family than the socket's: an IPv4 group needs an IPv4 socket (macOS takes
  none on a socket of both families, so the module takes none on any system), an IPv6 group an IPv6 one;
- `io::errc::closed` for a closed socket;
- the system's `errno` otherwise: `EADDRNOTAVAIL` for a source the socket did not join.

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
    net::udp::socket member = net::udp::bind("0.0.0.0:0");
    net::ip_address group("239.255.10.4");
    net::ip_address source = net::ip_address::loopback_v4();
    member.join_source_group(group, source, lo);
    println("{}", member.leave_source_group(group, source, lo).has_value());
    println("{}", member.leave_source_group(group, source, lo).error().code() == std::errc::address_not_available);
}
```

Output:

```text
true
true
```

## See also

- [join_source_group](join_source_group.md): the other way
- [udp::socket](README.md)
