[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::as_table

```cpp
optional<table> as_table() const;
```

The names and values of a table.

## Parameters

None.

## Return value

The table; none for a value of another type.

## Complexity

Linear in the entries.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::field nested(net::amqp::table{{"depth", net::amqp::field(2)}});
    println("{}", *net::amqp::find(*nested.as_table(), "depth")->as_int());
}
```

Output:

```text
2
```

## See also

- [as_array](as_array.md)
- [find](../find.md)
- [field](README.md)
