[sgcl](../../README.md) › [concurrent](../README.md) › [weak_set](../weak_set.md)

# sgcl::concurrent::weak_set\<Key\>::weak_set

```cpp
weak_set();                            // (1)
weak_set(const weak_set&) = delete;    // (2)
```

1. An empty set: the table of [concurrent::set](../set.md) with sixteen buckets, its head node and its counters, and
   the first sweep due after sixteen insertions.
2. The set is not copyable, and not movable: a structure shared by threads has one place.

## Parameters

None.

## Complexity

Constant: a few allocations.

## Exceptions

None.

## Notes

The set holds tracked pointers, so it lives on a thread's stack or inside a managed object; the set a whole program
shares goes into a managed object held by a [root_ptr](../../core/root_ptr.md).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Listener {
    int id;
};

struct Button {
    concurrent::weak_set<Listener> listeners;  // a member of a managed object
};

int main() {
    concurrent::weak_set<Listener> active;  // on the stack
    tracked_ptr button = make_tracked<Button>();

    println("{} {}", active.empty(), button->listeners.empty());
    println("{}", std::is_copy_constructible_v<concurrent::weak_set<Listener>>);
}
```

Output:

```text
true true
false
```

## See also

- [insert](insert.md): adds an object
- [sgcl::concurrent::weak_set\<Key\>](../weak_set.md)
