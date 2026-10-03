[sgcl](../../README.md) › [concurrent](../README.md) › [set](../set.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::bucket_count

```cpp
size_type bucket_count() const noexcept;
```

Returns the number of buckets of the current array, a power of two. The array doubles once the elements outnumber
the buckets, a load factor of one: an insertion compares [size](size.md) with the number of buckets when it brings
its stripe's count to a multiple of `bucket_count() / 128`, which is at every insertion while the array has fewer
than 256 buckets and every `bucket_count() / 8` insertions over all the stripes once it is large. One thread
doubles the array at a time; another that finds it doubling goes on with its insertion.

## Parameters

None.

## Return value

The number of buckets.

## Complexity

Constant.

## Exceptions

None.

## Notes

Wait-free: one load of the array. A doubling copies the slots of the old array into one twice as long and
publishes it with a compare-exchange; no node moves, and the slots of the new half are filled as their buckets are
first used. The array never shrinks: an erasure, or a [clear](clear.md), leaves it as it is.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::set<int> numbers;
    println("{}", numbers.bucket_count());

    for (int i : range(17)) {
        numbers.insert(i);
    }
    println("{}", numbers.bucket_count());

    for (int i : range(17, 1000)) {
        numbers.insert(i);
    }
    numbers.clear();
    println("{} {}", numbers.size(), numbers.bucket_count());
}
```

Output:

```text
16
32
0 1024
```

## See also

- [reserve](reserve.md): grows the array up front
- [size](size.md): the count the growth is decided by
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](../set.md)
