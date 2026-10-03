[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::mod

```cpp
big_integer mod(const big_integer& m) const;
```

The remainder of the division by `m` in `[0, |m|)`, whatever the signs: the one modular arithmetic wants, where
`%` takes the sign of the dividend as an `int`'s does. `math::big_integer(-7).mod(2)` is 1, and `-7 % 2` is -1.
It is Go's `Mod`, and Java's `mod`, which takes only a positive `m`.

## Parameters

| Parameter | Description |
|---|---|
| `m` | the modulus, of either sign but not zero |

## Return value

The `r` with `0 <= r < |m|` that differs from the number by a multiple of `m`.

## Complexity

That of a division ([operator%](operator_arith.md)).

## Exceptions

`domain_error` when `m` is zero.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_integer a = -7;
    println("{} {}", a % 2, a.mod(2));
    println("{} {}", a.mod(-2), math::big_integer(7).mod(-2));

    auto big = -(math::big_integer(1) << 100);
    println("{}", big.mod(1'000'000'007));
}
```

Output:

```text
-1 1
1 1
23628722
```

## See also

- [operator%](operator_arith.md): the remainder with the sign of the dividend
- [div_rem](div_rem.md): the quotient and the remainder of one division
- [mod_pow](mod_pow.md), [mod_inverse](mod_inverse.md): the modular power and inverse
- [sgcl::math::big_integer](../big_integer.md)
