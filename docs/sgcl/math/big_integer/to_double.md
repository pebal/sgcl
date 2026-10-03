[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::to_double

```cpp
double to_double() const noexcept;
```

The `double` nearest to the number, a tie going to the even one, as the conversion of an `int64_t` rounds: `2^53 + 1`
is `2^53`. A magnitude that rounds past the largest finite double is an infinity of the number's sign:
`2^1024 - 2^970` is infinity, and one less than that is the largest double.

## Parameters

None.

## Return value

The nearest `double`, or an infinity of the number's sign.

## Complexity

Constant but for a number whose limbs below the top two are zero: those are read until one is not, at most the
length of the number.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"
#include <limits>

using namespace sgcl;

int main() {
    math::big_integer two = 2;
    println("{}", (two.pow(53) + 1).to_double() == two.pow(53).to_double());
    math::big_integer edge = two.pow(1024) - two.pow(970);
    println("{} {}", edge.to_double(), (-edge).to_double());
    println("{}", (edge - 1).to_double() == std::numeric_limits<double>::max());
    println("{}", math::big_integer(10).pow(30).to_double());
}
```

Output:

```text
true
inf -inf
true
1e+30
```

## See also

- [to_int64](to_int64.md): exact, when it fits
- [big_integer](big_integer.md): the whole part of a `double`, the other way
- [sgcl::math::big_integer](../big_integer.md)
