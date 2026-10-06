[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::publish_options

```cpp
#include "sgcl/net/amqp/types.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    struct publish_options {
        bool mandatory = false;
    };
}
```

`sgcl::net::amqp::publish_options` is what [publish](channel/publish.md) takes beside the message.

## Member objects

| Member | Description |
|---|---|
| `mandatory` | given back ([receive_returned](channel/receive_returned.md)) when no queue takes it; false: dropped |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    ch.publish("", "no-such-queue", "x", {}, {.mandatory = true}).value();
    println("{}", ch.receive_returned()->reply_code);
}
```

Output:

```text
312
```

## See also

- [publish](channel/publish.md)
