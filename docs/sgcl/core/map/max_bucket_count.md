[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::max_bucket_count

```cpp
size_type max_bucket_count() const noexcept;
```

Returns the largest number of buckets a map could have: [max_size()](max_size.md), the largest value of
`difference_type`.

## Parameters

None.

## Return value

The largest number of buckets.

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
    map<int, int> m;
    println("{}", m.max_bucket_count() == m.max_size());
}
```

Output:

```text
true
```

## See also

- [bucket_count](bucket_count.md): the number of buckets
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
