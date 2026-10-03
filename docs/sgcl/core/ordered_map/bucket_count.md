[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::bucket_count

```cpp
size_type bucket_count() const noexcept;
```

Returns the number of buckets: 0 for a map that has no bucket array yet, a power of two otherwise. The table
grows when the size reaches `bucket_count() * max_load_factor()`, at least doubling the count, to eight buckets
at the least.

## Parameters

None.

## Return value

The number of buckets.

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
    println("{}", m.bucket_count());

    m[1] = 1;
    println("{}", m.bucket_count());

    for (int i : range(2, 10)) {
        m[i] = i;
    }
    println("{} {}", m.size(), m.bucket_count());

    ordered_map<int, int> sized(100);
    println("{}", sized.bucket_count());
}
```

Output:

```text
0
8
9 16
128
```

## See also

- [rehash](rehash.md), [reserve](reserve.md): set the number of buckets
- [load_factor](load_factor.md): the elements per bucket
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
