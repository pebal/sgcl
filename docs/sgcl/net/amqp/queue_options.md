[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::queue_options

```cpp
#include "sgcl/net/amqp/types.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    struct queue_options {
        bool passive = false;
        bool durable = false;
        bool exclusive = false;
        bool auto_delete = false;
        table arguments;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::queue_options` is what [declare_queue](channel/declare_queue.md) takes.

## Member objects

| Member | Description |
|---|---|
| `passive` | only checked that it exists |
| `durable` | kept over a restart of the broker |
| `exclusive` | this connection's alone, deleted at its end |
| `auto_delete` | deleted when its last consumer goes |
| `arguments` | "x-message-ttl", "x-max-length", "x-queue-type", "x-dead-letter-exchange"... |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    net::amqp::queue_options o;
    o.durable = true;
    o.arguments = {{"x-max-length", net::amqp::field(1000)}};
    println("{}", ch.declare_queue("bounded", o)->name);
}
```

Output:

```text
bounded
```

## See also

- [declare_queue](channel/declare_queue.md)
