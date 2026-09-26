# sgcl::math::rational

```cpp
#include "sgcl/math/rational.h"   // or "sgcl/math/math.h"

namespace sgcl::math {
    class rational;
}
```

`math::rational` is a fraction of two [`big_integer`](big_integer.md)s, always in lowest terms: what Go's `math/big.Rat` is, with the manners of a number. The arithmetic is exact — a third three times is one, and no sum of fractions ever loses a digit — and a `big_integer` or any whole number of the language goes where a `rational` is wanted, so `r * 2`, `r < 1` and `r == big_integer(5)` are written as they read.

```cpp
using namespace sgcl;

math::rational third(1, 3);
math::rational sum = third + third + third;       // 1
string s = (third * 2).to_decimal(4);              // "0.6667"
bool exact = math::rational(0.1) != math::rational(1, 10);   // true: 0.1 is not a tenth
```

## Rules

- **Lowest terms, always.** The numerator carries the sign, the denominator is above zero and has no factor in common with it — `rational(6, -4)` is `-3/2` — so each value has one form, equality compares the parts and the hash is the parts'. Sums and products take the common factors out the way Knuth does (TAOCP vol. 2, 4.5.1): the gcd of the denominators first, then only of what can still be common, so the gcds are of the small numbers rather than of the products.
- **A value, like `big_integer`.** Two `big_integer`s: a copy is four words, the objects are shared and never changed, and a `rational` lives where a `tracked_ptr` may — on a stack, in a managed object, in a container of the library.
- **Exact from a double.** Every finite `double` is a fraction whose denominator is a power of two, and `rational(double)` is that fraction: `rational(0.1)` is `3602879701896397/36028797018963968`. The text `"0.1"` is a tenth. The constructor from a `double` is `explicit`; `long double` and `bool` are refused, as for `big_integer`.
- **Rounded once, on the way out.** `to_double` is the nearest double to the exact value, a tie to the even one — not the quotient of the parts' doubles, which rounds three times and overflows for long parts; `to_decimal` rounds half away from zero, as Go's `FloatString`.
- **An error of the program throws.** A denominator of zero, the inverse of zero, a division by zero, zero to a negative power, NaN or an infinity made into a fraction are `domain_error`.
- **An error of data does not.** `parse` of text that is not a fraction is an [`expected`](../core/expected.md) whose [`parse_error`](big_integer.md) has `offset()` and `message()`; a denominator of zero in the text is such an error, and so is an exponent past a million (`"1e9999999"`), since a dozen bytes would otherwise ask for megabytes.

## Members

### Construction

```cpp
rational() noexcept;                                          // 0
template<std::integral T> rational(T value);                  // implicit: n/1
rational(big_integer value) noexcept;                         // implicit: n/1
rational(big_integer numerator, big_integer denominator);     // reduced; 0 → domain_error
explicit rational(double value);                              // exactly; NaN, ±∞ → domain_error
rational(bool) = delete;
explicit rational(long double) = delete;

static expected<rational, parse_error> parse(const string& text);
```

`parse` reads a fraction — `"3/4"`, `"-5"`, `"+0/7"`, digits, a slash and digits, the sign in front and none in the denominator — or a decimal: `"-0.125"`, `".5"`, `"7."`, `"1.5e-3"`, `"2E10"`, digits with a point (either side may be empty, not both) and an exponent of at most a million either way. Nothing else: no space, no `_`, no `inf` or `nan`. The error's `offset()` is the byte where reading stopped, the first digit of a denominator of zero, or the first byte after the `e` of an exponent too large.

### Parts

```cpp
const big_integer& numerator() const noexcept;               // with the sign
const big_integer& denominator() const noexcept;             // above zero, in lowest terms with the numerator
```

### Arithmetic and order

