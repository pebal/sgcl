[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [channel](README.md)

# sgcl::net::amqp::channel::channel

```cpp
channel() noexcept;                        // (1)
channel(const channel& other) noexcept;    // (2)
```

1. No channel: `operator bool` is false; an operation on it is a contract violation.
2. The same channel as `other`.

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
    net::amqp::channel none;
    net::amqp::channel same = ch;
    println("{} {}", bool(none), same == ch);
}
```

Output:

```text
false true
```

## See also

- [open_channel](../client/open_channel.md)
- [channel](README.md)
