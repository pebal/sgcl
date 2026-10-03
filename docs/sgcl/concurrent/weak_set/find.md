[sgcl](../../README.md) › [concurrent](../README.md) › [weak_set](README.md)

# sgcl::concurrent::weak_set\<Key\>::find

```cpp
iterator find(const key_pointer& object) noexcept;
```

Finds the entry of `object`: a search of the table by the object's address, from the dummy node of its bucket along
the list to the entry whose weak pointer holds that address. A null pointer has no entry, and an object that is gone
has none: its entry equals nothing, so it is never found, and the entry of an object that later takes the same
address is a different one.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entry to find |

## Return value

An iterator to the entry of `object`, holding its node and the object, or [end](end.md) when the set does not hold
`object` or it is null.

## Complexity

Constant on average: the walk of the object's bucket, an entry or two.

## Exceptions

None.

## Notes

Wait-free, and writes nothing but the iterator, once the object's bucket has its dummy node; the first lookup or
insertion in a bucket that has none makes it, an allocation and a compare-exchange, lock-free, once per bucket for
the life of the array of buckets.

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

    auto it = listeners.find(a);
    if (it != listeners.end()) {
        println("{} {}", (*it)->id, *it == a);
    }
    println("{}", listeners.find(b) == listeners.end());
    println("{}", listeners.find(nullptr) == listeners.end());
}
```

Output:

```text
1 true
true
true
```

## See also

- [contains](contains.md): checks whether the set holds an object
- [count](count.md): the number of entries of an object
- [sgcl::concurrent::weak_set\<Key\>](README.md)
