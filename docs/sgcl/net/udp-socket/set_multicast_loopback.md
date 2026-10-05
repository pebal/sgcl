[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::set_multicast_loopback

```cpp
expected<void, io::error> set_multicast_loopback(bool on) const noexcept;
```

Sets whether the socket's datagrams to a multicast group reach the group's members on this machine too:
`IP_MULTICAST_LOOP` on an IPv4 socket, `IPV6_MULTICAST_LOOP` on an IPv6 one. On by default. It is the sender's
setting: a member receives what a sender with it on sends, whatever its own says.

## Parameters

| Parameter | Description |
|---|---|
| `on` | `true`: this machine's members get the datagrams too; `false`: the network's alone |

## Return value

Nothing, or the [io::error](../../io/error/README.md), its operation `multicast loopback` and its path the socket:
`io::errc::closed` for a closed socket, the system's `errno` otherwise.

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
    net::udp::socket member = net::udp::listen_multicast("239.255.10.5:0", lo);
    net::endpoint to(net::ip_address("239.255.10.5"), member.local_endpoint().port());
    net::udp::socket sender = net::udp::bind("127.0.0.1:0");
    sender.set_multicast_interface(lo);
    vector<byte> room(64);

    sender.set_multicast_loopback(false);
    sender.send_to("kept", to);
    member.set_read_deadline(clock::now() + 200ms);
    println("{}", member.receive_from(room).error().is_timeout());

    sender.set_multicast_loopback(true);
    sender.send_to("looped", to);
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

- [multicast_loopback](multicast_loopback.md): the setting
- [udp::socket](README.md)
