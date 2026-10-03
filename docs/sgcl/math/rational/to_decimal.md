[sgcl](../../README.md) › [math](../README.md) › [rational](../rational.md)

# sgcl::math::rational::to_decimal

```cpp
string to_decimal(size_t places) const;
```

The value as a decimal with `places` digits after the point, rounded to the nearest and a half away from zero, as
Go's `FloatString` and the rounding taught at school: `2/3` to three places is `"0.667"`, `-1/8` to two `"-0.13"`,
`1/2` to none `"1"`. A negative value that rounds to zero keeps its minus (`"-0.00"`), as `printf` does; no places
means no point. The rounding is of the exact value, once.

## Parameters

| Parameter | Description |
|---|---|
| `places` | the number of digits after the point |

## Return value

The text: a minus for a value below zero, the whole digits (at least a `0`), and the point and `places` digits
when `places` is not zero.

## Complexity

A power of ten of `places` digits, a product and a division of the parts by it, and the conversion of the quotient
to decimal.

## Exceptions

- `length_error` when `places` is at least the most characters a string holds, before anything is computed, or
  when the text has more digits than a string holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::rational r(2, 3);
    println("{} {} {}", r.to_decimal(3), r.to_decimal(20), r.to_decimal(0));
    println("{} {} {}", math::rational(-1, 8).to_decimal(2), math::rational(1, 2).to_decimal(0),
            math::rational(5).to_decimal(2));
    println("{} {}", math::rational(-1, 1000).to_decimal(2), math::rational(-5, 2).to_decimal(0));
}
```

Output:

```text
0.667 0.66666666666666666667 1
-0.13 1 5.00
-0.00 -3
```

## See also

- [to_double](to_double.md): the nearest double
- [format_value](format_value.md): `{:.5f}` writes the decimal in a field
- [floor](floor.md), [ceil](ceil.md): the whole numbers either side
- [sgcl::math::rational](../rational.md)
