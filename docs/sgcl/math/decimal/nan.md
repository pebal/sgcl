[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::nan

```cpp
static decimal nan() noexcept;
```

NaN, the value PostgreSQL's `NUMERIC` writes `'NaN'`: not a number, and the result of every operation it takes part
in. Unlike a double's NaN it equals itself and orders above every other value, as in PostgreSQL, so that `==`, `<=>`
and the hash are one total order and a NaN can be a key of a map. Arithmetic on numbers never makes one: `inf − inf`
and `0 × inf` do, a division by zero is `domain_error`.

## Parameters

None.

## Return value

NaN; its [unscaled](unscaled.md) part is zero and its [scale](scale.md) 0, its [sign](sign.md) 0.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::decimal nan = math::decimal::nan();
    println("{} {} {}", nan, (nan + 1).is_nan(), nan == math::decimal::nan());
    println("{} {}", nan > math::decimal::infinity(), (math::decimal::infinity() * 0).is_nan());
}
```

Output:

```text
NaN true true
true true
```

## See also

- [infinity](infinity.md): +Infinity
- [is_nan](is_nan.md): asks for it
- [operator==, operator\<=\>](operator_cmp.md): the total order
- [sgcl::math::decimal](README.md)
