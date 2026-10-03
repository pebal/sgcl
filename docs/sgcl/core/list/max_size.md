[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::max_size

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
    list<int> l;
    println("{}", l.max_size() == PTRDIFF_MAX);
}
```

Output:

```text
true
```

## See also

- [size](size.md): the number of elements
- [sgcl::list\<T\>](../list.md)
