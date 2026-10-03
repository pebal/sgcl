[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::pow

```cpp
big_integer pow(int64_t exponent) const;
```

The number raised to `exponent`, by squaring: the bits of the exponent are read from the top, each a squaring and
each one bit a multiplication by the number besides. A power of two is a shift, and 0, 1 and -1 are answered at
once. `0^0` is 1, as in Go and Python. A negative exponent would not give a whole number and is an error; a result
past 2^52 bits (the longest number, 2^46 limbs) is refused before anything is computed.

## Parameters

| Parameter | Description |
|---|---|
| `exponent` | the power, zero or more |

## Return value

The number to the power `exponent`, negative when the number is negative and `exponent` odd; 1 when `exponent` is 0.

## Complexity

About log2(`exponent`) squarings of a growing number, the last of half the result's length: the time of a few
multiplications at the result's full length. A power of two is linear in the length of the result.

## Exceptions

- `domain_error` when `exponent` is negative.
- `length_error` when the result would be longer than 2^52 bits, before anything is computed.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_integer two = 2;
    println("{}", two.pow(100));
    println("{} {} {}", math::big_integer(-3).pow(5), math::big_integer(0).pow(0), two.pow(0));
    try {
        two.pow(-1);
    } catch (const domain_error& e) {
        println("{}", e.what());
    }
    try {
        math::big_integer(3).pow(INT64_MAX);  // refused at once
    } catch (const length_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
1267650600228229401496703205376
-243 1 1
sgcl::math::big_integer::pow: a negative exponent
sgcl::math::big_integer::pow: a result past the longest number
```

## See also

- [mod_pow](mod_pow.md): a power modulo a number
- [sqrt](sqrt.md): the whole part of the square root
- [operator\<\<](operator_arith.md): a power of two as a shift
- [sgcl::math::big_integer](README.md)
