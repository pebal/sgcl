[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::end, cend

```cpp
iterator end() const noexcept;           // (1)
const_iterator cend() const noexcept;    // (2)
```

An iterator past the last element: a plain pointer, `T*` (1) or `const T*` (2).

## Parameters

None.

## Return value

An iterator to the end.

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
    string text = "a,b";
    string_slice s = text;
    println("{} {}", s.end() - s.begin(), *(s.cend() - 1));
}
```

Output:

```text
3 b
```

## See also

- [begin, cbegin](begin.md): an iterator to the beginning
- [sgcl::slice\<T\>](../slice.md)
