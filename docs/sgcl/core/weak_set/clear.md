[sgcl](../../README.md) › [core](../README.md) › [weak_set](../weak_set.md)

# sgcl::weak_set\<Key\>::clear

```cpp
void clear() noexcept;
```

Erases every entry, dead or alive; the table keeps its buckets. The count of insertions towards the next sweep
starts again from zero, with the threshold back at 16.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of entries.

## Exceptions

None.

## Notes

Every iterator to the set is invalid after the call. The nodes are left to the collector.

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
    tracked_ptr a = make_tracked<Listener>(1);
    tracked_ptr b = make_tracked<Listener>(2);
    listeners.insert(a);
    listeners.insert(b);

    listeners.clear();
    println("{} {} {}", listeners.empty(), listeners.size(), listeners.contains(a));
}
```

Output:

```text
true 0 false
```

## See also

- [erase](erase.md): erases one entry
- [sweep](sweep.md): erases the dead entries alone
- [sgcl::weak_set\<Key\>](../weak_set.md)
