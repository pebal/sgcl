[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [consumer](README.md)

# sgcl::net::amqp::consumer::cancel, async_cancel

```cpp
expected<void, io::error> cancel() const;                                // (1)
async::task<expected<void, io::error>> async_cancel() const noexcept;    // (2)
```

basic.cancel: no more deliveries. Those received stay to be read and acknowledged; those not acknowledged when the
channel closes go back to the queue.

`cancel` waits on the calling thread; a task awaits `async_cancel`.

## Parameters

None.

## Return value

Nothing; the channel's end.

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
    ch.declare_queue("q").value();
    net::amqp::consumer in = ch.consume("q").value();
    in.cancel().value();
    println("{}", in.receive().error().code() == io::errc::closed);
}
```

Output:

```text
true
```

## See also

- [consume](../channel/consume.md)
- [consumer](README.md)
