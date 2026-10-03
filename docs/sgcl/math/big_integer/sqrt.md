[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::sqrt

```cpp
big_integer sqrt() const;
```

The whole part of the square root: the largest `s` with `s * s <= a`, as Go's `Sqrt` and Python's `math.isqrt`. A
number below 2^126 is answered on the processor's own numbers. Above, Newton's method starts from the square root
of the top half of the bits, shifted up, so that one step — one division at full length — brings it within a few
of the root; the few steps down are counted on the remainder `a - s * s`, not on new squares.

## Parameters

None.

## Return value

The largest `s` with `s * s <= a`; 0 for 0.

## Complexity

One division and one multiplication at the full length of the number, and the same at half the length, a quarter,
and so on down: the time of a few multiplications and divisions at full length.

## Exceptions

`domain_error` when the number is negative.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::big_integer(99).sqrt(), math::big_integer(100).sqrt(),
            math::big_integer(0).sqrt());
    math::big_integer ten = 10;
    println("{}", ten.pow(100).sqrt() == ten.pow(50));
    println("{}", (2 * ten.pow(40)).sqrt());  // the first 21 digits of √2
    try {
        math::big_integer(-1).sqrt();
    } catch (const domain_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
9 10 0
true
141421356237309504880
sgcl::math::big_integer::sqrt: the square root of a negative number
```

## See also

- [pow](pow.md): a power
- [sgcl::math::big_integer](README.md)
