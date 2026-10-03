[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](../weak_map.md)

# sgcl::concurrent::weak_map\<Key, T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the map holds an entry: a walk from the head of the table's list to its first entry. An entry whose
object is gone and that no sweep has dropped yet is an entry: `empty` answers for the table, as [size](size.md)
counts it, not for the live objects.

## Parameters

None.

## Return value

`true` when the walk found no entry, `false` otherwise.

## Complexity

Constant when an entry stands near the head of the list; at worst linear in the number of buckets in use, whose
dummy nodes the walk passes.

## Exceptions

None.

## Notes

Under concurrent insertions and erasures the answer is of the moment of the walk and may be stale when it returns.
It is exact after a [sweep](sweep.md) with the threads quiet. Whether the map has a live entry is what
[begin](begin.md) answers: `begin() == end()`.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    concurrent::weak_map<Node, int> ranks;
    println("{}", ranks.empty());

    ranks.try_emplace(make_tracked<Node>(1), 10);  // nobody else holds the node
    collector::force_collect(true);  // optional, for the demonstration: the entry is found dead
    println("{} {}", ranks.empty(), ranks.begin() == ranks.end());

    ranks.sweep();
    println("{}", ranks.empty());
}
```

Output:

```text
true
false true
true
```

## See also

- [size](size.md): the number of entries
- [sweep](sweep.md): erases the dead entries
- [sgcl::concurrent::weak_map\<Key, T\>](../weak_map.md)
