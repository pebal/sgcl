[sgcl](../../README.md) › [concurrent](../README.md) › [cache](../cache.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::misses

```cpp
uint64_t misses() const noexcept;
```

Returns the number of gets that found no value: every [get](get.md), and the get inside every
[get_or_compute](get_or_compute.md), that found the key absent or its entry stale.

## Parameters

None.

## Return value

The number of misses since the cache was made.

## Complexity

Constant: the sum of 16 stripes.

## Exceptions

None.

## Notes

A get that finds an entry past its time to live is a miss, and erases the entry. The counts are striped over cache
lines as [hits](hits.md) are, a snapshot of no particular moment while other threads get, and [clear](clear.md)
keeps them.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::cache<string, int> ages(10);
    ages.put("Ada", 36);
    ages.get("Ada");
    ages.get("Grace");
    ages.get("Linus");
    println("{}", ages.misses());
}
```

Output:

```text
2
```

## See also

- [hits](hits.md): the number of gets that found a value
- [ttl](ttl.md): the time after which an entry is a miss
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](../cache.md)
