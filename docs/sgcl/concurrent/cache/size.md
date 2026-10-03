[sgcl](../../README.md) › [concurrent](../README.md) › [cache](README.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of entries: the cache's own count, one word that every insertion and every erasure writes.

## Parameters

None.

## Return value

The number of entries; at most the capacity once the `put`s have returned.

## Complexity

Constant.

## Exceptions

None.

## Notes

The count is exact, not the sum of the map's stripes: an insertion is counted once its node is linked, an erasure
once by the one thread that claimed the entry. While several threads insert at once it may pass the capacity by
the number of them, each on its way to evict. An entry that is stale but not yet erased is counted until a `get` or
an eviction erases it.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::cache<int, int> numbers(3);
    for (int i : range(10)) {
        numbers.put(i, i * i);
    }
    println("{}", numbers.size());

    vector<thread> writers;
    for (int t : range(4)) {
        writers.emplace_back([&numbers, t] {
            for (int i : range(1000)) {
                numbers.put(t * 1000 + i, i);
            }
        });
    }
    for (auto& w : writers) {
        w.join();
    }
    println("{}", numbers.size() <= numbers.capacity());
}
```

Output:

```text
3
true
```

## See also

- [capacity](capacity.md): the number of entries the cache keeps
- [empty](empty.md): checks whether the cache holds an entry
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](README.md)
