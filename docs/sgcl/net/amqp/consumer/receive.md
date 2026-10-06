[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [consumer](README.md)

# sgcl::net::amqp::consumer::receive, async_receive

```cpp
expected<delivery, io::error> receive() const;                                // (1)
async::task<expected<delivery, io::error>> async_receive() const noexcept;    // (2)
```

The next delivery, waited for. After the consumer's end, the ones received before, then the reason.

`receive` waits on the calling thread; a task awaits `async_receive`.

## Parameters

None.

## Return value

The [delivery](../delivery.md); after the end, its reason (`io::errc::closed` after [cancel](cancel.md), `errc::not_found` when the broker cancelled it, the channel's or the connection's end).

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
    ch.declare_queue("q").value();
    net::amqp::consumer in = ch.consume("q").value();
    ch.publish("", "q", "hello").value();
    auto d = in.receive().value();
    println("{} {} {}", d.body, d.routing_key, d.redelivered);
    ch.ack(d.delivery_tag);
}
```

Output:

```text
hello q false
```

## See also

- [try_receive](try_receive.md)
- [ack](../channel/ack.md)
- [consumer](README.md)
