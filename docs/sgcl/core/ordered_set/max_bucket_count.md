[sgcl](../../README.md) › [core](../README.md) › [ordered_set](README.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::max_bucket_count

```cpp
size_type max_bucket_count() const noexcept;
```

Returns the largest number of buckets a set may have: [max_size()](max_size.md), the largest value of
`ptrdiff_t`.

## Parameters

None.

## Return value

The largest number of buckets.

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
    println("{}", s.max_bucket_count() == s.max_size());
}
```

Output:

```text
true
```

## See also

- [bucket_count](bucket_count.md): the number of buckets
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](README.md)
