[sgcl](../../README.md) › [concurrent](../README.md) › [cache](../cache.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::hits

```cpp
uint64_t hits() const noexcept;
```

Returns the number of gets that found a value: every [get](get.md), and the get inside every
[get_or_compute](get_or_compute.md), that returned a fresh entry's value.

## Parameters

None.

## Return value

The number of hits since the cache was made.

## Complexity

Constant: the sum of 16 stripes.

## Exceptions

None.

## Notes

The counts are striped over cache lines, as the map's count is: a get adds one to the stripe of its thread, and
`hits` sums the stripes, a snapshot of no particular moment while other threads get; it is exact once they are
quiet. [clear](clear.md) keeps the counts. With [misses](misses.md) it gives the hit rate.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::cache<int, int> squares(10);
    for (int i : range(10)) {
        int k = i % 4;
        squares.get_or_compute(k, [k] { return k * k; });
    }
    uint64_t hits = squares.hits();
    uint64_t misses = squares.misses();
    double rate = double(hits) / double(hits + misses);
    println("{} hits, {} misses, a hit rate of {}", hits, misses, rate);
}
```

Output:

```text
6 hits, 4 misses, a hit rate of 0.6
```

## See also

- [misses](misses.md): the number of gets that found none
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](../cache.md)
