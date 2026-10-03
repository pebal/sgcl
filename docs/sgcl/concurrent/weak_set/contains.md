[sgcl](../../README.md) › [concurrent](../README.md) › [weak_set](README.md)

# sgcl::concurrent::weak_set\<Key\>::contains

```cpp
bool contains(const key_pointer& object) const noexcept;
```

Checks whether the set holds `object`: the search of [find](find.md), without an iterator. A null pointer is never
in the set, and an object that is gone is not either.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to look for |

## Return value

`true` when the set holds `object`, `false` otherwise.

## Complexity

Constant on average: the walk of the object's bucket, an entry or two.

## Exceptions

None.

## Notes

Wait-free once the object's bucket has its dummy node, as [find](find.md) is. Under concurrent insertions and
erasures of the same object the answer is of the moment of the search: a thread that registers an object once asks
[insert](insert.md), whose answer is whether this call added it.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    int id;
};

int main() {
    concurrent::weak_set<Listener> listeners;
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
- [sgcl::concurrent::weak_set\<Key\>](README.md)
