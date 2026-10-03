[sgcl](../../README.md) › [math](../README.md) › [rational](../rational.md)

# sgcl::math::rational::rational

```cpp
/*(1)*/ rational() noexcept;
/*(2)*/ template<std::integral T>
        requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
        rational(T value) noexcept;
/*(3)*/ rational(big_integer value) noexcept;
/*(4)*/ rational(big_integer numerator, big_integer denominator);
/*(5)*/ explicit rational(double value);
/*(6)*/ template<std::same_as<bool> B>
        rational(B) = delete;
/*(7)*/ explicit rational(long double) = delete;
/*(8)*/ explicit rational(const string& text);
/*(9)*/ rational(const rational&) noexcept = default;
/*(10)*/ rational(rational&& other) noexcept;
```

Constructs a fraction.

1. Zero, `0/1`.
2. `value/1`, implicitly, from any whole number of the language but `bool`: `r * 2` and `r < 1` take the number as
   a fraction. Nothing is allocated for a value `int64_t` holds, as for a [big_integer](../big_integer.md).
3. `value/1`, implicitly: `r == big_integer(5)` takes the `big_integer` as a fraction.
4. `numerator/denominator` in lowest terms, the sign on the numerator: `rational(6, -4)` is `-3/2`. A denominator
   of zero is `domain_error`.
5. The double exactly: every finite double is a fraction whose denominator is a power of two, and that is the one
   made, so `rational(0.1)` is `3602879701896397/36028797018963968` and not `1/10` (the text `"0.1"` is a tenth).
   A `float` arrives promoted to a double, exactly. NaN and the infinities are `domain_error`. The constructor is
   `explicit`: a double does not take part in the arithmetic of fractions by itself, `r + 0.5` does not compile.
6. Deleted: a `bool` would arrive as a whole number. The template takes a `bool` alone and not a pointer, so
   `rational("3/4")` goes to (8) — a `const char*` to `bool` is a standard conversion, which would beat the
   user-defined one to `string`.
7. Deleted: a `long double` would arrive rounded to a double.
8. The number a literal in the program writes, `math::rational rate("0.075")`: the value [parse](parse.md) reads,
   or `bad_expected_access<parse_error>` with `parse`'s message. A text from outside the program is parsed, and
   its error is a value. The literal `0` still goes to (2), not to the text.
9. A copy: the two parts' objects are shared, nothing is copied but four words.
10. A move, which is the copy (9): the parts' objects are shared as by the copy, at its cost, and the fraction
    moved from keeps its value.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the whole number (2–3) or the double (5) |
| `numerator` | the numerator, with any sign |
| `denominator` | the denominator, with any sign but zero |
| `text` | a fraction or a decimal, as [parse](parse.md) reads it |
| `other` | the fraction moved from |

The copy (9) takes the fraction copied, unnamed in the declaration.

## Complexity

- (1–3), (9–10) Constant.
- (4) The gcd of the two parts and the division of each by it; none when the numerator is zero or the
  denominator 1 or -1.
- (5) Constant: a part has at most 1075 bits.
- (8) As [parse](parse.md).

## Exceptions

- (1–3), (9–10) None.
- (4) `domain_error` when `denominator` is zero.
- (5) `domain_error` when `value` is NaN or an infinity.
- (8) `bad_expected_access<parse_error>` when `text` is not a fraction; its `error()` is the
  [parse_error](../parse_error.md) of `parse`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::rational zero;
    math::rational seven = 7;
    math::rational big = math::big_integer("123456789012345678901234567890");
    math::rational ratio(6, -4);
    println("{} {} {} {}", zero, seven, big, ratio);

    println("{} {}", math::rational(0.75), math::rational(0.1));
    println("{}", math::rational("1.5e-3"));

    try {
        math::rational wrong("3/0");
    } catch (const bad_expected_access<math::parse_error>& e) {
        println(e.error().message());
    }
    try {
        math::rational none(1, 0);
    } catch (const domain_error& e) {
        println(e.what());
    }
}
```

Output:

```text
0 7 123456789012345678901234567890 -3/2
3/4 3602879701896397/36028797018963968
3/2000
a denominator of zero at byte 2
sgcl::math::rational: a denominator of zero
```

## See also

- [parse](parse.md): reads a text from outside the program
- [operator=](operator_assign.md): assigns another fraction
- [numerator](numerator.md), [denominator](denominator.md): the parts in lowest terms
- [sgcl::math::rational](../rational.md)
