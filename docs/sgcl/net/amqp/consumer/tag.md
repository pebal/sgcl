[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [consumer](README.md)

# sgcl::net::amqp::consumer::tag

```cpp
string tag() const;
```

The consumer's tag: the one asked for, or the broker's ("amq.ctag-...").

## Parameters

None.

## Return value

The tag.

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
    println("{}", in.tag().view().starts_with("amq.ctag-"));
}
```

Output:

```text
true
```

## See also

- [consume_options](../consume_options.md)
- [consumer](README.md)
