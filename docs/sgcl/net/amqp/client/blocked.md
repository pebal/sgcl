[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [client](README.md)

# sgcl::net::amqp::client::blocked

```cpp
bool blocked() const noexcept;
```

Whether the broker said it stops reading publications (RabbitMQ's connection.blocked, sent when it runs low on
memory or disk), until it says connection.unblocked. A publication meanwhile waits in the socket.

## Parameters

None.

## Return value

Whether it is blocked.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    println("{}", c.blocked());
}
```

Output:

```text
false
```

## See also

- [publish](../channel/publish.md)
- [client](README.md)
