[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md)

# sgcl::net::amqp::consumer

```cpp
#include "sgcl/net/amqp/client.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    class consumer;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::consumer` is a consumer of a queue ([consume](../channel/consume.md)): its
[deliveries](../delivery.md) in the order the broker sent them, kept until they are received. The `<-chan Delivery`
of Go's amqp091-go, read by [receive](receive.md).

## Rules

- A handle of one word: a copy is the same consumer.
- `client::options::queue` deliveries are kept; past it the connection's reader waits for the program, so a
  consumer that stops receiving stops the connection. [qos](../channel/qos.md) keeps the broker from sending more
  than are acknowledged.
- The consumer ends with [cancel](cancel.md), its channel's end, or the broker's cancel (its queue deleted): what
  was received before stays to be read, then receive fails with the reason.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](consumer.md) | no consumer, or a copy of one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another consumer |
| [receive, async_receive](receive.md) | the next delivery |
| [try_receive](try_receive.md) | the next delivery when one is there |
| [tag](tag.md) | the consumer's tag |
| [cancel, async_cancel](cancel.md) | no more deliveries |
| [operator bool](operator_bool.md) | whether the handle holds a consumer |
| [operator==](operator_cmp.md) | whether two handles are the same consumer |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    ch.declare_queue("work").value();
    ch.qos(1).value();
    net::amqp::consumer in = ch.consume("work").value();
    for (int i : range(3)) {
        ch.publish("", "work", string::concat("job ", to_string(i))).value();
    }
    for (int i : range(3)) {
        auto d = in.receive().value();
        println("{}", d.body);
        ch.ack(d.delivery_tag);
    }
}
```

Output:

```text
job 0
job 1
job 2
```

## See also

- [consume](../channel/consume.md)
- [delivery](../delivery.md)
