[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::load_factor

```cpp
float load_factor() const noexcept;
```

Returns the average number of elements per bucket, `size() / bucket_count()`; 0 when the map has no buckets.

## Parameters

None.

## Return value

The load factor.

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
    println("{}", m.load_factor());

    for (int i : range(12)) {
        m.emplace(i, i);
    }
    println("{} {} {}", m.size(), m.bucket_count(), m.load_factor());
    println("{}", m.load_factor() <= m.max_load_factor());
}
```

Output:

```text
0
12 16 0.75
true
```

## See also

- [max_load_factor](max_load_factor.md): the load factor the table grows at
- [bucket_count](bucket_count.md): the number of buckets
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
