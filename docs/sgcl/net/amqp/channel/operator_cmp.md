[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::operator== (sgcl::net::amqp::channel)

```cpp
friend bool operator==(const channel& a, const channel& b) noexcept;
```

Whether two handles are the same channel; `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles |

## Return value

Whether they are the same channel.

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
    net::amqp::channel same = ch;
    net::amqp::channel other = c.open_channel().value();
    println("{} {}", same == ch, other == ch);
}
```

Output:

```text
true false
```

## See also

- [channel](README.md)
