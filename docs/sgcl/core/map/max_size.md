[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::max_size

```cpp
size_type max_size() const noexcept;
```

Returns the largest number of elements a map could hold: the largest value of `difference_type`, the bound of the
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
    map<int, int> m;
    println("{}", m.max_size() == size_t(std::numeric_limits<ptrdiff_t>::max()));
}
```

Output:

```text
true
```

## See also

- [size](size.md): the number of elements
- [max_bucket_count](max_bucket_count.md): the largest number of buckets
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
