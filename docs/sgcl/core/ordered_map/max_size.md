[sgcl](../../README.md) › [core](../README.md) › [ordered_map](README.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::max_size

```cpp
size_type max_size() const noexcept;
```

Returns the largest number of elements a map may hold: the largest value of `ptrdiff_t`. The bound is of the
counting; the memory runs out far below it.

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
    ordered_map<string, int> m;
    println("{}", m.max_size() == size_t(std::numeric_limits<ptrdiff_t>::max()));
    println("{}", m.max_bucket_count() == m.max_size());
}
```

Output:

```text
true
true
```

## See also

- [size](size.md): the number of elements
- [max_bucket_count](max_bucket_count.md): the largest number of buckets
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](README.md)
