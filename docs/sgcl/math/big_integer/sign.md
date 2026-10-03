[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::sign

```cpp
int sign() const noexcept;
```

The sign of the number, as Go's `Sign`.

## Parameters

None.

## Return value

-1 for a negative number, 0 for zero, 1 for a positive number.

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
    math::big_integer debt = -5;
    math::big_integer none;
    auto huge = math::big_integer(1) << 100;
    println("{} {} {} {}", debt.sign(), none.sign(), huge.sign(), (-huge).sign());
}
```

Output:

```text
-1 0 1 -1
```

## See also

- [abs](abs.md): the number without its sign
- [operator==, operator\<=\>](operator_cmp.md): the comparisons
- [sgcl::math::big_integer](README.md)
