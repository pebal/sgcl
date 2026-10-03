[sgcl](../../README.md) › [concurrent](../README.md) › [sorted_set](README.md)

# sgcl::concurrent::sorted_set\<Key, Compare\>::clear

```cpp
void clear() noexcept;
```

Erases every key there is at the time of the walk: a walk over the bottom list that erases each node it reaches,
as [erase](erase.md) erases the key an iterator addresses.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of keys.

## Exceptions

None.

## Notes

`clear` is a walk of erasures, each lock-free and linearizable, not one step: under concurrent insertions a key
inserted behind the walk is left, and the set may hold keys when `clear` returns. The erased keys are destroyed by
the collector, as an erased key always is.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::sorted_set<int> ids = {1, 2, 3};
    ids.clear();
    println("{} {}", ids.empty(), ids.size());
}
```

Output:

```text
true 0
```

## See also

- [erase](erase.md): erases one key
- [sgcl::concurrent::sorted_set\<Key, Compare\>](README.md)
