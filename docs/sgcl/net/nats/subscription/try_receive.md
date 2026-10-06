[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [subscription](README.md)

# sgcl::net::nats::subscription::try_receive

```cpp
optional<message> try_receive() const;
```

The next message when one is there, at once.

## Parameters

None.

## Return value

The message, or none.

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
    println("{}", bool(sub.try_receive()));
}
```

Output:

```text
false
```

## See also

- [receive](receive.md)
- [subscription](README.md)
