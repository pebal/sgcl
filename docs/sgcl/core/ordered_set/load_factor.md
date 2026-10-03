[sgcl](../../README.md) › [core](../README.md) › [ordered_set](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::load_factor

```cpp
float load_factor() const noexcept;
```

Returns the average number of elements per bucket, `size() / bucket_count()`; 0 for a set with no bucket array.

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
    ordered_set<int> s;
    println("{}", s.load_factor());

    s.reserve(4);
    for (int i : range(3)) {
        s.insert(i);
    }
    println("{} {} {}", s.size(), s.bucket_count(), s.load_factor());
}
```

Output:

```text
0
3 4 0.75
```

## See also

- [max_load_factor](max_load_factor.md): the load factor at which the table grows
- [bucket_count](bucket_count.md): the number of buckets
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](README.md)
