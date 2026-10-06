[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [client](README.md)

# sgcl::net::amqp::client::client

```cpp
client() noexcept;                       // (1)
client(const client& other) noexcept;    // (2)
```

1. No connection: `operator bool` is false; an operation on it is a contract violation.
2. The same connection as `other`.

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
    net::amqp::client none;
    net::amqp::client same = c;
    println("{} {}", bool(none), same == c);
}
```

Output:

```text
false true
```

## See also

- [connect](connect.md)
- [client](README.md)
