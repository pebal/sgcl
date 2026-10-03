[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::merge

```cpp
/*(1)*/ template<class H2, class P2> void merge(multimap<Key, T, H2, P2>& source) noexcept;
/*(2)*/ template<class H2, class P2> void merge(multimap<Key, T, H2, P2>&& source) noexcept;
/*(3)*/ template<class H2, class P2> void merge(map<Key, T, H2, P2>& source) noexcept;
/*(4)*/ template<class H2, class P2> void merge(map<Key, T, H2, P2>&& source) noexcept;
```

Moves every node of `source` into this multimap, rehashing each with this multimap's hasher, and leaves `source`
empty; a node whose key is here already goes in front of the elements with that key. No element is copied, moved
or destroyed: the nodes are relinked, and every iterator follows its node into this multimap.

`source` may be a [multimap](../multimap.md) or a [map](../map.md) with the same `Key` and `T`, and any hasher and
equality. Not an [ordered_map](../ordered_map.md), whose nodes are of another shape: that call does not compile.
Merging a multimap into itself does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the multimap or map whose nodes to take |

## Return value

None.

## Complexity

Linear in `source.size()` on average: a lookup per node; a growth relinks every node, amortized constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<int, string> a = {{1, "a"}, {3, "c"}};
    map<int, string> b = {{2, "b"}, {3, "x"}};
    auto x = b.find(3);

    a.merge(b);  // every node moves over, the 3 of b too
    println("{} {} {}", a.size(), b.size(), a.count(3));
    println("{}", x == a.find(3));
}
```

Output:

```text
4 0 2
true
```

## See also

- [extract](extract.md): unlinks one element into a node handle
- [insert](insert.md): inserts a node handle's node
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
