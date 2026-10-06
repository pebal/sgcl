[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [consumer](README.md)

# sgcl::net::amqp::consumer::try_receive

```cpp
optional<delivery> try_receive() const;
```

The next delivery when one is there, at once.

## Parameters

None.

## Return value

The [delivery](../delivery.md), or none.

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
    println("{}", bool(in.try_receive()));
}
```

Output:

```text
false
```

## See also

- [receive](receive.md)
- [consumer](README.md)
