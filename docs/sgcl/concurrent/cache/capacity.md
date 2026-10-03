[sgcl](../../README.md) › [concurrent](../README.md) › [cache](../cache.md)

# sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>::capacity

```cpp
size_type capacity() const noexcept;
```

Returns the number of entries the cache keeps, the one given to the constructor: an insertion that takes the size
past it evicts down to it.

## Parameters

None.

## Return value

The capacity.

## Complexity

Constant.

## Exceptions

None.

## Notes

The capacity is fixed for the life of the cache. A capacity of 0 keeps nothing: every `put` inserts its entry and
evicts it.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::cache<int, int> numbers(100);
    println("{}", numbers.capacity());

    concurrent::cache<int, int> none(0);
    none.put(1, 1);
    println("{} {} {}", none.capacity(), none.size(), none.get(1).has_value());
}
```

Output:

```text
100
0 0 false
```

## See also

- [size](size.md): the number of entries
- [(constructor)](cache.md): sets the capacity
- [sgcl::concurrent::cache\<Key, T, Hash, KeyEqual\>](../cache.md)
