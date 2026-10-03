[sgcl](../README.md) › [net](README.md) › [udp](udp.md) › socket

# sgcl::net::udp::socket

```cpp
#include "sgcl/net/socket.h"   // or "sgcl/net.h"

namespace sgcl::net {
    struct udp {
        class socket;
    };
}
```

`sgcl::net::udp::socket` is a UDP socket: what [udp::bind](udp/bind.md) and [udp::connect](udp/connect.md) give.
Bound, it receives datagrams from anyone ([receive_from](udp-socket/receive_from.md), which says who sent each, a
[udp::datagram](udp-datagram.md)) and sends them to any address ([send_to](udp-socket/send_to.md)): Go's
`net.PacketConn` with its `ReadFrom` and `WriteTo`. Connected, it sends to its one peer and receives from it alone
([send](udp-socket/send.md), [receive](udp-socket/receive.md)), the `Read` and `Write` of the `net.Conn` Go's
`net.Dial("udp")` gives.

A socket is a handle of one word, a `tracked_ptr` to the object inside, as a [connection](connection.md) is: a copy
is the same socket, and a handle passed by value into a task keeps it alive for as long as the task runs. Every
operation that may wait comes twice, a blocking form and an `async_` one, both on the
[reactor](../async/readable.md), so that a close from any task and a deadline end either.

## Rules

- A socket made by the default constructor holds none (`!s`); an operation on it is a contract violation (debug
  builds assert).
- A handle is a tracked word: on a stack, in a task, in a managed object; in a global or a `std` container, a
  [rooted](../core/rooted.md) of it. It takes part in the atomics by its word
  ([atomic](../core/atomic-handle.md)), `atomic<net::udp::socket>`, compared by identity.
- A datagram is sent whole or not at all. `ENOBUFS` (the interface's queue full) is an error, as in Go, not a wait:
  the socket has room, so a wait for it to be writable would come back at once.
- A datagram longer than the buffer of a receive is cut to it, and `truncated` says so (`MSG_TRUNC`).
- The deadlines are absolute, on the module's [clock](../core/clock.md), as a connection's are
  ([set_deadline](udp-socket/set_deadline.md)).
- [close](udp-socket/close.md) from another task ends the receives and sends in progress with `io::errc::closed`.
  A socket not closed is closed by its destructor, on the collector's thread after the sweep that finds it dead.
- Errors are values, [expected\<T, io::error\>](../io/error.md), the operation `read` or `write` and the path the
  socket (`udp 127.0.0.1:5353`, `udp 127.0.0.1:50000->127.0.0.1:53` once connected).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](udp-socket/udp-socket.md) | constructs the handle: no socket, or a copy that is the same socket |
| `(destructor)` | releases the handle; the socket is closed by `close`, or when the collector finds it dead |
| `operator=` | makes the handle the same socket as another |

#### Datagrams

| Function | Description |
|---|---|
| [receive_from, async_receive_from](udp-socket/receive_from.md) | receives the next datagram, with its sender |
| [send_to, async_send_to](udp-socket/send_to.md) | sends a datagram to an address |
| [receive, async_receive](udp-socket/receive.md) | receives the next datagram of a connected socket's peer |
| [send, async_send](udp-socket/send.md) | sends a datagram to a connected socket's peer |

#### Closing

| Function | Description |
|---|---|
| [close](udp-socket/close.md) | closes the socket, and ends the operations in progress |
| [is_closed](udp-socket/is_closed.md) | checks whether the socket was closed |

#### Deadlines

| Function | Description |
|---|---|
| [set_deadline](udp-socket/set_deadline.md) | sets the deadline of both directions |
| [set_read_deadline](udp-socket/set_read_deadline.md) | sets the deadline of the receives |
| [set_write_deadline](udp-socket/set_write_deadline.md) | sets the deadline of the sends |
| [read_deadline](udp-socket/read_deadline.md) | the deadline of the receives |
| [write_deadline](udp-socket/write_deadline.md) | the deadline of the sends |

#### Observers

| Function | Description |
|---|---|
| [local_endpoint](udp-socket/local_endpoint.md) | the address the socket is bound to |
| [remote_endpoint](udp-socket/remote_endpoint.md) | the peer of a connected socket |
| [operator bool](udp-socket/operator_bool.md) | checks whether the handle holds a socket |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](udp-socket/operator_cmp.md) | checks whether two handles are the same socket |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

// Answers every datagram with its size
async::task<> measure(net::udp::socket s) {
    vector<byte> room(512);
    while (auto d = co_await s.async_receive_from(room)) {
        co_await s.async_send_to(to_string(d->size), d->from);
    }
}

int main() {
    net::udp::socket server = net::udp::bind("127.0.0.1:0");
    auto serving = async::spawn(measure(server));

    net::udp::socket client = net::udp::bind("127.0.0.1:0");
    vector<byte> answer(16);
    for (string text : {"a", "datagram"}) {
        client.send_to(text, server.local_endpoint());
        net::udp::datagram d = client.receive_from(answer).value();
        answer.resize(d.size);
        println("{}: {}", text, string(answer));
        answer.resize(16);
    }
    server.close();
    serving.wait();
}
```

Output:

```text
a: 1
datagram: 8
```

## See also

- [udp](udp.md): what makes a socket
- [udp::datagram](udp-datagram.md): what a `receive_from` gives
- [connection](connection.md): a stream
