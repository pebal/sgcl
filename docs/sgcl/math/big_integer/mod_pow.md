[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::mod_pow

```cpp
big_integer mod_pow(const big_integer& exponent, const big_integer& m) const;
```

The number raised to `exponent` modulo `m`, in `[0, m)`, as Python's `pow(a, e, m)`; a negative number is taken
modulo `m` first. For an odd `m` the products are Montgomery's, which need no division, over sliding windows of the
exponent's bits; for an even `m` each bit costs a square and a remainder, and a one bit a product and a remainder
more. Modulo 1 everything is 0, and an exponent of 0 gives 1 for any other modulus.

A negative exponent and a modulus of zero or below are errors. Go's `Exp` takes a negative exponent as an inverse;
here that is said as such: [mod_inverse](mod_inverse.md), then `mod_pow` of the inverse.

## Parameters

| Parameter | Description |
|---|---|
| `exponent` | the power, zero or more |
| `m` | the modulus, above zero |

## Return value

The power modulo `m`, in `[0, m)`.

## Complexity

For an odd `m`, a Montgomery product at the length of `m` per bit of the exponent and a fraction (the products of
the windows); for an even `m`, a multiplication and a division at the length of `m` per bit, two of each for a one
bit. A long number from outside is a long computation, as in any library.

## Exceptions

`domain_error` when `m` is zero or below, or when `exponent` is negative.

## Notes

Nothing here takes the same time whatever the value: the exponent and the modulus of a private key belong to
`crypto`'s own types.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", math::big_integer(4).mod_pow(13, 497), math::big_integer(-3).mod_pow(3, 7));
    math::big_integer p = math::big_integer(2).pow(127) - 1;  // a prime
    println("{}", math::big_integer(3).mod_pow(p - 1, p));  // Fermat: 1 for a prime
    // 3^-2 mod 11: the inverse first, then its power
    math::big_integer inverse = *math::big_integer(3).mod_inverse(11);
    println("{}", inverse.mod_pow(2, 11));
}
```

Output:

```text
445 1
1
5
```

## See also

- [mod_inverse](mod_inverse.md): the inverse modulo a number
- [pow](pow.md): a power without a modulus
- [mod](mod.md): the remainder in `[0, |m|)`
- [benchmarks](../benchmarks.md#big_integer): the time against Go's `math/big`
- [sgcl::math::big_integer](README.md)
