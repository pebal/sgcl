[sgcl](../../README.md) › [core](../README.md) › [multiset](README.md)

# sgcl::multiset\<Key, Hash, KeyEqual\>::bucket_size

```cpp
size_type bucket_size(size_type n) const noexcept;
```

Returns the number of elements in bucket `n`, equal ones counted each, by a walk of the bucket: the elements a
local iterator from [begin(n)](begin.md) passes. A bucket `n` that is not below `bucket_count()` holds none.

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
    multiset s = {1, 1, 2};
    size_t n = s.bucket(1);
    size_t in_bucket = 0;
    for (auto it = s.begin(n); it != s.end(n); ++it) {
        ++in_bucket;
    }
    println("{} {}", in_bucket == s.bucket_size(n), s.bucket_size(n) >= 2);
    println("{}", s.bucket_size(s.bucket_count()));
}
```

Output:

```text
true true
0
```

## See also

- [bucket](bucket.md): the bucket of a key
- [begin, cbegin](begin.md): a local iterator to the first element of a bucket
- [sgcl::multiset\<Key, Hash, KeyEqual\>](README.md)
