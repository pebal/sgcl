[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::load_factor

```cpp
float load_factor() const noexcept;
```

Returns the average number of elements per bucket, `size() / bucket_count()`, or 0 when the set has no buckets.
An insertion keeps it at [max_load_factor()](max_load_factor.md) or below by growing the table.

## Parameters

None.

## Return value

The load factor.

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
    set<int> s;
    println("{}", s.load_factor());

    s.insert({1, 2, 3, 4});
    println("{} {}", s.bucket_count(), s.load_factor());

    for (int i : range(5, 1000)) {
        s.insert(i);
    }
    println("{}", s.load_factor() <= s.max_load_factor());
}
```

Output:

```text
0
8 0.5
true
```

## See also

- [max_load_factor](max_load_factor.md): the load factor at which the table grows
- [bucket_count](bucket_count.md): the number of buckets
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
