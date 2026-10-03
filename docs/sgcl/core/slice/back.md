[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::back

```cpp
T& back() const noexcept;
```

The last element. Precondition: the slice is not empty; a debug build asserts it.

## Parameters

None.

## Return value

A reference to the last element.

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
    slice<int> s = v;
    s.back() = 30;
    println("{}", v);
}
```

Output:

```text
[1, 2, 30]
```

## See also

- [front](front.md): the first element
- [sgcl::slice\<T\>](../slice.md)
