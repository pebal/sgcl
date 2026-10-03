[sgcl](../../README.md) › [core](../README.md) › [sorted_multiset](../sorted_multiset.md)

# sgcl::sorted_multiset\<Key, Compare\>::merge

```cpp
template<class C2> void merge(sorted_multiset<Key, C2>& source) noexcept;     // (1)
template<class C2> void merge(sorted_multiset<Key, C2>&& source) noexcept;    // (2)
template<class C2> void merge(sorted_set<Key, C2>& source) noexcept;          // (3)
template<class C2> void merge(sorted_set<Key, C2>&& source) noexcept;         // (4)
```

Moves every node of `source` into this multiset, in the order of `source`, each after the elements here with an
equivalent key, and leaves `source` empty. A node is relinked, not copied: no element is copied, moved or
destroyed, and every iterator follows its node into this multiset. `source` may order its keys by another
comparison; merging a multiset into itself does nothing.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the multiset or the set to take the nodes from |

## Return value

None.

## Complexity

*N* log(*size* + *N*), *N* being the size of `source`.

## Exceptions

None.

## Notes

The header declares one member template over the tree under the containers; it takes the multisets and the sets
with the same `Key` (the same element type), and nothing else: a sorted_multimap, or a multiset of another key,
does not compile.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    sorted_multiset<int> a = {1, 3};
    sorted_set<int, std::greater<int>> b = {3, 2};

    auto two = b.find(2);
    a.merge(b);
    println("{} {}", a, b.empty());
    println("{}", a.find(2) == two);  // the node moved, the iterator with it
}
```

Output:

```text
{1, 2, 3, 3} true
true
```

## See also

- [extract](extract.md), [insert](insert.md): move one node
- [sgcl::sorted_multiset\<Key, Compare\>](../sorted_multiset.md)
