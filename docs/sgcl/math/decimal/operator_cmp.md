[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::operator==, operator\<=\> (sgcl::math::decimal)

```cpp
friend bool operator==(const decimal& a, const decimal& b) noexcept;                   // (1)
friend std::weak_ordering operator<=>(const decimal& a, const decimal& b) noexcept;    // (2)
```

Compare two values, whatever their scales: `1.0 == 1.00`, `0.1 < 0.25`, `1e1000 > 999`. The order is total over NaN
and the infinities, as in PostgreSQL: −Infinity is below every finite value, +Infinity above, NaN above
+Infinity, and NaN equals NaN — so a decimal is a key of a sorted map whatever it holds, which a double's NaN cannot
be. The ordering is weak because equal values need not be the same: `1.0` and `1.00` are equivalent and write
differently; [identical](identical.md) asks for both. A whole number or a `big_integer` on either side is converted,
`d == 0` and `d < 10` need no second set; `!=`, `<`, `<=`, `>` and `>=` follow from these two.

1. Whether the values are equal.
2. The order of the values.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the decimals, or whole numbers or `big_integer`s converted to decimals |

## Return value

1. `true` when the values are equal.
2. `less`, `equivalent` or `greater`.

## Complexity

At one scale, the comparison of the unscaled parts. At two, the signs first, then the lengths of the values, and only
when those are close the unscaled parts brought to one scale: a value at scale 2·10⁹ is told from 1 without being
multiplied out.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::decimal a("1.0");
    math::decimal b("1.00");
    println("{} {} {}", a == b, a.identical(b), math::decimal("0.1") < math::decimal("0.25"));
    println("{} {}", math::decimal("1e1000") > 999, a == 1);
    math::decimal nan = math::decimal::nan();
    println("{} {} {}", nan == nan, nan > math::decimal::infinity(),
            -math::decimal::infinity() < math::decimal("-1e1000"));
}
```

Output:

```text
true false true
true true
true true true
```

## See also

- [identical](identical.md): the same scale as well
- [operator+, operator-, operator\*, operator%](operator_arith.md): the arithmetic
- [sgcl::math::decimal](README.md)
