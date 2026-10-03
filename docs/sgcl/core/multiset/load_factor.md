[sgcl](../../README.md) › [core](../README.md) › [multiset](../multiset.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::load_factor

```cpp
float load_factor() const noexcept;
```

Returns the average number of elements per bucket, `size() / bucket_count()`, equal elements counted each, or 0
when the multiset has no buckets. An insertion keeps it at [max_load_factor()](max_load_factor.md) or below by
growing the table.

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
    multiset<int> s;
    println("{}", s.load_factor());

    s.insert({1, 1, 1, 1});
    println("{} {}", s.bucket_count(), s.load_factor());

    for (int i : range(1000)) {
        s.insert(i % 10);
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
- [sgcl::multiset\<Key, Hash, KeyEqual\>](../multiset.md)
