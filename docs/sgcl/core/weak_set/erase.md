[sgcl](../../README.md) › [core](../README.md) › [weak_set](../weak_set.md)

# sgcl::weak_set\<Key\>::erase

```cpp
/*(1)*/ size_type erase(const key_pointer& object) noexcept;
/*(2)*/ iterator erase(iterator pos) noexcept;
```

Erases an entry.

1. Erases the entry of `object`, if the set holds it. A null pointer has none.
2. Erases the entry `pos` stands on, and returns the next live object up to the iterator's own bound, `end()`.
   `pos` must stand on an entry, not be [end](end.md).

## Parameters

| Parameter | Description |
|---|---|
| `object` | the object to erase |
| `pos` | an iterator to the entry to erase |

## Return value

- (1) The number of entries erased, 0 or 1.
- (2) An iterator to the next live object after `pos`, or `end()`.

## Complexity

- (1) Constant on average.
- (2) Constant on average, plus the dead entries before the next live one, which the iterator passes.

## Exceptions

None.

## Notes

An entry whose object is gone needs no erasure: it is never found, and a [sweep](sweep.md) drops it.

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
- [sgcl::weak_set\<Key\>](../weak_set.md)
