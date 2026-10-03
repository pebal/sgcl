[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::front

```cpp
/*(1)*/ reference front() noexcept;
/*(2)*/ const_reference front() const noexcept;
```

Returns a reference to the first element, `(*this)[0]`. The array must not be empty.

## Parameters

None.

## Return value

A reference to the first element.

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
    dynamic_array<int> a(4, 1);
    a.front() = 0;
    println("{} {}", a.front(), a);
}
```

Output:

```text
0 [0, 1, 1, 1]
```

## See also

- [back](back.md): the last element
- [operator[]](operator_at.md): the element at a position
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
