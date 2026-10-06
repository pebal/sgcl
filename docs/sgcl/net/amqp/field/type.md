[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::type

```cpp
kind type() const noexcept;
```

The type of the value, as the wire has it.

## Parameters

None.

## Return value

The [kind](../field-kind.md).

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
    println("{}", net::amqp::field(1).type() == net::amqp::field::kind::int32);
    println("{}", net::amqp::field::uint8(1).type() == net::amqp::field::kind::uint8);
}
```

Output:

```text
true
true
```

## See also

- [kind](../field-kind.md)
- [field](README.md)
