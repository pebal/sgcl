[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::decimal

```cpp
static cbor decimal(const math::big_integer& mantissa, int64_t exponent) noexcept;
```

Tag 4, a decimal fraction (§3.4.4): the array of the exponent and the mantissa, the number mantissa · 10^exponent,
an amount of money held exactly.

## Parameters

| Parameter | Description |
|---|---|
| `mantissa` | the digits as an integer |
| `exponent` | the power of ten |

## Return value

The value.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    encoding::cbor price = encoding::cbor::decimal(math::big_integer(27315), -2);   // 273.15
    println("{} {}", price.to_string(), encoding::hex::encode(price.to_bytes()));
}
```

Output:

```text
4([-2, 27315]) c48221196ab3
```

## See also

- [as_decimal](as_decimal.md)
- [sgcl::encoding::cbor](README.md)
