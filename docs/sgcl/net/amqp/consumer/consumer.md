[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [consumer](README.md)

# sgcl::net::amqp::consumer::consumer

```cpp
consumer() noexcept;                         // (1)
consumer(const consumer& other) noexcept;    // (2)
```

1. No consumer: `operator bool` is false; an operation on it is a contract violation.
2. The same consumer as `other`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle copied |


## Return value

None.

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
    net::amqp::consumer same = in;
    println("{} {}", bool(none), same == in);
}
```

Output:

```text
false true
```

## See also

- [consume](../channel/consume.md)
- [consumer](README.md)
