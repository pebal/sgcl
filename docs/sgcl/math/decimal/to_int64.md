[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::to_int64

```cpp
optional<int64_t> to_int64() const noexcept;
```

The value as an `int64_t` when it is a whole number `int64_t` holds: `2.00` is 2, `1.5e3` is 1500; `2.50`, a value
past `int64_t`, NaN and the infinities are nothing. [to_big_integer](to_big_integer.md) rounds a value that is not
whole.

## Parameters

None.

## Return value

The value, or `nullopt`.

## Complexity

Constant for an unscaled part within `int64_t`; for a longer one a division by a power of ten of the scale.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", *math::decimal("2.00").to_int64(), *math::decimal("1.5e3").to_int64());
    println("{} {}", math::decimal("2.50").to_int64().has_value(),
            math::decimal("1e19").to_int64().has_value());
}
```

Output:

```text
2 1500
false false
```

## See also

- [to_big_integer](to_big_integer.md): the whole number of any value, rounded
- [sgcl::math::decimal](README.md)
