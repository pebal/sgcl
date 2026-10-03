[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::bit

```cpp
bool bit(size_t index) const noexcept;
```

Bit `index` of the two's complement stretching without end to the left, as in Go and Python: Go's `Bit`,
`(a >> index) & 1`. Of a negative number every bit past its length is one: `-1` is all ones, and bit 1000 of `-6`
is one. The bitwise operators work on the same bits.

## Parameters

| Parameter | Description |
|---|---|
| `index` | the position of the bit, 0 for the lowest |

## Return value

The bit, `true` for a one.

## Complexity

Constant; for a negative number past `int64_t`, linear in the number of limbs of zeros at the bottom.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::big_integer a = -6;  // ...11010
    println("{} {} {} {} {}", a.bit(0), a.bit(1), a.bit(2), a.bit(3), a.bit(1000));
    println("{}", a & 0xff);
    math::big_integer big = math::big_integer(2).pow(200);
    println("{} {} {}", big.bit(200), (-big).bit(200), (-big).bit(199));
}
```

Output:

```text
false true false true true
250
true true false
```

## See also

- [operator&, operator|, operator^, operator~](operator_arith.md): the bitwise operators
- [bit_length](bit_length.md), [trailing_zeros](trailing_zeros.md): counts of bits
- [sgcl::math::big_integer](README.md)
