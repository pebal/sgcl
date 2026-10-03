[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::rehash

```cpp
void rehash(size_type count) noexcept;
```

Sets the number of buckets to the smallest power of two not below `count` and not below what the elements need,
`size() / max_load_factor()`: it may grow the table or shrink it. A rehash relinks the nodes of the chain into a
new bucket array, in the chain's order; it hashes nothing, the hash being cached in each node, and the order of
the elements is not touched. When both `count` and the size are 0, nothing happens.

A `count` whose buckets the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused managed
allocation does ([collector](../collector.md#the-memory-limit)): a count past the largest array an address space
holds is taken as that array, refused in the same way.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of buckets wanted |

## Return value

None.

## Complexity

Linear in `size()` and in the new number of buckets.

## Exceptions

None.

## Notes

No iterator is invalidated, [end()](end.md) included, but the local iterators: they hold the bucket's number
and the mask of the old array. The bucket array left behind is reclaimed by the collector. On a map with no
bucket array, `rehash` makes the array and the sentinel, as [reserve](reserve.md) does: an `end()` taken before
is not its `end()` from then on.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_map<int, int> m = {{5, 5}, {1, 1}, {3, 3}};
    auto it = m.find(1);

    m.rehash(100);
    println("{} {} {}", m.bucket_count(), it->second, m);

    m.rehash(0);  // as few as the elements need
    println("{} {}", m.bucket_count(), m);
}
```

Output:

```text
128 1 {5: 5, 1: 1, 3: 3}
4 {5: 5, 1: 1, 3: 3}
```

## See also

- [reserve](reserve.md): the buckets for a number of elements
- [max_load_factor](max_load_factor.md): the load factor at which the table grows
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
