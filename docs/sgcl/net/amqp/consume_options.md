[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::consume_options

```cpp
#include "sgcl/net/amqp/types.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    struct consume_options {
        string tag;
        bool no_ack = false;
        bool exclusive = false;
        table arguments;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::consume_options` is what [consume](channel/consume.md) takes.

## Member objects

| Member | Description |
|---|---|
| `tag` | the consumer's tag; empty: the broker names it |
| `no_ack` | acknowledged by the broker as it sends: no [ack](channel/ack.md), and a delivery lost with a failing client |
| `exclusive` | the queue's only consumer |
| `arguments` | RabbitMQ's "x-priority" and the like |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    ch.declare_queue("tagged").value();
    auto in = ch.consume("tagged", {.tag = "worker-1", .no_ack = true}).value();
    println("{}", in.tag());
}
```

Output:

```text
worker-1
```

## See also

- [consume](channel/consume.md)
