[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::nack, async_nack

```cpp
expected<void, io::error> nack(uint64_t delivery_tag, bool multiple = false, bool requeue = true) const;                                // (1)
async::task<expected<void, io::error>> async_nack(uint64_t delivery_tag, bool multiple = false, bool requeue = true) const noexcept;    // (2)
```

basic.nack (RabbitMQ's): the delivery refused, and every one up to it with `multiple`: back to its queue, marked
redelivered, or dropped (dead-lettered where the queue says) without `requeue`.

`nack` waits on the calling thread; a task awaits `async_nack`.

## Parameters

| Parameter | Description |
|---|---|
| `delivery_tag` | the delivery's `delivery_tag` |
| `multiple` | every one up to it |
| `requeue` | back to the queue; false: dropped |


## Return value

Nothing; the channel's end.

## Complexity

Constant.

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
    ch.declare_queue("retry").value();
    ch.publish("", "retry", "flaky").value();
    auto d = ch.get("retry").value();
    ch.nack(d->delivery_tag).value();
    auto again = ch.get("retry", true).value();
    println("{} {}", again->body, again->redelivered);
}
```

Output:

```text
flaky true
```

## See also

- [ack](ack.md)
- [reject](reject.md)
- [channel](README.md)
