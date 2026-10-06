[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::precision

```cpp
size_t precision() const noexcept;
```

The decimal digits of the unscaled part, its significant digits as written: `1.50` has 3, `0.003` has 1, `0` has 1,
`decimal(0.1)` has 55. What PostgreSQL calls the precision of a `NUMERIC` value; Java's `precision()`.

## Parameters

None.

## Return value

The number of digits, at least 1.

## Complexity

Constant for an unscaled part within `int64_t`; for a longer one, a power of ten of about its length and a
comparison.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {} {}", math::decimal("1.50").precision(), math::decimal("0.003").precision(),
            math::decimal().precision(), math::decimal(0.1).precision());
}
```

Output:

```text
3 1 1 55
```

## See also

- [round_precision](round_precision.md): a value to fewer digits
- [scale](scale.md): the places
- [sgcl::math::decimal](README.md)
