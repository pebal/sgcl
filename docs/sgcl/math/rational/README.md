[sgcl](../../README.md) › [math](../README.md)

# sgcl::math::rational

```cpp
#include "sgcl/math/rational.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class rational;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::math::rational` is a fraction of two [big_integer](../big_integer/README.md)s, always in lowest terms: what Go's `math/big.Rat`
is, with the manners of a number. The arithmetic is exact — a third three times is one, and no sum of fractions ever
loses a digit — and a `big_integer` or any whole number of the language goes where a `rational` is wanted, so
`r * 2`, `r < 1` and `r == big_integer(5)` are written as they read. The standard library has nothing of the kind:
`std::ratio` is a fraction of the compiler's, fixed at compile time.

Go's `big.Rat` is a pointer whose methods write the result into a receiver (`z.Add(x, y)`); a `rational` is a value
and its operators make a new one: `third * 2 + 1`. Go's `SetString` is [parse](parse.md), `FloatString` is
[to_decimal](to_decimal.md) and `Float64` is [to_double](to_double.md); `{:.5f}` of
[txt::format](../../txt/format.md) writes the decimal. A sum and a product take the common factors out as Knuth does
(TAOCP vol. 2, 4.5.1), from the gcd of the denominators first, rather than reducing the whole product.

## Rules

- **Lowest terms, always.** The numerator carries the sign, the denominator is above zero and has no factor in
  common with it — `rational(6, -4)` is `-3/2` — so each value has one form, equality compares the parts and the
  hash is the parts'.
- **A value, like `big_integer`.** Two `big_integer`s: a copy is four words, the objects are shared and never changed.
- **Exact from a double.** Every finite `double` is a fraction whose denominator is a power of two, and
  `rational(double)` is that fraction: `rational(0.1)` is `3602879701896397/36028797018963968`. The text `"0.1"` is
  a tenth. The constructor from a `double` is `explicit`; `long double` and `bool` are refused, as for
  `big_integer`.
- **Rounded once, on the way out.** [to_double](to_double.md) is the nearest double to the exact value, a
  tie to the even one — not the quotient of the parts' doubles, which rounds three times and overflows for long
  parts; [to_decimal](to_decimal.md) rounds half away from zero, as Go's `FloatString`.
- **An error of the program throws.** A denominator of zero, the inverse of zero, a division by zero, zero to a
  negative power, NaN or an infinity made into a fraction are `domain_error`.
- **An error of data does not.** [parse](parse.md) of text that is not a fraction is an
  [expected](../../core/expected/README.md) whose [parse_error](../parse_error/README.md) has the offset and the message; a
  denominator of zero in the text is such an error, and so is an exponent past a million (`"1e9999999"`), since a
  dozen bytes would otherwise ask for megabytes. A fraction the program itself writes is constructed,
  `math::rational rate("0.075")`, and a wrong one throws.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](rational.md) | constructs a fraction: zero, a whole number, two parts, a double exactly, a text |
| `(destructor)` | drops the two parts; their objects are left to the collector |
| [operator=](operator_assign.md) | assigns another fraction |

#### Parts

| Function | Description |
|---|---|
| [numerator](numerator.md) | the numerator, with the sign |
| [denominator](denominator.md) | the denominator, above zero |

#### Arithmetic

| Function | Description |
|---|---|
| [operator+=, operator-=, operator\*=, operator/=, operator-](operator_arith.md) | the compound assignments and the negation |
| [abs](abs.md) | the absolute value |
| [inverse](inverse.md) | one over the fraction |
| [pow](pow.md) | the fraction to a whole power, a negative one too |
| [floor](floor.md) | the largest whole number not above |
| [ceil](ceil.md) | the smallest whole number not below |

#### Conversions

| Function | Description |
|---|---|
| [to_string](to_string.md) | the fraction as `"3/4"`, a whole number as `"-5"` |
| [to_decimal](to_decimal.md) | the decimal with a number of places, half away from zero |
| [to_double](to_double.md) | the nearest double, a tie to the even one |
| [parse](parse.md) | reads a fraction or a decimal (static) |

## Non-member functions

| Function | Description |
|---|---|
| [operator+, operator-, operator\*, operator/](operator_arith.md) | the arithmetic of two fractions, exact |
| [operator==, operator\<=\>](operator_cmp.md) | compare two fractions |
| [operator\<\<](to_string.md) | writes `to_string()` to a stream |
| [format_value](format_value.md) | writes the fraction for [txt::format](../../txt/format.md), `{}` or `{:.5f}` |

## Specializations

```cpp
template<>
struct std::hash<sgcl::math::rational>;

template<>
struct sgcl::txt::formatter<sgcl::math::rational>;
```

`std::hash` hashes the two parts, mixed: equal fractions have one form in lowest terms and so one hash, and a `map`
or a `set` is keyed by fractions. The formatter tells [txt::format](../../txt/format.md) which specifications a
fraction takes — no type or `f`, and a precision — so that a literal pattern is checked where it is compiled; the
writing is [format_value](format_value.md)'s.

## Complexity

A sum or a product is a few multiplications and one or two gcds of the parts; a whole number plus a whole number
(both denominators 1) is the `big_integer` sum and nothing more. The gcds are of the smaller numbers: a sum takes
the gcd of the two denominators and then only of what can still be common, a product the gcds across the two
fractions before it multiplies, so neither ever takes the gcd of a whole product. The parts grow as the arithmetic
asks — the harmonic sum of the first *n* terms has a denominator of about *n* · log₂ e bits — and nothing is ever
rounded away; where a bound on the size is wanted, round with [to_decimal](to_decimal.md) or
[floor](floor.md) and go on from that.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    // Exact: a third three times is one, and the harmonic sum has no error
    math::rational third(1, 3);
    println(third + third + third == 1);
    math::rational sum;
    for (int k : range(1, 11)) {
        sum += math::rational(1, k);
    }
    println("{} = {}", sum.to_string(), sum.to_decimal(6));

    // 0.1 as a double is not a tenth; as text it is
    println(math::rational(0.1).to_string());
    math::rational tenth("0.1");  // a literal: constructed
    println("{} {}", tenth.to_string(), math::rational(0.1) == tenth);

    // Rounded once, from the exact value
    println("{:.3f} {} {}", math::rational(-1, 8), (third * 2).to_double(), third.pow(-2));

    auto bad = math::rational::parse("3/0");  // text that may be wrong: parsed
    println(bad.error().message());
}
```

Output:

```text
true
7381/2520 = 2.928968
3602879701896397/36028797018963968
1/10 false
-0.125 0.6666666666666666 9
a denominator of zero at byte 2
```

## See also

- [big_integer](../big_integer/README.md): the parts, and the whole numbers a fraction takes
- [parse_error](../parse_error/README.md): why a text is not a fraction
- [txt::format](../../txt/format.md): `{}` and `{:.5f}` of a fraction
- [Benchmarks: rational](../benchmarks.md#rational)
- [README: math](../README.md)
