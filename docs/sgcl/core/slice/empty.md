[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the slice has no elements. An empty slice may still have an owner: a piece of a string cut to
nothing holds the string as any piece does.

## Parameters

None.

## Return value

`true` when the slice is empty, `false` otherwise.

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
    string text = "   ";
    string_slice trimmed = text.as_slice().trim();
    println("{} {} {}", string_slice().empty(), trimmed.empty(), trimmed.owned());
}
```

Output:

```text
true true true
```

## See also

- [size](size.md): the number of elements
- [sgcl::slice\<T\>](README.md)
