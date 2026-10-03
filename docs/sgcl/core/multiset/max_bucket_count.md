[sgcl](../../README.md) › [core](../README.md) › [multiset](README.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::max_bucket_count

```cpp
size_type max_bucket_count() const noexcept;
```

Returns the largest number of buckets a multiset may have: [max_size()](max_size.md), the largest value of
`difference_type`.

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
    multiset<int> s;
    println("{}", s.max_bucket_count() == s.max_size());
}
```

Output:

```text
true
```

## See also

- [bucket_count](bucket_count.md): the number of buckets
- [max_size](max_size.md): the largest number of elements
- [sgcl::multiset\<Key, Hash, KeyEqual\>](README.md)
