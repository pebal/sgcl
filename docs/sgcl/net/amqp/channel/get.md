[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::get, async_get

```cpp
expected<optional<delivery>, io::error> get(const string& queue, bool no_ack = false) const;                         // (1)
async::task<expected<optional<delivery>, io::error>> async_get(string queue, bool no_ack = false) const noexcept;    // (2)
```

basic.get: the queue's next message, or none when it is empty. A round trip a message: a consumer is the way to
read many.

`get` waits on the calling thread; a task awaits `async_get`.

## Parameters

| Parameter | Description |
|---|---|
| `queue` | the queue |
| `no_ack` | acknowledged by the broker as it sends: no [ack](ack.md) needed |


## Return value

The [delivery](../delivery.md) (its `message_count` the messages left), or none; `errc::not_found`.

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
    ch.declare_queue("inbox").value();
    ch.publish("", "inbox", "first").value();
    ch.publish("", "inbox", "second").value();
    auto d = ch.get("inbox").value();
    println("{} {}", d->body, d->message_count);
    ch.ack(d->delivery_tag);
}
```

Output:

```text
first 1
```

## See also

- [consume](consume.md)
- [delivery](../delivery.md)
- [channel](README.md)
