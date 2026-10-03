[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::abs

```cpp
big_integer abs() const noexcept;
```

The absolute value. A value past `int64_t` shares its object of limbs with the result, as a copy does, rather than
copying it; `INT64_MIN`, whose absolute value is past `int64_t`, gives 2^63.

## Parameters

None.

## Return value

The number with its sign dropped, never negative.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"
#include <cstdint>

using namespace sgcl;

int main() {
    math::big_integer debt = -7;
    math::big_integer bottom = INT64_MIN;
    auto deep = -(math::big_integer(1) << 100);
    println("{} {}", debt.abs(), bottom.abs());
    println("{}", deep.abs());
}
```

Output:

```text
7 9223372036854775808
1267650600228229401496703205376
```

## See also

- [sign](sign.md): the sign alone
- [operator-](operator_arith.md): the negation
- [sgcl::math::big_integer](README.md)
