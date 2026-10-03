[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::merge

```cpp
/*(1)*/ template<class H2, class P2> void merge(set<Key, H2, P2>& source) noexcept;
/*(2)*/ template<class H2, class P2> void merge(set<Key, H2, P2>&& source) noexcept;
/*(3)*/ template<class H2, class P2> void merge(multiset<Key, H2, P2>& source) noexcept;
/*(4)*/ template<class H2, class P2> void merge(multiset<Key, H2, P2>&& source) noexcept;
```

Relinks into this set the nodes of `source` whose keys are not here yet, each hashed with this set's hasher; a
node whose key is here already stays in `source`. No element is copied, moved or destroyed: an iterator follows
its node into this set. Merging a set into itself does nothing.

- (1–2) `source` is a set of the same `Key`, with any hasher and equality.
- (3–4) `source` is a [multiset](../multiset.md) of the same `Key`: of a run of equal keys one node moves, the
  others stay.

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

The set grows as the insertions make it grow. Only the containers above are taken, the ones whose
[node_type](../set-node_type.md) is the set's, as `std` asks: not a [map](../map.md), even into a set of
`pair<const K, T>`, whose elements have the map's type.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set a = {1, 3};
    multiset b = {2, 3, 3};
    auto it = b.find(2);

    a.merge(b);
    println("{} {} {}", a.size(), b.size(), it == a.find(2));
    println("{} {}", b.count(3), b.contains(2));

    a.merge(set{4, 5});
    println("{}", a.size());
}
```

Output:

```text
3 2 true
2 false
5
```

## See also

- [insert](insert.md): links the node of a handle
- [extract](extract.md): takes one node out of a set
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
