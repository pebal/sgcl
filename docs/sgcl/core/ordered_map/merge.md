[sgcl](../../README.md) › [core](../README.md) › [ordered_map](../ordered_map.md)

# sgcl::ordered_map\<Key, T, Hash, KeyEqual\>::merge

```cpp
template<class H2, class P2> void merge(ordered_map<Key, T, H2, P2>& source) noexcept;     // (1)
template<class H2, class P2> void merge(ordered_map<Key, T, H2, P2>&& source) noexcept;    // (2)
```

Relinks the nodes of `source` whose keys are not yet here into this map, hashing their keys with this map's hash;
a node whose key is here already stays in `source`, in its place there. No element is copied or destroyed, and
every iterator follows its node. The nodes taken are appended to this map's order in `source`'s order, the order
of their insertions. Merging a map into itself does nothing.

`source` is an `ordered_map` with the same `Key` and `T` and any hash and equality. A [map](../map.md) or a
[multimap](../multimap.md) is not: their nodes are of another shape, without the two words of the order, and
the call does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the map to take the nodes from |

## Return value

None.

## Complexity

Linear in `source.size()` on average, linear in `source.size()` times `size() + source.size()` in the worst case.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

template<class A, class B>
concept mergeable = requires(A& a, B& b) { a.merge(b); };

int main() {
    ordered_map<string, int> a = {{"x", 1}, {"y", 2}};
    ordered_map<string, int> b = {{"y", 20}, {"z", 3}, {"w", 4}};
    auto z = b.find("z");

    a.merge(b);
    println("{} {} {} {}", a.size(), a.front().first, a.at("y"), b);
    println("{} {}", z == a.find("z"), z->second);

    println("{}", mergeable<ordered_map<string, int>, map<string, int>>);
}
```

Output:

```text
4 x 2 {"y": 20}
true 3
false
```

## See also

- [extract](extract.md): takes one node out
- [insert](insert.md): links the node of a handle
- [sgcl::ordered_map\<Key, T, Hash, KeyEqual\>](../ordered_map.md)
