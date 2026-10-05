[sgcl](../../README.md) › [net](../README.md) › [udp](README.md)

# sgcl::net::udp::listen_multicast

```cpp
static expected<udp::socket, io::error> listen_multicast(const string& group,
                                                         const network_interface& ifi = {}) noexcept;
```

Returns a [udp::socket](../udp-socket/README.md) that receives the datagrams sent to the multicast `group` at its port:
Go's `net.ListenMulticastUDP`. The socket is of the group's family, bound to that family's wildcard (`0.0.0.0` or
`::`, IPv6 alone) at the port, with `SO_REUSEADDR` and `SO_REUSEPORT`, so that other sockets of this program and other
programs may listen on the same port and each gets every datagram; the group is joined on `ifi`
([join_group](../udp-socket/join_group.md)), which is also the interface the socket's own datagrams to groups leave by
([set_multicast_interface](../udp-socket/set_multicast_interface.md)). Without an interface the system picks one, or
an IPv6 group's zone names it (`"[ff02::fb%en0]:5353"`).

It never waits: there is no task form, and it serves on a worker as well. More groups join the same socket with
[join_group](../udp-socket/join_group.md).

## Parameters

| Parameter | Description |
|---|---|
| `group` | `"group:port"`: `"239.1.2.3:5000"`, `"[ff02::1:3]:5355"`; an address, never a name; port 0 is one the system picks |
| `ifi` | the interface to join on ([network_interface](../network_interface.md)); none by default |

## Return value

The socket, joined; or the [io::error](../../io/error/README.md), its operation `listen udp` and its path `group`:

- `net::errc::invalid_address` for a text that is not an address with a port, for an address that is not a
  multicast one, for an IPv6 group whose zone names no interface;
- `EADDRNOTAVAIL` for an interface without an IPv4 address given for an IPv4 group, and for one that cannot join;
- the system's `errno` otherwise (`EAFNOSUPPORT` for IPv6 on a machine without it).

## Complexity

A few system calls.

## Exceptions

None.

## Notes

The socket receives at its port whatever comes for the groups it joined, from every sender: datagrams of other
groups at the same port do not reach it, on Linux as on the BSDs (the module turns `IP_MULTICAST_ALL` off, so that a
socket bound to the wildcard does not get every group another socket of the machine joined). Two programs on one
group and port both receive each datagram; two unicast datagrams to the port go to one of them, as `SO_REUSEPORT`
spreads them.

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

size_t received(net::udp::socket member) {
    vector<byte> room(64);
    member.set_read_deadline(clock::now() + 2s);
    return member.receive_from(room)->size;
}

int main() {
    net::network_interface lo = loopback();
    net::udp::socket first = net::udp::listen_multicast("239.255.20.1:0", lo);
    string port = to_string(first.local_endpoint().port());
    net::udp::socket second = net::udp::listen_multicast("239.255.20.1:" + port, lo);

    net::udp::socket sender = net::udp::bind("127.0.0.1:0");
    sender.set_multicast_interface(lo);
    sender.send_to("to both", net::endpoint("239.255.20.1:" + port));

    println("{} {}", received(first), received(second));

    println("{}", net::udp::listen_multicast("10.0.0.1:5000").error().message());
}
```

Output:

```text
7 7
listen udp 10.0.0.1:5000: invalid address
```

## See also

- [join_group](../udp-socket/join_group.md), [leave_group](../udp-socket/leave_group.md): the memberships of a socket
- [interfaces](../interfaces.md): the interfaces to join on
- [bind, async_bind](bind.md): a socket for unicast
- [sgcl::net::udp](README.md)
