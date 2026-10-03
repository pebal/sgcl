[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::merge

```cpp
/*(1)*/ template<class H2, class P2> void merge(multiset<Key, H2, P2>& source) noexcept;
/*(2)*/ template<class H2, class P2> void merge(multiset<Key, H2, P2>&& source) noexcept;
/*(3)*/ template<class H2, class P2> void merge(set<Key, H2, P2>& source) noexcept;
/*(4)*/ template<class H2, class P2> void merge(set<Key, H2, P2>&& source) noexcept;
```

Relinks every node of `source` into this multiset, each hashed with this multiset's hasher and linked in front
of the elements with its key, and leaves `source` empty. No element is copied, moved or destroyed: an iterator
follows its node into this multiset. Merging a multiset into itself does nothing.

- (1–2) `source` is a multiset of the same `Key`, with any hasher and equality.
- (3–4) `source` is a [set](../set.md) of the same `Key`, with any hasher and equality.

An [ordered_set](../ordered_set.md) is not taken, its nodes being of another kind, nor a container of another
`Key`.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the container whose nodes to relink |

## Return value

None.

## Complexity

Linear in `source.size()` on average: a hash and the walk of one bucket per node.

## Exceptions

None.

## Notes

The multiset grows as the insertions make it grow; `source` keeps its buckets. Only the containers above are
taken, the ones whose [node_type](../set-node_type.md) is the multiset's, as `std` asks: not a [map](../map.md),
even into a multiset of `pair<const K, T>`, whose elements have the map's type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multiset a = {1, 3};
    set b = {2, 3};
    auto it = b.find(2);

    a.merge(b);
    println("{} {} {}", a.size(), b.size(), it == a.find(2));
    println("{}", a.count(3));

    a.merge(multiset{4, 4});
    println("{}", a.size());
}
```

Output:

```text
4 0 true
2
6
```

## See also

- [insert](insert.md): links the node of a handle
- [extract](extract.md): takes one node out of a multiset
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)
