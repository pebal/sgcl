[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::trailing_zeros

```cpp
size_t trailing_zeros() const noexcept;
```

How many zero bits are below the lowest one bit, as Go's `TrailingZeroBits`: the bits of the magnitude, which for
this question are those of the two's complement too, so `-40` has 3 as `40` has; 0 for 0. The number is a multiple
of 2 to that power, and shifted right by it, odd.

## Parameters

None.

## Return value

The count of zero bits below the lowest one bit; 0 for 0.

## Complexity

Linear in the number of limbs of zeros at the bottom; constant for a number within `int64_t`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::big_integer(40).trailing_zeros(),
            math::big_integer(-40).trailing_zeros(), math::big_integer(0).trailing_zeros());
    math::big_integer f = math::big_integer::factorial(100);
    size_t twos = f.trailing_zeros();
    println("{} {}", twos, (f >> twos).bit(0));  // odd after the shift
}
```

Output:

```text
3 3 0
97 true
```

## See also

- [bit_length](bit_length.md): the number of bits
- [bit](bit.md): one bit
- [sgcl::math::big_integer](../big_integer.md)
