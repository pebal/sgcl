[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::id

```cpp
uint16_t id() const noexcept;
```

The channel's number on its connection, 1 for the first.

## Parameters

None.

## Return value

The number.

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
    net::amqp::channel ch = c.open_channel().value();
    println("{}", ch.id());
}
```

Output:

```text
1
```

## See also

- [open_channel](../client/open_channel.md)
- [channel](README.md)
