[sgcl](../../README.md) › [concurrent](../README.md) › [weak_set](../weak_set.md)

# sgcl::concurrent::weak_set\<Key\>::count

```cpp
size_type count(const key_pointer& object) const noexcept;
```

Returns the number of entries of `object`: 1 when the set holds it, 0 when it does not, or the object is gone or
null. The search is that of [find](find.md); an object has at most one entry.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entries to count |

## Return value

The number of entries of `object`, 0 or 1.

## Complexity

Constant on average: the walk of the object's bucket, an entry or two.

## Exceptions

None.

## Notes

Wait-free once the object's bucket has its dummy node, as [find](find.md) is. Under concurrent insertions and
erasures of the same object the answer is of the moment of the search.

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
    listeners.insert(a);  // a is in the set: nothing added

    println("{} {}", listeners.count(a), listeners.count(b));
}
```

Output:

```text
1 0
```

## See also

- [contains](contains.md): the same question as a `bool`
- [find](find.md): the entry of an object
- [sgcl::concurrent::weak_set\<Key\>](../weak_set.md)
