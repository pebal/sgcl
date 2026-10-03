[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::swap

```cpp
void swap(slice& o) noexcept;
```

Swaps the elements and the owners of `*this` and `o`, through a copy and two assignments; the elements themselves
are not touched.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the slice to swap with |

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
    string text = "left right";
    string_slice a = text.as_slice(0, 4);
    string_slice b = text.as_slice(5);
    a.swap(b);
    println("{} {}", a, b);
}
```

Output:

```text
right left
```

## See also

- [operator=](operator_assign.md): assigns another slice
- [sgcl::slice\<T\>](../slice.md)
