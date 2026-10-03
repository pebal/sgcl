[sgcl](../../README.md) › [core](../README.md) › [weak_set](README.md)

# sgcl::weak_set\<Key\>::weak_set

```cpp
weak_set() noexcept;                    // (1)
weak_set(weak_set&& other) noexcept;    // (2)
weak_set(const weak_set&) = delete;     // (3)
```

Constructs a set.

1. An empty set. Nothing is allocated: the table gets its buckets at the first insertion, and the first sweep is
   due after sixteen insertions.
2. Takes the table of `other` over, no entry copied or moved; `other` is empty after.
3. The set is not copyable, as a [weak_map](../weak_map/weak_map.md) is not.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the set whose entries are taken over |

## Complexity

Constant.

## Exceptions

None.

## Notes

The set holds tracked pointers, so it lives on a stack or inside a managed object
([The rules](../README.md#the-rules), 1).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

struct Listener {
    int id;
};

struct Button {
    weak_set<Listener> listeners;  // a member of a managed object
};

int main() {
    weak_set<Listener> seen;  // on the stack
    tracked_ptr button = make_tracked<Button>();
    println("{} {}", seen.empty(), button->listeners.empty());

    tracked_ptr listener = make_tracked<Listener>(1);
    seen.insert(listener);
    weak_set<Listener> taken = std::move(seen);
    println("{} {}", seen.empty(), taken.contains(listener));

    println("{}", std::is_copy_constructible_v<weak_set<Listener>>);
}
```

Output:

```text
true true
true true
false
```

## See also

- [operator=](operator_assign.md): takes the entries of another set over
- [insert](insert.md): adds an object
- [sgcl::weak_set\<Key\>](README.md)
