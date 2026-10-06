[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [subscription](README.md)

# sgcl::net::nats::subscription::subscription

```cpp
subscription() noexcept;                             // (1)
subscription(const subscription& other) noexcept;    // (2)
```

1. No subscription: `operator bool` is false; an operation on it is a contract violation.
2. The same subscription as `other`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle copied |


## Return value

None.

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
    net::nats::subscription same = sub;
    println("{} {}", bool(none), same == sub);
}
```

Output:

```text
false true
```

## See also

- [subscribe](../client/subscribe.md)
- [subscription](README.md)
