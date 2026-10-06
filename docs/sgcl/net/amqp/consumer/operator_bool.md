[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [consumer](README.md)

# sgcl::net::amqp::consumer::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a consumer; an ended consumer is still held.

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
    ch.declare_queue("q").value();
    net::amqp::consumer in = ch.consume("q").value();
    net::amqp::consumer none;
    println("{} {}", bool(none), bool(in));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](consumer.md)
- [consumer](README.md)
