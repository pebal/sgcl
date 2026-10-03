[sgcl](../../README.md) › [net](../README.md) › [tcp](../tcp.md)

# sgcl::net::tcp::listen, async_listen

```cpp
/*(1)*/ static expected<net::listener, io::error> listen(const string& address) noexcept;
/*(2)*/ static expected<net::listener, io::error> listen(const string& address,
                                                         net::reuse_port_t flag) noexcept;
/*(3)*/ static async::task<expected<net::listener, io::error>> async_listen(const string& address) noexcept;
/*(4)*/ static async::task<expected<net::listener, io::error>> async_listen(const string& address,
                                                                            net::reuse_port_t flag) noexcept;
```

Listens for TCP connections on `address`: Go's `net.Listen("tcp", address)`. The socket is bound with
`SO_REUSEADDR` and listens with the system's backlog. `address` is `"host:port"`:

- no host (`":8080"`) is every address of both families: the IPv6 wildcard with `IPV6_V6ONLY` off, one socket for
  both, as in Go; an IPv4 socket on a system without IPv6;
- an address (`"127.0.0.1:8080"`, `"[::1]:8080"`) is that address;
- a name listens on its first IPv4 address, or its first, looked up as [dns::lookup](../dns/lookup.md) does.

A port 0 lets the system choose one, which the listener's [local_endpoint](../listener/local_endpoint.md) tells.

- (2, 4) With `net::reuse_port`, `SO_REUSEPORT`: other sockets, in this process or another, may listen on the same
  port with the flag too, and the kernel spreads the connections among them.
- (1–2) On the calling thread; only the lookup of a name waits.
- (3–4) The same for a task, which waits for the lookup without holding a worker.

## Parameters

| Parameter | Description |
|---|---|
| `address` | `"host:port"`; the port a number, never a service name |
| `flag` | `net::reuse_port`, the port shared |

## Return value

The [listener](../listener.md); or the [io::error](../../io/error.md), its operation `listen tcp` (`lookup` for a
failure of the lookup) and its path the address given: `net::errc::invalid_address` for an address that is not
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
    net::listener first = net::tcp::listen("127.0.0.1:0", net::reuse_port);
    string address = first.local_endpoint().to_string();
    auto second = net::tcp::listen(address, net::reuse_port);  // the same port, shared
    println("{}", second.has_value());

    auto taken = net::tcp::listen(address);  // without the flag the port is taken
    println("{}", taken.error().code() == std::errc::address_in_use);

    auto bad = net::tcp::listen("127.0.0.1:99999");
    println("{}", bad.error().message());
}
```

Output:

```text
true
true
listen tcp 127.0.0.1:99999: invalid address
```

## See also

- [connect, async_connect](connect.md): the other side
- [listener](../listener.md): what it gives
- [unix_domain::listen](../unix_domain/listen.md), [tls::listen](../tls/listen.md): the other listeners
- [sgcl::net::tcp](../tcp.md)
