[sgcl](../../README.md) › [core](../README.md) › [weak_set](../weak_set.md)

# sgcl::weak_set\<Key\>::sweep

```cpp
size_type sweep() noexcept;
```

Erases the entries whose objects are gone: a walk of the table that erases every entry whose weak pointer's cell
the collector has cleared. After the walk the threshold of the next sweep is set to the number of entries left, 16
at least, and the count of insertions starts again from zero.

The set runs the same sweep by itself: the insertion that brings the count since the last sweep past the threshold
runs it before it returns. A program calls `sweep` when it inserts little and wants the memory of the dead entries
back, or wants [size](size.md) to count the live objects.

## Parameters

None.

## Return value

The number of entries erased.

## Complexity

Linear in the number of entries.

## Exceptions

None.

## Notes

An entry is dead once a cycle of the collector has found its object unreachable; until then a sweep keeps it. An
entry an iterator stands on is never dead: the iterator holds its object.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    int id;
};

void register_temporaries(weak_set<Listener>& listeners) {
    for (int i : range(1, 8)) {
        tracked_ptr listener = make_tracked<Listener>(i);
        listeners.insert(listener);
    }
}

int main() {
    weak_set<Listener> listeners;
    tracked_ptr kept = make_tracked<Listener>(0);
    listeners.insert(kept);
    register_temporaries(listeners);

    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the listeners
    collector::force_collect(true);  // optional, for the demonstration
    size_t swept = listeners.sweep();
    println("{} swept, {} left", swept, listeners.size());

    swept = listeners.sweep();
    println("{} swept", swept);
}
```

Output:

```text
7 swept, 1 left
0 swept
```

## See also

- [size](size.md): the number of entries, the dead ones not yet swept included
- [erase](erase.md): erases an object
- [sgcl::weak_set\<Key\>](../weak_set.md)
