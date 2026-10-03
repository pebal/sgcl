[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::operator[]

```cpp
T& operator[](size_type i) const noexcept;
```

The element at position `i`, without bounds checking: a debug build asserts `i < size()`. The reference is to the
element in the owner's memory, writable when `T` is not `const`, through a `const` slice as well: the constness of
the elements is `T`'s, not the slice's.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the position of the element |

## Return value

A reference to the element.

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
    vector v = {1, 2, 3};
    const slice<int> s = v;
    s[1] = 20;  // a write through the slice reaches the vector's buffer
    println("{} {}", s[1], v);
}
```

Output:

```text
20 [1, 20, 3]
```

## See also

- [front](front.md), [back](back.md): the first, the last element
- [data](data.md): the elements as a plain pointer
- [sgcl::slice\<T\>](README.md)
