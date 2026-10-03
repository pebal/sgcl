[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::reserve

```cpp
void reserve(size_type count) noexcept;
```

Makes room for `count` elements: `rehash(count / max_load_factor())`, rounded up, so that `count` elements are
inserted with no growth on the way. As [rehash](rehash.md), it may shrink the table, never below what the
elements there need, and it does not touch the order.

A `count` whose buckets the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused managed
allocation does ([collector](../collector.md#the-memory-limit)), as [rehash](rehash.md) of their number does.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of elements the table is to hold |

## Return value

None.

## Complexity

Linear in `size()` and in the new number of buckets.

## Exceptions

None.

## Notes

On a map with no bucket array, `reserve` makes the array and the sentinel: its [end()](end.md) from then on is
the one the insertions keep. A rehash invalidates the local iterators, which hold the bucket's number and the
mask of the old array; the other iterators stay valid.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<int, int> m;
    m.reserve(1000);
    auto buckets = m.bucket_count();
    for (int i : range(1000)) {
        m.emplace(i, i);
    }
    println("{} {} {}", buckets, m.bucket_count(), m.load_factor() <= m.max_load_factor());
}
```

Output:

```text
1024 1024 true
```

## See also

- [rehash](rehash.md): sets the number of buckets
- [bucket_count](bucket_count.md): the number of buckets
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
