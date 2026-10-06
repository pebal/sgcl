[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::bytes

```cpp
static field bytes(const string& v) noexcept;
```

Bytes ('x', RabbitMQ's byte array): binary data, not text.

## Parameters

| Parameter | Description |
|---|---|
| `v` | the bytes |


## Return value

The field.

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
    auto key = net::amqp::field::bytes(string("\x01\x02\x03"));
    println("{} {}", key.as_string()->size(), key.type() == net::amqp::field::kind::bytes);
}
```

Output:

```text
3 true
```

## See also

- [as_string](as_string.md)
- [field](README.md)
