[sgcl](../../README.md) › [concurrent](../README.md) › [cache](README.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the cache holds an entry: the map's `empty`, a walk from the head of its list to the first entry
not erased.

## Parameters

None.

## Return value

`true` when the walk found no entry, `false` otherwise.

## Complexity

Constant when the cache holds entries in its first buckets; at most linear in the number of buckets in use, whose
dummy nodes the walk passes.

## Exceptions

None.

## Notes

Under concurrent `put`s and erasures the answer is of the moment of the walk and may be stale when it returns. It
asks the map, not the count of [size](size.md): an entry that is stale but not yet erased is an entry.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::cache<int, int> numbers(10);
    println("{}", numbers.empty());

    numbers.put(1, 1);
    println("{}", numbers.empty());

    numbers.erase(1);
    println("{}", numbers.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): the number of entries
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](README.md)
