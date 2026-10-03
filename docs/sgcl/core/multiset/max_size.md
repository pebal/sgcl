[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::max_size

```cpp
size_type max_size() const noexcept;
```

Returns the largest number of elements a multiset may hold: the largest value of `difference_type`, the
distance between two iterators.

## Parameters

None.

## Return value

The largest number of elements.

## Complexity

Constant.

## Exceptions

None.

## Notes

A bound of the types, not of the memory: a program whose managed heap reaches its limit ends long before
([collector](../collector.md#the-memory-limit)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <limits>

using namespace sgcl;

int main() {
    multiset<int> s;
    println("{}", s.max_size() == size_t(std::numeric_limits<ptrdiff_t>::max()));
}
```

Output:

```text
true
```

## See also

- [size](size.md): the number of elements
- [max_bucket_count](max_bucket_count.md): the largest number of buckets
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)
