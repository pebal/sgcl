[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::div_rem

```cpp
pair<big_integer, big_integer> div_rem(const big_integer& d) const;
```

The quotient and the remainder of one division, `{a / d, a % d}`: the quotient cut towards zero and the remainder
with the sign of the dividend, as [operator/ and operator%](operator_arith.md) give them, for the cost of one
division where the two operators divide twice. Go's `QuoRem`.

## Parameters

| Parameter | Description |
|---|---|
| `d` | the divisor, not zero |

## Return value

A pair of the quotient and the remainder.

## Complexity

That of a division ([operator/](operator_arith.md)).

## Exceptions

`domain_error` when `d` is zero.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    auto [q, r] = math::big_integer(-7).div_rem(2);
    println("{} {}", q, r);

    auto n = math::big_integer(1) << 128;
    auto [quotient, rest] = n.div_rem(1'000'000'007);
    println("{} {}", quotient, rest);
    println("{}", quotient * 1'000'000'007 + rest == n);
}
```

Output:

```text
-3 -1
340282364538961911690641225597 279632277
true
```

## See also

- [mod](mod.md): the remainder that is never negative
- [operator/, operator%](operator_arith.md): the quotient or the remainder alone
- [sgcl::math::big_integer](../big_integer.md)
