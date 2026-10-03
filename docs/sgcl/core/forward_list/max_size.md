[sgcl](../../README.md) › [core](../README.md) › [forward_list](README.md)

# sgcl::forward_list\<T\>::max_size

```cpp
size_type max_size() const noexcept;
```

Returns the largest number of elements a list may hold: `PTRDIFF_MAX`, the largest distance between two
iterators.

## Parameters

None.

## Return value

`PTRDIFF_MAX`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <cstdint>

using namespace sgcl;

int main() {
    forward_list<int> l;
    println("{}", l.max_size() == PTRDIFF_MAX);
}
```

Output:

```text
true
```

## See also

- [empty](empty.md): checks whether the list is empty
- [sgcl::forward_list\<T\>](README.md)
