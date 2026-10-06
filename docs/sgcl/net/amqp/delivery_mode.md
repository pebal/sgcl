[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::delivery_mode

```cpp
#include "sgcl/net/amqp/types.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    enum class delivery_mode : uint8_t { none = 0, transient = 1, persistent = 2 };
}
```

How a broker keeps a message ([properties](properties.md)): in memory, or on disk too when its queue is durable.

| Value | Description |
|---|---|
| `none` | not said: the broker's default, transient |
| `transient` | in memory |
| `persistent` | on disk too, kept over a restart in a durable queue |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    ch.declare_queue("kept", {.durable = true}).value();
    net::amqp::properties p;
    p.delivery_mode = net::amqp::delivery_mode::persistent;
    ch.publish("", "kept", "x", p).value();
    println("{}", ch.get("kept", true).value()->properties.delivery_mode == net::amqp::delivery_mode::persistent);
}
```

Output:

```text
true
```

## See also

- [properties](properties.md)
