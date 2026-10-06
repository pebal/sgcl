[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::exchange_options

```cpp
#include "sgcl/net/amqp/types.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    struct exchange_options {
        string type = string("direct");
        bool passive = false;
        bool durable = false;
        bool auto_delete = false;
        bool internal = false;
        table arguments;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::exchange_options` is what [declare_exchange](channel/declare_exchange.md) takes.

## Member objects

| Member | Description |
|---|---|
| `type` | "direct", "fanout", "topic", "headers", or a plugin's; "direct" by default |
| `passive` | only checked that it exists |
| `durable` | kept over a restart of the broker |
| `auto_delete` | deleted when its last binding goes |
| `internal` | no publications of clients: only other exchanges route to it |
| `arguments` | "alternate-exchange" and the like |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    ch.declare_exchange("audit", {.type = "fanout", .durable = true}).value();
    println("{}", bool(ch.declare_exchange("audit", {.type = "fanout", .passive = true})));
}
```

Output:

```text
true
```

## See also

- [declare_exchange](channel/declare_exchange.md)
