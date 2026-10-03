[sgcl](../README.md) › [net](README.md)

# sgcl::net::udp

```cpp
#include "sgcl/net/socket.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct udp {
        class socket;      // what bind and connect give
        struct datagram;   // what a receive_from gives
    };
}
```

`sgcl::net::udp` is UDP: [bind](udp/bind.md) takes an address to receive datagrams on, from anyone, and
[connect](udp/connect.md) fixes the peer, so that [send](udp-socket/send.md) and
[receive](udp-socket/receive.md) need no address and the system drops the datagrams of anyone else. Both give a
[udp::socket](udp-socket.md); a datagram received from anyone is a [udp::datagram](udp-datagram.md), its size, its
sender and whether it was cut. It is a structure of static functions, as [tcp](tcp.md) is: `net::udp::bind(a)` is
Go's `net.ListenPacket("udp", a)`, `net::udp::connect(a)` its `net.Dial("udp", a)`, and the socket's
[receive_from](udp-socket/receive_from.md) and [send_to](udp-socket/send_to.md) are `ReadFrom` and `WriteTo`.

UDP waits for nothing on the network: `bind` and `connect` differ from their `async_` forms only in the lookup of a
name. An address is `"host:port"`, as [tcp](tcp.md) takes it.

## Rules

- `bind` with no host (`":5353"`) receives on every address of both families, as [tcp::listen](tcp/listen.md)
  listens: the IPv6 wildcard with `IPV6_V6ONLY` off, an IPv4 socket on a system without IPv6. A name binds its first
  IPv4 address, or its first.
- `connect` with no host is `127.0.0.1`; a name is its first address in the resolver's order, with no race: nothing
  answers a datagram to tell which address is alive.
- A datagram is sent whole or not at all; one longer than the buffer of a receive is cut, and says so.
- Errors are values, [expected\<T, io::error\>](../io/error.md), the operation named as Go names it: `bind udp`,
  `dial udp`, `read`, `write`.

## Member types

| Type | Definition |
|---|---|
| [socket](udp-socket.md) | a UDP socket, what `bind` and `connect` give |
| [datagram](udp-datagram.md) | a datagram received: its size, its sender, whether it was cut |

## Member functions

| Function | Description |
|---|---|
| [bind, async_bind](udp/bind.md) | makes a socket that receives on an address (static) |
| [connect, async_connect](udp/connect.md) | makes a socket with its peer fixed (static) |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

// Sends every datagram back to its sender
async::task<> echo(net::udp::socket s) {
    vector<byte> datagram(8192);
    slice<byte> room = datagram;
    while (auto d = co_await s.async_receive_from(room)) {
        co_await s.async_send_to(room.first(d->size), d->from);
    }
}

int main() {
    net::udp::socket server = net::udp::bind("127.0.0.1:0");
    auto serving = async::spawn(echo(server));

    net::udp::socket client = net::udp::connect(server.local_endpoint().to_string());
    client.send("ping");
    vector<byte> answer(64);
    size_t n = client.receive(answer).value();
    answer.resize(n);
    println("{}", string(answer));

    server.close();  // the receive in progress ends, and so does the loop
    serving.wait();
}
```

Output:

```text
ping
```

## See also

- [udp::socket](udp-socket.md), [udp::datagram](udp-datagram.md): what it makes and receives
- [tcp](tcp.md): streams
- `tests/net/socket.cpp`: UDP, a datagram cut, an empty buffer, deadlines, dual stack
