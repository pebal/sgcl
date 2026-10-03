[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the array has no elements, `size() == 0`: default-constructed, made with no elements, or moved
from. The size is fixed at creation, so the answer changes only when the array is assigned over or moved from.

## Parameters

None.

## Return value

`true` when the array has no elements, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    dynamic_array<int> none;
    dynamic_array<int> zero(0);
    dynamic_array<int> some = {1, 2};
    println("{} {} {}", none.empty(), zero.empty(), some.empty());

    dynamic_array<int> taken = std::move(some);
    println("{} {}", some.empty(), taken.empty());
}
```

Output:

```text
true true false
true false
```

## See also

- [size](size.md): the number of elements
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
