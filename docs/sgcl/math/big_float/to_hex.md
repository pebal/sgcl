[sgcl](../../README.md) › [math](../README.md) › [big_float](README.md)

# sgcl::math::big_float::to_hex

```cpp
string to_hex() const;
```

The exact value in hexadecimal, as C's `%a` writes a double: `0x`, the first bit of the mantissa before the point,
the rest in hexadecimal digits after it (none when there is no rest), `p` and the binary exponent with its sign —
`0x1.8p+3` for 12, `0x1.999999999999ap-4` for the double 0.1, `-0x1p-1074`, `0x0p+0`; `+Inf` and `-Inf`. Nothing is
rounded: [parse](parse.md) at the value's precision reads it back to the same value. `{:.5a}` of
[txt::format](../../txt/format.md) rounds to that many hexadecimal digits.

## Parameters

None.

## Return value

The text.

## Complexity

The conversion of the mantissa to hexadecimal, linear.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {} {}", math::big_float(12).to_hex(), math::big_float(0.1).to_hex(), math::big_float(-0.0).to_hex());
    math::big_float x(math::rational(1, 3), 20);
    println("{} {}", x.to_hex(), math::big_float::parse(x.to_hex(), 20).value() == x);
}
```

Output:

```text
0x1.8p+3 0x1.999999999999ap-4 -0x0p+0
0x1.55556p-2 true
```

## See also

- [to_string](to_string.md): the shortest decimal
- [parse](parse.md): reads it back
- [sgcl::math::big_float](README.md)
