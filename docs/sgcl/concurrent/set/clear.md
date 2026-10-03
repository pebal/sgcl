[sgcl](../../README.md) › [concurrent](../README.md) › [set](../set.md)

# sgcl::concurrent::set\<Key, Hash, KeyEqual\>::clear

```cpp
void clear() noexcept;
```

Erases every element there is at the time of the walk: from the head along the list, each element erased as
[erase](erase.md) erases it, marked and unlinked.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of elements, plus the dummies of the buckets used.

## Exceptions

None.

## Notes

Under concurrent insertions `clear` takes what it reaches: a key inserted while it runs may be erased or left, and
the set may hold elements when it returns. Every erasure of it is lock-free. The elements are destroyed by the
collector, as an erased element is; the bucket array and the dummies of the buckets stay as they were.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::set<int> seen;
    for (int i : range(100)) {
        seen.insert(i);
    }

    seen.clear();
    println("{} {} {}", seen.empty(), seen.size(), seen.bucket_count());
}
```

Output:

```text
true 0 128
```

## See also

- [erase](erase.md): erases one element
- [sgcl::concurrent::set\<Key, Hash, KeyEqual\>](../set.md)
