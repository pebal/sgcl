[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::to_decimal

```cpp
decimal to_decimal() const;
```

The value as a [decimal](../decimal/README.md), exactly — every binary fraction ends in decimal: a mantissa over 2^k is a mantissa times 5^k over 10^k, so a value of p bits and an exponent of −k has about 0.3p + 0.7k digits. The infinities are decimal's; −0 is 0.

## Parameters

None.

## Return value

The decimal.

## Complexity

A power of five of the exponent and a product.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", math::big_float(0.375).to_decimal(),
            math::big_float(0.1).to_decimal().precision());
    println(math::big_float::infinity().to_decimal());
}
```

Output:

```text
0.375 55
Infinity
```

## See also

- [to_string](to_string.md): the shortest decimal that reads back
- [sgcl::math::big_float](README.md)
