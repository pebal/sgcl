[sgcl](../../README.md) › [concurrent](../README.md) › [weak_set](README.md)

# sgcl::concurrent::weak_set\<Key\>::sweep

```cpp
size_type sweep() noexcept;
```

Erases the entries whose objects are gone: a walk of the table that erases, through an iterator, every entry whose
weak pointer's cell the collector has cleared. After the walk the threshold of the next sweep is set to the number
of entries left, 16 at least, and the count of insertions starts again from zero.

The inserting threads run the same sweep by themselves: the insertion that brings the count since the last sweep to
the threshold runs it before it returns. A program calls `sweep` when it inserts little and wants the memory of the
dead entries back, or wants `size()` to count the live objects.

## Parameters

None.

## Return value

The number of entries erased; 0 at once when another thread's sweep is under way.

## Complexity

Linear in the number of entries, plus the dummy nodes of the buckets the walk passes.

## Exceptions

None.

## Notes

One sweep runs at a time: a thread that finds one under way does not wait for it, and `sweep` returns 0 to it at
once, so neither a sweep nor an insertion that would run one ever waits. The sweep is safe under the other threads:
the collector clears a cell before the object's slot can be handed out again, so an entry seen dead is dead for good
and no thread can find it alive meanwhile, and erasing it races with nothing but another erasure of the same node,
which the table settles at the compare-exchange that marks it. Each erasure is lock-free.

The threshold follows the entries: a set filled from empty with objects that all live is swept by its insertions at
the 16th, 32nd, 64th, 128th and 256th entry, and each of those sweeps finds nothing to drop; the work of the sweeps
stays proportional to the insertions.

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
    tracked_ptr kept = make_tracked<Listener>(0);
    listeners.insert(kept);
    for (int i : range(1, 8)) {
        listeners.insert(make_tracked<Listener>(i));  // nobody else holds these
    }

    collector::force_collect(true);  // optional, for the demonstration: seven entries die
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
- [insert](insert.md): the insertion that sweeps by itself
- [sgcl::concurrent::weak_set\<Key\>](README.md)
