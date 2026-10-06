[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [subscription](README.md)

# sgcl::net::nats::subscription::dropped

```cpp
uint64_t dropped() const noexcept;
```

The messages dropped because the subscription's queue was full: a consumer slower than its publishers.

## Parameters

None.

## Return value

The count.

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
    println("{}", sub.dropped());
}
```

Output:

```text
0
```

## See also

- [options](../client-options.md)
- [subscription](README.md)
