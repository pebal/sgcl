[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::scale

```cpp
uint8_t scale() const noexcept;
```

A decimal's scale: its value is the integer over 10 to it.

## Parameters

None.

## Return value

The scale; 0 for a value of another type.

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
    println("{}", net::amqp::field::decimal(3, 1500).scale());
}
```

Output:

```text
3
```

## See also

- [decimal](decimal.md)
- [field](README.md)
