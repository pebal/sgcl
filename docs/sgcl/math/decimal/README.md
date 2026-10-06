[sgcl](../../README.md) › [math](../README.md)

# sgcl::math::decimal

```cpp
#include "sgcl/math/decimal.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class decimal;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::math::decimal` is a decimal number of any precision: a whole number of any size, the unscaled part, and a
scale, the value being *unscaled* × 10^−*scale*. It is what `java.math.BigDecimal` is and what PostgreSQL's
`NUMERIC` holds: money, measures and database columns exactly as they were written, so `0.1 + 0.2` is `0.3` and
`1.50` keeps its two places. Neither the standard library nor Go has one (Go's `big.Float` is binary and
`big.Rat` keeps no places); Python's `decimal` is the nearest, and its answers are this type's tests.

The arithmetic `+`, `-` and `*` is exact, as for [big_integer](../big_integer/README.md) and
[rational](../rational/README.md). What has to round — a quotient, a square root, a value brought to fewer places
or fewer digits — takes the scale or the number of digits wanted and the [rounding](../../core/rounding.md), half-even when
none is given: [div](div.md), [div_precision](div_precision.md), [sqrt](sqrt.md), [rescale](rescale.md),
[round_precision](round_precision.md). There is no `/`, because `1/3` has no exact answer and a hidden precision
would be one nobody reads in the code.

## Rules

- **A value, with a scale.** A `big_integer` and an `int32_t`: the scale is positive for digits after the point and
  negative for zeros before it, `"1.5e3"` being 15 at scale −2. A value of up to eighteen digits lives in the
  `big_integer`'s own word and allocates nothing; its sums, products, roundings and comparisons are done on the
  processor's own numbers.
- **Exact sums and products.** The scale of a sum is the larger of the two, of a product their sum, as in Java and
  Python; nothing is rounded.
- **Equal values are equal.** `1.0 == 1.00`, and the two hash alike; [identical](identical.md) asks for the same
  scale too. [trim_scale](trim_scale.md) brings a value to its fewest places.
- **NaN and the infinities, as PostgreSQL has them.** `NUMERIC` holds NaN and, since version 14, ±Infinity, so a
  `decimal` holds them too and a column reads back whole. Arithmetic on finite values never makes one: a division by
  zero is `domain_error`. NaN goes through every operation; `inf − inf` and `0 × inf` are NaN. NaN equals NaN and
  orders above everything, so the order is total: −Infinity < every finite value < +Infinity < NaN. There is no −0.
- **An error of the program throws.** A division by zero, [rounding](../../core/rounding.md)`::unnecessary` when the result
  is not exact, a square root of a negative value, fewer than one digit, a scale past `int32_t` (`domain_error`,
  `length_error`).
- **An error of data does not.** [parse](parse.md) of text that is not a decimal is an
  [expected](../../core/expected/README.md) whose [parse_error](../parse_error/README.md) has the offset and the
  message; a value whose exponent in scientific form is past a million either way is such an error, as a dozen
  bytes would otherwise ask for megabytes when the value meets another. A decimal the program itself writes is constructed,
  `math::decimal price("19.99")`.
- **PostgreSQL's range and beyond.** `NUMERIC` keeps up to 131072 digits before the point and 16383 after; the
  unscaled part has no limit but memory and the scale goes to ±2³¹.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](decimal.md) | constructs a decimal: zero, a whole number, an unscaled part and a scale, a double exactly, a fraction rounded, a text |
| `(destructor)` | drops the unscaled part; its object is left to the collector |
| [operator=](operator_assign.md) | assigns another decimal |
| [shortest](shortest.md) | the shortest decimal that reads back as a double (static) |
| [nan](nan.md) | NaN (static) |
| [infinity](infinity.md) | +Infinity (static) |
| [parse](parse.md) | reads a decimal (static) |

#### Parts

| Function | Description |
|---|---|
| [unscaled](unscaled.md) | the unscaled part |
| [scale](scale.md) | the digits after the point, or the zeros before it when negative |
| [precision](precision.md) | the digits of the unscaled part |
| [sign](sign.md) | −1, 0 or 1 |
| [is_nan](is_nan.md) | whether the value is NaN |
| [is_infinite](is_infinite.md) | whether the value is an infinity |
| [is_finite](is_finite.md) | whether the value is a number |

