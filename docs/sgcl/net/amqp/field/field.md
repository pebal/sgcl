[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::field

```cpp
field() noexcept;                     // (1)
field(bool v) noexcept;               // (2)
field(int32_t v) noexcept;            // (3)
field(int64_t v) noexcept;            // (4)
field(double v) noexcept;             // (5)
field(const char* v);                 // (6)
field(const string& v) noexcept;      // (7)
field(const vector<field>& items);    // (8)
field(const table& t);                // (9)
```

1. None ('V').
2. A boolean ('t').
3. An integer of 32 bits ('I'): an `int` literal.
4. An integer of 64 bits ('l').
5. A double ('d').
6. – 7. A string ('S').
8. An array ('A') of the values.
9. A table ('F') of the names and values.

## Parameters

| Parameter | Description |
|---|---|
| `v` | the value |
| `items` | the array's values |
| `t` | the table's names and values |


## Return value

None.

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
    net::amqp::field ttl(60000), name("orders"), on(true);
    net::amqp::field list(vector<net::amqp::field>{net::amqp::field(1), net::amqp::field("two")});
    println("{} {} {} {}", *ttl.as_int(), *name.as_string(), *on.as_bool(), list.as_array()->size());
}
```

Output:

```text
60000 orders true 2
```

## See also

- [integers](integers.md)
- [type](type.md)
- [field](README.md)
