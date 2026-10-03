[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](../sorted_multimap.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::max_size

```cpp
size_type max_size() const noexcept;
```

Returns the largest number of elements a multimap may hold in theory: the largest value of `difference_type`, so
that the distance between two iterators is always representable. The memory runs out long before.

## Parameters

None.

## Return value

The largest number of elements.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <limits>

using namespace sgcl;

int main() {
    sorted_multimap<int, int> m;
    println("{}", m.max_size() == std::numeric_limits<ptrdiff_t>::max());
}
```

Output:

```text
true
```

## See also

- [size](size.md): the number of elements
- [sgcl::sorted_multimap\<Key, T, Compare\>](../sorted_multimap.md)
