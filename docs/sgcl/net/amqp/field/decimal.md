[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::decimal

```cpp
static field decimal(uint8_t scale, int32_t value) noexcept;
```

A decimal ('D'): the value over 10 to the scale, 19.99 as `decimal(2, 1999)`.

## Parameters

| Parameter | Description |
|---|---|
| `scale` | the digits after the point, 9 at most |
| `value` | the digits |


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
    auto price = net::amqp::field::decimal(2, 1999);
    println("{} {}", *price.as_double(), price.scale());
}
```

Output:

```text
19.990000000000002 2
```

## See also

- [as_double](as_double.md)
- [scale](scale.md)
- [field](README.md)