```cpp
friend rational operator+(const rational&, const rational&);  // and -, *, / (by zero → domain_error)
rational operator-() const;
rational& operator+=(const rational&);                        // and -=, *=, /=
friend bool operator==(const rational&, const rational&) noexcept;
friend std::strong_ordering operator<=>(const rational&, const rational&);

rational abs() const;
rational inverse() const;                                     // 1/x; of zero → domain_error
rational pow(int64_t exponent) const;                         // a negative exponent allowed; 0^0 == 1
big_integer floor() const;
big_integer ceil() const;
```

The operators are hidden friends taking two `rational`s, and a whole number or a `big_integer` on either side is converted, so `1 - third`, `third * 3 == 1` and `big_integer(6) > r` need no second set. `pow` raises the parts each (a power of a fraction in lowest terms is in lowest terms); `floor` and `ceil` are the whole numbers below and above, `floor(-7/2) == -4`.

### Conversions

```cpp
string to_string() const;                                     // "3/4", "-5" for a whole number
string to_decimal(size_t places) const;                       // "0.667"; half away from zero
double to_double() const;                                     // nearest, a tie to even
```

`to_decimal` writes `places` digits after the point, rounded to the nearest with a half away from zero — `2/3` to three places is `"0.667"`, `-1/8` to two `"-0.13"`, `1/2` to none `"1"`; a negative value that rounds to zero keeps its minus (`"-0.00"`), as `printf` does, and no places means no point. `to_double` goes past the largest double to an infinity and below the smallest subnormal to a zero, each of the sign; it is not `noexcept`, since the division of long parts allocates.

### Text, hashing, streams

```cpp
void format_value(txt::format_sink&, const rational&, const txt::format_spec&);   // found by txt::format
std::ostream& operator<<(std::ostream&, const rational&);                         // to_string()
template<> struct std::hash<rational>;
```

[`txt::format`](../txt/format.md) writes `{}` as `to_string` (`"3/4"`) and `{:f}`, `{:.5f}` or `{:.5}` as `to_decimal` with that many places, six when none is given as for a `double`; `+` and a space for the sign of a value not negative, and the width, fill and alignment of any number, right by default, the zeros of `{:08.3f}` after the sign. Any other type — `{:x}`, `{:d}` — is an error of the compiler in a literal pattern and `nullopt` from a runtime one.

## What it costs

A sum or a product is a few multiplications and one or two gcds of the parts; a whole number plus a whole number (both denominators 1) is the `big_integer` sum and nothing more. The parts grow as the arithmetic asks — the harmonic sum of the first *n* terms has a denominator of about *n* · log₂ e bits — and nothing is ever rounded away; where a bound on the size is wanted, round with `to_decimal` or `floor` and go on from that.

## Example

```cpp
#include "sgcl/sgcl.h"
#include "sgcl/math/math.h"
#include <iostream>

using namespace sgcl;

int main() {
    // Exact: a third three times is one, and the harmonic sum has no error
    math::rational third(1, 3);
    std::cout << (third + third + third == 1) << '\n';
    math::rational sum;
    for (auto k : range(1, 11)) {
        sum += math::rational(1, k);
    }
    std::cout << sum.to_string() << " = " << sum.to_decimal(6) << '\n';

    // 0.1 as a double is not a tenth; as text it is
    std::cout << math::rational(0.1).to_string() << '\n';
    auto tenth = math::rational::parse("0.1");
    if (!tenth) {
        return 1;
    }
    std::cout << tenth->to_string() << ' ' << (math::rational(0.1) == *tenth) << '\n';

    // Rounded once, from the exact value
    std::cout << txt::format("{:.3f} {} {}", math::rational(-1, 8), (third * 2).to_double(), third.pow(-2)) << '\n';

    auto bad = math::rational::parse("3/0");
    std::cout << bad.error().message() << '\n';
}
```

Output:

```text
1
7381/2520 = 2.928968
3602879701896397/36028797018963968
1/10 0
-0.125 0.6666666666666666 9
a denominator of zero at byte 2
```

## See also

[`big_integer`](big_integer.md), the parts and `parse_error`; [`txt::format`](../txt/format.md); the module's [README](README.md), with the table of SGCL against Go.
