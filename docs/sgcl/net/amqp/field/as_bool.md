[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::as_bool

```cpp
optional<bool> as_bool() const noexcept;
```

The value of a boolean.

## Parameters

None.

## Return value

The boolean; none for a value of another type.

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
    println("{} {}", *net::amqp::field(true).as_bool(), bool(net::amqp::field(1).as_bool()));
}
```

Output:

```text
true false
```

## See also

- [type](type.md)
- [field](README.md)
