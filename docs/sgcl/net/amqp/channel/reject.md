[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::reject, async_reject

```cpp
expected<void, io::error> reject(uint64_t delivery_tag, bool requeue = true) const;                                // (1)
async::task<expected<void, io::error>> async_reject(uint64_t delivery_tag, bool requeue = true) const noexcept;    // (2)
```

basic.reject: one delivery refused, back to its queue or dropped.

`reject` waits on the calling thread; a task awaits `async_reject`.

## Parameters

| Parameter | Description |
|---|---|
| `delivery_tag` | the delivery's `delivery_tag` |
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
    ch.declare_queue("bad").value();
    ch.publish("", "bad", "poison").value();
    auto d = ch.get("bad").value();
    ch.reject(d->delivery_tag, false).value();
    println("{}", bool(ch.get("bad").value()));
}
```

Output:

```text
false
```

## See also

- [nack](nack.md)
- [channel](README.md)
