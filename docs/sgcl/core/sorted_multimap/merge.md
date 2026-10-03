[sgcl](../../README.md) › [core](../README.md) › [sorted_multimap](README.md)

# sgcl::sorted_multimap\<Key, T, Compare\>::merge

```cpp
template<class C2> void merge(sorted_map<Key, T, C2>& source) noexcept;          // (1)
template<class C2> void merge(sorted_map<Key, T, C2>&& source) noexcept;         // (2)
template<class C2> void merge(sorted_multimap<Key, T, C2>& source) noexcept;     // (3)
template<class C2> void merge(sorted_multimap<Key, T, C2>&& source) noexcept;    // (4)
```

Relinks every node of `source` into this multimap, in `source`'s order, each after its equivalents: a multimap
takes them all, and `source` is empty after. No element is copied, moved or destroyed, and every iterator follows
its node: one to a relinked element points into this multimap now. `source` may be ordered by another comparator.
Merging a multimap into itself does nothing.

- (1–2) From a [sorted_map](../sorted_map/README.md) with the same `Key` and `T`.
- (3–4) From a sorted_multimap with the same `Key` and `T`.

A map of another `Key` or `T`, a hash [multimap](../multimap/README.md) or an [ordered_map](../ordered_map/README.md) is not
taken: the call does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the map whose nodes to relink |

## Return value

None.

## Complexity

*n* log(*size* + *n*) for the *n* elements of `source`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<int, int> a = {{1, 1}, {3, 3}};
    sorted_map<int, int> b = {{2, 2}, {3, 30}};
    a.merge(b);
    println("{} {}", a, b.empty());
}
```

Output:

```text
{1: 1, 2: 2, 3: 3, 3: 30} true
```

## See also

- [extract](extract.md): takes a node out of the multimap
- [insert](insert.md): links a node handle's node into a multimap
- [sgcl::sorted_multimap\<Key, T, Compare\>](README.md)
