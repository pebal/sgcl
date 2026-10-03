[sgcl](../../README.md) › [concurrent](../README.md) › [intern](../intern.md)

# sgcl::concurrent::intern\<T, Hash, KeyEqual\>::sweep

```cpp
size_type sweep() noexcept;
```

Drops the entries whose objects are gone: a walk of the table that erases every entry whose weak pointer the
collector has cleared. The same sweep runs by itself, on an inserting thread, every so many insertions; `sweep`
runs one on demand.

## Parameters

None.

## Return value

The number of entries dropped, or 0 at once when another thread's sweep is under way.

## Complexity

Linear in the number of entries.

## Exceptions

None.

## Notes

One sweep runs at a time, and a thread that finds one under way goes on without waiting: an insertion that comes to
its sweep never blocks, and stays lock-free. The sweep is safe under the threads because the collector clears a
weak pointer before the object's memory can be handed out again: an entry seen dead is dead for good. A sweep sets
the threshold of the next one to the number of entries it leaves, 16 at least. An object becomes dead only once a
cycle of the collector has found it unreachable, so what a sweep drops right after the last handle went depends on
the collector's progress.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::intern<string> tags;
    string kept = tags.of("kept");
    thread worker([&tags] {
        string temporary = tags.of("temporary");  // gone with the thread
    });
    worker.join();
    println("{} entries", tags.size());

    collector::force_collect(true);  // optional, for the demonstration: the dead object found
    size_t dropped = tags.sweep();
    println("{} dropped, {} left", dropped, tags.size());
}
```

Output:

```text
2 entries
1 dropped, 1 left
```

## See also

- [size](size.md): the number of entries, the dead ones included
- [clear](clear.md): forgets every entry, the live ones too
- [README: Weak containers](../README.md#weak-containers)
- [sgcl::concurrent::intern\<T, Hash, KeyEqual\>](../intern.md)
