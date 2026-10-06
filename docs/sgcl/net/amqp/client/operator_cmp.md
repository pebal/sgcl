[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [client](README.md)

# sgcl::net::amqp::operator== (sgcl::net::amqp::client)

```cpp
friend bool operator==(const client& a, const client& b) noexcept;
```

Whether two handles are the same connection; `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles |

## Return value

Whether they are the same connection.

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
    net::amqp::client same = c;
    net::amqp::client other = net::amqp::client::connect("amqp://guest:guest@localhost:5672/").value();
    println("{} {}", same == c, other == c);
}
```

Output:

```text
true false
```

## See also

- [client](README.md)
