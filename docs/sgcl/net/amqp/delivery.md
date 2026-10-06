[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::delivery

```cpp
#include "sgcl/net/amqp/types.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    struct delivery {
        string body;
        amqp::properties properties;
        string consumer_tag;
        uint64_t delivery_tag = 0;
        bool redelivered = false;
        string exchange;
        string routing_key;
        uint32_t message_count = 0;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::delivery` is a message as a [consumer](consumer/README.md) receives it, or [get](channel/get.md).

## Member objects

| Member | Description |
|---|---|
| `body` | the message's bytes |
| `properties` | its [properties](properties.md) |
| `consumer_tag` | the consumer's tag; empty for get |
| `delivery_tag` | what [ack](channel/ack.md), nack and reject take, on this channel |
| `redelivered` | delivered before and not acknowledged |
| `exchange` | the exchange it was published to |
| `routing_key` | its routing key |
| `message_count` | get: the messages left in the queue |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    ch.declare_queue("d").value();
    ch.publish("", "d", "payload").value();
    auto d = ch.get("d").value();
    println("[{}] {} {}", d->exchange, d->routing_key, d->body);
    ch.ack(d->delivery_tag);
}
```

Output:

```text
[] d payload
```

## See also

- [consumer](consumer/README.md)
- [get](channel/get.md)
