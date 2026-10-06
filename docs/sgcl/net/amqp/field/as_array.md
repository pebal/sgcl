[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::as_array

```cpp
optional<vector<field>> as_array() const;
```

The values of an array.

## Parameters

None.

## Return value

The values; none for a value of another type.

## Complexity

Linear in the values.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::field list(vector<net::amqp::field>{net::amqp::field(1), net::amqp::field(2)});
    println("{}", list.as_array()->size());
}
```

Output:

```text
2
```

## See also

- [as_table](as_table.md)
- [field](README.md)
