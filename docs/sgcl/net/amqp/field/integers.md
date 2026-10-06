[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::int8, uint8, int16, uint16, uint32

```cpp
static field int8(int8_t v) noexcept;        // (1)
static field uint8(uint8_t v) noexcept;      // (2)
static field int16(int16_t v) noexcept;      // (3)
static field uint16(uint16_t v) noexcept;    // (4)
static field uint32(uint32_t v) noexcept;    // (5)
```

An integer of an exact type on the wire, for an argument a broker reads as that type: 'b', 'B', 's' (RabbitMQ's
letter for 16 bits), 'u', 'i'. The constructors make 'I' (32 bits) and 'l' (64).

## Parameters

| Parameter | Description |
|---|---|
| `v` | the value |


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
    auto priority = net::amqp::field::uint8(10);
    println("{} {}", *priority.as_int(), priority.type() == net::amqp::field::kind::uint8);
}
```

Output:

```text
10 true
```

## See also

- [field](field.md)
- [field](README.md)
