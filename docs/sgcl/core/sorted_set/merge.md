[sgcl](../../README.md) › [core](../README.md) › [sorted_set](README.md)

# sgcl::sorted_set\<Key, Compare\>::merge

```cpp
template<class C2> void merge(sorted_set<Key, C2>& source) noexcept;          // (1)
template<class C2> void merge(sorted_set<Key, C2>&& source) noexcept;         // (2)
template<class C2> void merge(sorted_multiset<Key, C2>& source) noexcept;     // (3)
template<class C2> void merge(sorted_multiset<Key, C2>&& source) noexcept;    // (4)
```

Moves into this set the nodes of `source` whose keys are not here yet, in the order of `source`. A node is
relinked, not copied: no element is copied, moved or destroyed, and every iterator follows its node into this
set. A node whose key is already here stays in `source`, and of several equivalent keys of a sorted_multiset the
first is taken and the others stay. `source` may order its keys by another comparison; merging a set into
itself does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the set or the multiset to take the nodes from |

## Return value

None.

## Complexity

*N* log(*size* + *N*), *N* being the size of `source`.

## Exceptions

None.

## Notes

The header declares one member template over the tree under the containers; it takes the sets and the multisets
with the same `Key` (the same element type), and nothing else: a sorted_map, or a set of another key, does not
compile.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    sorted_set<int> a = {1, 3};
    sorted_multiset<int> b = {2, 3, 3};

    auto two = b.begin();
    a.merge(b);
    println("{} {}", a, b);
    println("{}", *two == 2 && a.find(2) == two);  // the node moved, the iterator with it

    sorted_set<int, std::greater<int>> c = {9, 4};
    a.merge(c);
    println("{} {}", a, c.empty());
}
```

Output:

```text
{1, 2, 3} {3, 3}
true
{1, 2, 3, 4, 9} true
```

## See also

- [extract](extract.md), [insert](insert.md): move one node
- [sgcl::sorted_set\<Key, Compare\>](README.md)
