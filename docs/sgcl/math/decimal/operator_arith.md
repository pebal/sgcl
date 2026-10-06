[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::operator+=, operator-=, operator\*=, operator%=, operator-, sgcl::math::operator+, operator-, operator\*, operator% (sgcl::math::decimal)

```cpp
friend decimal operator+(const decimal& a, const decimal& b) noexcept;    // (1)
friend decimal operator-(const decimal& a, const decimal& b) noexcept;    // (2)
friend decimal operator*(const decimal& a, const decimal& b);             // (3)
friend decimal operator%(const decimal& a, const decimal& b);             // (4)
decimal operator-() const noexcept;                                       // (5)
decimal& operator+=(const decimal& b) noexcept;                           // (6)
decimal& operator-=(const decimal& b) noexcept;                           // (7)
decimal& operator*=(const decimal& b);                                    // (8)
decimal& operator%=(const decimal& b);                                    // (9)
```

The exact arithmetic of decimals: nothing is rounded. The binary operators are hidden friends taking two decimals,
and a whole number or a `big_integer` on either side is converted, so `price * 3` and `1 - rate` need no second
set. A `double` is not converted by itself: `d + 0.5` does not compile. There is no `/`: a quotient takes a scale or
a number of digits ([div](div.md), [div_precision](div_precision.md)).

1. The sum, at the larger of the two scales: `1.5 + 0.25` is `1.75`, `1.50 + 1` is `2.50`.
2. The difference, as (1).
3. The product, at the sum of the scales: `1.5 * 0.25` is `0.375`, `1.50 * 2.0` is `3.000`. A sum of scales past
   `int32_t` is `length_error`.
4. The remainder of the quotient cut towards zero, with the sign of `a`, at the larger scale: `7.5 % 2` is `1.5`,
   `-7.5 % 2` is `-1.5` (C++'s `%`, Python's for its `Decimal`). A divisor of zero is `domain_error`.
5. The negation.
6. `*this = *this + b`.
7. `*this = *this - b`.
8. `*this = *this * b`.
9. `*this = *this % b`.

NaN and the infinities go as in PostgreSQL: NaN with anything is NaN; an infinity plus a finite value or itself is
itself, plus the other infinity NaN; an infinity times a value of a sign is an infinity of the product's sign, times
zero NaN; an infinity modulo anything is NaN, a finite value modulo an infinity is itself.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the decimals, or whole numbers or `big_integer`s converted to decimals |

## Return value

- (1–5) The decimal computed.
- (6–9) `*this`.

## Complexity

- (1–2) Linear in the longer unscaled part; at two scales the one at the smaller is multiplied by a power of ten
  first. Up to eighteen digits, on the processor's own numbers.
- (3) The `big_integer` product of the unscaled parts.
- (4) A division of the unscaled parts brought to one scale.
- (5) Constant: the unscaled part's object is shared.
- (6–9) As (1–4).

## Exceptions

- (1–2), (5–7) None.
- (3), (8) `length_error` when the sum of the scales is past `int32_t`.
- (4), (9) `domain_error` when `b` is zero.

`*this` is left as it was by an exception of (8) or (9).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::decimal a("1.5");
    math::decimal b("0.25");
    println("{} {} {} {}", a + b, a - b, a * b, -a);
    println("{} {} {}", math::decimal("1.50") + 1, math::decimal("1.50") * math::decimal("2.0"),
            a * 3 - 1);
    println("{} {}", math::decimal("7.5") % 2, math::decimal("-7.5") % 2);

    math::decimal sum;
    for (const char* price : {"19.99", "5.01", "0.10"}) {
        sum += math::decimal(price);
    }
    println(sum);
    println("{} {}", math::decimal::infinity() + 1, (math::decimal::infinity() * 0).is_nan());
}
```

Output:

```text
1.75 1.25 0.375 -1.5
2.50 3.000 3.5
1.5 -1.5
25.10
Infinity true
```

## See also

- [div](div.md), [div_precision](div_precision.md): the quotients
- [div_rem](div_rem.md): the whole quotient with the remainder
- [operator==, operator\<=\>](operator_cmp.md): the comparisons
- [sgcl::math::decimal](README.md)
