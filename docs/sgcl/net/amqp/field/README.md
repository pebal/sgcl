[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md)

# sgcl::net::amqp::field

```cpp
#include "sgcl/net/amqp/types.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    class field;
    using table = vector<pair<string, field>>;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::amqp::field` is a value of a field table (0-9-1 §4.2.5.5, the types RabbitMQ writes and reads): none, a
boolean, the integers of 8 to 64 bits, two floats, a decimal, a string, bytes, a timestamp, an array, a table. A
`table` of them is the arguments of a declaration and the headers of a message. A plain value: the constructors
make the common types, the named functions the exact type on the wire, and `as_*` read it back.

## Member types

| Type | Definition |
|---|---|
| [kind](../field-kind.md) | the type of the value |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](field.md) | a value of a common type |
| [type](type.md) | the type of the value |
| [as_bool](as_bool.md) | a boolean |
| [as_int](as_int.md) | an integer of any size |
| [as_double](as_double.md) | a float, a double, a decimal, an integer |
| [as_string](as_string.md) | a string or bytes |
| [as_timestamp](as_timestamp.md) | a timestamp |
| [as_array](as_array.md) | an array's values |
| [as_table](as_table.md) | a table's names and values |
| [scale](scale.md) | a decimal's scale |
| [int8, uint8, int16, uint16, uint32](integers.md) | an integer of an exact type on the wire |
| [float32](float32.md) | a float of 32 bits |
| [decimal](decimal.md) | a decimal |
| [bytes](bytes.md) | bytes, not text |
| [timestamp](timestamp.md) | a timestamp |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two values are the same type and value |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    net::amqp::table args{{"x-message-ttl", net::amqp::field(60000)},
                          {"x-queue-type", net::amqp::field("quorum")},
                          {"x-single-active-consumer", net::amqp::field(true)}};
    for (auto& [name, value] : args) {
        println("{}", name);
    }
}
```

Output:

```text
x-message-ttl
x-queue-type
x-single-active-consumer
```

## See also

- [properties](../properties.md)
- [find](../find.md)
