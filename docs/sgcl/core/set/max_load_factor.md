[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::max_load_factor

```cpp
/*(1)*/ float max_load_factor() const noexcept;
/*(2)*/ void max_load_factor(float z) noexcept;
```

Reads or sets the maximum load factor, the average number of elements per bucket at which an insertion grows
the table; 1.0 by default.

1. Returns the maximum load factor.
2. Sets it to `z`. The table is not rehashed now: the next insertion grows it when the size has reached
   `bucket_count() * z`. A `z` that is not positive, or not a number, is ignored.

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

## Notes

A copy and an assignment of a list keep the maximum load factor of the set; a move and a swap carry it with the
table.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<int> s;
    for (int i : range(8)) {
        s.insert(i);
    }
    println("{} {} {}", s.max_load_factor(), s.bucket_count(), s.load_factor());

    s.max_load_factor(0.5f);
    println("{}", s.bucket_count());

    s.insert(8);
    println("{} {}", s.bucket_count(), s.load_factor() <= 0.5f);

    s.max_load_factor(-1.0f);
    println("{}", s.max_load_factor());
}
```

Output:

```text
1 8 1
8
32 true
0.5
```

## See also

- [load_factor](load_factor.md): the elements per bucket
- [rehash](rehash.md): sets the number of buckets
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
