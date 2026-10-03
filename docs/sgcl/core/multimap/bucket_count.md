[sgcl](../../README.md) › [core](../README.md) › [multimap](README.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::bucket_count

```cpp
size_type bucket_count() const noexcept;
```

Returns the number of buckets: 0 for a multimap that has never held an element and was given no bucket count, a
power of two otherwise. The first insertion makes eight at least; the table doubles at least each time the size
reaches `bucket_count() * max_load_factor()`.

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
    multimap<int, int> m;
    println("{}", m.bucket_count());

    m.emplace(1, 1);
    println("{}", m.bucket_count());

    for (int i : range(100)) {
        m.emplace(i, i);
    }
    println("{} {}", m.size(), m.bucket_count());

    m.clear();
    println("{}", m.bucket_count());
}
```

Output:

```text
0
8
101 128
128
```

## See also

- [bucket_size](bucket_size.md): the number of elements in a bucket
- [rehash](rehash.md): sets the number of buckets
- [load_factor](load_factor.md): the average number of elements per bucket
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](README.md)
