[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [subscription](README.md)

# sgcl::net::nats::operator== (sgcl::net::nats::subscription)

```cpp
friend bool operator==(const subscription& a, const subscription& b) noexcept;
```

Whether two handles are the same subscription; `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles |

## Return value

Whether they are the same subscription.

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
    net::nats::subscription same = sub;
    net::nats::subscription other = nc.subscribe("events.>").value();
    println("{} {}", same == sub, other == sub);
}
```

Output:

```text
true false
```

## See also

- [subscription](README.md)
