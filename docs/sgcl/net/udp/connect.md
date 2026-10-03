[sgcl](../../README.md) › [net](../README.md) › [udp](../udp.md)

# sgcl::net::udp::connect, async_connect

```cpp
/*(1)*/ static expected<udp::socket, io::error> connect(const string& address) noexcept;
/*(2)*/ static async::task<expected<udp::socket, io::error>> async_connect(const string& address) noexcept;
```

Makes a UDP socket whose peer is `address`: Go's `net.Dial("udp", address)`. [send](../udp-socket/send.md) and
[receive](../udp-socket/receive.md) then need no address, and the system drops the datagrams of anyone else.
Nothing goes on the network: a datagram socket has no handshake. `address` is `"host:port"`; no host is
`127.0.0.1`, and a name is its first address in the resolver's order, with no race, since nothing answers a
datagram to tell which address is alive.

1. On the calling thread; only the lookup of a name waits.
2. The same for a task, which waits for the lookup without holding a worker.

## Parameters

| Parameter | Description |
|---|---|
| `address` | `"host:port"` of the peer; the port a number, never a service name |

## Return value

The [udp::socket](../udp-socket.md), its [remote_endpoint](../udp-socket/remote_endpoint.md) the peer; or the
[io::error](../../io/error.md), its operation `dial udp` (`lookup` for a failure of the lookup) and its path the
address given: `net::errc::invalid_address` for an address that is not `"host:port"`, `net::errc::host_not_found`
for a name nobody knows, the `errno` of the socket otherwise.

## Complexity

A few system calls, and the lookup of a name.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::udp::socket server = net::udp::bind("127.0.0.1:0");
    net::udp::socket client = net::udp::connect(server.local_endpoint().to_string());
    println("{}", client.remote_endpoint() == server.local_endpoint());

    net::udp::socket here = net::udp::connect(":53");
    println("{}", here.remote_endpoint());
}
```

Output:

```text
true
127.0.0.1:53
```

## See also

- [bind, async_bind](bind.md): a socket that receives from anyone
- [send](../udp-socket/send.md), [receive](../udp-socket/receive.md): what a connected socket does
- [sgcl::net::udp](../udp.md)
