[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::consume, async_consume

```cpp
expected<consumer, io::error> consume(const string& queue, const consume_options& o = {}) const;                  // (1)
async::task<expected<consumer, io::error>> async_consume(string queue, consume_options o = {}) const noexcept;    // (2)
```

basic.consume: the queue's messages, as the broker sends them, to a [consumer](../consumer/README.md) that
receives them in order. Several consumers of one queue get its messages in turn; [qos](qos.md) bounds how many each
holds unacknowledged.

`consume` waits on the calling thread; a task awaits `async_consume`.

## Parameters

| Parameter | Description |
|---|---|
| `queue` | the queue |
| `o` | the tag, no_ack, exclusive, the arguments |


## Return value

The consumer; `errc::not_found` for a queue that is not there.

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
    ch.declare_queue("jobs").value();
    net::amqp::consumer in = ch.consume("jobs").value();
    ch.publish("", "jobs", "resize image 17").value();
    auto d = in.receive().value();
    println("{}", d.body);
    ch.ack(d.delivery_tag);
}
```

Output:

```text
resize image 17
```

## See also

- [consumer](../consumer/README.md)
- [consume_options](../consume_options.md)
- [qos](qos.md)
- [channel](README.md)
