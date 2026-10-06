[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::div_precision

```cpp
decimal div_precision(const decimal& by, int32_t digits, rounding mode = rounding::half_even) const;
```

The quotient of this value by `by`, rounded to `digits` significant digits with the [rounding](../../core/rounding.md), as
Python's `decimal` divides in a context of that precision (`getcontext().prec = digits`), and Java's
`divide(divisor, new MathContext(digits))`. An inexact quotient has all its digits: `1.div_precision(3, 5)` is
`0.33333`. An exact one drops the zeros at its end down to the scale of this value less the divisor's, the ideal
scale of a quotient: `1.div_precision(4, 28)` is `0.25` and `1.div_precision(0.01, 28)` is `1e+2` (scale −2), as in
Python.

NaN divided by anything, or anything by NaN, is NaN; an infinity by a finite value is an infinity of the quotient's
sign, by an infinity NaN; a finite value by an infinity is zero.

## Parameters

| Parameter | Description |
|---|---|
| `by` | the divisor |
| `digits` | the significant digits of the quotient, at least 1 |
| `mode` | how the quotient is rounded to them |

## Return value

The quotient: `digits` digits or fewer, at the scale they take.

## Complexity

The lengths of the two (to know where the quotient's first digit is), and one division of numbers of about
`digits` digits more than the operands.

## Exceptions

- `domain_error` when `by` is zero, when `digits` is below 1, and when `mode` is `rounding::unnecessary` and the
  quotient has more than `digits` digits.
- `length_error` when the scale of the quotient is past `int32_t`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::decimal one(1);
    println("{} {} {}", one.div_precision(3, 5), one.div_precision(4, 28),
            one.div_precision(math::decimal("0.01"), 28));
    println("{} {}", one.div_precision(7, 40),
            math::decimal(2).div_precision(3, 3, rounding::down));
    println(math::decimal("1e100").div_precision(7, 10).to_scientific());
}
```

Output:

```text
0.33333 0.25 100
0.1428571428571428571428571428571428571429 0.666
1.428571429e+99
```

## See also

- [div](div.md): the quotient to a number of places
- [round_precision](round_precision.md): a value to a number of digits
- [sgcl::math::decimal](README.md)
