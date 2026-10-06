[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::big_float

```cpp
big_float() noexcept;                                                                            // (1)
template<std::integral T>
requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
big_float(T value) noexcept;                                                                     // (2)
big_float(const big_integer& value) noexcept;                                                    // (3)
explicit big_float(double value);                                                                // (4)
template<std::integral T>
requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
big_float(T value, uint32_t precision, rounding mode = rounding::half_even);                     // (5)
big_float(const big_integer& value, uint32_t precision, rounding mode = rounding::half_even);    // (6)
big_float(double value, uint32_t precision, rounding mode = rounding::half_even);                // (7)
big_float(const rational& value, uint32_t precision, rounding mode = rounding::half_even);       // (8)
big_float(const decimal& value, uint32_t precision, rounding mode = rounding::half_even);        // (9)
big_float(const big_float& value, uint32_t precision, rounding mode);                            // (10)
template<std::same_as<bool> B>
big_float(B) = delete;                                                                           // (11)
explicit big_float(long double) = delete;                                                        // (12)
explicit big_float(const string& text);                                                          // (13)
big_float(const big_float&) noexcept = default;                                                  // (14)
big_float(big_float&& other) noexcept;                                                           // (15)
```

Constructs a binary floating-point number. A precision is the number of bits of the mantissa, from 1 to 2³² − 1;
the mode is how the value is rounded to it ([rounding](../../core/rounding.md)).

- (1) +0 at precision 0: an operand that takes the other's precision, as Go's zero value.
- (2) A whole number exactly, at 64 bits (128 for the compiler's 128-bit types), implicitly: `x * 2` and `x < 0` take
   the number as a value.
- (3) A [big_integer](../big_integer/README.md) exactly, at 64 bits or its length if longer, implicitly.
- (4) A double exactly, at 53 bits; ±0 and the infinities as themselves, NaN `domain_error`.
- (5–9) A whole number, a [big_integer](../big_integer/README.md), a double, a fraction or a
  [decimal](../decimal/README.md) rounded once to `precision` bits by `mode`; a decimal's NaN is `domain_error`, its
  infinities are this type's. A whole number is taken exactly before the rounding: 2⁵³ + 1 at 200 bits is itself, not
  the double next to it.
- (10) The value rounded to another precision and mode: Go's `SetPrec` and `SetMode` on a copy.
- (11) Deleted: a `bool` would arrive as a whole number.
- (12) Deleted: a `long double` would arrive rounded to a double.
- (13) The number a literal in the program writes, at 64 bits: the value [parse](parse.md) reads, or
    `bad_expected_access<parse_error>`.
- (14) A copy: the mantissa's object is shared.
- (15) A move, which is the copy (14): the value moved from keeps its value.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the number |
| `precision` | the bits of the mantissa |
| `mode` | how the value is rounded to them |
| `text` | a number, as [parse](parse.md) reads it |
| `other` | the value moved from |

## Complexity

- (1–2), (14–15) Constant.
- (3–10) Linear in the value's length; a fraction one division of its parts.
- (13) As [parse](parse.md).

## Exceptions

- (4), (9) `domain_error` for NaN.
- (5–10) `invalid_argument` when `precision` is 0 and the value is not zero; `domain_error` when `mode` is
  `rounding::unnecessary` and the value does not fit the precision.
- (13) `bad_expected_access<parse_error>` when `text` is not a number.
- The others: none.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_float n = 7;
    math::big_float d(0.1);
    math::big_float third(math::rational(1, 3), 20);
    math::big_float down(math::rational(1, 3), 20, rounding::down);
    println("{} {} {} {}", n.precision(), d.precision(), third, down);
    math::big_float tenth(math::decimal("0.1"), 10);
    println("{} {}", tenth, tenth.to_hex());
    println(math::big_float(d, 8, rounding::half_even));
    println(math::big_float(9007199254740993LL, 200));
}
```

Output:

```text
64 53 0.3333335 0.333333
0.1 0x1.998p-4
0.1
9007199254740993
```

## See also

- [parse](parse.md): text from outside the program
- [precision](precision.md), [mode](mode.md): what a value carries
- [sgcl::math::big_float](README.md)
