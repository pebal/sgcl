[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [subscription](README.md)

# sgcl::net::nats::subscription::unsubscribe, async_unsubscribe

```cpp
expected<void, io::error> unsubscribe(uint64_t max = 0) const;                                // (1)
async::task<expected<void, io::error>> async_unsubscribe(uint64_t max = 0) const noexcept;    // (2)
```

UNSUB: no more messages; or, with `max`, after the subscription's `max`-th message, counted from its start as the
server counts them. The messages received stay to be read.

`unsubscribe` waits on the calling thread; a task awaits `async_unsubscribe`.

## Parameters

| Parameter | Description |
|---|---|
| `max` | the messages after which it ends; 0: at once |


## Return value

Nothing; the subscription's end before, the connection's end.

## Complexity

Constant.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    auto once = nc.subscribe("single").value();
    once.unsubscribe(1).value();
    nc.publish("single", "first").value();
    nc.publish("single", "second").value();
    println("{}", once.receive().value().data);
    println("{}", bool(once.receive()));
}
```

Output:

```text
first
false
```

## See also

- [subscribe](../client/subscribe.md)
- [subscription](README.md)
