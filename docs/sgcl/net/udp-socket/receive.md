[sgcl](../../README.md) › [net](../README.md) › [udp](../udp/README.md) › [socket](README.md)

# sgcl::net::udp::socket::receive, async_receive

```cpp
expected<size_t, io::error> receive(const slice<byte>& buffer) const;                                // (1)
async::task<expected<size_t, io::error>> async_receive(const slice<byte>& buffer) const noexcept;    // (2)
```

Receives the next datagram into `buffer`, waiting until one comes: the `Read` of the `net.Conn` Go's
`net.Dial("udp")` gives. It is [receive_from](receive_from.md) without the sender, for a socket of
[udp::connect](../udp/connect.md), which the system lets receive from its peer alone. A datagram longer than
`buffer` is cut to it, and nothing says so; `receive_from` does.

1. On the calling thread, which waits on the [reactor](../../async/readable.md) meanwhile.
2. The same for a task, which holds no worker while it waits.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the datagram goes; its size is the most kept |

## Return value

The number of bytes in `buffer`. Or the [io::error](../../io/error/README.md), its operation `read` and its path the
socket: `io::errc::closed` after [close](close.md), `ETIMEDOUT` (`is_timeout()`) when the read deadline passed, the
`errno` of `recvmsg` otherwise.

## Complexity

One system call, and one more per wait for readiness.

## Exceptions

- (1) `std::system_error` when the receive has to wait and the thread of the reactor, or of the timers, which its
  first use starts, cannot be made.
- (2) None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    net::udp::socket peer = net::udp::bind("127.0.0.1:0");
    net::udp::socket other = net::udp::bind("127.0.0.1:0");
    net::udp::socket s = net::udp::connect(peer.local_endpoint().to_string());

    other.send_to("from someone else", s.local_endpoint());  // dropped by the system
    peer.send_to("from the peer", s.local_endpoint());
    vector<byte> room(64);
    size_t n = s.receive(room).value();
    room.resize(n);
    println("{}", string(room));

    s.set_read_deadline(clock::now() + 20ms);
    println("{}", s.receive(room).error().is_timeout());
}
```

Output:

```text
from the peer
true
```

## See also

- [send, async_send](send.md): the other direction
- [receive_from, async_receive_from](receive_from.md): with the sender
- [sgcl::net::udp::socket](README.md)
