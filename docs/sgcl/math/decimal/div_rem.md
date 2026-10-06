[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::div_rem

```cpp
pair<decimal, decimal> div_rem(const decimal& by) const;
```

The quotient cut towards zero, a whole number at scale 0, and the remainder, `*this % by`, with the sign of this
value at the larger of the two scales: `7.5.div_rem(2)` is `{3, 1.5}`, `-7.5.div_rem(2)` is `{-3, -1.5}`. The
quotient times `by` plus the remainder is this value. One division for both, as
[big_integer::div_rem](../big_integer/div_rem.md); Python's `divmod` of two `Decimal`s, Java's
`divideAndRemainder`.

NaN with anything gives two NaNs, and so does an infinity divided; a finite value divided by an infinity gives zero
and the value itself.

## Parameters

| Parameter | Description |
|---|---|
| `by` | the divisor |

## Return value

The pair `{quotient, remainder}`.

## Complexity

One division of the unscaled parts brought to one scale.

## Exceptions

- `domain_error` when `by` is zero.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    auto [q, r] = math::decimal("7.5").div_rem(2);
    println("{} {}", q, r);
    auto [nq, nr] = math::decimal("-7.5").div_rem(math::decimal("0.7"));
    println("{} {} {}", nq, nr, nq * math::decimal("0.7") + nr);
}
```

Output:

```text
3 1.5
-10 -0.5 -7.5
```

## See also

- [operator%](operator_arith.md): the remainder alone
- [div](div.md): the quotient rounded to places
- [sgcl::math::decimal](README.md)
