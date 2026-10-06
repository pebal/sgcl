[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a channel; an ended channel is still held.

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
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::client c = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    net::amqp::channel ch = c.open_channel().value();
    net::amqp::channel none;
    println("{} {}", bool(none), bool(ch));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](channel.md)
- [channel](README.md)
