[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::close, async_close

```cpp
expected<void, io::error> close() const;                                // (1)
async::task<expected<void, io::error>> async_close() const noexcept;    // (2)
```

channel.close: the channel ended, its unacknowledged deliveries back to their queues, its consumers ended.

`close` waits on the calling thread; a task awaits `async_close`.

## Parameters

None.

## Return value

Nothing; the channel's end before.

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
    net::amqp::channel ch = c.open_channel().value();
    println("{}", bool(ch.close()));
    println("{}", bool(ch.declare_queue("after")));
}
```

Output:

```text
true
false
```

## See also

- [open_channel](../client/open_channel.md)
- [channel](README.md)
