[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::max_load_factor

```cpp
float max_load_factor() const noexcept;    // (1)
void max_load_factor(float z) noexcept;    // (2)
```

1. Returns the load factor at which the table grows: an insertion that finds the size at
   `bucket_count() * max_load_factor()` grows the table first. 1.0 by default; a copy keeps the one of the set it
   copies.
2. Sets it to `z`. It takes effect on the next insertion, which grows the table if it is overloaded now; nothing
   is rehashed at the call. A `z` that is not positive, or not a number, is ignored.

A factor so small that the elements need more buckets than the managed heap gives ends the program at the growth it
refuses, as any refused managed allocation does ([collector](../collector.md#the-memory-limit)).

## Parameters

| Parameter | Description |
|---|---|
| `z` | the new maximum load factor |

## Return value

- (1) The maximum load factor.
- (2) None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> s;
    for (int i : range(8)) {
        s.insert(i);
    }
    println("{} {}", s.max_load_factor(), s.bucket_count());

    s.max_load_factor(0.5f);
    s.max_load_factor(-1.0f);  // ignored
    println("{} {}", s.max_load_factor(), s.bucket_count());

    s.insert(8);  // overloaded now: the table grows first
    println("{}", s.bucket_count());
}
```

Output:

```text
1 8
0.5 8
32
```

## See also

- [load_factor](load_factor.md): the elements per bucket
- [rehash](rehash.md), [reserve](reserve.md): set the number of buckets
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
