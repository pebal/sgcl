[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::max_size

```cpp
size_type max_size() const noexcept;
```

Returns the largest number of elements a set may hold: the largest value of `ptrdiff_t`. The bound is of the
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
    ordered_set<int> s;
    println("{}", s.max_size() == size_t(std::numeric_limits<ptrdiff_t>::max()));
    println("{}", s.max_bucket_count() == s.max_size());
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
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
