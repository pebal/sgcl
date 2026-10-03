[sgcl](../../README.md) › [core](../README.md) › [weak_map](README.md)

# sgcl::weak_map\<Key, T\>::weak_map

```cpp
weak_map() noexcept;                    // (1)
weak_map(weak_map&& other) noexcept;    // (2)
weak_map(const weak_map&) = delete;     // (3)
```

Constructs a map.

1. An empty map. Nothing is allocated: the table gets its buckets at the first insertion, and the first sweep is
   due after sixteen insertions.
2. Takes the table of `other` over, no entry copied or moved; `other` is empty after.
3. The map is not copyable: a copy would copy the values and share the keys, and which of the two a program means
   is its own to say.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the map whose entries are taken over |

## Complexity

Constant.

## Exceptions

None.

## Notes

The map holds tracked pointers, so it lives on a stack or inside a managed object
([The rules](../README.md#the-rules), 1).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Node {
    int id;
};

struct Registry {
    weak_map<Node, string> names;  // a member of a managed object
};

int main() {
    weak_map<Node, int> ranks;  // on the stack
    tracked_ptr registry = make_tracked<Registry>();
    println("{} {}", ranks.empty(), registry->names.empty());

    tracked_ptr node = make_tracked<Node>(1);
    ranks[node] = 10;
    weak_map<Node, int> taken = std::move(ranks);
    println("{} {}", ranks.empty(), taken[node]);

    println("{}", std::is_copy_constructible_v<weak_map<Node, int>>);
}
```

Output:

```text
true true
true 10
false
```

## See also

- [operator=](operator_assign.md): takes the entries of another map over
- [emplace](emplace.md), [operator[]](operator_at.md): add an entry
- [sgcl::weak_map\<Key, T\>](README.md)
