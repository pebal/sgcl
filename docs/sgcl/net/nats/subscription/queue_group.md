[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [subscription](README.md)

# sgcl::net::nats::subscription::queue_group

```cpp
string queue_group() const;
```

The queue group the subscription belongs to; empty for none.

## Parameters

None.

## Return value

The group.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/nats.h"

using namespace sgcl;

int main() {
    net::nats::client nc = net::nats::client::connect("nats://localhost:4222").value();
    net::nats::subscription sub = nc.subscribe("events.>").value();
    nc.flush().value();
    auto worker = nc.subscribe("tasks", "pool").value();
    println("[{}] {}", sub.queue_group(), worker.queue_group());
}
```

Output:

```text
[] pool
```

## See also

- [subscribe](../client/subscribe.md)
- [subscription](README.md)
