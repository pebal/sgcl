[sgcl](../../README.md) › [math](../README.md) › [big_integer](../big_integer.md)

# sgcl::math::big_integer::lcm

```cpp
big_integer lcm(const big_integer& other) const noexcept;
```

The least common multiple of the number and `other`, never negative, whatever the signs; 0 when either is 0, as
Python's `math.lcm`. It is `|a| / gcd(a, other) * |other|`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the other number |

## Return value

The least common multiple, zero or more.

## Complexity

That of [gcd](gcd.md), and a division and a multiplication.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    println("{} {}", math::big_integer(-4).lcm(6), math::big_integer(0).lcm(5));
    math::big_integer all = 1;
    for (int i : range(1, 31)) {
        all = all.lcm(i);
    }
    println("{}", all);  // the smallest number that 1 to 30 divide
}
```

Output:

```text
12 0
2329089562800
```

## See also

- [gcd](gcd.md): the greatest common divisor
- [sgcl::math::big_integer](../big_integer.md)
