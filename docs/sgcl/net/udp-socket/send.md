[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::send, async_send

```cpp
expected<size_t, io::error> send(const slice<const byte>& data) const;                                // (1)
async::task<expected<size_t, io::error>> async_send(const slice<const byte>& data) const noexcept;    // (2)
```

Sends `data` as one datagram to the peer of a socket of [udp::connect](../udp/connect.md): the `Write` of the
`net.Conn` Go's `net.Dial("udp")` gives. A datagram is sent whole or not at all; `ENOBUFS` is an error, not a wait.
A socket of [udp::bind](../udp/bind.md) has no peer, and its `send` fails with `EDESTADDRREQ`: it sends with
[send_to](send_to.md).

1. On the calling thread; a send that would block waits on the [reactor](../../async/readable.md).
2. The same for a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the datagram |

## Return value

The number of bytes sent, all of `data`. Or the [io::error](../../io/error/README.md), its operation `write` and its path
the socket: `io::errc::closed` after [close](close.md), `ETIMEDOUT` (`is_timeout()`) when the write deadline
passed, `EDESTADDRREQ` for a socket with no peer, `EMSGSIZE` for a datagram too long, the `errno` of `send`
otherwise.

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
    net::udp::socket server = net::udp::bind("127.0.0.1:0");
    net::udp::socket client = net::udp::connect(server.local_endpoint().to_string());
    println("{}", client.send("ping").value());

    vector<byte> room(64);
    println("{}", server.receive_from(room).value().size);

    auto unbound = server.send("no peer");
    println("{}", unbound.error().code() == std::errc::destination_address_required);
}
```

Output:

```text
4
4
true
```

## See also

- [receive, async_receive](receive.md): the other direction
- [send_to, async_send_to](send_to.md): to any address
- [sgcl::net::udp::socket](README.md)
