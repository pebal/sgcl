[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [client](README.md)

# sgcl::net::amqp::client::open_channel, async_open_channel

```cpp
expected<amqp::channel, io::error> open_channel() const;                                // (1)
async::task<expected<amqp::channel, io::error>> async_open_channel() const noexcept;    // (2)
```

channel.open: a channel of a number of its own on the connection. A program opens one per task that publishes or
consumes, as a channel's synchronous methods are taken one at a time.

`open_channel` waits on the calling thread; a task awaits `async_open_channel`.

## Parameters

None.

## Return value

The channel; `errc::channel_error` when every number up to the channel limit is in use, the connection's end.

## Complexity

A round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel a = c.open_channel().value();
    net::amqp::channel b = c.open_channel().value();
    println("{} {}", a.id(), b.id());
}
```

Output:

```text
1 2
```

## See also

- [channel](../channel/README.md)
- [client](README.md)
