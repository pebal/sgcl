[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::max_bucket_count

```cpp
size_type max_bucket_count() const noexcept;
```

Returns the largest number of buckets a map may have: [max_size()](max_size.md), the largest value of
`ptrdiff_t`.

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
    ordered_map<int, int> m;
    println("{}", m.max_bucket_count() == m.max_size());
}
```

Output:

```text
true
```

## See also

- [bucket_count](bucket_count.md): the number of buckets
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
