[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::leave_group

```cpp
expected<void, io::error> leave_group(const ip_address& group, const network_interface& ifi = {}) const noexcept;
```

Leaves the multicast `group` on the interface `ifi`, a membership [join_group](join_group.md) made: the socket no
longer receives the group's datagrams (`MCAST_LEAVE_GROUP`). The interface is named as the join named it: index 0 is
the system's choice, or an IPv6 group's zone. The other sockets of the machine that joined the group keep it.

## Parameters

| Parameter | Description |
|---|---|
| `group` | the group joined |
| `ifi` | the interface it was joined on; none by default |

## Return value

Nothing, or the [io::error](../../io/error/README.md), its operation `leave` and its path the group (`"239.1.2.3"`,
`"239.1.2.3 from 10.0.0.1"` with a source):

- `net::errc::invalid_address` for a group that is not a multicast address, and for an IPv6
  group whose zone names no interface;
- `EAFNOSUPPORT` for a group of the other family than the socket's: an IPv4 group needs an IPv4 socket (macOS takes
  none on a socket of both families, so the module takes none on any system), an IPv6 group an IPv6 one;
- `io::errc::closed` for a closed socket;
- the system's `errno` otherwise: `EADDRNOTAVAIL` for a group the socket is not a member of on that interface.

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
    net::udp::socket member = net::udp::listen_multicast("239.255.10.2:0", lo);
    net::ip_address group("239.255.10.2");
    net::udp::socket sender = net::udp::bind("127.0.0.1:0");
    sender.set_multicast_interface(lo);
    net::endpoint to(group, member.local_endpoint().port());

    member.leave_group(group, lo);
    sender.send_to("nobody", to);
    vector<byte> room(64);
    member.set_read_deadline(clock::now() + 200ms);
    println("{}", member.receive_from(room).error().is_timeout());
    println("{}", member.leave_group(group, lo).error().code() == std::errc::address_not_available);
}
```

Output:

```text
true
true
```

## See also

- [join_group](join_group.md): the other way
- [leave_source_group](leave_source_group.md): a source-specific membership left
- [udp::socket](README.md)
