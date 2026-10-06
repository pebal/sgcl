[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::confirm, async_confirm

```cpp
expected<void, io::error> confirm() const;                                // (1)
async::task<expected<void, io::error>> async_confirm() const noexcept;    // (2)
```

confirm.select: publisher confirms on the channel from now on. Each [publish](publish.md) then waits until the
broker acknowledges the message (it has it, on disk for a persistent message to a durable queue) and fails with
`errc::nacked` when it refuses it.

`confirm` waits on the calling thread; a task awaits `async_confirm`.

## Parameters

None.

## Return value

Nothing.

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
    ch.declare_queue("ledger", {.durable = true}).value();
    ch.confirm().value();
    net::amqp::properties p;
    p.delivery_mode = net::amqp::delivery_mode::persistent;
    println("{}", bool(ch.publish("", "ledger", "entry 1", p)));
}
```

Output:

```text
true
```

## See also

- [publish](publish.md)
- [channel](README.md)
