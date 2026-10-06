[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::as_int

```cpp
optional<int64_t> as_int() const noexcept;
```

The value of an integer of any of the sizes, signed or not.

## Parameters

None.

## Return value

The integer; none for a value of another type.

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
    println("{} {}", *net::amqp::field::uint16(65535).as_int(), bool(net::amqp::field("1").as_int()));
}
```

Output:

```text
65535 false
```

## See also

- [as_double](as_double.md)
- [field](README.md)
