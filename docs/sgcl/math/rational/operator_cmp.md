[sgcl](../../README.md) › [math](../README.md) › [rational](README.md)

# sgcl::math::operator==, operator\<=\> (sgcl::math::rational)

```cpp
friend bool operator==(const rational& a, const rational& b) noexcept;                     // (1)
friend std::strong_ordering operator<=>(const rational& a, const rational& b) noexcept;    // (2)
```

Compare two fractions by their values. `!=`, `<`, `<=`, `>` and `>=` are made from these by the compiler. The
operators are hidden friends, and a whole number or a `big_integer` on either side is converted, so `r < 1` and
`big_integer(6) > r` compare as fractions; a `double` is not converted by itself.

1. `true` when the two are the same fraction. Both are in lowest terms, so equality compares the parts.
2. The order of the two values: by the signs first, by the numerators when the denominators are equal, and
   otherwise `p/q` against `r/s` as `p·s` against `r·q` (the denominators are above zero).

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the fractions compared |

## Return value

- (1) Whether the fractions are equal.
- (2) `std::strong_ordering::less`, `equal` or `greater`.

## Complexity

- (1) A comparison of the numerators and of the denominators.
- (2) Constant when the signs differ; a comparison of the numerators when the denominators are equal; two products
  of the parts otherwise.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::rational third(1, 3);
    println("{} {} {}", third == math::rational(2, 6), third < 1, math::big_integer(6) > third);
    println("{} {}", math::rational(-1, 2) < third, math::rational(2, 3) >= math::rational(3, 5));
    println("{}", math::rational(0.1) == math::rational(1, 10));
}
```

Output:

```text
true true true
true true
false
```

## See also

- [operator+](operator_arith.md): the arithmetic
- [sgcl::math::rational](README.md)
