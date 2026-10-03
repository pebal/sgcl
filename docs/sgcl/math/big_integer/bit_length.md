[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::bit_length

```cpp
size_t bit_length() const noexcept;
```

The number of bits of the magnitude, as Go's `BitLen` and Python's `int.bit_length`: 0 for 0, 8 for 255 and for
-255. A count of bits is what a shift takes, so `a << a.bit_length()` needs no cast.

## Parameters

None.

## Return value

The position of the highest one bit of `|a|`, counted from 1; 0 for 0.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_integer a = 255;
    println("{} {} {} {}", math::big_integer(0).bit_length(), a.bit_length(), (-a).bit_length(),
            (a + 1).bit_length());
    math::big_integer big = math::big_integer(2).pow(100);
    println("{} {}", big.bit_length(), (big.bit_length() + 7) / 8 == big.to_bytes().size());
    println("{}", a << a.bit_length());
}
```

Output:

```text
0 8 8 9
101 true
65280
```

## See also

- [trailing_zeros](trailing_zeros.md): the zero bits below the lowest one
- [bit](bit.md): one bit
- [operator\<\<](operator_arith.md): a shift by a count of bits
- [sgcl::math::big_integer](../big_integer.md)
