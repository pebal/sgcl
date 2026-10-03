[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](../weak_map.md)

# sgcl::concurrent::weak_map\<Key, T\>::weak_map

```cpp
weak_map();                            // (1)
weak_map(const weak_map&) = delete;    // (2)
```

1. An empty map: the table of [concurrent::map](../map.md) with sixteen buckets, its head node and its counters, and
   the first sweep due after sixteen insertions.
2. The map is not copyable, and not movable: a structure shared by threads has one place.

## Parameters

None.

## Complexity

Constant: a few allocations.

## Exceptions

None.

## Notes

The map holds tracked pointers, so it lives on a thread's stack or inside a managed object; the map a whole program
shares goes into a managed object held by a [root_ptr](../../core/root_ptr.md).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Node {
    int id;
};

struct Registry {
    concurrent::weak_map<Node, string> names;  // a member of a managed object
};

int main() {
    concurrent::weak_map<Node, int> ranks;  // on the stack
    tracked_ptr registry = make_tracked<Registry>();

    println("{} {}", ranks.empty(), registry->names.empty());
    println("{}", std::is_copy_constructible_v<concurrent::weak_map<Node, int>>);
}
```

Output:

```text
true true
false
```

## See also

- [try_emplace](try_emplace.md), [insert](insert.md): add an entry
- [sgcl::concurrent::weak_map\<Key, T\>](../weak_map.md)
