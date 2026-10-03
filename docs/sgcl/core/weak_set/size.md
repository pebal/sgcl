[sgcl](../../README.md) › [core](../README.md) › [weak_set](README.md)

# sgcl::weak_set\<Key\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of entries, the dead ones not yet swept included: the table's count, which a sweep brings down
to the live objects.

## Parameters

None.

## Return value

The number of entries, live and dead.

## Complexity

Constant.

## Exceptions

None.

## Notes

The number is exact for the live objects right after a [sweep](sweep.md); between the sweeps it counts too the
entries whose objects have gone since. The number of live objects is what a walk from [begin](begin.md) counts.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Listener {
    int id;
};

void register_a_temporary(weak_set<Listener>& listeners) {
    tracked_ptr listener = make_tracked<Listener>(2);
    listeners.insert(listener);
}

int main() {
    weak_set<Listener> listeners;
    tracked_ptr kept = make_tracked<Listener>(1);
    listeners.insert(kept);
    register_a_temporary(listeners);
    println("{}", listeners.size());

    collector::clear_stack();  // the dead frame zeroed: nothing on the stack keeps the listener
    collector::force_collect(true);  // optional, for the demonstration
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
- [sgcl::weak_set\<Key\>](README.md)
