[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::try_receive_returned

```cpp
optional<returned> try_receive_returned() const;
```

The next publication the broker gave back when there is one, at once.

## Parameters

None.

## Return value

The [returned](../returned.md) message, or none.

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
    println("{}", bool(ch.try_receive_returned()));
}
```

Output:

```text
false
```

## See also

- [receive_returned](receive_returned.md)
- [channel](README.md)
