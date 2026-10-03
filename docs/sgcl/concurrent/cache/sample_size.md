[sgcl](../../README.md) › [concurrent](../README.md) › [cache](../cache.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::sample_size

```cpp
unsigned sample_size() const noexcept;
```

Returns the number of entries an eviction looks at: the `sample` given to the constructor, `DefaultSample` (8) when
none was, 1 when 0 was.

## Parameters

None.

## Return value

The size of the sample.

## Complexity

Constant.

## Exceptions

None.

## Notes

An eviction walks this many entries on from where its stripe's last walk ended, erases the stale ones and then the
one with the oldest stamp among the rest. With a sample of *n* the entry evicted is older than *n* − 1 others at
least: a larger sample approximates an exact LRU closer, at a longer eviction.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::cache<int, int> usual(100);
    concurrent::cache<int, int> precise(100, {}, 16);
    concurrent::cache<int, int> one(100, {}, 0);
    println("{} {} {}", usual.sample_size(), precise.sample_size(), one.sample_size());
}
```

Output:

```text
8 16 1
```

## See also

- [(constructor)](cache.md): sets the sample
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](../cache.md)
