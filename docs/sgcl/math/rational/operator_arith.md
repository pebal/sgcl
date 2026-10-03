[sgcl](../../README.md) › [math](../README.md) › [rational](../rational.md)

# sgcl::math::rational::operator+=, operator-=, operator\*=, operator/=, operator-, sgcl::math::operator+, operator-, operator\*, operator/ (sgcl::math::rational)

```cpp
/*(1)*/ friend rational operator+(const rational& a, const rational& b) noexcept;
/*(2)*/ friend rational operator-(const rational& a, const rational& b) noexcept;
/*(3)*/ friend rational operator*(const rational& a, const rational& b) noexcept;
/*(4)*/ friend rational operator/(const rational& a, const rational& b);
/*(5)*/ rational operator-() const noexcept;
/*(6)*/ rational& operator+=(const rational& b) noexcept;
/*(7)*/ rational& operator-=(const rational& b) noexcept;
/*(8)*/ rational& operator*=(const rational& b) noexcept;
/*(9)*/ rational& operator/=(const rational& b);
```

The arithmetic of fractions, exact: nothing is ever rounded, and the result is in lowest terms. The binary
operators are hidden friends taking two fractions, and a whole number or a `big_integer` on either side is
converted, so `1 - third`, `third * 3 == 1` and `big_integer(6) / r` need no second set. A `double` is not converted
by itself: `r + 0.5` does not compile, `r + rational(0.5)` adds the double's exact value.

1. The sum. For `p/q + r/s`, with `g` the gcd of the denominators `q` and `s`, the numerator is `p·(s/g) + r·(q/g)`
   and the denominator `(q/g)·s`, of which only a factor of `g` can be common with the numerator: the second gcd
   is of the numerator and `g`, not of the numerator and the whole product (Knuth, TAOCP vol. 2, 4.5.1).
2. The difference, as (1).
3. The product. `(p/q)·(r/s)` with `gcd(p, s)` and `gcd(r, q)` taken out first, so the product is in lowest terms
   without a gcd of the products.
4. The quotient, `a` times the [inverse](inverse.md) of `b`. A division by zero is `domain_error`.
5. The negation.
6. `*this = *this + b`.
7. `*this = *this - b`.
8. `*this = *this * b`.
9. `*this = *this / b`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the fractions, or whole numbers or `big_integer`s converted to fractions |

## Return value

- (1–5) The fraction computed.
- (6–9) `*this`.

## Complexity

- (1–2) A few multiplications and one or two gcds of the parts; with both denominators 1, the `big_integer` sum
  and nothing more.
- (3) Two gcds across the fractions and two products; with both denominators 1, the `big_integer` product.
- (4), (9) As (3).
- (5) Constant: the numerator's objects are shared.
- (6–8) As (1–3).

## Exceptions

- (1–3), (5–8) None.
- (4), (9) `domain_error` when `b` is zero; `*this` is left as it was.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::rational third(1, 3);
    println("{} {} {}", third + third + third, 1 - third, third * 3 == 1);
    println("{} {} {}", math::rational(3, 4) * math::rational(2, 9), third / 4, -third);
    println("{}", math::big_integer(6) / math::rational(4, 5));

    math::rational sum;
    for (int k : {2, 3, 6}) {
        sum += math::rational(1, k);
    }
    println(sum);

    try {
        sum /= 0;
    } catch (const domain_error& e) {
        println("{} {}", e.what(), sum);
    }
}
```

Output:

```text
1 2/3 true
1/6 1/12 -1/3
15/2
1
sgcl::math::rational::inverse: the inverse of zero 1
```

## See also

- [operator==, operator\<=\>](operator_cmp.md): the comparisons
- [inverse](inverse.md), [pow](pow.md), [abs](abs.md): the other arithmetic
- [sgcl::math::rational](../rational.md)
