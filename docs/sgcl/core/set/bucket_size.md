[sgcl](../../README.md) › [core](../README.md) › [set](../set.md)

# sgcl::set\<Key, Hash, KeyEqual\>::bucket_size

```cpp
size_type bucket_size(size_type n) const noexcept;
```

Returns the number of elements in bucket `n`, counted by a walk of the bucket: the elements a local iterator
from [begin(n)](begin.md) passes. A bucket `n` that is not below `bucket_count()` holds none.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of a bucket |

## Return value

The number of elements in the bucket.

## Complexity

Linear in the number of elements in the bucket.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    set<int> s;
    for (int i : range(100)) {
        s.insert(i);
    }

    size_t total = 0;
    for (size_t n : range(s.bucket_count())) {
        total += s.bucket_size(n);
    }
    println("{} {}", total, s.bucket_size(s.bucket_count()));
}
```

Output:

```text
100 0
```

## See also

- [bucket](bucket.md): the bucket of a key
- [begin, cbegin](begin.md): a local iterator to the first element of a bucket
- [sgcl::set\<Key, Hash, KeyEqual\>](../set.md)
