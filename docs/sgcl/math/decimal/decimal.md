[sgcl](../../README.md) › [math](../README.md) › [decimal](README.md)

# sgcl::math::decimal::decimal

```cpp
decimal() noexcept;                                                                    // (1)
template<std::integral T>
requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
decimal(T value) noexcept;                                                             // (2)
decimal(big_integer value) noexcept;                                                   // (3)
decimal(big_integer unscaled, int32_t scale) noexcept;                                 // (4)
template<std::integral T>
requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
decimal(T unscaled, int32_t scale) noexcept;                                           // (5)
explicit decimal(double value) noexcept;                                               // (6)
decimal(const rational& value, int32_t scale, rounding mode = rounding::half_even);    // (7)
template<std::same_as<bool> B>
decimal(B) = delete;                                                                   // (8)
explicit decimal(long double) = delete;                                                // (9)
explicit decimal(const string& text);                                                  // (10)
decimal(const decimal&) noexcept;                                                      // (11), implicitly declared
decimal(decimal&&) noexcept;                                                           // (12), implicitly declared
```

Constructs a decimal.

1. Zero, at scale 0.
2. A whole number at scale 0, implicitly, from any whole number of the language but `bool`: `d * 3`, `d + 1` and
   `d == 0` take the number as a decimal. Nothing is allocated for a value `int64_t` holds.
3. A `big_integer` at scale 0, implicitly.
4. `unscaled` × 10^−`scale`: `decimal(150, 2)` is `1.50`, `decimal(15, -2)` is `1500` (written `1.5e+3` in
   scientific form).
5. (4) for a whole number of the language, so that `decimal(150, 2)` takes this form and not (7), which a whole
   number converts to as well.
6. The double exactly: every finite double is a decimal of at most 767 significant digits, and that is the one made,
   at the smallest scale that holds it (0 for a whole number) — `decimal(0.1)` is
   `0.1000000000000000055511151231257827021181583404541015625`. NaN and the infinities are the decimal's own NaN and
   infinities; −0.0 is 0. The constructor is `explicit`: a double does not take part in the arithmetic by itself.
   [shortest](shortest.md) gives the decimal a double was written as, `0.1`.
7. The fraction rounded to `scale` places with the [rounding](../../core/rounding.md): `decimal(rational(2, 3), 4)` is
   `0.6667`.
8. Deleted: a `bool` would arrive as a whole number. The template takes a `bool` alone and not a pointer, so
   `decimal("1.5")` goes to (10).
9. Deleted: a `long double` would arrive rounded to a double.
10. The number a literal in the program writes, `math::decimal price("19.99")`: the value [parse](parse.md) reads,
    at the text's scale, or `bad_expected_access<parse_error>` with `parse`'s message. A text from outside the
    program is parsed, and its error is a value.
11. A copy: the unscaled part's object is shared.
12. A move: the unscaled part's object is handed on; the decimal moved from is zero at its scale (NaN and the
    infinities stay as they were).

## Parameters

| Parameter | Description |
|---|---|
| `value` | the whole number (2–3), the double (6) or the fraction (7) |
| `unscaled` | the unscaled part, with the sign |
| `scale` | the digits after the point, or the zeros before it when negative |
| `mode` | how the fraction is rounded to the scale |
| `text` | a decimal, as [parse](parse.md) reads it |

## Complexity

- (1–5), (11–12) Constant.
- (6) A power of five of up to 1074 and a product: the value has at most 767 significant digits.
- (7) A power of ten of `scale` digits, a product and a division of the fraction's parts.
- (10) As [parse](parse.md).

## Exceptions

- (1–6), (11–12) None.
- (7) `domain_error` when `mode` is `rounding::unnecessary` and the fraction does not end within `scale` places.
- (10) `bad_expected_access<parse_error>` when `text` is not a decimal; its `error()` is the
  [parse_error](../parse_error/README.md) of `parse`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::decimal zero;
    math::decimal cents(1999, 2);
    math::decimal thousands(15, -2);
    println("{} {} {} {}", zero, cents, thousands, thousands.to_scientific());

    math::decimal price("19.99");  // a literal: constructed
    println("{} {}", price == cents, price * 3);

    println(math::decimal(0.1));
    println(math::decimal(0.75));
    println(math::decimal(math::rational(2, 3), 4));
    println(math::decimal(math::rational(2, 3), 4, rounding::down));
}
```

Output:

```text
0 19.99 1500 1.5e+3
true 59.97
0.1000000000000000055511151231257827021181583404541015625
0.75
0.6667
0.6666
```

## See also

- [parse](parse.md): text from outside the program
- [shortest](shortest.md): the decimal a double was written as
- [operator=](operator_assign.md): the assignments
- [sgcl::math::decimal](README.md)
