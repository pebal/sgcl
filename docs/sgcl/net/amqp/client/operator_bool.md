[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [client](README.md)

# sgcl::net::amqp::client::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a connection; an ended connection is still held.

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
    net::amqp::client none;
    println("{} {}", bool(none), bool(c));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](client.md)
- [client](README.md)
