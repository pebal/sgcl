[sgcl](../../README.md) › [core](../README.md) › [set](README.md)

# sgcl::set\<Key, Hash, KeyEqual\>::bucket_count

```cpp
size_type bucket_count() const noexcept;
```

Returns the number of buckets: 0 or a power of two. A default-constructed set has none until its first
insertion, which gives it eight. The table grows when an insertion finds the size at
`bucket_count() * max_load_factor()`: to the smallest power of two that holds the elements at the maximum load
factor, at least twice the bucket count and at least eight. It never shrinks by itself: an erasure or a
[clear](clear.md) keeps the buckets, and only [rehash](rehash.md) gives them back.

## Parameters

None.

## Return value

The number of buckets.

## Complexity

Constant.

## Exceptions

None.

## Notes

The bucket array is a managed array of tracked pointers, each to the node before the bucket's first one; the
array a growth abandons is reclaimed by the collector.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<int> s;
    println("{}", s.bucket_count());

    s.insert(0);
    println("{}", s.bucket_count());

    for (int i : range(1, 9)) {
        s.insert(i);  // the ninth element finds eight in eight buckets
    }
    println("{}", s.bucket_count());

    s.clear();
    println("{} {}", s.size(), s.bucket_count());
}
```

Output:

```text
0
8
16
0 16
```

## See also

- [rehash](rehash.md), [reserve](reserve.md): set the number of buckets
- [load_factor](load_factor.md): the elements per bucket
- [sgcl::set\<Key, Hash, KeyEqual\>](README.md)
