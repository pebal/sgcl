[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::ack, async_ack

```cpp
expected<void, io::error> ack(uint64_t delivery_tag, bool multiple = false) const;                                // (1)
async::task<expected<void, io::error>> async_ack(uint64_t delivery_tag, bool multiple = false) const noexcept;    // (2)
```

basic.ack: the delivery done, the broker forgets it; with `multiple`, every delivery of the channel up to it too.
Written with the next frames, not waited for; a tag the channel does not have closes it (406).

`ack` waits on the calling thread; a task awaits `async_ack`.

## Parameters

| Parameter | Description |
|---|---|
| `delivery_tag` | the delivery's `delivery_tag` |
| `multiple` | every one up to it |


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
    ch.declare_queue("acks").value();
    ch.publish("", "acks", "work").value();
    auto d = ch.get("acks").value();
    println("{}", bool(ch.ack(d->delivery_tag)));
}
```

Output:

```text
true
```

## See also

- [nack](nack.md)
- [reject](reject.md)
- [channel](README.md)
