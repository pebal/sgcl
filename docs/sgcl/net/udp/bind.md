[sgcl](../../README.md) › [net](../README.md) › [udp](README.md)

# sgcl::net::udp::bind, async_bind

```cpp
static expected<udp::socket, io::error> bind(const string& address) noexcept;                       // (1)
static async::task<expected<udp::socket, io::error>> async_bind(const string& address) noexcept;    // (2)
```

Makes a UDP socket bound to `address`, which receives datagrams from anyone and sends them to any address: Go's
`net.ListenPacket("udp", address)`. `address` is `"host:port"`:

- no host (`":5353"`) is every address of both families: the IPv6 wildcard with `IPV6_V6ONLY` off, one socket for
  both, as [tcp::listen](../tcp/listen.md) takes it; an IPv4 socket on a system without IPv6;
- an address is that address;
- a name binds its first IPv4 address, or its first.

A port 0 lets the system choose one, which the socket's [local_endpoint](../udp-socket/local_endpoint.md) tells.

1. On the calling thread; only the lookup of a name waits.
2. The same for a task, which waits for the lookup without holding a worker.

## Parameters

| Parameter | Description |
|---|---|
| `address` | `"host:port"`; the port a number, never a service name |

## Return value

The [udp::socket](../udp-socket/README.md); or the [io::error](../../io/error/README.md), its operation `bind udp` (`lookup` for
a failure of the lookup) and its path the address given: `net::errc::invalid_address` for an address that is not
`"host:port"`, `net::errc::host_not_found` for a name nobody knows, `EADDRINUSE` for a port taken, the `errno` of
the socket otherwise.

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
    net::udp::socket local = net::udp::bind("127.0.0.1:0");
    println("{}", local.local_endpoint().address());

    net::udp::socket both = net::udp::bind(":0");
    println("{}", both.local_endpoint().address());

    auto bad = net::udp::bind("5353");
    println("{}", bad.error().message());
}
```

Output:

```text
127.0.0.1
::
bind udp 5353: invalid address
```

## See also

- [connect, async_connect](connect.md): a socket with its peer fixed
- [udp::socket](../udp-socket/README.md): what it gives
- [sgcl::net::udp](README.md)
