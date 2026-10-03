[sgcl](../../README.md) › [core](../README.md) › [set](README.md)

# sgcl::set\<Key, Hash, KeyEqual\>::rehash

```cpp
void rehash(size_type count) noexcept;
```

Sets the number of buckets to the smallest power of two that is not below `count` and not below
`size() / max_load_factor()`, and relinks the nodes into the new bucket array. The table may shrink as well as
grow. When that number is 0 (an empty set, `count` 0) or the bucket count already, nothing happens.

A `count` whose buckets the managed heap cannot give, up to `SIZE_MAX`, ends the program as any refused managed
allocation does ([collector](../collector/README.md#the-memory-limit)): a count past the largest array an address space
holds is taken as that array, refused in the same way.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of buckets wanted |

## Return value

None.

## Complexity

Linear in `size()` and the new bucket count.

## Exceptions

None.

## Notes

A rehash relinks the nodes and hashes nothing, each node's hash being cached; it moves no element and
invalidates no iterator, only the local ones. The old bucket array is left to the collector.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<int> s;
    s.insert({1, 2, 3});
    auto it = s.find(2);
    println("{}", s.bucket_count());

    s.rehash(100);
    println("{} {}", s.bucket_count(), *it);

    s.rehash(0);  // as few as the three elements need
    println("{}", s.bucket_count());
}
```

Output:

```text
8
128 2
4
```

## See also

- [reserve](reserve.md): the buckets for a number of elements
- [bucket_count](bucket_count.md): the number of buckets
- [max_load_factor](max_load_factor.md): the load factor at which the table grows
- [sgcl::set\<Key, Hash, KeyEqual\>](README.md)
