[sgcl](../../README.md) › [net](../README.md) › [udp](../udp.md) › [socket](../udp-socket.md)

# sgcl::net::udp::socket::receive_from, async_receive_from

```cpp
/*(1)*/ expected<udp::datagram, io::error> receive_from(const slice<byte>& buffer) const;
/*(2)*/ async::task<expected<udp::datagram, io::error>> async_receive_from(const slice<byte>& buffer) const noexcept;
```

Receives the next datagram into `buffer`, waiting until one comes, and says who sent it: Go's
`PacketConn.ReadFrom`. A datagram longer than `buffer` is cut to it, the rest lost, and `truncated` says so. An
empty `buffer` takes the next datagram too, `size` 0 and `truncated` when it had bytes (macOS alone would answer an
empty buffer with a datagram of nothing and leave the real one queued).

1. On the calling thread, which waits on the [reactor](../../async/readable.md) meanwhile.
2. The same for a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the datagram goes; its size is the most kept |

## Return value

The [udp::datagram](../udp-datagram.md): the bytes in `buffer`, the sender (an IPv4 peer of a socket of both
families reported as IPv4), whether it was cut. Or the [io::error](../../io/error.md), its operation `read` and its
path the socket: `io::errc::closed` after [close](close.md), `ETIMEDOUT` (`is_timeout()`) when the read deadline
passed, the `errno` of `recvmsg` otherwise.

## Complexity

One system call, and one more per wait for readiness.

## Exceptions

- (1) `std::system_error` when the receive has to wait and the thread of the reactor, or of the timers, which its
  first use starts, cannot be made.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::udp::socket server = net::udp::bind("127.0.0.1:0");
    net::udp::socket client = net::udp::bind("127.0.0.1:0");
    client.send_to("hello, datagram", server.local_endpoint());

    vector<byte> small(5);
    net::udp::datagram d = server.receive_from(small).value();
    println("{} {} {}", d.size, d.truncated, d.from == client.local_endpoint());
    println("{}", string(small));
}
```

Output:

```text
5 true true
hello
```

## See also

- [send_to, async_send_to](send_to.md): the answer, to `from`
- [receive, async_receive](receive.md): from a connected socket's peer
- [udp::datagram](../udp-datagram.md): what it gives
- [sgcl::net::udp::socket](../udp-socket.md)
