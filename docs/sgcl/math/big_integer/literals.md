[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::literals::operator""_big

```cpp
template<char... C>
big_integer operator""_big() noexcept;
```

A constant of any length, written as the language writes an integer: decimal, hexadecimal after `0x` or `0X`,
binary after `0b` or `0B`, octal after a leading `0`, and `'` between digits anywhere. The digits are those the
compiler has checked as an integer literal before the operator sees them, and the value is computed by the compiler
too; what is left for the run is the allocation of a value past `int64_t`. The literal has no sign: `-5_big` is the
minus of `5_big`. `using namespace math::literals;` brings it in. Go has no literal of a big number: its `SetString`
reads the text when the program runs.

A literal that is not an integer does not compile. A digit outside the base of an integer literal (`09_big`,
`0b102_big`) is the compiler's own error. A floating literal (`1.5_big`, `1e5_big`, `0x1p3_big`) reaches the
operator, whose `static_assert` stops it: "not a whole number: _big takes the digits of an integer literal". There
is no `0o` for octal in C++: `0o17_big` does not compile either.

## Parameters

None.

## Return value

The number the literal writes.

## Complexity

Computed by the compiler. At run time, constant for a value within `int64_t`, which allocates nothing; linear in
the length otherwise, a copy of the limbs into a new number.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;
using namespace math::literals;

int main() {
    auto n = 0xffff'ffff'ffff'ffff'ffff'ffff'ffff'ffff_big;
    auto [q, r] = n.div_rem(1'000'000'007);
    println("{} {}", q, r);
    println("{} {} {}", 123456789012345678901234567890_big, 0b1010_big, 017_big);
}
```

Output:

```text
340282364538961911690641225597 279632276
123456789012345678901234567890 10 15
```

## See also

- [(constructor)](big_integer.md): a number from a text the program writes
- [parse](parse.md): a number from text read from outside
- [sgcl::math::big_integer](../big_integer.md)
