[sgcl](../../README.md) › [core](../README.md) › [multimap](../multimap.md)

# sgcl::multimap\<Key, T, Hash, KeyEqual\>::max_load_factor

```cpp
float max_load_factor() const noexcept;    // (1)
void max_load_factor(float z) noexcept;    // (2)
```

Reads or sets the load factor the table grows at: an insertion grows it when the size has reached
`bucket_count() * max_load_factor()`.

1. Returns the maximum load factor, 1.0 by default.
2. Sets it to `z`. A value that is not positive, or not a number, is ignored. Nothing is rehashed here: the table
   grows on the next insertion if it is now overloaded.

A factor so small that the elements need more buckets than the managed heap gives ends the program at the growth it
refuses, as any refused managed allocation does ([collector](../collector.md#the-memory-limit)).

## Parameters

| Parameter | Description |
|---|---|
| `z` | the new maximum load factor, above 0 |

## Return value

- (1) The maximum load factor.
- (2) None.

## Complexity

Constant.

## Exceptions

None.

## Notes

A copy and a move of the multimap carry the maximum load factor over; [operator=](operator_assign.md) of a list and
[clear](clear.md) keep the multimap's own.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    multimap<int, int> m;
    for (int i : range(9)) {
        m.emplace(i, i);
    }
    println("{} {}", m.max_load_factor(), m.bucket_count());

    m.max_load_factor(0.25f);
    println("{} {}", m.max_load_factor(), m.bucket_count());  // not rehashed yet
    m.emplace(100, 100);
    println("{} {}", m.bucket_count(), m.load_factor() <= 0.25f);

    m.max_load_factor(-1.0f);  // ignored
    println("{}", m.max_load_factor());
}
```

Output:

```text
1 16
0.25 16
64 true
0.25
```

## See also

- [load_factor](load_factor.md): the average number of elements per bucket
- [rehash](rehash.md), [reserve](reserve.md): set the number of buckets
- [sgcl::multimap\<Key, T, Hash, KeyEqual\>](../multimap.md)
