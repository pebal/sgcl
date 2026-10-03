[sgcl](../../README.md) › [concurrent](../README.md) › [weak_set](../weak_set.md)

# sgcl::concurrent::weak_set\<Key\>::erase

```cpp
/*(1)*/ size_type erase(const key_pointer& object) noexcept;
/*(2)*/ iterator erase(iterator pos) noexcept;
```

Erases an entry.

1. Erases the entry of `object`, if the set holds it. A null pointer has none, and an object that is gone has none.
2. Erases the entry `pos` stands on, if it is still there: another thread may have erased it since. `pos` must stand
   on an entry, not be [end](end.md).

The node is marked, with a compare-exchange that links a marker node after it, and then unlinked from the list by a
search: by the hash the entry was placed with, which it carries, so an entry is erased from where it was put. The
object itself is not touched: the set never kept it alive.

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object whose entry to erase |
| `pos` | an iterator to the entry to erase |

## Return value

- (1) The number of entries erased, 0 or 1.
- (2) An iterator to the next live object after `pos`, or [end](end.md).

## Complexity

- (1) Constant on average: the walk of the object's bucket, an entry or two.
- (2) Constant on average, plus the walk to the next live object.

## Exceptions

None.

## Notes

Lock-free, and linearizable at the compare-exchange that marks the node: of several threads erasing the same entry,
exactly one marks it, and (1) returns 1 to that one alone. An iterator standing on the entry keeps the object it
holds and walks on from it.

An entry whose object is gone needs no erasure: it is never found, and a [sweep](sweep.md) drops it.

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
    vector<tracked_ptr<Listener>> owned;
    for (int i : range(4)) {
        owned.push_back(make_tracked<Listener>(i));
        listeners.insert(owned.back());
    }

    size_t first = listeners.erase(owned[0]);
    size_t again = listeners.erase(owned[0]);
    println("{} {} {}", first, again, listeners.size());

    for (auto it = listeners.begin(); it != listeners.end();) {
        if ((*it)->id >= 2) {
            it = listeners.erase(it);
        } else {
            ++it;
        }
    }
    println("{} {}", listeners.size(), listeners.contains(owned[1]));
}
```

Output:

```text
1 0 3
1 true
```

## See also

- [sweep](sweep.md): erases the entries whose objects are gone
- [clear](clear.md): erases every entry
- [sgcl::concurrent::weak_set\<Key\>](../weak_set.md)
