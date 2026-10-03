[sgcl](../../README.md) › [core](../README.md) › [array](README.md)

# sgcl::array\<T, N\>::max_size

```cpp
constexpr size_type max_size() const noexcept;
```

Returns the largest number of elements the array may hold: `N`, as its size, since an array never grows.

## Parameters

None.

## Return value

`N`.

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
    array<int, 8> a = {};
    array<int, 0> none;
    println("{} {}", a.max_size(), a.max_size() == a.size());
    println("{}", none.max_size());
}
```

Output:

```text
8 true
0
```

## See also

- [size](size.md): the number of elements
- [sgcl::array\<T, N\>](README.md)
