[sgcl](../../README.md) › [concurrent](../README.md) › [cache](README.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::clear

```cpp
void clear() noexcept;
```

Erases every entry there is at the time of the walk, as [erase](erase.md) erases one, and lets go of the cursors of
the evictions, which hold the nodes the last walks ended at. The counts of the hits and the misses stay.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of entries.

## Exceptions

None.

## Notes

Under concurrent `put`s `clear` erases what it reaches: an entry put while it runs may be erased or left, and the
cache may hold entries when it returns. Every erasure of it is lock-free. The entries, and the nodes the cursors
held, are destroyed by the collector once nothing holds them.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::cache<int, string> pages(10);
    pages.put(1, "home");
    pages.put(2, "about");
    pages.get(1);
    pages.get(3);

    pages.clear();
    println("{} {}", pages.empty(), pages.size());
    println("hits {}, misses {}", pages.hits(), pages.misses());
}
```

Output:

```text
true 0
hits 1, misses 1
```

## See also

- [erase](erase.md): erases the entry of a key
- [hits](hits.md), [misses](misses.md): the counts the `clear` keeps
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](README.md)
