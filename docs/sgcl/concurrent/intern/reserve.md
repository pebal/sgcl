[sgcl](../../README.md) › [concurrent](../README.md) › [intern](../intern.md)

# sgcl::concurrent::intern\<T, Hash, KeyEqual\>::reserve

```cpp
void reserve(size_type count) noexcept;
```

Grows the table's array of buckets to at least `count` buckets now, doubling it as often as that takes, rather
than as the insertions come: the table doubles its array once its entries outnumber its buckets.

## Parameters

| Parameter | Description |
|---|---|
| `count` | the number of entries the table is to hold without growing |

## Return value

None.

## Complexity

Linear in the number of buckets of the new array, when it grows; constant otherwise.

## Exceptions

None.

## Notes

The array of [concurrent::set](../set.md) doubles without moving an entry, and so does `reserve`: it may be called
while other threads intern, and they go on in the old array or the new one. A pool that will hold many values from
the start saves the doublings that its first insertions would make. A `count` whose buckets the managed heap cannot
give, up to `SIZE_MAX`, ends the program at the doubling it refuses, as any refused managed allocation does
([collector](../../core/collector.md#the-memory-limit)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::intern<int> ids;
    ids.reserve(10000);

    vector<tracked_ptr<const int>> held;
    for (int i : range(10000)) {
        held.push_back(ids.of(i));
    }
    println("{}", ids.size());
}
```

Output:

```text
10000
```

## See also

- [size](size.md): the number of entries
- [sgcl::concurrent::intern\<T, Hash, KeyEqual\>](../intern.md)
