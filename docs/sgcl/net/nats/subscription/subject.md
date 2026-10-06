[sgcl](../../../README.md) › [net](../../README.md) › [nats](../README.md) › [subscription](README.md)

# sgcl::net::nats::subscription::subject

```cpp
string subject() const;
```

The subject subscribed to, with its wildcards.

## Parameters

None.

## Return value

The subject.

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
    println("{}", sub.subject());
}
```

Output:

```text
events.>
```

## See also

- [queue_group](queue_group.md)
- [subscription](README.md)
