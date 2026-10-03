[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::rehash

```cpp
void rehash(size_type count) noexcept;
```

Sets the number of buckets to the smallest power of two not below `count` and not below what the elements need,
`size() / max_load_factor()`; it may shrink the table. `rehash(0)` on an empty multimap that has no buckets
leaves it without them.

The nodes are relinked into a new bucket array in the order of the chain, so the runs of equal keys stay together
and in their order, hashing nothing (the hash of each key is cached in its node).

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

Linear in `size()` and the new `bucket_count()`.

## Exceptions

None.

## Notes

A rehash invalidates no iterator, reference or pointer to an element: the elements stay in their nodes, and only
the links change. It changes the order of iteration, and invalidates the local iterators.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<int, string> m = {{1, "one"}, {2, "two"}};
    auto one = m.find(1);

    m.rehash(100);
    println("{} {}", m.bucket_count(), one->second);  // the iterator is still valid

    m.rehash(1);  // shrinks, but not below what the elements need
    println("{}", m.bucket_count());

    multimap<int, int> empty;
    empty.rehash(0);
    println("{}", empty.bucket_count());
}
```

Output:

```text
128 one
2
0
```

## See also

- [reserve](reserve.md): the buckets for a number of elements
- [max_load_factor](max_load_factor.md): the load factor the table grows at
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
