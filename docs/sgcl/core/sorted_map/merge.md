[sgcl](../../README.md) › [core](../README.md) › [sorted_map](../sorted_map.md)

# sgcl::sorted_map\<Key, T, Compare\>::merge

```cpp
/*(1)*/ template<class C2> void merge(sorted_map<Key, T, C2>& source) noexcept;
/*(2)*/ template<class C2> void merge(sorted_map<Key, T, C2>&& source) noexcept;
/*(3)*/ template<class C2> void merge(sorted_multimap<Key, T, C2>& source) noexcept;
/*(4)*/ template<class C2> void merge(sorted_multimap<Key, T, C2>&& source) noexcept;
```

Relinks into this map the nodes of `source` whose keys it does not hold yet, in `source`'s order; a node whose key
is already here stays in `source`. No element is copied, moved or destroyed, and every iterator follows its node:
one to a relinked element points into this map now. `source` may be ordered by another comparator. Merging a map
into itself does nothing.

- (1–2) From a [sorted_map](../sorted_map.md) with the same `Key` and `T`.
- (3–4) From a [sorted_multimap](../sorted_multimap.md) with the same `Key` and `T`: of its equal keys the first
  comes over, the others stay.

A map of another `Key` or `T`, a hash [map](../map.md) or an [ordered_map](../ordered_map.md) is not taken: the
call does not compile.

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
#include <functional>

using namespace sgcl;

int main() {
    sorted_map<int, int> a = {{1, 1}, {3, 3}};
    sorted_multimap<int, int> b = {{2, 2}, {3, 30}, {3, 31}};
    a.merge(b);
    println("{} {}", a, b);  // b keeps both 3s

    sorted_map<int, int, std::greater<int>> c = {{5, 5}, {4, 4}};
    auto four = c.find(4);
    a.merge(c);
    println("{} {} {}", a, c.empty(), four == a.find(4));
}
```

Output:

```text
{1: 1, 2: 2, 3: 3} {3: 30, 3: 31}
{1: 1, 2: 2, 3: 3, 4: 4, 5: 5} true true
```

## See also

- [extract](extract.md): takes a node out of the map
- [insert](insert.md): links a node handle's node into a map
- [sgcl::sorted_map\<Key, T, Compare\>](../sorted_map.md)
