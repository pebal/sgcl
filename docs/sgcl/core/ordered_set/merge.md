[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::merge

```cpp
template<class H2, class P2> void merge(ordered_set<Key, H2, P2>& source) noexcept;     // (1)
template<class H2, class P2> void merge(ordered_set<Key, H2, P2>&& source) noexcept;    // (2)
```

Relinks the nodes of `source` whose elements are not yet here into this set, hashing them with this set's hash;
a node whose element is here already stays in `source`, in its place there. No element is copied or destroyed,
and every iterator follows its node. The nodes taken are appended to this set's order in `source`'s order, the
order of their insertions. Merging a set into itself does nothing.

`source` is an `ordered_set` with the same `Key` and any hash and equality. A [set](../set.md) or a
[multiset](../multiset.md) is not: their nodes are of another shape, without the two words of the order, and the
call does not compile.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the set to take the nodes from |

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
    ordered_set<string> a = {"x", "y"};
    ordered_set<string> b = {"y", "z", "w"};
    auto z = b.find("z");

    a.merge(b);
    println("{} {} {}", a, a.size(), b);
    println("{}", z == a.find("z"));

    println("{}", mergeable<ordered_set<string>, set<string>>);
}
```

Output:

```text
{"x", "y", "z", "w"} 4 {"y"}
true
false
```

## See also

- [extract](extract.md): takes one node out
- [insert](insert.md): links the node of a handle
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
