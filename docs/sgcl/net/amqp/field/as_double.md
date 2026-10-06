[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::as_double

```cpp
optional<double> as_double() const noexcept;
```

The value of a float, a double, a decimal (its value over 10 to its scale) or an integer.

## Parameters

None.

## Return value

The number; none for a value of another type.

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
    println("{} {}", *net::amqp::field::decimal(2, 1999).as_double(), *net::amqp::field(3).as_double());
}
```

Output:

```text
19.990000000000002 3
```

## See also

- [decimal](decimal.md)
- [field](README.md)
