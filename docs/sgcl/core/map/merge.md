[sgcl](../../README.md) › [core](../README.md) › [map](README.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::merge

```cpp
template<class H2, class P2> void merge(map<Key, T, H2, P2>& source) noexcept;          // (1)
template<class H2, class P2> void merge(map<Key, T, H2, P2>&& source) noexcept;         // (2)
template<class H2, class P2> void merge(multimap<Key, T, H2, P2>& source) noexcept;     // (3)
template<class H2, class P2> void merge(multimap<Key, T, H2, P2>&& source) noexcept;    // (4)
```

Moves the nodes of `source` whose keys are not in this map yet into it, rehashing each with this map's hasher; a
node whose key is here already stays in `source`. No element is copied, moved or destroyed: the nodes are
relinked, and every iterator follows its node into this map.

`source` may be a [map](README.md) or a [multimap](../multimap/README.md) with the same `Key` and `T`, and any hasher and
equality. Not an [ordered_map](../ordered_map/README.md), whose nodes are of another shape: that call does not compile.
Merging a map into itself does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the map or multimap whose nodes to take |

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
    map<int, string> a = {{1, "a"}, {3, "c"}};
    multimap<int, string> b = {{2, "b"}, {3, "x"}, {3, "y"}};
    auto two = b.find(2);

    a.merge(b);  // 2 moves over; both 3s stay in b
    println("{} {} {}", a.size(), b.size(), a.at(3));
    println("{} {}", two == a.find(2), b.count(3));

    a.merge(map<int, string>{{4, "d"}});
    println("{}", a.size());
}
```

Output:

```text
3 2 c
true 2
4
```

## See also

- [extract](extract.md): unlinks one element into a node handle
- [insert](insert.md): inserts a node handle's node
- [sgcl::map\<Key, T, Hash, KeyEqual\>](README.md)