#### Arithmetic

| Function | Description |
|---|---|
| [operator+=, operator-=, operator\*=, operator%=, operator-](operator_arith.md) | the compound assignments and the negation |
| [div](div.md) | the quotient to a number of places |
| [div_precision](div_precision.md) | the quotient to a number of significant digits |
| [div_rem](div_rem.md) | the whole quotient and the remainder |
| [sqrt](sqrt.md) | the square root to a number of significant digits |
| [abs](abs.md) | the absolute value |

#### Rounding

| Function | Description |
|---|---|
| [rescale](rescale.md) | the value at a number of places |
| [round_precision](round_precision.md) | the value to a number of significant digits |
| [trim_scale](trim_scale.md) | the value at its fewest places |

#### Comparison

| Function | Description |
|---|---|
| [identical](identical.md) | the same value at the same scale |

#### Conversions

| Function | Description |
|---|---|
| [to_string](to_string.md) | the plain text, `"-0.0012"`, `"1500"` |
| [to_scientific](to_scientific.md) | the text with an exponent, `"1.5e+3"` |
| [to_double](to_double.md) | the nearest double |
| [to_int64](to_int64.md) | the value when it is whole and fits |
| [to_big_integer](to_big_integer.md) | the value rounded to a whole number |
| [to_rational](to_rational.md) | the value as a fraction, exactly |

## Non-member functions

| Function | Description |
|---|---|
| [operator+, operator-, operator\*, operator%](operator_arith.md) | the arithmetic of two decimals, exact |
| [operator==, operator\<=\>](operator_cmp.md) | compare two values |
| [operator\<\<](to_string.md) | writes `to_string()` to a stream |
| [format_value](format_value.md) | writes the decimal for [txt::format](../../txt/format.md), `{}`, `{:.2f}` or `{:e}` |

## Specializations

```cpp
template<>
struct std::hash<sgcl::math::decimal>;

template<>
struct sgcl::txt::formatter<sgcl::math::decimal>;
```

`std::hash` hashes the value, not its representation: the unscaled part modulo the prime 2⁶¹ − 1 times the inverse
of ten to the scale, so `1.0` and `1.00` hash alike without either being reduced, and a `map` or a `set` is keyed by
values. NaN, +Infinity and −Infinity have hashes of their own. The formatter tells [txt::format](../../txt/format.md)
which specifications a decimal takes — no type, `f` or `e`, and a precision — so that a literal pattern is checked
where it is compiled; the writing is [format_value](format_value.md)'s.

## Complexity

A sum, a difference and a comparison of two values at one scale are the `big_integer`'s; at two scales the value of
the smaller is multiplied by a power of ten first, and a comparison looks at the lengths before it does. A product is
the `big_integer` product. A quotient to *n* digits is one division of numbers of about *n* digits more than the
operands; a square root one integer square root of about 2*n* digits. Every value of up to eighteen digits is
computed in 64 and 128 bits.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::decimal price("19.99");
    math::decimal total = price * 3 + math::decimal("0.05");
    math::decimal tax = (total * math::decimal("0.23")).rescale(2);
    println("{} + {} = {}", total, tax, total + tax);

    println(math::decimal("0.1") + math::decimal("0.2") == math::decimal("0.3"));
    println("{} {}", math::decimal(1).div(3, 5), math::decimal(2).sqrt(30));
    println("{} {}", math::decimal(0.1).precision(), math::decimal::shortest(0.1));
}
```

Output:

```text
60.02 + 13.80 = 73.82
true
0.33333 1.41421356237309504880168872421
55 0.1
```

## See also

- [rounding](../../core/rounding.md): the ways a result is rounded
- [big_integer](../big_integer/README.md): the unscaled part
- [rational](../rational/README.md): the exact fractions a decimal converts to and from
- [parse_error](../parse_error/README.md): why a text is not a decimal
- [txt::format](../../txt/format.md): `{}`, `{:.2f}` and `{:e}` of a decimal
- [Benchmarks: decimal](../benchmarks.md#decimal)
- [README: math](../README.md)
