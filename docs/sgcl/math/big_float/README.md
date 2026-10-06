[sgcl](../../README.md) › [math](../README.md)

# sgcl::math::big_float

```cpp
#include "sgcl/math/big_float.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class big_float;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::math::big_float` is a binary floating-point number of any precision: a sign, a mantissa of at most
`precision` bits, a binary exponent, and every result rounded to the precision in one of the modes of
[rounding](../../core/rounding.md). It is Go's `math/big.Float` as a value: Go's `z.Add(x, y)` is `x + y`, its
`SetPrec` and `SetMode` are the [constructor](big_float.md) of a value at another precision, its `Text` is
[to_string](to_string.md), [to_scientific](to_scientific.md), [to_hex](to_hex.md) and the formats of
[txt::format](../../txt/format.md), and Go's own answers are this type's tests. Where a `double` keeps 53 bits, a
`big_float` keeps as many as asked — 200 bits of the square root of 2, a thousand of π — and rounds each operation
once, correctly.

## Rules

- **Precision and mode belong to the value**, as Go keeps them on the receiver: a result has the larger precision
  of its two operands and the mode of the left one, a precision of 0 — the default value, +0 — taking the other's.
  A whole number comes in exactly at 64 bits (more for a longer [big_integer](../big_integer/README.md)), a double
  exactly at 53, as Go's `SetInt64`, `SetInt` and `SetFloat64`; a fraction, a decimal and text are rounded once to
  the precision asked for.
- **The modes** are [rounding](../../core/rounding.md)'s: `half_even` is Go's `ToNearestEven`, `half_up`
  `ToNearestAway`, `down` `ToZero`, `up` `AwayFromZero`, `floor` and `ceiling` its `ToNegativeInf` and
  `ToPositiveInf`; `half_down` and `unnecessary` (an assertion that nothing is rounded) besides.
- **±0 and ±infinity, no NaN.** An operation that would make NaN — ∞ − ∞, 0·∞, 0/0, ∞/∞, the square root of a
  negative number — is `domain_error`, where Go panics with `ErrNaN`; x/0 is an infinity of the sign. A sum of
  zeros is −0 only when both are; an exact cancellation is +0, −0 when rounding by `floor`.
- **Go's exponent range.** With the value written 0.m × 2^exp, exp stays within `int32_t`; past it a result is an
  infinity or a zero of its sign.
- **The shortest text reads back.** [to_string](to_string.md) writes the fewest decimal digits that read back to the
  same value at its precision — Go's choice of digits, but for a power of two, where the next value below is nearer
  and Go's digits may read back as it: a value's text is always its own. A number of digits is asked through
  [txt::format](../../txt/format.md) (`{:.50f}`, `{:.10e}`).
- **A value.** A copy shares the mantissa's object; a move is the copy, as for [rational](../rational/README.md). A
  mantissa that fits in 63 bits allocates nothing.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](big_float.md) | +0, a whole number, a double, a fraction, a decimal, a text; any of them at a precision and a mode |
| `(destructor)` | drops the mantissa; its object is left to the collector |
| [operator=](operator_assign.md) | assigns another value |
| [infinity](infinity.md) | an infinity (static) |
| [parse](parse.md) | reads a decimal or a hexadecimal number (static) |

#### Arithmetic

| Function | Description |
|---|---|
| [operator+=, operator-=, operator\*=, operator/=, operator-](operator_arith.md) | the compound assignments and the negation |
| [sqrt](sqrt.md) | the square root |
| [abs](abs.md) | the absolute value |

#### Parts

| Function | Description |
|---|---|
| [sign](sign.md) | −1, 0 or 1 |
| [signbit](signbit.md) | whether the sign is set, −0 included |
| [precision](precision.md) | the bits of the mantissa |
| [mode](mode.md) | the rounding of the results |
| [is_infinite](is_infinite.md) | whether the value is an infinity |
| [is_integer](is_integer.md) | whether the value is a whole number |
| [exponent](exponent.md) | the binary exponent of 0.m × 2^exp |

#### Conversions

| Function | Description |
|---|---|
| [to_string](to_string.md) | the shortest decimal that reads back |
| [to_scientific](to_scientific.md) | the same with an exponent |
| [to_hex](to_hex.md) | the exact value in hexadecimal, as C's `%a` |
| [to_double](to_double.md) | the nearest double |
| [to_big_integer](to_big_integer.md) | the value rounded to a whole number |
| [to_rational](to_rational.md) | the value as a fraction, exactly |
| [to_decimal](to_decimal.md) | the value as a decimal, exactly |

## Non-member functions

| Function | Description |
|---|---|
| [operator+, operator-, operator\*, operator/](operator_arith.md) | the arithmetic of two values, each rounded once |
| [operator==, operator\<=\>](operator_cmp.md) | compare two values |
| [operator\<\<](to_string.md) | writes `to_string()` to a stream |
| [format_value](format_value.md) | writes the value for [txt::format](../../txt/format.md), `{}`, `{:.30f}` or `{:e}` |

## Specializations

```cpp
template<>
struct std::hash<sgcl::math::big_float>;

template<>
struct sgcl::txt::formatter<sgcl::math::big_float>;
```

`std::hash` hashes the value: equal values of any precisions hash alike, and so do +0 and −0. The formatter says
which specifications [txt::format](../../txt/format.md) takes — no type, `e`, `f` or `g`, and a precision.

## Complexity

A sum is linear in the longer mantissa, a product and a quotient the [big_integer](../big_integer/README.md)
multiplication and division of mantissas of about the precision, a square root one integer square root of twice
the precision; each rounding is linear. A value of up to 63 bits of mantissa allocates nothing, and a result one
object.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_float two(2, 200);  // 2 at 200 bits
    println("{:.50f}", two.sqrt());
    math::big_float third = math::big_float(1, 100) / 3;
    println("{} {}", third, third.precision());
    println("{} {}", math::big_float(0.1), math::big_float(0.1).to_hex());
}
```

Output:

```text
1.41421356237309504880168872420969807856967187537695
0.3333333333333333333333333333335 100
0.1 0x1.999999999999ap-4
```

## See also

- [rounding](../../core/rounding.md): the modes
- [decimal](../decimal/README.md): decimal numbers of any precision
- [rational](../rational/README.md): exact fractions
- [Benchmarks: big_float](../benchmarks.md#big_float)
- [README: math](../README.md)
