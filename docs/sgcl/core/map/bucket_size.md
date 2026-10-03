[sgcl](../../README.md) › [core](../README.md) › [map](../map.md)

# sgcl::map\<Key, T, Hash, KeyEqual\>::bucket_size

```cpp
size_type bucket_size(size_type n) const noexcept;
```

Returns the number of elements in bucket `n`, counted by a walk of the bucket; 0 for an `n` that is not below
`bucket_count()`.

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
#include <algorithm>

using namespace sgcl;

int main() {
    map<int, int> m;
    for (int i : range(100)) {
        m.emplace(i, i);
    }
    size_t total = 0;
    size_t largest = 0;
    for (size_t n : range(m.bucket_count())) {
        total += m.bucket_size(n);
        largest = std::max(largest, m.bucket_size(n));
    }
    println("{} {} {}", total, largest, m.bucket_size(m.bucket_count()));
}
```

Output:

```text
100 1 0
```

## See also

- [bucket](bucket.md): the bucket of a key
- [begin, cbegin](begin.md): the local iterators of a bucket
- [sgcl::map\<Key, T, Hash, KeyEqual\>](../map.md)
