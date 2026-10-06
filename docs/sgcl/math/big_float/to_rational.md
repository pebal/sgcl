[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::to_rational

```cpp
rational to_rational() const;
```

The value as a [rational](../rational/README.md), exactly: a mantissa over a power of two. ±0 is 0.

## Parameters

None.

## Return value

The fraction.

## Complexity

A shift by the exponent.

## Exceptions

- `domain_error` for an infinity.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", math::big_float(0.75).to_rational(), math::big_float(0.1).to_rational());
}
```

Output:

```text
3/4 3602879701896397/36028797018963968
```

## See also

- [(constructor)](big_float.md): from a fraction, rounded
- [sgcl::math::big_float](README.md)
