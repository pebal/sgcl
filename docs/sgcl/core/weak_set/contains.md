[sgcl](../../README.md) › [core](../README.md) › [weak_set](../weak_set.md)

# sgcl::weak_set\<Key\>::contains

```cpp
bool contains(const key_pointer& object) const noexcept;
```

Checks whether the set holds `object`: the search of [find](find.md), without an iterator. A null pointer is never
in the set.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to look for |

## Return value

`true` when the set holds `object`, `false` otherwise.

## Complexity

Constant on average.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    int id;
};

int main() {
    weak_set<Listener> listeners;
    tracked_ptr a = make_tracked<Listener>(1);
    tracked_ptr b = make_tracked<Listener>(2);
    listeners.insert(a);

    println("{} {}", listeners.contains(a), listeners.contains(b));
    listeners.erase(a);
    println("{}", listeners.contains(a));
}
```

Output:

```text
true false
false
```

## See also

- [count](count.md): the same question as a number
- [find](find.md): the entry of an object
- [sgcl::weak_set\<Key\>](../weak_set.md)
