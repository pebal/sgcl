[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md)

# sgcl::net::amqp::queue_info

```cpp
#include "sgcl/net/amqp/types.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    struct queue_info {
        string name;
        uint32_t messages = 0;
        uint32_t consumers = 0;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::queue_info` is what [declare_queue](channel/declare_queue.md) gives: the queue's name (the broker's for one declared without), its ready messages and its consumers.

## Member objects

| Member | Description |
|---|---|
| `name` | the queue's name |
| `messages` | its messages ready to be delivered |
| `consumers` | its consumers |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    auto q = ch.declare_queue("counted").value();
    ch.publish("", "counted", "1").value();
    println("{} {}", q.messages, ch.declare_queue("counted", {.passive = true})->messages);
}
```

Output:

```text
0 1
```

## See also

- [declare_queue](channel/declare_queue.md)
