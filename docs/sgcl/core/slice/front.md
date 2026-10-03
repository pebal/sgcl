[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::front

```cpp
T& front() const noexcept;
```

The first element. Precondition: the slice is not empty; a debug build asserts it.

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
    string text = "hello";
    string_slice s = text;
    println("{} {}", s.front(), s.back());
}
```

Output:

```text
h o
```

## See also

- [back](back.md): the last element
- [sgcl::slice\<T\>](README.md)
