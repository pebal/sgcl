[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::operator+=, operator-=, operator\*=, operator/=, operator-, sgcl::math::operator+, operator-, operator\*, operator/ (sgcl::math::big_float)

```cpp
friend big_float operator+(const big_float& a, const big_float& b);    // (1)
friend big_float operator-(const big_float& a, const big_float& b);    // (2)
friend big_float operator*(const big_float& a, const big_float& b);    // (3)
friend big_float operator/(const big_float& a, const big_float& b);    // (4)
big_float operator-() const noexcept;                                  // (5)
big_float& operator+=(const big_float& b);                             // (6)
big_float& operator-=(const big_float& b);                             // (7)
big_float& operator*=(const big_float& b);                             // (8)
big_float& operator/=(const big_float& b);                             // (9)
```

The arithmetic of binary floating point: each result is the exact one rounded once to the larger precision of the
two operands by the left one's mode (Go's receiver of precision 0). A whole number on either side converts at 64
bits; a `double` does not convert by itself.

- (1) The sum. A value far below the last bit of the other at the precision is folded into a sticky bit rather than
   added out.
- (2) The difference.
- (3) The product.
- (4) The quotient, to two bits more than the precision and the remainder as a sticky bit; x/0 is an infinity of the
   signs, 0/0 `domain_error`.
- (5) The negation: the sign changed, −0 for +0.
- (6–9) `*this = *this op b`.

∞ − ∞ (and ∞ + −∞), 0·∞ and ∞/∞ are `domain_error`; a sum of zeros is −0 only when both are; an exact cancellation
is +0, −0 when rounding by `floor`; a result past the exponent's range is an infinity, below it a zero.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the values, or whole numbers converted |

## Return value

- (1–5) The value computed.
- (6–9) `*this`.

## Complexity

- (1–2) Linear in the longer mantissa.
- (3–4) The multiplication and the division of the mantissas.
- (5) Constant.
- (6–9) As (1–4).

## Exceptions

- (1–4), (6–9) `domain_error` for a result that would be NaN, and for an inexact one under `rounding::unnecessary`;
  `*this` is left as it was.
- (5) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_float a(math::rational(1, 3), 100);
    math::big_float b(0.1);
    println("{} {}", a + b, (a + b).precision());
    println("{} {}", a * 3, a / 0);
    // a tenth is no binary fraction: ten of them at 200 bits miss 1 in the last place
    math::big_float sum;
    for (int i = 0; i < 10; ++i) {
        sum += math::big_float(math::rational(1, 10), 200);
    }
    println(sum);
}
```

Output:

```text
0.4333333333333333388844484564592 100
1 +Inf
1.000000000000000000000000000000000000000000000000000000000001
```

## See also

- [sqrt](sqrt.md): the square root
- [operator==, operator\<=\>](operator_cmp.md): the comparisons
- [sgcl::math::big_float](README.md)
