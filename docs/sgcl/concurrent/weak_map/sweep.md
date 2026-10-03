[sgcl](../../README.md) › [concurrent](../README.md) › [weak_map](README.md)

# sgcl::concurrent::weak_map\<Key, T\>::sweep

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

An entry is dead once a cycle of the collector has found its object unreachable; until then a sweep keeps it.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int id;
};

int main() {
    concurrent::weak_map<Node, int> ranks;
    tracked_ptr kept = make_tracked<Node>(0);
    ranks.try_emplace(kept, 0);
    for (int i : range(1, 8)) {
        ranks.try_emplace(make_tracked<Node>(i), i);  // nobody else holds these nodes
    }

    collector::force_collect(true);  // optional, for the demonstration: seven entries die
    size_t swept = ranks.sweep();
    println("{} swept, {} left", swept, ranks.size());

    swept = ranks.sweep();
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
- [erase](erase.md): erases the entry of an object
- [sgcl::concurrent::weak_map\<Key, T\>](README.md)
