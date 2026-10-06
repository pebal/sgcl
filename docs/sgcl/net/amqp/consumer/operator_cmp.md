[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [consumer](README.md)

# sgcl::net::amqp::operator== (sgcl::net::amqp::consumer)

```cpp
friend bool operator==(const consumer& a, const consumer& b) noexcept;
```

Whether two handles are the same consumer; `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles |

## Return value

Whether they are the same consumer.

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
    net::amqp::consumer same = in;
    net::amqp::consumer other = ch.consume("q").value();
    println("{} {}", same == in, other == in);
}
```

Output:

```text
true false
```

## See also

- [consumer](README.md)
