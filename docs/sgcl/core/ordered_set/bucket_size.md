[sgcl](../../README.md) › [core](../README.md) › [ordered_set](../ordered_set.md)

# sgcl::ordered_set\<Key, Hash, KeyEqual\>::bucket_size

```cpp
size_type bucket_size(size_type n) const noexcept;
```

Returns the number of elements in bucket `n`, counted by a walk of the bucket. An `n` not below
[bucket_count()](bucket_count.md) gives 0.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of the bucket |

## Return value

The number of elements in the bucket.

## Complexity

Linear in the size of the bucket.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    ordered_set<int> s = {1, 2, 3};
    size_t total = 0;
    for (size_t n : range(s.bucket_count())) {
        total += s.bucket_size(n);
    }
    println("{} {}", total, s.bucket_size(1000));
}
```

Output:

```text
3 0
```

## See also

- [bucket](bucket.md): the bucket of a key
- [begin, cbegin](begin.md): a local iterator to the first element of a bucket
- [sgcl::ordered_set\<Key, Hash, KeyEqual\>](../ordered_set.md)
