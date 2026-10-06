[sgcl](../../README.md) › [net](../README.md) › [amqp](README.md) › [field](field/README.md)

# sgcl::net::amqp::field::kind

```cpp
#include "sgcl/net/amqp/types.h"   // or "sgcl/net/amqp.h"

namespace sgcl::net::amqp {
    class field {
    public:
        enum class kind : uint8_t { none, boolean, int8, uint8, int16, uint16, int32, uint32, int64, float32, float64,
                                    decimal, string, bytes, timestamp, array, table };
    };
}
```

The type of a [field](field/README.md)'s value, as the wire has it.

| Value | Description |
|---|---|
| `none` | 'V', no value |
| `boolean` | 't' |
| `int8`, `uint8` | 'b', 'B' |
| `int16`, `uint16` | 's', 'u' |
| `int32`, `uint32` | 'I', 'i' |
| `int64` | 'l' |
| `float32`, `float64` | 'f', 'd' |
| `decimal` | 'D' |
| `string` | 'S' |
| `bytes` | 'x' |
| `timestamp` | 'T' |
| `array` | 'A' |
| `table` | 'F' |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/amqp.h"

using namespace sgcl;

int main() {
    println("{}", net::amqp::field("x").type() == net::amqp::field::kind::string);
}
```

Output:

```text
true
```

## See also

- [type](field/type.md)
