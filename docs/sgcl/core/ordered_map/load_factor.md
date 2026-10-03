[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::load_factor

```cpp
float load_factor() const noexcept;
```

Returns the average number of elements per bucket, `size() / bucket_count()`; 0 for a map with no bucket array.

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
    ordered_map<int, int> m;
    println("{}", m.load_factor());

    m.reserve(4);
    for (int i : range(3)) {
        m[i] = i;
    }
    println("{} {} {}", m.size(), m.bucket_count(), m.load_factor());
}
```

Output:

```text
0
3 4 0.75
```

## See also

- [max_load_factor](max_load_factor.md): the load factor at which the table grows
- [bucket_count](bucket_count.md): the number of buckets
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
