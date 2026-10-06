[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::operator== (sgcl::net::amqp::field)

```cpp
friend bool operator==(const field& a, const field& b) noexcept;
```

Whether two values are of the same type and equal: the floats as floats (a NaN equal to nothing), an array or a
table item by item. `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the values |

## Return value

Whether they are the same.

## Complexity

Linear in the items of an array or a table; constant otherwise.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    println("{} {}", net::amqp::field(1) == net::amqp::field(1), net::amqp::field(1) == net::amqp::field::uint8(1));
}
```

Output:

```text
true false
```

## See also

- [field](README.md)
