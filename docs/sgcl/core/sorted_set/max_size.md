[sgcl](../../README.md) › [core](../README.md) › [sorted_set](../sorted_set.md)

# sgcl::sorted_set\<Key, Compare\>::max_size

```cpp
size_type max_size() const noexcept;
```

Returns the largest number of elements a set could hold: the largest value of `difference_type`, the bound of the
distance between two iterators. The managed heap runs out long before.

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
    sorted_set<int> numbers;
    println("{}", numbers.max_size() == size_t(std::numeric_limits<ptrdiff_t>::max()));
}
```

Output:

```text
true
```

## See also

- [size](size.md): the number of elements
- [sgcl::sorted_set\<Key, Compare\>](../sorted_set.md)
