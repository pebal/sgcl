[sgcl](../../README.md) › [concurrent](../README.md) › [weak_set](../weak_set.md)

# sgcl::concurrent::weak_set\<Key\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of entries: the table's count, the dead entries not yet swept included. The count is striped over
cache lines, as Java's `LongAdder` is: an insertion or an erasure adds to the stripe of its thread, and `size` sums
the stripes.

## Parameters

None.

## Return value

The number of entries, live and dead.

## Complexity

Constant: a sum of sixteen stripes.

## Exceptions

None.

## Notes

Under concurrent insertions and erasures the sum reads the stripes at different moments, so the number is a snapshot
of no particular moment; it is exact once the other threads are quiet. The number of live objects is what a
[sweep](sweep.md) leaves: `size()` after it, with the threads quiet.

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
    tracked_ptr kept = make_tracked<Listener>(1);
    listeners.insert(kept);
    listeners.insert(make_tracked<Listener>(2));  // nobody else holds this listener
    println("{}", listeners.size());

    collector::force_collect(true);  // optional, for the demonstration: the second entry dies
    println("{}", listeners.size());

    listeners.sweep();
    println("{}", listeners.size());
}
```

Output:

```text
2
2
1
```

## See also

- [empty](empty.md): checks whether the set holds an entry
- [sweep](sweep.md): erases the dead entries
- [sgcl::concurrent::weak_set\<Key\>](../weak_set.md)
