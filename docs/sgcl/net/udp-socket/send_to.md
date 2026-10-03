[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::send_to, async_send_to

```cpp
expected<size_t, io::error> send_to(const slice<const byte>& data,                           // (1)
                                    const endpoint& to) const;
async::task<expected<size_t, io::error>> async_send_to(const slice<const byte>& data,        // (2)
                                                      const endpoint& to) const noexcept;
```

Sends `data` as one datagram to `to`: Go's `PacketConn.WriteTo`. A datagram is sent whole or not at all. An IPv4
address given to a socket of both families goes through the IPv4-mapped form; an IPv6 address given to an IPv4
socket is `EAFNOSUPPORT`, and one whose zone names no interface `net::errc::invalid_address`, as a connect's.
`ENOBUFS` (the interface's queue full) is an error, as in Go, not a wait. A `string` or a
literal is taken as its bytes.

1. On the calling thread; a send that would block waits on the [reactor](../../async/readable.md).
2. The same for a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the datagram |
| `to` | the address and port to send it to |

## Return value

The number of bytes sent, all of `data`. Or the [io::error](../../io/error/README.md), its operation `write`:
`EAFNOSUPPORT` for an address of the other family and `net::errc::invalid_address` for a zone that names no
interface, their path the address; `io::errc::closed` after
[close](close.md), `ETIMEDOUT` (`is_timeout()`) when the write deadline passed, `EMSGSIZE` for a datagram too long,
the `errno` of `sendto` otherwise, their path the socket.

## Complexity

One system call, linear in the size of the datagram.

## Exceptions

- (1) `std::system_error` when the send has to wait and the thread of the reactor, or of the timers, which its
  first use starts, cannot be made.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::udp::socket both = net::udp::bind(":0");  // both families
    net::udp::socket v4 = net::udp::bind("127.0.0.1:0");
    println("{}", both.send_to("to IPv4", v4.local_endpoint()).value());

    vector<byte> room(64);
    net::udp::datagram d = v4.receive_from(room).value();
    println("{} {}", d.size, d.from.address());

    net::endpoint v6(net::ip_address::loopback_v6(), 9);
    auto e = v4.send_to("to IPv6", v6);
    println("{}", e.error().message());
}
```

Output:

```text
7
7 127.0.0.1
write [::1]:9: Address family not supported by protocol family
```

## See also

- [receive_from, async_receive_from](receive_from.md): the other direction
- [send, async_send](send.md): to a connected socket's peer
- [sgcl::net::udp::socket](README.md)
