[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::swap

```cpp
void swap(dynamic_array& other) noexcept;
```

Exchanges the contents of this array with those of `other`: the handles exchange their buffers and their counts,
and no element is moved, copied or destroyed. The arrays may be of different sizes.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the array to exchange the contents with |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

An iterator, a pointer, a reference or a slice of an element stays valid and keeps its element, which belongs to
the other array after the swap.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    dynamic_array<int> a = {1, 2};
    dynamic_array<int> b = {3, 4, 5};
    int* first = &a[0];

    a.swap(b);
    println("{} {} {}", a, b, &b[0] == first);
}
```

Output:

```text
[3, 4, 5] [1, 2] true
```

## See also

- [swap](swap2.md): the non-member form
- [operator=](operator_assign.md): assigns values to the array
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
