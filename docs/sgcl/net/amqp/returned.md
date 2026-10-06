[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::returned

```cpp
#include "sgcl/net/amqp/types.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    struct returned {
        string body;
        amqp::properties properties;
        int reply_code = 0;
        string reply_text;
        string exchange;
        string routing_key;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::returned` is a publication the broker gave back (basic.return): a mandatory one no queue took.

## Member objects

| Member | Description |
|---|---|
| `body` | the message's bytes |
| `properties` | its [properties](properties.md) |
| `reply_code` | why: 312 no route |
| `reply_text` | the broker's text: "NO_ROUTE" |
| `exchange` | the exchange it was published to |
| `routing_key` | its routing key |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    ch.publish("", "missing-queue", "x", {}, {.mandatory = true}).value();
    println("{}", ch.receive_returned()->routing_key);
}
```

Output:

```text
missing-queue
```

## See also

- [receive_returned](channel/receive_returned.md)
