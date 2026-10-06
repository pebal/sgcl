[sgcl](../../../README.md) › [net](../../README.md) › [amqp](../README.md) › [field](README.md)

# sgcl::net::amqp::field::float32

```cpp
static field float32(float v) noexcept;
```

A float of 32 bits ('f'); the constructor of a double makes 'd'.

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
    println("{}", *net::amqp::field::float32(0.5f).as_double());
}
```

Output:

```text
0.5
```

## See also

- [as_double](as_double.md)
- [field](README.md)
