[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::swap (sgcl::list)

```cpp
#include "sgcl/core/list.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T> void swap(list<T>& lhs, list<T>& rhs) noexcept;
}
```

Exchanges the contents of `lhs` and `rhs`, as `lhs.swap(rhs)`: the sentinels and the counts change places, and no
element or node is touched. An unqualified `swap(a, b)` and `std::ranges::swap` find it through the argument's
type.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the lists to exchange the contents of |

## Return value

None.

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
    list a = {1, 2, 3};
    list b = {4, 5};
    swap(a, b);
    println("{} {}", a, b);

    std::ranges::swap(a, b);
    println("{} {}", a, b);
}
```

Output:

```text
[4, 5] [1, 2, 3]
[1, 2, 3] [4, 5]
```

## See also

- [swap](swap.md): the member function
- [sgcl::list\<T\>](README.md)
