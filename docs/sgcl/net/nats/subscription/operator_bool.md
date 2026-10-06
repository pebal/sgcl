[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [subscription](README.md)

# sgcl::net::nats::subscription::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a subscription; an ended subscription is still held.

## Parameters

None.

## Return value

Whether it does.

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
    net::nats::subscription none;
    println("{} {}", bool(none), bool(sub));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](subscription.md)
- [subscription](README.md)
