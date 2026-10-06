[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::qos, async_qos

```cpp
expected<void, io::error> qos(uint16_t prefetch_count, bool global = false) const;                                // (1)
async::task<expected<void, io::error>> async_qos(uint16_t prefetch_count, bool global = false) const noexcept;    // (2)
```

basic.qos: how many deliveries the broker sends a consumer of the channel before they are acknowledged — each
consumer's count, or all of the channel's together with `global` (RabbitMQ's reading). Zero: no limit. The way to
share a queue's work fairly among consumers and to bound what a slow one holds.

`qos` waits on the calling thread; a task awaits `async_qos`.

## Parameters

| Parameter | Description |
|---|---|
| `prefetch_count` | the deliveries without acknowledgement at most; 0: no limit |
| `global` | the channel's consumers together |


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
    ch.qos(10).value();
    println("prefetch set");
}
```

Output:

```text
prefetch set
```

## See also

- [consume](consume.md)
- [channel](README.md)
